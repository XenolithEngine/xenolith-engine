/**
 Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#ifndef XENOLITH_APPLICATION_XLCLIENTCONTEXT_H_
#define XENOLITH_APPLICATION_XLCLIENTCONTEXT_H_

#include "XLContextInfo.h"
#include "XLClientAppThread.h"
#include "XLRemoteAddress.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

// Small, standalone host for the client process. Unlike the server Context, it does NOT derive from
// sprt::window::Context and pulls in none of the windowing / ContextController stack: a client owns
// no native window. It holds the config, runs a ClientAppThread as its main thread, and receives
// that thread's lifecycle callbacks.
//
// Scaffolding for now: the remote transport, the client-side FontController/context extensions, and
// the entry-point bootstrap are later stages.
class SP_PUBLIC ClientContext : public Ref {
public:
	virtual ~ClientContext();

	virtual bool init();

	const ContextInfo *getInfo() const { return _info; }

	// The server endpoint to dial; assigned by the client's main() before run().
	void setServerAddress(StringView addr) { _serverAddress = remote::Address::parse(addr); }
	const remote::Address &getServerAddress() const { return _serverAddress; }

	// The 64-byte bearer key presented at the handshake (assigned by main()).
	void setBearerKey(BytesView key) { _bearerKey = key.bytes<Interface>(); }
	BytesView getBearerKey() const { return _bearerKey; }

	// Optional LZ4 dictionary suggested to the server (used only if the server has none).
	void setSuggestedDictionary(BytesView d) { _suggestedDict = d.bytes<Interface>(); }
	BytesView getSuggestedDictionary() const { return _suggestedDict; }

	// SHA-256 of the server's DER SubjectPublicKeyInfo, learned out-of-band alongside the address and
	// the key (see remote::Listener::getCertificateFingerprint). Leaving it empty connects without
	// authenticating the server, which also exposes the bearer key to a man in the middle.
	void setServerFingerprint(BytesView fp) { _serverFingerprint = fp.bytes<Interface>(); }
	BytesView getServerFingerprint() const { return _serverFingerprint; }

	// Create the client's main thread and start it.
	virtual void run();
	virtual void stop();

	ClientAppThread *getAppThread() const { return _appThread; }

	// Lifecycle callbacks issued by the ClientAppThread (mirrors Context::handleAppThread*).
	virtual void handleAppThreadCreated(NotNull<ClientAppThread>);
	virtual void handleAppThreadDestroyed(NotNull<ClientAppThread>);
	virtual void handleAppThreadUpdate(NotNull<ClientAppThread>, const UpdateTime &);

	virtual bool handleWindowConnected(NotNull<ClientAppThread>, NotNull<RemoteWindow>);
	virtual void handleWindowDisconnected(NotNull<ClientAppThread>, NotNull<RemoteWindow>);

	void setWindowConnectedCallback(Function<bool(NotNull<RemoteWindow>)> &&);
	void setWindowDisconnectedCallback(Function<void(NotNull<RemoteWindow>)> &&);

protected:
	Rc<ContextInfo> _info;
	Rc<ClientAppThread> _appThread;
	remote::Address _serverAddress;
	Bytes _bearerKey;
	Bytes _suggestedDict;
	Bytes _serverFingerprint;

	Function<bool(NotNull<RemoteWindow>)> _onWindowConnected;
	Function<void(NotNull<RemoteWindow>)> _onWindowDisconnected;
};

} // namespace stappler::xenolith

#endif /* XENOLITH_APPLICATION_XLCLIENTCONTEXT_H_ */
