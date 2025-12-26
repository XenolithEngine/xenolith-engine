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

#include "XLLinuxWaylandKdeDisplayConfigManager.h"
#include "SPStatus.h"
#include "SPString.h"
#include "XlCoreMonitorInfo.h"
#include "linux/thirdparty/wayland-protocols/kde-output-device-v2.h"
#include "linux/thirdparty/wayland-protocols/kde-output-management-v2.h"
#include "linux/thirdparty/wayland-protocols/kde-output-order-v1.h"
#include "linux/wayland/XLLinuxWaylandProtocol.h"
#include "platform/XLDisplayConfigManager.h"
#include <wayland-util.h>
#include <xcb/randr.h>

#ifndef XL_WAYLAND_KDE_DEBUG
#define XL_WAYLAND_KDE_DEBUG 0
#endif

#ifndef XL_WAYLAND_KDE_LOG
#if XL_WAYLAND_KDE_DEBUG
#define XL_WAYLAND_KDE_LOG(...) log::source().debug("WaylandKdeDisplayConfigManager", __VA_ARGS__)
#else
#define XL_WAYLAND_KDE_LOG(...)
#endif
#endif

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

struct OutputConfigurationListener {
	WaylandLibrary *wayland = nullptr;
	kde_output_configuration_v2 *config = nullptr;
	Function<void(Status)> callback;
};

// clang-format off

