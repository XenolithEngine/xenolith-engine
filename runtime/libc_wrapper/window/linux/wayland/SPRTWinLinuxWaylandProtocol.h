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

#ifndef CORE_RUNTIME_PRIVATE_WINDOW_LINUX_SPRTWINLINUXWAYLANDPROTOCOL_H_
#define CORE_RUNTIME_PRIVATE_WINDOW_LINUX_SPRTWINLINUXWAYLANDPROTOCOL_H_

#include <sprt/runtime/init.h>

#if SPRT_LINUX

#include <sprt/runtime/ref.h>
#include <sprt/runtime/window/window_info.h>
#include <sprt/runtime/window/input.h>

#include <xkbcommon/xkbcommon.h>

#include <wayland-client-core.h>

extern "C" {

#define WL_DISPLAY_INTERFACE
extern struct wl_interface wl_display_interface;

#define WL_REGISTRY_INTERFACE
extern struct wl_interface wl_registry_interface;

#define WL_COMPOSITOR_INTERFACE
extern struct wl_interface wl_compositor_interface;

#define WL_OUTPUT_INTERFACE
extern struct wl_interface wl_output_interface;

#define WL_SEAT_INTERFACE
extern struct wl_interface wl_seat_interface;

#define WL_SURFACE_INTERFACE
extern struct wl_interface wl_surface_interface;

#define WL_REGION_INTERFACE
extern struct wl_interface wl_region_interface;

#define WL_CALLBACK_INTERFACE
extern struct wl_interface wl_callback_interface;

#define WL_POINTER_INTERFACE
extern struct wl_interface wl_pointer_interface;

#define WL_KEYBOARD_INTERFACE
extern struct wl_interface wl_keyboard_interface;

#define WL_TOUCH_INTERFACE
extern struct wl_interface wl_touch_interface;

#define WL_SHM_INTERFACE
extern struct wl_interface wl_shm_interface;

#define WL_SUBCOMPOSITOR_INTERFACE
extern struct wl_interface wl_subcompositor_interface;

#define WL_SUBSURFACE_INTERFACE
extern struct wl_interface wl_subsurface_interface;

#define WL_SHM_POOL_INTERFACE
extern struct wl_interface wl_shm_pool_interface;

#define WL_BUFFER_INTERFACE
extern struct wl_interface wl_buffer_interface;

#define WL_DATA_OFFER_INTERFACE
extern struct wl_interface wl_data_offer_interface;

#define WL_DATA_SOURCE_INTERFACE
extern struct wl_interface wl_data_source_interface;

#define WL_DATA_DEVICE_INTERFACE
extern struct wl_interface wl_data_device_interface;

#define WL_DATA_DEVICE_MANAGER_INTERFACE
extern struct wl_interface wl_data_device_manager_interface;
}

#include <wayland-client-protocol.h>
#include <wayland-cursor.h>

#include <wayland-protocols/xdg-shell.h>
#include <wayland-protocols/viewporter.h>
#include <wayland-protocols/xdg-decoration.h>
#include <wayland-protocols/kde-output-device-v2.h>
#include <wayland-protocols/kde-output-order-v1.h>
#include <wayland-protocols/kde-output-management-v2.h>
#include <wayland-protocols/cursor-shape.h>

//#define XL_WAYLAND_DEBUG 1

#if defined(XL_WAYLAND_DEBUG) && defined(DEBUG)
#define XL_WAYLAND_DEBUG 1
#endif

#if XL_WAYLAND_DEBUG
#define XL_WAYLAND_LOG(...) log::vpdebug(__SPRT_LOCATION, "Wayland", __VA_ARGS__)
#else
#define XL_WAYLAND_LOG(...)
#endif

namespace sprt::window {

SPRT_UNUSED static const char *s_XenolithWaylandTag = "org.stappler.xenolith.wayland";

class WaylandLibrary;

enum class WaylandDecorationName {
	RightSide,
	TopRightCorner,
	TopSide,
	TopLeftCorner,
	BottomRightCorner,
	BottomSide,
	BottomLeftCorner,
	LeftSide,
	HeaderLeft,
	HeaderRight,
	HeaderCenter,
	HeaderBottom,
	IconClose,
	IconMaximize,
	IconMinimize,
	IconRestore
};

struct SPRT_API WaylandBuffer : Ref {
	virtual ~WaylandBuffer();

