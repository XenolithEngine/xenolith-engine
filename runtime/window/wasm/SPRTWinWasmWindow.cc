/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
SPDX-License-Identifier: MIT
**/
#include "SPRTWinWasmWindow.h"

#if SPRT_WASM

#include "SPRTWinWasmController.h"
#include <sprt/runtime/window/input.h>
#include <sprt/runtime/window/presentation.h>
#include <sprt/runtime/window/controller.h>

// Host import: the viewport backing size in device pixels, packed as (width << 16 | height), or 0
// when the JS side did not provide a canvas. Lets the engine render at the real window resolution.
__attribute__((import_module("sprt"), import_name("display_size"))) //
extern "C" uint32_t __sprt_host_display_size();

// Pixel density (devicePixelRatio) x1000, or 0 if unknown. Used so content is laid out in logical
// (CSS) pixels and rendered at the native device resolution instead of appearing tiny/upscaled.
__attribute__((import_module("sprt"), import_name("display_density"))) //
extern "C" uint32_t __sprt_host_display_density();

// Packed events from the JS canvas (see sprt-imports.mjs input_poll). 52 bytes, little-endian.
struct WasmHostInputEvent {
	uint32_t id;
	uint32_t name;
	uint32_t button;
	uint32_t modifiers;
	float x;
	float y;
	float valueX;
	float valueY;
	float density;
	uint32_t keycode;
	uint32_t compose;
	uint32_t keysym;
	uint32_t keychar;
};
static_assert(sizeof(WasmHostInputEvent) == 52, "sprt.input_poll packing");

__attribute__((import_module("sprt"), import_name("input_poll"))) //
extern "C" int32_t __sprt_host_input_poll(WasmHostInputEvent *out, uint32_t maxCount);

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

	// The canvas is the window. Native minExtent (e.g. form's 1280×760) would clamp the
	// rect above the backing store, so hit-testing and layout run in a different space
	// than pointer events. Fit the constraints to the canvas instead.
	info->rect.width = _extent.width;
	info->rect.height = _extent.height;
	if (info->minExtent.width > _extent.width) {
		info->minExtent.width = _extent.width;
	}
	if (info->minExtent.height > _extent.height) {
		info->minExtent.height = _extent.height;
	}

	// Lay content out in logical (CSS) pixels and render at the device resolution: the frame
	// constraints take their density from the window info.
	info->density = _density;

	if (!NativeWindow::init(c, sprt::move(info), caps)) {
		return false;
	}
	// The canvas is the only window: it starts enabled, focused and with the
	// pointer inside so :hover and keyboard work before the first JS WindowState.
	updateState(0, _info->state | WindowState::Enabled | WindowState::Focused | WindowState::Pointer);
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

void WasmWindow::pollHostInput() {
	uint32_t packed = __sprt_host_display_size();
	uint32_t w = packed >> 16, h = packed & 0xFFFF;
	if (w > 0 && h > 0 && (w != _extent.width || h != _extent.height)) {
		_extent = Extent2(w, h);
		uint32_t dens = __sprt_host_display_density();
		if (dens > 0) {
			_density = float(dens) / 1000.0f;
		}
		if (_info) {
			_info->rect.width = w;
			_info->rect.height = h;
			_info->density = _density;
			if (_info->minExtent.width > w) {
				_info->minExtent.width = w;
			}
			if (_info->minExtent.height > h) {
				_info->minExtent.height = h;
			}
		}
		if (_controller) {
			_controller->notifyWindowGeometryChanged(this);
			_controller->notifyWindowConstraintsChanged(this,
					UpdateConstraintsFlags::WindowResized);
		}
	}

	WasmHostInputEvent buf[32];
	const int32_t n = __sprt_host_input_poll(buf, 32);
	if (n <= 0) {
		return;
	}
	for (int32_t i = 0; i < n; i++) {
		const auto &h = buf[i];
		const auto name = InputEventName(h.name);
		if (name == InputEventName::WindowState) {
			updateState(h.id, WindowState(uint64_t(h.keycode) | (uint64_t(h.compose) << 32)));
			continue;
		}
		InputEventData ev;
		ev.id = h.id;
		ev.event = name;
		if (name == InputEventName::KeyPressed || name == InputEventName::KeyRepeated
				|| name == InputEventName::KeyReleased || name == InputEventName::KeyCanceled) {
			ev.input.modifiers = InputModifier(h.modifiers);
			ev.input.x = h.x;
			ev.input.y = h.y;
			ev.key.keycode = InputKeyCode(h.keycode);
			ev.key.compose = InputKeyComposeState(h.compose);
			ev.key.keysym = h.keysym;
			ev.key.keychar = char32_t(h.keychar);
		} else {
			ev.input.button = InputMouseButton(h.button);
			ev.input.modifiers = InputModifier(h.modifiers);
			ev.input.x = h.x;
			ev.input.y = h.y;
			ev.point.valueX = h.valueX;
			ev.point.valueY = h.valueY;
			ev.point.density = h.density > 0.f ? h.density : _density;
		}
		_pendingEvents.emplace_back(sprt::move(ev));
	}
}

void WasmWindow::dispatchPendingEvents() {
	pollHostInput();
	NativeWindow::dispatchPendingEvents();
}

// No OS IME: this window *is* the IME. Accept the request and enable the shared
// TextInputProcessor so it intercepts keys (same as xcb/headless). Returning false
// here rolls the request back and typing never reaches a focused field.
bool WasmWindow::updateTextInput(const TextInputRequest &, TextInputFlags) {
	if (_textInput) {
		_textInput->handleInputEnabled(true);
	}
	return true;
}

void WasmWindow::cancelTextInput() {
	if (_textInput) {
		_textInput->handleInputEnabled(false);
	}
}

} // namespace sprt::window

#endif
