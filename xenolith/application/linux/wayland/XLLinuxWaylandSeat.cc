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

#include "XLLinuxWaylandSeat.h"
#include "XLLinuxWaylandDisplay.h"
#include "XLLinuxWaylandWindow.h"

#include "SPSharedModule.h"

#if MODULE_XENOLITH_FONT
#include "XLFontLocale.h"
#endif

#include <sys/mman.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

// clang-format off

static struct wl_pointer_listener s_WaylandPointerListener{
	.enter = [](void *data, wl_pointer *wl_pointer, uint32_t serial, wl_surface *surface, wl_fixed_t x, wl_fixed_t y) {
		auto seat = reinterpret_cast<WaylandSeat *>(data);
		seat->pointerFocus = surface;
		seat->serial = serial;

		if (seat->root->isDecoration(surface)) {
			if (auto decor =
							(WaylandDecoration *)seat->wayland->wl_surface_get_user_data(surface)) {
				if (decor->image != seat->cursorImage) {
					seat->setCursor(decor->image, true);
				}
				seat->pointerDecorations.emplace(decor);
				decor->onEnter();
			}
			return;
		}

		if (surface != seat->cursorSurface && seat->wayland->ownsProxy(surface)) {
			auto window = reinterpret_cast<WaylandWindow *>(seat->wayland->wl_surface_get_user_data(surface));
			if (window) {
				seat->pointerViews.emplace(window);
				if (window->getCursor() != seat->cursorImage) {
					seat->setCursor(window->getCursor(), window->isServerSideCursors());
				}
				window->handlePointerEnter(x, y);
			}
		}
	},

	.leave = [](void *data, wl_pointer *wl_pointer, uint32_t serial, wl_surface *surface) {
		auto seat = reinterpret_cast<WaylandSeat *>(data);

		if (seat->root->isDecoration(surface)) {
			if (auto decor = (WaylandDecoration *)seat->wayland->wl_surface_get_user_data(surface)) {
				decor->waitForMove = false;
				seat->pointerDecorations.erase(decor);
				decor->onLeave();
			}
		} else if (seat->root->ownsSurface(surface)) {
			auto window = reinterpret_cast<WaylandWindow *>(seat->wayland->wl_surface_get_user_data(surface));
			if (window) {
				window->handlePointerLeave();
				seat->pointerViews.erase(window);
			}
		}

		if (seat->pointerFocus == surface) {
			seat->pointerFocus = NULL;
			seat->cursorImage = WindowCursor::Undefined;
		}
	},

	.motion = [](void *data, wl_pointer *wl_pointer, uint32_t time, wl_fixed_t x, wl_fixed_t y) {
		auto seat = reinterpret_cast<WaylandSeat *>(data);
		for (auto &it : seat->pointerViews) { it->handlePointerMotion(time, x, y); }
		for (auto &it : seat->pointerDecorations) { it->handleMotion(x, y); }
	},

	.button = [](void *data, wl_pointer *wl_pointer, uint32_t serial, uint32_t time, uint32_t button, uint32_t state) {
		auto seat = reinterpret_cast<WaylandSeat *>(data);
		for (auto &it : seat->pointerViews) { it->handlePointerButton(serial, time, button, state); }
		for (auto &it : seat->pointerDecorations) { it->handlePress(seat, serial, button, state); }
	},

	.axis = [](void *data, wl_pointer *wl_pointer, uint32_t time, uint32_t axis, wl_fixed_t value) {
		auto seat = reinterpret_cast<WaylandSeat *>(data);
		for (auto &it : seat->pointerViews) { it->handlePointerAxis(time, axis, wl_fixed_to_double(value)); }
	},

	.frame = [](void *data, wl_pointer *wl_pointer) {
		auto seat = reinterpret_cast<WaylandSeat *>(data);
		for (auto &it : seat->pointerViews) { it->handlePointerFrame(); }
	},

	.axis_source = [](void *data, wl_pointer *wl_pointer, uint32_t axis_source) {
		auto seat = reinterpret_cast<WaylandSeat *>(data);
		for (auto &it : seat->pointerViews) { it->handlePointerAxisSource(axis_source); }
	},

	.axis_stop = [](void *data, wl_pointer *wl_pointer, uint32_t time, uint32_t axis) {
		auto seat = reinterpret_cast<WaylandSeat *>(data);
		for (auto &it : seat->pointerViews) { it->handlePointerAxisStop(time, axis); }
	},

