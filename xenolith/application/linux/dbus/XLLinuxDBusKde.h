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

#ifndef XENOLITH_APPLICATION_LINUX_DBUS_XLLINUXDBUSKDE_H_
#define XENOLITH_APPLICATION_LINUX_DBUS_XLLINUXDBUSKDE_H_

#include "XLCommon.h"
#include "XlCoreMonitorInfo.h"

#if LINUX

#include "XLLinuxDBusController.h"
#include "platform/XLDisplayConfigManager.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform::dbus {

class SP_PUBLIC KdeDisplayConfigManager : public DisplayConfigManager {
public:
	virtual ~KdeDisplayConfigManager() = default;

	virtual bool init(NotNull<Controller>, Function<void(NotNull<DisplayConfigManager>)> &&);

	virtual void invalidate() override;

protected:
	void updateDisplayConfig(Function<void(DisplayConfig *)> && = nullptr);

	void readDisplayConfig(NotNull<DBusMessage> reply, Function<void(DisplayConfig *)> &&fn);

	virtual void prepareDisplayConfigUpdate(Function<void(DisplayConfig *)> &&) override;
	virtual void applyDisplayConfig(NotNull<DisplayConfig>, Function<void(Status)> &&) override;

	Rc<Controller> _dbus;
	Rc<dbus::BusFilter> _configFilter;
	Map<String, core::EdidInfo> _edidCache;
};

} // namespace stappler::xenolith::platform::dbus

#endif

#endif // XENOLITH_APPLICATION_LINUX_DBUS_XLLINUXDBUSKDE_H_
