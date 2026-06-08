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

#ifndef XENOLITH_REMOTE_XLREMOTELISTENER_H_
#define XENOLITH_REMOTE_XLREMOTELISTENER_H_

#include "XLRemoteAddress.h"
#include "XLRemoteProtocol.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::remote {

// One accepted QUIC connection (server side). Wraps the OpenSSL SSL* (held as void* to keep
// OpenSSL out of this header). Stage 5: establishment only; the per-connection synchronous stream
// I/O (the render protocol) is a later stage.
class SP_PUBLIC ServerConnection : public Ref {
public:
	virtual ~ServerConnection();

	bool init(void *ssl); // takes ownership of the accepted SSL*

	void *getSsl() const { return _ssl; }

	// Pump this connection's QUIC events and report whether it has begun terminating (peer sent
	// CONNECTION_CLOSE, local close, or idle timeout). Used by the server to free the slot for a
	// new client after the current one disconnects.
	bool isClosed();

	// Run the server side of the X11-like setup handshake: validate the client's bearer key against
	// `expectedKey`, negotiate the dictionary (server `serverDict` has priority, else the client's
	// suggestion, else none), and reply with the window info. Stores the negotiated dictionary for
	// subsequent sendData/recvData. `outStatus` reports the outcome; returns true iff authenticated.
	ErrorCode handshake(BytesView expectedKey, BytesView serverDict);

	ErrorCode ping();
	ErrorCode pong(uint32_t serial);

	ErrorCode sendCborMessage(Domain, uint8_t message, const Value &);
	ErrorCode sendMessage(Domain, uint8_t message, BytesView);

	// Non-blocking: drain the QUIC stream into the reassembler and dispatch complete messages. `cb`
	// returns true to consume a message, false to defer it (kept and retried on a later poll, so
	// replies/events can be handled out of order by serial). Driven by the host AppThread's pump.
	void poll(const Callback<bool(const MessageHeader &, BytesView)> &cb);

	// Gracefully shut the QUIC connection down (drives SSL_shutdown to completion so CONNECTION_CLOSE
	// is actually transmitted; bounded so an unresponsive peer can't hang us). Idempotent.
	void close();

protected:
	void *_ssl = nullptr;
	Bytes _dict; // negotiated LZ4 dictionary (empty == none)
	uint32_t _serial = 1; // handshake is always serial = 0
	bool _shutdown = false;
	MessageReader _reader; // receive-side stream reassembler + deferred-message queue
};

// Non-blocking QUIC listener. It owns no thread: the host (AppThread) registers getPollFd() with
// its Looper (PollFlags::In) and drives handleEvents()/getEventTimeout() from the looper.
class SP_PUBLIC Listener : public Ref {
public:
	using AcceptCallback = Function<void(Rc<ServerConnection> &&)>;

	virtual ~Listener();

	// Bind a non-blocking QUIC listener to the address. Network only this stage; unix-domain
	// returns false (needs a custom datagram BIO -- a follow-up).
	bool open(const Address &);
	void close();
	bool isOpen() const { return _ssl != nullptr; }

	// The socket fd to register with Looper::listenPollableHandle (PollFlags::In). -1 if closed.
	int getPollFd() const { return _fd; }

	// Pump QUIC events and accept any pending connections (onAccept per new connection).
	void handleEvents(const AcceptCallback &onAccept);

	// Microseconds until the next QUIC timer event; maxOf<uint64_t>() == infinite (no timer).
	uint64_t getEventTimeout() const;

protected:
	void *_ctx = nullptr; // SSL_CTX*
	void *_ssl = nullptr; // SSL* (the listener)
	void *_pkey = nullptr; // EVP_PKEY* (self-signed key)
	void *_cert = nullptr; // X509* (self-signed cert)
	int _fd = -1; // bound UDP socket
};

} // namespace stappler::xenolith::remote

#endif /* XENOLITH_REMOTE_XLREMOTELISTENER_H_ */
