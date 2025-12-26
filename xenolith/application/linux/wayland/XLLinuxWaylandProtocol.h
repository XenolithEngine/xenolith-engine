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

#ifndef XENOLITH_APPLICATION_LINUX_WAYLAND_XLLINUXWAYLANDPROTOCOL_H_
#define XENOLITH_APPLICATION_LINUX_WAYLAND_XLLINUXWAYLANDPROTOCOL_H_

#include "XLCommon.h" // IWYU pragma: keep
#include "XLContextInfo.h" // IWYU pragma: keep

#if LINUX

#include "linux/XLLinux.h" // IWYU pragma: keep
#include "linux/XLLinuxXkbLibrary.h" // IWYU pragma: keep
#include "XLCoreInput.h"

#include "linux/thirdparty/wayland-protocols/xdg-shell.h" // IWYU pragma: keep
#include "linux/thirdparty/wayland-protocols/viewporter.h" // IWYU pragma: keep
#include "linux/thirdparty/wayland-protocols/xdg-decoration.h" // IWYU pragma: keep
#include "linux/thirdparty/wayland-protocols/kde-output-device-v2.h" // IWYU pragma: keep
#include "linux/thirdparty/wayland-protocols/kde-output-order-v1.h" // IWYU pragma: keep
#include "linux/thirdparty/wayland-protocols/kde-output-management-v2.h" // IWYU pragma: keep
#include "linux/thirdparty/wayland-protocols/cursor-shape-v1.h" // IWYU pragma: keep

#ifndef XL_WAYLAND_DEBUG
#define XL_WAYLAND_DEBUG 1
#endif

#if XL_WAYLAND_DEBUG
#define XL_WAYLAND_LOG(...) log::source().debug("Wayland", __VA_ARGS__)
#else
#define XL_WAYLAND_LOG(...)
#endif

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

SP_UNUSED static const char *s_XenolithWaylandTag = "org.stappler.xenolith.wayland";

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

struct ViewporterInterface {
	const struct wl_interface *viewporter_types[6];

	const struct wl_message wp_viewporter_requests[2];
	const struct wl_message wp_viewport_requests[3];

	const struct wl_interface wp_viewporter_interface;
	const struct wl_interface wp_viewport_interface;

	ViewporterInterface(const struct wl_interface *wl_surface_interface);
};

struct XdgInterface {
	const struct wl_interface *xdg_shell_types[26];

	const struct wl_message xdg_wm_base_requests[4];
	const struct wl_message xdg_wm_base_events[1];
	const struct wl_message xdg_positioner_requests[10];
	const struct wl_message xdg_surface_requests[5];
	const struct wl_message xdg_surface_events[1];
	const struct wl_message xdg_toplevel_requests[14];
	const struct wl_message xdg_toplevel_events[4];
	const struct wl_message xdg_popup_requests[3];
	const struct wl_message xdg_popup_events[3];

	const struct wl_interface xdg_wm_base_interface;
	const struct wl_interface xdg_positioner_interface;
	const struct wl_interface xdg_surface_interface;
	const struct wl_interface xdg_toplevel_interface;
	const struct wl_interface xdg_popup_interface;

	XdgInterface(const struct wl_interface *wl_output_interface,
			const struct wl_interface *wl_seat_interface,
			const struct wl_interface *wl_surface_interface);
};

struct XdgDecorationInterface {
	const struct wl_interface *xdg_decoration_unstable_v1_types[3];

	const struct wl_message zxdg_decoration_manager_v1_requests[2];
	const struct wl_message zxdg_toplevel_decoration_v1_requests[3];
	const struct wl_message zxdg_toplevel_decoration_v1_events[1];

	const struct wl_interface zxdg_decoration_manager_v1_interface;
	const struct wl_interface zxdg_toplevel_decoration_v1_interface;

	XdgDecorationInterface(const struct wl_interface *xdg_toplevel_interface);
};

struct CursorShapeInterface {
	const struct wl_interface *cursor_shape_v1_types[6];

	const struct wl_message wp_cursor_shape_manager_v1_requests[3];
	const struct wl_message wp_cursor_shape_device_v1_requests[2];

	const struct wl_interface wp_cursor_shape_manager_v1_interface;
	const struct wl_interface wp_cursor_shape_device_v1_interface;

	CursorShapeInterface(const struct wl_interface *wl_pointer_interface);
};

struct KdeOutputDeviceInterface {
	const struct wl_interface *kde_output_device_v2_types[10] = {
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		&kde_output_device_mode_v2_interface,
		&kde_output_device_mode_v2_interface,
	};

	const struct wl_interface *kde_output_order_v1_types[1] = {
		NULL,
	};

