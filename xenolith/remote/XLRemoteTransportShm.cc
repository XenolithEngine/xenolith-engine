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

/* The `shm:` transport: ShmBlock rings in memory both peers see, handed out by a ShmProvider.
 *
 * `shm:@name` rendezvous inside one process; `shm:<path>` goes to the platform provider. The
 * transport has no descriptor: the looper waits on the doorbell word (getWaitAddress).
 */

#include "XLRemoteTransport.h"
#include "XLRemoteShmProvider.h"
#include "XLRemoteShmEmbox.h"

#if SPRT_LINUX || SPRT_EMBOX_ANY

namespace STAPPLER_VERSIONIZED stappler::xenolith::remote {

// The offsets the kernel writes on a task's death are part of the Embox contract.
static_assert(offsetof(ShmBlockHeader, closed) == XL_SHM_HEADER_CLOSED_OFFSET);
static_assert(offsetof(ShmBlockHeader, doorbell) == XL_SHM_HEADER_DOORBELL_OFFSET);
static_assert(sizeof(struct xl_wm_connect) == XL_WM_CONNECT_SIZE);

namespace {

class ShmConnection;

class ShmStream : public TransportStream {
public:
	virtual ~ShmStream() = default;

	bool init(ShmConnection *connection, StreamClass c) {
		_connection = connection;
		_class = c;
		return true;
	}

	void invalidate() { _connection = nullptr; }

	virtual Status write(BytesView data, size_t &written) override;
	virtual Status read(uint8_t *buf, size_t len, size_t &got) override;
	virtual bool isClosed() const override;

protected:
	ShmConnection *_connection = nullptr; // the owning connection
	StreamClass _class = StreamClass::Control;
};

class ShmConnection : public TransportConnection {
public:
	virtual ~ShmConnection();

	bool init(Rc<ShmBlockMemory> &&, ShmSide, PeerIdentity &&);

	virtual TransportCaps getCaps() const override {
		auto caps = TransportCaps::MultiStream;
		if (_peer.authenticated) {
			caps |= TransportCaps::PeerAuthenticated;
		}
		return caps;
	}

	virtual TransportStream *getStream(StreamClass c) override {
		return c == StreamClass::Bulk ? _bulk.get() : _control.get();
	}

	virtual const PeerIdentity &getPeerIdentity() const override { return _peer; }

	virtual TransportWaitAddress getWaitAddress() override;

	virtual Status handleEvents() override;

	virtual bool isClosed() override;
	virtual void close(bool graceful = true) override;

	ShmEndpoint &getEndpoint() { return _endpoint; }
	bool isPeerGone() const { return _peerGone; }
	bool isClosedLocally() const { return _closed; }

protected:
	Rc<ShmBlockMemory> _memory;
	ShmEndpoint _endpoint;
	Rc<ShmStream> _control;
	Rc<ShmStream> _bulk;
	PeerIdentity _peer;
	bool _peerGone = false;
	bool _closed = false;
};

Status ShmStream::write(BytesView data, size_t &written) {
	written = 0;
	if (!_connection || _connection->isClosedLocally() || _connection->isPeerGone()) {
		return Status::ErrorNotPermitted;
	}
	return _connection->getEndpoint().write(_class, data, written);
}

Status ShmStream::read(uint8_t *buf, size_t len, size_t &got) {
	got = 0;
	if (!_connection || _connection->isClosedLocally()) {
		return Status::ErrorNotPermitted;
	}
	auto st = _connection->getEndpoint().read(_class, buf, len, got);
	if (st == Status::Ok && got == 0 && len > 0 && _connection->isPeerGone()
			&& !_connection->getEndpoint().hasPending()) {
		return Status::ErrorNotPermitted;
	}
	return st;
}

bool ShmStream::isClosed() const { return !_connection || _connection->isClosed(); }

__SPRT_PUSH_ALLOW_CXXABI_ALLOC

// The memory outlives close(): a wait on the doorbell may still be armed until the owner of the
// connection cancels it and lets go of the connection.
ShmConnection::~ShmConnection() {
	close(false);
	_endpoint.detach();
}

__SPRT_POP_ALLOW_CXXABI_ALLOC

bool ShmConnection::init(Rc<ShmBlockMemory> &&memory, ShmSide side, PeerIdentity &&peer) {
	_memory = sp::move(memory);
	if (!_memory || _endpoint.attach(_memory->getData(), _memory->getSize(), side) != Status::Ok) {
		return false;
	}

	_control = Rc<ShmStream>::create(this, StreamClass::Control);
	_bulk = Rc<ShmStream>::create(this, StreamClass::Bulk);
	if (!_control || !_bulk) {
		return false;
	}

	_peer = sp::move(peer);
	return true;
}

Status ShmConnection::handleEvents() {
	if (_closed) {
		return Status::Ok;
	}
	if (!_peerGone && !_memory->isPeerAlive()) {
		_peerGone = true;
		log::source().info("remote::shm", "peer is gone (", _peer.getDescription(), ")");
	}
	return _endpoint.isCorrupted() ? Status::ErrorInvalidArguemnt : Status::Ok;
}

bool ShmConnection::isClosed() {
	return _closed || _endpoint.isClosed() || (_peerGone && !_endpoint.hasPending());
}

// The peer keeps its own view of the block, so bytes already in the rings stay readable for it.
void ShmConnection::close(bool) {
	if (_closed) {
		return;
	}
	_closed = true;
	_endpoint.close();
	if (_control) {
		_control->invalidate();
	}
	if (_bulk) {
		_bulk->invalidate();
	}
	if (_memory) {
		_memory->handleLocalClose();
	}
}

TransportWaitAddress ShmConnection::getWaitAddress() {
	if (_closed || !_endpoint.isAttached()) {
		return TransportWaitAddress();
	}
	auto value = _endpoint.prepareWait();
	return TransportWaitAddress{_endpoint.getDoorbell(), value};
}

// `@name` is the in-process rendezvous; the rest belongs to the platform.
static ShmProvider *shmSelectProvider(const Address &addr) {
	if (addr.path.starts_with("@")) {
		return getLocalShmProvider();
	}
	auto provider = getPlatformShmProvider();
	if (!provider) {
		log::source().error("remote::shm", "this platform has no provider for ", addr.description(),
				"; use shm:@<name> within one process");
	}
	return provider;
}

class ShmListener : public TransportListener {
public:
	virtual ~ShmListener();

