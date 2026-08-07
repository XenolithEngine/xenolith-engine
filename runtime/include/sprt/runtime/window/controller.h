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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_WINDOW_CONTROLLER_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_WINDOW_CONTROLLER_H_

#include <sprt/runtime/ref.h>
#include <sprt/runtime/platform.h>
#include <sprt/runtime/dispatch/looper.h>
#include <sprt/runtime/window/types.h>
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
#include <sprt/runtime/window/dialog.h>
#include <sprt/cxx/function>

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

class SPRT_API ContextController : public Ref {
public:
	static Rc<ContextController> create(NotNull<Context>, ContextConfig &&info,
			NotNull<dispatch::Looper>);

	static void acquireDefaultConfig(ContextConfig &, NativeContextHandle *);

	virtual ~ContextController() = default;

	virtual bool init(NotNull<Context>, NotNull<dispatch::Looper>);

	virtual int run(NotNull<ContextContainer>);

	Context *getContext() const { return _context; }
	dispatch::Looper *getLooper() const { return _looper; }

	DisplayConfigManager *getDisplayConfigManager() const { return _displayConfigManager; }

	bool isWithinPoll() const { return _pollDepth > 0; }

	void retainPollDepth();
	void releasePollDepth();

	virtual bool isCursorSupported(WindowCursor, bool serverSide) const = 0;
	virtual WindowCapabilities getCapabilities() const = 0;

	virtual bool configureWindow(NotNull<WindowInfo>);

	// Find a live window by its WindowInfo::id
	NativeWindow *findWindow(StringView id) const;

	// Last pointer/button serial seen by the backend. xdg_popup.grab needs the serial of a fresh
	// input event; other platforms ignore it.
	void notePointerSerial(uint32_t serial) { _lastPointerSerial = serial; }
	uint32_t getLastPointerSerial() const { return _lastPointerSerial; }

	// Common entry point for window creation, safe to call at any point when context is active.
	// Validates type/parent requirements and id uniqueness, then hands off to the backend
	// implementation (`loadWindow`).
	// Returns Status::ErrorNotSupported if the platform can not create this kind of window;
	// caller may then fall back to an in-scene emulation.
	virtual Status createWindow(Rc<WindowInfo> &&);

	// Dismiss the whole menu chain `w` belongs to: walks up to the outermost Popup/Tooltip and
	// closes it, which cascades back down through every nested level. This is the "clicked
	// outside" / "Esc" / "lost focus" path — a single level must never be dismissed alone,
	// or the parent menus stay on screen with no way to reach them.
	void dismissPopupChain(NotNull<NativeWindow> w);

	// Take down every Popup/Tooltip owned by `parent` (the cascade carries on to nested levels).
	// Called when something happens to the owner that a menu must not survive: a press inside it,
	// or the window system raising, moving or resizing it.
	void dismissChildPopups(NotNull<NativeWindow> parent, StringView reason);

	// A window lost WindowState::Focused. Menus must not outlive focus, but the check has to be
	// deferred: on Wayland an xdg_popup grab moves keyboard focus off the toplevel and onto the
	// popup itself, so "the parent lost focus" alone would dismiss the menu that just opened.
	void notifyWindowFocusLost(NotNull<NativeWindow>);

	// Optional sink so app-layer log capture (SceneInspector) sees window diagnostics.
	void setWindowDiagSink(Function<void(StringView line)> &&sink) {
		_windowDiagSink = sprt::move(sink);
	}

	// Native window was created on WM side and now operational
	virtual void notifyWindowCreated(NotNull<NativeWindow>);

	// Native window's size, pixel density or transform was changed by WM
	virtual void notifyWindowConstraintsChanged(NotNull<NativeWindow>, UpdateConstraintsFlags);

	// Some input should be transferred to application
	virtual void notifyWindowInputEvents(NotNull<NativeWindow>, Vector<InputEventData> &&);

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

	// Open an OS dialog described by `req`; its completion runs on `target`. Call on this
	// controller's looper. `req->parentWindowId`, when set, must name a live window: the dialog is
	// parented to it, DialogFlags::Modal blocks it, and the dialog is cancelled if it closes.
	//
	// Status::Ok means the dialog was handed to the OS. ANY other return means the completion has
	// ALREADY been posted to `target` with that same status — callers never answer their own
	// callback.
	virtual Status openDialog(NotNull<dispatch::Looper> target, Rc<DialogRequest> &&req);

	// Dismiss a dialog opened with `req`; its completion still runs, with Status::ErrorCancelled.
	// Status::ErrorNotFound if it already finished.
	virtual Status cancelDialog(NotNull<DialogRequest> req);