	bool init(WaylandLibrary *lib, wl_shm_pool *wl_shm_pool, int32_t offset, int32_t width,
			int32_t height, int32_t stride, uint32_t format);

	Rc<WaylandLibrary> wayland;
	wl_buffer *buffer = nullptr;
	uint32_t width = 0;
	uint32_t height = 0;
};

struct ShadowBuffers {
	Rc<WaylandBuffer> top;
	Rc<WaylandBuffer> left;
	Rc<WaylandBuffer> bottom;
	Rc<WaylandBuffer> right;
	Rc<WaylandBuffer> topLeft;
	Rc<WaylandBuffer> topRight;
	Rc<WaylandBuffer> bottomLeft;
	Rc<WaylandBuffer> bottomRight;

	Rc<WaylandBuffer> topActive;
	Rc<WaylandBuffer> leftActive;
	Rc<WaylandBuffer> bottomActive;
	Rc<WaylandBuffer> rightActive;
	Rc<WaylandBuffer> topLeftActive;
	Rc<WaylandBuffer> topRightActive;
	Rc<WaylandBuffer> bottomLeftActive;
	Rc<WaylandBuffer> bottomRightActive;

	Rc<WaylandBuffer> headerLeft;
	Rc<WaylandBuffer> headerLeftActive;
	Rc<WaylandBuffer> headerRight;
	Rc<WaylandBuffer> headerRightActive;
	Rc<WaylandBuffer> headerLightCenter;
	Rc<WaylandBuffer> headerLightCenterActive;

	Rc<WaylandBuffer> headerDarkLeft;
	Rc<WaylandBuffer> headerDarkLeftActive;
	Rc<WaylandBuffer> headerDarkRight;
	Rc<WaylandBuffer> headerDarkRightActive;
	Rc<WaylandBuffer> headerDarkCenter;
	Rc<WaylandBuffer> headerDarkCenterActive;

	Rc<WaylandBuffer> iconClose;
	Rc<WaylandBuffer> iconMaximize;
	Rc<WaylandBuffer> iconMinimize;
	Rc<WaylandBuffer> iconRestore;

	Rc<WaylandBuffer> iconCloseActive;
	Rc<WaylandBuffer> iconMaximizeActive;
	Rc<WaylandBuffer> iconMinimizeActive;
	Rc<WaylandBuffer> iconRestoreActive;
};

struct KeyState {
	xkb_mod_index_t controlIndex = 0;
	xkb_mod_index_t altIndex = 0;
	xkb_mod_index_t shiftIndex = 0;
	xkb_mod_index_t superIndex = 0;
	xkb_mod_index_t capsLockIndex = 0;
	xkb_mod_index_t numLockIndex = 0;

	int32_t keyRepeatRate = 0;
	int32_t keyRepeatDelay = 0;
	int32_t keyRepeatInterval = 0;
	uint32_t modsDepressed = 0;
	uint32_t modsLatched = 0;
	uint32_t modsLocked = 0;

	InputKeyCode keycodes[256] = {InputKeyCode::Unknown};

	KeyState();
};

struct WaylandDecorationInfo {
	ShadowBuffers *ret = nullptr;
	Color3B headerLight;
	Color3B headerLightActive;
	Color3B headerDark;
	Color3B headerDarkActive;
	uint32_t width;
	uint32_t inset;
	float shadowMin = 0.0f;
	float shadowMax = 0.0f;
};

SPRT_API bool allocateDecorations(WaylandLibrary *wayland, wl_shm *shm, WaylandDecorationInfo &);

SPRT_API uint32_t getWaylandCursor(WindowCursor);

} // namespace sprt::window

#endif

#endif // CORE_RUNTIME_PRIVATE_WINDOW_LINUX_SPRTWINLINUXWAYLANDPROTOCOL_H_
