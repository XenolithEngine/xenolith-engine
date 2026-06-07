/**
 Copyright (c) 2026 Stappler Team <admin@stappler.org>

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

namespace STAPPLER_VERSIONIZED stappler::xenolith::remote {

// One accepted QUIC connection (server side). Wraps the OpenSSL SSL* (held as void* to keep
// OpenSSL out of this header). Stage 5: establishment only; the per-connection synchronous stream
// I/O (the render protocol) is a later stage.
class SP_PUBLIC ServerConnection : public Ref {
public:
	virtual ~ServerConnection();

	bool init(void *ssl); // takes ownership of the accepted SSL*

	void *getSsl() const { return _ssl; }

protected:
	void *_ssl = nullptr;
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
