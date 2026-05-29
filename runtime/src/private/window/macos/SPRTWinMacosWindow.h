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

#ifndef CORE_RUNTIME_PRIVATE_WINDOW_WINDOWS_SPRTWINMACOSWINNOW_H_
#define CORE_RUNTIME_PRIVATE_WINDOW_WINDOWS_SPRTWINMACOSWINNOW_H_

#include "private/window/macos/SPRTWinMacos.h"

#if SPRT_MACOS

#include <sprt/runtime/window/native_window.h>
#include <CoreFoundation/CFCGTypes.h>

namespace sprt::window {

enum class MacosFullscreenRequest {
	None,
	EnterFullscreen,
	ExitFullscreen,
	ToggleFullscreen
};

class MacosWindow : public NativeWindow {
public:
	virtual ~MacosWindow();

	virtual bool init(NotNull<ContextController>, Rc<WindowInfo> &&);

	virtual void mapWindow() override;
	virtual void unmapWindow() override;
	virtual bool close() override;

	virtual void handleFramePresented(const PresentationFrameInfo &) override;

	virtual SurfaceInterfaceInfo getSurfaceInterfaceInfo() const override;

	virtual FrameConstraints exportConstraints(uint64_t &serial) const override;

	virtual Extent2 getExtent() const override;

	virtual PresentationOptions getPreferredOptions() const override;

	SpanView<WindowLayer> getLayers() const { return _layers; }

	void handleWindowLoaded();
	void handleDisplayLink();

	SPRTMacosWindow *getWindow() const { return _window; }

	bool hasOriginalFrame() const { return _hasOriginalFrame; }
	CGRect getOriginalFrame() const { return _originalFrame; }

	void handleFullscreenTransitionComplete(MacosFullscreenRequest);

	MacosFullscreenRequest getFullscreenRequest() const { return _fullscreenRequest; }
	NSScreen *getNextScreen() const { return _nextScreen; }

	WindowLayerFlags getGripFlags() const { return _gripFlags; }

	virtual bool enableState(WindowState) override;
	virtual bool disableState(WindowState) override;

	using NativeWindow::updateState;
	using NativeWindow::emitAppFrame;

protected:
	virtual Status setFullscreenState(FullscreenInfo &&info) override;

	virtual bool updateTextInput(const TextInputRequest &,
			TextInputFlags flags = TextInputFlags::RunIfDisabled) override;

	virtual void cancelTextInput() override;

	virtual void setCursor(WindowCursor) override;

	SPRTMacosViewController *_rootViewController = nullptr;
	SPRTMacosWindow *_window = nullptr;
	WindowCursor _currentCursor = WindowCursor::Undefined;

	bool _initialized = false;
	bool _windowLoaded = false;

	bool _hasOriginalFrame = false;
	CGRect _originalFrame;
	MacosFullscreenRequest _fullscreenRequest = MacosFullscreenRequest::None;
	NSScreen *_nextScreen = nullptr;
};

} // namespace sprt::window

#endif

#endif // XENOLITH_APPLICATION_MACOS_XLMACOSWINNOW_H_
