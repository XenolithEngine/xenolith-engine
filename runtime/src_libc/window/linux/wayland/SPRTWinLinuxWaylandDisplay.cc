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

#define __SPRT_BUILD 1

#include "private/window/linux/SPRTWinLinuxWaylandDisplay.h"
#include "private/window/linux/SPRTWinLinuxWaylandLibrary.h"
#include "private/window/linux/SPRTWinLinuxWaylandWindow.h"
#include "private/window/linux/SPRTWinLinuxWaylandSeat.h"
#include "private/window/linux/SPRTWinLinuxWaylandDataDevice.h"
#include "private/window/linux/SPRTWinLinuxWaylandKeys.h"
#include "private/window/linux/SPRTWinLinuxWaylandKdeDisplayConfigManager.h"

#include <sprt/runtime/log.h>

namespace sprt::window {

// clang-format off
static const xdg_wm_base_listener s_XdgWmBaseListener{
	.ping = [](void *data, xdg_wm_base *xdg_wm_base, uint32_t serial) {
		xdg_wm_base_pong(xdg_wm_base, serial);
	}
};

static const wl_output_listener s_WaylandOutputListener{
	.geometry = [](void *data, wl_output *wl_output, int32_t x, int32_t y, int32_t mmWidth, int32_t mmHeight,
			int32_t subpixel, char const *make, char const *model, int32_t transform) {

		auto out = reinterpret_cast<WaylandOutput *>(data);
		auto geom = WaylandOutput::Geometry{
			IVec2{x, y},
			Extent2(static_cast<uint32_t>(mmWidth), static_cast<uint32_t>(mmHeight)),
			subpixel,
			transform,
			make,
			model
		};

		if (geom != out->geometry) {
			out->geometry = sprt::move(geom);
			out->state = WaylandUpdateState::Updating;
		}

		if (out->name.empty()) {
			out->name = toString(make, " ", model);
		}
	},

	.mode = [](void *data, wl_output *output, uint32_t flags, int32_t width, int32_t height, int32_t refresh) {
		auto out = reinterpret_cast<WaylandOutput *>(data);

		auto mode = WaylandOutput::Mode{
			Extent2(static_cast<uint32_t>(width), static_cast<uint32_t>(height)),
			static_cast<uint32_t>(refresh),
			flags
		};

		out->newModes.emplace_back(mode);
		out->state = WaylandUpdateState::Updating;
	},

	.done = [](void *data, wl_output *output) {
		auto out = reinterpret_cast<WaylandOutput *>(data);
		if (out->state == WaylandUpdateState::Updating) {
			if (!out->newModes.empty()) {
				out->availableModes = sprt::move(out->newModes);
				out->newModes.clear();

				out->currentMode = out->availableModes.front();
				out->preferredMode = out->availableModes.front();
			}

			for (auto &it : out->availableModes) {
				if (hasFlag(it.flags, uint32_t(WL_OUTPUT_MODE_CURRENT))) {
					out->currentMode = it;
				}
				if (hasFlag(it.flags, uint32_t(WL_OUTPUT_MODE_PREFERRED))) {
					out->preferredMode = it;
				}
			}

			out->state = WaylandUpdateState::Done;
		}
	},

	.scale = [](void *data, wl_output *output, int32_t factor) {
		auto out = reinterpret_cast<WaylandOutput *>(data);
		if (out->scale != static_cast<float>(factor)) {
			out->scale = static_cast<float>(factor);
			out->state = WaylandUpdateState::Updating;
		}
	},

	.name = [](void *data, struct wl_output *wl_output, const char *name) {
		auto out = reinterpret_cast<WaylandOutput *>(data);
		out->name = name;
	},

	.description = [](void *data, struct wl_output *wl_output, const char *name) {
		auto out = reinterpret_cast<WaylandOutput *>(data);
		out->desc = name;
	}
};

static struct wl_shm_listener s_WaylandShmListener = {
	.format = [](void *data, wl_shm *shm, uint32_t format) {
		reinterpret_cast<WaylandShm *>(data)->format = format;
	}
};

static const wl_registry_listener s_WaylandRegistryListener{
	.global = [](void *data, struct wl_registry *registry, uint32_t name, const char *interface,
			uint32_t version) {
		auto display = (WaylandDisplay *)data;
		auto wayland = display->wayland.get();

		StringView iname = StringView(interface);

		if (iname == StringView(wl_compositor_interface.name)) {
			auto availableVersion = sprt::min(version, uint32_t(wl_compositor_interface.version));
			auto targetVersion = sprt::min(WaylandLibrary::wl_compositor_version_supported, availableVersion);

			display->compositor = static_cast<wl_compositor *>(
				wl_registry_bind(registry, name, &wl_compositor_interface,
					targetVersion));

			XL_WAYLAND_LOG("Init: '", interface, "', version: ", targetVersion, "(", availableVersion, "), name: ", name);

		} else if (iname == StringView(wl_subcompositor_interface.name)) {
			auto availableVersion = sprt::min(version, uint32_t(wl_subcompositor_interface.version));
			auto targetVersion = sprt::min(WaylandLibrary::wl_subcompositor_version_supported, availableVersion);

			display->subcompositor = static_cast<wl_subcompositor *>(
				wl_registry_bind(registry, name, &wl_subcompositor_interface,
					targetVersion));

			XL_WAYLAND_LOG("Init: '", interface, "', version: ", targetVersion, "(", availableVersion, "), name: ", name);

		} else if (iname == StringView(wl_output_interface.name)) {
			auto availableVersion = sprt::min(version, uint32_t(wl_output_interface.version));
			auto targetVersion = sprt::min(WaylandLibrary::wl_output_version_supported, availableVersion);

			auto out = Rc<WaylandOutput>::create(wayland, registry, name, targetVersion);
			display->outputs.emplace_back(move(out));

			XL_WAYLAND_LOG("Init: '", interface, "', version: ", targetVersion, "(", availableVersion, "), name: ", name);

		} else if (iname == StringView(wp_viewporter_interface.name)) {
			auto availableVersion = sprt::min(version, uint32_t(wp_viewporter_interface.version));
			auto targetVersion = sprt::min(WaylandLibrary::wp_viewporter_version_supported, availableVersion);

			display->viewporter = static_cast<struct wp_viewporter *>(
					wl_registry_bind(registry, name, &wp_viewporter_interface,
						targetVersion));

			XL_WAYLAND_LOG("Init: '", interface, "', version: ", targetVersion, "(", availableVersion, "), name: ", name);

		} else if (iname == StringView(xdg_wm_base_interface.name)) {
			auto availableVersion = sprt::min(version, uint32_t(xdg_wm_base_interface.version));
			auto targetVersion = sprt::min(WaylandLibrary::xdg_wm_base_version_supported, availableVersion);

			display->xdgWmBase = static_cast<struct xdg_wm_base *>(
				wl_registry_bind(registry, name, &xdg_wm_base_interface,
					targetVersion));

			xdg_wm_base_add_listener(display->xdgWmBase, &s_XdgWmBaseListener, display);

			XL_WAYLAND_LOG("Init: '", interface, "', version: ", targetVersion, "(", availableVersion, "), name: ", name);

		} else if (iname == StringView(wl_shm_interface.name)) {
			auto availableVersion = sprt::min(version, uint32_t(wl_shm_interface.version));
			auto targetVersion = sprt::min(WaylandLibrary::wl_shm_version_supported, availableVersion);

			display->shm = Rc<WaylandShm>::create(wayland, registry, name, targetVersion);

			XL_WAYLAND_LOG("Init: '", interface, "', version: ", targetVersion, "(", availableVersion, "), name: ", name);

		} else if (iname == StringView(wl_seat_interface.name)) {
			auto availableVersion = sprt::min(version, uint32_t(wl_seat_interface.version));
			auto targetVersion = sprt::min(WaylandLibrary::wl_seat_version_supported, availableVersion);

			display->seat = Rc<WaylandSeat>::create(wayland, display, registry, name, targetVersion);

			XL_WAYLAND_LOG("Init: '", interface, "', version: ", targetVersion, "(", availableVersion, "), name: ", name);

		} else if (iname == StringView(zxdg_decoration_manager_v1_interface.name)) {
			auto availableVersion = sprt::min(version, uint32_t(zxdg_decoration_manager_v1_interface.version));
			auto targetVersion = sprt::min(WaylandLibrary::zxdg_decoration_manager_v1_version_supported, availableVersion);

			display->decorationManager = static_cast<struct zxdg_decoration_manager_v1 *>(
				wl_registry_bind(registry, name, &zxdg_decoration_manager_v1_interface,
					targetVersion));

			XL_WAYLAND_LOG("Init: '", interface, "', version: ", targetVersion, "(", availableVersion, "), name: ", name);

		} else if (iname == StringView(kde_output_device_v2_interface.name)) {
			auto availableVersion = sprt::min(version, uint32_t(kde_output_device_v2_interface.version));
			auto targetVersion = sprt::min(WaylandLibrary::kde_output_device_v2_version_supported, availableVersion);

			if (!display->kdeDisplayConfigManager) {
				display->kdeDisplayConfigManager = Rc<WaylandKdeDisplayConfigManager>::create(display);
			}

			auto output = static_cast<struct kde_output_device_v2 *>(
				wl_registry_bind(registry, name, &kde_output_device_v2_interface,
					targetVersion));

			display->kdeDisplayConfigManager->addOutput(output, name);
			XL_WAYLAND_LOG("Init: '", interface, "', version: ", targetVersion, "(", availableVersion, "), name: ", name);

		} else if (iname == StringView(kde_output_order_v1_interface.name)) {
			auto availableVersion = sprt::min(version, uint32_t(kde_output_order_v1_interface.version));
			auto targetVersion = sprt::min(WaylandLibrary::kde_output_order_v1_version_supported, availableVersion);

			if (!display->kdeDisplayConfigManager) {
				display->kdeDisplayConfigManager = Rc<WaylandKdeDisplayConfigManager>::create(display);
			}

			auto order = static_cast<struct kde_output_order_v1 *>(
				wl_registry_bind(registry, name, &kde_output_order_v1_interface,
					targetVersion));

			display->kdeDisplayConfigManager->setOrder(order);
			XL_WAYLAND_LOG("Init: '", interface, "', version: ", targetVersion, "(", availableVersion, "), name: ", name);

		} else if (iname == StringView(kde_output_management_v2_interface.name)) {
			auto availableVersion = sprt::min(version, uint32_t(kde_output_management_v2_interface.version));
			auto targetVersion = sprt::min(WaylandLibrary::kde_output_management_v2_version_supported, availableVersion);

			if (!display->kdeDisplayConfigManager) {
				display->kdeDisplayConfigManager = Rc<WaylandKdeDisplayConfigManager>::create(display);
			}

			auto manager = static_cast<struct kde_output_management_v2 *>(
				wl_registry_bind(registry, name, &kde_output_management_v2_interface,
					targetVersion));

			display->kdeDisplayConfigManager->setManager(manager);
			XL_WAYLAND_LOG("Init: '", interface, "', version: ", targetVersion, "(", availableVersion, "), name: ", name);

		} else if (iname == StringView(wp_cursor_shape_manager_v1_interface.name)) {
			auto availableVersion = sprt::min(version, uint32_t(wp_cursor_shape_manager_v1_interface.version));
			auto targetVersion = sprt::min(WaylandLibrary::wp_cursor_shape_manager_v1_version_supported, availableVersion);

			display->cursorManager = static_cast<struct wp_cursor_shape_manager_v1 *>(
				wl_registry_bind(registry, name, &wp_cursor_shape_manager_v1_interface,
					targetVersion));

			XL_WAYLAND_LOG("Init: '", interface, "', version: ", targetVersion, "(", availableVersion, "), name: ", name);

		} else if (iname == StringView(wl_data_device_manager_interface.name)) {
			auto availableVersion = sprt::min(version, uint32_t(wl_data_device_manager_interface.version));
			auto targetVersion = sprt::min(WaylandLibrary::wl_data_device_manager_version_supported, availableVersion);

			display->dataDeviceManager = Rc<WaylandDataDeviceManager>::create(display, registry, name, targetVersion);
			XL_WAYLAND_LOG("Init: '", interface, "', version: ", targetVersion, "(", availableVersion, "), name: ", name);

		} else {
			XL_WAYLAND_LOG("Unknown registry interface: '", interface, "', version: ", version, ", name: ", name);
		}
	},
	.global_remove = [](void *data, struct wl_registry *registry, uint32_t name) {
		XL_WAYLAND_LOG("Registry remove: ", name);
		auto display = (WaylandDisplay *)data;
		if (display->kdeDisplayConfigManager) {
			display->kdeDisplayConfigManager->removeOutput(name);
		}
	}
};

// clang-format on

WaylandDisplay::~WaylandDisplay() {
	if (decorationManager) {
		zxdg_decoration_manager_v1_destroy(decorationManager);
		decorationManager = nullptr;
	}
	if (cursorManager) {
		wp_cursor_shape_manager_v1_destroy(cursorManager);
	}
	if (xdgWmBase) {
		xdg_wm_base_destroy(xdgWmBase);
		xdgWmBase = nullptr;
	}
	if (compositor) {
		wl_compositor_destroy(compositor);
		compositor = nullptr;
	}
	if (subcompositor) {
		wl_subcompositor_destroy(subcompositor);
		subcompositor = nullptr;
	}
	if (viewporter) {
		wp_viewporter_destroy(viewporter);
		viewporter = nullptr;
	}

	dataDeviceManager = nullptr;
	shm = nullptr;
	seat = nullptr;
	outputs.clear();

	if (display) {
		wl_display_disconnect(display);
		display = nullptr;
	}
}

bool WaylandDisplay::init(NotNull<WaylandLibrary> lib, NotNull<XkbLibrary> xkbLib, StringView d) {
	wayland = lib;

	display = wl_display_connect(
			d.empty() ? nullptr : (d.terminated() ? d.data() : d.str<String>().data()));
	if (!display) {
		oslog::vperror(__SPRT_LOCATION, "WaylandDisplay", "Fail to connect to Wayland Display");
		return false;
	}

	struct wl_registry *registry = wl_display_get_registry(display);

	wl_registry_add_listener(registry, &s_WaylandRegistryListener, this);
	wl_display_roundtrip(display); // registry
	wl_display_roundtrip(display); // seats and outputs
	wl_registry_destroy(registry);

	xkb = xkbLib;
	return true;
}

Rc<DisplayConfigManager> WaylandDisplay::makeDisplayConfigManager(
		Function<void(NotNull<DisplayConfigManager>)> &&cb) {
	if (kdeDisplayConfigManager) {
		kdeDisplayConfigManager->setCallback(sprt::move(cb));
	}
	return kdeDisplayConfigManager;
}

wl_surface *WaylandDisplay::createSurface(WaylandWindow *view) {
	auto surface = wl_compositor_create_surface(compositor);
	wl_surface_set_user_data(surface, view);
	wl_proxy_set_tag((struct wl_proxy *)surface, &s_XenolithWaylandTag);
	surfaces.emplace(surface);
	windows.emplace(view);
	return surface;
}

void WaylandDisplay::destroySurface(WaylandWindow *window) {
	seat->clearWindow(window);

	auto surface = window->getSurface();
	surfaces.erase(window->getSurface());
	windows.erase(window);
	wl_surface_destroy(surface);
}

wl_surface *WaylandDisplay::createDecorationSurface(WaylandDecoration *decor) {
	auto surface = wl_compositor_create_surface(compositor);
	wl_surface_set_user_data(surface, decor);
	wl_proxy_set_tag((struct wl_proxy *)surface, &s_XenolithWaylandTag);
	decorations.emplace(surface);
	return surface;
}

void WaylandDisplay::destroyDecorationSurface(wl_surface *surface) {
	decorations.erase(surface);
	wl_surface_destroy(surface);
}

bool WaylandDisplay::ownsSurface(wl_surface *surface) const {
	return surfaces.find(surface) != surfaces.end();
}

bool WaylandDisplay::isDecoration(wl_surface *surface) const {
	return decorations.find(surface) != decorations.end();
}

bool WaylandDisplay::flush() { return wl_display_flush(display) != -1; }

bool WaylandDisplay::poll() {
	if (seatDirty) {
		seat->update();
	}

	auto rv = wl_display_prepare_read(display);
	while (rv != 0) {
		wl_display_dispatch_pending(display);
		rv = wl_display_prepare_read(display);
	}

	wl_display_read_events(display);
	wl_display_dispatch_pending(display);

	for (auto &it : windows) { it->dispatchPendingEvents(); }

	flush();

	if (kdeDisplayConfigManager) {
		kdeDisplayConfigManager->done();
	}

	return true;
}

int WaylandDisplay::getFd() const { return wl_display_get_fd(display); }

void WaylandDisplay::updateThemeInfo(const ThemeInfo &theme) {
	if (seat) {
		if ((seat->capabilities & WL_SEAT_CAPABILITY_POINTER) != 0) {
			seat->setCursors(theme.systemTheme, theme.cursorSize);
			seat->doubleClickInterval = theme.doubleClickInterval;
		}
	}
	for (auto &it : windows) { it->motifyThemeChanged(theme); }
}

void WaylandDisplay::notifyScreenChange() {
	for (auto &it : windows) { it->notifyScreenChange(); }
}

Status WaylandDisplay::readFromClipboard(Rc<ClipboardRequest> &&req) {
	if (seat->dataDevice) {
		return seat->dataDevice->readFromClipboard(sprt::move(req));
	}
	return Status::ErrorNotImplemented;
}

Status WaylandDisplay::probeClipboard(Rc<ClipboardProbe> &&probe) {
	if (seat->dataDevice) {
		return seat->dataDevice->probeClipboard(sprt::move(probe));
	}
	return Status::ErrorNotImplemented;
}

Status WaylandDisplay::writeToClipboard(Rc<ClipboardData> &&data) {
	if (seat->dataDevice) {
		return seat->dataDevice->writeToClipboard(sprt::move(data));
	}
	return Status::ErrorNotImplemented;
}

bool WaylandDisplay::isCursorSupported(WindowCursor cursor, bool serverSide) const {
	if (serverSide) {
		if (getWaylandCursor(cursor)) {
			return true;
		}
		return false;
	} else {
		return seat->cursorTheme && seat->cursorTheme->hasCursor(cursor);
	}
}

WindowCapabilities WaylandDisplay::getCapabilities() const {
	auto caps = WindowCapabilities::Fullscreen | WindowCapabilities::FullscreenWithMode
			| WindowCapabilities::UserSpaceDecorations | WindowCapabilities::CloseGuard;

	if (decorationManager) {
		caps |= WindowCapabilities::ServerSideDecorations;
	}

	if (seat->cursorShape) {
		caps |= WindowCapabilities::ServerSideCursors;
	}

	return caps;
}

void WaylandDisplay::handleClipboardChanged() {
	if (systemNotification) {
		systemNotification(SystemNotification::ClipboardChanged);
	}
}

WaylandShm::~WaylandShm() {
	if (shm) {
		wl_shm_destroy(shm);
		shm = nullptr;
	}
}

bool WaylandShm::init(const Rc<WaylandLibrary> &lib, wl_registry *registry, uint32_t name,
		uint32_t version) {
	wayland = lib;
	id = name;
	shm = static_cast<wl_shm *>(wl_registry_bind(registry, name, &wl_shm_interface, version));
	wl_shm_set_user_data(shm, this);
	wl_shm_add_listener(shm, &s_WaylandShmListener, this);
	wl_proxy_set_tag((struct wl_proxy *)shm, &s_XenolithWaylandTag);
	return true;
}

WaylandOutput::~WaylandOutput() {
	if (output) {
		wl_output_destroy(output);
		output = nullptr;
	}
}

bool WaylandOutput::init(const Rc<WaylandLibrary> &lib, wl_registry *registry, uint32_t name,
		uint32_t version) {
	wayland = lib;
	id = name;
	output = static_cast<wl_output *>(
			wl_registry_bind(registry, name, &wl_output_interface, version));
	wl_output_set_user_data(output, this);
	wl_output_add_listener(output, &s_WaylandOutputListener, this);
	wl_proxy_set_tag((struct wl_proxy *)output, &s_XenolithWaylandTag);
	return true;
}

String WaylandOutput::description() const {
	return toString(geometry.make, " ", geometry.model, ": ", currentMode.size.width, "x",
			currentMode.size.height, "@", currentMode.rate / 1'000, "Hz (x", scale, ");",
			(currentMode.flags & WL_OUTPUT_MODE_CURRENT) ? " Current; " : "",
			(currentMode.flags & WL_OUTPUT_MODE_PREFERRED) ? " Preferred; " : "",
			!desc.empty() ? StringView(desc) : StringView());
}

WaylandDecoration::~WaylandDecoration() {
	if (viewport) {
		wp_viewport_destroy(viewport);
		viewport = nullptr;
	}
	if (subsurface) {
		wl_subsurface_destroy(subsurface);
		subsurface = nullptr;
	}
	if (surface) {
		display->destroyDecorationSurface(surface);
		surface = nullptr;
	}
}

bool WaylandDecoration::init(WaylandWindow *view, Rc<WaylandBuffer> &&b, Rc<WaylandBuffer> &&a,
		WaylandDecorationName n) {
	root = view;
	display = view->getDisplay();
	wayland = display->wayland;
	surface = display->createDecorationSurface(this);
	name = n;
	switch (n) {
	case WaylandDecorationName::RightSide: image = WindowCursor::ResizeRight; break;
	case WaylandDecorationName::TopRightCorner: image = WindowCursor::ResizeTopRight; break;
	case WaylandDecorationName::TopSide: image = WindowCursor::ResizeTop; break;
	case WaylandDecorationName::TopLeftCorner: image = WindowCursor::ResizeTopLeft; break;
	case WaylandDecorationName::BottomRightCorner: image = WindowCursor::ResizeBottomRight; break;
	case WaylandDecorationName::BottomSide: image = WindowCursor::ResizeBottom; break;
	case WaylandDecorationName::BottomLeftCorner: image = WindowCursor::ResizeBottomLeft; break;
	case WaylandDecorationName::LeftSide: image = WindowCursor::ResizeLeft; break;
	default: image = WindowCursor::Default; break;
	}
	buffer = move(b);
	if (a) {
		active = move(a);
	}

	auto parent = root->getSurface();

	subsurface = wl_subcompositor_get_subsurface(display->subcompositor, surface, parent);
	wl_subsurface_place_below(subsurface, parent);
	wl_subsurface_set_sync(subsurface);

	viewport = wp_viewporter_get_viewport(display->viewporter, surface);
	wl_surface_attach(surface, buffer->buffer, 0, 0);

	dirty = true;

	return true;
}

bool WaylandDecoration::init(WaylandWindow *view, Rc<WaylandBuffer> &&b, WaylandDecorationName n) {
	return init(view, move(b), nullptr, n);
}

void WaylandDecoration::setAltBuffers(Rc<WaylandBuffer> &&b, Rc<WaylandBuffer> &&a) {
	altBuffer = move(b);
	altActive = move(a);
}

void WaylandDecoration::handlePress(WaylandSeat *seat, uint32_t s, uint32_t button,
		uint32_t state) {
	serial = s;
	waitForMove = false;
	if (isTouchable()) {
		if (state == WL_POINTER_BUTTON_STATE_RELEASED && button == BTN_LEFT) {
			root->handleDecorationPress(this, serial, button);
		}
	} else if (image == WindowCursor::Default) {
		if (state == WL_POINTER_BUTTON_STATE_PRESSED && button == BTN_RIGHT) {
			root->handleDecorationPress(this, serial, button, false);
		} else if (state == WL_POINTER_BUTTON_STATE_RELEASED && button == BTN_LEFT) {
			auto n = platform::clock(platform::ClockType::Realtime);
			if (n - lastTouch < seat->doubleClickInterval) {
				root->handleDecorationPress(this, serial, button, true);
			}
			lastTouch = n;
		} else if (state == WL_POINTER_BUTTON_STATE_PRESSED && button == BTN_LEFT) {
			waitForMove = true;
			//root->handleDecorationPress(this, serial, button);
		}
	} else {
		if (state == WL_POINTER_BUTTON_STATE_PRESSED && button == BTN_LEFT) {
			root->handleDecorationPress(this, serial, button);
		}
	}
}

void WaylandDecoration::handleMotion(wl_fixed_t x, wl_fixed_t y) {
	pointerX = x;
	pointerY = y;
	if (waitForMove) {
		root->handleDecorationPress(this, serial, 0);
	}
}

void WaylandDecoration::onEnter() {
	if (isTouchable() && !isActive) {
		isActive = true;
		auto &b = (active && isActive) ? active : buffer;
		wl_surface_attach(surface, b->buffer, 0, 0);
		wl_surface_damage_buffer(surface, 0, 0, b->width, b->height);
		dirty = true;
	}
}

void WaylandDecoration::onLeave() {
	if (isTouchable() && isActive) {
		isActive = false;
		auto &b = (active && isActive) ? active : buffer;
		wl_surface_attach(surface, b->buffer, 0, 0);
		wl_surface_damage_buffer(surface, 0, 0, b->width, b->height);
		dirty = true;
	}
}

void WaylandDecoration::setActive(bool val) {
	if (!isTouchable()) {
		if (val != isActive) {
			isActive = val;
			auto &b = (active && isActive) ? active : buffer;
			wl_surface_attach(surface, b->buffer, 0, 0);
			wl_surface_damage_buffer(surface, 0, 0, b->width, b->height);
			dirty = true;
		}
	}
}

void WaylandDecoration::setVisible(bool val) {
	if (val != visible) {
		visible = val;
		if (visible) {
			auto &b = (active && isActive) ? active : buffer;
			wl_surface_attach(surface, b->buffer, 0, 0);
			wl_surface_damage_buffer(surface, 0, 0, b->width, b->height);
		} else {
			wl_surface_attach(surface, nullptr, 0, 0);
		}
		dirty = true;
	}
}

void WaylandDecoration::setAlternative(bool val) {
	if (!altBuffer || !altActive) {
		return;
	}

	if (alternative != val) {
		alternative = val;
		auto tmpA = move(altBuffer);
		auto tmpB = move(altActive);

		altBuffer = move(buffer);
		altActive = move(active);

		buffer = move(tmpA);
		active = move(tmpB);

		auto &b = (active && isActive) ? active : buffer;
		wl_surface_attach(surface, b->buffer, 0, 0);
		wl_surface_damage_buffer(surface, 0, 0, b->width, b->height);
		dirty = true;
	}
}

void WaylandDecoration::setGeometry(int32_t x, int32_t y, int32_t width, int32_t height) {
	if (_x == x && _y == y && _width == width && _height == height) {
		return;
	}

	_x = x;
	_y = y;
	_width = width;
	_height = height;

	wl_subsurface_set_position(subsurface, _x, _y);
	wp_viewport_set_destination(viewport, _width, _height);

	auto &b = (active && isActive) ? active : buffer;
	wl_surface_damage_buffer(surface, 0, 0, b->width, b->height);
	dirty = true;
}

bool WaylandDecoration::commit() {
	if (dirty) {
		wl_surface_commit(surface);
		dirty = false;
		return true;
	}
	return false;
}

bool WaylandDecoration::isTouchable() const {
	switch (name) {
	case WaylandDecorationName::IconClose:
	case WaylandDecorationName::IconMaximize:
	case WaylandDecorationName::IconMinimize:
	case WaylandDecorationName::IconRestore: return true; break;
	default: break;
	}
	return false;
}

void WaylandDecoration::setLightTheme() {
	switch (name) {
	case WaylandDecorationName::HeaderLeft:
	case WaylandDecorationName::HeaderRight:
	case WaylandDecorationName::HeaderBottom:
	case WaylandDecorationName::HeaderCenter: setAlternative(false); break;
	default: break;
	}
}

void WaylandDecoration::setDarkTheme() {
	switch (name) {
	case WaylandDecorationName::HeaderLeft:
	case WaylandDecorationName::HeaderRight:
	case WaylandDecorationName::HeaderBottom:
	case WaylandDecorationName::HeaderCenter: setAlternative(true); break;
	default: break;
	}
}

} // namespace sprt::window
