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

#ifndef RUNTIME_WINDOW_HEADLESS_SPRTWINHEADLESSCONTROLLER_H_
#define RUNTIME_WINDOW_HEADLESS_SPRTWINHEADLESSCONTROLLER_H_

#include <sprt/runtime/init.h>

#if __SPRT_RUNTIME_CONFIG_HAVE_WINDOW

#include <sprt/runtime/window/controller.h>

namespace sprt::window {

// Cross-platform pseudo-controller: runs the engine on a plain dispatch::Looper with no window
// system underneath.
//
// Selected at runtime (not by platform #if, unlike every other controller) when ContextFlags::
// Headless is set - see ContextController::create. The one thing it must get right for the gAPI is
// getSupportInfo(): an empty backendMask is what makes the Vulkan instance skip VK_KHR_surface and
// every WSI extension, and what makes Context::makeLoop drop the presentation requirement from
// device selection.
//
// Control comes from outside the process over the inspector socket (scene dump, screenshots,
// scene-registered commands, input injection, frame stepping, shutdown).
class HeadlessContextController : public ContextController {
public:
	static Rc<HeadlessContextController> create(NotNull<Context>, ContextConfig &&,
			NotNull<dispatch::Looper>);

	static void acquireDefaultConfig(ContextConfig &, NativeContextHandle *);

	virtual ~HeadlessContextController();

	virtual bool init(NotNull<Context>, ContextConfig &&, NotNull<dispatch::Looper>);

	virtual int run(NotNull<ContextContainer>) override;

	virtual bool isCursorSupported(WindowCursor, bool serverSide) const override { return false; }
	virtual WindowCapabilities getCapabilities() const override;
	virtual void openUrl(StringView) override;

protected:
	virtual bool loadWindow(Rc<WindowInfo> &&) override;
};

} // namespace sprt::window

#endif // __SPRT_RUNTIME_CONFIG_HAVE_WINDOW

#endif // RUNTIME_WINDOW_HEADLESS_SPRTWINHEADLESSCONTROLLER_H_
