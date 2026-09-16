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

#ifndef CORE_RUNTIME_PRIVATE_WINDOW_IOS_SPRTWINIOSCONTEXTCONTROLLER_H_
#define CORE_RUNTIME_PRIVATE_WINDOW_IOS_SPRTWINIOSCONTEXTCONTROLLER_H_

#include "SPRTWinIos.h"

#include <sprt/runtime/window/controller.h>

#if SPRT_IOS

namespace sprt::window {

// Preliminary iOS context controller. Mirrors the public surface of
// MacosContextController so window/common/SPRuntimeController.cc can dispatch to
// it, but the UIKit/Metal plumbing is left as a stub for now.
class SPRT_API IosContextController : public ContextController {
public:
	static void acquireDefaultConfig(ContextConfig &, NativeContextHandle *);

	static Rc<IosContextController> create(NotNull<Context>, ContextConfig &&,
			NotNull<dispatch::Looper>);

	virtual ~IosContextController();

	virtual bool init(NotNull<Context> ctx, ContextConfig &&, NotNull<dispatch::Looper>);

	ContextInfo *getContextInfo() const { return _contextInfo; }

	virtual int run(NotNull<ContextContainer>) override;

	virtual bool isCursorSupported(WindowCursor, bool serverSide) const override;
	virtual WindowCapabilities getCapabilities() const override;

	virtual void openUrl(StringView) override;

	virtual SurfaceSupportInfo getSupportInfo() const override;

protected:
	Rc<ContextContainer> _container;
};

} // namespace sprt::window

#endif // SPRT_IOS

#endif // CORE_RUNTIME_PRIVATE_WINDOW_IOS_SPRTWINIOSCONTEXTCONTROLLER_H_
