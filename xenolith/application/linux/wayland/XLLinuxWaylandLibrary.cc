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

#include "XLLinuxWaylandLibrary.h"
#include "linux/dbus/XLLinuxDBusLibrary.h"
#include "XLLinuxWaylandWindow.h"
#include "linux/wayland/XLLinuxWaylandProtocol.h"
#include <wayland-client-protocol.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

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
	XL_LOAD_PROTO(handle, wl_registry_interface)
	XL_LOAD_PROTO(handle, wl_compositor_interface)
	XL_LOAD_PROTO(handle, wl_output_interface)
	XL_LOAD_PROTO(handle, wl_seat_interface)
	XL_LOAD_PROTO(handle, wl_surface_interface)
	XL_LOAD_PROTO(handle, wl_region_interface)
	XL_LOAD_PROTO(handle, wl_callback_interface)
	XL_LOAD_PROTO(handle, wl_pointer_interface)
	XL_LOAD_PROTO(handle, wl_keyboard_interface)
	XL_LOAD_PROTO(handle, wl_touch_interface)
	XL_LOAD_PROTO(handle, wl_shm_interface)
	XL_LOAD_PROTO(handle, wl_subcompositor_interface)
	XL_LOAD_PROTO(handle, wl_subsurface_interface)
	XL_LOAD_PROTO(handle, wl_shm_pool_interface)
	XL_LOAD_PROTO(handle, wl_buffer_interface)
	XL_LOAD_PROTO(handle, wl_data_offer_interface)
	XL_LOAD_PROTO(handle, wl_data_source_interface)
	XL_LOAD_PROTO(handle, wl_data_device_interface)
	XL_LOAD_PROTO(handle, wl_data_device_manager_interface)

	XL_LOAD_PROTO(handle, wl_display_connect)
	XL_LOAD_PROTO(handle, wl_display_get_fd)
	XL_LOAD_PROTO(handle, wl_display_dispatch)
	XL_LOAD_PROTO(handle, wl_display_dispatch_pending)
	XL_LOAD_PROTO(handle, wl_display_prepare_read)
	XL_LOAD_PROTO(handle, wl_display_flush)
	XL_LOAD_PROTO(handle, wl_display_read_events)
	XL_LOAD_PROTO(handle, wl_display_disconnect)
	XL_LOAD_PROTO(handle, wl_proxy_marshal_flags)
	XL_LOAD_PROTO(handle, wl_proxy_get_version)
	XL_LOAD_PROTO(handle, wl_proxy_add_listener)
	XL_LOAD_PROTO(handle, wl_proxy_set_user_data)
	XL_LOAD_PROTO(handle, wl_proxy_get_user_data)
	XL_LOAD_PROTO(handle, wl_proxy_set_tag)
	XL_LOAD_PROTO(handle, wl_proxy_get_tag)
	XL_LOAD_PROTO(handle, wl_proxy_destroy)
	XL_LOAD_PROTO(handle, wl_display_roundtrip)

	if (!validateFunctionList(&_wl_first_fn, &_wl_last_fn)) {
		log::source().error("WaylandLibrary", "Fail to load libwayland-client");
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

	XL_LOAD_PROTO(handle, wl_cursor_theme_load)
	XL_LOAD_PROTO(handle, wl_cursor_theme_destroy)
	XL_LOAD_PROTO(handle, wl_cursor_theme_get_cursor)
	XL_LOAD_PROTO(handle, wl_cursor_image_get_buffer)

	if (!validateFunctionList(&_wlcursor_first_fn, &_wlcursor_last_fn)) {
		log::source().error("WaylandLibrary", "Fail to load libwayland-client");
		return false;
	}

	return true;
}

bool WaylandLibrary::openDecor(Dso &handle) {
	if (!handle) {
		return false;
	}

	XL_LOAD_PROTO(handle, libdecor_unref)
	XL_LOAD_PROTO(handle, libdecor_new)
	XL_LOAD_PROTO(handle, libdecor_get_fd)
	XL_LOAD_PROTO(handle, libdecor_dispatch)
	XL_LOAD_PROTO(handle, libdecor_decorate)
	XL_LOAD_PROTO(handle, libdecor_frame_ref)
	XL_LOAD_PROTO(handle, libdecor_frame_unref)
	XL_LOAD_PROTO(handle, libdecor_frame_set_visibility)
	XL_LOAD_PROTO(handle, libdecor_frame_is_visible)
	XL_LOAD_PROTO(handle, libdecor_frame_set_parent)
	XL_LOAD_PROTO(handle, libdecor_frame_set_title)
	XL_LOAD_PROTO(handle, libdecor_frame_get_title)
	XL_LOAD_PROTO(handle, libdecor_frame_set_app_id)
	XL_LOAD_PROTO(handle, libdecor_frame_set_capabilities)
	XL_LOAD_PROTO(handle, libdecor_frame_unset_capabilities)
	XL_LOAD_PROTO(handle, libdecor_frame_has_capability)
	XL_LOAD_PROTO(handle, libdecor_frame_show_window_menu)
	XL_LOAD_PROTO(handle, libdecor_frame_popup_grab)
	XL_LOAD_PROTO(handle, libdecor_frame_popup_ungrab)
	XL_LOAD_PROTO(handle, libdecor_frame_translate_coordinate)
	XL_LOAD_PROTO(handle, libdecor_frame_set_min_content_size)
	XL_LOAD_PROTO(handle, libdecor_frame_set_max_content_size)
	XL_LOAD_PROTO(handle, libdecor_frame_get_min_content_size)
	XL_LOAD_PROTO(handle, libdecor_frame_get_max_content_size)
	XL_LOAD_PROTO(handle, libdecor_frame_resize)
	XL_LOAD_PROTO(handle, libdecor_frame_move)
	XL_LOAD_PROTO(handle, libdecor_frame_commit)
	XL_LOAD_PROTO(handle, libdecor_frame_set_minimized)
	XL_LOAD_PROTO(handle, libdecor_frame_set_maximized)
	XL_LOAD_PROTO(handle, libdecor_frame_unset_maximized)
	XL_LOAD_PROTO(handle, libdecor_frame_set_fullscreen)
	XL_LOAD_PROTO(handle, libdecor_frame_unset_fullscreen)
	XL_LOAD_PROTO(handle, libdecor_frame_is_floating)
	XL_LOAD_PROTO(handle, libdecor_frame_close)
	XL_LOAD_PROTO(handle, libdecor_frame_map)
	XL_LOAD_PROTO(handle, libdecor_frame_get_xdg_surface)
	XL_LOAD_PROTO(handle, libdecor_frame_get_xdg_toplevel)
	XL_LOAD_PROTO(handle, libdecor_state_new)
	XL_LOAD_PROTO(handle, libdecor_state_free)
	XL_LOAD_PROTO(handle, libdecor_configuration_get_content_size)
	XL_LOAD_PROTO(handle, libdecor_configuration_get_window_state)

	if (!validateFunctionList(&_libdecor_first_fn, &_libdecor_last_fn)) {
		log::source().error("WaylandLibrary", "Fail to load libdecor");
		return false;
	}

	return true;
}

} // namespace stappler::xenolith::platform
