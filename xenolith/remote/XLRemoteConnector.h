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

#include "XLRemoteConnection.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::remote {

// One established connection to a server, client side.
class SP_PUBLIC ClientConnection : public Connection {
public:
	// Dial the address, resolving its scheme through the TransportRegistry.
	//
	// `expectedFingerprint` is the SHA-256 of the server's DER SubjectPublicKeyInfo, obtained
	// out-of-band (Listener::getCertificateFingerprint). On a TLS-based transport a non-empty value
	// is what authenticates the server; WITHOUT it the certificate is ephemeral and self-signed, so
	// any man in the middle both intercepts the session and receives the bearer key the handshake
	// then presents. Transports that authenticate by other means (a unix socket) ignore it.
	static Rc<ClientConnection> connect(const Address &,
			BytesView expectedFingerprint = BytesView());

	virtual ~ClientConnection();

	bool init(Rc<TransportConnection> &&conn) {
		return Connection::init(sp::move(conn), Role::Client);
	}

	bool isOpen() const { return getTransport() != nullptr; }

	// Run the setup handshake: authenticate with `key`, offer `suggestedDict`, and take the server's
	// reply. On success the negotiated dictionary is stored for subsequent messages.
	GlobalError handshake(BytesView key, BytesView suggestedDict);
};

} // namespace stappler::xenolith::remote

#endif /* XENOLITH_REMOTE_XLREMOTECONNECTOR_H_ */
