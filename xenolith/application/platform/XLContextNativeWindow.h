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

#ifndef XENOLITH_APPLICATION_PLATFORM_XLCONTEXTNATIVEWINDOW_H_
#define XENOLITH_APPLICATION_PLATFORM_XLCONTEXTNATIVEWINDOW_H_

#include "XLContextInfo.h" // IWYU pragma: keep
#include "XLCoreTextInput.h"
#include "XLCorePresentationFrame.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

class ContextController;

class NativeWindow : public Ref {
public:
	using TextInputProcessor = core::TextInputProcessor;
	using InputEventData = core::InputEventData;
	using InputEventName = core::InputEventName;
	using TextInputRequest = core::TextInputRequest;
	using TextInputState = core::TextInputState;
	using TextInputFlags = core::TextInputFlags;

	virtual ~NativeWindow();

	virtual bool init(NotNull<ContextController>, Rc<WindowInfo> &&, WindowCapabilities);

	virtual void mapWindow() = 0;
	virtual void unmapWindow() = 0;

	// true if successfully closed
	virtual bool close() = 0;

	virtual void handleFramePresented(NotNull<core::PresentationFrame>) { }

	virtual core::SurfaceInfo getSurfaceOptions(const core::Device &dev,
			NotNull<core::Surface> surface) const;

	virtual core::FrameConstraints exportConstraints() const;

	virtual Extent2 getExtent() const = 0;

	virtual Rc<core::Surface> makeSurface(NotNull<core::Instance>) = 0;

	// Pointer enter layer
	virtual void handleLayerEnter(const WindowLayer &);

	// Pointer exit layer
	virtual void handleLayerExit(const WindowLayer &);

	virtual core::PresentationOptions getPreferredOptions() const {
		return core::PresentationOptions();
	}

	void setFrameOrder(uint64_t v) { _frameOrder = v; }
	uint64_t getFrameOrder() const { return _frameOrder; }

	bool isTextInputEnabled() const { return _textInput->isRunning(); }

	const WindowInfo *getInfo() const { return _info; }

	ContextController *getController() const { return _controller; }

	// application requests
	void acquireTextInput(const TextInputRequest &);
	void releaseTextInput();

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

	virtual Status setFullscreenState(FullscreenInfo &&) { return Status::ErrorNotImplemented; }

	// Force-emit application frame rendering request
	virtual void emitAppFrame();

	virtual void updateState(uint32_t, WindowState);

	virtual void setCursor(WindowCursor) { }

	uint64_t _frameOrder = 0;

	Rc<ContextController> _controller;
	Rc<WindowInfo> _info;
	Rc<core::TextInputProcessor> _textInput;

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
	Vector<core::InputEventData> _pendingEvents;

	WindowLayerFlags _currentLayerFlags = WindowLayerFlags::None;
	WindowLayerFlags _gripFlags = WindowLayerFlags::None;
};

} // namespace stappler::xenolith::platform

#endif /* XENOLITH_APPLICATION_PLATFORM_XLCONTEXTNATIVEWINDOW_H_ */