static kde_output_device_v2_listener s_kdeOutputListener{

.geometry = [](void *data, struct kde_output_device_v2 *kde_output_device_v2, int32_t x, int32_t y,
		int32_t physical_width, int32_t physical_height, int32_t subpixel, const char *make, const char *model, int32_t transform) {
	auto dev = reinterpret_cast<KdeOutputDevice *>(data);
	dev->next.x = x;
	dev->next.y = y;
	dev->next.physical_width = physical_width;
	dev->next.physical_height = physical_height;
	dev->next.subpixel = subpixel;
	dev->next.make = make;
	dev->next.model = model;
	dev->next.transform = transform;
	XL_WAYLAND_KDE_LOG("geometry: ", x, " ", y);
},

.current_mode = [](void *data, struct kde_output_device_v2 *kde_output_device_v2, struct kde_output_device_mode_v2 *mode) {
	auto dev = reinterpret_cast<KdeOutputDevice *>(data);

	XL_WAYLAND_KDE_LOG("current_mode: ", mode);

	for (auto &it : dev->modes) {
		if (it->mode == mode) {
			it->next.current = true;
		} else {
			it->next.current = false;
		}
	}
},

.mode = [](void *data, struct kde_output_device_v2 *kde_output_device_v2, struct kde_output_device_mode_v2 *mode) {
	auto dev = reinterpret_cast<KdeOutputDevice *>(data);
	XL_WAYLAND_KDE_LOG("mode: ", mode);
	for (auto &it : dev->modes) {
		if (it->mode == mode) {
			return;
		}
	}

	auto m = dev->manager->addOutputMode(dev, mode);
	dev->modes.emplace_back(sp::move(m));
},

.done = [](void *data, struct kde_output_device_v2 *kde_output_device_v2) {
	auto dev = reinterpret_cast<KdeOutputDevice *>(data);

	for (auto it = dev->modes.begin(); it != dev->modes.end(); ++it) {
		if ((*it)->next.removed) {
			dev->modes.erase(it);
		}
	}

	for (auto &it : dev->modes) { it->data = it->next; }

	dev->data = dev->next;
	dev->dirty = true;

	XL_WAYLAND_KDE_LOG("done");
},

.scale = [](void *data, struct kde_output_device_v2 *kde_output_device_v2, wl_fixed_t factor) {
	auto dev = reinterpret_cast<KdeOutputDevice *>(data);
	dev->next.scale = wl_fixed_to_double(factor);
	XL_WAYLAND_KDE_LOG("scale");
},

.edid = [](void *data, struct kde_output_device_v2 *kde_output_device_v2, const char *raw) {
	auto dev = reinterpret_cast<KdeOutputDevice *>(data);
	auto edidData = base64::decode<Interface>(StringView(raw));
	dev->next.edid = core::EdidInfo::parse(edidData);
	XL_WAYLAND_KDE_LOG("edid");
},

.enabled = [](void *data, struct kde_output_device_v2 *kde_output_device_v2, int32_t enabled) {
	auto dev = reinterpret_cast<KdeOutputDevice *>(data);
	dev->next.enabled = enabled;
	XL_WAYLAND_KDE_LOG("enabled");
},

.uuid = [](void *data, struct kde_output_device_v2 *kde_output_device_v2, const char *uuid) {
	auto dev = reinterpret_cast<KdeOutputDevice *>(data);
	dev->next.uuid = uuid;
	XL_WAYLAND_KDE_LOG("uuid");
},

.serial_number = [](void *data, struct kde_output_device_v2 *kde_output_device_v2, const char *serialNumber) {
	auto dev = reinterpret_cast<KdeOutputDevice *>(data);
	dev->next.serial = serialNumber;
	XL_WAYLAND_KDE_LOG("serial_number");
},

.eisa_id = [](void *data, struct kde_output_device_v2 *kde_output_device_v2, const char *eisaId) {
	auto dev = reinterpret_cast<KdeOutputDevice *>(data);
	dev->next.eisaId = eisaId;
	XL_WAYLAND_KDE_LOG("eisa_id");
},

.capabilities = [](void *data, struct kde_output_device_v2 *kde_output_device_v2, uint32_t flags) {
	auto dev = reinterpret_cast<KdeOutputDevice *>(data);
	dev->next.capabilities = flags;
	XL_WAYLAND_KDE_LOG("capabilities");
},

.overscan = [](void *data, struct kde_output_device_v2 *kde_output_device_v2, uint32_t overscan) {
	auto dev = reinterpret_cast<KdeOutputDevice *>(data);
	dev->next.overscan = overscan;
	XL_WAYLAND_KDE_LOG("overscan");
},

.vrr_policy = [](void *data, struct kde_output_device_v2 *kde_output_device_v2, uint32_t vrr_policy) {
	auto dev = reinterpret_cast<KdeOutputDevice *>(data);
	dev->next.vrr = vrr_policy;
	XL_WAYLAND_KDE_LOG("vrr_policy");
},

.rgb_range = [](void *data, struct kde_output_device_v2 *kde_output_device_v2, uint32_t rgb_range) {
	auto dev = reinterpret_cast<KdeOutputDevice *>(data);
	dev->next.rgb_range = rgb_range;
	XL_WAYLAND_KDE_LOG("rgb_range");
},

.name = [](void *data, struct kde_output_device_v2 *kde_output_device_v2, const char *name) {
	auto dev = reinterpret_cast<KdeOutputDevice *>(data);
	dev->next.name = name;
	XL_WAYLAND_KDE_LOG("name");
},

.high_dynamic_range = [](void *data, struct kde_output_device_v2 *kde_output_device_v2, uint32_t hdr_enabled) {
	auto dev = reinterpret_cast<KdeOutputDevice *>(data);
	dev->next.hdr = hdr_enabled;
	XL_WAYLAND_KDE_LOG("high_dynamic_range");
},

.sdr_brightness = [](void *data, struct kde_output_device_v2 *kde_output_device_v2, uint32_t sdr_brightness) {
	auto dev = reinterpret_cast<KdeOutputDevice *>(data);
	dev->next.sdr_brightness = sdr_brightness;
	XL_WAYLAND_KDE_LOG("sdr_brightness");
},

.wide_color_gamut = [](void *data, struct kde_output_device_v2 *kde_output_device_v2, uint32_t wcg_enabled) {
	auto dev = reinterpret_cast<KdeOutputDevice *>(data);
	dev->next.wcg_enabled = wcg_enabled;
	XL_WAYLAND_KDE_LOG("wide_color_gamut");
},

.auto_rotate_policy = [](void *data, struct kde_output_device_v2 *kde_output_device_v2, uint32_t policy) {
	auto dev = reinterpret_cast<KdeOutputDevice *>(data);
	dev->next.auto_rotate_policy = policy;
	XL_WAYLAND_KDE_LOG("auto_rotate_policy");
},

.icc_profile_path = [](void *data, struct kde_output_device_v2 *kde_output_device_v2, const char *profile_path) {
	auto dev = reinterpret_cast<KdeOutputDevice *>(data);
	dev->next.icc = profile_path;
	XL_WAYLAND_KDE_LOG("icc_profile_path");
},

.brightness_metadata = [](void *data, struct kde_output_device_v2 *kde_output_device_v2,
		uint32_t max_peak_brightness, uint32_t max_frame_average_brightness, uint32_t min_brightness) {
	auto dev = reinterpret_cast<KdeOutputDevice *>(data);
	dev->next.max_peak_brightness = max_peak_brightness;
	dev->next.max_average_brightness = max_frame_average_brightness;
	dev->next.min_brightness = min_brightness;
	XL_WAYLAND_KDE_LOG("brightness_metadata");
},

.brightness_overrides = [](void *data, struct kde_output_device_v2 *kde_output_device_v2,
		int32_t max_peak_brightness, int32_t max_average_brightness, int32_t min_brightness) {
	auto dev = reinterpret_cast<KdeOutputDevice *>(data);
	dev->next.override_max_peak_brightness = max_peak_brightness;
	dev->next.override_max_average_brightness = max_average_brightness;
	dev->next.override_min_brightness = min_brightness;
	XL_WAYLAND_KDE_LOG("brightness_overrides");
},

.sdr_gamut_wideness = [](void *data, struct kde_output_device_v2 *kde_output_device_v2, uint32_t gamut_wideness) {
	auto dev = reinterpret_cast<KdeOutputDevice *>(data);
	dev->next.sdr_gamut_wideness = gamut_wideness;
	XL_WAYLAND_KDE_LOG("sdr_gamut_wideness");
},

.color_profile_source = [](void *data, struct kde_output_device_v2 *kde_output_device_v2, uint32_t source) {
	auto dev = reinterpret_cast<KdeOutputDevice *>(data);
	dev->next.color_profile_source = source;
	XL_WAYLAND_KDE_LOG("color_profile_source");
},

.brightness = [](void *data, struct kde_output_device_v2 *kde_output_device_v2, uint32_t brightness) {
	auto dev = reinterpret_cast<KdeOutputDevice *>(data);
	dev->next.brightness = brightness;
	XL_WAYLAND_KDE_LOG("brightness");
},

.color_power_tradeoff = [](void *data, struct kde_output_device_v2 *kde_output_device_v2, uint32_t preference) {
	auto dev = reinterpret_cast<KdeOutputDevice *>(data);
	dev->next.color_power_tradeoff = preference;
	XL_WAYLAND_KDE_LOG("color_power_tradeoff");
},

.dimming = [](void *data, struct kde_output_device_v2 *kde_output_device_v2, uint32_t multiplier) {
	auto dev = reinterpret_cast<KdeOutputDevice *>(data);
	dev->next.dimming = multiplier;
	XL_WAYLAND_KDE_LOG("dimming");
},

.replication_source = [](void *data, struct kde_output_device_v2 *kde_output_device_v2, const char *source) {
	auto dev = reinterpret_cast<KdeOutputDevice *>(data);
	dev->next.replication_source = source;
	XL_WAYLAND_KDE_LOG("replication_source");
},

.ddc_ci_allowed = [](void *data, struct kde_output_device_v2 *kde_output_device_v2, uint32_t allowed) {
	auto dev = reinterpret_cast<KdeOutputDevice *>(data);
	dev->next.ddc_ci_allowed = allowed;
	XL_WAYLAND_KDE_LOG("ddc_ci_allowed");
},

.max_bits_per_color = [](void *data, struct kde_output_device_v2 *kde_output_device_v2, uint32_t max_bpc) {
	auto dev = reinterpret_cast<KdeOutputDevice *>(data);
	dev->next.max_bits_per_color = max_bpc;
	XL_WAYLAND_KDE_LOG("max_bits_per_color");
},

.max_bits_per_color_range = [](void *data, struct kde_output_device_v2 *kde_output_device_v2,
		uint32_t min_value, uint32_t max_value) {
	auto dev = reinterpret_cast<KdeOutputDevice *>(data);
	dev->next.max_bits_per_color_min = min_value;
	dev->next.max_bits_per_color_max = max_value;
	XL_WAYLAND_KDE_LOG("max_bits_per_color_range");
},

.automatic_max_bits_per_color_limit = [](void *data, struct kde_output_device_v2 *kde_output_device_v2,
		uint32_t max_bpc_limit) {
	auto dev = reinterpret_cast<KdeOutputDevice *>(data);
	dev->next.automatic_max_bits_per_color_limit = max_bpc_limit;
	XL_WAYLAND_KDE_LOG("automatic_max_bits_per_color_limit");
},

.edr_policy = [](void *data, struct kde_output_device_v2 *kde_output_device_v2, uint32_t policy) {
	auto dev = reinterpret_cast<KdeOutputDevice *>(data);
	dev->next.edr_policy = policy;
	XL_WAYLAND_KDE_LOG("edr_policy");
},

.sharpness = [](void *data, struct kde_output_device_v2 *kde_output_device_v2, uint32_t sharpness) {
	auto dev = reinterpret_cast<KdeOutputDevice *>(data);
	dev->next.sharpness = sharpness;
	XL_WAYLAND_KDE_LOG("sharpness");
},

};

