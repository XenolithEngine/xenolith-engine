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

#ifndef XENOLITH_APPLICATION_LINUX_XCB_XLLINUXXCBLIBRARY_H_
#define XENOLITH_APPLICATION_LINUX_XCB_XLLINUXXCBLIBRARY_H_

#include "XLCommon.h" // IWYU pragma: keep

#if LINUX

#include "linux/XLLinux.h"

#include <xcb/xcb.h>
#include <xcb/randr.h>
#include <xcb/sync.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xcb_cursor.h>
#include <xcb/xfixes.h>
#include <xcb/shape.h>
#include <xcb/xcb_errors.h>
#include <xcb/xcb_icccm.h>

#define explicit _explicit;
#include <xcb/xkb.h>
#undef explicit

#define MWM_HINTS_FUNCTIONS	(1L << 0)
#define MWM_HINTS_DECORATIONS	(1L << 1)
#define MWM_HINTS_INPUT_MODE	(1L << 2)
#define MWM_HINTS_STATUS	(1L << 3)

#define MWM_FUNC_ALL		(1L << 0)
#define MWM_FUNC_RESIZE		(1L << 1)
#define MWM_FUNC_MOVE		(1L << 2)
#define MWM_FUNC_MINIMIZE	(1L << 3)
#define MWM_FUNC_MAXIMIZE	(1L << 4)
#define MWM_FUNC_CLOSE		(1L << 5)

#define MWM_DECOR_ALL		(1L << 0)
#define MWM_DECOR_BORDER	(1L << 1)
#define MWM_DECOR_RESIZEH	(1L << 2)
#define MWM_DECOR_TITLE		(1L << 3)
#define MWM_DECOR_MENU		(1L << 4)
#define MWM_DECOR_MINIMIZE	(1L << 5)
#define MWM_DECOR_MAXIMIZE	(1L << 6)

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

class SP_PUBLIC XcbLibrary : public Ref {
public:
	static constexpr int RANDR_MAJOR_VERSION = XCB_RANDR_MAJOR_VERSION;
	static constexpr int RANDR_MINOR_VERSION = XCB_RANDR_MINOR_VERSION;
	static constexpr int XFIXES_MAJOR_VERSION = XCB_XFIXES_MAJOR_VERSION;
	static constexpr int XFIXES_MINOR_VERSION = XCB_XFIXES_MINOR_VERSION;
	static constexpr int SHAPE_MAJOR_VERSION = XCB_SHAPE_MAJOR_VERSION;
	static constexpr int SHAPE_MINOR_VERSION = XCB_SHAPE_MINOR_VERSION;

	static XcbLibrary *getInstance();

	XcbLibrary() { }

	virtual ~XcbLibrary();

	bool init();

	bool open(Dso &handle);
	void close();

	bool hasRandr() const;
	bool hasKeysyms() const;
	bool hasXkb() const;
	bool hasSync() const;
	bool hasXfixes() const;
	bool hasShape() const;
	bool hasErrors() const;

	decltype(&_xl_null_fn) _xcb_first_fn = &_xl_null_fn;
	XL_DEFINE_PROTO(xcb_grab_server_checked)
	XL_DEFINE_PROTO(xcb_grab_server)
	XL_DEFINE_PROTO(xcb_ungrab_server_checked)
	XL_DEFINE_PROTO(xcb_ungrab_server)
	XL_DEFINE_PROTO(xcb_discard_reply)
	XL_DEFINE_PROTO(xcb_discard_reply64)
	XL_DEFINE_PROTO(xcb_connect)
	XL_DEFINE_PROTO(xcb_get_maximum_request_length)
	XL_DEFINE_PROTO(xcb_get_setup)
	XL_DEFINE_PROTO(xcb_setup_roots_iterator)
	XL_DEFINE_PROTO(xcb_screen_next)
	XL_DEFINE_PROTO(xcb_connection_has_error)
	XL_DEFINE_PROTO(xcb_get_file_descriptor)
	XL_DEFINE_PROTO(xcb_generate_id)
	XL_DEFINE_PROTO(xcb_flush)
	XL_DEFINE_PROTO(xcb_disconnect)
	XL_DEFINE_PROTO(xcb_poll_for_event)
	XL_DEFINE_PROTO(xcb_send_event)
	XL_DEFINE_PROTO(xcb_get_extension_data)
	XL_DEFINE_PROTO(xcb_map_window)
	XL_DEFINE_PROTO(xcb_unmap_window)
	XL_DEFINE_PROTO(xcb_create_window)
	XL_DEFINE_PROTO(xcb_destroy_window)
	XL_DEFINE_PROTO(xcb_configure_window)
	XL_DEFINE_PROTO(xcb_change_window_attributes)
	XL_DEFINE_PROTO(xcb_create_colormap)
	XL_DEFINE_PROTO(xcb_free_colormap)
	XL_DEFINE_PROTO(xcb_create_pixmap)
	XL_DEFINE_PROTO(xcb_free_pixmap)
	XL_DEFINE_PROTO(xcb_create_gc)
	XL_DEFINE_PROTO(xcb_change_gc)
	XL_DEFINE_PROTO(xcb_free_gc)
	XL_DEFINE_PROTO(xcb_poly_fill_rectangle)
	XL_DEFINE_PROTO(xcb_poly_fill_arc)
	XL_DEFINE_PROTO(xcb_put_image)
	XL_DEFINE_PROTO(xcb_copy_area)
	XL_DEFINE_PROTO(xcb_delete_property)
	XL_DEFINE_PROTO(xcb_change_property)
	XL_DEFINE_PROTO(xcb_intern_atom)
	XL_DEFINE_PROTO(xcb_intern_atom_unchecked)
	XL_DEFINE_PROTO(xcb_intern_atom_reply)
	XL_DEFINE_PROTO(xcb_grab_pointer)
	XL_DEFINE_PROTO(xcb_ungrab_pointer)

