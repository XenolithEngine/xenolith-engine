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

#include "XLRemoteConnection.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::remote {

// One accepted connection, server side.
class SP_PUBLIC ServerConnection : public Connection {
public:
	virtual ~ServerConnection();

	bool init(Rc<TransportConnection> &&conn) { return Connection::init(sp::move(conn), Role::Server); }

	// Run the server side of the setup handshake: validate the client's bearer key against
	// `expectedKey`, negotiate the dictionary (server `serverDict` has priority, else the client's
	// suggestion, else none), and reply. Stores the negotiated dictionary for subsequent messages.
	GlobalError handshake(BytesView expectedKey, BytesView serverDict);

	// Turn this connection away without negotiating: answer its setup with `status` (typically
	// GlobalError::Busy) so the peer learns why instead of waiting out its own handshake deadline.
	// The caller closes the connection afterwards.
	GlobalError reject(GlobalError status);
};

// A bound endpoint, owned by the host AppThread. It owns no thread: the host registers
// getPollHandle() with its Looper (PollFlags::In) and drives handleEvents()/getEventTimeout().
//
// The transport underneath is chosen by the address's scheme, so this class knows nothing about
// QUIC -- only how to turn accepted transport connections into protocol sessions.
class SP_PUBLIC Listener : public Ref {
public:
	using AcceptCallback = Function<void(Rc<ServerConnection> &&)>;

	virtual ~Listener();

	// Resolve the address's scheme through the TransportRegistry and bind. False when the scheme is
	// not registered in this build (the reason is logged) or the endpoint cannot be bound.
	bool open(const Address &);
	void close();
	bool isOpen() const;

	// The handle to register with Looper::listenPollableHandle (PollFlags::In).
	sprt::dispatch::NativeHandle getPollHandle() const;

	// Pump the transport and accept any pending connections (onAccept per new connection).
	void handleEvents(const AcceptCallback &onAccept);

	// Microseconds until the next transport timer event; maxOf<uint64_t>() == infinite.
	uint64_t getEventTimeout() const;

	// This listener's identity for handing to a client out-of-band -- the SHA-256 of the DER
	// SubjectPublicKeyInfo on a TLS-based transport. Empty for a transport with no such notion (a
	// unix socket authenticates by credentials instead) or while closed.
	BytesView getCertificateFingerprint() const;

	// Format a fingerprint for that out-of-band channel (lowercase base16).
	static String encodeFingerprint(BytesView);

protected:
	Rc<TransportListener> _listener;
};

} // namespace stappler::xenolith::remote

#endif /* XENOLITH_REMOTE_XLREMOTELISTENER_H_ */
