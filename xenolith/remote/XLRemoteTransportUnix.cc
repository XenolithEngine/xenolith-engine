/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 **/

/* The `unix:` transport: an AF_UNIX stream socket.
 *
 * The right carrier for a session that never leaves the machine, which is what live reload and every
 * local tool actually are. Compared with running QUIC over loopback for the same job it removes the
 * whole TLS apparatus -- no ephemeral certificate, no SPKI to hand over out of band, no handshake in
 * the startup path, no free UDP port to find -- and it removes the man in the middle with it: the
 * bytes never leave the kernel, and the peer is identified by credentials the kernel vouches for
 * rather than by a secret the two sides happen to share.
 *
 * Hence PeerAuthenticated: with SO_PEERCRED the server knows the peer's uid before a single protocol
 * byte is read, which is strictly stronger than the bearer key and lets the server stop requiring it.
 * Access control is the socket's filesystem permissions (0600 by default).
 */

#include "XLRemoteTransport.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

// AF_UNIX with SO_PEERCRED. Windows has AF_UNIX but no peer credentials, and wasm has no
// sockets at all -- without the credentials the transport could not claim PeerAuthenticated, which
// is the whole reason to prefer it locally.
#if SPRT_LINUX || SPRT_APPLE || SPRT_ANDROID

namespace STAPPLER_VERSIONIZED stappler::xenolith::remote {

namespace {

// Bound per accept pass, so a burst cannot monopolise the caller's thread in one looper iteration.
// Named per transport: every transport lands in the same SCU, where anonymous namespaces merge.
static constexpr uint32_t kUnixMaxAcceptsPerPump = 8;

static bool unixSetNonBlocking(int fd) {
	int flags = ::fcntl(fd, F_GETFL, 0);
	return flags >= 0 && ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

// Fill a sockaddr_un from a path. Returns false when the path does not fit -- sun_path is a fixed
// buffer, and silently truncating it would bind (or connect to) a different socket than the caller
// asked for.
static bool unixMakeAddr(StringView path, struct sockaddr_un &addr, socklen_t &len) {
	__sprt_memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	if (path.empty() || path.size() >= sizeof(addr.sun_path)) {
		log::source().error("remote::unix", "socket path is empty or too long (max ",
				sizeof(addr.sun_path) - 1, "): ", path);
		return false;
	}
	__sprt_memcpy(addr.sun_path, path.data(), path.size());
	len = socklen_t(offsetof(struct sockaddr_un, sun_path) + path.size() + 1);
	return true;
}

// SO_PEERCRED's payload, declared here rather than taken from the libc.
//
// `struct ucred` is not exposed by the sprt shim on a cross target (SO_PEERCRED itself is), and this
// is one getsockopt result rather than a type the rest of the tree needs -- so the three fields are
// spelled out locally instead of growing the libc surface for them. The layout is the kernel's and
// is stable ABI: three 32-bit values, pid then uid then gid.
struct PeerCred {
	uint32_t pid;
	uint32_t uid;
	uint32_t gid;
};

static PeerIdentity readPeerIdentity(int fd) {
	PeerIdentity id;
#if defined(SO_PEERCRED)
	PeerCred cred{};
	socklen_t len = sizeof(cred);
	if (::getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &cred, &len) == 0 && len == sizeof(cred)) {
		id.uid = int64_t(cred.uid);
		id.gid = int64_t(cred.gid);
		id.pid = int64_t(cred.pid);
		// The kernel vouches for this: it is not a claim the peer made, which is what makes it
		// stronger than the bearer key and lets the server stop requiring one.
		id.authenticated = true;
		id.description = toString("unix:uid=", id.uid, " pid=", id.pid);
	}
#endif
	return id;
}

class UnixStream : public TransportStream {
public:
	virtual ~UnixStream() = default;

	bool init(int fd) {
		_fd = fd;
		return _fd >= 0;
	}

	void invalidate() { _fd = -1; }

	virtual Status write(BytesView data, size_t &written) override {
		written = 0;
		if (_fd < 0) {
			return Status::ErrorNotPermitted;
		}
		if (data.empty()) {
			return Status::Ok;
		}
		// MSG_NOSIGNAL: a peer that went away must surface as EPIPE here, not as SIGPIPE killing the
		// process.
		auto n = ::send(_fd, data.data(), data.size(), MSG_NOSIGNAL);
		if (n >= 0) {
			written = size_t(n);
			return Status::Ok;
		}
		if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
			return Status::Ok; // backpressure, not an error
		}
		return Status::ErrorNotPermitted;
	}