	XL_DEFINE_PROTO(xcb_screen_allowed_depths_iterator)
	XL_DEFINE_PROTO(xcb_depth_visuals_iterator)
	XL_DEFINE_PROTO(xcb_visualtype_next)
	XL_DEFINE_PROTO(xcb_depth_next)

	XL_DEFINE_PROTO(xcb_get_property_reply)
	XL_DEFINE_PROTO(xcb_get_property)
	XL_DEFINE_PROTO(xcb_get_property_unchecked)
	XL_DEFINE_PROTO(xcb_get_property_value)
	XL_DEFINE_PROTO(xcb_get_property_value_length)
	XL_DEFINE_PROTO(xcb_get_modifier_mapping_unchecked)
	XL_DEFINE_PROTO(xcb_get_modifier_mapping_reply)
	XL_DEFINE_PROTO(xcb_get_modifier_mapping_keycodes)
	XL_DEFINE_PROTO(xcb_convert_selection)
	XL_DEFINE_PROTO(xcb_set_selection_owner)
	XL_DEFINE_PROTO(xcb_get_selection_owner);
	XL_DEFINE_PROTO(xcb_get_selection_owner_reply)
	XL_DEFINE_PROTO(xcb_get_keyboard_mapping)
	XL_DEFINE_PROTO(xcb_get_keyboard_mapping_reply)
	XL_DEFINE_PROTO(xcb_get_atom_name)
	XL_DEFINE_PROTO(xcb_get_atom_name_unchecked)
	XL_DEFINE_PROTO(xcb_get_atom_name_name)
	XL_DEFINE_PROTO(xcb_get_atom_name_name_length)
	XL_DEFINE_PROTO(xcb_get_atom_name_name_end)
	XL_DEFINE_PROTO(xcb_get_atom_name_reply)
	XL_DEFINE_PROTO(xcb_request_check)
	XL_DEFINE_PROTO(xcb_open_font_checked)
	XL_DEFINE_PROTO(xcb_create_glyph_cursor)
	XL_DEFINE_PROTO(xcb_create_gc_checked)
	XL_DEFINE_PROTO(xcb_free_cursor)
	XL_DEFINE_PROTO(xcb_close_font_checked)

	// this function was not publicly exposed, but it's used in macros and inlines
	void *(*xcb_wait_for_reply)(xcb_connection_t *c, unsigned int request,
			xcb_generic_error_t **e) = nullptr;

	decltype(&_xl_null_fn) _xcb_last_fn = &_xl_null_fn;