static kde_output_device_mode_v2_listener s_kdeOutputModeListener{

.size = [](void *data, struct kde_output_device_mode_v2 *kde_output_device_mode_v2,
		int32_t width, int32_t height) {
	auto mode = reinterpret_cast<KdeOutputMode *>(data);
	mode->next.width = width;
	mode->next.height = height;
	XL_WAYLAND_KDE_LOG("mode.size");
},

.refresh = [](void *data, struct kde_output_device_mode_v2 *kde_output_device_mode_v2,
					int32_t refresh) {
	auto mode = reinterpret_cast<KdeOutputMode *>(data);
	mode->next.rate = refresh;
	XL_WAYLAND_KDE_LOG("mode.refresh");
},

.preferred = [](void *data, struct kde_output_device_mode_v2 *kde_output_device_mode_v2) {
	auto mode = reinterpret_cast<KdeOutputMode *>(data);
	mode->next.preferred = true;
	XL_WAYLAND_KDE_LOG("mode.preferred");
},

.removed = [](void *data, struct kde_output_device_mode_v2 *kde_output_device_mode_v2) {
	auto mode = reinterpret_cast<KdeOutputMode *>(data);
	mode->next.removed = true;
	XL_WAYLAND_KDE_LOG("mode.removed");
},

};

static kde_output_order_v1_listener s_kdeOutputOrderListener{

.output = [](void *data, struct kde_output_order_v1 *kde_output_order_v1, const char *output_name) {
	auto order = reinterpret_cast<KdeOutputOrder *>(data);

	order->next.emplace_back(String(output_name));
	XL_WAYLAND_KDE_LOG("output: ", output_name);
},

.done = [](void *data, struct kde_output_order_v1 *kde_output_order_v1) {
	auto order = reinterpret_cast<KdeOutputOrder *>(data);

	order->data = sp::move(order->next);
	order->next.clear();
	order->dirty = true;
	XL_WAYLAND_KDE_LOG("output: done");
}

};


