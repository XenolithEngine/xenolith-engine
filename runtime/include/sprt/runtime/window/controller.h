/**
 Copyright (c) 2026 Xenolith Team <admin@stappler.org>

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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_WINDOW_CONTROLLER_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_WINDOW_CONTROLLER_H_

#include <sprt/runtime/ref.h>
#include <sprt/runtime/mem/function.h>
#include <sprt/runtime/mem/vector.h>
#include <sprt/runtime/mem/string.h>
#include <sprt/runtime/mem/set.h>
#include <sprt/runtime/platform.h>
#include <sprt/runtime/window/window_info.h>
#include <sprt/runtime/window/input.h>
#include <sprt/runtime/window/text_input.h>
#include <sprt/runtime/window/presentation.h>
#include <sprt/runtime/window/interface.h>
#include <sprt/runtime/window/notifications.h>
#include <sprt/runtime/window/theme_info.h>
#include <sprt/runtime/window/context.h>
#include <sprt/runtime/window/gapi.h>
#include <sprt/runtime/window/clipboard.h>

namespace sprt::window {

class NativeWindow;
class DisplayConfigManager;
class ContextController;

enum class ContextState {
	None,
	Created,
	Started,
	Active,
};

enum class WindowCloseOptions : uint32_t {
	None,
	CloseInPlace = 1 << 1,
	IgnoreExitGuard = 1 << 2,
};

SPRT_DEFINE_ENUM_AS_MASK(WindowCloseOptions)

// For platforms, that has no return to entry point (like, MacOS [NSApp run])
// We need a proper way to release context.
// So, we need some container, from which we can remove context to release it
struct ContextContainer : public Ref {
	Rc<Context> context;
	Rc<ContextController> controller;
};

struct ContextConfig {
	NativeContextHandle *native = nullptr;

	Rc<ContextInfo> context;
	Rc<WindowInfo> window;
	Rc<gapi::InstanceInfo> instance;
	Rc<gapi::LoopInfo> loop;
};

class ContextController : public Ref {
public:
	static Rc<ContextController> create(NotNull<Context>, ContextConfig &&info,
			NotNull<LooperAdapter>);

	static void acquireDefaultConfig(ContextConfig &, NativeContextHandle *);

	virtual ~ContextController() = default;

	virtual bool init(NotNull<Context>, NotNull<LooperAdapter>);

	virtual int run(NotNull<ContextContainer>);

	Context *getContext() const { return _context; }

	DisplayConfigManager *getDisplayConfigManager() const { return _displayConfigManager; }

	bool isWithinPoll() const { return _pollDepth > 0; }

	void retainPollDepth();
	void releasePollDepth();

	virtual bool isCursorSupported(WindowCursor, bool serverSide) const = 0;
	virtual WindowCapabilities getCapabilities() const = 0;

	virtual bool configureWindow(NotNull<WindowInfo>);

	// Native window was created on WM side and now operational
	virtual void notifyWindowCreated(NotNull<NativeWindow>);

	// Native window's size, pixel density or transform was changed by WM
	virtual void notifyWindowConstraintsChanged(NotNull<NativeWindow>, UpdateConstraintsFlags);

	// Some input should be transferred to application
	virtual void notifyWindowInputEvents(NotNull<NativeWindow>,
			memory::dynvector<InputEventData> &&);

	// Internal text input buffer was changed
	virtual void notifyWindowTextInput(NotNull<NativeWindow>, const TextInputState &);

	// Window was closed (or ask to be closed) by WM
	// true if window should be closed, false otherwise (e.g. ExitGuard)
	virtual bool notifyWindowClosed(NotNull<NativeWindow>,
			WindowCloseOptions = WindowCloseOptions::CloseInPlace);

	// Window was allocated by engine, you should not store references on it within this call
	virtual void notifyWindowAllocated(NotNull<NativeWindow>);

	// Window was deallocated by engine, you should not store references on it within this call
	virtual void notifyWindowDeallocated(NotNull<NativeWindow>);

	virtual Status readFromClipboard(Rc<ClipboardRequest> &&);
	virtual Status probeClipboard(Rc<ClipboardProbe> &&);
	virtual Status writeToClipboard(Rc<ClipboardData> &&);

	virtual Rc<ScreenInfo> getScreenInfo() const;

	virtual const ThemeInfo &getThemeInfo() const { return _themeInfo; }
	virtual NetworkFlags getNetworkFlags() const { return _networkFlags; }

	virtual void handleStateChanged(ContextState prevState, ContextState newState);

	virtual void handleSystemNotification(SystemNotification);
	virtual void handleNetworkStateChanged(NetworkFlags);
	virtual void handleThemeInfoChanged(ThemeInfo &&);

	virtual void handleContextWillDestroy();
	virtual void handleContextDidDestroy();

	virtual void handleContextWillStop();
	virtual void handleContextDidStop();

	virtual void handleContextWillPause();
	virtual void handleContextDidPause();

	virtual void handleContextWillResume();
	virtual void handleContextDidResume();

	virtual void handleContextWillStart();
	virtual void handleContextDidStart();

	virtual void handleAllWindowsClosed();

	virtual bool start();
	virtual bool resume();
	virtual bool pause();
	virtual bool stop();
	virtual bool destroy();

	virtual void openUrl(StringView) = 0;

	virtual SurfaceSupportInfo getSupportInfo() const;

protected:
	virtual void notifyPendingWindows();

	int _resultCode = 0;
	ContextState _state = ContextState::Created;
	Context *_context = nullptr;
	Rc<LooperAdapter> _looper;

	Rc<ContextInfo> _contextInfo;
	Rc<WindowInfo> _windowInfo;
	Rc<gapi::InstanceInfo> _instanceInfo;
	Rc<gapi::LoopInfo> _loopInfo;

	Rc<DisplayConfigManager> _displayConfigManager;

	NetworkFlags _networkFlags = NetworkFlags::None;
	ThemeInfo _themeInfo;

	memory::dynset<Rc<NativeWindow>> _activeWindows;
	memory::dynset<NativeWindow *> _allWindows;

	uint32_t _pollDepth = 0;

	memory::dynvector<pair<NativeWindow *, UpdateConstraintsFlags>> _resizedWindows;
	memory::dynvector<pair<NativeWindow *, WindowCloseOptions>> _closedWindows;
};

} // namespace sprt::window

#endif
