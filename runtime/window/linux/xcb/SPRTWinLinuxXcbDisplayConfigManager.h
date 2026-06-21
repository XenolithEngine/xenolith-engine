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

#ifndef CORE_RUNTIME_PRIVATE_WINDOW_LINUX_SPRTWINLINUXXCBDISPLAYCONFIGMANAGER_H_
#define CORE_RUNTIME_PRIVATE_WINDOW_LINUX_SPRTWINLINUXXCBDISPLAYCONFIGMANAGER_H_

#include <sprt/runtime/init.h>

#if SPRT_LINUX

#include <sprt/runtime/window/types.h>
#include <sprt/runtime/window/display_config.h>

#include "SPRTWinLinuxXcbLibrary.h"

namespace sprt::window {

static inline float FixedToFloat(xcb_render_fixed_t val) { return float(val) / 65536.0f; }
static inline double FixedToDouble(xcb_render_fixed_t val) { return double(val) / 65536.0; }

class XcbConnection;

class SPRT_API XcbDisplayConfigManager : public DisplayConfigManager {
public:
	virtual ~XcbDisplayConfigManager() = default;

	virtual bool init(NotNull<XcbConnection>, Function<void(NotNull<DisplayConfigManager>)> &&);

	void setCallback(Function<void(NotNull<DisplayConfigManager>)> &&);

	virtual void invalidate() override;

	void update();

	String getMonitorForPosition(int16_t x, int16_t y);

protected:
	void updateDisplayConfig(Function<void(DisplayConfig *)> && = nullptr);

	virtual void prepareDisplayConfigUpdate(Function<void(DisplayConfig *)> &&) override;
	virtual void applyDisplayConfig(NotNull<DisplayConfig>, Function<void(Status)> &&) override;

	Rc<XcbConnection> _connection;
	Rc<XcbLibrary> _xcb;
	xcb_window_t _root = 0;
};

} // namespace sprt::window

#endif

#endif // CORE_RUNTIME_PRIVATE_WINDOW_LINUX_SPRTWINLINUXXCBDISPLAYCONFIGMANAGER_H_