static kde_output_configuration_v2_listener s_kdeOutputConfigurationListener{

.applied = [](void *data, struct kde_output_configuration_v2 *kde_output_configuration_v2) {
	auto l = reinterpret_cast<OutputConfigurationListener *>(data);
	if (l->callback) {
		l->callback(Status::Ok);
	}
	l->wayland->kde_output_configuration_v2_destroy(l->config);
	delete l;
},

.failed = [](void *data, struct kde_output_configuration_v2 *kde_output_configuration_v2) {
	auto l = reinterpret_cast<OutputConfigurationListener *>(data);
	if (l->callback) {
		l->callback(Status::ErrorInvalidArguemnt);
	}
	l->wayland->kde_output_configuration_v2_destroy(l->config);
	delete l;
},

.failure_reason = [](void *data, struct kde_output_configuration_v2 *kde_output_configuration_v2, const char *reason) {
	log::source().error("WaylandKdeDisplayConfigManager", "Fail to update display configuration: ", reason);
}

};

// clang-format on

KdeOutputMode *KdeOutputDevice::getCurrentMode() const {
	for (auto &it : modes) {
		if (it->data.current) {
			return it.get();
		}
	}
	return nullptr;
}

KdeOutputMode *KdeOutputDevice::getMode(NativeId id) const {
	for (auto &it : modes) {
		if (it->mode == id.ptr) {
			return it.get();
		}
	}
	return nullptr;
}

