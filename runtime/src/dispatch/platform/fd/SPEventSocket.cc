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

#include "SPEventSocket.h"
#include "../../detail/SPRuntimeDispatchQueueData.h"

#include <sprt/runtime/status.h>
#include <sprt/runtime/log.h>
#include <sprt/c/__sprt_errno.h>
#include <sprt/c/__sprt_fcntl.h>
#include <sprt/c/__sprt_unistd.h>
#include <sprt/c/sys/__sprt_socket.h>
#include <sprt/c/sys/__sprt_poll.h>

#if SPRT_WINDOWS
// Winsock reports socket errors through WSAGetLastError(), not errno; the
// wrapper header also provides FIONBIO and the WSAE* constants.
#include <sprt/wrappers/windows/windows.h>
#include <sprt/wrappers/windows/winsock.h>
#endif

namespace sprt::dispatch {

namespace {

// AF_UNIX address layout: the sprt socket surface has no __sprt_sockaddr_un, so
// define the POD here. Linux/Android/Windows(afunix.h) share the layout; the
// BSD variants carry a length byte first.
struct SockAddrUn {
#if SPRT_APPLE
	uint8_t sun_len;
	uint8_t sun_family;
#else
	uint16_t sun_family;
#endif
	char sun_path[108];
};

union AnySockAddr {
	struct __SPRT_SOCKADDR_NAME base;
	struct __SPRT_SOCKADDR_IN_NAME v4;
	struct __SPRT_SOCKADDR_IN6_NAME v6;
	SockAddrUn un; // also the largest member - covers every address this API produces
};

// --- error plumbing ----------------------------------------------------------

#if SPRT_WINDOWS

static int lastSockError() { return ::WSAGetLastError(); }

static bool isWouldBlock(int e) { return e == WSAEWOULDBLOCK; }
static bool isInterrupted(int e) { return false; } // winsock calls do not EINTR
static bool isConnectInProgress(int e) { return e == WSAEWOULDBLOCK || e == WSAEINPROGRESS; }

static Status sockErrorToStatus(int e) {
	// translate the common WSA codes onto their POSIX errno counterparts so the
	// Status space stays uniform across platforms
	switch (e) {
	case WSAEWOULDBLOCK: return status::errnoToStatus(EWOULDBLOCK);
	case WSAEINPROGRESS: return status::errnoToStatus(EINPROGRESS);
	case WSAEADDRINUSE: return status::errnoToStatus(EADDRINUSE);
	case WSAEADDRNOTAVAIL: return status::errnoToStatus(EADDRNOTAVAIL);
	case WSAECONNABORTED: return status::errnoToStatus(ECONNABORTED);
	case WSAECONNRESET: return status::errnoToStatus(ECONNRESET);
	case WSAETIMEDOUT: return status::errnoToStatus(ETIMEDOUT);
	case WSAECONNREFUSED: return status::errnoToStatus(ECONNREFUSED);
	case WSAEACCES: return status::errnoToStatus(EACCES);
	default: return status::errnoToStatus(EIO);
	}
}

static bool isAcceptTransient(int e) { return e == WSAECONNABORTED; }

static bool setNonBlocking(SocketHandle sock) {
	unsigned long one = 1;
	return ::__sprt_ioctlsocket(SOCKET(sock), long(FIONBIO), &one) == 0;
}

static constexpr int kSendFlags = 0; // Windows has no SIGPIPE (nor MSG_NOSIGNAL)

#else

static int lastSockError() { return __sprt_errno; }

static bool isWouldBlock(int e) { return e == EAGAIN || e == EWOULDBLOCK; }
static bool isInterrupted(int e) { return e == EINTR; }
static bool isConnectInProgress(int e) { return e == EINPROGRESS; }

static Status sockErrorToStatus(int e) { return status::errnoToStatus(e); }

static bool isAcceptTransient(int e) { return e == ECONNABORTED; }

static bool setNonBlocking(SocketHandle sock) {
	auto fl = ::__sprt_fcntl(int(sock), __SPRT_F_GETFL, 0);
	if (fl < 0) {
		return false;
	}
	return ::__sprt_fcntl(int(sock), __SPRT_F_SETFL, fl | __SPRT_O_NONBLOCK) == 0;
}

// suppress SIGPIPE on writes to a peer-closed socket (present on Linux,
// Android and modern Darwin; the wrapper static_asserts the value)
static constexpr int kSendFlags = __SPRT_MSG_NOSIGNAL;

#endif

// --- address translation -----------------------------------------------------

static void writeBe16(void *target, uint16_t value) {
	auto p = reinterpret_cast<uint8_t *>(target);
	p[0] = uint8_t(value >> 8);
	p[1] = uint8_t(value & 0xFF);
}

static uint16_t readBe16(const void *source) {
	auto p = reinterpret_cast<const uint8_t *>(source);
	return uint16_t((uint16_t(p[0]) << 8) | uint16_t(p[1]));
}

// parse a dotted-quad IPv4 literal into 4 network-order bytes
static bool parseIPv4(StringView text, uint8_t out[4]) {
	uint32_t octet = 0;
	uint32_t digits = 0;
	uint32_t idx = 0;
	for (size_t i = 0; i < text.size(); ++i) {
		auto c = text[i];
		if (c >= '0' && c <= '9') {
			octet = octet * 10 + uint32_t(c - '0');
			if (++digits > 3 || octet > 255) {
				return false;
			}
		} else if (c == '.') {
			if (digits == 0 || idx >= 3) {
				return false;
			}
			out[idx++] = uint8_t(octet);
			octet = 0;
			digits = 0;
		} else {
			return false;
		}
	}
	if (digits == 0 || idx != 3) {
		return false;
	}
	out[idx] = uint8_t(octet);
	return true;
}

// parse an IPv6 literal (hex groups, one optional "::" compression, optional
// embedded IPv4 tail) into 16 network-order bytes; zone ids are not supported
static bool parseIPv6(StringView text, uint8_t out[16]) {
	uint16_t groups[8];
	uint32_t nGroups = 0;
	int32_t zerosAt = -1; // group index where "::" expands
	uint8_t v4tail[4];
	bool hasV4 = false;

	size_t i = 0;
	const size_t n = text.size();
	if (n >= 2 && text[0] == ':' && text[1] == ':') {
		zerosAt = 0;
		i = 2;
	} else if (n > 0 && text[0] == ':') {
		return false;
	}

	while (i < n) {
		// try an embedded IPv4 tail ("::ffff:127.0.0.1")
		size_t j = i;
		bool dotted = false;
		while (j < n && text[j] != ':') {
			if (text[j] == '.') {
				dotted = true;
			}
			++j;
		}
		if (dotted) {
			if (j != n || nGroups > 6 || !parseIPv4(text.sub(i), v4tail)) {
				return false;
			}
			hasV4 = true;
			i = n;
			break;
		}

		uint32_t value = 0;
		uint32_t digits = 0;
		while (i < n) {
			auto c = text[i];
			uint32_t d;
			if (c >= '0' && c <= '9') {
				d = uint32_t(c - '0');
			} else if (c >= 'a' && c <= 'f') {
				d = uint32_t(c - 'a') + 10;
			} else if (c >= 'A' && c <= 'F') {
				d = uint32_t(c - 'A') + 10;
			} else {
				break;
			}
			value = (value << 4) | d;
			if (++digits > 4) {
				return false;
			}
			++i;
		}
		if (digits == 0 || nGroups >= 8) {
			return false;
		}
		groups[nGroups++] = uint16_t(value);

		if (i < n) {
			if (text[i] != ':') {
				return false;
			}
			++i;
			if (i < n && text[i] == ':') {
				if (zerosAt >= 0) {
					return false; // second "::"
				}
				zerosAt = int32_t(nGroups);
				++i;
				if (i == n) {
					break; // trailing "::"
				}
			} else if (i == n) {
				return false; // trailing single ':'
			}
		}
	}

	const uint32_t have = nGroups + (hasV4 ? 2 : 0);
	if ((zerosAt < 0 && have != 8) || (zerosAt >= 0 && have >= 8)) {
		return false;
	}

	// lay the groups out around the "::" expansion
	uint16_t full[8] = {0};
	const uint32_t tail = nGroups - uint32_t(zerosAt < 0 ? 0 : zerosAt);
	const uint32_t head = nGroups - tail;
	for (uint32_t k = 0; k < head; ++k) { full[k] = groups[k]; }
	uint32_t base = 8 - (hasV4 ? 2 : 0) - tail;
	for (uint32_t k = 0; k < tail; ++k) { full[base + k] = groups[head + k]; }
	for (uint32_t k = 0; k < 8; ++k) {
		out[k * 2] = uint8_t(full[k] >> 8);
		out[k * 2 + 1] = uint8_t(full[k] & 0xFF);
	}
	if (hasV4) {
		out[12] = v4tail[0];
		out[13] = v4tail[1];
		out[14] = v4tail[2];
		out[15] = v4tail[3];
	}
	return true;
}

static bool parsePort(StringView text, uint16_t &out) {
	if (text.empty() || text.size() > 5) {
		return false;
	}
	uint32_t value = 0;
	for (size_t i = 0; i < text.size(); ++i) {
		auto c = text[i];
		if (c < '0' || c > '9') {
			return false;
		}
		value = value * 10 + uint32_t(c - '0');
	}
	if (value > 65'535) {
		return false;
	}
	out = uint16_t(value);
	return true;
}

// Build the native sockaddr for `addr`. Returns the address length to pass to
// bind/connect, or 0 on failure with *st set.
static __sprt_socklen_t makeSockAddr(const SocketAddress &addr, AnySockAddr &out, Status *st) {
	for (size_t i = 0; i < sizeof(AnySockAddr); ++i) {
		reinterpret_cast<uint8_t *>(&out)[i] = 0;
	}
	switch (addr.family) {
	case SocketAddress::Family::Unix: {
		const bool abstract = !addr.path.empty() && addr.path[0] == '@';
#if !SPRT_LINUX && !SPRT_ANDROID
		if (abstract) {
			*st = Status::ErrorNotSupported; // abstract namespace is Linux-only
			return 0;
		}
#endif
		const size_t nameLen = addr.path.size() - (abstract ? 1 : 0);
		if (nameLen == 0 || nameLen >= sizeof(out.un.sun_path) - 1) {
			*st = Status::ErrorInvalidArguemnt;
			return 0;
		}
#if SPRT_APPLE
		out.un.sun_family = uint8_t(__SPRT_AF_UNIX);
#else
		out.un.sun_family = uint16_t(__SPRT_AF_UNIX);
#endif
		const size_t pathOffset = offsetof(SockAddrUn, sun_path);
		__sprt_socklen_t len;
		if (abstract) {
			out.un.sun_path[0] = '\0';
			for (size_t i = 0; i < nameLen; ++i) { out.un.sun_path[1 + i] = addr.path[1 + i]; }
			len = __sprt_socklen_t(pathOffset + 1 + nameLen);
		} else {
			for (size_t i = 0; i < nameLen; ++i) { out.un.sun_path[i] = addr.path[i]; }
			len = __sprt_socklen_t(pathOffset + nameLen + 1);
		}
#if SPRT_APPLE
		out.un.sun_len = uint8_t(len);
#endif
		return len;
	}
	case SocketAddress::Family::IPv4: {
		uint8_t ip[4] = {127, 0, 0, 1}; // empty host = loopback (secure default)
		if (!addr.host.empty() && !parseIPv4(addr.host, ip)) {
			*st = Status::ErrorInvalidArguemnt;
			return 0;
		}
		out.v4.sin_family = decltype(out.v4.sin_family)(__SPRT_AF_INET);
		writeBe16(&out.v4.sin_port, addr.port);
		auto ap = reinterpret_cast<uint8_t *>(&out.v4.sin_addr);
		ap[0] = ip[0];
		ap[1] = ip[1];
		ap[2] = ip[2];
		ap[3] = ip[3];
#if SPRT_APPLE
		out.v4.sin_len = uint8_t(sizeof(out.v4));
#endif
		return __sprt_socklen_t(sizeof(out.v4));
	}
	case SocketAddress::Family::IPv6: {
		uint8_t ip[16] = {0};
		if (addr.host.empty()) {
			ip[15] = 1; // ::1 - loopback (secure default)
		} else if (!parseIPv6(addr.host, ip)) {
			*st = Status::ErrorInvalidArguemnt;
			return 0;
		}
		out.v6.sin6_family = decltype(out.v6.sin6_family)(__SPRT_AF_INET6);
		writeBe16(&out.v6.sin6_port, addr.port);
		auto ap = reinterpret_cast<uint8_t *>(&out.v6.sin6_addr);
		for (size_t i = 0; i < 16; ++i) { ap[i] = ip[i]; }
#if SPRT_APPLE
		out.v6.sin6_len = uint8_t(sizeof(out.v6));
#endif
		return __sprt_socklen_t(sizeof(out.v6));
	}
	default: break;
	}
	*st = Status::ErrorInvalidArguemnt;
	return 0;
}

static int addressFamily(const SocketAddress &addr) {
	switch (addr.family) {
	case SocketAddress::Family::Unix: return __SPRT_AF_UNIX; break;
	case SocketAddress::Family::IPv6: return __SPRT_AF_INET6; break;
	default: break;
	}
	return __SPRT_AF_INET;
}

static void fireCompletionError(CompletionHandle<ListenHandle> &c, Status st) {
	if (c.fn) {
		c.fn(c.userdata, nullptr, 0, st);
	}
}

static void fireCompletionError(CompletionHandle<StreamHandle> &c, Status st) {
	if (c.fn) {
		c.fn(c.userdata, nullptr, 0, st);
	}
}

// socket() + non-blocking; returns InvalidSocket with *st set on failure
static SocketHandle openSocket(int family, Status *st) {
	auto s = SocketHandle(::__sprt_socket(family, __SPRT_SOCK_STREAM, 0));
	if (s == InvalidSocket) {
		*st = sockErrorToStatus(lastSockError());
		return InvalidSocket;
	}
	if (!setNonBlocking(s)) {
		*st = sockErrorToStatus(lastSockError());
		::__sprt_closesocket(SOCKET(s));
		return InvalidSocket;
	}
	return s;
}

} // namespace

Rc<StreamState> makeStreamState(QueueData *q, SocketHandle sock, bool connecting) {
	auto state = Rc<StreamState>::alloc();
	state->qdata = q;
	state->sock = sock;
	state->connecting = connecting;
	state->readStopped = true; // no reader until read() is called
	return state;
}

Rc<StreamHandle> makeSocketStreamPollHandle(QueueData *q, Rc<StreamState> &&state) {
	auto h = Rc<SocketStreamHandle>::create(&q->_socketStreamClass);
	if (!h) {
		return nullptr;
	}
	h->setUserdata(state.get());
	state->handle = h.get();
	return h;
}

Rc<ListenHandle> makeSocketListenPollHandle(QueueData *q, Rc<ListenState> &&state) {
	CompletionHandle<void> completion;
	completion = state->pendingCompletion;
	auto h = Rc<SocketListenHandle>::create(&q->_socketListenClass, sprt::move(completion));
	if (!h) {
		return nullptr;
	}
	h->setUserdata(state.get());
	state->handle = h.get();
	return h;
}

Rc<StreamHandle> QueueData::makeStreamFromSocket(SocketHandle sock, bool connecting) {
	auto state = makeStreamState(this, sock, connecting);
	if (_makeSocketStream) {
		return _makeSocketStream(this, _platformQueue, sprt::move(state));
	}
	return makeSocketStreamPollHandle(this, sprt::move(state));
}

// --- SocketAddress -----------------------------------------------------------

SocketAddress SocketAddress::parse(StringView text) {
	SocketAddress ret;
	if (text.empty()) {
		return ret;
	}
	if (text.starts_with("unix:")) {
		auto path = text.sub(5);
		const bool abstract = path.starts_with('@');
		const size_t nameLen = path.size() - (abstract ? 1 : 0);
		if (nameLen == 0 || nameLen >= 107) {
			return ret;
		}
		ret.family = Family::Unix;
		ret.path = String(path.data(), path.size());
		return ret;
	}

	// "[v6-literal]:port"
	if (text.starts_with('[')) {
		size_t close = 0;
		for (size_t i = 1; i < text.size(); ++i) {
			if (text[i] == ']') {
				close = i;
				break;
			}
		}
		if (close == 0 || close + 1 >= text.size() || text[close + 1] != ':') {
			return ret;
		}
		auto host = text.sub(1, close - 1);
		uint16_t port = 0;
		uint8_t ip[16];
		if (host.empty() || !parseIPv6(host, ip) || !parsePort(text.sub(close + 2), port)) {
			return ret;
		}
		ret.family = Family::IPv6;
		ret.host = String(host.data(), host.size());
		ret.port = port;
		return ret;
	}

	// split on the LAST ':' - "host:port" or ":port"
	size_t colon = text.size();
	for (size_t i = text.size(); i > 0; --i) {
		if (text[i - 1] == ':') {
			colon = i - 1;
			break;
		}
	}
	if (colon == text.size()) {
		return ret;
	}

	uint16_t port = 0;
	if (!parsePort(text.sub(colon + 1), port)) {
		return ret;
	}
	auto host = text.sub(0, colon);
	if (!host.empty()) {
		uint8_t ip[4];
		if (!parseIPv4(host, ip)) {
			return ret; // DNS names are future work
		}
	}
	ret.family = Family::IPv4;
	ret.host = String(host.data(), host.size());
	ret.port = port;
	return ret;
}

String SocketAddress::description() const {
	switch (family) {
	case Family::Unix: return toString("unix:", path); break;
	case Family::IPv4:
		return toString(host.empty() ? StringView("127.0.0.1") : StringView(host), ":", port);
		break;
	case Family::IPv6:
		return toString("[", host.empty() ? StringView("::1") : StringView(host), "]:", port);
		break;
	default: break;
	}
	return String();
}

// --- SocketState -------------------------------------------------------------

SocketState::~SocketState() { closeSocket(); }

void SocketState::closeSocket() {
	if (sock != InvalidSocket) {
		::__sprt_closesocket(SOCKET(sock));
		sock = InvalidSocket;
	}
}

bool SocketState::startPoller(CompletionHandle<PollHandle> &&cb) {
	if (!qdata || !qdata->_socketPoll) {
		return false;
	}
	poller = qdata->_socketPoll(qdata, qdata->_platformQueue, sock, interest, sprt::move(cb));
	if (!poller) {
		return false;
	}
	// The poller pins the socket handle (which owns this state) for its whole
	// lifetime, so its completion can never observe a freed state. The cycle
	// handle -> state -> poller -> handle is broken in cancelFn, which drops
	// state->poller after cancelling it.
	poller->setUserdata(handle);
	if (qdata->runHandle(poller) != Status::Ok) {
		poller = nullptr;
		return false;
	}
	return true;
}

void SocketState::setInterest(PollFlags flags) {
	if (interest == flags) {
		return;
	}
	interest = flags;
	if (!poller || poller->getStatus() != Status::Ok) {
		return;
	}
	// Apply SYNCHRONOUSLY, even from inside a notify callback (the event batch
	// pins handles, and PollFdEPollHandle::notify itself cancels from notify -
	// this is a supported reentry). Deferring the reset must NOT be done: the
	// io_uring poll handle auto-rearms with the OLD mask before delivering the
	// completion, so an always-ready fd (e.g. writable after a full flush, or
	// readable at EOF) would generate a CQE storm that keeps the queue's pop
	// loop busy and starves any deferred task forever.
	poller->reset(flags);
}

void SocketState::finalizeSocket(Status st) {
	if (finalized) {
		return;
	}
	finalized = true;
	if (!handle) {
		return;
	}
	// Cancel directly - Handle::cancel is itself scheduled asynchronously and is
	// safe to call from a notify callback (see PollFdEPollHandle::notify); going
	// through a deferred perform() could be starved by an event storm.
	Rc<Handle> h(handle);
	h->cancel(st);
}

// --- ListenState -------------------------------------------------------------

ListenState::~ListenState() { }

void ListenState::handleEvents(PollFlags flags, Status st) {
	if (st != Status::Ok) {
		// the poller terminated (backend teardown or self-cancel on error)
		if (!terminating) {
			finalizeSocket(st);
		}
		return;
	}
	if (terminating || finalized) {
		return;
	}
	if (hasFlag(flags, PollFlags::Err) || hasFlag(flags, PollFlags::Invalid)) {
		finalizeSocket(sockErrorToStatus(EIO));
		return;
	}

	for (;;) {
		auto c = SocketHandle(
				::__sprt_accept4(SOCKET(sock), nullptr, nullptr, __SPRT_SOCK_NONBLOCK));
		if (c == InvalidSocket) {
			auto e = lastSockError();
			if (isInterrupted(e)) {
				continue;
			}
			if (isAcceptTransient(e)) {
				continue; // connection died in the backlog; keep accepting
			}
			if (!isWouldBlock(e)) {
				// EMFILE/ENFILE and друзья: stay armed and retry on the next
				// readiness event instead of killing the listener
				oslog::vperror(__SPRT_LOCATION, "dispatch::Socket", "accept() failed: ", e);
			}
			break;
		}

		auto stream = qdata->makeStreamFromSocket(c, false);
		if (!stream) {
			::__sprt_closesocket(SOCKET(c));
			continue;
		}
		if (qdata->runHandle(stream) != Status::Ok) {
			continue; // handle release closes the socket via state teardown
		}
		if (onAccept) {
			onAccept(Rc<StreamHandle>(stream.get()));
		} else {
			stream->cancel();
		}
	}
}

// --- StreamState -------------------------------------------------------------

static int getSoError(SocketHandle sock) {
	int err = 0;
	__sprt_socklen_t len = __sprt_socklen_t(sizeof(err));
	if (::__sprt_getsockopt(SOCKET(sock), __SPRT_SOL_SOCKET, __SPRT_SO_ERROR,
				reinterpret_cast<sockdata_t *>(&err), &len)
			!= 0) {
		return EIO;
	}
	return err;
}

void StreamState::fireConnect(Status st) {
	if (connectFired) {
		return;
	}
	connectFired = true;
	if (connectCompletion.fn) {
		auto c = connectCompletion;
		connectCompletion = ConnectInfo::Completion();
		c.fn(c.userdata, static_cast<StreamHandle *>(handle), 0, st);
	}
}

void StreamState::handleEvents(PollFlags flags, Status st) {
	if (st != Status::Ok) {
		if (!terminating && !finalized) {
			closeStatus = st;
			finalizeSocket(st);
		}
		return;
	}
	if (terminating || finalized) {
		return;
	}

	if (connecting) {
		// non-blocking connect resolution: writable (or error) -> SO_ERROR
		auto err = getSoError(sock);
		if (err != 0) {
			auto s = sockErrorToStatus(err);
			closeStatus = s;
			fireConnect(s);
			finalizeSocket(s);
			return;
		}
		if (hasFlag(flags, PollFlags::Out) || hasFlag(flags, PollFlags::In)) {
			connecting = false;
			fireConnect(Status::Ok);
			if (terminating || finalized) {
				return; // the completion may cancel the handle
			}
			// flushes bytes queued while connecting AND issues a shutdownWrite()
			// that was requested before the connect resolved
			flushWrite();
			if (!terminating && !finalized) {
				updateInterest();
			}
		}
		return;
	}

	if (hasFlag(flags, PollFlags::Err) || hasFlag(flags, PollFlags::Invalid)) {
		auto err = getSoError(sock);
		auto s = sockErrorToStatus(err != 0 ? err : EIO);
		closeStatus = s;
		drainRead(); // deliver whatever is still buffered + EOF
		if (!finalized) {
			finalizeSocket(s);
		}
		return;
	}

	const bool hup = hasFlag(flags, PollFlags::HungUp);
	// A backend may report readiness bits outside the PollFlags vocabulary
	// (io_uring can deliver POLLRDHUP == 0x2000 even when not requested).
	// Attempting a drain on such events is always safe (EWOULDBLOCK breaks the
	// loop) and turns them into progress (EOF detection) instead of a storm.
	const bool unknown = (flags
								 & (PollFlags::In | PollFlags::Out | PollFlags::HungUp
										 | PollFlags::Err | PollFlags::Pri | PollFlags::Invalid))
			== PollFlags::None;
	if (hasFlag(flags, PollFlags::In) || hup || unknown) {
		drainRead();
	}
	if (!finalized && !terminating && hasFlag(flags, PollFlags::Out)) {
		flushWrite();
	}
	if (hup && !finalized && !terminating) {
		// both directions are gone; nothing further can happen on this socket
		finalizeSocket(closeStatus);
	} else if (!finalized && !terminating) {
		checkFinished();
	}
}

void StreamState::drainRead() {
	if (readEof) {
		return;
	}
	for (;;) {
		auto n = ::__sprt_recv(SOCKET(sock), reinterpret_cast<sockdata_t *>(chunkBuf),
				SocketChunkSize, 0);
		if (n < 0) {
			auto e = lastSockError();
			if (isInterrupted(e)) {
				continue;
			}
			if (isWouldBlock(e)) {
				break;
			}
			auto s = sockErrorToStatus(e);
			closeStatus = s;
			readEof = true;
			finalizeSocket(s);
			return;
		}
		if (n == 0) {
			readEof = true;
			if (reader && !readStopped) {
				readStopped = true;
				reader(BytesView()); // EOF marker
			}
			if (!terminating && !finalized) {
				updateInterest(); // drop In: an EOF-ready socket stays readable forever
			}
			break;
		}
		if (!reader || readStopped) {
			// no active reader: leave data in the kernel buffer (backpressure);
			// In interest is not armed in this state, this is a stray event
			break;
		}
		if (reader(BytesView(chunkBuf, size_t(n))) != Status::Ok) {
			readStopped = true;
			if (!terminating && !finalized) {
				updateInterest();
			}
			break;
		}
		if (terminating || finalized) {
			break;
		}
	}
}

void StreamState::flushWrite() {
	if (shutdownDone || connecting || finalized) {
		return;
	}
	while (outPos < outBuf.size()) {
		auto n = ::__sprt_send(SOCKET(sock),
				reinterpret_cast<const sockdata_t *>(outBuf.data() + outPos),
				outBuf.size() - outPos, kSendFlags);
		if (n < 0) {
			auto e = lastSockError();
			if (isInterrupted(e)) {
				continue;
			}
			if (isWouldBlock(e)) {
				// compact the already-sent prefix and wait for writability
				if (outPos > 0) {
					outBuf.erase(outBuf.begin(), outBuf.begin() + outPos);
					outPos = 0;
				}
				updateInterest();
				return;
			}
			auto s = sockErrorToStatus(e);
			closeStatus = s;
			finalizeSocket(s);
			return;
		}
		outPos += size_t(n);
	}
	outBuf.clear();
	outPos = 0;
	if (shutdownRequested && !shutdownDone) {
		::__sprt_shutdown(SOCKET(sock), __SPRT_SHUT_WR);
		shutdownDone = true;
	}
	updateInterest();
	checkFinished();
}

void StreamState::engage() {
	if (engageFn) {
		engageFn(this);
	} else {
		updateInterest();
	}
}

void StreamState::updateInterest() {
	PollFlags desired = PollFlags::Err | PollFlags::HungUp;
	if (connecting) {
		desired |= PollFlags::Out;
	} else {
		if (reader && !readStopped && !readEof) {
			desired |= PollFlags::In;
		}
		if (outPos < outBuf.size() && !shutdownDone) {
			desired |= PollFlags::Out;
		}
	}
	setInterest(desired);
}

void StreamState::checkFinished() {
	if (finalized || terminating) {
		return;
	}
	// Finished when the peer closed its side AND our write side is shut down.
	// A stream with a stopped reader but no shutdown stays open until peer EOF.
	if (readEof && shutdownDone) {
		finalizeSocket(closeStatus);
	}
}

// --- handle implementations --------------------------------------------------

bool SocketListenHandle::init(HandleClass *cl, CompletionHandle<void> &&c) {
	return Handle::init(cl, sprt::move(c));
}

Status SocketListenHandle::start() {
	auto state = getState();
	if (!state || !state->qdata) {
		return Status::ErrorInvalidArguemnt;
	}
	state->interest = PollFlags::In | PollFlags::Err | PollFlags::HungUp;
	if (!state->startPoller(CompletionHandle<PollHandle>::create<ListenState>(state,
				[](ListenState *s, PollHandle *, uint32_t value, Status st) {
		s->handleEvents(PollFlags(value), st);
	}))) {
		return Status::ErrorNotImplemented;
	}
	return Status::Ok;
}

bool SocketStreamHandle::init(HandleClass *cl) {
	return Handle::init(cl, CompletionHandle<void>());
}

Status SocketStreamHandle::start() {
	auto state = getState();
	if (!state || !state->qdata) {
		return Status::ErrorInvalidArguemnt;
	}
	state->interest = PollFlags::Err | PollFlags::HungUp;
	if (state->connecting || state->outPos < state->outBuf.size()) {
		state->interest |= PollFlags::Out;
	}
	if (!state->startPoller(CompletionHandle<PollHandle>::create<StreamState>(state,
				[](StreamState *s, PollHandle *, uint32_t value, Status st) {
		s->handleEvents(PollFlags(value), st);
	}))) {
		return Status::ErrorNotImplemented;
	}
	if (!state->connecting) {
		// already-established socket (accepted, or connect() succeeded inline):
		// report the connect completion right away
		state->fireConnect(Status::Ok);
	}
	return Status::Ok;
}

// --- public ListenHandle / StreamHandle API ----------------------------------

const SocketAddress &ListenHandle::getAddress() const {
	static SocketAddress s_invalid;
	auto state = static_cast<ListenState *>(getUserdata());
	return state ? state->address : s_invalid;
}

SocketHandle ListenHandle::getSocket() const {
	auto state = static_cast<ListenState *>(getUserdata());
	return state ? state->sock : InvalidSocket;
}

Status StreamHandle::read(Function<Status(BytesView)> &&reader) {
	auto state = static_cast<StreamState *>(getUserdata());
	if (!state || state->terminating || state->finalized) {
		return Status::ErrorCancelled;
	}
	state->reader = sprt::move(reader);
	state->readStopped = false;
	if (state->readEof) {
		state->readStopped = true;
		state->reader(BytesView()); // already at EOF
		return Status::Ok;
	}
	if (!state->connecting) {
		state->engage();
	}
	return Status::Ok;
}

Status StreamHandle::write(BytesView data) {
	auto state = static_cast<StreamState *>(getUserdata());
	if (!state || state->terminating || state->finalized || state->shutdownRequested) {
		return Status::ErrorCancelled;
	}
	if (data.empty()) {
		return Status::Ok;
	}
	if (!state->connecting && state->outBuf.empty() && !state->sendBusy
			&& (state->poller || state->engageFn)) {
		// fast path: try to send inline before buffering (never while an async
		// send op is in flight - that would reorder the byte stream)
		size_t off = 0;
		while (off < data.size()) {
			auto n = ::__sprt_send(SOCKET(state->sock),
					reinterpret_cast<const sockdata_t *>(data.data() + off), data.size() - off,
					kSendFlags);
			if (n < 0) {
				auto e = lastSockError();
				if (isInterrupted(e)) {
					continue;
				}
				if (isWouldBlock(e)) {
					break;
				}
				auto s = sockErrorToStatus(e);
				state->closeStatus = s;
				state->finalizeSocket(s);
				return s;
			}
			off += size_t(n);
		}
		if (off == data.size()) {
			return Status::Ok;
		}
		data = BytesView(data.data() + off, data.size() - off);
	}
	state->outBuf.insert(state->outBuf.end(), data.data(), data.data() + data.size());
	if (!state->connecting) {
		state->engage();
	}
	return Status::Ok;
}

Status StreamHandle::shutdownWrite() {
	auto state = static_cast<StreamState *>(getUserdata());
	if (!state || state->terminating || state->finalized) {
		return Status::ErrorCancelled;
	}
	if (state->shutdownRequested) {
		return Status::ErrorAlreadyPerformed;
	}
	state->shutdownRequested = true;
	if (!state->connecting && !state->sendBusy && state->outPos >= state->outBuf.size()) {
		::__sprt_shutdown(SOCKET(state->sock), __SPRT_SHUT_WR);
		state->shutdownDone = true;
		state->engage();
		state->checkFinished();
	}
	return Status::Ok;
}

void StreamHandle::setCloseCallback(Function<void(Status)> &&cb) {
	auto state = static_cast<StreamState *>(getUserdata());
	if (state) {
		state->onClose = sprt::move(cb);
	}
}

SocketHandle StreamHandle::getSocket() const {
	auto state = static_cast<StreamState *>(getUserdata());
	return state ? state->sock : InvalidSocket;
}

// --- HandleClass setup -------------------------------------------------------

void setupSocketHandleClasses(QueueHandleClassInfo *info, QueueData *q) {
	auto lcl = &q->_socketListenClass;
	lcl->info = info;
	lcl->createFn = HandleClass::create;
	lcl->destroyFn = HandleClass::destroy;
	lcl->runFn = [](HandleClass *cl, Handle *handle, uint8_t data[Handle::DataSize]) {
		auto status = static_cast<SocketListenHandle *>(handle)->start();
		if (status == Status::Ok) {
			return HandleClass::run(cl, handle, data);
		}
		return status;
	};
	// suspend/resume are plain bookkeeping: the poller is a separate,
	// reactor-suspendable handle managed by the queue itself
	lcl->suspendFn = HandleClass::suspend;
	lcl->resumeFn = HandleClass::resume;
	lcl->cancelFn = [](HandleClass *cl, Handle *handle, uint8_t data[Handle::DataSize], Status st) {
		auto state = static_cast<ListenState *>(handle->getUserdata());
		if (state) {
			state->terminating = true;
			if (state->poller) {
				state->poller->cancel();
				state->poller = nullptr;
			}
			state->closeSocket();
			if (state->ownsUnixPath && !state->address.path.empty()
					&& state->address.path[0] != '@') {
				::__sprt_unlink(state->address.path.data());
			}
			// break handle <-> closure reference cycles
			state->onAccept = nullptr;
			state->userRef = nullptr;
			state->strategy = nullptr;
		}
		return HandleClass::cancel(cl, handle, data, st);
	};

	auto scl = &q->_socketStreamClass;
	scl->info = info;
	scl->createFn = HandleClass::create;
	scl->destroyFn = HandleClass::destroy;
	scl->runFn = [](HandleClass *cl, Handle *handle, uint8_t data[Handle::DataSize]) {
		auto status = static_cast<SocketStreamHandle *>(handle)->start();
		if (status == Status::Ok) {
			return HandleClass::run(cl, handle, data);
		}
		return status;
	};
	scl->suspendFn = HandleClass::suspend;
	scl->resumeFn = HandleClass::resume;
	scl->cancelFn = [](HandleClass *cl, Handle *handle, uint8_t data[Handle::DataSize], Status st) {
		auto state = static_cast<StreamState *>(handle->getUserdata());
		if (state) {
			state->terminating = true;
			if (state->poller) {
				state->poller->cancel();
				state->poller = nullptr;
			}
			const Status final = (st == Status::Done) ? state->closeStatus : st;
			state->fireConnect(isSuccessful(final) ? Status::ErrorCancelled : final);
			state->closeSocket();
			if (state->onClose) {
				auto cb = sprt::move(state->onClose);
				state->onClose = nullptr;
				cb(final);
			}
			// break handle <-> closure reference cycles
			state->reader = nullptr;
			state->userRef = nullptr;
			state->strategy = nullptr;
			state->engageFn = nullptr;
		}
		return HandleClass::cancel(cl, handle, data, st);
	};
}

// --- portable probe poller (timer + zero-timeout poll) -----------------------

// probe cadence; latency-throughput compromise for the debug/IPC use cases the
// probe-based backends serve
static constexpr uint32_t SocketProbeIntervalMs = 10;

namespace {

struct SocketProbeSource {
	SocketHandle sock = InvalidSocket;
	PollFlags flags = PollFlags::None;
	Handle *timer = nullptr; // kept alive by the queue registry until cancelled
};

} // namespace

bool SocketProbeHandle::init(HandleClass *cl, SocketHandle sock, PollFlags flags,
		CompletionHandle<PollHandle> &&c) {
	static_assert(sizeof(SocketProbeSource) <= Handle::DataSize
			&& sprt::is_standard_layout<SocketProbeSource>::value);
	if (!Handle::init(cl, sprt::move(c))) {
		return false;
	}
	auto source = new (_data) SocketProbeSource;
	source->sock = sock;
	source->flags = flags;
	return true;
}

NativeHandle SocketProbeHandle::getNativeHandle() const {
	return NativeHandle(reinterpret_cast<void *>(
			reinterpret_cast<const SocketProbeSource *>(_data)->sock));
}

bool SocketProbeHandle::reset(PollFlags flags) {
	// the probe reads `flags` on every tick - no rearm dance needed
	reinterpret_cast<SocketProbeSource *>(_data)->flags = flags;
	return true;
}

void SocketProbeHandle::probe() {
	if (_status != Status::Ok) {
		return;
	}
	auto source = reinterpret_cast<SocketProbeSource *>(_data);

	struct __SPRT_POLLFD_NAME p;
	p.fd = decltype(p.fd)(source->sock);
	p.events = 0;
	p.revents = 0;
	if (hasFlag(source->flags, PollFlags::In)) {
		p.events |= __SPRT_POLLIN;
	}
	if (hasFlag(source->flags, PollFlags::Out)) {
		p.events |= __SPRT_POLLOUT;
	}

	auto n = ::__sprt_poll(&p, 1, 0);
	if (n == 0) {
		return; // nothing ready
	}

	PollFlags pollFlags = PollFlags::None;
	if (n < 0) {
		pollFlags |= PollFlags::Err;
	} else {
		if (p.revents & __SPRT_POLLIN) {
			pollFlags |= PollFlags::In;
		}
		if (p.revents & __SPRT_POLLOUT) {
			pollFlags |= PollFlags::Out;
		}
		if (p.revents & __SPRT_POLLHUP) {
			pollFlags |= PollFlags::In | PollFlags::HungUp;
		}
		if (p.revents & __SPRT_POLLERR) {
			pollFlags |= PollFlags::Err;
		}
		if (p.revents & __SPRT_POLLNVAL) {
			pollFlags |= PollFlags::Err | PollFlags::Invalid;
		}
	}
	if (pollFlags != PollFlags::None) {
		sendCompletion(toInt(pollFlags), Status::Ok);
	}
}

Status SocketProbeHandle::startProbeTimer() {
	auto source = reinterpret_cast<SocketProbeSource *>(_data);
	if (source->timer) {
		return Status::Ok;
	}
	auto qdata = _class->info->data;

	TimerInfo tinfo;
	tinfo.timeout = TimeInterval::milliseconds(SocketProbeIntervalMs);
	tinfo.interval = TimeInterval::milliseconds(SocketProbeIntervalMs);
	tinfo.count = TimerInfo::Infinite;
	tinfo.completion = TimerInfo::Completion::create<SocketProbeHandle>(this,
			[](SocketProbeHandle *h, TimerHandle *, uint32_t, Status st) {
		if (st == Status::Ok) {
			h->probe();
		}
	});

	auto timer = qdata->scheduleTimer(sprt::move(tinfo));
	if (!timer) {
		return Status::ErrorNotImplemented;
	}
	// The timer's userdata pins this probe handle for the timer's lifetime (its
	// completion holds a raw `this`); the running timer itself is kept alive by
	// the queue registry until cancelled.
	timer->setUserdata(this);
	source->timer = timer.get();
	qdata->runHandle(timer.get());
	return Status::Ok;
}

void SocketProbeHandle::stopProbeTimer() {
	auto source = reinterpret_cast<SocketProbeSource *>(_data);
	if (source->timer) {
		source->timer->cancel();
		source->timer = nullptr;
	}
}

void setupSocketProbeClass(QueueHandleClassInfo *info, HandleClass *cl) {
	cl->info = info;
	cl->createFn = HandleClass::create;
	cl->destroyFn = HandleClass::destroy;
	cl->runFn = [](HandleClass *cl, Handle *handle, uint8_t data[Handle::DataSize]) {
		auto status = static_cast<SocketProbeHandle *>(handle)->startProbeTimer();
		if (status == Status::Ok) {
			return HandleClass::run(cl, handle, data);
		}
		return status;
	};
	cl->suspendFn = HandleClass::suspend;
	cl->resumeFn = HandleClass::resume;
	cl->cancelFn = [](HandleClass *cl, Handle *handle, uint8_t data[Handle::DataSize], Status st) {
		static_cast<SocketProbeHandle *>(handle)->stopProbeTimer();
		return HandleClass::cancel(cl, handle, data, st);
	};
}

Rc<PollHandle> makeSocketProbeHandle(QueueData *q, SocketHandle sock, PollFlags flags,
		CompletionHandle<PollHandle> &&cb) {
	return Rc<SocketProbeHandle>::create(&q->_socketProbeClass, sock, flags, sprt::move(cb));
}

// --- QueueData factories -----------------------------------------------------

Rc<ListenState> prepareListenState(QueueData *q, ListenInfo &&info, Ref *ref) {
	if (!info.address.isValid()) {
		fireCompletionError(info.completion, Status::ErrorInvalidArguemnt);
		return nullptr;
	}

	Status st = Status::Ok;
	AnySockAddr sa;
	auto len = makeSockAddr(info.address, sa, &st);
	if (len == 0) {
		fireCompletionError(info.completion, st);
		return nullptr;
	}

	const bool isUnix = info.address.family == SocketAddress::Family::Unix;
	auto sock = openSocket(addressFamily(info.address), &st);
	if (sock == InvalidSocket) {
		fireCompletionError(info.completion, st);
		return nullptr;
	}

	if (!isUnix) {
		int one = 1;
		::__sprt_setsockopt(SOCKET(sock), __SPRT_SOL_SOCKET, __SPRT_SO_REUSEADDR,
				reinterpret_cast<const sockdata_t *>(&one), __sprt_socklen_t(sizeof(one)));
	} else if (info.address.path[0] != '@') {
		// remove a stale socket file left by a crashed process
		::__sprt_unlink(info.address.path.data());
	}

	// Every platform sockdef carries __SPRT_SOMAXCONN: 128 on Linux/Darwin/Android, and
	// winsock's 0x7fffffff, which is not a length but its "give me the backlog maximum"
	// sentinel - clamping to it is what a Windows listen() wants.
	constexpr int maxBacklog = __SPRT_SOMAXCONN;
	int backlog = int(info.backlog);
	if (backlog <= 0 || backlog > maxBacklog) {
		backlog = maxBacklog;
	}

	if (::__sprt_bind(SOCKET(sock), &sa.base, len) != 0
			|| ::__sprt_listen(SOCKET(sock), backlog) != 0) {
		st = sockErrorToStatus(lastSockError());
		oslog::vperror(__SPRT_LOCATION, "dispatch::Socket", "fail to bind/listen on '",
				info.address.description(), "'");
		::__sprt_closesocket(SOCKET(sock));
		fireCompletionError(info.completion, st);
		return nullptr;
	}

	auto address = sprt::move(info.address);
	if (!isUnix && address.port == 0) {
		// resolve the ephemeral port
		AnySockAddr bound;
		__sprt_socklen_t blen = __sprt_socklen_t(sizeof(bound));
		if (::__sprt_getsockname(SOCKET(sock), &bound.base, &blen) == 0) {
			address.port = (int(bound.base.sa_family) == __SPRT_AF_INET6)
					? readBe16(&bound.v6.sin6_port)
					: readBe16(&bound.v4.sin_port);
		}
	}

	auto state = Rc<ListenState>::alloc();
	state->qdata = q;
	state->sock = sock;
	state->address = sprt::move(address);
	state->onAccept = sprt::move(info.onAccept);
	state->pendingCompletion = info.completion;
	state->ownsUnixPath = isUnix;
	state->userRef = ref;
	return state;
}

Rc<StreamState> prepareConnectState(QueueData *q, ConnectInfo &&info, Ref *ref) {
	if (!info.address.isValid()) {
		fireCompletionError(info.completion, Status::ErrorInvalidArguemnt);
		return nullptr;
	}

	Status st = Status::Ok;
	AnySockAddr sa;
	auto len = makeSockAddr(info.address, sa, &st);
	if (len == 0) {
		fireCompletionError(info.completion, st);
		return nullptr;
	}

	auto sock = openSocket(addressFamily(info.address), &st);
	if (sock == InvalidSocket) {
		fireCompletionError(info.completion, st);
		return nullptr;
	}

	bool connecting = false;
	if (::__sprt_connect(SOCKET(sock), &sa.base, len) != 0) {
		auto e = lastSockError();
		if (isConnectInProgress(e)) {
			connecting = true;
		} else {
			st = sockErrorToStatus(e);
			::__sprt_closesocket(SOCKET(sock));
			fireCompletionError(info.completion, st);
			return nullptr;
		}
	}

	auto state = makeStreamState(q, sock, connecting);
	state->connectCompletion = info.completion;
	state->userRef = ref;
	return state;
}

Rc<ListenHandle> QueueData::listenSocket(ListenInfo &&info, Ref *ref) {
	if (!_makeSocketListen && !_socketPoll) {
		fireCompletionError(info.completion, Status::ErrorNotImplemented);
		return nullptr;
	}
	auto state = prepareListenState(this, sprt::move(info), ref);
	if (!state) {
		return nullptr;
	}
	if (_makeSocketListen) {
		return _makeSocketListen(this, _platformQueue, sprt::move(state));
	}
	return makeSocketListenPollHandle(this, sprt::move(state));
}

Rc<StreamHandle> QueueData::connectSocket(ConnectInfo &&info, Ref *ref) {
	if (!_makeSocketStream && !_socketPoll) {
		fireCompletionError(info.completion, Status::ErrorNotImplemented);
		return nullptr;
	}
	auto state = prepareConnectState(this, sprt::move(info), ref);
	if (!state) {
		return nullptr;
	}
	if (_makeSocketStream) {
		return _makeSocketStream(this, _platformQueue, sprt::move(state));
	}
	return makeSocketStreamPollHandle(this, sprt::move(state));
}

} // namespace sprt::dispatch
