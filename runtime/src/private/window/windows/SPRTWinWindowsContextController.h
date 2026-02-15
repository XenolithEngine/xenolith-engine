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

#ifndef CORE_RUNTIME_PRIVATE_WINDOW_WINDOWS_SPRTWINWINDOWSCONTEXTCONTROLLER_H_
#define CORE_RUNTIME_PRIVATE_WINDOW_WINDOWS_SPRTWINWINDOWSCONTEXTCONTROLLER_H_

#include "private/window/windows/SPRTWinWindows.h" // IWYU pragma: keep

#if SPRT_WINDOWS

#include <sprt/runtime/window/controller.h>

namespace sprt::window {

class WindowClass;
class MessageWindow;

class WindowsContextController : public ContextController {
public:
	static void acquireDefaultConfig(ContextConfig &, NativeContextHandle *);

	static Rc<WindowsContextController> create(NotNull<Context>, ContextConfig &&,
			NotNull<LooperAdapter>);

	virtual ~WindowsContextController() = default;

	virtual bool init(NotNull<Context> ctx, ContextConfig &&, NotNull<LooperAdapter>);

	virtual int run(NotNull<ContextContainer> ctx) override;

	virtual bool isCursorSupported(WindowCursor, bool serverSide) const override;
	virtual WindowCapabilities getCapabilities() const override;

	WindowClass *acquuireWindowClass(WideStringView);

	Status handleDisplayChanged(Extent2);

	virtual void handleNetworkStateChanged(NetworkFlags) override;

	virtual void handleContextWillDestroy() override;

	virtual Status readFromClipboard(Rc<ClipboardRequest> &&) override;
	virtual Status writeToClipboard(Rc<ClipboardData> &&) override;

	virtual void openUrl(StringView) override;

	virtual SurfaceSupportInfo getSupportInfo() const override;

protected:
	bool loadWindow();

	Rc<MessageWindow> _messageWindow;
	Map<WideStringView, Rc<WindowClass>> _classes;
};

} // namespace sprt::window

#endif

#endif // CORE_RUNTIME_PRIVATE_WINDOW_WINDOWS_SPRTWINWINDOWSCONTEXTCONTROLLER_H_