	const struct wl_interface *kde_output_management_v2_types[56] = {
		NULL,
		&kde_output_configuration_v2_interface,
		&kde_output_device_v2_interface,
		NULL,
		&kde_output_device_v2_interface,
		&kde_output_device_mode_v2_interface,
		&kde_output_device_v2_interface,
		NULL,
		&kde_output_device_v2_interface,
		NULL,
		NULL,
		&kde_output_device_v2_interface,
		NULL,
		&kde_output_device_v2_interface,
		NULL,
		&kde_output_device_v2_interface,
		NULL,
		&kde_output_device_v2_interface,
		NULL,
		&kde_output_device_v2_interface,
		&kde_output_device_v2_interface,
		NULL,
		&kde_output_device_v2_interface,
		NULL,
		&kde_output_device_v2_interface,
		NULL,
		&kde_output_device_v2_interface,
		NULL,
		&kde_output_device_v2_interface,
		NULL,
		&kde_output_device_v2_interface,
		NULL,
		&kde_output_device_v2_interface,
		NULL,
		NULL,
		NULL,
		&kde_output_device_v2_interface,
		NULL,
		&kde_output_device_v2_interface,
		NULL,
		&kde_output_device_v2_interface,
		NULL,
		&kde_output_device_v2_interface,
		NULL,
		&kde_output_device_v2_interface,
		NULL,
		&kde_output_device_v2_interface,
		NULL,
		&kde_output_device_v2_interface,
		NULL,
		&kde_output_device_v2_interface,
		NULL,
		&kde_output_device_v2_interface,
		NULL,
		&kde_output_device_v2_interface,
		NULL,
	};

	const struct wl_message kde_output_device_v2_events[34] = {
		{"geometry", "iiiiissi", kde_output_device_v2_types + 0},
		{"current_mode", "o", kde_output_device_v2_types + 8},
		{"mode", "n", kde_output_device_v2_types + 9},
		{"done", "", kde_output_device_v2_types + 0},
		{"scale", "f", kde_output_device_v2_types + 0},
		{"edid", "s", kde_output_device_v2_types + 0},
		{"enabled", "i", kde_output_device_v2_types + 0},
		{"uuid", "s", kde_output_device_v2_types + 0},
		{"serial_number", "s", kde_output_device_v2_types + 0},
		{"eisa_id", "s", kde_output_device_v2_types + 0},
		{"capabilities", "u", kde_output_device_v2_types + 0},
		{"overscan", "u", kde_output_device_v2_types + 0},
		{"vrr_policy", "u", kde_output_device_v2_types + 0},
		{"rgb_range", "u", kde_output_device_v2_types + 0},
		{"name", "2s", kde_output_device_v2_types + 0},
		{"high_dynamic_range", "3u", kde_output_device_v2_types + 0},
		{"sdr_brightness", "3u", kde_output_device_v2_types + 0},
		{"wide_color_gamut", "3u", kde_output_device_v2_types + 0},
		{"auto_rotate_policy", "4u", kde_output_device_v2_types + 0},
		{"icc_profile_path", "5s", kde_output_device_v2_types + 0},
		{"brightness_metadata", "6uuu", kde_output_device_v2_types + 0},
		{"brightness_overrides", "6iii", kde_output_device_v2_types + 0},
		{"sdr_gamut_wideness", "6u", kde_output_device_v2_types + 0},
		{"color_profile_source", "7u", kde_output_device_v2_types + 0},
		{"brightness", "8u", kde_output_device_v2_types + 0},
		{"color_power_tradeoff", "10u", kde_output_device_v2_types + 0},
		{"dimming", "11u", kde_output_device_v2_types + 0},
		{"replication_source", "13s", kde_output_device_v2_types + 0},
		{"ddc_ci_allowed", "14u", kde_output_device_v2_types + 0},
		{"max_bits_per_color", "15u", kde_output_device_v2_types + 0},
		{"max_bits_per_color_range", "15uu", kde_output_device_v2_types + 0},
		{"automatic_max_bits_per_color_limit", "15u", kde_output_device_v2_types + 0},
		{"edr_policy", "16u", kde_output_device_v2_types + 0},
		{"sharpness", "17u", kde_output_device_v2_types + 0},
	};

	const struct wl_message kde_output_device_mode_v2_events[4] = {
		{"size", "ii", kde_output_device_v2_types + 0},
		{"refresh", "i", kde_output_device_v2_types + 0},
		{"preferred", "", kde_output_device_v2_types + 0},
		{"removed", "", kde_output_device_v2_types + 0},
	};

