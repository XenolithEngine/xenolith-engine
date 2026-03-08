/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
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

#ifndef CORE_RUNTIME_PRIVATE_WINDOW_LINUX_SPRTWINLINUXWAYLANDLIBRARY_H_
#define CORE_RUNTIME_PRIVATE_WINDOW_LINUX_SPRTWINLINUXWAYLANDLIBRARY_H_

#include <sprt/runtime/init.h>

#if SPRT_LINUX

#include "SPRTWinLinuxWaylandProtocol.h"
#include "private/SPRTDso.h"

namespace sprt::window {

class SPRT_API WaylandLibrary : public Ref {
public:
	static constexpr uint32_t wl_compositor_version_supported = 6;
	static constexpr uint32_t wl_subcompositor_version_supported = 1;
	static constexpr uint32_t wl_output_version_supported = 4;
	static constexpr uint32_t wp_viewporter_version_supported = 1;
	static constexpr uint32_t xdg_wm_base_version_supported = 7;
	static constexpr uint32_t wl_shm_version_supported = 2;
	static constexpr uint32_t wl_seat_version_supported = 10;
	static constexpr uint32_t zxdg_decoration_manager_v1_version_supported = 2;
	static constexpr uint32_t kde_output_device_v2_version_supported = 20;
	static constexpr uint32_t kde_output_order_v1_version_supported = 1;
	static constexpr uint32_t kde_output_management_v2_version_supported = 19;
	static constexpr uint32_t wp_cursor_shape_manager_v1_version_supported = 1;
	static constexpr uint32_t wl_data_device_manager_version_supported = 3;

	virtual ~WaylandLibrary();
	WaylandLibrary();

	bool init();
	void close();

	bool ownsProxy(wl_proxy *);
	bool ownsProxy(wl_output *);
	bool ownsProxy(wl_surface *);

	decltype(&_sprt_null_fn) _wl_first_fn = &_sprt_null_fn;
	SPRT_DEFINE_PROTO(wl_display_connect)
	SPRT_DEFINE_PROTO(wl_display_get_fd)
	SPRT_DEFINE_PROTO(wl_display_dispatch)
	SPRT_DEFINE_PROTO(wl_display_dispatch_pending)
	SPRT_DEFINE_PROTO(wl_display_prepare_read)
	SPRT_DEFINE_PROTO(wl_display_flush)
	SPRT_DEFINE_PROTO(wl_display_read_events)
	SPRT_DEFINE_PROTO(wl_display_disconnect)
	SPRT_DEFINE_PROTO(wl_proxy_marshal_array_flags)
	SPRT_DEFINE_PROTO(wl_proxy_marshal_flags)
	SPRT_DEFINE_PROTO(wl_proxy_get_version)
	SPRT_DEFINE_PROTO(wl_proxy_add_listener)
	SPRT_DEFINE_PROTO(wl_proxy_set_user_data)
	SPRT_DEFINE_PROTO(wl_proxy_get_user_data)
	SPRT_DEFINE_PROTO(wl_proxy_set_tag)
	SPRT_DEFINE_PROTO(wl_proxy_get_tag)
	SPRT_DEFINE_PROTO(wl_proxy_destroy)
	SPRT_DEFINE_PROTO(wl_display_roundtrip)
	decltype(&_sprt_null_fn) _wl_last_fn = &_sprt_null_fn;

	decltype(&_sprt_null_fn) _wlcursor_first_fn = &_sprt_null_fn;
	SPRT_DEFINE_PROTO(wl_cursor_theme_load)
	SPRT_DEFINE_PROTO(wl_cursor_theme_destroy)
	SPRT_DEFINE_PROTO(wl_cursor_theme_get_cursor)
	SPRT_DEFINE_PROTO(wl_cursor_image_get_buffer)
	decltype(&_sprt_null_fn) _wlcursor_last_fn = &_sprt_null_fn;

protected:
	bool open(Dso &);
	bool openWaylandCursor(Dso &);

	Dso _handle;
	Dso _cursor;
};

} // namespace sprt::window

#endif

#endif // CORE_RUNTIME_PRIVATE_WINDOW_LINUX_SPRTWINLINUXWAYLANDLIBRARY_H_