	.axis_discrete = [](void *data, wl_pointer *wl_pointer, uint32_t axis, int32_t discrete) {
		auto seat = reinterpret_cast<WaylandSeat *>(data);
		for (auto &it : seat->pointerViews) { it->handlePointerAxisDiscrete(axis, discrete * 120); }
	},

	.axis_value120 = [](void *data, wl_pointer *wl_pointer, uint32_t axis, int32_t value120) {
		auto seat = reinterpret_cast<WaylandSeat *>(data);
		for (auto &it : seat->pointerViews) { it->handlePointerAxisDiscrete(axis, value120); }
	},

	.axis_relative_direction = [](void *data, struct wl_pointer *wl_pointer, uint32_t axis, uint32_t direction) {
		auto seat = reinterpret_cast<WaylandSeat *>(data);
		for (auto &it : seat->pointerViews) { it->handlePointerAxisRelativeDirection(axis, direction); }
	}
};

static struct wl_keyboard_listener s_WaylandKeyboardListener{// keymap
	.keymap = [](void *data, wl_keyboard *wl_keyboard, uint32_t format, int32_t fd, uint32_t size) {
		auto seat = reinterpret_cast<WaylandSeat *>(data);
		if (seat->root->xkb) {
			auto map_shm = ::mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
			if (map_shm != MAP_FAILED) {
				if (seat->state) {
					seat->root->xkb->xkb_state_unref(seat->state);
					seat->state = nullptr;
				}

				if (seat->compose) {
					seat->root->xkb->xkb_compose_state_unref(seat->compose);
					seat->compose = nullptr;
				}

				auto keymap = seat->root->xkb->xkb_keymap_new_from_string(seat->root->xkb->getContext(),
						(const char *)map_shm, xkb_keymap_format(format), XKB_KEYMAP_COMPILE_NO_FLAGS);
				if (keymap) {
					seat->state = seat->root->xkb->xkb_state_new(keymap);
					seat->keyState.controlIndex =
							seat->root->xkb->xkb_keymap_mod_get_index(keymap, "Control");
					seat->keyState.altIndex = seat->root->xkb->xkb_keymap_mod_get_index(keymap, "Mod1");
					seat->keyState.shiftIndex =
							seat->root->xkb->xkb_keymap_mod_get_index(keymap, "Shift");
					seat->keyState.superIndex =
							seat->root->xkb->xkb_keymap_mod_get_index(keymap, "Mod4");
					seat->keyState.capsLockIndex =
							seat->root->xkb->xkb_keymap_mod_get_index(keymap, "Lock");
					seat->keyState.numLockIndex =
							seat->root->xkb->xkb_keymap_mod_get_index(keymap, "Mod2");
					seat->root->xkb->xkb_keymap_unref(keymap);
				}

				String posixLocale;

#if MODULE_XENOLITH_FONT
				auto getLocaleInfo = SharedModule::acquireTypedSymbol<decltype(&locale::getLocaleInfo)>(
						buildconfig::MODULE_XENOLITH_FONT_NAME, "locale::getLocaleInfo");
				if (getLocaleInfo) {
					auto info = getLocaleInfo();
					posixLocale = info.id.getPosixName<Interface>();
				}
#endif
				if (posixLocale.empty()) {
					posixLocale = stappler::platform::getOsLocale().str<Interface>();
				}

				auto composeTable = seat->root->xkb->xkb_compose_table_new_from_locale(
						seat->root->xkb->getContext(), posixLocale.empty() ? "C" : posixLocale.data(),
						XKB_COMPOSE_COMPILE_NO_FLAGS);
				if (composeTable) {
					seat->compose = seat->root->xkb->xkb_compose_state_new(composeTable,
							XKB_COMPOSE_STATE_NO_FLAGS);
					seat->root->xkb->xkb_compose_table_unref(composeTable);
				}

				::munmap(map_shm, size);
			}
		}
		::close(fd);
	},

	.enter = [](void *data, wl_keyboard *wl_keyboard, uint32_t serial, wl_surface *surface,
			struct wl_array *keys) {
		auto seat = (WaylandSeat *)data;
		if (seat->root->ownsSurface(surface)) {
			if (auto view = (WaylandWindow *)seat->wayland->wl_surface_get_user_data(surface)) {
				Vector<uint32_t> keysVec;
				for (uint32_t *it = (uint32_t *)keys->data;
						(const char *)it < ((const char *)keys->data + keys->size); ++it) {
					keysVec.emplace_back(*it);
				}

				seat->keyboardViews.emplace(view);
				view->handleKeyboardEnter(sp::move(keysVec), seat->keyState.modsDepressed,
						seat->keyState.modsLatched, seat->keyState.modsLocked);
			}
		}
	},

