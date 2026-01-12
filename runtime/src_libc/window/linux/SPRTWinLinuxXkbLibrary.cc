/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>

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

#include "private/window/linux/SPRTWinLinuxXkbLibrary.h"
#include "private/window/linux/SPRTWinLinuxXcbLibrary.h"

#include <sprt/runtime/log.h>

#include <stdlib.h>
#include <string.h>

namespace sprt::window {

XkbInfo::XkbInfo(NotNull<XkbLibrary> l) : lib(l) { }

XkbInfo::~XkbInfo() {
	if (state) {
		lib->xkb_state_unref(state);
		state = nullptr;
	}
	if (keymap) {
		lib->xkb_keymap_unref(keymap);
		keymap = nullptr;
	}
	if (compose) {
		lib->xkb_compose_state_unref(compose);
		compose = nullptr;
	}
}

bool XkbInfo::initXcb(NotNull<XcbLibrary> xcb, xcb_connection_t *conn) {
	if (!initialized) {
		if (lib->xkb_x11_setup_xkb_extension(conn, XKB_X11_MIN_MAJOR_XKB_VERSION,
					XKB_X11_MIN_MINOR_XKB_VERSION, XKB_X11_SETUP_XKB_EXTENSION_NO_FLAGS,
					&majorVersion, &minorVersion, &firstEvent, &firstError)
				!= 1) {
			return false;
		}
	}

	initialized = true;
	deviceId = lib->xkb_x11_get_core_keyboard_device_id(conn);

	enum {
		required_events = (XCB_XKB_EVENT_TYPE_NEW_KEYBOARD_NOTIFY | XCB_XKB_EVENT_TYPE_MAP_NOTIFY
				| XCB_XKB_EVENT_TYPE_STATE_NOTIFY),

		required_nkn_details = (XCB_XKB_NKN_DETAIL_KEYCODES),

		required_map_parts = (XCB_XKB_MAP_PART_KEY_TYPES | XCB_XKB_MAP_PART_KEY_SYMS
				| XCB_XKB_MAP_PART_MODIFIER_MAP | XCB_XKB_MAP_PART_EXPLICIT_COMPONENTS
				| XCB_XKB_MAP_PART_KEY_ACTIONS | XCB_XKB_MAP_PART_VIRTUAL_MODS
				| XCB_XKB_MAP_PART_VIRTUAL_MOD_MAP),

		required_state_details =
				(XCB_XKB_STATE_PART_MODIFIER_BASE | XCB_XKB_STATE_PART_MODIFIER_LATCH
						| XCB_XKB_STATE_PART_MODIFIER_LOCK | XCB_XKB_STATE_PART_GROUP_BASE
						| XCB_XKB_STATE_PART_GROUP_LATCH | XCB_XKB_STATE_PART_GROUP_LOCK),
	};

	static const xcb_xkb_select_events_details_t details = {.affectNewKeyboard =
																	required_nkn_details,
		.newKeyboardDetails = required_nkn_details,
		.affectState = required_state_details,
		.stateDetails = required_state_details};

	xcb->xcb_xkb_select_events(conn, deviceId, required_events, 0, required_events,
			required_map_parts, required_map_parts, &details);

	updateXkbMapping(conn);
	return true;
}

void XkbInfo::updateXkbMapping(xcb_connection_t *conn) {
	if (state) {
		lib->xkb_state_unref(state);
		state = nullptr;
	}
	if (keymap) {
		lib->xkb_keymap_unref(keymap);
		keymap = nullptr;
	}
	if (compose) {
		lib->xkb_compose_state_unref(compose);
		compose = nullptr;
	}

	keymap = lib->xkb_x11_keymap_new_from_device(lib->getContext(), conn, deviceId,
			XKB_KEYMAP_COMPILE_NO_FLAGS);
	if (keymap == nullptr) {
		fprintf(stderr, "Failed to get Keymap for current keyboard device.\n");
		return;
	}

	state = lib->xkb_x11_state_new_from_device(keymap, conn, deviceId);
	if (state == nullptr) {
		fprintf(stderr, "Failed to get state object for current keyboard device.\n");
		return;
	}

	memset(keycodes, 0, sizeof(InputKeyCode) * 256);

	lib->xkb_keymap_key_for_each(keymap,
			[](struct xkb_keymap *keymap, xkb_keycode_t key, void *data) {
		((XkbInfo *)data)->updateXkbKey(key);
	}, this);

	auto locale = getenv("LC_ALL");
	if (!locale) {
		locale = getenv("LC_CTYPE");
	}
	if (!locale) {
		locale = getenv("LANG");
	}

	auto composeTable = lib->xkb_compose_table_new_from_locale(lib->getContext(),
			locale ? locale : "C", XKB_COMPOSE_COMPILE_NO_FLAGS);
	if (composeTable) {
		compose = lib->xkb_compose_state_new(composeTable, XKB_COMPOSE_STATE_NO_FLAGS);
		lib->xkb_compose_table_unref(composeTable);
	}
}

void XkbInfo::updateXkbKey(xcb_keycode_t code) {
	static const struct {
		InputKeyCode key;
		const char *name;
	} keymap[] = {{InputKeyCode::GRAVE_ACCENT, "TLDE"}, {InputKeyCode::_1, "AE01"},
		{InputKeyCode::_2, "AE02"}, {InputKeyCode::_3, "AE03"}, {InputKeyCode::_4, "AE04"},
		{InputKeyCode::_5, "AE05"}, {InputKeyCode::_6, "AE06"}, {InputKeyCode::_7, "AE07"},
		{InputKeyCode::_8, "AE08"}, {InputKeyCode::_9, "AE09"}, {InputKeyCode::_0, "AE10"},
		{InputKeyCode::MINUS, "AE11"}, {InputKeyCode::EQUAL, "AE12"}, {InputKeyCode::Q, "AD01"},
		{InputKeyCode::W, "AD02"}, {InputKeyCode::E, "AD03"}, {InputKeyCode::R, "AD04"},
		{InputKeyCode::T, "AD05"}, {InputKeyCode::Y, "AD06"}, {InputKeyCode::U, "AD07"},
		{InputKeyCode::I, "AD08"}, {InputKeyCode::O, "AD09"}, {InputKeyCode::P, "AD10"},
		{InputKeyCode::LEFT_BRACKET, "AD11"}, {InputKeyCode::RIGHT_BRACKET, "AD12"},
		{InputKeyCode::A, "AC01"}, {InputKeyCode::S, "AC02"}, {InputKeyCode::D, "AC03"},
		{InputKeyCode::F, "AC04"}, {InputKeyCode::G, "AC05"}, {InputKeyCode::H, "AC06"},
		{InputKeyCode::J, "AC07"}, {InputKeyCode::K, "AC08"}, {InputKeyCode::L, "AC09"},
		{InputKeyCode::SEMICOLON, "AC10"}, {InputKeyCode::APOSTROPHE, "AC11"},
		{InputKeyCode::Z, "AB01"}, {InputKeyCode::X, "AB02"}, {InputKeyCode::C, "AB03"},
		{InputKeyCode::V, "AB04"}, {InputKeyCode::B, "AB05"}, {InputKeyCode::N, "AB06"},
		{InputKeyCode::M, "AB07"}, {InputKeyCode::COMMA, "AB08"}, {InputKeyCode::PERIOD, "AB09"},
		{InputKeyCode::SLASH, "AB10"}, {InputKeyCode::BACKSLASH, "BKSL"},
		{InputKeyCode::WORLD_1, "LSGT"}, {InputKeyCode::SPACE, "SPCE"},
		{InputKeyCode::ESCAPE, "ESC"}, {InputKeyCode::ENTER, "RTRN"}, {InputKeyCode::TAB, "TAB"},
		{InputKeyCode::BACKSPACE, "BKSP"}, {InputKeyCode::INSERT, "INS"},
		{InputKeyCode::DELETE, "DELE"}, {InputKeyCode::RIGHT, "RGHT"}, {InputKeyCode::LEFT, "LEFT"},
		{InputKeyCode::DOWN, "DOWN"}, {InputKeyCode::UP, "UP"}, {InputKeyCode::PAGE_UP, "PGUP"},
		{InputKeyCode::PAGE_DOWN, "PGDN"}, {InputKeyCode::HOME, "HOME"}, {InputKeyCode::END, "END"},
		{InputKeyCode::CAPS_LOCK, "CAPS"}, {InputKeyCode::SCROLL_LOCK, "SCLK"},
		{InputKeyCode::NUM_LOCK, "NMLK"}, {InputKeyCode::PRINT_SCREEN, "PRSC"},
		{InputKeyCode::PAUSE, "PAUS"}, {InputKeyCode::F1, "FK01"}, {InputKeyCode::F2, "FK02"},
		{InputKeyCode::F3, "FK03"}, {InputKeyCode::F4, "FK04"}, {InputKeyCode::F5, "FK05"},
		{InputKeyCode::F6, "FK06"}, {InputKeyCode::F7, "FK07"}, {InputKeyCode::F8, "FK08"},
		{InputKeyCode::F9, "FK09"}, {InputKeyCode::F10, "FK10"}, {InputKeyCode::F11, "FK11"},
		{InputKeyCode::F12, "FK12"}, {InputKeyCode::F13, "FK13"}, {InputKeyCode::F14, "FK14"},
		{InputKeyCode::F15, "FK15"}, {InputKeyCode::F16, "FK16"}, {InputKeyCode::F17, "FK17"},
		{InputKeyCode::F18, "FK18"}, {InputKeyCode::F19, "FK19"}, {InputKeyCode::F20, "FK20"},
		{InputKeyCode::F21, "FK21"}, {InputKeyCode::F22, "FK22"}, {InputKeyCode::F23, "FK23"},
		{InputKeyCode::F24, "FK24"}, {InputKeyCode::F25, "FK25"}, {InputKeyCode::KP_0, "KP0"},
		{InputKeyCode::KP_1, "KP1"}, {InputKeyCode::KP_2, "KP2"}, {InputKeyCode::KP_3, "KP3"},
		{InputKeyCode::KP_4, "KP4"}, {InputKeyCode::KP_5, "KP5"}, {InputKeyCode::KP_6, "KP6"},
		{InputKeyCode::KP_7, "KP7"}, {InputKeyCode::KP_8, "KP8"}, {InputKeyCode::KP_9, "KP9"},
		{InputKeyCode::KP_DECIMAL, "KPDL"}, {InputKeyCode::KP_DIVIDE, "KPDV"},
		{InputKeyCode::KP_MULTIPLY, "KPMU"}, {InputKeyCode::KP_SUBTRACT, "KPSU"},
		{InputKeyCode::KP_ADD, "KPAD"}, {InputKeyCode::KP_ENTER, "KPEN"},
		{InputKeyCode::KP_EQUAL, "KPEQ"}, {InputKeyCode::LEFT_SHIFT, "LFSH"},
		{InputKeyCode::LEFT_CONTROL, "LCTL"}, {InputKeyCode::LEFT_ALT, "LALT"},
		{InputKeyCode::LEFT_SUPER, "LWIN"}, {InputKeyCode::RIGHT_SHIFT, "RTSH"},
		{InputKeyCode::RIGHT_CONTROL, "RCTL"}, {InputKeyCode::RIGHT_ALT, "RALT"},
		{InputKeyCode::RIGHT_ALT, "LVL3"}, {InputKeyCode::RIGHT_ALT, "MDSW"},
		{InputKeyCode::RIGHT_SUPER, "RWIN"}, {InputKeyCode::MENU, "MENU"}};

	InputKeyCode key = InputKeyCode::Unknown;
	if (auto name = lib->xkb_keymap_key_get_name(this->keymap, code)) {
		for (size_t i = 0; i < sizeof(keymap) / sizeof(keymap[0]); i++) {
			if (strncmp(name, keymap[i].name, 4) == 0) {
				key = keymap[i].key;
				break;
			}
		}
	}

	if (key != InputKeyCode::Unknown) {
		keycodes[code] = key;
	}
}

xkb_keysym_t XkbInfo::composeSymbol(xkb_keysym_t sym, InputKeyComposeState &cState) const {
	if (sym == XKB_KEY_NoSymbol || !compose) {
		return sym;
	}
	if (lib->xkb_compose_state_feed(compose, sym) != XKB_COMPOSE_FEED_ACCEPTED) {
		return sym;
	}
	auto composedSym = sym;
	auto state = lib->xkb_compose_state_get_status(compose);
	switch (state) {
	case XKB_COMPOSE_COMPOSED:
		cState = InputKeyComposeState::Composed;
		composedSym = lib->xkb_compose_state_get_one_sym(compose);
		lib->xkb_compose_state_reset(compose);
		break;
	case XKB_COMPOSE_COMPOSING: cState = InputKeyComposeState::Composing; break;
	case XKB_COMPOSE_CANCELLED: lib->xkb_compose_state_reset(compose); break;
	case XKB_COMPOSE_NOTHING: lib->xkb_compose_state_reset(compose); break;
	default: break;
	}
	return composedSym;
}

XkbLibrary::~XkbLibrary() { close(); }

bool XkbLibrary::init() {
	_handle = Dso("libxkbcommon.so");
	if (!_handle) {
		log::vperror(__SPRT_LOCATION, "XkbLibrary",
				"Fail to open libxkbcommon.so: ", _handle.getError());
		return false;
	}

	if (open(_handle)) {
		_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
		return true;
	} else {
		_handle = Dso();
	}
	return false;
}

void XkbLibrary::close() {
	if (_context) {
		xkb_context_unref(_context);
		_context = nullptr;
	}
}

bool XkbLibrary::open(Dso &handle) {
	SPRT_LOAD_PROTO(handle, xkb_context_new)
	SPRT_LOAD_PROTO(handle, xkb_context_ref)
	SPRT_LOAD_PROTO(handle, xkb_context_unref)
	SPRT_LOAD_PROTO(handle, xkb_keymap_unref)
	SPRT_LOAD_PROTO(handle, xkb_state_unref)
	SPRT_LOAD_PROTO(handle, xkb_keymap_new_from_string)
	SPRT_LOAD_PROTO(handle, xkb_state_new)
	SPRT_LOAD_PROTO(handle, xkb_state_update_mask)
	SPRT_LOAD_PROTO(handle, xkb_state_key_get_utf8)
	SPRT_LOAD_PROTO(handle, xkb_state_key_get_utf32)
	SPRT_LOAD_PROTO(handle, xkb_state_key_get_one_sym)
	SPRT_LOAD_PROTO(handle, xkb_state_mod_index_is_active)
	SPRT_LOAD_PROTO(handle, xkb_state_key_get_syms)
	SPRT_LOAD_PROTO(handle, xkb_state_get_keymap)
	SPRT_LOAD_PROTO(handle, xkb_keymap_key_for_each)
	SPRT_LOAD_PROTO(handle, xkb_keymap_key_get_name)
	SPRT_LOAD_PROTO(handle, xkb_keymap_mod_get_index)
	SPRT_LOAD_PROTO(handle, xkb_keymap_key_repeats)
	SPRT_LOAD_PROTO(handle, xkb_keysym_to_utf32)
	SPRT_LOAD_PROTO(handle, xkb_compose_table_new_from_locale)
	SPRT_LOAD_PROTO(handle, xkb_compose_table_unref)
	SPRT_LOAD_PROTO(handle, xkb_compose_state_new)
	SPRT_LOAD_PROTO(handle, xkb_compose_state_feed)
	SPRT_LOAD_PROTO(handle, xkb_compose_state_reset)
	SPRT_LOAD_PROTO(handle, xkb_compose_state_get_status)
	SPRT_LOAD_PROTO(handle, xkb_compose_state_get_one_sym)
	SPRT_LOAD_PROTO(handle, xkb_compose_state_unref)

	if (!validateFunctionList(&_xkb_first_fn, &_xkb_last_fn)) {
		log::vperror(__SPRT_LOCATION, "XkbLibrary", "Fail to load libxkb");
		return false;
	}

	openAux();
	return true;
}

void XkbLibrary::openAux() {
	if (auto handle = Dso("libxkbcommon-x11.so")) {
		SPRT_LOAD_PROTO(handle, xkb_x11_setup_xkb_extension);
		SPRT_LOAD_PROTO(handle, xkb_x11_get_core_keyboard_device_id)
		SPRT_LOAD_PROTO(handle, xkb_x11_keymap_new_from_device)
		SPRT_LOAD_PROTO(handle, xkb_x11_state_new_from_device)

		if (!validateFunctionList(&_xkb_x11_first_fn, &_xkb_x11_last_fn)) {
			log::vperror(__SPRT_LOCATION, "XcbLibrary", "Fail to load libxcb-randr function");
		} else {
			_x11 = move(handle);
		}
	}
}

} // namespace sprt::window
