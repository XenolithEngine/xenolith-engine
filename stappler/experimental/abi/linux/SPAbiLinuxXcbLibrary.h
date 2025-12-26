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

#include "SPAbiLinux.h"

#if LINUX

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

namespace STAPPLER_VERSIONIZED stappler::abi {

class SP_PUBLIC XcbLibrary : public Ref {
public:
	static constexpr int RANDR_MAJOR_VERSION = XCB_RANDR_MAJOR_VERSION;
	static constexpr int RANDR_MINOR_VERSION = XCB_RANDR_MINOR_VERSION;
	static constexpr int XFIXES_MAJOR_VERSION = XCB_XFIXES_MAJOR_VERSION;
	static constexpr int XFIXES_MINOR_VERSION = XCB_XFIXES_MINOR_VERSION;
	static constexpr int SHAPE_MAJOR_VERSION = XCB_SHAPE_MAJOR_VERSION;
	static constexpr int SHAPE_MINOR_VERSION = XCB_SHAPE_MINOR_VERSION;

	XcbLibrary() { }

	virtual ~XcbLibrary();

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
};


class SP_PUBLIC XcbLibraryAbi : public XcbLibrary {
public:
	bool init();

protected:
	bool open();
	bool openAux();

	void *_handle = nullptr;
	void *_randr = nullptr;
	void *_keysyms = nullptr;
	void *_xkb = nullptr;
	void *_sync = nullptr;
	void *_cursor = nullptr;
	void *_xfixes = nullptr;
	void *_shape = nullptr;
	void *_errors = nullptr;
};

} // namespace stappler::abi

#endif

#endif /* XENOLITH_APPLICATION_LINUX_XCB_XLLINUXXCBLIBRARY_H_ */
