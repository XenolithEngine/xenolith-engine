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

#include "XLLinuxXkbLibrary.h"
#include "xcb/XLLinuxXcbLibrary.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

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

	memset(keycodes, 0, sizeof(core::InputKeyCode) * 256);

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
		core::InputKeyCode key;
		const char *name;
	} keymap[] = {{core::InputKeyCode::GRAVE_ACCENT, "TLDE"}, {core::InputKeyCode::_1, "AE01"},
		{core::InputKeyCode::_2, "AE02"}, {core::InputKeyCode::_3, "AE03"},
		{core::InputKeyCode::_4, "AE04"}, {core::InputKeyCode::_5, "AE05"},
		{core::InputKeyCode::_6, "AE06"}, {core::InputKeyCode::_7, "AE07"},
		{core::InputKeyCode::_8, "AE08"}, {core::InputKeyCode::_9, "AE09"},
		{core::InputKeyCode::_0, "AE10"}, {core::InputKeyCode::MINUS, "AE11"},
		{core::InputKeyCode::EQUAL, "AE12"}, {core::InputKeyCode::Q, "AD01"},
		{core::InputKeyCode::W, "AD02"}, {core::InputKeyCode::E, "AD03"},
		{core::InputKeyCode::R, "AD04"}, {core::InputKeyCode::T, "AD05"},
		{core::InputKeyCode::Y, "AD06"}, {core::InputKeyCode::U, "AD07"},
		{core::InputKeyCode::I, "AD08"}, {core::InputKeyCode::O, "AD09"},
		{core::InputKeyCode::P, "AD10"}, {core::InputKeyCode::LEFT_BRACKET, "AD11"},
		{core::InputKeyCode::RIGHT_BRACKET, "AD12"}, {core::InputKeyCode::A, "AC01"},
		{core::InputKeyCode::S, "AC02"}, {core::InputKeyCode::D, "AC03"},
		{core::InputKeyCode::F, "AC04"}, {core::InputKeyCode::G, "AC05"},
		{core::InputKeyCode::H, "AC06"}, {core::InputKeyCode::J, "AC07"},
		{core::InputKeyCode::K, "AC08"}, {core::InputKeyCode::L, "AC09"},
		{core::InputKeyCode::SEMICOLON, "AC10"}, {core::InputKeyCode::APOSTROPHE, "AC11"},
		{core::InputKeyCode::Z, "AB01"}, {core::InputKeyCode::X, "AB02"},
		{core::InputKeyCode::C, "AB03"}, {core::InputKeyCode::V, "AB04"},
		{core::InputKeyCode::B, "AB05"}, {core::InputKeyCode::N, "AB06"},
		{core::InputKeyCode::M, "AB07"}, {core::InputKeyCode::COMMA, "AB08"},
		{core::InputKeyCode::PERIOD, "AB09"}, {core::InputKeyCode::SLASH, "AB10"},
		{core::InputKeyCode::BACKSLASH, "BKSL"}, {core::InputKeyCode::WORLD_1, "LSGT"},
		{core::InputKeyCode::SPACE, "SPCE"}, {core::InputKeyCode::ESCAPE, "ESC"},
		{core::InputKeyCode::ENTER, "RTRN"}, {core::InputKeyCode::TAB, "TAB"},
		{core::InputKeyCode::BACKSPACE, "BKSP"}, {core::InputKeyCode::INSERT, "INS"},
		{core::InputKeyCode::DELETE, "DELE"}, {core::InputKeyCode::RIGHT, "RGHT"},
		{core::InputKeyCode::LEFT, "LEFT"}, {core::InputKeyCode::DOWN, "DOWN"},
		{core::InputKeyCode::UP, "UP"}, {core::InputKeyCode::PAGE_UP, "PGUP"},
		{core::InputKeyCode::PAGE_DOWN, "PGDN"}, {core::InputKeyCode::HOME, "HOME"},
		{core::InputKeyCode::END, "END"}, {core::InputKeyCode::CAPS_LOCK, "CAPS"},
		{core::InputKeyCode::SCROLL_LOCK, "SCLK"}, {core::InputKeyCode::NUM_LOCK, "NMLK"},
		{core::InputKeyCode::PRINT_SCREEN, "PRSC"}, {core::InputKeyCode::PAUSE, "PAUS"},
		{core::InputKeyCode::F1, "FK01"}, {core::InputKeyCode::F2, "FK02"},
		{core::InputKeyCode::F3, "FK03"}, {core::InputKeyCode::F4, "FK04"},
		{core::InputKeyCode::F5, "FK05"}, {core::InputKeyCode::F6, "FK06"},
		{core::InputKeyCode::F7, "FK07"}, {core::InputKeyCode::F8, "FK08"},
		{core::InputKeyCode::F9, "FK09"}, {core::InputKeyCode::F10, "FK10"},
		{core::InputKeyCode::F11, "FK11"}, {core::InputKeyCode::F12, "FK12"},
		{core::InputKeyCode::F13, "FK13"}, {core::InputKeyCode::F14, "FK14"},
		{core::InputKeyCode::F15, "FK15"}, {core::InputKeyCode::F16, "FK16"},
		{core::InputKeyCode::F17, "FK17"}, {core::InputKeyCode::F18, "FK18"},
		{core::InputKeyCode::F19, "FK19"}, {core::InputKeyCode::F20, "FK20"},
		{core::InputKeyCode::F21, "FK21"}, {core::InputKeyCode::F22, "FK22"},
		{core::InputKeyCode::F23, "FK23"}, {core::InputKeyCode::F24, "FK24"},
		{core::InputKeyCode::F25, "FK25"}, {core::InputKeyCode::KP_0, "KP0"},
		{core::InputKeyCode::KP_1, "KP1"}, {core::InputKeyCode::KP_2, "KP2"},
		{core::InputKeyCode::KP_3, "KP3"}, {core::InputKeyCode::KP_4, "KP4"},
		{core::InputKeyCode::KP_5, "KP5"}, {core::InputKeyCode::KP_6, "KP6"},
		{core::InputKeyCode::KP_7, "KP7"}, {core::InputKeyCode::KP_8, "KP8"},
		{core::InputKeyCode::KP_9, "KP9"}, {core::InputKeyCode::KP_DECIMAL, "KPDL"},
		{core::InputKeyCode::KP_DIVIDE, "KPDV"}, {core::InputKeyCode::KP_MULTIPLY, "KPMU"},
		{core::InputKeyCode::KP_SUBTRACT, "KPSU"}, {core::InputKeyCode::KP_ADD, "KPAD"},
		{core::InputKeyCode::KP_ENTER, "KPEN"}, {core::InputKeyCode::KP_EQUAL, "KPEQ"},
		{core::InputKeyCode::LEFT_SHIFT, "LFSH"}, {core::InputKeyCode::LEFT_CONTROL, "LCTL"},
		{core::InputKeyCode::LEFT_ALT, "LALT"}, {core::InputKeyCode::LEFT_SUPER, "LWIN"},
		{core::InputKeyCode::RIGHT_SHIFT, "RTSH"}, {core::InputKeyCode::RIGHT_CONTROL, "RCTL"},
		{core::InputKeyCode::RIGHT_ALT, "RALT"}, {core::InputKeyCode::RIGHT_ALT, "LVL3"},
		{core::InputKeyCode::RIGHT_ALT, "MDSW"}, {core::InputKeyCode::RIGHT_SUPER, "RWIN"},
		{core::InputKeyCode::MENU, "MENU"}};

	core::InputKeyCode key = core::InputKeyCode::Unknown;
	if (auto name = lib->xkb_keymap_key_get_name(this->keymap, code)) {
		for (size_t i = 0; i < sizeof(keymap) / sizeof(keymap[0]); i++) {
			if (strncmp(name, keymap[i].name, 4) == 0) {
				key = keymap[i].key;
				break;
			}
		}
	}

	if (key != core::InputKeyCode::Unknown) {
		keycodes[code] = key;
	}
}

