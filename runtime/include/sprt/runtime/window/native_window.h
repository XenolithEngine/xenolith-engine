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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_WINDOW_NATIVE_WINDOW_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_WINDOW_NATIVE_WINDOW_H_

#include <sprt/runtime/ref.h>
#include <sprt/runtime/window/text_input.h>
#include <sprt/runtime/window/interface.h>
#include <sprt/runtime/window/surface_info.h>
#include <sprt/runtime/window/software_surface.h>
#include <sprt/runtime/window/window_info.h>
#include <sprt/runtime/window/presentation.h>
#include <sprt/runtime/window/gapi.h>
#include <sprt/cxx/vector>
#include <sprt/cxx/function>

namespace sprt::window {

class ContextController;

class SPRT_API NativeWindow : public Ref {
public:
	using InputEventData = sprt::window::InputEventData;
	using InputEventName = sprt::window::InputEventName;
	using TextInputProcessor = sprt::window::TextInputProcessor;
	using TextInputRequest = sprt::window::TextInputRequest;
	using TextInputState = sprt::window::TextInputState;
	using TextInputFlags = sprt::window::TextInputFlags;

	virtual ~NativeWindow();

	virtual bool init(NotNull<ContextController>, Rc<WindowInfo> &&, WindowCapabilities);

	virtual void mapWindow() = 0;
	virtual void unmapWindow() = 0;

	// Stop presenting / input monitors. Safe to call multiple times.
	virtual void prepareClose() { }

	virtual bool isMapped() const { return false; }

	// Resize the native content area (points). Keeps the top-left corner fixed when possible.
	virtual bool setContentExtent(Extent2) { return false; }

	// true if successfully closed (destroyed)
	virtual bool close() = 0;

	virtual void handleFrameReady(const PresentationFrameInfo &) { }
	virtual void handleFramePresented(const PresentationFrameInfo &) { }
	virtual void handleSwapchainUpdated(const FrameConstraints &) { }

	virtual SurfaceInterfaceInfo getSurfaceInterfaceInfo() const = 0;

	// CPU-writable presentation, for a gAPI that rasterizes on the host (see software_surface.h).
	// Null means this window system cannot hand out a pixel buffer, which is the only check a
	// caller needs; getSurfaceInterfaceInfo() stays the seam for GPU surfaces.
	virtual Rc<SoftwareSurface> makeSoftwareSurface() { return nullptr; }

	virtual SurfaceInfo getSurfaceOptions(SurfaceInfo &&info) const;

	virtual FrameConstraints exportConstraints(uint64_t &serial) const;

	virtual Extent2 getExtent() const = 0;

	/* The window's content rect in screen coordinates, in LOGICAL units - the same space as
	WindowInfo::rect, so the result can be handed back to createWindow to reopen the window where
	it was.

	The CONTENT rect, not the frame: restoring a frame origin would walk the window down and right
	by the thickness of the decoration on every save/restore cycle.

	The base answers `IRect(0, 0, extent)` - the honest answer for a window system that does not
	tell a client where its window is (Wayland) or has no notion of one (direct output). A backend
	that CAN answer overrides this and sets WindowCapabilities::WindowPosition, which is what tells
	the two zeroes apart from a window really at the origin. Context thread. */
	virtual IRect getContentScreenRect() const;

	// The snapshot an application reads. Assembled from getContentScreenRect(), getExtent() and
	// the window's density; `hasPosition` follows WindowCapabilities::WindowPosition, so a backend
	// that overrides getContentScreenRect() gets this for free. Context thread.
	virtual WindowGeometry getWindowGeometry() const;

	// Pointer enter layer. Notification: the aggregate pointer state (cursor, layer flags, grips)
	// is already recomputed by the time this is called.
	virtual void handleLayerEnter(const WindowLayer &);

	// Pointer exit layer. Notification, see handleLayerEnter.
	virtual void handleLayerExit(const WindowLayer &);

	virtual PresentationOptions getPreferredOptions() const { return PresentationOptions(); }

	void setFrameOrder(uint64_t v) { _frameOrder = v; }
	uint64_t getFrameOrder() const { return _frameOrder; }

	bool isTextInputEnabled() const { return _textInput && _textInput->isRunning(); }

	const WindowInfo *getInfo() const { return _info; }

