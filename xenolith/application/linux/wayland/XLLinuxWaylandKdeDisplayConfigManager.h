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

#ifndef XENOLITH_APPLICATION_LINUX_WAYLAND_XLLINUXWAYLANDKDEDISPLAYCONFIGMANAGER_H_
#define XENOLITH_APPLICATION_LINUX_WAYLAND_XLLINUXWAYLANDKDEDISPLAYCONFIGMANAGER_H_

#include "XLLinuxWaylandDisplay.h"
#include "XlCoreMonitorInfo.h"
#include "linux/thirdparty/wayland-protocols/kde-output-device-v2.h"
#include "linux/wayland/XLLinuxWaylandLibrary.h"
#include "platform/XLDisplayConfigManager.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

struct KdeOutputDevice;

struct KdeOutputModeData {
	int32_t width = 0;
	int32_t height = 0;
	int32_t rate = 0;
	bool preferred = false;
	bool current = false;
	bool removed = false;
};

struct KdeOutputDeviceData {
	core::EdidInfo edid;

	int32_t x = 0;
	int32_t y = 0;
	int32_t physical_width = 0;
	int32_t physical_height = 0;
	int32_t subpixel = 0;
	String make;
	String model;
	int32_t transform = 0;
	double scale = 1.0;
	int32_t enabled = 0;
	String uuid;
	String serial;
	String eisaId;
	uint32_t capabilities = 0;
	uint32_t overscan = 0;
	uint32_t vrr = 0;
	uint32_t rgb_range = 0;
	String name;
	uint32_t hdr = 0;
	uint32_t sdr_brightness = 0;
	uint32_t wcg_enabled = 0;
	uint32_t auto_rotate_policy = 0;
	String icc;
	uint32_t max_peak_brightness = 0;
	uint32_t max_average_brightness = 0;
	uint32_t min_brightness = 0;
	uint32_t override_max_peak_brightness = 0;
	uint32_t override_max_average_brightness = 0;
	uint32_t override_min_brightness = 0;
	uint32_t sdr_gamut_wideness = 0;
	uint32_t color_profile_source = 0;
	uint32_t brightness = 0;
	uint32_t color_power_tradeoff = 0;
	uint32_t dimming = 0;
	String replication_source;
	uint32_t ddc_ci_allowed = 0;
	uint32_t max_bits_per_color = 0;
	uint32_t max_bits_per_color_min = 0;
	uint32_t max_bits_per_color_max = 0;
	uint32_t automatic_max_bits_per_color_limit = 0;
	uint32_t edr_policy = 0;
	uint32_t sharpness = 0;

	core::MonitorId getId() const { return core::MonitorId{name, edid}; }
};

struct KdeOutputMode : public Ref {
	kde_output_device_mode_v2 *mode = nullptr;
	KdeOutputDevice *device = nullptr;

	KdeOutputModeData next;
	KdeOutputModeData data;
};

struct KdeOutputDevice : public Ref {
	uint32_t index = 0;
	kde_output_device_v2 *device = nullptr;
	WaylandKdeDisplayConfigManager *manager = nullptr;

	Vector<Rc<KdeOutputMode>> modes;

	KdeOutputDeviceData next;
	KdeOutputDeviceData data;
	bool dirty = true;

	KdeOutputMode *getCurrentMode() const;
	KdeOutputMode *getMode(NativeId) const;
};

struct KdeOutputOrder : public Ref {
	kde_output_order_v1 *order = nullptr;
	WaylandKdeDisplayConfigManager *manager = nullptr;

	Vector<String> next;
	Vector<String> data;
	bool dirty = true;
};

class SP_PUBLIC WaylandKdeDisplayConfigManager final : public DisplayConfigManager {
public:
	virtual ~WaylandKdeDisplayConfigManager();

	virtual bool init(NotNull<WaylandDisplay>);

	void setCallback(Function<void(NotNull<DisplayConfigManager>)> &&);

	void addOutput(kde_output_device_v2 *, uint32_t index);
	void removeOutput(uint32_t);

	void setOrder(kde_output_order_v1 *);
	void setManager(kde_output_management_v2 *);

	Rc<KdeOutputMode> addOutputMode(KdeOutputDevice *, kde_output_device_mode_v2 *);

	void done();

	virtual void invalidate() override;

protected:
	virtual void prepareDisplayConfigUpdate(Function<void(DisplayConfig *)> &&) override;
	virtual void applyDisplayConfig(NotNull<DisplayConfig>, Function<void(Status)> &&) override;

	KdeOutputDevice *getDevice(NativeId);
	Rc<DisplayConfig> makeDisplayConfig();

	Rc<WaylandDisplay> _display;
	Rc<WaylandLibrary> _wayland;
	Vector<Rc<KdeOutputDevice>> _devices;
	Rc<KdeOutputOrder> _order;
	kde_output_management_v2 *_manager = nullptr;
};

} // namespace stappler::xenolith::platform

#endif // XENOLITH_APPLICATION_LINUX_WAYLAND_XLLINUXWAYLANDKDEDISPLAYCONFIGMANAGER_H_
