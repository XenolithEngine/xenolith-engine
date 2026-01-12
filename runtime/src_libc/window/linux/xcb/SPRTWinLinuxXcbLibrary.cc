/**
 Copyright (c) 2023-2024 Stappler LLC <admin@stappler.dev>
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

#include "private/window/linux/SPRTWinLinuxXcbLibrary.h"

#include <sprt/runtime/log.h>

namespace sprt::window {

static XcbLibrary *s_XcbLibrary = nullptr;

// redirect xcb_wait_for_reply to libxcb
__SPRT_C_FUNC void *xcb_wait_for_reply(xcb_connection_t *c, unsigned int request,
		xcb_generic_error_t **e) {
	return s_XcbLibrary->xcb_wait_for_reply(c, request, e);
}

XcbLibrary::~XcbLibrary() { close(); }

bool XcbLibrary::init() {
	_handle = Dso("libxcb.so");
	if (!_handle) {
		return false;
	}

	if (open(_handle)) {
		s_XcbLibrary = this;
		return true;
	} else {
		_handle = Dso();
	}
	return false;
}

bool XcbLibrary::open(Dso &handle) {
	SPRT_LOAD_PROTO(handle, xcb_grab_server_checked)
	SPRT_LOAD_PROTO(handle, xcb_grab_server)
	SPRT_LOAD_PROTO(handle, xcb_ungrab_server_checked)
	SPRT_LOAD_PROTO(handle, xcb_ungrab_server)
	SPRT_LOAD_PROTO(handle, xcb_discard_reply)
	SPRT_LOAD_PROTO(handle, xcb_discard_reply64)
	SPRT_LOAD_PROTO(handle, xcb_connect)
	SPRT_LOAD_PROTO(handle, xcb_get_maximum_request_length)
	SPRT_LOAD_PROTO(handle, xcb_get_setup)
	SPRT_LOAD_PROTO(handle, xcb_setup_roots_iterator)
	SPRT_LOAD_PROTO(handle, xcb_screen_next)
	SPRT_LOAD_PROTO(handle, xcb_connection_has_error)
	SPRT_LOAD_PROTO(handle, xcb_get_file_descriptor)
	SPRT_LOAD_PROTO(handle, xcb_generate_id)
	SPRT_LOAD_PROTO(handle, xcb_flush)
	SPRT_LOAD_PROTO(handle, xcb_disconnect)
	SPRT_LOAD_PROTO(handle, xcb_poll_for_event)
	SPRT_LOAD_PROTO(handle, xcb_send_event)
	SPRT_LOAD_PROTO(handle, xcb_get_extension_data)
	SPRT_LOAD_PROTO(handle, xcb_map_window)
	SPRT_LOAD_PROTO(handle, xcb_unmap_window)
	SPRT_LOAD_PROTO(handle, xcb_create_window)
	SPRT_LOAD_PROTO(handle, xcb_destroy_window)
	SPRT_LOAD_PROTO(handle, xcb_configure_window)
	SPRT_LOAD_PROTO(handle, xcb_change_window_attributes)
	SPRT_LOAD_PROTO(handle, xcb_create_colormap)
	SPRT_LOAD_PROTO(handle, xcb_free_colormap)
	SPRT_LOAD_PROTO(handle, xcb_create_pixmap)
	SPRT_LOAD_PROTO(handle, xcb_free_pixmap)
	SPRT_LOAD_PROTO(handle, xcb_create_gc)
	SPRT_LOAD_PROTO(handle, xcb_change_gc)
	SPRT_LOAD_PROTO(handle, xcb_free_gc)
	SPRT_LOAD_PROTO(handle, xcb_poly_fill_rectangle)
	SPRT_LOAD_PROTO(handle, xcb_poly_fill_arc)
	SPRT_LOAD_PROTO(handle, xcb_put_image)
	SPRT_LOAD_PROTO(handle, xcb_copy_area)
	SPRT_LOAD_PROTO(handle, xcb_delete_property)
	SPRT_LOAD_PROTO(handle, xcb_change_property)
	SPRT_LOAD_PROTO(handle, xcb_intern_atom)
	SPRT_LOAD_PROTO(handle, xcb_intern_atom_unchecked)
	SPRT_LOAD_PROTO(handle, xcb_intern_atom_reply)
	SPRT_LOAD_PROTO(handle, xcb_grab_pointer)
	SPRT_LOAD_PROTO(handle, xcb_ungrab_pointer)

	SPRT_LOAD_PROTO(handle, xcb_screen_allowed_depths_iterator)
	SPRT_LOAD_PROTO(handle, xcb_depth_visuals_iterator)
	SPRT_LOAD_PROTO(handle, xcb_visualtype_next)
	SPRT_LOAD_PROTO(handle, xcb_depth_next)

	SPRT_LOAD_PROTO(handle, xcb_get_property_reply)
	SPRT_LOAD_PROTO(handle, xcb_get_property)
	SPRT_LOAD_PROTO(handle, xcb_get_property_unchecked)
	SPRT_LOAD_PROTO(handle, xcb_get_property_value)
	SPRT_LOAD_PROTO(handle, xcb_get_property_value_length)
	SPRT_LOAD_PROTO(handle, xcb_request_check)
	SPRT_LOAD_PROTO(handle, xcb_open_font_checked)
	SPRT_LOAD_PROTO(handle, xcb_create_glyph_cursor)
	SPRT_LOAD_PROTO(handle, xcb_wait_for_reply)
	SPRT_LOAD_PROTO(handle, xcb_create_gc_checked)
	SPRT_LOAD_PROTO(handle, xcb_free_cursor)
	SPRT_LOAD_PROTO(handle, xcb_close_font_checked)
	SPRT_LOAD_PROTO(handle, xcb_get_modifier_mapping_unchecked)
	SPRT_LOAD_PROTO(handle, xcb_get_modifier_mapping_reply)
	SPRT_LOAD_PROTO(handle, xcb_get_modifier_mapping_keycodes)
	SPRT_LOAD_PROTO(handle, xcb_convert_selection)
	SPRT_LOAD_PROTO(handle, xcb_set_selection_owner)
	SPRT_LOAD_PROTO(handle, xcb_get_selection_owner);
	SPRT_LOAD_PROTO(handle, xcb_get_selection_owner_reply)
	SPRT_LOAD_PROTO(handle, xcb_get_keyboard_mapping)
	SPRT_LOAD_PROTO(handle, xcb_get_keyboard_mapping_reply)
	SPRT_LOAD_PROTO(handle, xcb_get_atom_name)
	SPRT_LOAD_PROTO(handle, xcb_get_atom_name_unchecked)
	SPRT_LOAD_PROTO(handle, xcb_get_atom_name_name)
	SPRT_LOAD_PROTO(handle, xcb_get_atom_name_name_length)
	SPRT_LOAD_PROTO(handle, xcb_get_atom_name_name_end)
	SPRT_LOAD_PROTO(handle, xcb_get_atom_name_reply)

	if (!validateFunctionList(&_xcb_first_fn, &_xcb_last_fn)) {
		log::vperror(__SPRT_LOCATION, "XcbLibrary", "Fail to load libxcb");
		return false;
	}

	openAux();
	return true;
}

void XcbLibrary::close() {
	if (s_XcbLibrary == this) {
		s_XcbLibrary = nullptr;
	}
}

bool XcbLibrary::hasRandr() const { return _randr ? true : false; }

bool XcbLibrary::hasKeysyms() const { return _keysyms ? true : false; }

bool XcbLibrary::hasXkb() const { return _xkb ? true : false; }

bool XcbLibrary::hasSync() const { return _sync ? true : false; }

bool XcbLibrary::hasXfixes() const { return _xfixes ? true : false; }

bool XcbLibrary::hasShape() const { return _shape ? true : false; }

void XcbLibrary::openAux() {
	if (auto randr = Dso("libxcb-randr.so")) {
		SPRT_LOAD_PROTO(randr, xcb_randr_id)
		SPRT_LOAD_PROTO(randr, xcb_randr_select_input)
		SPRT_LOAD_PROTO(randr, xcb_randr_select_input_checked)
		SPRT_LOAD_PROTO(randr, xcb_randr_query_version)
		SPRT_LOAD_PROTO(randr, xcb_randr_query_version_reply)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_screen_info)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_screen_info_unchecked)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_screen_info_reply)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_screen_info_sizes)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_screen_info_sizes_length)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_screen_info_sizes_iterator)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_screen_info_rates_length)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_screen_info_rates_iterator)
		SPRT_LOAD_PROTO(randr, xcb_randr_add_output_mode_checked)
		SPRT_LOAD_PROTO(randr, xcb_randr_add_output_mode)
		SPRT_LOAD_PROTO(randr, xcb_randr_delete_output_mode_checked)
		SPRT_LOAD_PROTO(randr, xcb_randr_delete_output_mode)
		SPRT_LOAD_PROTO(randr, xcb_randr_refresh_rates_next)
		SPRT_LOAD_PROTO(randr, xcb_randr_refresh_rates_end)
		SPRT_LOAD_PROTO(randr, xcb_randr_refresh_rates_rates)
		SPRT_LOAD_PROTO(randr, xcb_randr_refresh_rates_rates_length)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_screen_resources)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_screen_resources_unchecked)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_screen_resources_crtcs)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_screen_resources_crtcs_length)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_screen_resources_crtcs_end)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_screen_resources_outputs)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_screen_resources_outputs_length)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_screen_resources_outputs_end)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_screen_resources_modes)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_screen_resources_modes_length)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_screen_resources_modes_iterator)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_screen_resources_names)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_screen_resources_names_length)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_screen_resources_names_end)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_screen_resources_reply)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_screen_resources_current)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_screen_resources_current_unchecked)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_screen_resources_current_reply)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_screen_resources_current_outputs)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_screen_resources_current_outputs_length)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_screen_resources_current_modes)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_screen_resources_current_modes_length)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_screen_resources_current_names)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_screen_resources_current_names_length)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_screen_resources_current_crtcs)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_screen_resources_current_crtcs_length)
		SPRT_LOAD_PROTO(randr, xcb_randr_list_output_properties)
		SPRT_LOAD_PROTO(randr, xcb_randr_list_output_properties_unchecked)
		SPRT_LOAD_PROTO(randr, xcb_randr_list_output_properties_atoms)
		SPRT_LOAD_PROTO(randr, xcb_randr_list_output_properties_atoms_length)
		SPRT_LOAD_PROTO(randr, xcb_randr_list_output_properties_atoms_end)
		SPRT_LOAD_PROTO(randr, xcb_randr_list_output_properties_reply)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_output_primary)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_output_primary_unchecked)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_output_primary_reply)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_output_info)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_output_info_unchecked)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_output_info_reply)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_output_info_crtcs)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_output_info_crtcs_length)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_output_info_crtcs_end)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_output_info_modes)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_output_info_modes_length)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_output_info_name)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_output_info_name_length)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_output_property)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_output_property_unchecked)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_output_property_data)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_output_property_data_length)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_output_property_data_end)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_output_property_reply)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_crtc_info)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_crtc_info_unchecked)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_crtc_info_reply)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_crtc_info_outputs)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_crtc_info_outputs_length)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_crtc_info_possible)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_crtc_info_possible_length)
		SPRT_LOAD_PROTO(randr, xcb_randr_set_screen_size_checked)
		SPRT_LOAD_PROTO(randr, xcb_randr_set_screen_size)
		SPRT_LOAD_PROTO(randr, xcb_randr_set_crtc_config)
		SPRT_LOAD_PROTO(randr, xcb_randr_set_crtc_config_unchecked)
		SPRT_LOAD_PROTO(randr, xcb_randr_set_crtc_config_reply)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_crtc_transform)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_crtc_transform_unchecked)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_crtc_transform_reply)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_crtc_transform_current_filter_name)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_crtc_transform_current_filter_name_length)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_crtc_transform_current_params)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_crtc_transform_current_params_length)
		SPRT_LOAD_PROTO(randr, xcb_randr_set_crtc_transform)
		SPRT_LOAD_PROTO(randr, xcb_randr_set_crtc_transform_checked)
		SPRT_LOAD_PROTO(randr, xcb_randr_monitor_info_outputs)
		SPRT_LOAD_PROTO(randr, xcb_randr_monitor_info_outputs_length)
		SPRT_LOAD_PROTO(randr, xcb_randr_monitor_info_outputs_end)
		SPRT_LOAD_PROTO(randr, xcb_randr_monitor_info_next)
		SPRT_LOAD_PROTO(randr, xcb_randr_monitor_info_end)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_monitors)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_monitors_unchecked)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_monitors_monitors_length)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_monitors_monitors_iterator)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_monitors_reply)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_panning)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_panning_unchecked)
		SPRT_LOAD_PROTO(randr, xcb_randr_get_panning_reply)
		SPRT_LOAD_PROTO(randr, xcb_randr_set_panning)
		SPRT_LOAD_PROTO(randr, xcb_randr_set_panning_unchecked)
		SPRT_LOAD_PROTO(randr, xcb_randr_set_panning_reply)
		SPRT_LOAD_PROTO(randr, xcb_randr_set_output_primary_checked)
		SPRT_LOAD_PROTO(randr, xcb_randr_set_output_primary)

		if (!validateFunctionList(&_xcb_randr_first_fn, &_xcb_randr_last_fn)) {
			log::vperror(__SPRT_LOCATION, "XcbLibrary", "Fail to load libxcb-randr function");
		} else {
			_randr = move(randr);
		}
	}

	if (auto keysyms = Dso("libxcb-keysyms.so")) {
		SPRT_LOAD_PROTO(keysyms, xcb_key_symbols_alloc)
		SPRT_LOAD_PROTO(keysyms, xcb_key_symbols_free)
		SPRT_LOAD_PROTO(keysyms, xcb_key_symbols_get_keysym)
		SPRT_LOAD_PROTO(keysyms, xcb_key_symbols_get_keycode)
		SPRT_LOAD_PROTO(keysyms, xcb_key_press_lookup_keysym)
		SPRT_LOAD_PROTO(keysyms, xcb_key_release_lookup_keysym)
		SPRT_LOAD_PROTO(keysyms, xcb_refresh_keyboard_mapping)
		SPRT_LOAD_PROTO(keysyms, xcb_is_keypad_key)
		SPRT_LOAD_PROTO(keysyms, xcb_is_private_keypad_key)
		SPRT_LOAD_PROTO(keysyms, xcb_is_cursor_key)
		SPRT_LOAD_PROTO(keysyms, xcb_is_pf_key)
		SPRT_LOAD_PROTO(keysyms, xcb_is_function_key)
		SPRT_LOAD_PROTO(keysyms, xcb_is_misc_function_key)
		SPRT_LOAD_PROTO(keysyms, xcb_is_modifier_key)

		if (!validateFunctionList(&_xcb_key_first_fn, &_xcb_key_last_fn)) {
			log::vperror(__SPRT_LOCATION, "XcbLibrary", "Fail to load libxcb-randr function");
		} else {
			_keysyms = move(keysyms);
		}
	}

	if (auto xkb = Dso("libxcb-xkb.so")) {
		SPRT_LOAD_PROTO(xkb, xcb_xkb_id)
		SPRT_LOAD_PROTO(xkb, xcb_xkb_select_events)

		if (!validateFunctionList(&_xcb_xkb_first_fn, &_xcb_xkb_last_fn)) {
			log::vperror(__SPRT_LOCATION, "XcbLibrary", "Fail to load libxcb-xkb function");
		} else {
			_xkb = move(xkb);
		}
	}

	if (auto sync = Dso("libxcb-sync.so")) {
		SPRT_LOAD_PROTO(sync, xcb_sync_id)
		SPRT_LOAD_PROTO(sync, xcb_sync_create_counter)
		SPRT_LOAD_PROTO(sync, xcb_sync_create_counter_checked)
		SPRT_LOAD_PROTO(sync, xcb_sync_destroy_counter)
		SPRT_LOAD_PROTO(sync, xcb_sync_destroy_counter_checked)
		SPRT_LOAD_PROTO(sync, xcb_sync_set_counter)

		if (!validateFunctionList(&_xcb_sync_first_fn, &_xcb_sync_last_fn)) {
			log::vperror(__SPRT_LOCATION, "XcbLibrary", "Fail to load libxcb-sync function");
		} else {
			_sync = move(sync);
		}
	}

	if (auto cursor = Dso("libxcb-cursor.so")) {
		SPRT_LOAD_PROTO(cursor, xcb_cursor_context_new)
		SPRT_LOAD_PROTO(cursor, xcb_cursor_load_cursor)
		SPRT_LOAD_PROTO(cursor, xcb_cursor_context_free)

		if (!validateFunctionList(&_xcb_cursor_first_fn, &_xcb_cursor_last_fn)) {
			log::vperror(__SPRT_LOCATION, "XcbLibrary", "Fail to load libxcb-cursor function");
		} else {
			_cursor = move(cursor);
		}
	}

	if (auto xfixes = Dso("libxcb-xfixes.so")) {
		SPRT_LOAD_PROTO(xfixes, xcb_xfixes_id)
		SPRT_LOAD_PROTO(xfixes, xcb_xfixes_query_version)
		SPRT_LOAD_PROTO(xfixes, xcb_xfixes_query_version_unchecked)
		SPRT_LOAD_PROTO(xfixes, xcb_xfixes_query_version_reply)
		SPRT_LOAD_PROTO(xfixes, xcb_xfixes_select_selection_input)

		if (!validateFunctionList(&_xcb_xfixes_first_fn, &_xcb_xfixes_last_fn)) {
			log::vperror(__SPRT_LOCATION, "XcbLibrary", "Fail to load libxcb-xfixes function");
		} else {
			_xfixes = move(xfixes);
		}
	}

	if (auto shape = Dso("libxcb-shape.so")) {
		SPRT_LOAD_PROTO(shape, xcb_shape_id)
		SPRT_LOAD_PROTO(shape, xcb_shape_op_next)
		SPRT_LOAD_PROTO(shape, xcb_shape_op_end)
		SPRT_LOAD_PROTO(shape, xcb_shape_kind_next)
		SPRT_LOAD_PROTO(shape, xcb_shape_kind_end)
		SPRT_LOAD_PROTO(shape, xcb_shape_query_version)
		SPRT_LOAD_PROTO(shape, xcb_shape_query_version_unchecked)
		SPRT_LOAD_PROTO(shape, xcb_shape_query_version_reply)
		SPRT_LOAD_PROTO(shape, xcb_shape_rectangles_checked)
		SPRT_LOAD_PROTO(shape, xcb_shape_rectangles)
		SPRT_LOAD_PROTO(shape, xcb_shape_rectangles_rectangles)
		SPRT_LOAD_PROTO(shape, xcb_shape_rectangles_rectangles_length)
		SPRT_LOAD_PROTO(shape, xcb_shape_mask_checked)
		SPRT_LOAD_PROTO(shape, xcb_shape_mask)
		SPRT_LOAD_PROTO(shape, xcb_shape_combine_checked)
		SPRT_LOAD_PROTO(shape, xcb_shape_combine)
		SPRT_LOAD_PROTO(shape, xcb_shape_offset_checked)
		SPRT_LOAD_PROTO(shape, xcb_shape_offset)
		SPRT_LOAD_PROTO(shape, xcb_shape_query_extents)
		SPRT_LOAD_PROTO(shape, xcb_shape_query_extents_unchecked)
		SPRT_LOAD_PROTO(shape, xcb_shape_query_extents_reply)
		SPRT_LOAD_PROTO(shape, xcb_shape_select_input_checked)
		SPRT_LOAD_PROTO(shape, xcb_shape_select_input)
		SPRT_LOAD_PROTO(shape, xcb_shape_input_selected)
		SPRT_LOAD_PROTO(shape, xcb_shape_input_selected_unchecked)
		SPRT_LOAD_PROTO(shape, xcb_shape_input_selected_reply)
		SPRT_LOAD_PROTO(shape, xcb_shape_get_rectangles)
		SPRT_LOAD_PROTO(shape, xcb_shape_get_rectangles_unchecked)
		SPRT_LOAD_PROTO(shape, xcb_shape_get_rectangles_rectangles)
		SPRT_LOAD_PROTO(shape, xcb_shape_get_rectangles_rectangles_length)
		SPRT_LOAD_PROTO(shape, xcb_shape_get_rectangles_reply)

		if (!validateFunctionList(&_xcb_shape_first_fn, &_xcb_shape_last_fn)) {
			log::vperror(__SPRT_LOCATION, "XcbLibrary", "Fail to load libxcb-shape function");
		} else {
			_shape = move(shape);
		}
	}

	if (auto errors = Dso("libxcb-errors.so")) {
		SPRT_LOAD_PROTO(errors, xcb_errors_context_new)
		SPRT_LOAD_PROTO(errors, xcb_errors_context_free)
		SPRT_LOAD_PROTO(errors, xcb_errors_get_name_for_major_code)
		SPRT_LOAD_PROTO(errors, xcb_errors_get_name_for_minor_code)
		SPRT_LOAD_PROTO(errors, xcb_errors_get_name_for_core_event)
		SPRT_LOAD_PROTO(errors, xcb_errors_get_name_for_xge_event)
		SPRT_LOAD_PROTO(errors, xcb_errors_get_name_for_xcb_event)
		SPRT_LOAD_PROTO(errors, xcb_errors_get_name_for_error)

		if (!validateFunctionList(&_xcb_errors_first_fn, &_xcb_errors_last_fn)) {
			log::vperror(__SPRT_LOCATION, "XcbLibrary", "Fail to load libxcb-errors function");
		} else {
			_errors = move(errors);
		}
	}
}

FrameExtents FrameExtents::getExtents(xcb_rectangle_t bounding, xcb_rectangle_t content) {
	FrameExtents ret;
	ret.left = content.x;
	ret.top = content.y;
	ret.right = bounding.width - ret.left - content.width;
	ret.bottom = bounding.height - ret.top - content.height;
	return ret;
}

} // namespace sprt::window