	// Move the application payload off the window info (see WindowInfo::appData). Meant to be
	// called exactly once, by whichever layer owns this window's content, so the payload does not
	// have to be destroyed on the context thread together with the WindowInfo. Returns null once
	// taken, or when the window was not created with one.
	Rc<Ref> takeAppData() { return _info ? _info->takeAppData() : nullptr; }

	ContextController *getController() const { return _controller; }

	// application requests
	void acquireTextInput(const TextInputRequest &);
	void releaseTextInput();

	// Drive this window's TextInputProcessor as the platform IME would. Used by a test harness to
	// reproduce composition, autocorrection and paste - edits that arrive without a keystroke and
	// therefore cannot be injected as input events. Context thread.
	void performTextInput(const TextInputCommand &);

	void setAppWindow(Rc<AppWindow> &&);
	AppWindow *getAppWindow() const;

	virtual void updateLayers(Vector<WindowLayer> &&);

	virtual void setFullscreen(FullscreenInfo &&, Function<void(Status)> &&cb, Ref *ref);

	virtual void handleInputEvents(Vector<InputEventData> &&events);

	virtual void dispatchPendingEvents();

	virtual bool enableState(WindowState);
	virtual bool disableState(WindowState);

	virtual void openWindowMenu(Vec2 pos);

	virtual void handleBackButton();

	virtual Status setPreferredFrameRate(float);

	// Resize the window from within the application. Only windows that own their extent outright
	// (the headless pseudo-window) can honour this; with a window system in play the size is the
	// WM's to decide. From the context thread.
	virtual Status setExtent(Extent2) { return Status::ErrorNotSupported; }

	// A modal dialog opened on top of this window, or the last one closed.
	//
	// The base clears/sets WindowState::Enabled so the application can SEE that it is blocked —
	// input to it is dropped in ContextController::notifyWindowInputEvents regardless. An override
	// should call the base and then add the advisory OS hint (Win32 EnableWindow,
	// _NET_WM_STATE_MODAL, xdg_dialog_v1); macOS needs none, a sheet blocks its parent by itself.
	// Context thread.
	virtual void setModalBlocked(bool);

protected:
	// Run text input mode or update text input buffer
	//
	// should be forwarded to OS input method
	virtual bool updateTextInput(const TextInputRequest &,
			TextInputFlags flags = TextInputFlags::RunIfDisabled) = 0;

	// Disable text input, if it was enabled
	//
	// should be forwarded to OS input method
	virtual void cancelTextInput() = 0; // from view thread

	virtual void handleMotionEvent(const InputEventData &);

	// Recompute cursor, layer flags and grips from the layers currently under the pointer, and
	// push the cursor down to the window system if the result changed.
	void updateLayerState();

	virtual Status setFullscreenState(FullscreenInfo &&) { return Status::ErrorNotImplemented; }

	// Force-emit application frame rendering request
	virtual void emitAppFrame();

	virtual void updateState(uint32_t, WindowState);

	virtual void setCursor(WindowCursor) { }

	uint64_t _frameOrder = 0;

	Rc<ContextController> _controller;
	Rc<WindowInfo> _info;
	Rc<TextInputProcessor> _textInput;

	Rc<AppWindow> _appWindow;

	// usually, text input can be captured from keyboard, but on some systems text input separated from keyboard input
	bool _handleTextInputFromKeyboard = true;

	// intercept pointer motion event to track layers enter/exit
	// On some WM we can offload layers to WM directly and disable this flag
	bool _handleLayerForMotion = true;

	// On some platforms (MacOS) fullscren op is async, so, we need a flag to check if op is in progress
	// When this flag is set, fullscreen function should return Status::ErrorAgain
	bool _hasPendingFullscreenOp = false;

	bool _allocated = false;

	Vec2 _layerLocation;
	Vector<WindowLayer> _layers;
	Vector<WindowLayer> _currentLayers;
	Vector<InputEventData> _pendingEvents;

	WindowLayerFlags _currentLayerFlags = WindowLayerFlags::None;
	WindowLayerFlags _gripFlags = WindowLayerFlags::None;

	// Last cursor pushed to setCursor(). Starts at the shape a window with no layers already has,
	// so a window the pointer never visited does not touch the window system's cursor at all.
	WindowCursor _layerCursor = WindowCursor::Default;
};

} // namespace sprt::window

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_WINDOW_NATIVE_WINDOW_H_