xkb_keysym_t XkbInfo::composeSymbol(xkb_keysym_t sym, core::InputKeyComposeState &cState) const {
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
		cState = core::InputKeyComposeState::Composed;
		composedSym = lib->xkb_compose_state_get_one_sym(compose);
		lib->xkb_compose_state_reset(compose);
		break;
	case XKB_COMPOSE_COMPOSING: cState = core::InputKeyComposeState::Composing; break;
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
		log::source().error("XkbLibrary", "Fail to open libxkbcommon.so: ", _handle.getError());
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
	XL_LOAD_PROTO(handle, xkb_context_new)
	XL_LOAD_PROTO(handle, xkb_context_ref)
	XL_LOAD_PROTO(handle, xkb_context_unref)
	XL_LOAD_PROTO(handle, xkb_keymap_unref)
	XL_LOAD_PROTO(handle, xkb_state_unref)
	XL_LOAD_PROTO(handle, xkb_keymap_new_from_string)
	XL_LOAD_PROTO(handle, xkb_state_new)
	XL_LOAD_PROTO(handle, xkb_state_update_mask)
	XL_LOAD_PROTO(handle, xkb_state_key_get_utf8)
	XL_LOAD_PROTO(handle, xkb_state_key_get_utf32)
	XL_LOAD_PROTO(handle, xkb_state_key_get_one_sym)
	XL_LOAD_PROTO(handle, xkb_state_mod_index_is_active)
	XL_LOAD_PROTO(handle, xkb_state_key_get_syms)
	XL_LOAD_PROTO(handle, xkb_state_get_keymap)
	XL_LOAD_PROTO(handle, xkb_keymap_key_for_each)
	XL_LOAD_PROTO(handle, xkb_keymap_key_get_name)
	XL_LOAD_PROTO(handle, xkb_keymap_mod_get_index)
	XL_LOAD_PROTO(handle, xkb_keymap_key_repeats)
	XL_LOAD_PROTO(handle, xkb_keysym_to_utf32)
	XL_LOAD_PROTO(handle, xkb_compose_table_new_from_locale)
	XL_LOAD_PROTO(handle, xkb_compose_table_unref)
	XL_LOAD_PROTO(handle, xkb_compose_state_new)
	XL_LOAD_PROTO(handle, xkb_compose_state_feed)
	XL_LOAD_PROTO(handle, xkb_compose_state_reset)
	XL_LOAD_PROTO(handle, xkb_compose_state_get_status)
	XL_LOAD_PROTO(handle, xkb_compose_state_get_one_sym)
	XL_LOAD_PROTO(handle, xkb_compose_state_unref)

	if (!validateFunctionList(&_xkb_first_fn, &_xkb_last_fn)) {
		log::source().error("XkbLibrary", "Fail to load libxkb");
		return false;
	}

	openAux();
	return true;
}

void XkbLibrary::openAux() {
	if (auto handle = Dso("libxkbcommon-x11.so")) {
		XL_LOAD_PROTO(handle, xkb_x11_setup_xkb_extension);
		XL_LOAD_PROTO(handle, xkb_x11_get_core_keyboard_device_id)
		XL_LOAD_PROTO(handle, xkb_x11_keymap_new_from_device)
		XL_LOAD_PROTO(handle, xkb_x11_state_new_from_device)

		if (!validateFunctionList(&_xkb_x11_first_fn, &_xkb_x11_last_fn)) {
			log::source().error("XcbLibrary", "Fail to load libxcb-randr function");
		} else {
			_x11 = move(handle);
		}
	}
}

} // namespace stappler::xenolith::platform
