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

#ifndef EXAMPLES_OS_SERVER_SRC_WMSERVER_H_
#define EXAMPLES_OS_SERVER_SRC_WMSERVER_H_

#include "XLCommon.h"
#include "XLCompositorSwapchainPipe.h"
#include "XLWindowSceneInfo.h"

#include <sprt/runtime/dispatch/handle.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class RemoteSession;
class Node;

} // namespace stappler::xenolith

namespace STAPPLER_VERSIONIZED stappler::xenolith::wm {

/* The window manager: a compositor pipe over the host window, a remote session for the clients,
and the applications it launches into it. App thread only.

Every application gets one window, virtual and the size of the screen, and its window one plane.
The layout is a stack: the last launched application is on top and the only one enabled, every
other one is paused. */
class WmServer : public Ref {
public:
	struct App : public Ref {
		// Null once the server stopped: the window's callbacks outlive it.
		WmServer *server = nullptr;
		uint32_t index = 0;
		String label; // the key label its session presents: app<index>
		String command;
		String inspector; // the inspector address it was launched with
		String log; // where its output goes
		Rc<sprt::dispatch::ProcessHandle> process;
		bool running = false;
		int exitCode = 0;
		Rc<WindowSceneInfo> window;
		Rc<compositor::DisplayPlane> plane;
	};

	virtual ~WmServer();

	bool init(NotNull<ServerAppThread>, NotNull<AppWindow> host);

	// Take the host window over, open the session and launch XL_WM_APPS.
	Status start();

	// Kill the applications; the host goes back to its scene.
	void stop();

	// The stack layout, committed.
	void relayout();

	compositor::SwapchainDisplayPipe *getPipe() const { return _pipe; }

	// The inspector commands: wm-state, wm-plane, wm-pause, wm-resume.
	void registerCommands(Node *);

	Value encodeState() const;
	Value encodePlane(const compositor::DisplayPlane *) const;

protected:
	Status handleClientWindow(NotNull<RemoteSession>, NotNull<sprt::window::WindowInfo>,
			Rc<WindowSceneInfo> &out);
	void handleWindowClosed(App *);
	void handleProcessExit(App *, int exitCode);

	void launch(StringView command);

	App *getApp(StringView label);
	const App *getApp(const compositor::DisplayPlane *) const;

	// The plane a command names: { plane: <id> } or { window: <id> }.
	compositor::DisplayPlane *resolvePlane(const Value &) const;

	// Commit, or undo every pending change and say why not.
	Value commitOrRevert();

	Rc<ServerAppThread> _app;
	Rc<AppWindow> _host;
	Rc<compositor::SwapchainDisplayPipe> _pipe;
	String _address;
	Vector<Rc<App>> _apps;
	bool _started = false;
};

} // namespace stappler::xenolith::wm

#endif /* EXAMPLES_OS_SERVER_SRC_WMSERVER_H_ */
