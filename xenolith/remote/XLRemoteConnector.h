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

#ifndef XENOLITH_REMOTE_XLREMOTECONNECTOR_H_
#define XENOLITH_REMOTE_XLREMOTECONNECTOR_H_

#include "XLRemoteAddress.h"
#include "XLRemoteProtocol.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::remote {

// One established QUIC connection to a server (client side). Owns the OpenSSL SSL*, its SSL_CTX*,
// and the bound UDP socket (held as void*/int to keep OpenSSL out of this header). The connection
// is blocking this stage; per-connection stream I/O (the render protocol) is a later stage.
class SP_PUBLIC ClientConnection : public Ref {
public:
	static Rc<ClientConnection> connect(const Address &);

	virtual ~ClientConnection();

	bool init(void *ctx, void *ssl, int fd); // takes ownership of all three

	void *getSsl() const { return _ssl; }
	bool isOpen() const { return _ssl != nullptr; }

	int getPollFd() const { return _fd; }

	// Run the X11-like setup handshake: authenticate with `key`, offer `suggestedDict`, and receive
	// the server's reply (window info + negotiated dictionary) into `out`. On success the negotiated
	// dictionary is stored for subsequent sendData/recvData. Returns true iff out.status == Ok.
	ErrorCode handshake(BytesView key, BytesView suggestedDict);

	ErrorCode ping();
	ErrorCode pong(uint32_t serial);

	// Non-blocking: drain the QUIC stream into the reassembler and dispatch complete messages. `cb`
	// returns true to consume a message, false to defer it (kept and retried on a later poll, so
	// replies/events can be handled out of order by serial). Driven by the host AppThread's looper.
	void poll(const Callback<bool(const MessageHeader &, BytesView)> &cb);

	// Gracefully shut the QUIC connection down (sends CONNECTION_CLOSE via SSL_shutdown). Idempotent.
	void close();

protected:
	void *_ctx = nullptr; // SSL_CTX*
	void *_ssl = nullptr; // SSL* (the connection)
	int _fd = -1; // bound UDP socket (SSL_set_fd uses BIO_NOCLOSE)
	bool _shutdown = false;
	Bytes _dict; // negotiated LZ4 dictionary (empty == none)
	uint32_t _clientSerial = 1; // for handshake = 0
	MessageReader _reader; // receive-side stream reassembler + deferred-message queue
};

} // namespace stappler::xenolith::remote

#endif /* XENOLITH_REMOTE_XLREMOTECONNECTOR_H_ */
