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

/* The `mem:` transport -- a connected pair of byte queues inside one process.
 *
 * It carries no data anywhere, which is the point: it makes the ENTIRE protocol above the transport
 * seam -- the setup handshake, framing, the reassembler, block transfer, the font exchange -- run in
 * a test with no socket, no TLS, no ports and no second process. Those paths could not be asserted
 * before, because both of their ends only ever spoke through an `SSL *`.
 *
 * It is also the only transport that exists on every target: wasm has no sockets at all (every
 * entry point in the runtime's socket layer answers ENOSYS), and an RTOS build may have no TLS.
 *
 * Single-threaded by design. Both endpoints live on the caller's thread and are driven by explicit
 * pumping, so a test is deterministic: nothing arrives except where the test moved it.
 */

#include "XLRemoteTransport.h"

#include <unistd.h> // getuid/getpid for the in-process PeerIdentity

namespace STAPPLER_VERSIONIZED stappler::xenolith::remote {

namespace {

// The shared middle of a connected pair. Two of these exist per connection, one per direction, and
// each endpoint reads from one and writes to the other.
struct MemPipe : public Ref {
	Bytes buffer;
	size_t offset = 0;
	bool closed = false;

	size_t pending() const { return buffer.size() - offset; }

	void push(BytesView data) {
		buffer.insert(buffer.end(), data.data(), data.data() + data.size());
	}

	size_t pull(uint8_t *out, size_t len) {
		auto n = sprt::min(len, pending());
		if (n) {
			__sprt_memcpy(out, buffer.data() + offset, n);
			offset += n;
		}
		// Reclaim the consumed prefix once it is worth moving, so a long exchange does not grow a
		// buffer made entirely of bytes already read.
		if (offset >= buffer.size()) {
			buffer.clear();
			offset = 0;
		} else if (offset >= 64u * 1'024) {
			buffer.erase(buffer.begin(), buffer.begin() + offset);
			offset = 0;
		}
		return n;
	}
};

class MemStream : public TransportStream {
public:
	virtual ~MemStream() = default;

	bool init(Rc<MemPipe> &&in, Rc<MemPipe> &&out) {
		_in = sp::move(in);
		_out = sp::move(out);
		return _in && _out;
	}

	virtual Status write(BytesView data, size_t &written) override {
		if (_out->closed) {
			written = 0;
			return Status::ErrorNotPermitted;
		}
		// No flow control: the peer is in the same process and this is a test aid, so a write always
		// takes everything. A transport that CAN refuse is exercised by the socket ones.
		_out->push(data);
		written = data.size();
		if (_onWritten) {
			_onWritten();
		}
		return Status::Ok;
	}

	virtual Status read(uint8_t *buf, size_t len, size_t &got) override {
		got = _in->pull(buf, len);
		if (got == 0 && _in->closed) {
			return Status::ErrorNotPermitted;
		}
		return Status::Ok;
	}

	virtual bool isClosed() const override { return _in->closed && _in->pending() == 0; }

	// Lets the connection tell its peer that bytes landed, which is what stands in for socket
	// readiness here.
	void setOnWritten(Function<void()> &&cb) { _onWritten = sp::move(cb); }

	MemPipe *getIn() const { return _in; }
	MemPipe *getOut() const { return _out; }

protected:
	Rc<MemPipe> _in;
	Rc<MemPipe> _out;
	Function<void()> _onWritten;
};

class MemConnection : public TransportConnection {
public:
	virtual ~MemConnection() = default;

	bool init(Rc<MemPipe> &&in, Rc<MemPipe> &&out, StringView name) {
		_stream = Rc<MemStream>::create(sp::move(in), sp::move(out));
		if (!_stream) {
			return false;
		}
		// Both ends of a mem: pair are this process, so the peer is as authenticated as it gets --
		// which is exactly what lets a test drive the "transport already knows who this is" policy.
		_peer.authenticated = true;
		_peer.uid = int64_t(::getuid());
		_peer.pid = int64_t(::getpid());
		_peer.description = toString("mem:", name);
		return true;
	}

	virtual TransportCaps getCaps() const override {
		// MessageFramed is NOT declared: the pipe is a byte stream like every other transport here,
		// so the reassembler above stays on the same code path a socket exercises. Claiming framing
		// would make the tests stop covering the case that actually ships.
		return TransportCaps::Encrypted | TransportCaps::PeerAuthenticated;
	}

	virtual TransportStream *getStream(StreamClass) override { return _stream; }

	virtual const PeerIdentity &getPeerIdentity() const override { return _peer; }

	virtual Status handleEvents() override {
		// Nothing to service: a write already delivered.
		if (_onReadable && _stream->getIn()->pending() > 0) {
			_onReadable();
		}
		return Status::Ok;
	}

