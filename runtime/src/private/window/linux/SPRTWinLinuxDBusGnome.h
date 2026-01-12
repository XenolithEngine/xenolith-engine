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

#ifndef CORE_RUNTIME_PRIVATE_WINDOW_LINUX_SPRTWINLINUXDBUSGNOME_H_
#define CORE_RUNTIME_PRIVATE_WINDOW_LINUX_SPRTWINLINUXDBUSGNOME_H_

#include <sprt/runtime/init.h>

#if SPRT_LINUX

#include <sprt/runtime/window/display_config.h>
#include "SPRTWinLinuxDBusController.h"

namespace sprt::window::dbus {

class SPRT_API GnomeDisplayConfigManager : public DisplayConfigManager {
public:
	virtual ~GnomeDisplayConfigManager() = default;

	virtual bool init(NotNull<Controller>, Function<void(NotNull<DisplayConfigManager>)> &&);

	virtual void invalidate() override;

protected:
	void updateDisplayConfig(Function<void(DisplayConfig *)> && = nullptr);

	virtual void prepareDisplayConfigUpdate(Function<void(DisplayConfig *)> &&) override;
	virtual void applyDisplayConfig(NotNull<DisplayConfig>, Function<void(Status)> &&) override;

	Rc<Controller> _dbus;
	Rc<dbus::BusFilter> _configFilter;
};

} // namespace sprt::window::dbus

#endif

#endif // CORE_RUNTIME_PRIVATE_WINDOW_LINUX_SPRTWINLINUXDBUSGNOME_H_
