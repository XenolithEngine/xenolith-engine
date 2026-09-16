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

#include "SPRTWinLinuxDrmDevice.h"

#if SPRT_LINUX

#include "SPRTWinLinuxDrmLibrary.h"

#include <sprt/runtime/log.h>

#include <fcntl.h>
#include <unistd.h>

namespace sprt::window {

DrmDevice::~DrmDevice() {
	if (_fd >= 0) {
		::close(_fd);
		_fd = -1;
	}
}

#if SPRT_HAS_LIBDRM

// Refresh rate in mHz (FPS multiplied by 1000), which is what ModeInfo::rate wants.
// drmModeModeInfo::vrefresh is a rounded integer Hz, so derive from the timings.
static uint32_t Drm_getModeRate(const drmModeModeInfo &mode) {
	if (mode.htotal == 0 || mode.vtotal == 0) {
		return 0;
	}

	uint64_t rate = (uint64_t(mode.clock) * 1'000'000ull / mode.htotal + mode.vtotal / 2)
			/ mode.vtotal;
	if (mode.flags & DRM_MODE_FLAG_INTERLACE) {
		rate *= 2;
	}
	if (mode.flags & DRM_MODE_FLAG_DBLSCAN) {
		rate /= 2;
	}
	if (mode.vscan > 1) {
		rate /= mode.vscan;
	}
	return uint32_t(rate);
}

static bool Drm_isModeBetter(const drmModeModeInfo &mode, const drmModeModeInfo &best) {
	const bool modePreferred = (mode.type & DRM_MODE_TYPE_PREFERRED) != 0;
	const bool bestPreferred = (best.type & DRM_MODE_TYPE_PREFERRED) != 0;
	if (modePreferred != bestPreferred) {
		return modePreferred;
	}

	const auto modeArea = uint64_t(mode.hdisplay) * uint64_t(mode.vdisplay);
	const auto bestArea = uint64_t(best.hdisplay) * uint64_t(best.vdisplay);
	if (modeArea != bestArea) {
		return modeArea > bestArea;
	}
	return Drm_getModeRate(mode) > Drm_getModeRate(best);
}

// Last-resort size source when the kernel has no modeset and the connector lists
// no usable mode: the fbdev emulation layer still reports the scanout size.
static bool Drm_readFbVirtualSize(uint32_t &w, uint32_t &h) {
	int fd = ::open("/sys/class/graphics/fb0/virtual_size", O_RDONLY | O_CLOEXEC);
	if (fd < 0) {
		return false;
	}

	char buf[64] = {};
	const auto n = ::read(fd, buf, sizeof(buf) - 1);
	::close(fd);
	if (n <= 0) {
		return false;
	}

	// Format is "WWWW,HHHH\n"
	unsigned long ww = 0, hh = 0;
	const char *p = buf;
	while (*p >= '0' && *p <= '9') {
		ww = ww * 10u + unsigned(*p - '0');
		++p;
	}
	if (*p != ',') {
		return false;
	}
	++p;
	while (*p >= '0' && *p <= '9') {
		hh = hh * 10u + unsigned(*p - '0');
		++p;
	}
	if (ww == 0 || hh == 0) {
		return false;
	}

	w = uint32_t(ww);
	h = uint32_t(hh);
	return true;
}

Rc<DrmDevice> DrmDevice::openFirst(NotNull<DrmLibrary> lib) {
	auto count = lib->drmGetDevices2(0, nullptr, 0);
	if (count > 0) {
		Vector<drmDevicePtr> devices;
		devices.resize(count);

		count = lib->drmGetDevices2(0, devices.data(), count);
		if (count > 0) {
			Rc<DrmDevice> ret;
			for (int i = 0; i < count; ++i) {
				auto dev = devices[i];
				if ((dev->available_nodes & (1 << DRM_NODE_PRIMARY)) == 0) {
					continue;
				}
				ret = Rc<DrmDevice>::create(lib, StringView(dev->nodes[DRM_NODE_PRIMARY]));
				if (ret) {
					break;
				}
			}
			lib->drmFreeDevices(devices.data(), count);
			if (ret) {
				return ret;
			}
			return nullptr;
		}
	}

	// drmGetDevices2 can fail on exotic setups (no sysfs); fall back to the
	// conventional node names.
	for (auto path : {StringView("/dev/dri/card0"), StringView("/dev/dri/card1")}) {
		if (auto ret = Rc<DrmDevice>::create(lib, path)) {
			return ret;
		}
	}
	return nullptr;
}

bool DrmDevice::init(NotNull<DrmLibrary> lib, StringView path) {
	_drm = lib;
	_path = path.str<String>();

	_fd = ::open(_path.c_str(), O_RDWR | O_CLOEXEC);
	if (_fd < 0) {
		return false;
	}

	if (!resolveTarget()) {
		::close(_fd);
		_fd = -1;
		return false;
	}

	return true;
}

bool DrmDevice::resolveTarget() {
	auto res = _drm->drmModeGetResources(_fd);
	if (!res) {
		return false;
	}

	uint32_t connectorId = 0;
	uint32_t encoderId = 0;
	ModeInfo mode;

	for (int i = 0; i < res->count_connectors; ++i) {
		auto conn = _drm->drmModeGetConnector(_fd, res->connectors[i]);
		if (!conn) {
			continue;
		}

		if (conn->connection == DRM_MODE_CONNECTED && conn->count_modes > 0) {
			connectorId = conn->connector_id;
			encoderId = conn->encoder_id;

			// EDID preferred (or largest listed) mode on this connector
			const drmModeModeInfo *best = nullptr;
			for (int m = 0; m < conn->count_modes; ++m) {
				auto &it = conn->modes[m];
				if (it.hdisplay == 0 || it.vdisplay == 0) {
					continue;
				}
				if (!best || Drm_isModeBetter(it, *best)) {
					best = &it;
				}
			}
			if (best) {
				mode = ModeInfo{best->hdisplay, best->vdisplay, Drm_getModeRate(*best), 1.0f};
			}

			oslog::vpinfo(__SPRT_LOCATION, "DrmDevice", "Using first connected connector id=",
					connectorId, " ", _drm->getConnectorTypeName(conn->connector_type), "-",
					conn->connector_type_id, " (index ", i, ")");
		}

		_drm->drmModeFreeConnector(conn);

		if (connectorId != 0) {
			break;
		}
	}

	_drm->drmModeFreeResources(res);

	if (connectorId == 0) {
		return false;
	}

	_connectorId = connectorId;

	// The kernel's active modeset wins over the EDID pick: it is what QEMU
	// (video=1024x768) and a booted RPi already scan out, and QEMU virtio-gpu
	// advertises bogus EDID modes (e.g. 5120x2160) that would OOM the swapchain.
	const char *src = "drm-preferred";
	if (encoderId != 0) {
		if (auto enc = _drm->drmModeGetEncoder(_fd, encoderId)) {
			if (enc->crtc_id != 0) {
				if (auto crtc = _drm->drmModeGetCrtc(_fd, enc->crtc_id)) {
					if (crtc->mode_valid && crtc->mode.hdisplay != 0 && crtc->mode.vdisplay != 0) {
						_crtcId = crtc->crtc_id;
						mode = ModeInfo{crtc->mode.hdisplay, crtc->mode.vdisplay,
							Drm_getModeRate(crtc->mode), 1.0f};
						src = "drm-crtc";
					}
					_drm->drmModeFreeCrtc(crtc);
				}
			}
			_drm->drmModeFreeEncoder(enc);
		}
	}

	if (mode.width == 0 || mode.height == 0) {
		uint32_t w = 0, h = 0;
		if (!Drm_readFbVirtualSize(w, h)) {
			return false;
		}
		mode = ModeInfo{w, h, 0, 1.0f};
		src = "fb0";
	}

	_mode = mode;

	oslog::vpinfo(__SPRT_LOCATION, "DrmDevice", "Display mode from ", src, ": ", _mode.width, "x",
			_mode.height, "@", _mode.rate / 1'000, " (", _path, ")");
	return true;
}

bool DrmDevice::readConfig(NotNull<DisplayConfig> config) const {
	auto res = _drm->drmModeGetResources(_fd);
	if (!res) {
		return false;
	}

	uint32_t index = 0;
	for (int i = 0; i < res->count_connectors; ++i) {
		auto conn = _drm->drmModeGetConnector(_fd, res->connectors[i]);
		if (!conn) {
			continue;
		}

		if (conn->connection != DRM_MODE_CONNECTED || conn->count_modes == 0) {
			_drm->drmModeFreeConnector(conn);
			continue;
		}

		auto name = toString(_drm->getConnectorTypeName(conn->connector_type), "-",
				conn->connector_type_id);

		auto &monitor = config->monitors.emplace_back(PhysicalDisplay{
			uintptr_t(conn->connector_id),
			index,
			MonitorId{name},
			Extent2(conn->mmWidth, conn->mmHeight),
		});

		// EDID arrives as a blob property on the connector
		for (int p = 0; p < conn->count_props; ++p) {
			auto prop = _drm->drmModeGetProperty(_fd, conn->props[p]);
			if (!prop) {
				continue;
			}
			if ((prop->flags & DRM_MODE_PROP_BLOB) != 0 && StringView(prop->name) == "EDID") {
				if (auto blob = _drm->drmModeGetPropertyBlob(_fd, uint32_t(conn->prop_values[p]))) {
					monitor.id.edid = EdidInfo::parse(
							BytesView(static_cast<const uint8_t *>(blob->data), blob->length));
					_drm->drmModeFreePropertyBlob(blob);
				}
			}
			_drm->drmModeFreeProperty(prop);
		}

		// The active CRTC mode marks the current one; there is no DRM object id
		// for a mode, so its index within the connector serves as the xid.
		drmModeModeInfo currentMode = {};
		bool hasCurrentMode = false;
		IRect logicalRect;
		if (conn->encoder_id != 0) {
			if (auto enc = _drm->drmModeGetEncoder(_fd, conn->encoder_id)) {
				if (enc->crtc_id != 0) {
					if (auto crtc = _drm->drmModeGetCrtc(_fd, enc->crtc_id)) {
						if (crtc->mode_valid) {
							currentMode = crtc->mode;
							hasCurrentMode = true;
							logicalRect = IRect{int32_t(crtc->x), int32_t(crtc->y),
								crtc->mode.hdisplay, crtc->mode.vdisplay};
						}
						_drm->drmModeFreeCrtc(crtc);
					}
				}
				_drm->drmModeFreeEncoder(enc);
			}
		}

		for (int m = 0; m < conn->count_modes; ++m) {
			auto &it = conn->modes[m];
			auto rate = Drm_getModeRate(it);
			monitor.modes.emplace_back(DisplayMode{
				uintptr_t(m),
				ModeInfo{it.hdisplay, it.vdisplay, rate, 1.0f},
				String(),
				toString(it.hdisplay, "x", it.vdisplay, "@", rate),
				Vector<float>(),
				(it.type & DRM_MODE_TYPE_PREFERRED) != 0,
				hasCurrentMode && it.hdisplay == currentMode.hdisplay
						&& it.vdisplay == currentMode.vdisplay
						&& rate == Drm_getModeRate(currentMode),
			});
		}

		if (logicalRect.width == 0 || logicalRect.height == 0) {
			auto &cMode = monitor.getCurrent();
			logicalRect = IRect{0, 0, cMode.mode.width, cMode.mode.height};
		}

		config->logical.emplace_back(LogicalDisplay{
			uintptr_t(conn->connector_id),
			logicalRect,
			1.0f,
			0,
			index == 0,
			Vector<MonitorId>{monitor.id},
		});

		_drm->drmModeFreeConnector(conn);
		++index;
	}

	_drm->drmModeFreeResources(res);
	return !config->monitors.empty();
}

void DrmDevice::logConnectors() const {
	auto res = _drm->drmModeGetResources(_fd);
	if (!res || res->count_connectors == 0) {
		oslog::vpinfo(__SPRT_LOCATION, "DrmDevice", "no connectors on ", _path);
		if (res) {
			_drm->drmModeFreeResources(res);
		}
		return;
	}

	oslog::vpinfo(__SPRT_LOCATION, "DrmDevice", _path, ": ", res->count_connectors, " connector(s)");

	for (int i = 0; i < res->count_connectors; ++i) {
		auto conn = _drm->drmModeGetConnector(_fd, res->connectors[i]);
		if (!conn) {
			continue;
		}

		uint32_t curW = 0, curH = 0;
		if (conn->encoder_id != 0) {
			if (auto enc = _drm->drmModeGetEncoder(_fd, conn->encoder_id)) {
				if (enc->crtc_id != 0) {
					if (auto crtc = _drm->drmModeGetCrtc(_fd, enc->crtc_id)) {
						if (crtc->mode_valid) {
							curW = crtc->mode.hdisplay;
							curH = crtc->mode.vdisplay;
						}
						_drm->drmModeFreeCrtc(crtc);
					}
				}
				_drm->drmModeFreeEncoder(enc);
			}
		}

		StringView connection;
		switch (conn->connection) {
		case DRM_MODE_CONNECTED: connection = StringView("connected"); break;
		case DRM_MODE_DISCONNECTED: connection = StringView("disconnected"); break;
		default: connection = StringView("unknown"); break;
		}

		oslog::vpinfo(__SPRT_LOCATION, "DrmDevice", "  [", i, "] id=", conn->connector_id, " ",
				_drm->getConnectorTypeName(conn->connector_type), "-", conn->connector_type_id,
				" status=", connection, " modes=", conn->count_modes, " crtc=", curW, "x", curH);

		// Cap log spam: preferred + first few + largest.
		int largestIdx = 0;
		uint64_t largestArea = 0;
		for (int m = 0; m < conn->count_modes; ++m) {
			const auto area = uint64_t(conn->modes[m].hdisplay) * uint64_t(conn->modes[m].vdisplay);
			if (area > largestArea) {
				largestArea = area;
				largestIdx = m;
			}
		}

		int shown = 0;
		for (int m = 0; m < conn->count_modes; ++m) {
			auto &md = conn->modes[m];
			const bool preferred = (md.type & DRM_MODE_TYPE_PREFERRED) != 0;
			const bool largest = (m == largestIdx);
			if (!preferred && !largest && m >= 3) {
				continue;
			}
			oslog::vpinfo(__SPRT_LOCATION, "DrmDevice", "      mode ", md.hdisplay, "x",
					md.vdisplay, "@", Drm_getModeRate(md) / 1'000, preferred ? " preferred" : "",
					largest ? " largest" : "");
			if (++shown >= 8) {
				oslog::vpinfo(__SPRT_LOCATION, "DrmDevice", "      ... ", conn->count_modes - shown,
						" more mode(s)");
				break;
			}
		}

		_drm->drmModeFreeConnector(conn);
	}

	_drm->drmModeFreeResources(res);
}

#else // SPRT_HAS_LIBDRM

// No libdrm in this sysroot: the class keeps its shape, it just never finds a
// device, so direct-to-display mode stays off (see LinuxContextController).
Rc<DrmDevice> DrmDevice::openFirst(NotNull<DrmLibrary>) { return nullptr; }

bool DrmDevice::init(NotNull<DrmLibrary>, StringView) { return false; }

bool DrmDevice::resolveTarget() { return false; }

bool DrmDevice::readConfig(NotNull<DisplayConfig>) const { return false; }

void DrmDevice::logConnectors() const { }

#endif // SPRT_HAS_LIBDRM

} // namespace sprt::window

#endif // SPRT_LINUX
