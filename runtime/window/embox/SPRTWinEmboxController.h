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

#ifndef RUNTIME_WINDOW_EMBOX_SPRTWINEMBOXCONTROLLER_H_
#define RUNTIME_WINDOW_EMBOX_SPRTWINEMBOXCONTROLLER_H_

#include <sprt/runtime/window/controller.h>

#if SPRT_EMBOX_ANY

namespace sprt::window {

// Flat Embox image: one fullscreen window over /dev/fb0. The software rasterizer writes the
// window's CPU buffers; Vulkan (lavapipe in the image) presents through VK_KHR_display, which
// Mesa's fbdev WSI puts on the same framebuffer.
class EmboxContextController : public ContextController {
public:
	static Rc<EmboxContextController> create(NotNull<Context>, ContextConfig &&,
			NotNull<dispatch::Looper>);

	static void acquireDefaultConfig(ContextConfig &, NativeContextHandle *);

	virtual ~EmboxContextController();

	virtual bool init(NotNull<Context>, ContextConfig &&, NotNull<dispatch::Looper>);

	virtual int run(NotNull<ContextContainer>) override;

	virtual bool isCursorSupported(WindowCursor, bool serverSide) const override { return false; }
	virtual WindowCapabilities getCapabilities() const override;
	virtual SurfaceSupportInfo getSupportInfo() const override;
	virtual void openUrl(StringView) override;

	// Vulkan presents through VK_KHR_display. XL_VK_DISPLAY=0: frames are copied into the
	// window's CPU buffers instead (the engine's headless swapchain with an output).
	bool isVulkanDisplay() const { return _vulkanDisplay; }

protected:
	virtual bool loadWindow(Rc<WindowInfo> &&) override;

	bool _vulkanDisplay = false;
};

} // namespace sprt::window

#endif // SPRT_EMBOX_ANY

#endif // RUNTIME_WINDOW_EMBOX_SPRTWINEMBOXCONTROLLER_H_
