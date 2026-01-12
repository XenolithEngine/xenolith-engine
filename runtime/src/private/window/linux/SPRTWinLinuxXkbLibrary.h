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

#ifndef CORE_RUNTIME_PRIVATE_WINDOW_LINUX_SPRTWINPLATFORMLINUXXKB_H_
#define CORE_RUNTIME_PRIVATE_WINDOW_LINUX_SPRTWINPLATFORMLINUXXKB_H_

#include <sprt/runtime/init.h>

#if SPRT_LINUX

#include <sprt/runtime/ref.h>
#include <sprt/runtime/window/input.h>

#include "private/SPRTDso.h"

#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-x11.h>
#include <xkbcommon/xkbcommon-compose.h>
#include <xkbcommon/xkbcommon-names.h>

namespace sprt::window {

class XkbLibrary;
class XcbLibrary;

struct XkbInfo {
	Rc<XkbLibrary> lib;

	bool enabled = true;
	bool initialized = false;
	uint8_t firstEvent = 0;
	uint8_t firstError = 0;

	uint16_t majorVersion = 0;
	uint16_t minorVersion = 0;
	int32_t deviceId = 0;

	xkb_keymap *keymap = nullptr;
	xkb_state *state = nullptr;
	xkb_compose_state *compose = nullptr;

	InputKeyCode keycodes[256] = {InputKeyCode::Unknown};

	XkbInfo(NotNull<XkbLibrary>);
	~XkbInfo();

	bool initXcb(NotNull<XcbLibrary>, xcb_connection_t *);
	void updateXkbMapping(xcb_connection_t *conn);
	void updateXkbKey(xcb_keycode_t code);

	xkb_keysym_t composeSymbol(xkb_keysym_t sym, InputKeyComposeState &compose) const;
};

class SPRT_API XkbLibrary : public Ref {
public:
	XkbLibrary() { }

	virtual ~XkbLibrary();

	bool init();
	void close();

	bool hasX11() const { return _x11 ? true : false; }

	struct xkb_context *getContext() const { return _context; }

	decltype(&_sprt_null_fn) _xkb_first_fn = &_sprt_null_fn;
	SPRT_DEFINE_PROTO(xkb_context_new)
	SPRT_DEFINE_PROTO(xkb_context_ref)
	SPRT_DEFINE_PROTO(xkb_context_unref)
	SPRT_DEFINE_PROTO(xkb_keymap_unref)
	SPRT_DEFINE_PROTO(xkb_state_unref)
	SPRT_DEFINE_PROTO(xkb_keymap_new_from_string)
	SPRT_DEFINE_PROTO(xkb_state_new)
	SPRT_DEFINE_PROTO(xkb_state_update_mask)
	SPRT_DEFINE_PROTO(xkb_state_key_get_utf8)
	SPRT_DEFINE_PROTO(xkb_state_key_get_utf32)
	SPRT_DEFINE_PROTO(xkb_state_key_get_one_sym)
	SPRT_DEFINE_PROTO(xkb_state_mod_index_is_active)
	SPRT_DEFINE_PROTO(xkb_state_key_get_syms)
	SPRT_DEFINE_PROTO(xkb_state_get_keymap)
	SPRT_DEFINE_PROTO(xkb_keymap_key_for_each)
	SPRT_DEFINE_PROTO(xkb_keymap_key_get_name)
	SPRT_DEFINE_PROTO(xkb_keymap_mod_get_index)
	SPRT_DEFINE_PROTO(xkb_keymap_key_repeats)
	SPRT_DEFINE_PROTO(xkb_keysym_to_utf32)
	SPRT_DEFINE_PROTO(xkb_compose_table_new_from_locale)
	SPRT_DEFINE_PROTO(xkb_compose_table_unref)
	SPRT_DEFINE_PROTO(xkb_compose_state_new)
	SPRT_DEFINE_PROTO(xkb_compose_state_feed)
	SPRT_DEFINE_PROTO(xkb_compose_state_reset)
	SPRT_DEFINE_PROTO(xkb_compose_state_get_status)
	SPRT_DEFINE_PROTO(xkb_compose_state_get_one_sym)
	SPRT_DEFINE_PROTO(xkb_compose_state_unref)
	decltype(&_sprt_null_fn) _xkb_last_fn = &_sprt_null_fn;

	decltype(&_sprt_null_fn) _xkb_x11_first_fn = &_sprt_null_fn;
	SPRT_DEFINE_PROTO(xkb_x11_setup_xkb_extension)
	SPRT_DEFINE_PROTO(xkb_x11_get_core_keyboard_device_id)
	SPRT_DEFINE_PROTO(xkb_x11_keymap_new_from_device)
	SPRT_DEFINE_PROTO(xkb_x11_state_new_from_device)
	decltype(&_sprt_null_fn) _xkb_x11_last_fn = &_sprt_null_fn;

protected:
	bool open(Dso &);
	void openAux();

	Dso _handle;
	Dso _x11;
	struct xkb_context *_context = nullptr;
};

} // namespace sprt::window

#endif

#endif /* CORE_RUNTIME_PRIVATE_WINDOW_LINUX_SPRTWINPLATFORMLINUXXKB_H_ */
