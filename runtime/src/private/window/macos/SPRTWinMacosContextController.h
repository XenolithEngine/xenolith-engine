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

#ifndef CORE_RUNTIME_PRIVATE_WINDOW_WINDOWS_SPRTWINMACOSCONTEXTCONTROLLER_H_
#define CORE_RUNTIME_PRIVATE_WINDOW_WINDOWS_SPRTWINMACOSCONTEXTCONTROLLER_H_

#include "private/window/macos/SPRTWinMacos.h"

#include <sprt/runtime/window/controller.h>

namespace sprt::window {

class MacosContextController : public ContextController {
public:
	static void acquireDefaultConfig(ContextConfig &, NativeContextHandle *);

	static Rc<MacosContextController> create(NotNull<Context>, ContextConfig &&,
			NotNull<LooperAdapter>);

	virtual ~MacosContextController();

	virtual bool init(NotNull<Context> ctx, ContextConfig &&, NotNull<LooperAdapter>);

	ContextInfo *getContextInfo() const { return _contextInfo; }

	virtual int run(NotNull<ContextContainer>) override;

	virtual void handleContextDidStop() override;

	virtual void handleContextWillStart() override;
	virtual void handleContextDidStart() override;

	virtual void handleContextDidDestroy() override;

	virtual void applicationWillTerminate(bool terminated);

	virtual Status readFromClipboard(Rc<ClipboardRequest> &&) override;
	virtual Status writeToClipboard(Rc<ClipboardData> &&) override;

	virtual bool isCursorSupported(WindowCursor, bool serverSide) const override;
	virtual WindowCapabilities getCapabilities() const override;

	virtual void openUrl(StringView) override;

protected:
	SPRTMacosAppDelegate *_appDelegate = nullptr;
	Rc<ContextContainer> _container;

	bool _inTerminate = false;
};

} // namespace sprt::window

#endif // CORE_RUNTIME_PRIVATE_WINDOW_WINDOWS_SPRTWINMACOSCONTEXTCONTROLLER_H_