	decltype(&_xl_null_fn) _xcb_randr_first_fn = &_xl_null_fn;
	XL_DEFINE_PROTO(xcb_randr_id)
	XL_DEFINE_PROTO(xcb_randr_select_input)
	XL_DEFINE_PROTO(xcb_randr_select_input_checked)
	XL_DEFINE_PROTO(xcb_randr_query_version)
	XL_DEFINE_PROTO(xcb_randr_query_version_reply)
	XL_DEFINE_PROTO(xcb_randr_get_screen_info)
	XL_DEFINE_PROTO(xcb_randr_get_screen_info_unchecked)
	XL_DEFINE_PROTO(xcb_randr_get_screen_info_reply)
	XL_DEFINE_PROTO(xcb_randr_get_screen_info_sizes)
	XL_DEFINE_PROTO(xcb_randr_get_screen_info_sizes_length)
	XL_DEFINE_PROTO(xcb_randr_get_screen_info_sizes_iterator)
	XL_DEFINE_PROTO(xcb_randr_get_screen_info_rates_length)
	XL_DEFINE_PROTO(xcb_randr_get_screen_info_rates_iterator)
	XL_DEFINE_PROTO(xcb_randr_refresh_rates_next)
	XL_DEFINE_PROTO(xcb_randr_refresh_rates_end)
	XL_DEFINE_PROTO(xcb_randr_refresh_rates_rates)
	XL_DEFINE_PROTO(xcb_randr_refresh_rates_rates_length)
	XL_DEFINE_PROTO(xcb_randr_add_output_mode_checked)
	XL_DEFINE_PROTO(xcb_randr_add_output_mode)
	XL_DEFINE_PROTO(xcb_randr_delete_output_mode_checked)
	XL_DEFINE_PROTO(xcb_randr_delete_output_mode)
	XL_DEFINE_PROTO(xcb_randr_get_screen_resources)
	XL_DEFINE_PROTO(xcb_randr_get_screen_resources_unchecked)
	XL_DEFINE_PROTO(xcb_randr_get_screen_resources_crtcs)
	XL_DEFINE_PROTO(xcb_randr_get_screen_resources_crtcs_length)
	XL_DEFINE_PROTO(xcb_randr_get_screen_resources_crtcs_end)
	XL_DEFINE_PROTO(xcb_randr_get_screen_resources_outputs)
	XL_DEFINE_PROTO(xcb_randr_get_screen_resources_outputs_length)
	XL_DEFINE_PROTO(xcb_randr_get_screen_resources_outputs_end)
	XL_DEFINE_PROTO(xcb_randr_get_screen_resources_modes)
	XL_DEFINE_PROTO(xcb_randr_get_screen_resources_modes_length)
	XL_DEFINE_PROTO(xcb_randr_get_screen_resources_modes_iterator)
	XL_DEFINE_PROTO(xcb_randr_get_screen_resources_names)
	XL_DEFINE_PROTO(xcb_randr_get_screen_resources_names_length)
	XL_DEFINE_PROTO(xcb_randr_get_screen_resources_names_end)
	XL_DEFINE_PROTO(xcb_randr_get_screen_resources_reply)
	XL_DEFINE_PROTO(xcb_randr_get_screen_resources_current)
	XL_DEFINE_PROTO(xcb_randr_get_screen_resources_current_unchecked)
	XL_DEFINE_PROTO(xcb_randr_get_screen_resources_current_reply)
	XL_DEFINE_PROTO(xcb_randr_get_screen_resources_current_outputs)
	XL_DEFINE_PROTO(xcb_randr_get_screen_resources_current_outputs_length)
	XL_DEFINE_PROTO(xcb_randr_get_screen_resources_current_modes)
	XL_DEFINE_PROTO(xcb_randr_get_screen_resources_current_modes_length)
	XL_DEFINE_PROTO(xcb_randr_get_screen_resources_current_names)
	XL_DEFINE_PROTO(xcb_randr_get_screen_resources_current_names_length)
	XL_DEFINE_PROTO(xcb_randr_get_screen_resources_current_crtcs)
	XL_DEFINE_PROTO(xcb_randr_get_screen_resources_current_crtcs_length)
	XL_DEFINE_PROTO(xcb_randr_list_output_properties)
	XL_DEFINE_PROTO(xcb_randr_list_output_properties_unchecked)
	XL_DEFINE_PROTO(xcb_randr_list_output_properties_atoms)
	XL_DEFINE_PROTO(xcb_randr_list_output_properties_atoms_length)
	XL_DEFINE_PROTO(xcb_randr_list_output_properties_atoms_end)
	XL_DEFINE_PROTO(xcb_randr_list_output_properties_reply)
	XL_DEFINE_PROTO(xcb_randr_get_output_primary)
	XL_DEFINE_PROTO(xcb_randr_get_output_primary_unchecked)
	XL_DEFINE_PROTO(xcb_randr_get_output_primary_reply)
	XL_DEFINE_PROTO(xcb_randr_get_output_info)
	XL_DEFINE_PROTO(xcb_randr_get_output_info_unchecked)
	XL_DEFINE_PROTO(xcb_randr_get_output_info_reply)
	XL_DEFINE_PROTO(xcb_randr_get_output_info_crtcs)
	XL_DEFINE_PROTO(xcb_randr_get_output_info_crtcs_length)
	XL_DEFINE_PROTO(xcb_randr_get_output_info_crtcs_end)
	XL_DEFINE_PROTO(xcb_randr_get_output_info_modes)
	XL_DEFINE_PROTO(xcb_randr_get_output_info_modes_length)
	XL_DEFINE_PROTO(xcb_randr_get_output_info_name)
	XL_DEFINE_PROTO(xcb_randr_get_output_info_name_length)
	XL_DEFINE_PROTO(xcb_randr_get_output_property)
	XL_DEFINE_PROTO(xcb_randr_get_output_property_unchecked)
	XL_DEFINE_PROTO(xcb_randr_get_output_property_data)
	XL_DEFINE_PROTO(xcb_randr_get_output_property_data_length)
	XL_DEFINE_PROTO(xcb_randr_get_output_property_data_end)
	XL_DEFINE_PROTO(xcb_randr_get_output_property_reply)
	XL_DEFINE_PROTO(xcb_randr_get_crtc_info)
	XL_DEFINE_PROTO(xcb_randr_get_crtc_info_unchecked)
	XL_DEFINE_PROTO(xcb_randr_get_crtc_info_reply)
	XL_DEFINE_PROTO(xcb_randr_get_crtc_info_outputs)
	XL_DEFINE_PROTO(xcb_randr_get_crtc_info_outputs_length)
	XL_DEFINE_PROTO(xcb_randr_get_crtc_info_possible)
	XL_DEFINE_PROTO(xcb_randr_get_crtc_info_possible_length)
	XL_DEFINE_PROTO(xcb_randr_set_screen_size_checked)
	XL_DEFINE_PROTO(xcb_randr_set_screen_size)
	XL_DEFINE_PROTO(xcb_randr_set_crtc_config)
	XL_DEFINE_PROTO(xcb_randr_set_crtc_config_unchecked)
	XL_DEFINE_PROTO(xcb_randr_set_crtc_config_reply)
	XL_DEFINE_PROTO(xcb_randr_get_crtc_transform)
	XL_DEFINE_PROTO(xcb_randr_get_crtc_transform_unchecked)
	XL_DEFINE_PROTO(xcb_randr_get_crtc_transform_reply)
	XL_DEFINE_PROTO(xcb_randr_get_crtc_transform_current_filter_name)
	XL_DEFINE_PROTO(xcb_randr_get_crtc_transform_current_filter_name_length)
	XL_DEFINE_PROTO(xcb_randr_get_crtc_transform_current_params)
	XL_DEFINE_PROTO(xcb_randr_get_crtc_transform_current_params_length)
	XL_DEFINE_PROTO(xcb_randr_set_crtc_transform)
	XL_DEFINE_PROTO(xcb_randr_set_crtc_transform_checked)
	XL_DEFINE_PROTO(xcb_randr_monitor_info_outputs)
	XL_DEFINE_PROTO(xcb_randr_monitor_info_outputs_length)
	XL_DEFINE_PROTO(xcb_randr_monitor_info_outputs_end)
	XL_DEFINE_PROTO(xcb_randr_monitor_info_next)
	XL_DEFINE_PROTO(xcb_randr_monitor_info_end)
	XL_DEFINE_PROTO(xcb_randr_get_monitors)
	XL_DEFINE_PROTO(xcb_randr_get_monitors_unchecked)
	XL_DEFINE_PROTO(xcb_randr_get_monitors_monitors_length)
	XL_DEFINE_PROTO(xcb_randr_get_monitors_monitors_iterator)
	XL_DEFINE_PROTO(xcb_randr_get_monitors_reply)
	XL_DEFINE_PROTO(xcb_randr_get_panning)
	XL_DEFINE_PROTO(xcb_randr_get_panning_unchecked)
	XL_DEFINE_PROTO(xcb_randr_get_panning_reply)
	XL_DEFINE_PROTO(xcb_randr_set_panning)
	XL_DEFINE_PROTO(xcb_randr_set_panning_unchecked)
	XL_DEFINE_PROTO(xcb_randr_set_panning_reply)
	XL_DEFINE_PROTO(xcb_randr_set_output_primary_checked)
	XL_DEFINE_PROTO(xcb_randr_set_output_primary)
	decltype(&_xl_null_fn) _xcb_randr_last_fn = &_xl_null_fn;