	const struct wl_message kde_output_order_v1_requests[1] = {
		{"destroy", "", kde_output_order_v1_types + 0},
	};

	const struct wl_message kde_output_order_v1_events[2] = {
		{"output", "s", kde_output_order_v1_types + 0},
		{"done", "", kde_output_order_v1_types + 0},
	};

	const struct wl_message kde_output_management_v2_requests[1] = {
		{"create_configuration", "n", kde_output_management_v2_types + 1},
	};

	const struct wl_message kde_output_configuration_v2_requests[28]{
		{"enable", "oi", kde_output_management_v2_types + 2},
		{"mode", "oo", kde_output_management_v2_types + 4},
		{"transform", "oi", kde_output_management_v2_types + 6},
		{"position", "oii", kde_output_management_v2_types + 8},
		{"scale", "of", kde_output_management_v2_types + 11},
		{"apply", "", kde_output_management_v2_types + 0},
		{"destroy", "", kde_output_management_v2_types + 0},
		{"overscan", "ou", kde_output_management_v2_types + 13},
		{"set_vrr_policy", "ou", kde_output_management_v2_types + 15},
		{"set_rgb_range", "ou", kde_output_management_v2_types + 17},
		{"set_primary_output", "2o", kde_output_management_v2_types + 19},
		{"set_priority", "3ou", kde_output_management_v2_types + 20},
		{"set_high_dynamic_range", "4ou", kde_output_management_v2_types + 22},
		{"set_sdr_brightness", "4ou", kde_output_management_v2_types + 24},
		{"set_wide_color_gamut", "4ou", kde_output_management_v2_types + 26},
		{"set_auto_rotate_policy", "5ou", kde_output_management_v2_types + 28},
		{"set_icc_profile_path", "6os", kde_output_management_v2_types + 30},
		{"set_brightness_overrides", "7oiii", kde_output_management_v2_types + 32},
		{"set_sdr_gamut_wideness", "7ou", kde_output_management_v2_types + 36},
		{"set_color_profile_source", "8ou", kde_output_management_v2_types + 38},
		{"set_brightness", "9ou", kde_output_management_v2_types + 40},
		{"set_color_power_tradeoff", "10ou", kde_output_management_v2_types + 42},
		{"set_dimming", "11ou", kde_output_management_v2_types + 44},
		{"set_replication_source", "13os", kde_output_management_v2_types + 46},
		{"set_ddc_ci_allowed", "14ou", kde_output_management_v2_types + 48},
		{"set_max_bits_per_color", "15ou", kde_output_management_v2_types + 50},
		{"set_edr_policy", "16ou", kde_output_management_v2_types + 52},
		{"set_sharpness", "17ou", kde_output_management_v2_types + 54},
	};

	const struct wl_message kde_output_configuration_v2_events[3] = {
		{"applied", "", kde_output_management_v2_types + 0},
		{"failed", "", kde_output_management_v2_types + 0},
		{"failure_reason", "12s", kde_output_management_v2_types + 0},
	};

	const struct wl_interface kde_output_device_v2_interface = {
		"kde_output_device_v2",
		17,
		0,
		NULL,
		34,
		kde_output_device_v2_events,
	};

	const struct wl_interface kde_output_device_mode_v2_interface = {
		"kde_output_device_mode_v2",
		1,
		0,
		NULL,
		4,
		kde_output_device_mode_v2_events,
	};

	const struct wl_interface kde_output_order_v1_interface = {
		"kde_output_order_v1",
		1,
		1,
		kde_output_order_v1_requests,
		2,
		kde_output_order_v1_events,
	};

	const struct wl_interface kde_output_management_v2_interface = {
		"kde_output_management_v2",
		17,
		1,
		kde_output_management_v2_requests,
		0,
		NULL,
	};

	const struct wl_interface kde_output_configuration_v2_interface{
		"kde_output_configuration_v2",
		17,
		28,
		kde_output_configuration_v2_requests,
		3,
		kde_output_configuration_v2_events,
	};
};

struct SP_PUBLIC WaylandBuffer : Ref {
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

	core::InputKeyCode keycodes[256] = {core::InputKeyCode::Unknown};

	KeyState();
};

struct DecorationInfo {
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

SP_PUBLIC bool allocateDecorations(WaylandLibrary *wayland, wl_shm *shm, DecorationInfo &);

SP_PUBLIC uint32_t getWaylandCursor(WindowCursor);

SP_PUBLIC std::ostream &operator<<(std::ostream &, WaylandDecorationName);

} // namespace stappler::xenolith::platform

#endif

#endif // XENOLITH_APPLICATION_LINUX_WAYLAND_XLLINUXWAYLANDPROTOCOL_H_