	virtual Status read(uint8_t *buf, size_t len, size_t &got) override {
		got = 0;
		if (_fd < 0) {
			return Status::ErrorNotPermitted;
		}
		if (len == 0) {
			return Status::Ok;
		}
		auto n = ::recv(_fd, buf, len, 0);
		if (n > 0) {
			got = size_t(n);
			return Status::Ok;
		}
		if (n == 0) {
			_eof = true; // orderly shutdown by the peer
			return Status::Ok;
		}
		if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
			return Status::Ok; // drained, not closed
		}
		return Status::ErrorNotPermitted;
	}

	virtual bool isClosed() const override { return _fd < 0 || _eof; }

protected:
	int _fd = -1; // borrowed from the owning connection
	bool _eof = false;
};

class UnixConnection : public TransportConnection {
public:
	virtual ~UnixConnection();

	bool init(int fd) {
		_fd = fd;
		if (_fd < 0 || !unixSetNonBlocking(_fd)) {
			return false;
		}
		_stream = Rc<UnixStream>::create(_fd);
		if (!_stream) {
			return false;
		}
		_peer = readPeerIdentity(_fd);
		return true;
	}

	virtual TransportCaps getCaps() const override {
		// No Encrypted: the bytes never leave the kernel, so there is nothing to encrypt them
		// against -- claiming it would be a lie the policy layer might trust.
		// No MultiStream: one ordered stream, so every StreamClass folds onto it.
		return TransportCaps::PeerAuthenticated | TransportCaps::Pollable;
	}

	virtual TransportStream *getStream(StreamClass) override { return _stream; }
	virtual const PeerIdentity &getPeerIdentity() const override { return _peer; }

	virtual sprt::dispatch::NativeHandle getPollHandle() const override {
		return sprt::dispatch::NativeHandle(_fd);
	}

	// Nothing to service: a stream socket has no timers of its own, and the kernel already moved the
	// bytes. Readability is what the looper waits on.
	virtual Status handleEvents() override { return Status::Ok; }

	virtual bool isClosed() override { return _fd < 0 || _stream->isClosed(); }

	virtual void close(bool graceful) override {
		if (_fd < 0) {
			return;
		}
		if (graceful) {
			// Half-close: the peer sees EOF after draining everything we already wrote, which is what
			// lets a final message still arrive.
			::shutdown(_fd, SHUT_WR);
		}
		_stream->invalidate();
		::close(_fd);
		_fd = -1;
	}

protected:
	int _fd = -1;
	Rc<UnixStream> _stream;
	PeerIdentity _peer;
};

__SPRT_PUSH_ALLOW_CXXABI_ALLOC

UnixConnection::~UnixConnection() {
	if (_fd >= 0) {
		if (_stream) {
			_stream->invalidate();
		}
		::close(_fd);
		_fd = -1;
	}
}

__SPRT_POP_ALLOW_CXXABI_ALLOC

class UnixListener : public TransportListener {
public:
	virtual ~UnixListener();

	virtual Status open(const Address &, const TransportServerConfig &) override;
	virtual bool isOpen() const override { return _fd >= 0; }
	virtual void close() override;

	virtual sprt::dispatch::NativeHandle getPollHandle() const override {
		return sprt::dispatch::NativeHandle(_fd);
	}
	virtual uint64_t getEventTimeout() const override { return maxOf<uint64_t>(); }

	virtual void handleEvents(const Callback<void(Rc<TransportConnection> &&)> &) override;

protected:
	int _fd = -1;
	String _path;
};

__SPRT_PUSH_ALLOW_CXXABI_ALLOC

UnixListener::~UnixListener() { close(); }

__SPRT_POP_ALLOW_CXXABI_ALLOC

