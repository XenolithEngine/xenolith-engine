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

#ifndef XENOLITH_APPLICATION_ANDROID_XLANDROIDWINDOW_H_
#define XENOLITH_APPLICATION_ANDROID_XLANDROIDWINDOW_H_

#include "XLAndroid.h" // IWYU pragma: keep
#include "platform/XLContextNativeWindow.h"

#include <android/choreographer.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

class AndroidContextController;
class AndroidActivity;

class AndroidWindow : public NativeWindow {
public:
	virtual ~AndroidWindow();

	bool init(NotNull<AndroidActivity>, Rc<WindowInfo> &&, ANativeWindow *);

	//virtual uint64_t retain(uint64_t id) override { return NativeWindow::retain(id); }

	virtual void mapWindow() override;
	virtual void unmapWindow() override;
	virtual bool close() override;

	virtual void handleFramePresented(NotNull<core::PresentationFrame>) override;

	virtual core::SurfaceInfo getSurfaceOptions(const core::Device &,
			NotNull<core::Surface>) const override;

	virtual core::FrameConstraints exportConstraints() const override;

	virtual Extent2 getExtent() const override;

	virtual Rc<core::Surface> makeSurface(NotNull<core::Instance>) override;

	virtual core::PresentationOptions getPreferredOptions() const override;

	virtual bool enableState(WindowState) override;
	virtual bool disableState(WindowState) override;

	void updateWindow(bool);
	void setContentPadding(const Padding &);

	ANativeWindow *getWindow() const { return _window; }

	void setVsyncPeriod(uint64_t);
	void postDisplayLink();

	virtual void updateLayers(Vector<WindowLayer> &&) override;

	virtual void handleBackButton() override;

	virtual Status setPreferredFrameRate(float) override;

protected:
	friend class AndroidActivity;

	// For InputQueue
	using NativeWindow::updateState;

	virtual bool updateTextInput(const TextInputRequest &,
			TextInputFlags flags = TextInputFlags::RunIfDisabled) override;


	virtual void cancelTextInput() override; // from view thread

	void postFrameCallback();

	float acquireWindowDensity() const;

	Extent2 _extent;
	Extent2 _identityExtent;
	core::ImageFormat _format = core::ImageFormat::Undefined;
	ANativeWindow *_window = nullptr;
	AChoreographer *_choreographer = nullptr;
	AndroidActivity *_activity = nullptr;

	Dso _selfHandle;

	int32_t (*_ANativeWindow_setBuffersTransform)(ANativeWindow *window,
			int32_t transform) = nullptr;
	int32_t (*_ANativeWindow_setFrameRate)(ANativeWindow *window, float frameRate,
			int8_t compatibility) = nullptr;
	int32_t (*_ANativeWindow_setFrameRateWithChangeStrategy)(ANativeWindow *window, float frameRate,
			int8_t compatibility, int8_t changeFrameRateStrategy) = nullptr;

	void (*_AChoreographer_postFrameCallback64)(AChoreographer *choreographer,
			AChoreographer_frameCallback64 callback, void *data) = nullptr;
	void (*_AChoreographer_postVsyncCallback)(AChoreographer *choreographer,
			AChoreographer_vsyncCallback callback, void *data) = nullptr;
	void (*_AChoreographer_registerRefreshRateCallback)(AChoreographer *choreographer,
			AChoreographer_refreshRateCallback, void *data) = nullptr;
	void (*_AChoreographer_unregisterRefreshRateCallback)(AChoreographer *choreographer,
			AChoreographer_refreshRateCallback, void *data) = nullptr;

	bool _refreshRateCallbackRegistred = false;
	uint64_t _vsyncPeriodNanos = 0;

	bool _unmapped = false;
};

} // namespace stappler::xenolith::platform

#endif // XENOLITH_APPLICATION_ANDROID_XLANDROIDWINDOW_H_
