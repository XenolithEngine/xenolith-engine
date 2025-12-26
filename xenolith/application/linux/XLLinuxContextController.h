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

#ifndef XENOLITH_APPLICATION_LINUX_XLLINUXCONTEXTCONTROLLER_H_
#define XENOLITH_APPLICATION_LINUX_XLLINUXCONTEXTCONTROLLER_H_

#include "XLContextInfo.h"
#include "SPEventPollHandle.h"
#include "dbus/XLLinuxDBusLibrary.h"
#include "platform/XLContextController.h"

#include "wayland/XLLinuxWaylandDisplay.h"
#include "xcb/XLLinuxXcbConnection.h"
#include "dbus/XLLinuxDBusController.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

class NativeWindow;

class LinuxContextController : public ContextController {
public:
	static void acquireDefaultConfig(ContextConfig &, NativeContextHandle *);

	virtual ~LinuxContextController() = default;

	virtual bool init(NotNull<Context>, ContextConfig &&);

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

protected:
	Rc<core::Instance> loadInstance();
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

	Rc<event::PollHandle> _xcbPollHandle;
	Rc<event::PollHandle> _waylandPollHandle;
};

} // namespace stappler::xenolith::platform

#endif /* XENOLITH_APPLICATION_LINUX_XLLINUXCONTEXTCONTROLLER_H_ */
