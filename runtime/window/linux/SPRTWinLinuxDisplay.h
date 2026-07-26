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

#ifndef CORE_RUNTIME_PRIVATE_WINDOW_LINUX_SPRTWINLINUXDISPLAY_H_
#define CORE_RUNTIME_PRIVATE_WINDOW_LINUX_SPRTWINLINUXDISPLAY_H_

#include <sprt/runtime/init.h>

#if SPRT_LINUX

#include <sprt/runtime/ref.h>
#include <sprt/runtime/window/native_window.h>

namespace sprt::window {

class LinuxContextController;

// Direct-to-display native window: no window system (no Wayland/X11).
//
// Uses VK_KHR_display: the actual surface is created from the physical device's
// enumerated display/plane/mode (see AppWindow::makeSurface Display case, which
// mirrors PresentationEngine::setFullscreenSurface). This class is a thin holder
// for the target extent + a Display-tagged SurfaceInterfaceInfo. There is no OS
// window to map/unmap and (for now) no input/cursor/text-input.
class DisplayWindow final : public NativeWindow {
public:
	virtual ~DisplayWindow();

	DisplayWindow();

	bool init(NotNull<LinuxContextController>, Rc<WindowInfo> &&);

	virtual void mapWindow() override;
	virtual void unmapWindow() override;
	virtual bool close() override;

	virtual Extent2 getExtent() const override;

	virtual SurfaceInterfaceInfo getSurfaceInterfaceInfo() const override;

	// Direct-KMS has no window-system frame callback (no Wayland/X11 display link).
	// The natural vsync is the DRM page-flip completion, which Mesa's wsi_display
	// signals by releasing a swapchain image. So we pace frames with a blocking
	// acquire (acquireImageWithoutFence): the render loop parks in vkAcquireNextImageKHR
	// until a flipped image recycles, instead of polling with timeout=0 and stalling
	// (there is no display-link tick to re-drive scheduleNextImage in KMS mode).
	virtual PresentationOptions getPreferredOptions() const override;

protected:
	virtual bool updateTextInput(const TextInputRequest &,
			TextInputFlags flags = TextInputFlags::RunIfDisabled) override;
	virtual void cancelTextInput() override;

	Extent2 _extent;
};

} // namespace sprt::window

#endif

#endif /* CORE_RUNTIME_PRIVATE_WINDOW_LINUX_SPRTWINLINUXDISPLAY_H_ */