	decltype(&_xl_null_fn) _xcb_key_first_fn = &_xl_null_fn;
	XL_DEFINE_PROTO(xcb_key_symbols_alloc)
	XL_DEFINE_PROTO(xcb_key_symbols_free)
	XL_DEFINE_PROTO(xcb_key_symbols_get_keysym)
	XL_DEFINE_PROTO(xcb_key_symbols_get_keycode)
	XL_DEFINE_PROTO(xcb_key_press_lookup_keysym)
	XL_DEFINE_PROTO(xcb_key_release_lookup_keysym)
	XL_DEFINE_PROTO(xcb_refresh_keyboard_mapping)
	XL_DEFINE_PROTO(xcb_is_keypad_key)
	XL_DEFINE_PROTO(xcb_is_private_keypad_key)
	XL_DEFINE_PROTO(xcb_is_cursor_key)
	XL_DEFINE_PROTO(xcb_is_pf_key)
	XL_DEFINE_PROTO(xcb_is_function_key)
	XL_DEFINE_PROTO(xcb_is_misc_function_key)
	XL_DEFINE_PROTO(xcb_is_modifier_key)
	decltype(&_xl_null_fn) _xcb_key_last_fn = &_xl_null_fn;

	decltype(&_xl_null_fn) _xcb_xkb_first_fn = &_xl_null_fn;
	XL_DEFINE_PROTO(xcb_xkb_id)
	XL_DEFINE_PROTO(xcb_xkb_select_events)
	decltype(&_xl_null_fn) _xcb_xkb_last_fn = &_xl_null_fn;