	// Refuse `req` with `st`, delivering the completion on `target` rather than dropping it.
	// Returns `st`, so a caller can `return declineDialog(...)`.
	Status declineDialog(NotNull<dispatch::Looper> target, Rc<DialogRequest> &&req, Status st);

	// Cancel every dialog owned by `w`, completing each callback with `st`. A dialog must never
	// outlive its parent window.
	void cancelWindowDialogs(NotNull<NativeWindow> w, Status st = Status::ErrorCancelled);

	// The user poked a window blocked by a modal dialog: ask its dialogs to come forward. On
	// platforms with real OS parenting the WM has already done this and the backends no-op.
	void raiseWindowDialogs(NotNull<NativeWindow> w);

	bool isModalBlocked(NotNull<NativeWindow> w) const;

	// Backends call these once the OS has accepted / finished a dialog. registerDialog also takes
	// the modal block on the parent; unregisterDialog releases it, which is why every completion
	// path must go through DialogHandle::finalize (that is where unregisterDialog is called from).
	void registerDialog(NotNull<DialogHandle>);
	void unregisterDialog(NotNull<DialogHandle>);

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

	// Backend part of `createWindow`: create the native window for an already validated
	// and configured WindowInfo. Platforms that can not create windows on demand
	// (Android, iOS - windows come from the OS) keep the default.
	virtual bool loadWindow(Rc<WindowInfo> &&) { return false; }

	void emitWindowDiag(StringView line) const;

	void retainModalBlock(NotNull<NativeWindow>);
	void releaseModalBlock(NotNull<NativeWindow>);

	// Note which pointers/keys go down and come back up, so that cancelWindowInput knows what to
	// release. Called for every dispatched input batch.
	void trackHeldInput(NotNull<NativeWindow>, const Vector<InputEventData> &);

	// Release anything the scene is holding down before input to `w` is cut off.
	void cancelWindowInput(NotNull<NativeWindow>);

	// Request a close on every live window matching `pred`, re-scanning after each step because a
	// close cascades into children. Returns the number of windows asked to close; the teardown
	// itself may be deferred to the end of the poll iteration.
	uint32_t closeWindows(StringView reason, const callback<bool(const WindowInfo &)> &pred);

	// Actually retire a window: unmap it, tell the Context (which winds down its AppWindow and
	// presentation engine) and drop the controller's reference. Idempotent — a window that is no
	// longer active is silently skipped, which is what makes the cascade safe to re-enter.
	void performWindowTeardown(NotNull<NativeWindow>);

	int _resultCode = 0;
	ContextState _state = ContextState::Created;
	Context *_context = nullptr;
	Rc<dispatch::Looper> _looper;

	Rc<ContextInfo> _contextInfo;
	Rc<WindowInfo> _windowInfo;
	Rc<gapi::InstanceInfo> _instanceInfo;
	Rc<gapi::LoopInfo> _loopInfo;

	Rc<DisplayConfigManager> _displayConfigManager;

	NetworkFlags _networkFlags = NetworkFlags::None;
	ThemeInfo _themeInfo;

	Set<Rc<NativeWindow>> _activeWindows;
	Set<NativeWindow *> _allWindows;

	// Dialogs on screen, grouped by the window that owns them (nullptr = parentless). Drained in
	// performWindowTeardown while the native parent is still valid.
	Map<NativeWindow *, Vector<Rc<DialogHandle>>> _dialogs;

	// How many modal dialogs currently block each window. Counted, so nested modals compose and
	// the parent is only unblocked when the last one goes away.
	Map<NativeWindow *, uint32_t> _modalBlocks;

	// Pointers and keys currently down in each window: the Begin / KeyPressed event that has not
	// yet been matched by an End / Cancel / KeyReleased. Kept only so that cancelWindowInput can
	// echo them back as Cancel / KeyCanceled when input to the window is cut off — otherwise the
	// scene keeps a phantom press for as long as a modal dialog is up, and past it.
	Map<NativeWindow *, Vector<InputEventData>> _heldInput;

	uint32_t _lastPointerSerial = 0;
	uint32_t _pollDepth = 0;
	bool _focusDismissScheduled = false;

	Vector<pair<NativeWindow *, UpdateConstraintsFlags>> _resizedWindows;
	Vector<pair<Rc<NativeWindow>, WindowCloseOptions>> _closedWindows;

	Function<void(StringView)> _windowDiagSink;
};

} // namespace sprt::window

#endif
