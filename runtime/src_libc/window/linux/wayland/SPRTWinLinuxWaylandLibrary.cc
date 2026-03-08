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

#include <sprt/c/__sprt_assert.h>

namespace sprt::window {

static WaylandLibrary *s_library = nullptr;

}

extern "C" {

#define WL_CLOSURE_MAX_ARGS 20

struct wl_interface wl_display_interface = wl_interface();
struct wl_interface wl_registry_interface = wl_interface();
struct wl_interface wl_compositor_interface = wl_interface();
struct wl_interface wl_output_interface = wl_interface();
struct wl_interface wl_seat_interface = wl_interface();
struct wl_interface wl_surface_interface = wl_interface();
struct wl_interface wl_region_interface = wl_interface();
struct wl_interface wl_callback_interface = wl_interface();
struct wl_interface wl_pointer_interface = wl_interface();
struct wl_interface wl_keyboard_interface = wl_interface();
struct wl_interface wl_touch_interface = wl_interface();
struct wl_interface wl_shm_interface = wl_interface();
struct wl_interface wl_subcompositor_interface = wl_interface();
struct wl_interface wl_subsurface_interface = wl_interface();
struct wl_interface wl_shm_pool_interface = wl_interface();
struct wl_interface wl_buffer_interface = wl_interface();
struct wl_interface wl_data_offer_interface = wl_interface();
struct wl_interface wl_data_source_interface = wl_interface();
struct wl_interface wl_data_device_interface = wl_interface();
struct wl_interface wl_data_device_manager_interface = wl_interface();

struct wl_object {
	const struct wl_interface *interface;
	const void *implementation;
	uint32_t id;
};

struct wl_proxy {
	struct wl_object object;
};

enum wl_arg_type {
	WL_ARG_INT = 'i',
	WL_ARG_UINT = 'u',
	WL_ARG_FIXED = 'f',
	WL_ARG_STRING = 's',
	WL_ARG_OBJECT = 'o',
	WL_ARG_NEW_ID = 'n',
	WL_ARG_ARRAY = 'a',
	WL_ARG_FD = 'h',
};

struct argument_details {
	enum wl_arg_type type;
	int nullable;
};

static const char *get_next_argument(const char *signature, struct argument_details *details) {
	details->nullable = 0;
	for (; *signature; ++signature) {
		switch (*signature) {
		case WL_ARG_INT:
		case WL_ARG_UINT:
		case WL_ARG_FIXED:
		case WL_ARG_STRING:
		case WL_ARG_OBJECT:
		case WL_ARG_NEW_ID:
		case WL_ARG_ARRAY:
		case WL_ARG_FD: details->type = (wl_arg_type)*signature; return signature + 1;
		case '?': details->nullable = 1;
		}
	}
	details->type = (wl_arg_type)'\0';
	return signature;
}

static void wl_argument_from_va_list(const char *signature, union wl_argument *args, int count,
		va_list ap) {
	int i;
	const char *sig_iter;
	struct argument_details arg;

	sig_iter = signature;
	for (i = 0; i < count; i++) {
		sig_iter = get_next_argument(sig_iter, &arg);

		switch (arg.type) {
		case WL_ARG_INT: args[i].i = va_arg(ap, int32_t); break;
		case WL_ARG_UINT: args[i].u = va_arg(ap, uint32_t); break;
		case WL_ARG_FIXED: args[i].f = va_arg(ap, wl_fixed_t); break;
		case WL_ARG_STRING: args[i].s = va_arg(ap, const char *); break;
		case WL_ARG_OBJECT: args[i].o = va_arg(ap, struct wl_object *); break;
		case WL_ARG_NEW_ID: args[i].o = va_arg(ap, struct wl_object *); break;
		case WL_ARG_ARRAY: args[i].a = va_arg(ap, struct wl_array *); break;
		case WL_ARG_FD: args[i].h = va_arg(ap, int32_t); break;
		}
	}
}

struct wl_proxy *wl_proxy_marshal_flags(struct wl_proxy *proxy, uint32_t opcode,
		const struct wl_interface *interface, uint32_t version, uint32_t flags, ...) {
	union wl_argument args[WL_CLOSURE_MAX_ARGS];
	va_list ap;

	va_start(ap, flags);
	wl_argument_from_va_list(proxy->object.interface->methods[opcode].signature, args,
			WL_CLOSURE_MAX_ARGS, ap);
	va_end(ap);

	return wl_proxy_marshal_array_flags(proxy, opcode, interface, version, flags, args);
}

struct wl_display *wl_display_connect(const char *name) {
	sprt_passert(sprt::window::s_library, "Wayland not loaded");
	return sprt::window::s_library->wl_display_connect(name);
}

int wl_display_get_fd(struct wl_display *display) {
	sprt_passert(sprt::window::s_library, "Wayland not loaded");
	return sprt::window::s_library->wl_display_get_fd(display);
}

int wl_display_dispatch(struct wl_display *display) {
	sprt_passert(sprt::window::s_library, "Wayland not loaded");
	return sprt::window::s_library->wl_display_dispatch(display);
}

int wl_display_dispatch_pending(struct wl_display *display) {
	sprt_passert(sprt::window::s_library, "Wayland not loaded");
	return sprt::window::s_library->wl_display_dispatch_pending(display);
}

int wl_display_prepare_read(struct wl_display *display) {
	sprt_passert(sprt::window::s_library, "Wayland not loaded");
	return sprt::window::s_library->wl_display_prepare_read(display);
}

int wl_display_flush(struct wl_display *display) {
	sprt_passert(sprt::window::s_library, "Wayland not loaded");
	return sprt::window::s_library->wl_display_flush(display);
}

int wl_display_read_events(struct wl_display *display) {
	sprt_passert(sprt::window::s_library, "Wayland not loaded");
	return sprt::window::s_library->wl_display_read_events(display);
}

void wl_display_disconnect(struct wl_display *display) {
	sprt_passert(sprt::window::s_library, "Wayland not loaded");
	sprt::window::s_library->wl_display_disconnect(display);
}

struct wl_proxy *wl_proxy_marshal_array_flags(struct wl_proxy *proxy, uint32_t opcode,
		const struct wl_interface *interface, uint32_t version, uint32_t flags,
		union wl_argument *args) {
	sprt_passert(sprt::window::s_library, "Wayland not loaded");
	return sprt::window::s_library->wl_proxy_marshal_array_flags(proxy, opcode, interface, version,
			flags, args);
}

uint32_t wl_proxy_get_version(struct wl_proxy *proxy) {
	sprt_passert(sprt::window::s_library, "Wayland not loaded");
	return sprt::window::s_library->wl_proxy_get_version(proxy);
}

int wl_proxy_add_listener(struct wl_proxy *proxy, void (**implementation)(void), void *data) {
	sprt_passert(sprt::window::s_library, "Wayland not loaded");
	return sprt::window::s_library->wl_proxy_add_listener(proxy, implementation, data);
}

void wl_proxy_set_user_data(struct wl_proxy *proxy, void *user_data) {
	sprt_passert(sprt::window::s_library, "Wayland not loaded");
	return sprt::window::s_library->wl_proxy_set_user_data(proxy, user_data);
}

void *wl_proxy_get_user_data(struct wl_proxy *proxy) {
	sprt_passert(sprt::window::s_library, "Wayland not loaded");
	return sprt::window::s_library->wl_proxy_get_user_data(proxy);
}

void wl_proxy_set_tag(struct wl_proxy *proxy, const char *const *tag) {
	sprt_passert(sprt::window::s_library, "Wayland not loaded");
	return sprt::window::s_library->wl_proxy_set_tag(proxy, tag);
}

const char *const *wl_proxy_get_tag(struct wl_proxy *proxy) {
	sprt_passert(sprt::window::s_library, "Wayland not loaded");
	return sprt::window::s_library->wl_proxy_get_tag(proxy);
}

void wl_proxy_destroy(struct wl_proxy *proxy) {
	sprt_passert(sprt::window::s_library, "Wayland not loaded");
	sprt::window::s_library->wl_proxy_destroy(proxy);
}

int wl_display_roundtrip(struct wl_display *display) {
	sprt_passert(sprt::window::s_library, "Wayland not loaded");
	return sprt::window::s_library->wl_display_roundtrip(display);
}

struct wl_cursor_theme *wl_cursor_theme_load(const char *name, int size, struct wl_shm *shm) {
	sprt_passert(sprt::window::s_library, "Wayland not loaded");
	return sprt::window::s_library->wl_cursor_theme_load(name, size, shm);
}

void wl_cursor_theme_destroy(struct wl_cursor_theme *theme) {
	sprt_passert(sprt::window::s_library, "Wayland not loaded");
	sprt::window::s_library->wl_cursor_theme_destroy(theme);
}

struct wl_cursor *wl_cursor_theme_get_cursor(struct wl_cursor_theme *theme, const char *name) {
	sprt_passert(sprt::window::s_library, "Wayland not loaded");
	return sprt::window::s_library->wl_cursor_theme_get_cursor(theme, name);
}

struct wl_buffer *wl_cursor_image_get_buffer(struct wl_cursor_image *image) {
	sprt_passert(sprt::window::s_library, "Wayland not loaded");
	return sprt::window::s_library->wl_cursor_image_get_buffer(image);
}
}