	decltype(&_xl_null_fn) _xcb_sync_first_fn = &_xl_null_fn;
	XL_DEFINE_PROTO(xcb_sync_id)
	XL_DEFINE_PROTO(xcb_sync_create_counter)
	XL_DEFINE_PROTO(xcb_sync_create_counter_checked)
	XL_DEFINE_PROTO(xcb_sync_destroy_counter)
	XL_DEFINE_PROTO(xcb_sync_destroy_counter_checked)
	XL_DEFINE_PROTO(xcb_sync_set_counter)
	decltype(&_xl_null_fn) _xcb_sync_last_fn = &_xl_null_fn;

	decltype(&_xl_null_fn) _xcb_cursor_first_fn = &_xl_null_fn;
	XL_DEFINE_PROTO(xcb_cursor_context_new)
	XL_DEFINE_PROTO(xcb_cursor_load_cursor)
	XL_DEFINE_PROTO(xcb_cursor_context_free)
	decltype(&_xl_null_fn) _xcb_cursor_last_fn = &_xl_null_fn;

	decltype(&_xl_null_fn) _xcb_xfixes_first_fn = &_xl_null_fn;
	XL_DEFINE_PROTO(xcb_xfixes_id)
	XL_DEFINE_PROTO(xcb_xfixes_query_version)
	XL_DEFINE_PROTO(xcb_xfixes_query_version_unchecked)
	XL_DEFINE_PROTO(xcb_xfixes_query_version_reply)
	XL_DEFINE_PROTO(xcb_xfixes_select_selection_input)
	decltype(&_xl_null_fn) _xcb_xfixes_last_fn = &_xl_null_fn;