	virtual void setOnReadable(Function<void()> &&cb) override { _onReadable = sp::move(cb); }

	virtual bool isClosed() override { return _closed || _stream->getIn()->closed; }

	virtual void close(bool) override {
		if (_closed) {
			return;
		}
		_closed = true;
		// Close the direction WE write: the peer must still drain what we already sent, which is why
		// MemStream::isClosed also waits for the buffer to empty.
		_stream->getOut()->closed = true;
	}

protected:
	Rc<MemStream> _stream;
	PeerIdentity _peer;
	Function<void()> _onReadable;
	bool _closed = false;
};

// Endpoints waiting for a peer, keyed by the address's name. A listener publishes itself here and a
// connect() finds it -- the in-process stand-in for a bound port.
class MemListener : public TransportListener {
public:
	virtual ~MemListener();

	virtual Status open(const Address &addr, const TransportServerConfig &) override;
	virtual bool isOpen() const override { return _open; }
	virtual void close() override;

	virtual sprt::dispatch::NativeHandle getPollHandle() const override {
		return sprt::dispatch::NativeHandle(-1);
	}
	virtual uint64_t getEventTimeout() const override { return maxOf<uint64_t>(); }

	virtual void handleEvents(const Callback<void(Rc<TransportConnection> &&)> &cb) override {
		auto pending = sp::move(_pending);
		_pending.clear();
		for (auto &it : pending) { cb(Rc<TransportConnection>(it.get())); }
	}

	// Called by MemTransport::connect: builds the pair and parks the server half here.
	Rc<TransportConnection> accept(StringView name);

	StringView getName() const { return _name; }

protected:
	String _name;
	bool _open = false;
	Vector<Rc<MemConnection>> _pending;
};

// Every open mem: listener in this process. A raw back-pointer, cleared by close()/destructor: the
// table must not keep a listener alive past its owner.
static Map<String, MemListener *> &memListeners() {
	static Map<String, MemListener *> s_listeners;
	return s_listeners;
}

MemListener::~MemListener() { close(); }

Status MemListener::open(const Address &addr, const TransportServerConfig &) {
	if (addr.path.empty()) {
		log::source().error("remote::mem", "an endpoint name is required: mem:<name>");
		return Status::ErrorInvalidArguemnt;
	}
	auto &table = memListeners();
	if (table.find(addr.path) != table.end()) {
		log::source().error("remote::mem", "endpoint '", addr.path, "' is already bound");
		return Status::ErrorFileExists;
	}
	_name = addr.path;
	table.emplace(_name, this);
	_open = true;
	log::source().info("remote::mem", "listening on ", addr.description());
	return Status::Ok;
}

void MemListener::close() {
	if (!_open) {
		return;
	}
	_open = false;
	memListeners().erase(_name);
	_pending.clear();
}

Rc<TransportConnection> MemListener::accept(StringView name) {
	// One pipe per direction; the two endpoints get them crossed over.
	auto toServer = Rc<MemPipe>::alloc();
	auto toClient = Rc<MemPipe>::alloc();
	if (!toServer || !toClient) {
		return nullptr;
	}

	auto server = Rc<MemConnection>::create(Rc<MemPipe>(toServer), Rc<MemPipe>(toClient), name);
	auto client = Rc<MemConnection>::create(Rc<MemPipe>(toClient), Rc<MemPipe>(toServer), name);
	if (!server || !client) {
		return nullptr;
	}

	_pending.emplace_back(sp::move(server));
	return Rc<TransportConnection>(client.get());
}

class MemTransport : public Transport {
public:
	virtual ~MemTransport() = default;

	virtual AddressScheme getScheme() const override { return AddressScheme::Mem; }

	virtual TransportCaps getCaps() const override {
		return TransportCaps::Encrypted | TransportCaps::PeerAuthenticated;
	}

	virtual Rc<TransportConnection> connect(const Address &addr,
			const TransportClientConfig &) override {
		auto &table = memListeners();
		auto it = table.find(addr.path);
		if (it == table.end()) {
			log::source().error("remote::mem", "no endpoint bound at ", addr.description());
			return nullptr;
		}
		return it->second->accept(addr.path);
	}

	virtual Rc<TransportListener> listen(const Address &addr,
			const TransportServerConfig &cfg) override {
		auto l = Rc<MemListener>::create();
		if (!l || l->open(addr, cfg) != Status::Ok) {
			return nullptr;
		}
		return Rc<TransportListener>(l.get());
	}
};

} // namespace

void registerMemTransport() { TransportRegistry::registerTransport(Rc<MemTransport>::create()); }

} // namespace stappler::xenolith::remote
