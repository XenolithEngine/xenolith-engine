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

#include "XLLinuxWaylandDisplay.h"
#include "XLLinuxWaylandProtocol.h"
#include "XLLinuxWaylandDataDevice.h"
#include "XLLinuxWaylandWindow.h"
#include "XLLinuxWaylandKdeDisplayConfigManager.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

// clang-format off
static const xdg_wm_base_listener s_XdgWmBaseListener{
	.ping = [](void *data, xdg_wm_base *xdg_wm_base, uint32_t serial) {
		((WaylandDisplay *)data)->wayland->xdg_wm_base_pong(xdg_wm_base, serial);
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
			out->geometry = sp::move(geom);
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
				out->availableModes = sp::move(out->newModes);
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

static struct libdecor_interface s_libdecorInterface {
	.error = [](libdecor *context, enum libdecor_error error, const char *message) {
		switch (error) {
		case LIBDECOR_ERROR_COMPOSITOR_INCOMPATIBLE:
			log::source().error("WaylandDisplay", "LIBDECOR_ERROR_COMPOSITOR_INCOMPATIBLE: ", message);
			break;
		case LIBDECOR_ERROR_INVALID_FRAME_CONFIGURATION:
			log::source().error("WaylandDisplay", "LIBDECOR_ERROR_INVALID_FRAME_CONFIGURATION: ", message);
			break;
		}
	}
};

static const wl_registry_listener s_WaylandRegistryListener{
	.global = [](void *data, struct wl_registry *registry, uint32_t name, const char *interface,
			uint32_t version) {
		auto display = (WaylandDisplay *)data;
		auto wayland = display->wayland.get();

		StringView iname = StringView(interface);

		if (iname == StringView(wayland->wl_compositor_interface->name)) {
			display->compositor = static_cast<wl_compositor *>(
				wayland->wl_registry_bind(registry, name,
					wayland->wl_compositor_interface,
					std::min(version, uint32_t(wayland->wl_compositor_interface->version))));

			XL_WAYLAND_LOG("Init: '", interface, "', version: ",
				std::min(int(version), wayland->kde_output_management_v2_interface->version), ", name: ", name);
		} else if (iname == StringView(wayland->wl_subcompositor_interface->name)) {
			display->subcompositor = static_cast<wl_subcompositor *>(
				wayland->wl_registry_bind(registry, name,
					wayland->wl_subcompositor_interface,
					std::min(version, uint32_t(wayland->wl_subcompositor_interface->version))));

			XL_WAYLAND_LOG("Init: '", interface, "', version: ",
				std::min(int(version), wayland->kde_output_management_v2_interface->version), ", name: ", name);
		} else if (iname == StringView(wayland->wl_output_interface->name)) {
			auto out = Rc<WaylandOutput>::create(wayland, registry, name, version);
			display->outputs.emplace_back(move(out));

			XL_WAYLAND_LOG("Init: '", interface, "', version: ",
				std::min(int(version), wayland->kde_output_management_v2_interface->version), ", name: ", name);
		} else if (iname == StringView(wayland->wp_viewporter_interface->name)) {
			display->viewporter = static_cast<struct wp_viewporter *>(
					wayland->wl_registry_bind(registry, name,
						wayland->wp_viewporter_interface,
						std::min(version, uint32_t(wayland->wp_viewporter_interface->version))));

			XL_WAYLAND_LOG("Init: '", interface, "', version: ",
				std::min(int(version), wayland->kde_output_management_v2_interface->version), ", name: ", name);
		} else if (iname == StringView(wayland->xdg_wm_base_interface->name)) {
			display->xdgWmBase = static_cast<struct xdg_wm_base *>(
				wayland->wl_registry_bind(registry, name,
					wayland->xdg_wm_base_interface,
					std::min(int(version), wayland->xdg_wm_base_interface->version)));

			wayland->xdg_wm_base_add_listener(display->xdgWmBase, &s_XdgWmBaseListener, display);

			XL_WAYLAND_LOG("Init: '", interface, "', version: ",
				std::min(int(version), wayland->kde_output_management_v2_interface->version), ", name: ", name);
		} else if (iname == StringView(wayland->wl_shm_interface->name)) {
			display->shm = Rc<WaylandShm>::create(wayland, registry, name, version);

			XL_WAYLAND_LOG("Init: '", interface, "', version: ",
				std::min(int(version), wayland->kde_output_management_v2_interface->version), ", name: ", name);
		} else if (iname == StringView(wayland->wl_seat_interface->name)) {
			display->seat = Rc<WaylandSeat>::create(wayland, display, registry, name, version);

			XL_WAYLAND_LOG("Init: '", interface, "', version: ",
				std::min(int(version), wayland->kde_output_management_v2_interface->version), ", name: ", name);
		} else if (iname == StringView(wayland->zxdg_decoration_manager_v1_interface->name)) {
			display->decorationManager = static_cast<struct zxdg_decoration_manager_v1 *>(
				wayland->wl_registry_bind(registry, name,
					wayland->zxdg_decoration_manager_v1_interface,
					std::min(int(version), wayland->xdg_wm_base_interface->version)));

			XL_WAYLAND_LOG("Init: '", interface, "', version: ",
				std::min(int(version), wayland->kde_output_management_v2_interface->version), ", name: ", name);
		} else if (iname == StringView(wayland->kde_output_device_v2_interface->name)) {
			if (!display->kdeDisplayConfigManager) {
				display->kdeDisplayConfigManager = Rc<WaylandKdeDisplayConfigManager>::create(display);
			}

			auto output = static_cast<struct kde_output_device_v2 *>(
				wayland->wl_registry_bind(registry, name,
					wayland->kde_output_device_v2_interface,
					std::min(int(version), wayland->kde_output_device_v2_interface->version)));
			display->kdeDisplayConfigManager->addOutput(output, name);
			XL_WAYLAND_LOG("Init: '", interface, "', version: ",
				std::min(int(version), wayland->kde_output_management_v2_interface->version), ", name: ", name);
		} else if (iname == StringView(wayland->kde_output_order_v1_interface->name)) {
			if (!display->kdeDisplayConfigManager) {
				display->kdeDisplayConfigManager = Rc<WaylandKdeDisplayConfigManager>::create(display);
			}

			auto order = static_cast<struct kde_output_order_v1 *>(
				wayland->wl_registry_bind(registry, name,
					wayland->kde_output_order_v1_interface,
					std::min(int(version), wayland->kde_output_order_v1_interface->version)));

			display->kdeDisplayConfigManager->setOrder(order);
			XL_WAYLAND_LOG("Init: '", interface, "', version: ",
				std::min(int(version), wayland->kde_output_management_v2_interface->version), ", name: ", name);
		} else if (iname == StringView(wayland->kde_output_management_v2_interface->name)) {
			if (!display->kdeDisplayConfigManager) {
				display->kdeDisplayConfigManager = Rc<WaylandKdeDisplayConfigManager>::create(display);
			}

			auto manager = static_cast<struct kde_output_management_v2 *>(
				wayland->wl_registry_bind(registry, name,
					wayland->kde_output_management_v2_interface,
					std::min(int(version), wayland->kde_output_management_v2_interface->version)));
			display->kdeDisplayConfigManager->setManager(manager);
			XL_WAYLAND_LOG("Init: '", interface, "', version: ",
				std::min(int(version), wayland->kde_output_management_v2_interface->version), ", name: ", name);
		} else if (iname == StringView(wayland->wp_cursor_shape_manager_v1_interface->name)) {
			display->cursorManager = static_cast<struct wp_cursor_shape_manager_v1 *>(
				wayland->wl_registry_bind(registry, name,
					wayland->wp_cursor_shape_manager_v1_interface,
					1));
			XL_WAYLAND_LOG("Init: '", interface, "', version: ", 1, ", name: ", name);
		} else if (iname == StringView(wayland->wl_data_device_manager_interface->name)) {
			display->dataDeviceManager = Rc<WaylandDataDeviceManager>::create(display, registry, name, version);
			XL_WAYLAND_LOG("Init: '", interface, "', version: ",
				std::min(int(version), wayland->wl_data_device_manager_interface->version), ", name: ", name);
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
		wayland->zxdg_decoration_manager_v1_destroy(decorationManager);
		decorationManager = nullptr;
	}
	if (cursorManager) {
		wayland->wp_cursor_shape_manager_v1_destroy(cursorManager);
	}
	if (xdgWmBase) {
		wayland->xdg_wm_base_destroy(xdgWmBase);
		xdgWmBase = nullptr;
	}
	if (compositor) {
		wayland->wl_compositor_destroy(compositor);
		compositor = nullptr;
	}
	if (subcompositor) {
		wayland->wl_subcompositor_destroy(subcompositor);
		subcompositor = nullptr;
	}
	if (viewporter) {
		wayland->wp_viewporter_destroy(viewporter);
		viewporter = nullptr;
	}

	dataDeviceManager = nullptr;
	shm = nullptr;
	seat = nullptr;
	outputs.clear();

	if (display) {
		wayland->wl_display_disconnect(display);
		display = nullptr;
	}
}

bool WaylandDisplay::init(NotNull<WaylandLibrary> lib, NotNull<XkbLibrary> xkbLib, StringView d) {
	wayland = lib;

	display = wayland->wl_display_connect(
			d.empty() ? nullptr : (d.terminated() ? d.data() : d.str<Interface>().data()));
	if (!display) {
		log::source().error("WaylandDisplay", "Fail to connect to Wayland Display");
		return false;
	}

	if (wayland->hasDecor()) {
		decor = wayland->libdecor_new(display, &s_libdecorInterface);
	}

	struct wl_registry *registry = wayland->wl_display_get_registry(display);

	wayland->wl_registry_add_listener(registry, &s_WaylandRegistryListener, this);
	wayland->wl_display_roundtrip(display); // registry
	wayland->wl_display_roundtrip(display); // seats and outputs
	wayland->wl_registry_destroy(registry);

	xkb = xkbLib;
	return true;
}

Rc<DisplayConfigManager> WaylandDisplay::makeDisplayConfigManager(
		Function<void(NotNull<DisplayConfigManager>)> &&cb) {
	if (kdeDisplayConfigManager) {
		kdeDisplayConfigManager->setCallback(sp::move(cb));
	}
	return kdeDisplayConfigManager;
}

wl_surface *WaylandDisplay::createSurface(WaylandWindow *view) {
	auto surface = wayland->wl_compositor_create_surface(compositor);
	wayland->wl_surface_set_user_data(surface, view);
	wayland->wl_proxy_set_tag((struct wl_proxy *)surface, &s_XenolithWaylandTag);
	surfaces.emplace(surface);
	windows.emplace(view);
	return surface;
}

void WaylandDisplay::destroySurface(WaylandWindow *window) {
	seat->clearWindow(window);

	auto surface = window->getSurface();
	surfaces.erase(window->getSurface());
	windows.erase(window);
	wayland->wl_surface_destroy(surface);
}

wl_surface *WaylandDisplay::createDecorationSurface(WaylandDecoration *decor) {
	auto surface = wayland->wl_compositor_create_surface(compositor);
	wayland->wl_surface_set_user_data(surface, decor);
	wayland->wl_proxy_set_tag((struct wl_proxy *)surface, &s_XenolithWaylandTag);
	decorations.emplace(surface);
	return surface;
}

void WaylandDisplay::destroyDecorationSurface(wl_surface *surface) {
	decorations.erase(surface);
	wayland->wl_surface_destroy(surface);
}

bool WaylandDisplay::ownsSurface(wl_surface *surface) const {
	return surfaces.find(surface) != surfaces.end();
}

bool WaylandDisplay::isDecoration(wl_surface *surface) const {
	return decorations.find(surface) != decorations.end();
}

bool WaylandDisplay::flush() { return wayland->wl_display_flush(display) != -1; }

bool WaylandDisplay::poll() {
	if (seatDirty) {
		seat->update();
	}

	if (decor) {
		wayland->libdecor_dispatch(decor, 0);
	}

	while (wayland->wl_display_prepare_read(display) != 0) {
		wayland->wl_display_dispatch_pending(display);
	}

	wayland->wl_display_read_events(display);
	wayland->wl_display_dispatch_pending(display);

	for (auto &it : windows) { it->dispatchPendingEvents(); }

	flush();

	if (kdeDisplayConfigManager) {
		kdeDisplayConfigManager->done();
	}

	return true;
}

int WaylandDisplay::getFd() const { return wayland->wl_display_get_fd(display); }

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
		return seat->dataDevice->readFromClipboard(sp::move(req));
	}
	return Status::ErrorNotImplemented;
}

Status WaylandDisplay::probeClipboard(Rc<ClipboardProbe> &&probe) {
	if (seat->dataDevice) {
		return seat->dataDevice->probeClipboard(sp::move(probe));
	}
	return Status::ErrorNotImplemented;
}

Status WaylandDisplay::writeToClipboard(Rc<ClipboardData> &&data) {
	if (seat->dataDevice) {
		return seat->dataDevice->writeToClipboard(sp::move(data));
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

	if (wayland->hasDecor()) {
		caps |= WindowCapabilities::NativeDecorations;
	}

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
		wayland->wl_shm_destroy(shm);
		shm = nullptr;
	}
}

bool WaylandShm::init(const Rc<WaylandLibrary> &lib, wl_registry *registry, uint32_t name,
		uint32_t version) {
	wayland = lib;
	id = name;
	shm = static_cast<wl_shm *>(wayland->wl_registry_bind(registry, name, wayland->wl_shm_interface,
			std::min(version, uint32_t(wayland->wl_shm_interface->version))));
	wayland->wl_shm_set_user_data(shm, this);
	wayland->wl_shm_add_listener(shm, &s_WaylandShmListener, this);
	wayland->wl_proxy_set_tag((struct wl_proxy *)shm, &s_XenolithWaylandTag);
	return true;
}

WaylandOutput::~WaylandOutput() {
	if (output) {
		wayland->wl_output_destroy(output);
		output = nullptr;
	}
}

bool WaylandOutput::init(const Rc<WaylandLibrary> &lib, wl_registry *registry, uint32_t name,
		uint32_t version) {
	wayland = lib;
	id = name;
	output = static_cast<wl_output *>(
			wayland->wl_registry_bind(registry, name, wayland->wl_output_interface,
					std::min(version, uint32_t(wayland->wl_output_interface->version))));
	wayland->wl_output_set_user_data(output, this);
	wayland->wl_output_add_listener(output, &s_WaylandOutputListener, this);
	wayland->wl_proxy_set_tag((struct wl_proxy *)output, &s_XenolithWaylandTag);
	return true;
}

String WaylandOutput::description() const {
	StringStream stream;
	stream << geometry.make << " " << geometry.model << ": " << currentMode.size.width << "x"
		   << currentMode.size.height << "@" << currentMode.rate / 1'000 << "Hz (x" << scale
		   << ");";
	if (currentMode.flags & WL_OUTPUT_MODE_CURRENT) {
		stream << " Current;";
	}
	if (currentMode.flags & WL_OUTPUT_MODE_PREFERRED) {
		stream << " Preferred;";
	}
	if (!desc.empty()) {
		stream << " " << desc << ";";
	}
	return stream.str();
}

WaylandDecoration::~WaylandDecoration() {
	if (viewport) {
		wayland->wp_viewport_destroy(viewport);
		viewport = nullptr;
	}
	if (subsurface) {
		wayland->wl_subsurface_destroy(subsurface);
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

	subsurface = wayland->wl_subcompositor_get_subsurface(display->subcompositor, surface, parent);
	wayland->wl_subsurface_place_below(subsurface, parent);
	wayland->wl_subsurface_set_sync(subsurface);

	viewport = wayland->wp_viewporter_get_viewport(display->viewporter, surface);
	wayland->wl_surface_attach(surface, buffer->buffer, 0, 0);

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
			auto n = Time::now().toMicros();
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
		wayland->wl_surface_attach(surface, b->buffer, 0, 0);
		wayland->wl_surface_damage_buffer(surface, 0, 0, b->width, b->height);
		dirty = true;
	}
}

void WaylandDecoration::onLeave() {
	if (isTouchable() && isActive) {
		isActive = false;
		auto &b = (active && isActive) ? active : buffer;
		wayland->wl_surface_attach(surface, b->buffer, 0, 0);
		wayland->wl_surface_damage_buffer(surface, 0, 0, b->width, b->height);
		dirty = true;
	}
}

void WaylandDecoration::setActive(bool val) {
	if (!isTouchable()) {
		if (val != isActive) {
			isActive = val;
			auto &b = (active && isActive) ? active : buffer;
			wayland->wl_surface_attach(surface, b->buffer, 0, 0);
			wayland->wl_surface_damage_buffer(surface, 0, 0, b->width, b->height);
			dirty = true;
		}
	}
}

void WaylandDecoration::setVisible(bool val) {
	if (val != visible) {
		visible = val;
		if (visible) {
			auto &b = (active && isActive) ? active : buffer;
			wayland->wl_surface_attach(surface, b->buffer, 0, 0);
			wayland->wl_surface_damage_buffer(surface, 0, 0, b->width, b->height);
		} else {
			wayland->wl_surface_attach(surface, nullptr, 0, 0);
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
		wayland->wl_surface_attach(surface, b->buffer, 0, 0);
		wayland->wl_surface_damage_buffer(surface, 0, 0, b->width, b->height);
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

	wayland->wl_subsurface_set_position(subsurface, _x, _y);
	wayland->wp_viewport_set_destination(viewport, _width, _height);

	auto &b = (active && isActive) ? active : buffer;
	wayland->wl_surface_damage_buffer(surface, 0, 0, b->width, b->height);
	dirty = true;
}

bool WaylandDecoration::commit() {
	if (dirty) {
		wayland->wl_surface_commit(surface);
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

} // namespace stappler::xenolith::platform