	decltype(&_xl_null_fn) _xcb_shape_first_fn = &_xl_null_fn;
	XL_DEFINE_PROTO(xcb_shape_id)
	XL_DEFINE_PROTO(xcb_shape_op_next)
	XL_DEFINE_PROTO(xcb_shape_op_end)
	XL_DEFINE_PROTO(xcb_shape_kind_next)
	XL_DEFINE_PROTO(xcb_shape_kind_end)
	XL_DEFINE_PROTO(xcb_shape_query_version)
	XL_DEFINE_PROTO(xcb_shape_query_version_unchecked)
	XL_DEFINE_PROTO(xcb_shape_query_version_reply)
	XL_DEFINE_PROTO(xcb_shape_rectangles_checked)
	XL_DEFINE_PROTO(xcb_shape_rectangles)
	XL_DEFINE_PROTO(xcb_shape_rectangles_rectangles)
	XL_DEFINE_PROTO(xcb_shape_rectangles_rectangles_length)
	XL_DEFINE_PROTO(xcb_shape_mask_checked)
	XL_DEFINE_PROTO(xcb_shape_mask)
	XL_DEFINE_PROTO(xcb_shape_combine_checked)
	XL_DEFINE_PROTO(xcb_shape_combine)
	XL_DEFINE_PROTO(xcb_shape_offset_checked)
	XL_DEFINE_PROTO(xcb_shape_offset)
	XL_DEFINE_PROTO(xcb_shape_query_extents)
	XL_DEFINE_PROTO(xcb_shape_query_extents_unchecked)
	XL_DEFINE_PROTO(xcb_shape_query_extents_reply)
	XL_DEFINE_PROTO(xcb_shape_select_input_checked)
	XL_DEFINE_PROTO(xcb_shape_select_input)
	XL_DEFINE_PROTO(xcb_shape_input_selected)
	XL_DEFINE_PROTO(xcb_shape_input_selected_unchecked)
	XL_DEFINE_PROTO(xcb_shape_input_selected_reply)
	XL_DEFINE_PROTO(xcb_shape_get_rectangles)
	XL_DEFINE_PROTO(xcb_shape_get_rectangles_unchecked)
	XL_DEFINE_PROTO(xcb_shape_get_rectangles_rectangles)
	XL_DEFINE_PROTO(xcb_shape_get_rectangles_rectangles_length)
	XL_DEFINE_PROTO(xcb_shape_get_rectangles_reply)
	decltype(&_xl_null_fn) _xcb_shape_last_fn = &_xl_null_fn;

	decltype(&_xl_null_fn) _xcb_errors_first_fn = &_xl_null_fn;
	XL_DEFINE_PROTO(xcb_errors_context_new)
	XL_DEFINE_PROTO(xcb_errors_context_free)
	XL_DEFINE_PROTO(xcb_errors_get_name_for_major_code)
	XL_DEFINE_PROTO(xcb_errors_get_name_for_minor_code)
	XL_DEFINE_PROTO(xcb_errors_get_name_for_core_event)
	XL_DEFINE_PROTO(xcb_errors_get_name_for_xge_event)
	XL_DEFINE_PROTO(xcb_errors_get_name_for_xcb_event)
	XL_DEFINE_PROTO(xcb_errors_get_name_for_error)
	decltype(&_xl_null_fn) _xcb_errors_last_fn = &_xl_null_fn;

protected:
	void openAux();

	Dso _handle;
	Dso _randr;
	Dso _keysyms;
	Dso _xkb;
	Dso _sync;
	Dso _cursor;
	Dso _xfixes;
	Dso _shape;
	Dso _errors;
};

enum class XcbAtomIndex {
	WM_PROTOCOLS,
	WM_DELETE_WINDOW,
	WM_NAME,
	WM_ICON_NAME,
	WM_CHANGE_STATE,
	_NET_FRAME_EXTENTS,
	_NET_WM_DESKTOP,
	_NET_WM_SYNC_REQUEST,
	_NET_WM_SYNC_REQUEST_COUNTER,
	_NET_WM_PING,
	_NET_WM_PID,
	_NET_WM_WINDOW_TYPE,
	_NET_WM_WINDOW_TYPE_DESKTOP,
	_NET_WM_WINDOW_TYPE_DOCK,
	_NET_WM_WINDOW_TYPE_TOOLBAR,
	_NET_WM_WINDOW_TYPE_MENU,
	_NET_WM_WINDOW_TYPE_UTILITY,
	_NET_WM_WINDOW_TYPE_SPLASH,
	_NET_WM_WINDOW_TYPE_DIALOG,
	_NET_WM_WINDOW_TYPE_NORMAL,
	_NET_WM_STATE,
	_NET_WM_STATE_MODAL,
	_NET_WM_STATE_STICKY,
	_NET_WM_STATE_MAXIMIZED_VERT,
	_NET_WM_STATE_MAXIMIZED_HORZ,
	_NET_WM_STATE_SHADED,
	_NET_WM_STATE_SKIP_TASKBAR,
	_NET_WM_STATE_SKIP_PAGER,
	_NET_WM_STATE_HIDDEN,
	_NET_WM_STATE_FULLSCREEN,
	_NET_WM_STATE_ABOVE,
	_NET_WM_STATE_BELOW,
	_NET_WM_STATE_DEMANDS_ATTENTION,
	_NET_WM_STATE_FOCUSED,
	_NET_WM_ALLOWED_ACTIONS,
	_NET_WM_ACTION_MOVE,
	_NET_WM_ACTION_RESIZE,
	_NET_WM_ACTION_MINIMIZE,
	_NET_WM_ACTION_SHADE,
	_NET_WM_ACTION_STICK,
	_NET_WM_ACTION_MAXIMIZE_HORZ,
	_NET_WM_ACTION_MAXIMIZE_VERT,
	_NET_WM_ACTION_FULLSCREEN,
	_NET_WM_ACTION_CHANGE_DESKTOP,
	_NET_WM_ACTION_CLOSE,
	_NET_WM_FULLSCREEN_MONITORS,
	_NET_WM_BYPASS_COMPOSITOR,
	_NET_WM_WINDOW_OPACITY,
	_NET_WM_MOVERESIZE,
	_NET_WM_USER_TIME,
	_NET_SUPPORTED,
	_MOTIF_WM_HINTS,
	_GTK_EDGE_CONSTRAINTS,
	_GTK_FRAME_EXTENTS,
	_GTK_SHOW_WINDOW_MENU,
	SAVE_TARGETS,
	CLIPBOARD,
	PRIMARY,
	TIMESTAMP,
	TARGETS,
	MULTIPLE,
	TEXT,
	UTF8_STRING,
	OCTET_STREAM,
	ATOM_PAIR,
	INCR,
	XNULL,
	XENOLITH_CLIPBOARD,
	_XSETTINGS_SETTINGS,
};