	.leave = [](void *data, wl_keyboard *wl_keyboard, uint32_t serial, wl_surface *surface) {
		auto seat = (WaylandSeat *)data;
		if (seat->root->ownsSurface(surface)) {
			if (auto view = (WaylandWindow *)seat->wayland->wl_surface_get_user_data(surface)) {
				view->handleKeyboardLeave();
				seat->keyboardViews.erase(view);
			}
		}
	},

	.key = [](void *data, wl_keyboard *wl_keyboard, uint32_t serial, uint32_t time, uint32_t key,
			uint32_t state) {
		auto seat = (WaylandSeat *)data;
		for (auto &it : seat->keyboardViews) { it->handleKey(time, key, state); }
	},

	.modifiers = [](void *data, wl_keyboard *wl_keyboard, uint32_t serial, uint32_t mods_depressed,
			uint32_t mods_latched, uint32_t mods_locked, uint32_t group) {
		auto seat = (WaylandSeat *)data;
		if (seat->state) {
			seat->root->xkb->xkb_state_update_mask(seat->state, mods_depressed, mods_latched,
					mods_locked, 0, 0, group);
			seat->keyState.modsDepressed = mods_depressed;
			seat->keyState.modsLatched = mods_latched;
			seat->keyState.modsLocked = mods_locked;
			for (auto &it : seat->keyboardViews) {
				it->handleKeyModifiers(mods_depressed, mods_latched, mods_locked);
			}
		}
	},

	.repeat_info = [](void *data, wl_keyboard *wl_keyboard, int32_t rate, int32_t delay) {
		auto seat = (WaylandSeat *)data;
		seat->keyState.keyRepeatRate = rate;
		seat->keyState.keyRepeatDelay = delay;
		seat->keyState.keyRepeatInterval = 1'000'000 / rate;
	}
};

static struct wl_touch_listener s_WaylandTouchListener{// down
	[](void *data, wl_touch *touch, uint32_t serial, uint32_t time, wl_surface *surface, int32_t id,
			wl_fixed_t x, wl_fixed_t y) {

},

	// up
	[](void *data, wl_touch *touch, uint32_t serial, uint32_t time, int32_t id) {

},

	// motion
	[](void *data, wl_touch *touch, uint32_t time, int32_t id, wl_fixed_t x, wl_fixed_t y) {

},

	// frame
	[](void *data, wl_touch *touch) {

},

	// cancel
	[](void *data, wl_touch *touch) {

},

	// shape
	[](void *data, wl_touch *touch, int32_t id, wl_fixed_t major, wl_fixed_t minor) {

},

	// orientation
	[](void *data, wl_touch *touch, int32_t id, wl_fixed_t orientation) {

}};

static struct wl_seat_listener s_WaylandSeatListener{
	[](void *data, struct wl_seat *wl_seat, uint32_t capabilities) {
	auto seat = (WaylandSeat *)data;
	seat->capabilities = capabilities;
	seat->root->seatDirty = true;

	seat->update();
}, [](void *data, struct wl_seat *wl_seat, const char *name) {
	auto seat = (WaylandSeat *)data;
	seat->name = name;
}};

static struct wl_surface_listener s_cursorSurfaceListener = {
	.enter = [](void *data, wl_surface *surface, wl_output *output) {
		auto seat = reinterpret_cast<WaylandSeat *>(data);
		if (!seat->wayland->ownsProxy(output)) {
			return;
		}

		auto out = (WaylandOutput *)seat->wayland->wl_output_get_user_data(output);
		seat->pointerOutputs.emplace(out);
		seat->tryUpdateCursor();
	},
	.leave = [](void *data, struct wl_surface *wl_surface, struct wl_output *output) {
		auto seat = reinterpret_cast<WaylandSeat *>(data);
		if (!seat->wayland->ownsProxy(output)) {
			return;
		}

		auto out = (WaylandOutput *)seat->wayland->wl_output_get_user_data(output);
		seat->pointerOutputs.erase(out);
	},
	.preferred_buffer_scale = [](void *data, struct wl_surface *wl_surface, int32_t factor) {
		auto seat = reinterpret_cast<WaylandSeat *>(data);
		seat->pointerScale = float(factor);
	},
	.preferred_buffer_transform = [](void *data, struct wl_surface *wl_surface, uint32_t transform) { }
};

