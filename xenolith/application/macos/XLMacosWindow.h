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

#ifndef XENOLITH_APPLICATION_MACOS_XLMACOSWINNOW_H_
#define XENOLITH_APPLICATION_MACOS_XLMACOSWINNOW_H_

#include "XLMacos.h"
#include "platform/XLContextNativeWindow.h"
#include <CoreFoundation/CFCGTypes.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

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

	virtual void handleFramePresented(NotNull<core::PresentationFrame>) override;

	virtual core::FrameConstraints exportConstraints(core::FrameConstraints &&) const override;

	virtual Extent2 getExtent() const override;

	virtual Rc<core::Surface> makeSurface(NotNull<core::Instance>) override;

	virtual core::PresentationOptions getPreferredOptions() const override;

	SpanView<WindowLayer> getLayers() const { return _layers; }

	void handleWindowLoaded();
	void handleDisplayLink();

	XLMacosWindow *getWindow() const { return _window; }

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

	XLMacosViewController *_rootViewController = nullptr;
	XLMacosWindow *_window = nullptr;
	WindowCursor _currentCursor = WindowCursor::Undefined;

	bool _initialized = false;
	bool _windowLoaded = false;

	bool _hasOriginalFrame = false;
	CGRect _originalFrame;
	MacosFullscreenRequest _fullscreenRequest = MacosFullscreenRequest::None;
	NSScreen *_nextScreen = nullptr;
};

} // namespace stappler::xenolith::platform

#endif // XENOLITH_APPLICATION_MACOS_XLMACOSWINNOW_H_