WaylandKdeDisplayConfigManager::~WaylandKdeDisplayConfigManager() { invalidate(); }

bool WaylandKdeDisplayConfigManager::init(NotNull<WaylandDisplay> disp) {
	if (!DisplayConfigManager::init(nullptr)) {
		return false;
	}

	_display = disp;
	_wayland = disp->wayland;
	return true;
}

void WaylandKdeDisplayConfigManager::setCallback(
		Function<void(NotNull<DisplayConfigManager>)> &&cb) {
	_onConfigChanged = sp::move(cb);
}

void WaylandKdeDisplayConfigManager::addOutput(kde_output_device_v2 *d, uint32_t index) {
	auto dev = Rc<KdeOutputDevice>::create();
	dev->index = index;
	dev->device = d;
	dev->manager = this;

	_wayland->kde_output_device_v2_add_listener(dev->device, &s_kdeOutputListener, dev.get());

	_devices.emplace_back(dev);
}

void WaylandKdeDisplayConfigManager::removeOutput(uint32_t index) {
	for (auto it = _devices.begin(); it != _devices.end(); ++it) {
		if ((*it)->index == index) {
			_wayland->kde_output_device_v2_destroy((*it)->device);
			(*it)->device = nullptr;

			_devices.erase(it);
			break;
		}
	}
}

void WaylandKdeDisplayConfigManager::setOrder(kde_output_order_v1 *order) {
	if (_order) {
		_wayland->kde_output_order_v1_destroy(order);
	}

	auto o = Rc<KdeOutputOrder>::create();
	o->order = order;
	o->manager = this;

	_wayland->kde_output_order_v1_add_listener(order, &s_kdeOutputOrderListener, o.get());

	_order = o;
}

void WaylandKdeDisplayConfigManager::setManager(kde_output_management_v2 *m) { _manager = m; }

Rc<KdeOutputMode> WaylandKdeDisplayConfigManager::addOutputMode(KdeOutputDevice *dev,
		kde_output_device_mode_v2 *mode) {
	Rc<KdeOutputMode> ret = Rc<KdeOutputMode>::create();
	ret->device = dev;
	ret->mode = mode;

	_wayland->kde_output_device_mode_v2_add_listener(mode, &s_kdeOutputModeListener, ret.get());
	return ret;
}

void WaylandKdeDisplayConfigManager::done() {
	// prevent single-output notifications
	bool isDirty = false;
	for (auto &it : _devices) {
		if (it->dirty) {
			it->dirty = false;
			isDirty = true;
		}
	}
	if (isDirty) {
		handleConfigChanged(makeDisplayConfig());
	}
}

void WaylandKdeDisplayConfigManager::invalidate() {
	DisplayConfigManager::invalidate();

	for (auto &it : _devices) {
		_wayland->kde_output_device_v2_destroy(it->device);
		it->device = nullptr;
	}
	_devices.clear();

	if (_order) {
		_wayland->kde_output_order_v1_destroy(_order->order);
		_order->order = nullptr;
		_order = nullptr;
	}

	if (_manager) {
		_wayland->kde_output_management_v2_destroy(_manager);
		_manager = nullptr;
	}

	_display = nullptr;
	_wayland = nullptr;
}