// clang-format on

WaylandSeat::~WaylandSeat() {
	if (state) {
		root->xkb->xkb_state_unref(state);
		state = nullptr;
	}
	if (compose) {
		root->xkb->xkb_compose_state_unref(compose);
		compose = nullptr;
	}
	if (seat) {
		wayland->wl_seat_destroy(seat);
		seat = nullptr;
	}
}

bool WaylandSeat::init(NotNull<WaylandLibrary> lib, NotNull<WaylandDisplay> view,
		wl_registry *registry, uint32_t name, uint32_t version) {
	wayland = lib;
	root = view;
	id = name;
	if (version >= 5U) {
		hasPointerFrames = true;
	}
	seat = static_cast<wl_seat *>(
			wayland->wl_registry_bind(registry, name, wayland->wl_seat_interface,
					std::min(version, uint32_t(wayland->wl_seat_interface->version))));

	wayland->wl_seat_set_user_data(seat, this);
	wayland->wl_seat_add_listener(seat, &s_WaylandSeatListener, this);
	wayland->wl_proxy_set_tag((struct wl_proxy *)seat, &s_XenolithWaylandTag);

	return true;
}

void WaylandSeat::setCursor(WindowCursor image, bool serverSide) {
	cursorImage = image;
	auto waylandCursor = getWaylandCursor(cursorImage);
	if (serverSide && cursorShape && waylandCursor) {
		serverSideCursor = true;
		wayland->wp_cursor_shape_device_v1_set_shape(cursorShape, serial, waylandCursor);
	} else if (cursorTheme) {
		serverSideCursor = false;
		cursorTheme->setCursor(this);
	}
}

void WaylandSeat::setCursors(StringView theme, int32_t size) {
	size *= pointerScale;

	if (!cursorTheme || cursorTheme->cursorSize != size || cursorTheme->cursorName != theme) {
		cursorTheme = Rc<WaylandCursorTheme>::create(root, theme, size);
	}

	if (!cursorSurface) {
		cursorSurface = wayland->wl_compositor_create_surface(root->compositor);
		wayland->wl_surface_add_listener(cursorSurface, &s_cursorSurfaceListener, this);
	}
}

void WaylandSeat::tryUpdateCursor() {
	auto waylandCursor = getWaylandCursor(cursorImage);
	if (serverSideCursor && cursorShape && waylandCursor) {
		wayland->wp_cursor_shape_device_v1_set_shape(cursorShape, serial, waylandCursor);
	} else {
		serverSideCursor = false;
		if (cursorTheme) {
			setCursors(cursorTheme->cursorName, cursorTheme->cursorSize);
		}
		if (cursorTheme) {
			cursorTheme->setCursor(this);
		}
	}
}

void WaylandSeat::update() {
	if (!root->seatDirty) {
		return;
	}

	if (!dataDevice && root->dataDeviceManager) {
		dataDevice = Rc<WaylandDataDevice>::create(root->dataDeviceManager, this);
	}

	root->seatDirty = false;

	if ((capabilities & WL_SEAT_CAPABILITY_POINTER) != 0 && !pointer) {
		pointer = wayland->wl_seat_get_pointer(seat);
		wayland->wl_pointer_add_listener(pointer, &s_WaylandPointerListener, this);
		pointerScale = 1;
		if (root->cursorManager) {
			cursorShape =
					wayland->wp_cursor_shape_manager_v1_get_pointer(root->cursorManager, pointer);
		}
		if (cursorTheme) {
			setCursors(cursorTheme->cursorName, cursorTheme->cursorSize);
		}
	} else if ((capabilities & WL_SEAT_CAPABILITY_POINTER) == 0 && pointer) {
		if (cursorShape) {
			wayland->wp_cursor_shape_device_v1_destroy(cursorShape);
		}
		wayland->wl_pointer_release(pointer);
		pointer = NULL;
	}

	if ((capabilities & WL_SEAT_CAPABILITY_KEYBOARD) != 0 && !keyboard) {
		keyboard = wayland->wl_seat_get_keyboard(seat);
		wayland->wl_keyboard_add_listener(keyboard, &s_WaylandKeyboardListener, this);
	} else if ((capabilities & WL_SEAT_CAPABILITY_KEYBOARD) == 0 && keyboard) {
		wayland->wl_keyboard_release(keyboard);
		keyboard = NULL;
	}

	if ((capabilities & WL_SEAT_CAPABILITY_TOUCH) != 0 && !touch) {
		touch = wayland->wl_seat_get_touch(seat);
		wayland->wl_touch_add_listener(touch, &s_WaylandTouchListener, this);
	} else if ((capabilities & WL_SEAT_CAPABILITY_TOUCH) == 0 && touch) {
		wayland->wl_touch_release(touch);
		touch = NULL;
	}

	// wayland->wl_display_roundtrip(root->display);
}