namespace sprt::window {

WaylandLibrary::~WaylandLibrary() {
	close();
	s_library = nullptr;
}

WaylandLibrary::WaylandLibrary() { s_library = this; }

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

void WaylandLibrary::close() { }

bool WaylandLibrary::ownsProxy(wl_proxy *proxy) {
	auto tag = wl_proxy_get_tag(proxy);
	return tag == &s_XenolithWaylandTag;
}

bool WaylandLibrary::ownsProxy(wl_output *output) { return ownsProxy((struct wl_proxy *)output); }

bool WaylandLibrary::ownsProxy(wl_surface *surface) {
	return ownsProxy((struct wl_proxy *)surface);
}

#define SPRT_LOAD_INTERFACE(handle, Name) const_cast<wl_interface &>(::Name) = *handle.sym<const wl_interface *>(#Name);

bool WaylandLibrary::open(Dso &handle) {
	//::wl_registry_interface = *handle.sym< decltype(::wl_registry_interface) >("wl_registry_interface");
	SPRT_LOAD_INTERFACE(handle, wl_display_interface)
	SPRT_LOAD_INTERFACE(handle, wl_registry_interface)
	SPRT_LOAD_INTERFACE(handle, wl_compositor_interface)
	SPRT_LOAD_INTERFACE(handle, wl_output_interface)
	SPRT_LOAD_INTERFACE(handle, wl_seat_interface)
	SPRT_LOAD_INTERFACE(handle, wl_surface_interface)
	SPRT_LOAD_INTERFACE(handle, wl_region_interface)
	SPRT_LOAD_INTERFACE(handle, wl_callback_interface)
	SPRT_LOAD_INTERFACE(handle, wl_pointer_interface)
	SPRT_LOAD_INTERFACE(handle, wl_keyboard_interface)
	SPRT_LOAD_INTERFACE(handle, wl_touch_interface)
	SPRT_LOAD_INTERFACE(handle, wl_shm_interface)
	SPRT_LOAD_INTERFACE(handle, wl_subcompositor_interface)
	SPRT_LOAD_INTERFACE(handle, wl_subsurface_interface)
	SPRT_LOAD_INTERFACE(handle, wl_shm_pool_interface)
	SPRT_LOAD_INTERFACE(handle, wl_buffer_interface)
	SPRT_LOAD_INTERFACE(handle, wl_data_offer_interface)
	SPRT_LOAD_INTERFACE(handle, wl_data_source_interface)
	SPRT_LOAD_INTERFACE(handle, wl_data_device_interface)
	SPRT_LOAD_INTERFACE(handle, wl_data_device_manager_interface)

	SPRT_LOAD_PROTO(handle, wl_display_connect)
	SPRT_LOAD_PROTO(handle, wl_display_get_fd)
	SPRT_LOAD_PROTO(handle, wl_display_dispatch)
	SPRT_LOAD_PROTO(handle, wl_display_dispatch_pending)
	SPRT_LOAD_PROTO(handle, wl_display_prepare_read)
	SPRT_LOAD_PROTO(handle, wl_display_flush)
	SPRT_LOAD_PROTO(handle, wl_display_read_events)
	SPRT_LOAD_PROTO(handle, wl_display_disconnect)
	SPRT_LOAD_PROTO(handle, wl_proxy_marshal_array_flags)
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

	_cursor = Dso("libwayland-cursor.so");
	if (!openWaylandCursor(_cursor)) {
		_cursor = Dso();
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

} // namespace sprt::window