struct XcbAtomInfo {
	XcbAtomIndex index;
	StringView name;
	bool onlyIfExists;
	xcb_atom_t value;
};

#define DEFINE_ATOM_REQUEST(Name, OnlyIfExists) {XcbAtomIndex::Name, #Name, OnlyIfExists, 0}

static XcbAtomInfo s_atomRequests[] = {
	DEFINE_ATOM_REQUEST(WM_PROTOCOLS, true),
	DEFINE_ATOM_REQUEST(WM_DELETE_WINDOW, true),
	DEFINE_ATOM_REQUEST(WM_NAME, true),
	DEFINE_ATOM_REQUEST(WM_ICON_NAME, true),
	DEFINE_ATOM_REQUEST(WM_CHANGE_STATE, true),
	DEFINE_ATOM_REQUEST(_NET_FRAME_EXTENTS, true),
	DEFINE_ATOM_REQUEST(_NET_WM_DESKTOP, true),
	DEFINE_ATOM_REQUEST(_NET_WM_SYNC_REQUEST, true),
	DEFINE_ATOM_REQUEST(_NET_WM_SYNC_REQUEST_COUNTER, true),
	DEFINE_ATOM_REQUEST(_NET_WM_PING, true),
	DEFINE_ATOM_REQUEST(_NET_WM_PID, true),
	DEFINE_ATOM_REQUEST(_NET_WM_WINDOW_TYPE, true),
	DEFINE_ATOM_REQUEST(_NET_WM_WINDOW_TYPE_DESKTOP, true),
	DEFINE_ATOM_REQUEST(_NET_WM_WINDOW_TYPE_DOCK, true),
	DEFINE_ATOM_REQUEST(_NET_WM_WINDOW_TYPE_TOOLBAR, true),
	DEFINE_ATOM_REQUEST(_NET_WM_WINDOW_TYPE_MENU, true),
	DEFINE_ATOM_REQUEST(_NET_WM_WINDOW_TYPE_UTILITY, true),
	DEFINE_ATOM_REQUEST(_NET_WM_WINDOW_TYPE_SPLASH, true),
	DEFINE_ATOM_REQUEST(_NET_WM_WINDOW_TYPE_DIALOG, true),
	DEFINE_ATOM_REQUEST(_NET_WM_WINDOW_TYPE_NORMAL, true),
	DEFINE_ATOM_REQUEST(_NET_WM_STATE, true),
	DEFINE_ATOM_REQUEST(_NET_WM_STATE_MODAL, true),
	DEFINE_ATOM_REQUEST(_NET_WM_STATE_STICKY, true),
	DEFINE_ATOM_REQUEST(_NET_WM_STATE_MAXIMIZED_VERT, true),
	DEFINE_ATOM_REQUEST(_NET_WM_STATE_MAXIMIZED_HORZ, true),
	DEFINE_ATOM_REQUEST(_NET_WM_STATE_SHADED, true),
	DEFINE_ATOM_REQUEST(_NET_WM_STATE_SKIP_TASKBAR, true),
	DEFINE_ATOM_REQUEST(_NET_WM_STATE_SKIP_PAGER, true),
	DEFINE_ATOM_REQUEST(_NET_WM_STATE_HIDDEN, true),
	DEFINE_ATOM_REQUEST(_NET_WM_STATE_FULLSCREEN, true),
	DEFINE_ATOM_REQUEST(_NET_WM_STATE_ABOVE, true),
	DEFINE_ATOM_REQUEST(_NET_WM_STATE_BELOW, true),
	DEFINE_ATOM_REQUEST(_NET_WM_STATE_DEMANDS_ATTENTION, true),
	DEFINE_ATOM_REQUEST(_NET_WM_STATE_FOCUSED, true),
	DEFINE_ATOM_REQUEST(_NET_WM_ALLOWED_ACTIONS, true),
	DEFINE_ATOM_REQUEST(_NET_WM_ACTION_MOVE, true),
	DEFINE_ATOM_REQUEST(_NET_WM_ACTION_RESIZE, true),
	DEFINE_ATOM_REQUEST(_NET_WM_ACTION_MINIMIZE, true),
	DEFINE_ATOM_REQUEST(_NET_WM_ACTION_SHADE, true),
	DEFINE_ATOM_REQUEST(_NET_WM_ACTION_STICK, true),
	DEFINE_ATOM_REQUEST(_NET_WM_ACTION_MAXIMIZE_HORZ, true),
	DEFINE_ATOM_REQUEST(_NET_WM_ACTION_MAXIMIZE_VERT, true),
	DEFINE_ATOM_REQUEST(_NET_WM_ACTION_FULLSCREEN, true),
	DEFINE_ATOM_REQUEST(_NET_WM_ACTION_CHANGE_DESKTOP, true),
	DEFINE_ATOM_REQUEST(_NET_WM_ACTION_CLOSE, true),
	DEFINE_ATOM_REQUEST(_NET_WM_FULLSCREEN_MONITORS, true),
	DEFINE_ATOM_REQUEST(_NET_WM_BYPASS_COMPOSITOR, true),
	DEFINE_ATOM_REQUEST(_NET_WM_WINDOW_OPACITY, true),
	DEFINE_ATOM_REQUEST(_NET_WM_MOVERESIZE, true),
	DEFINE_ATOM_REQUEST(_NET_WM_USER_TIME, true),
	DEFINE_ATOM_REQUEST(_NET_SUPPORTED, true),
	DEFINE_ATOM_REQUEST(_MOTIF_WM_HINTS, true),
	DEFINE_ATOM_REQUEST(_GTK_EDGE_CONSTRAINTS, true),
	DEFINE_ATOM_REQUEST(_GTK_FRAME_EXTENTS, true),
	DEFINE_ATOM_REQUEST(_GTK_SHOW_WINDOW_MENU, true),
	DEFINE_ATOM_REQUEST(SAVE_TARGETS, false),
	DEFINE_ATOM_REQUEST(CLIPBOARD, false),
	DEFINE_ATOM_REQUEST(PRIMARY, false),
	DEFINE_ATOM_REQUEST(TIMESTAMP, false),
	DEFINE_ATOM_REQUEST(TARGETS, false),
	DEFINE_ATOM_REQUEST(MULTIPLE, false),
	DEFINE_ATOM_REQUEST(TEXT, false),
	DEFINE_ATOM_REQUEST(UTF8_STRING, false),
	DEFINE_ATOM_REQUEST(OCTET_STREAM, false),
	DEFINE_ATOM_REQUEST(ATOM_PAIR, false),
	DEFINE_ATOM_REQUEST(INCR, false),
	DEFINE_ATOM_REQUEST(XNULL, false),
	DEFINE_ATOM_REQUEST(XENOLITH_CLIPBOARD, false),
	DEFINE_ATOM_REQUEST(_XSETTINGS_SETTINGS, false),
};

#undef DEFINE_ATOM_REQUEST

struct MotifWmHints {
	uint32_t flags;
	uint32_t functions;
	uint32_t decorations;
	int32_t inputMode;
	uint32_t status;
};

struct FrameExtents {
	static FrameExtents getExtents(xcb_rectangle_t bounding, xcb_rectangle_t content);

	uint32_t left;
	uint32_t right;
	uint32_t top;
	uint32_t bottom;
};

inline bool isEqual(xcb_rectangle_t l, xcb_rectangle_t r) {
	return l.x == r.x && l.y == r.y && l.width == r.width && l.height == r.height;
}

} // namespace stappler::xenolith::platform

#endif

#endif /* XENOLITH_APPLICATION_LINUX_XCB_XLLINUXXCBLIBRARY_H_ */