Status UnixListener::open(const Address &addr, const TransportServerConfig &cfg) {
	struct sockaddr_un sa;
	socklen_t len = 0;
	if (!unixMakeAddr(addr.path, sa, len)) {
		return Status::ErrorInvalidArguemnt;
	}

	int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0) {
		log::source().error("remote::unix", "socket() failed: ", errno);
		return Status::ErrorNotPermitted;
	}

	// A socket file left behind by a crashed process would make bind() fail with EADDRINUSE for ever.
	// Removing it is safe precisely because bind() below is what re-creates it: a LIVE listener still
	// holding the path keeps working through its own fd, and the next connect() then finds our new
	// socket -- which is why two servers must not share a path (the caller picks per-session names).
	::unlink(addr.path.data());

	if (::bind(fd, (struct sockaddr *)&sa, len) != 0) {
		log::source().error("remote::unix", "bind(", addr.path, ") failed: ", errno);
		::close(fd);
		return Status::ErrorNotPermitted;
	}

	// The socket's permissions ARE the access control here -- there is no bearer key to fall back on
	// once PeerAuthenticated lets the server drop it.
	if (::chmod(addr.path.data(), mode_t(cfg.socketMode)) != 0) {
		log::source().warn("remote::unix", "chmod(", addr.path, ") failed: ", errno);
	}

	if (!unixSetNonBlocking(fd) || ::listen(fd, 8) != 0) {
		log::source().error("remote::unix", "listen(", addr.path, ") failed: ", errno);
		::close(fd);
		::unlink(addr.path.data());
		return Status::ErrorNotPermitted;
	}

	_fd = fd;
	_path = addr.path;
	log::source().info("remote::unix", "listening on ", addr.description());
	return Status::Ok;
}

void UnixListener::close() {
	if (_fd >= 0) {
		::close(_fd);
		_fd = -1;
	}
	if (!_path.empty()) {
		::unlink(_path.data());
		_path.clear();
	}
}

void UnixListener::handleEvents(const Callback<void(Rc<TransportConnection> &&)> &onAccept) {
	if (_fd < 0) {
		return;
	}
	for (uint32_t n = 0; n < kUnixMaxAcceptsPerPump; ++n) {
		int cfd = ::accept(_fd, nullptr, nullptr);
		if (cfd < 0) {
			if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
				log::source().error("remote::unix", "accept() failed: ", errno);
			}
			break;
		}
		auto conn = Rc<UnixConnection>::create(cfd);
		if (conn) {
			log::source().info("remote::unix", "accepted a client connection (",
					conn->getPeerIdentity().getDescription(), ")");
			onAccept(Rc<TransportConnection>(conn.get()));
		} else {
			::close(cfd);
		}
	}
}

class UnixTransport : public Transport {
public:
	virtual ~UnixTransport() = default;

	virtual AddressScheme getScheme() const override { return AddressScheme::Unix; }

	virtual TransportCaps getCaps() const override {
		return TransportCaps::PeerAuthenticated | TransportCaps::Pollable;
	}

	virtual Rc<TransportConnection> connect(const Address &addr,
			const TransportClientConfig &) override {
		struct sockaddr_un sa;
		socklen_t len = 0;
		if (!unixMakeAddr(addr.path, sa, len)) {
			return nullptr;
		}
		int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
		if (fd < 0) {
			log::source().error("remote::unix", "socket() failed: ", errno);
			return nullptr;
		}
		// Connect while still blocking: an AF_UNIX connect either succeeds or fails at once (there is
		// no network round trip), so this costs nothing and keeps the error handling simple. The
		// socket goes non-blocking in UnixConnection::init, before any protocol byte moves.
		if (::connect(fd, (struct sockaddr *)&sa, len) != 0) {
			log::source().error("remote::unix", "connect(", addr.path, ") failed: ", errno);
			::close(fd);
			return nullptr;
		}
		auto conn = Rc<UnixConnection>::create(fd);
		if (!conn) {
			::close(fd);
			return nullptr;
		}
		log::source().info("remote::unix", "connected to ", addr.description());
		return Rc<TransportConnection>(conn.get());
	}

	virtual Rc<TransportListener> listen(const Address &addr,
			const TransportServerConfig &cfg) override {
		auto l = Rc<UnixListener>::create();
		if (!l || l->open(addr, cfg) != Status::Ok) {
			return nullptr;
		}
		return Rc<TransportListener>(l.get());
	}
};

} // namespace

void registerUnixTransport() { TransportRegistry::registerTransport(Rc<UnixTransport>::create()); }

} // namespace stappler::xenolith::remote

#else

namespace STAPPLER_VERSIONIZED stappler::xenolith::remote {

// Not available in this build: without the credentials the transport could not claim PeerAuthenticated, which
// is the whole reason to prefer it locally.
void registerUnixTransport() { }

} // namespace stappler::xenolith::remote

#endif