void WaylandSeat::clearWindow(WaylandWindow *window) {
	pointerViews.erase(window);
	keyboardViews.erase(window);
}

core::InputKeyCode WaylandSeat::translateKey(uint32_t scancode) const {
	if (scancode < sizeof(keyState.keycodes) / sizeof(keyState.keycodes[0])) {
		return keyState.keycodes[scancode];
	}

	return core::InputKeyCode::Unknown;
}

xkb_keysym_t WaylandSeat::composeSymbol(xkb_keysym_t sym,
		core::InputKeyComposeState &composeState) const {
	if (sym == XKB_KEY_NoSymbol || !compose) {
		return sym;
	}
	if (root->xkb->xkb_compose_state_feed(compose, sym) != XKB_COMPOSE_FEED_ACCEPTED) {
		return sym;
	}
	switch (root->xkb->xkb_compose_state_get_status(compose)) {
	case XKB_COMPOSE_COMPOSED:
		composeState = core::InputKeyComposeState::Composed;
		return root->xkb->xkb_compose_state_get_one_sym(compose);
	case XKB_COMPOSE_COMPOSING: composeState = core::InputKeyComposeState::Composing; return sym;
	case XKB_COMPOSE_CANCELLED: return sym;
	case XKB_COMPOSE_NOTHING:
	default: return sym;
	}
}

WaylandCursorTheme::~WaylandCursorTheme() {
	if (cursorTheme) {
		wayland->wl_cursor_theme_destroy(cursorTheme);
		cursorTheme = nullptr;
	}
}

bool WaylandCursorTheme::init(WaylandDisplay *display, StringView name, int size) {
	wayland = display->wayland;
	cursorSize = size;
	cursorName = name.str<Interface>();
	cursorTheme = wayland->wl_cursor_theme_load(name.data(), size, display->shm->shm);

	if (cursorTheme) {
		for (auto cursor : sp::each<WindowCursor>()) {
			auto names = getCursorNames(cursor);
			if (names.empty()) {
				cursors.emplace_back(nullptr);
				continue;
			}

			wl_cursor *c = nullptr;
			for (auto &it : names) {
				c = wayland->wl_cursor_theme_get_cursor(cursorTheme, it.data());
				if (c) {
					cursors.emplace_back(c);
					break;
				}
			}
			if (!c) {
				cursors.emplace_back(nullptr);
			}
		}
		return true;
	}

	return false;
}

void WaylandCursorTheme::setCursor(WaylandSeat *seat) {
	setCursor(seat->pointer, seat->cursorSurface, seat->serial, seat->cursorImage,
			seat->pointerScale);
}

void WaylandCursorTheme::setCursor(wl_pointer *pointer, wl_surface *cursorSurface, uint32_t serial,
		WindowCursor cursorIndex, int scale) {
	if (!cursorTheme || cursors.size() <= size_t(cursorIndex)) {
		return;
	}

	auto cursor = cursors.at(toInt(cursorIndex));
	if (!cursor) {
		cursor = cursors[toInt(WindowCursor::Default)]; // default arrow
	}

	auto image = cursor->images[0];
	auto buffer = wayland->wl_cursor_image_get_buffer(image);
	wayland->wl_pointer_set_cursor(pointer, serial, cursorSurface, image->hotspot_x / scale,
			image->hotspot_y / scale);
	wayland->wl_surface_attach(cursorSurface, buffer, 0, 0);
	wayland->wl_surface_set_buffer_scale(cursorSurface, scale);
	wayland->wl_surface_damage_buffer(cursorSurface, 0, 0, image->width, image->height);
	wayland->wl_surface_commit(cursorSurface);
}

bool WaylandCursorTheme::hasCursor(WindowCursor cursor) const {
	auto idx = toInt(cursor);
	if (idx < cursors.size()) {
		return cursors[idx] != nullptr;
	}
	return false;
}

} // namespace stappler::xenolith::platform
