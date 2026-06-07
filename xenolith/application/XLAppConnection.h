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

#ifndef XENOLITH_APPLICATION_XLAPPCONNECTION_H_
#define XENOLITH_APPLICATION_XLAPPCONNECTION_H_

#include "XLCoreRenderSession.h" // core::RenderClientChannel

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class AppThread;
class AppWindow;

//
// Connection: a single client->server render-session link, owned by the server-side AppThread (the
// logical connection owner). It is client-driven: the server runs its normal scene by default (that
// running Director is the fallback shown when no client is connected); when a client connects, the
// Connection attaches it -- by negotiation -- to an EXISTING AppWindow, swapping that window's bound
// RenderClientChannel from the local fallback Director to the remote client. On detach it reverts to
// the local Director. The protocol is synchronous early, so the remote client is just a
// RenderClientChannel the AppWindow drives exactly like the local Director.
//
// Stage 4 (prep): the abstraction + attach/detach. The remote RenderClientChannel and the
// negotiation/transport that call this are later stages (this is dormant for now).
//
class SP_PUBLIC Connection : public Ref {
public:
	virtual ~Connection();

	bool init(NotNull<AppThread>);

	// Attach a connected client's channel to an existing window (bind it as the window's render
	// client; the window's local Director stays alive as the fallback). Returns false if `remote`
	// is null.
	bool attach(NotNull<AppWindow>, core::RenderClientChannel *remote);

	// Revert the window to its local fallback Director.
	void detach();

	bool isConnected() const { return _window != nullptr && _client != nullptr; }
	AppWindow *getWindow() const { return _window; }

protected:
	Rc<AppThread> _app;
	Rc<AppWindow> _window;
	core::RenderClientChannel *_client = nullptr;
};

} // namespace stappler::xenolith

#endif /* XENOLITH_APPLICATION_XLAPPCONNECTION_H_ */
