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

#include "private/window/linux/SPRTWinLinuxWaylandLibrary.h"
#include "private/window/linux/SPRTWinLinuxDBusLibrary.h"

namespace sprt::window {

WaylandLibrary::~WaylandLibrary() { close(); }

WaylandLibrary::WaylandLibrary() { }

bool WaylandLibrary::init() {
	_handle = Dso("libwayland-client.so");
	if (!_handle) {
		return false;
	}

	if (open(_handle)) {
		return true;
	} else {
		_handle = Dso();
	}
	return false;
}

void WaylandLibrary::close() {
	if (kdeOutputDevice) {
		delete kdeOutputDevice;
		kdeOutputDevice = nullptr;
	}

	if (cursorShape) {
		delete cursorShape;
		cursorShape = nullptr;
	}

	if (xdgDecoration) {
		delete xdgDecoration;
		xdgDecoration = nullptr;
	}

	if (viewporter) {
		delete viewporter;
		viewporter = nullptr;
	}

	if (xdg) {
		delete xdg;
		xdg = nullptr;
	}
}

bool WaylandLibrary::ownsProxy(wl_proxy *proxy) {
	auto tag = wl_proxy_get_tag(proxy);
	return tag == &s_XenolithWaylandTag;
}

bool WaylandLibrary::ownsProxy(wl_output *output) { return ownsProxy((struct wl_proxy *)output); }

bool WaylandLibrary::ownsProxy(wl_surface *surface) {
	return ownsProxy((struct wl_proxy *)surface);
}

bool WaylandLibrary::open(Dso &handle) {
	SPRT_LOAD_PROTO(handle, wl_registry_interface)
	SPRT_LOAD_PROTO(handle, wl_compositor_interface)
	SPRT_LOAD_PROTO(handle, wl_output_interface)
	SPRT_LOAD_PROTO(handle, wl_seat_interface)
	SPRT_LOAD_PROTO(handle, wl_surface_interface)
	SPRT_LOAD_PROTO(handle, wl_region_interface)
	SPRT_LOAD_PROTO(handle, wl_callback_interface)
	SPRT_LOAD_PROTO(handle, wl_pointer_interface)
	SPRT_LOAD_PROTO(handle, wl_keyboard_interface)
	SPRT_LOAD_PROTO(handle, wl_touch_interface)
	SPRT_LOAD_PROTO(handle, wl_shm_interface)
	SPRT_LOAD_PROTO(handle, wl_subcompositor_interface)
	SPRT_LOAD_PROTO(handle, wl_subsurface_interface)
	SPRT_LOAD_PROTO(handle, wl_shm_pool_interface)
	SPRT_LOAD_PROTO(handle, wl_buffer_interface)
	SPRT_LOAD_PROTO(handle, wl_data_offer_interface)
	SPRT_LOAD_PROTO(handle, wl_data_source_interface)
	SPRT_LOAD_PROTO(handle, wl_data_device_interface)
	SPRT_LOAD_PROTO(handle, wl_data_device_manager_interface)

	SPRT_LOAD_PROTO(handle, wl_display_connect)
	SPRT_LOAD_PROTO(handle, wl_display_get_fd)
	SPRT_LOAD_PROTO(handle, wl_display_dispatch)
	SPRT_LOAD_PROTO(handle, wl_display_dispatch_pending)
	SPRT_LOAD_PROTO(handle, wl_display_prepare_read)
	SPRT_LOAD_PROTO(handle, wl_display_flush)
	SPRT_LOAD_PROTO(handle, wl_display_read_events)
	SPRT_LOAD_PROTO(handle, wl_display_disconnect)
	SPRT_LOAD_PROTO(handle, wl_proxy_marshal_flags)
	SPRT_LOAD_PROTO(handle, wl_proxy_get_version)
	SPRT_LOAD_PROTO(handle, wl_proxy_add_listener)
	SPRT_LOAD_PROTO(handle, wl_proxy_set_user_data)
	SPRT_LOAD_PROTO(handle, wl_proxy_get_user_data)
	SPRT_LOAD_PROTO(handle, wl_proxy_set_tag)
	SPRT_LOAD_PROTO(handle, wl_proxy_get_tag)
	SPRT_LOAD_PROTO(handle, wl_proxy_destroy)
	SPRT_LOAD_PROTO(handle, wl_display_roundtrip)

	if (!validateFunctionList(&_wl_first_fn, &_wl_last_fn)) {
		log::vperror(__SPRT_LOCATION, "WaylandLibrary", "Fail to load libwayland-client");
		return false;
	}

	viewporter = new ViewporterInterface(wl_surface_interface);
	xdg = new XdgInterface(wl_output_interface, wl_seat_interface, wl_surface_interface);

	wp_viewporter_interface = &viewporter->wp_viewporter_interface;
	wp_viewport_interface = &viewporter->wp_viewport_interface;

	xdg_wm_base_interface = &xdg->xdg_wm_base_interface;
	xdg_positioner_interface = &xdg->xdg_positioner_interface;
	xdg_surface_interface = &xdg->xdg_surface_interface;
	xdg_toplevel_interface = &xdg->xdg_toplevel_interface;
	xdg_popup_interface = &xdg->xdg_popup_interface;

	xdgDecoration = new XdgDecorationInterface(xdg_toplevel_interface);

	zxdg_decoration_manager_v1_interface = &xdgDecoration->zxdg_decoration_manager_v1_interface;
	zxdg_toplevel_decoration_v1_interface = &xdgDecoration->zxdg_toplevel_decoration_v1_interface;

	cursorShape = new CursorShapeInterface(wl_pointer_interface);

	wp_cursor_shape_manager_v1_interface = &cursorShape->wp_cursor_shape_manager_v1_interface;
	wp_cursor_shape_device_v1_interface = &cursorShape->wp_cursor_shape_device_v1_interface;

	kdeOutputDevice = new KdeOutputDeviceInterface();

	kde_output_device_v2_interface = &kdeOutputDevice->kde_output_device_v2_interface;
	kde_output_device_mode_v2_interface = &kdeOutputDevice->kde_output_device_mode_v2_interface;
	kde_output_order_v1_interface = &kdeOutputDevice->kde_output_order_v1_interface;
	kde_output_management_v2_interface = &kdeOutputDevice->kde_output_management_v2_interface;
	kde_output_configuration_v2_interface = &kdeOutputDevice->kde_output_configuration_v2_interface;

	_cursor = Dso("libwayland-cursor.so");
	if (!openWaylandCursor(_cursor)) {
		_cursor = Dso();
	}

	_decor = Dso("libdecor.so");
	if (!_decor) {
		_decor = Dso("libdecor-0.so");
	}
	if (!openDecor(_decor)) {
		_decor = Dso();
	}

	return true;
}

bool WaylandLibrary::openWaylandCursor(Dso &handle) {
	if (!handle) {
		return false;
	}

	SPRT_LOAD_PROTO(handle, wl_cursor_theme_load)
	SPRT_LOAD_PROTO(handle, wl_cursor_theme_destroy)
	SPRT_LOAD_PROTO(handle, wl_cursor_theme_get_cursor)
	SPRT_LOAD_PROTO(handle, wl_cursor_image_get_buffer)

	if (!validateFunctionList(&_wlcursor_first_fn, &_wlcursor_last_fn)) {
		log::vperror(__SPRT_LOCATION, "WaylandLibrary", "Fail to load libwayland-client");
		return false;
	}

	return true;
}

bool WaylandLibrary::openDecor(Dso &handle) {
	if (!handle) {
		return false;
	}

	SPRT_LOAD_PROTO(handle, libdecor_unref)
	SPRT_LOAD_PROTO(handle, libdecor_new)
	SPRT_LOAD_PROTO(handle, libdecor_get_fd)
	SPRT_LOAD_PROTO(handle, libdecor_dispatch)
	SPRT_LOAD_PROTO(handle, libdecor_decorate)
	SPRT_LOAD_PROTO(handle, libdecor_frame_ref)
	SPRT_LOAD_PROTO(handle, libdecor_frame_unref)
	SPRT_LOAD_PROTO(handle, libdecor_frame_set_visibility)
	SPRT_LOAD_PROTO(handle, libdecor_frame_is_visible)
	SPRT_LOAD_PROTO(handle, libdecor_frame_set_parent)
	SPRT_LOAD_PROTO(handle, libdecor_frame_set_title)
	SPRT_LOAD_PROTO(handle, libdecor_frame_get_title)
	SPRT_LOAD_PROTO(handle, libdecor_frame_set_app_id)
	SPRT_LOAD_PROTO(handle, libdecor_frame_set_capabilities)
	SPRT_LOAD_PROTO(handle, libdecor_frame_unset_capabilities)
	SPRT_LOAD_PROTO(handle, libdecor_frame_has_capability)
	SPRT_LOAD_PROTO(handle, libdecor_frame_show_window_menu)
	SPRT_LOAD_PROTO(handle, libdecor_frame_popup_grab)
	SPRT_LOAD_PROTO(handle, libdecor_frame_popup_ungrab)
	SPRT_LOAD_PROTO(handle, libdecor_frame_translate_coordinate)
	SPRT_LOAD_PROTO(handle, libdecor_frame_set_min_content_size)
	SPRT_LOAD_PROTO(handle, libdecor_frame_set_max_content_size)
	SPRT_LOAD_PROTO(handle, libdecor_frame_get_min_content_size)
	SPRT_LOAD_PROTO(handle, libdecor_frame_get_max_content_size)
	SPRT_LOAD_PROTO(handle, libdecor_frame_resize)
	SPRT_LOAD_PROTO(handle, libdecor_frame_move)
	SPRT_LOAD_PROTO(handle, libdecor_frame_commit)
	SPRT_LOAD_PROTO(handle, libdecor_frame_set_minimized)
	SPRT_LOAD_PROTO(handle, libdecor_frame_set_maximized)
	SPRT_LOAD_PROTO(handle, libdecor_frame_unset_maximized)
	SPRT_LOAD_PROTO(handle, libdecor_frame_set_fullscreen)
	SPRT_LOAD_PROTO(handle, libdecor_frame_unset_fullscreen)
	SPRT_LOAD_PROTO(handle, libdecor_frame_is_floating)
	SPRT_LOAD_PROTO(handle, libdecor_frame_close)
	SPRT_LOAD_PROTO(handle, libdecor_frame_map)
	SPRT_LOAD_PROTO(handle, libdecor_frame_get_xdg_surface)
	SPRT_LOAD_PROTO(handle, libdecor_frame_get_xdg_toplevel)
	SPRT_LOAD_PROTO(handle, libdecor_state_new)
	SPRT_LOAD_PROTO(handle, libdecor_state_free)
	SPRT_LOAD_PROTO(handle, libdecor_configuration_get_content_size)
	SPRT_LOAD_PROTO(handle, libdecor_configuration_get_window_state)

	if (!validateFunctionList(&_libdecor_first_fn, &_libdecor_last_fn)) {
		log::vperror(__SPRT_LOCATION, "WaylandLibrary", "Fail to load libdecor");
		return false;
	}

	return true;
}

} // namespace sprt::window
