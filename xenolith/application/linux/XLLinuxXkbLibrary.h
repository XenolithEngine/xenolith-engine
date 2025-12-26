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

#ifndef XENOLITH_PLATFORM_LINUX_XLPLATFORMLINUXXKB_H_
#define XENOLITH_PLATFORM_LINUX_XLPLATFORMLINUXXKB_H_

#include "XLCommon.h"

#if LINUX

#include "XLLinux.h"
#include "XLCoreInput.h"

#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-x11.h>
#include <xkbcommon/xkbcommon-compose.h>
#include <xkbcommon/xkbcommon-names.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

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

	core::InputKeyCode keycodes[256] = {core::InputKeyCode::Unknown};

	XkbInfo(NotNull<XkbLibrary>);
	~XkbInfo();

	bool initXcb(NotNull<XcbLibrary>, xcb_connection_t *);
	void updateXkbMapping(xcb_connection_t *conn);
	void updateXkbKey(xcb_keycode_t code);

	xkb_keysym_t composeSymbol(xkb_keysym_t sym, core::InputKeyComposeState &compose) const;
};

class SP_PUBLIC XkbLibrary : public Ref {
public:
	XkbLibrary() { }

	virtual ~XkbLibrary();

	bool init();
	void close();

	bool hasX11() const { return _x11 ? true : false; }

	struct xkb_context *getContext() const { return _context; }

	decltype(&_xl_null_fn) _xkb_first_fn = &_xl_null_fn;
	XL_DEFINE_PROTO(xkb_context_new)
	XL_DEFINE_PROTO(xkb_context_ref)
	XL_DEFINE_PROTO(xkb_context_unref)
	XL_DEFINE_PROTO(xkb_keymap_unref)
	XL_DEFINE_PROTO(xkb_state_unref)
	XL_DEFINE_PROTO(xkb_keymap_new_from_string)
	XL_DEFINE_PROTO(xkb_state_new)
	XL_DEFINE_PROTO(xkb_state_update_mask)
	XL_DEFINE_PROTO(xkb_state_key_get_utf8)
	XL_DEFINE_PROTO(xkb_state_key_get_utf32)
	XL_DEFINE_PROTO(xkb_state_key_get_one_sym)
	XL_DEFINE_PROTO(xkb_state_mod_index_is_active)
	XL_DEFINE_PROTO(xkb_state_key_get_syms)
	XL_DEFINE_PROTO(xkb_state_get_keymap)
	XL_DEFINE_PROTO(xkb_keymap_key_for_each)
	XL_DEFINE_PROTO(xkb_keymap_key_get_name)
	XL_DEFINE_PROTO(xkb_keymap_mod_get_index)
	XL_DEFINE_PROTO(xkb_keymap_key_repeats)
	XL_DEFINE_PROTO(xkb_keysym_to_utf32)
	XL_DEFINE_PROTO(xkb_compose_table_new_from_locale)
	XL_DEFINE_PROTO(xkb_compose_table_unref)
	XL_DEFINE_PROTO(xkb_compose_state_new)
	XL_DEFINE_PROTO(xkb_compose_state_feed)
	XL_DEFINE_PROTO(xkb_compose_state_reset)
	XL_DEFINE_PROTO(xkb_compose_state_get_status)
	XL_DEFINE_PROTO(xkb_compose_state_get_one_sym)
	XL_DEFINE_PROTO(xkb_compose_state_unref)
	decltype(&_xl_null_fn) _xkb_last_fn = &_xl_null_fn;

	decltype(&_xl_null_fn) _xkb_x11_first_fn = &_xl_null_fn;
	XL_DEFINE_PROTO(xkb_x11_setup_xkb_extension)
	XL_DEFINE_PROTO(xkb_x11_get_core_keyboard_device_id)
	XL_DEFINE_PROTO(xkb_x11_keymap_new_from_device)
	XL_DEFINE_PROTO(xkb_x11_state_new_from_device)
	decltype(&_xl_null_fn) _xkb_x11_last_fn = &_xl_null_fn;

protected:
	bool open(Dso &);
	void openAux();

	Dso _handle;
	Dso _x11;
	struct xkb_context *_context = nullptr;
};

} // namespace stappler::xenolith::platform

#endif

#endif /* XENOLITH_PLATFORM_LINUX_XLPLATFORMLINUXXKB_H_ */
