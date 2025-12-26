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

#ifndef XENOLITH_APPLICATION_PLATFORM_XLCONTEXTCONTROLLER_H_
#define XENOLITH_APPLICATION_PLATFORM_XLCONTEXTCONTROLLER_H_

#include "XLContext.h"
#include "XLCoreTextInput.h"
#include "XLDisplayConfigManager.h"
#include "XLContextNativeWindow.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class ContextComponent;

}

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

class NativeWindow;

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

SP_DEFINE_ENUM_AS_MASK(WindowCloseOptions)

struct ClipboardRequest : public Ref {
	// receives data from clipboard
	Function<void(Status, BytesView, StringView)> dataCallback;

	// Select one of suggested types to receive
	// You should not assume on what thread this function would be called,
	// so, assume the worst case
	Function<StringView(SpanView<StringView>)> typeCallback;

	// Target to keep
	Rc<Ref> target;
};

struct ClipboardData : public Ref {
	Time initial = Time::now();

	// User-friendly description
	String label;

	// Supported types for the data
	Vector<String> types;

	// Function to convert clipboard's data into target format
	// You should not assume on what thread this function would be called,
	// so, assume the worst case
	Function<Bytes(StringView)> encodeCallback;

	// Data owner
	Rc<Ref> owner;
};

struct ClipboardProbe : public Ref {
	// Function to receive available types
	Function<void(Status, SpanView<StringView>)> typeCallback;

	// Target to keep
	Rc<Ref> target;
};

// For platforms, that has no return to entry point (like, MacOS [NSApp run])
// We need a proper way to release context.
// So, we need some container, from which we can remove context to release it
struct ContextContainer : public Ref {
	Rc<Context> context;
	Rc<ContextController> controller;
};

class ContextController : public Ref {
public:
	static Rc<ContextController> create(NotNull<Context>, ContextConfig &&info);

	static void acquireDefaultConfig(ContextConfig &, NativeContextHandle *);

	virtual ~ContextController() = default;

	virtual bool init(NotNull<Context>);

	virtual int run(NotNull<ContextContainer>);

	event::Looper *getLooper() const { return _looper; }
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
	virtual void notifyWindowConstraintsChanged(NotNull<NativeWindow>,
			core::UpdateConstraintsFlags);

	// Some input should be transferred to application
	virtual void notifyWindowInputEvents(NotNull<NativeWindow>, Vector<core::InputEventData> &&);

	// Internal text input buffer was changed
	virtual void notifyWindowTextInput(NotNull<NativeWindow>, const core::TextInputState &);

	// Window was closed (or ask to be closed) by WM
	// true if window should be closed, false otherwise (e.g. ExitGuard)
	virtual bool notifyWindowClosed(NotNull<NativeWindow>,
			WindowCloseOptions = WindowCloseOptions::CloseInPlace);

	// Window was allocated by engine, you should not store references on it within this call
	virtual void notifyWindowAllocated(NotNull<NativeWindow>);

	// Window was deallocated by engine, you should not store references on it within this call
	virtual void notifyWindowDeallocated(NotNull<NativeWindow>);

	virtual Rc<AppWindow> makeAppWindow(NotNull<AppThread>, NotNull<NativeWindow>);

	virtual void initializeComponent(NotNull<ContextComponent>);

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

	virtual Value saveContextState();

	virtual void openUrl(StringView) = 0;

protected:
	virtual Rc<core::Loop> makeLoop(NotNull<core::Instance>);

	virtual void poll();

	virtual void notifyPendingWindows();

	int _resultCode = 0;
	ContextState _state = ContextState::Created;
	Context *_context = nullptr;
	event::Looper *_looper = nullptr;

	Rc<ContextInfo> _contextInfo;
	Rc<WindowInfo> _windowInfo;
	Rc<core::InstanceInfo> _instanceInfo;
	Rc<core::LoopInfo> _loopInfo;

	Rc<DisplayConfigManager> _displayConfigManager;

	NetworkFlags _networkFlags = NetworkFlags::None;
	ThemeInfo _themeInfo;

	Set<Rc<NativeWindow>> _activeWindows;
	Set<NativeWindow *> _allWindows;

	uint32_t _pollDepth = 0;

	Vector<Pair<NativeWindow *, core::UpdateConstraintsFlags>> _resizedWindows;
	Vector<Pair<NativeWindow *, WindowCloseOptions>> _closedWindows;
};

} // namespace stappler::xenolith::platform

#endif /* XENOLITH_APPLICATION_PLATFORM_XLCONTEXTCONTROLLER_H_ */
