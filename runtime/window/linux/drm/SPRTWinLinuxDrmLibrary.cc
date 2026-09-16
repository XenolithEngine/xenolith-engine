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

#include "SPRTWinLinuxDrmLibrary.h"

#if SPRT_LINUX

#include <sprt/runtime/log.h>

namespace sprt::window {

DrmLibrary::~DrmLibrary() { }

bool DrmLibrary::init() {
#if SPRT_HAS_LIBDRM
	// SONAME, not the "libdrm.so" devel symlink: the latter is only present when
	// the -dev package is installed.
	_handle = Dso("libdrm.so.2");
	if (!_handle) {
		return false;
	}

	if (open(_handle)) {
		return true;
	}

	_handle = Dso();
#endif
	return false;
}

#if SPRT_HAS_LIBDRM
bool DrmLibrary::open(Dso &handle) {
	SPRT_LOAD_PROTO(handle, drmGetDevices2)
	SPRT_LOAD_PROTO(handle, drmFreeDevices)
	SPRT_LOAD_PROTO(handle, drmGetVersion)
	SPRT_LOAD_PROTO(handle, drmFreeVersion)
	SPRT_LOAD_PROTO(handle, drmModeGetResources)
	SPRT_LOAD_PROTO(handle, drmModeFreeResources)
	SPRT_LOAD_PROTO(handle, drmModeGetConnector)
	SPRT_LOAD_PROTO(handle, drmModeFreeConnector)
	SPRT_LOAD_PROTO(handle, drmModeGetEncoder)
	SPRT_LOAD_PROTO(handle, drmModeFreeEncoder)
	SPRT_LOAD_PROTO(handle, drmModeGetCrtc)
	SPRT_LOAD_PROTO(handle, drmModeFreeCrtc)
	SPRT_LOAD_PROTO(handle, drmModeGetProperty)
	SPRT_LOAD_PROTO(handle, drmModeFreeProperty)
	SPRT_LOAD_PROTO(handle, drmModeGetPropertyBlob)
	SPRT_LOAD_PROTO(handle, drmModeFreePropertyBlob)

	if (!validateFunctionList(&_drm_first_fn, &_drm_last_fn)) {
		oslog::vperror(__SPRT_LOCATION, "DrmLibrary", "Fail to load libdrm.so.2");
		return false;
	}

	// Optional, may stay null on older libdrm - see getConnectorTypeName()
	SPRT_LOAD_PROTO(handle, drmModeGetConnectorTypeName)

	return true;
}

StringView DrmLibrary::getConnectorTypeName(uint32_t connectorType) const {
	if (drmModeGetConnectorTypeName) {
		if (auto name = drmModeGetConnectorTypeName(connectorType)) {
			return StringView(name);
		}
		return StringView("Unknown");
	}

	// Fallback for libdrm < 2.4.112; names match what libdrm returns.
	switch (connectorType) {
	case DRM_MODE_CONNECTOR_VGA: return StringView("VGA");
	case DRM_MODE_CONNECTOR_DVII: return StringView("DVI-I");
	case DRM_MODE_CONNECTOR_DVID: return StringView("DVI-D");
	case DRM_MODE_CONNECTOR_DVIA: return StringView("DVI-A");
	case DRM_MODE_CONNECTOR_Composite: return StringView("Composite");
	case DRM_MODE_CONNECTOR_SVIDEO: return StringView("SVIDEO");
	case DRM_MODE_CONNECTOR_LVDS: return StringView("LVDS");
	case DRM_MODE_CONNECTOR_Component: return StringView("Component");
	case DRM_MODE_CONNECTOR_9PinDIN: return StringView("DIN");
	case DRM_MODE_CONNECTOR_DisplayPort: return StringView("DP");
	case DRM_MODE_CONNECTOR_HDMIA: return StringView("HDMI-A");
	case DRM_MODE_CONNECTOR_HDMIB: return StringView("HDMI-B");
	case DRM_MODE_CONNECTOR_TV: return StringView("TV");
	case DRM_MODE_CONNECTOR_eDP: return StringView("eDP");
	case DRM_MODE_CONNECTOR_VIRTUAL: return StringView("Virtual");
	case DRM_MODE_CONNECTOR_DSI: return StringView("DSI");
	case DRM_MODE_CONNECTOR_DPI: return StringView("DPI");
	case DRM_MODE_CONNECTOR_WRITEBACK: return StringView("Writeback");
	default: break;
	}
	return StringView("Unknown");
}
#else
StringView DrmLibrary::getConnectorTypeName(uint32_t) const { return StringView("Unknown"); }
#endif // SPRT_HAS_LIBDRM

} // namespace sprt::window

#endif // SPRT_LINUX
