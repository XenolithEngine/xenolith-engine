/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
SPDX-License-Identifier: MIT
**/
#include "SPRTWinWasmWindow.h"

#if SPRT_WASM

#include "SPRTWinWasmController.h"

// Host import: the viewport backing size in device pixels, packed as (width << 16 | height), or 0
// when the JS side did not provide a canvas. Lets the engine render at the real window resolution.
__attribute__((import_module("sprt"), import_name("display_size"))) //
extern "C" uint32_t __sprt_host_display_size();

// Pixel density (devicePixelRatio) x1000, or 0 if unknown. Used so content is laid out in logical
// (CSS) pixels and rendered at the native device resolution instead of appearing tiny/upscaled.
__attribute__((import_module("sprt"), import_name("display_density"))) //
extern "C" uint32_t __sprt_host_display_density();

namespace sprt::window {

WasmWindow::WasmWindow() { }
WasmWindow::~WasmWindow() { }

bool WasmWindow::init(NotNull<WasmContextController> c, Rc<WindowInfo> &&info,
		WindowCapabilities caps) {
	uint32_t packed = __sprt_host_display_size();
	uint32_t w = packed >> 16, h = packed & 0xFFFF;
	_extent = (w > 0 && h > 0) ? Extent2(w, h) : Extent2(800, 600);
	uint32_t dens = __sprt_host_display_density();
	_density = dens > 0 ? float(dens) / 1000.0f : 1.0f;

	// Lay content out in logical (CSS) pixels and render at the device resolution: the frame
	// constraints take their density from the window info.
	info->density = _density;

	if (!NativeWindow::init(c, sprt::move(info), caps)) {
		return false;
	}
	return true;
}

SurfaceInterfaceInfo WasmWindow::getSurfaceInterfaceInfo() const {
	SurfaceInterfaceInfo info;
	info.backend = SurfaceBackend::Canvas;
	info.canvas.handle = nullptr; // the JS binding maps this to the OffscreenCanvas
	return info;
}

SurfaceInfo WasmWindow::getSurfaceOptions(SurfaceInfo &&info) const {
	info.currentExtent = _extent;
	return sprt::move(info);
}

} // namespace sprt::window

#endif
