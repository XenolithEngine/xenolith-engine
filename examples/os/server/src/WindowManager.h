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


#ifndef EXAMPLES_OS_SERVER_SRC_WINDOWMANAGER_H_
#define EXAMPLES_OS_SERVER_SRC_WINDOWMANAGER_H_

#include "AppCatalog.h"
#include "SystemStats.h"
#include "XLCompositorSwapchainPipe.h"
#include "XLWindowSceneInfo.h"

#include <sprt/runtime/dispatch/handle.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class RemoteSession;
class AppReply;
class Node;

} // namespace stappler::xenolith

namespace STAPPLER_VERSIONIZED stappler::xenolith::wm {

/* The window manager: a compositor pipe over the host window, a remote session for the clients,
and the programs it launches into it. App thread only.

Every program gets one virtual window and its window one plane. Two layouts:

* Stack (XL_WM_APPS given): the programs are launched from that list, each window is the size of
  the screen, the last launched one is on top and the only one enabled. The bench the compositor
  and input checks run on.
* Shell: the shell (launcher) and the shade (status bar and pull-down) are launched and kept
  running; applications come from the catalog. One surface is active - the shell or one
  application, below the status bar - the others are paused; the shade covers the screen above
  them, transparent, taking input only where it says. */
class WindowManager : public Ref {
public:
	enum class Mode {
		Stack,
		Shell,
	};

	enum class Role {
		Application,
		Shell,
		Shade,
	};

	struct App : public Ref {
		// Null once the server stopped: the window's callbacks outlive it.
		WindowManager *server = nullptr;
		Role role = Role::Application;
		uint32_t index = 0;
		String label; // the key label its session presents
		String catalogId;
		String command;
		String inspector; // the inspector address it was launched with
		String log; // where its output goes
		Rc<sprt::dispatch::ProcessHandle> process;
		bool running = false;
		bool closing = false; // cancelled by the server
		int exitCode = 0;
		uint64_t launchTime = 0;
		Rc<RemoteSession> session;
		Rc<WindowSceneInfo> window;
		Rc<compositor::DisplayPlane> plane;

		// Frames the server's own scene presented in the window: a later one is the client's.
		uint64_t serverFrames = 0;
		bool shown = false; // the client has drawn; until then the plane is invisible
		Rc<sprt::dispatch::Handle> showTimer;
	};

	virtual ~WindowManager();

	bool init(NotNull<ServerAppThread>, NotNull<AppWindow> host);

	// Take the host window over, open the session and launch the programs.
	Status start();

	// Kill the programs; the host goes back to its scene.
	void stop();

	Mode getMode() const { return _mode; }

	compositor::SwapchainDisplayPipe *getPipe() const { return _pipe; }

	// Shell mode
	Status launchApp(StringView catalogId);
	Status switchTo(StringView label);
	Status closeApp(StringView label);
	void goHome();
	void setShadeOpen(bool);

	// wm-state, wm-plane, wm-pause, wm-resume, wm-focus, wm-routes; wm-launch, wm-home, wm-switch,
	// wm-close
	void registerCommands(Node *);

	Value encodeState() const;
	Value encodePlane(const compositor::DisplayPlane *) const;

protected:
	Status handleClientWindow(NotNull<RemoteSession>, NotNull<sprt::window::WindowInfo>,
			Rc<WindowSceneInfo> &out);
	void handleWindowCreated(App *, AppWindow *);
	void handleWindowClosed(App *);
	void handleProcessExit(App *, int exitCode, Status);
	void handlePlanePublished(compositor::DisplayPlane *, uint64_t serial);
	void handleAppMessage(NotNull<RemoteSession>, Value &&, Rc<AppReply> &&);

	Rc<App> launch(Role, StringView label, StringView command, LaunchStyle, Extent2 window);
	void launchSystem(Role);

	// The plane of a window the client has drawn into: in its place, visible.
	void show(App *);

	// The stack layout, committed.
	void relayout();

	// The shell layout, committed, and the focus that goes with it.
	void applyLayout();

	void sendApps();
	void sendStatus();
	void scheduleStatus();

	Vector<protocol::RunningApp> getRunningApps() const;

	App *getApp(StringView label) const;
	App *getApp(const compositor::DisplayPlane *) const;
	App *getSystem(Role) const;

	uint32_t getBarHeight() const;
	Extent2 getWindowExtent(Role) const;

	// The plane a command names: { plane: <id> } or { window: <id> }.
	compositor::DisplayPlane *resolvePlane(const Value &) const;

	// Commit, or undo every pending change and say why not.
	Value commitOrRevert();

	Rc<ServerAppThread> _app;
	Rc<AppWindow> _host;
	Rc<compositor::SwapchainDisplayPipe> _pipe;
	Rc<AppCatalog> _catalog;
	SystemStats _stats;
	Mode _mode = Mode::Stack;
	String _address;
	Vector<Rc<App>> _apps;
	Map<String, uint32_t> _launchCounts;
	Rc<App> _active; // shell mode: the surface the user sees
	Rc<App> _pending; // shell mode: becomes active once shown
	bool _shadeOpen = false;
	Vector<IRect> _shadeRegion;

	// Shell and shade restarts: quick exits in a row, and whether the server gave up
	struct Restarts {
		uint32_t quickExits = 0;
		bool failed = false;
	};
	Restarts _shellRestarts;
	Restarts _shadeRestarts;

	Rc<sprt::dispatch::Handle> _statusTimer;
	bool _started = false;
};

} // namespace stappler::xenolith::wm

#endif /* EXAMPLES_OS_SERVER_SRC_WINDOWMANAGER_H_ */
