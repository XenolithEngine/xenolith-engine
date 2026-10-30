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

#ifndef RUNTIME_WINDOW_TAU_SPRTWINTAUCONTROLLER_H_
#define RUNTIME_WINDOW_TAU_SPRTWINTAUCONTROLLER_H_

#include <sprt/runtime/window/controller.h>

#if SPRT_TAU

namespace sprt::window {

// One fullscreen window over dev.display@1, with CPU buffers or Vulkan display WSI.
class TauContextController : public ContextController {
public:
	static Rc<TauContextController> create(NotNull<Context>, ContextConfig &&, NotNull<dispatch::Looper>);

	static void acquireDefaultConfig(ContextConfig &, NativeContextHandle *);

	virtual ~TauContextController();

	virtual bool init(NotNull<Context>, ContextConfig &&, NotNull<dispatch::Looper>);

	virtual int run(NotNull<ContextContainer>) override;

	virtual bool isCursorSupported(WindowCursor, bool serverSide) const override { return false; }
	virtual WindowCapabilities getCapabilities() const override { return WindowCapabilities::None; }
	virtual SurfaceSupportInfo getSupportInfo() const override;
	bool isVulkanDisplay() const { return _vulkanDisplay; }
	virtual void openUrl(StringView) override;

protected:
	bool _vulkanDisplay = false;
	virtual bool loadWindow(Rc<WindowInfo> &&) override;
};

} // namespace sprt::window

#endif // SPRT_TAU

#endif // RUNTIME_WINDOW_TAU_SPRTWINTAUCONTROLLER_H_