	virtual Status open(const Address &, const TransportServerConfig &) override;
	virtual bool isOpen() const override { return _backend != nullptr && !_closed; }
	virtual void close() override;

	virtual sprt::dispatch::NativeHandle getPollHandle() const override {
		return sprt::dispatch::NativeHandle(-1);
	}
	virtual uint64_t getEventTimeout() const override { return maxOf<uint64_t>(); }

	virtual TransportWaitAddress getWaitAddress() override {
		return isOpen() ? _backend->getWaitAddress() : TransportWaitAddress();
	}

	virtual void handleEvents(const Callback<void(Rc<TransportConnection> &&)> &) override;

protected:
	Rc<ShmListenerBackend> _backend;
	bool _closed = false;
};

__SPRT_PUSH_ALLOW_CXXABI_ALLOC

ShmListener::~ShmListener() { close(); }

__SPRT_POP_ALLOW_CXXABI_ALLOC

Status ShmListener::open(const Address &addr, const TransportServerConfig &cfg) {
	auto provider = shmSelectProvider(addr);
	if (!provider) {
		return Status::ErrorNotSupported;
	}
	_backend = provider->listen(addr, cfg);
	if (!_backend) {
		return Status::ErrorNotPermitted;
	}
	log::source().info("remote::shm", "listening on ", addr.description());
	return Status::Ok;
}

void ShmListener::close() {
	if (_closed) {
		return;
	}
	_closed = true;
	if (_backend) {
		_backend->close();
	}
}

void ShmListener::handleEvents(const Callback<void(Rc<TransportConnection> &&)> &onAccept) {
	if (!isOpen()) {
		return;
	}
	_backend->accept([&](Rc<ShmBlockMemory> &&memory, PeerIdentity &&peer) {
		auto conn = Rc<ShmConnection>::create(sp::move(memory), ShmSide::Server, sp::move(peer));
		if (!conn) {
			log::source().warn("remote::shm", "refusing a connection block with a bad layout");
			return;
		}
		log::source().info("remote::shm", "accepted a client connection (",
				conn->getPeerIdentity().getDescription(), ")");
		onAccept(Rc<TransportConnection>(conn.get()));
	});
}

class ShmTransport : public Transport {
public:
	virtual ~ShmTransport() = default;

	virtual AddressScheme getScheme() const override { return AddressScheme::Shm; }

	virtual TransportCaps getCaps() const override {
		return TransportCaps::MultiStream | TransportCaps::PeerAuthenticated;
	}

	virtual Rc<TransportConnection> connect(const Address &addr,
			const TransportClientConfig &cfg) override {
		ShmBlockConfig blockConfig{cfg.shmControlCapacity, cfg.shmBulkCapacity};
		if (addr.path.empty() || ShmBlock::computeSize(blockConfig) == 0) {
			log::source().error("remote::shm", "invalid address or ring capacities for ",
					addr.description());
			return nullptr;
		}

		auto provider = shmSelectProvider(addr);
		if (!provider) {
			return nullptr;
		}

		PeerIdentity peer;
		auto memory = provider->connect(addr, blockConfig, peer);
		if (!memory) {
			return nullptr;
		}

		auto conn = Rc<ShmConnection>::create(sp::move(memory), ShmSide::Client, sp::move(peer));
		if (!conn) {
			return nullptr;
		}
		log::source().info("remote::shm", "connected to ", addr.description());
		return Rc<TransportConnection>(conn.get());
	}

	virtual Rc<TransportListener> listen(const Address &addr,
			const TransportServerConfig &cfg) override {
		auto l = Rc<ShmListener>::create();
		if (!l || l->open(addr, cfg) != Status::Ok) {
			return nullptr;
		}
		return Rc<TransportListener>(l.get());
	}
};

} // namespace

void registerShmTransport() { TransportRegistry::registerTransport(Rc<ShmTransport>::create()); }

} // namespace stappler::xenolith::remote

#else

namespace STAPPLER_VERSIONIZED stappler::xenolith::remote {

void registerShmTransport() { }

} // namespace stappler::xenolith::remote

#endif