void WaylandKdeDisplayConfigManager::prepareDisplayConfigUpdate(
		Function<void(DisplayConfig *)> &&cb) {
	cb(makeDisplayConfig());
}

void WaylandKdeDisplayConfigManager::applyDisplayConfig(NotNull<DisplayConfig> config,
		Function<void(Status)> &&cb) {
	if (!_manager) {
		cb(Status::ErrorNotImplemented);
		return;
	}

	auto c = _wayland->kde_output_management_v2_create_configuration(_manager);

	bool hasUpdates = false;
	for (auto &it : config->monitors) {
		auto d = getDevice(it.xid);
		if (!d) {
			cb(Status::ErrorInvalidArguemnt);
			return;
		}

		auto &reqMode = it.getCurrent();

		if (auto l = config->getLogical(it.id)) {
			auto scale = std::ceil(d->data.scale);
			auto rectX = l->rect.x / scale;
			auto rectY = l->rect.y / scale;

			_wayland->kde_output_configuration_v2_position(c, d->device, rectX, rectY);
		}

		if (reqMode.xid.ptr != d->getCurrentMode()->mode) {
			auto m = d->getMode(reqMode.xid);
			if (!m) {
				cb(Status::ErrorInvalidArguemnt);
				return;
			} else {
				_wayland->kde_output_configuration_v2_mode(c, d->device, m->mode);
				hasUpdates = true;
			}
		}
	}

	if (!hasUpdates) {
		_wayland->kde_output_configuration_v2_destroy(c);
		cb(Status::Declined);
	} else {
		auto listener = new OutputConfigurationListener;
		listener->wayland = _wayland;
		listener->config = c;
		listener->callback = sp::move(cb);

		_wayland->kde_output_configuration_v2_add_listener(c, &s_kdeOutputConfigurationListener,
				listener);
		_wayland->kde_output_configuration_v2_apply(c);
	}
}

KdeOutputDevice *WaylandKdeDisplayConfigManager::getDevice(NativeId dev) {
	for (auto &it : _devices) {
		if (it->device == dev.ptr) {
			return it.get();
		}
	}
	return nullptr;
}

Rc<DisplayConfig> WaylandKdeDisplayConfigManager::makeDisplayConfig() {
	auto cfg = Rc<DisplayConfig>::create();
	for (auto &it : _devices) {
		auto orderIt = std::find(_order->data.begin(), _order->data.end(), it->data.name);

		uint32_t index = maxOf<uint32_t>();
		if (orderIt != _order->data.end()) {
			index = _order->data.begin() - orderIt;
		}

		auto &m = cfg->monitors.emplace_back(PhysicalDisplay{
			it->device,
			index,
			MonitorId{it->data.name, it->data.edid},
			Extent2{static_cast<uint32_t>(it->data.physical_width),
				static_cast<uint32_t>(it->data.physical_height)},
		});
		for (auto &mIt : it->modes) {
			m.modes.emplace_back(DisplayMode{
				mIt->mode,
				core::ModeInfo{
					static_cast<uint32_t>(mIt->data.width),
					static_cast<uint32_t>(mIt->data.height),
					static_cast<uint32_t>(mIt->data.rate),
					static_cast<float>(it->data.scale),
				},
				String(),
				toString(mIt->data.width, "x", mIt->data.height, "@", mIt->data.rate),
				Vector<float>{static_cast<float>(it->data.scale)},
				mIt->data.preferred,
				mIt->data.current,
			});
		}
		if (it->data.replication_source.empty()) {
			cfg->logical.emplace_back(LogicalDisplay{
				it->device,
				IRect{it->data.x, it->data.y, 0, 0},
				static_cast<float>(it->data.scale),
				static_cast<uint32_t>(it->data.transform),
				index == 0,
				Vector<MonitorId>{
					MonitorId{it->data.name, it->data.edid},
				},
			});
		}
	}
	return cfg;
}

} // namespace stappler::xenolith::platform
