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

#ifndef CORE_RUNTIME_PRIVATE_WINDOW_LINUX_SPRTWINLINUXCONTROLLER_H_
#define CORE_RUNTIME_PRIVATE_WINDOW_LINUX_SPRTWINLINUXCONTROLLER_H_

#include <sprt/runtime/window/controller.h>

#if SPRT_LINUX

namespace sprt::window::dbus {

class Library;
class Controller;

} // namespace sprt::window::dbus

namespace sprt::window {

class NativeWindow;
class XcbConnection;
class XcbLibrary;
class XkbLibrary;
class WaylandLibrary;
struct WaylandDisplay;

class LinuxContextController : public ContextController {
public:
	static void acquireDefaultConfig(ContextConfig &, NativeContextHandle *);

	static Rc<LinuxContextController> create(NotNull<Context>, ContextConfig &&,
			NotNull<LooperAdapter>);

	virtual ~LinuxContextController() = default;

	virtual bool init(NotNull<Context>, ContextConfig &&, NotNull<LooperAdapter>);

	virtual int run(NotNull<ContextContainer>) override;

	virtual bool isCursorSupported(WindowCursor, bool serverSide) const override;
	virtual WindowCapabilities getCapabilities() const override;

	XcbConnection *getXcbConnection() const { return _xcbConnection; }
	WaylandDisplay *getWaylandDisplay() const { return _waylandDisplay; }

	void notifyScreenChange(NotNull<DisplayConfigManager>);

	virtual Status readFromClipboard(Rc<ClipboardRequest> &&) override;
	virtual Status probeClipboard(Rc<ClipboardProbe> &&) override;
	virtual Status writeToClipboard(Rc<ClipboardData> &&) override;

	virtual void handleThemeInfoChanged(ThemeInfo &&) override;

	void tryStart();

	virtual void openUrl(StringView) override;

	virtual SurfaceSupportInfo getSupportInfo() const override;

protected:
	bool loadWindow();

	virtual void handleContextWillDestroy() override;
	virtual void handleContextDidDestroy() override;

	Rc<XcbLibrary> _xcb;
	Rc<WaylandLibrary> _wayland;
	Rc<XkbLibrary> _xkb;
	Rc<dbus::Library> _dbus;

	Rc<dbus::Controller> _dbusController;
	Rc<XcbConnection> _xcbConnection;
	Rc<WaylandDisplay> _waylandDisplay;

	Rc<HandleAdapter> _xcbPollHandle;
	Rc<HandleAdapter> _waylandPollHandle;
};

} // namespace sprt::window

#endif

#endif /* CORE_RUNTIME_PRIVATE_WINDOW_LINUX_SPRTWINLINUXCONTROLLER_H_ */
