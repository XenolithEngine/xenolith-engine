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

#ifndef RUNTIME_WINDOW_HEADLESS_SPRTWINHEADLESSWINDOW_H_
#define RUNTIME_WINDOW_HEADLESS_SPRTWINHEADLESSWINDOW_H_

#include <sprt/runtime/init.h>

#if __SPRT_RUNTIME_CONFIG_HAVE_WINDOW

#include <sprt/runtime/window/native_window.h>

namespace sprt::window {

class HeadlessContextController;

// Pseudo-window: there is no window system at all.
//
// The gAPI is told SurfaceBackend::Headless, which carries no native handle; the backend answers
// with a synthetic surface over a pseudo-swapchain of ordinary device images (see
// vk::HeadlessSurface / vk::HeadlessSwapchain). Everything a real window would get from the WM -
// extent, density, frame interval - comes from WindowInfo instead, and the extent can be changed
// at runtime through setExtent() by whoever drives the process (the inspector socket).
//
// No input, no cursor, no text input: events are injected by the external controller straight into
// AppWindow::handleInputEvents.
class HeadlessWindow final : public NativeWindow {
public:
	virtual ~HeadlessWindow();

	HeadlessWindow();

	bool init(NotNull<HeadlessContextController>, Rc<WindowInfo> &&);

	virtual void mapWindow() override;
	virtual void unmapWindow() override;
	virtual bool close() override;

	virtual Extent2 getExtent() const override;

	virtual SurfaceInterfaceInfo getSurfaceInterfaceInfo() const override;

	virtual SurfaceInfo getSurfaceOptions(SurfaceInfo &&) const override;

	virtual PresentationOptions getPreferredOptions() const override;

	// Resize the pseudo-screen. Deprecates the swapchain, so the next frame is rendered at the new
	// extent. Must be called on the context thread.
	virtual Status setExtent(Extent2) override;

protected:
	virtual bool updateTextInput(const TextInputRequest &,
			TextInputFlags flags = TextInputFlags::RunIfDisabled) override;
	virtual void cancelTextInput() override;

	Extent2 _extent;
	bool _closed = false;
};

} // namespace sprt::window

#endif // __SPRT_RUNTIME_CONFIG_HAVE_WINDOW

#endif // RUNTIME_WINDOW_HEADLESS_SPRTWINHEADLESSWINDOW_H_
