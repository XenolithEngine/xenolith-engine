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

#include "XLLinuxXcbLibrary.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

static XcbLibrary *s_XcbLibrary = nullptr;

// redirect xcb_wait_for_reply to libxcb
SP_EXTERN_C void *xcb_wait_for_reply(xcb_connection_t *c, unsigned int request,
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
	XL_LOAD_PROTO(handle, xcb_grab_server_checked)
	XL_LOAD_PROTO(handle, xcb_grab_server)
	XL_LOAD_PROTO(handle, xcb_ungrab_server_checked)
	XL_LOAD_PROTO(handle, xcb_ungrab_server)
	XL_LOAD_PROTO(handle, xcb_discard_reply)
	XL_LOAD_PROTO(handle, xcb_discard_reply64)
	XL_LOAD_PROTO(handle, xcb_connect)
	XL_LOAD_PROTO(handle, xcb_get_maximum_request_length)
	XL_LOAD_PROTO(handle, xcb_get_setup)
	XL_LOAD_PROTO(handle, xcb_setup_roots_iterator)
	XL_LOAD_PROTO(handle, xcb_screen_next)
	XL_LOAD_PROTO(handle, xcb_connection_has_error)
	XL_LOAD_PROTO(handle, xcb_get_file_descriptor)
	XL_LOAD_PROTO(handle, xcb_generate_id)
	XL_LOAD_PROTO(handle, xcb_flush)
	XL_LOAD_PROTO(handle, xcb_disconnect)
	XL_LOAD_PROTO(handle, xcb_poll_for_event)
	XL_LOAD_PROTO(handle, xcb_send_event)
	XL_LOAD_PROTO(handle, xcb_get_extension_data)
	XL_LOAD_PROTO(handle, xcb_map_window)
	XL_LOAD_PROTO(handle, xcb_unmap_window)
	XL_LOAD_PROTO(handle, xcb_create_window)
	XL_LOAD_PROTO(handle, xcb_destroy_window)
	XL_LOAD_PROTO(handle, xcb_configure_window)
	XL_LOAD_PROTO(handle, xcb_change_window_attributes)
	XL_LOAD_PROTO(handle, xcb_create_colormap)
	XL_LOAD_PROTO(handle, xcb_free_colormap)
	XL_LOAD_PROTO(handle, xcb_create_pixmap)
	XL_LOAD_PROTO(handle, xcb_free_pixmap)
	XL_LOAD_PROTO(handle, xcb_create_gc)
	XL_LOAD_PROTO(handle, xcb_change_gc)
	XL_LOAD_PROTO(handle, xcb_free_gc)
	XL_LOAD_PROTO(handle, xcb_poly_fill_rectangle)
	XL_LOAD_PROTO(handle, xcb_poly_fill_arc)
	XL_LOAD_PROTO(handle, xcb_put_image)
	XL_LOAD_PROTO(handle, xcb_copy_area)
	XL_LOAD_PROTO(handle, xcb_delete_property)
	XL_LOAD_PROTO(handle, xcb_change_property)
	XL_LOAD_PROTO(handle, xcb_intern_atom)
	XL_LOAD_PROTO(handle, xcb_intern_atom_unchecked)
	XL_LOAD_PROTO(handle, xcb_intern_atom_reply)
	XL_LOAD_PROTO(handle, xcb_grab_pointer)
	XL_LOAD_PROTO(handle, xcb_ungrab_pointer)

	XL_LOAD_PROTO(handle, xcb_screen_allowed_depths_iterator)
	XL_LOAD_PROTO(handle, xcb_depth_visuals_iterator)
	XL_LOAD_PROTO(handle, xcb_visualtype_next)
	XL_LOAD_PROTO(handle, xcb_depth_next)

	XL_LOAD_PROTO(handle, xcb_get_property_reply)
	XL_LOAD_PROTO(handle, xcb_get_property)
	XL_LOAD_PROTO(handle, xcb_get_property_unchecked)
	XL_LOAD_PROTO(handle, xcb_get_property_value)
	XL_LOAD_PROTO(handle, xcb_get_property_value_length)
	XL_LOAD_PROTO(handle, xcb_request_check)
	XL_LOAD_PROTO(handle, xcb_open_font_checked)
	XL_LOAD_PROTO(handle, xcb_create_glyph_cursor)
	XL_LOAD_PROTO(handle, xcb_wait_for_reply)
	XL_LOAD_PROTO(handle, xcb_create_gc_checked)
	XL_LOAD_PROTO(handle, xcb_free_cursor)
	XL_LOAD_PROTO(handle, xcb_close_font_checked)
	XL_LOAD_PROTO(handle, xcb_get_modifier_mapping_unchecked)
	XL_LOAD_PROTO(handle, xcb_get_modifier_mapping_reply)
	XL_LOAD_PROTO(handle, xcb_get_modifier_mapping_keycodes)
	XL_LOAD_PROTO(handle, xcb_convert_selection)
	XL_LOAD_PROTO(handle, xcb_set_selection_owner)
	XL_LOAD_PROTO(handle, xcb_get_selection_owner);
	XL_LOAD_PROTO(handle, xcb_get_selection_owner_reply)
	XL_LOAD_PROTO(handle, xcb_get_keyboard_mapping)
	XL_LOAD_PROTO(handle, xcb_get_keyboard_mapping_reply)
	XL_LOAD_PROTO(handle, xcb_get_atom_name)
	XL_LOAD_PROTO(handle, xcb_get_atom_name_unchecked)
	XL_LOAD_PROTO(handle, xcb_get_atom_name_name)
	XL_LOAD_PROTO(handle, xcb_get_atom_name_name_length)
	XL_LOAD_PROTO(handle, xcb_get_atom_name_name_end)
	XL_LOAD_PROTO(handle, xcb_get_atom_name_reply)

	if (!validateFunctionList(&_xcb_first_fn, &_xcb_last_fn)) {
		log::source().error("XcbLibrary", "Fail to load libxcb");
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
		XL_LOAD_PROTO(randr, xcb_randr_id)
		XL_LOAD_PROTO(randr, xcb_randr_select_input)
		XL_LOAD_PROTO(randr, xcb_randr_select_input_checked)
		XL_LOAD_PROTO(randr, xcb_randr_query_version)
		XL_LOAD_PROTO(randr, xcb_randr_query_version_reply)
		XL_LOAD_PROTO(randr, xcb_randr_get_screen_info)
		XL_LOAD_PROTO(randr, xcb_randr_get_screen_info_unchecked)
		XL_LOAD_PROTO(randr, xcb_randr_get_screen_info_reply)
		XL_LOAD_PROTO(randr, xcb_randr_get_screen_info_sizes)
		XL_LOAD_PROTO(randr, xcb_randr_get_screen_info_sizes_length)
		XL_LOAD_PROTO(randr, xcb_randr_get_screen_info_sizes_iterator)
		XL_LOAD_PROTO(randr, xcb_randr_get_screen_info_rates_length)
		XL_LOAD_PROTO(randr, xcb_randr_get_screen_info_rates_iterator)
		XL_LOAD_PROTO(randr, xcb_randr_add_output_mode_checked)
		XL_LOAD_PROTO(randr, xcb_randr_add_output_mode)
		XL_LOAD_PROTO(randr, xcb_randr_delete_output_mode_checked)
		XL_LOAD_PROTO(randr, xcb_randr_delete_output_mode)
		XL_LOAD_PROTO(randr, xcb_randr_refresh_rates_next)
		XL_LOAD_PROTO(randr, xcb_randr_refresh_rates_end)
		XL_LOAD_PROTO(randr, xcb_randr_refresh_rates_rates)
		XL_LOAD_PROTO(randr, xcb_randr_refresh_rates_rates_length)
		XL_LOAD_PROTO(randr, xcb_randr_get_screen_resources)
		XL_LOAD_PROTO(randr, xcb_randr_get_screen_resources_unchecked)
		XL_LOAD_PROTO(randr, xcb_randr_get_screen_resources_crtcs)
		XL_LOAD_PROTO(randr, xcb_randr_get_screen_resources_crtcs_length)
		XL_LOAD_PROTO(randr, xcb_randr_get_screen_resources_crtcs_end)
		XL_LOAD_PROTO(randr, xcb_randr_get_screen_resources_outputs)
		XL_LOAD_PROTO(randr, xcb_randr_get_screen_resources_outputs_length)
		XL_LOAD_PROTO(randr, xcb_randr_get_screen_resources_outputs_end)
		XL_LOAD_PROTO(randr, xcb_randr_get_screen_resources_modes)
		XL_LOAD_PROTO(randr, xcb_randr_get_screen_resources_modes_length)
		XL_LOAD_PROTO(randr, xcb_randr_get_screen_resources_modes_iterator)
		XL_LOAD_PROTO(randr, xcb_randr_get_screen_resources_names)
		XL_LOAD_PROTO(randr, xcb_randr_get_screen_resources_names_length)
		XL_LOAD_PROTO(randr, xcb_randr_get_screen_resources_names_end)
		XL_LOAD_PROTO(randr, xcb_randr_get_screen_resources_reply)
		XL_LOAD_PROTO(randr, xcb_randr_get_screen_resources_current)
		XL_LOAD_PROTO(randr, xcb_randr_get_screen_resources_current_unchecked)
		XL_LOAD_PROTO(randr, xcb_randr_get_screen_resources_current_reply)
		XL_LOAD_PROTO(randr, xcb_randr_get_screen_resources_current_outputs)
		XL_LOAD_PROTO(randr, xcb_randr_get_screen_resources_current_outputs_length)
		XL_LOAD_PROTO(randr, xcb_randr_get_screen_resources_current_modes)
		XL_LOAD_PROTO(randr, xcb_randr_get_screen_resources_current_modes_length)
		XL_LOAD_PROTO(randr, xcb_randr_get_screen_resources_current_names)
		XL_LOAD_PROTO(randr, xcb_randr_get_screen_resources_current_names_length)
		XL_LOAD_PROTO(randr, xcb_randr_get_screen_resources_current_crtcs)
		XL_LOAD_PROTO(randr, xcb_randr_get_screen_resources_current_crtcs_length)
		XL_LOAD_PROTO(randr, xcb_randr_list_output_properties)
		XL_LOAD_PROTO(randr, xcb_randr_list_output_properties_unchecked)
		XL_LOAD_PROTO(randr, xcb_randr_list_output_properties_atoms)
		XL_LOAD_PROTO(randr, xcb_randr_list_output_properties_atoms_length)
		XL_LOAD_PROTO(randr, xcb_randr_list_output_properties_atoms_end)
		XL_LOAD_PROTO(randr, xcb_randr_list_output_properties_reply)
		XL_LOAD_PROTO(randr, xcb_randr_get_output_primary)
		XL_LOAD_PROTO(randr, xcb_randr_get_output_primary_unchecked)
		XL_LOAD_PROTO(randr, xcb_randr_get_output_primary_reply)
		XL_LOAD_PROTO(randr, xcb_randr_get_output_info)
		XL_LOAD_PROTO(randr, xcb_randr_get_output_info_unchecked)
		XL_LOAD_PROTO(randr, xcb_randr_get_output_info_reply)
		XL_LOAD_PROTO(randr, xcb_randr_get_output_info_crtcs)
		XL_LOAD_PROTO(randr, xcb_randr_get_output_info_crtcs_length)
		XL_LOAD_PROTO(randr, xcb_randr_get_output_info_crtcs_end)
		XL_LOAD_PROTO(randr, xcb_randr_get_output_info_modes)
		XL_LOAD_PROTO(randr, xcb_randr_get_output_info_modes_length)
		XL_LOAD_PROTO(randr, xcb_randr_get_output_info_name)
		XL_LOAD_PROTO(randr, xcb_randr_get_output_info_name_length)
		XL_LOAD_PROTO(randr, xcb_randr_get_output_property)
		XL_LOAD_PROTO(randr, xcb_randr_get_output_property_unchecked)
		XL_LOAD_PROTO(randr, xcb_randr_get_output_property_data)
		XL_LOAD_PROTO(randr, xcb_randr_get_output_property_data_length)
		XL_LOAD_PROTO(randr, xcb_randr_get_output_property_data_end)
		XL_LOAD_PROTO(randr, xcb_randr_get_output_property_reply)
		XL_LOAD_PROTO(randr, xcb_randr_get_crtc_info)
		XL_LOAD_PROTO(randr, xcb_randr_get_crtc_info_unchecked)
		XL_LOAD_PROTO(randr, xcb_randr_get_crtc_info_reply)
		XL_LOAD_PROTO(randr, xcb_randr_get_crtc_info_outputs)
		XL_LOAD_PROTO(randr, xcb_randr_get_crtc_info_outputs_length)
		XL_LOAD_PROTO(randr, xcb_randr_get_crtc_info_possible)
		XL_LOAD_PROTO(randr, xcb_randr_get_crtc_info_possible_length)
		XL_LOAD_PROTO(randr, xcb_randr_set_screen_size_checked)
		XL_LOAD_PROTO(randr, xcb_randr_set_screen_size)
		XL_LOAD_PROTO(randr, xcb_randr_set_crtc_config)
		XL_LOAD_PROTO(randr, xcb_randr_set_crtc_config_unchecked)
		XL_LOAD_PROTO(randr, xcb_randr_set_crtc_config_reply)
		XL_LOAD_PROTO(randr, xcb_randr_get_crtc_transform)
		XL_LOAD_PROTO(randr, xcb_randr_get_crtc_transform_unchecked)
		XL_LOAD_PROTO(randr, xcb_randr_get_crtc_transform_reply)
		XL_LOAD_PROTO(randr, xcb_randr_get_crtc_transform_current_filter_name)
		XL_LOAD_PROTO(randr, xcb_randr_get_crtc_transform_current_filter_name_length)
		XL_LOAD_PROTO(randr, xcb_randr_get_crtc_transform_current_params)
		XL_LOAD_PROTO(randr, xcb_randr_get_crtc_transform_current_params_length)
		XL_LOAD_PROTO(randr, xcb_randr_set_crtc_transform)
		XL_LOAD_PROTO(randr, xcb_randr_set_crtc_transform_checked)
		XL_LOAD_PROTO(randr, xcb_randr_monitor_info_outputs)
		XL_LOAD_PROTO(randr, xcb_randr_monitor_info_outputs_length)
		XL_LOAD_PROTO(randr, xcb_randr_monitor_info_outputs_end)
		XL_LOAD_PROTO(randr, xcb_randr_monitor_info_next)
		XL_LOAD_PROTO(randr, xcb_randr_monitor_info_end)
		XL_LOAD_PROTO(randr, xcb_randr_get_monitors)
		XL_LOAD_PROTO(randr, xcb_randr_get_monitors_unchecked)
		XL_LOAD_PROTO(randr, xcb_randr_get_monitors_monitors_length)
		XL_LOAD_PROTO(randr, xcb_randr_get_monitors_monitors_iterator)
		XL_LOAD_PROTO(randr, xcb_randr_get_monitors_reply)
		XL_LOAD_PROTO(randr, xcb_randr_get_panning)
		XL_LOAD_PROTO(randr, xcb_randr_get_panning_unchecked)
		XL_LOAD_PROTO(randr, xcb_randr_get_panning_reply)
		XL_LOAD_PROTO(randr, xcb_randr_set_panning)
		XL_LOAD_PROTO(randr, xcb_randr_set_panning_unchecked)
		XL_LOAD_PROTO(randr, xcb_randr_set_panning_reply)
		XL_LOAD_PROTO(randr, xcb_randr_set_output_primary_checked)
		XL_LOAD_PROTO(randr, xcb_randr_set_output_primary)

		if (!validateFunctionList(&_xcb_randr_first_fn, &_xcb_randr_last_fn)) {
			log::source().error("XcbLibrary", "Fail to load libxcb-randr function");
		} else {
			_randr = move(randr);
		}
	}

	if (auto keysyms = Dso("libxcb-keysyms.so")) {
		XL_LOAD_PROTO(keysyms, xcb_key_symbols_alloc)
		XL_LOAD_PROTO(keysyms, xcb_key_symbols_free)
		XL_LOAD_PROTO(keysyms, xcb_key_symbols_get_keysym)
		XL_LOAD_PROTO(keysyms, xcb_key_symbols_get_keycode)
		XL_LOAD_PROTO(keysyms, xcb_key_press_lookup_keysym)
		XL_LOAD_PROTO(keysyms, xcb_key_release_lookup_keysym)
		XL_LOAD_PROTO(keysyms, xcb_refresh_keyboard_mapping)
		XL_LOAD_PROTO(keysyms, xcb_is_keypad_key)
		XL_LOAD_PROTO(keysyms, xcb_is_private_keypad_key)
		XL_LOAD_PROTO(keysyms, xcb_is_cursor_key)
		XL_LOAD_PROTO(keysyms, xcb_is_pf_key)
		XL_LOAD_PROTO(keysyms, xcb_is_function_key)
		XL_LOAD_PROTO(keysyms, xcb_is_misc_function_key)
		XL_LOAD_PROTO(keysyms, xcb_is_modifier_key)

		if (!validateFunctionList(&_xcb_key_first_fn, &_xcb_key_last_fn)) {
			log::source().error("XcbLibrary", "Fail to load libxcb-randr function");
		} else {
			_keysyms = move(keysyms);
		}
	}

	if (auto xkb = Dso("libxcb-xkb.so")) {
		XL_LOAD_PROTO(xkb, xcb_xkb_id)
		XL_LOAD_PROTO(xkb, xcb_xkb_select_events)

		if (!validateFunctionList(&_xcb_xkb_first_fn, &_xcb_xkb_last_fn)) {
			log::source().error("XcbLibrary", "Fail to load libxcb-xkb function");
		} else {
			_xkb = move(xkb);
		}
	}

	if (auto sync = Dso("libxcb-sync.so")) {
		XL_LOAD_PROTO(sync, xcb_sync_id)
		XL_LOAD_PROTO(sync, xcb_sync_create_counter)
		XL_LOAD_PROTO(sync, xcb_sync_create_counter_checked)
		XL_LOAD_PROTO(sync, xcb_sync_destroy_counter)
		XL_LOAD_PROTO(sync, xcb_sync_destroy_counter_checked)
		XL_LOAD_PROTO(sync, xcb_sync_set_counter)

		if (!validateFunctionList(&_xcb_sync_first_fn, &_xcb_sync_last_fn)) {
			log::source().error("XcbLibrary", "Fail to load libxcb-sync function");
		} else {
			_sync = move(sync);
		}
	}

	if (auto cursor = Dso("libxcb-cursor.so")) {
		XL_LOAD_PROTO(cursor, xcb_cursor_context_new)
		XL_LOAD_PROTO(cursor, xcb_cursor_load_cursor)
		XL_LOAD_PROTO(cursor, xcb_cursor_context_free)

		if (!validateFunctionList(&_xcb_cursor_first_fn, &_xcb_cursor_last_fn)) {
			log::source().error("XcbLibrary", "Fail to load libxcb-cursor function");
		} else {
			_cursor = move(cursor);
		}
	}

	if (auto xfixes = Dso("libxcb-xfixes.so")) {
		XL_LOAD_PROTO(xfixes, xcb_xfixes_id)
		XL_LOAD_PROTO(xfixes, xcb_xfixes_query_version)
		XL_LOAD_PROTO(xfixes, xcb_xfixes_query_version_unchecked)
		XL_LOAD_PROTO(xfixes, xcb_xfixes_query_version_reply)
		XL_LOAD_PROTO(xfixes, xcb_xfixes_select_selection_input)

		if (!validateFunctionList(&_xcb_xfixes_first_fn, &_xcb_xfixes_last_fn)) {
			log::source().error("XcbLibrary", "Fail to load libxcb-xfixes function");
		} else {
			_xfixes = move(xfixes);
		}
	}

	if (auto shape = Dso("libxcb-shape.so")) {
		XL_LOAD_PROTO(shape, xcb_shape_id)
		XL_LOAD_PROTO(shape, xcb_shape_op_next)
		XL_LOAD_PROTO(shape, xcb_shape_op_end)
		XL_LOAD_PROTO(shape, xcb_shape_kind_next)
		XL_LOAD_PROTO(shape, xcb_shape_kind_end)
		XL_LOAD_PROTO(shape, xcb_shape_query_version)
		XL_LOAD_PROTO(shape, xcb_shape_query_version_unchecked)
		XL_LOAD_PROTO(shape, xcb_shape_query_version_reply)
		XL_LOAD_PROTO(shape, xcb_shape_rectangles_checked)
		XL_LOAD_PROTO(shape, xcb_shape_rectangles)
		XL_LOAD_PROTO(shape, xcb_shape_rectangles_rectangles)
		XL_LOAD_PROTO(shape, xcb_shape_rectangles_rectangles_length)
		XL_LOAD_PROTO(shape, xcb_shape_mask_checked)
		XL_LOAD_PROTO(shape, xcb_shape_mask)
		XL_LOAD_PROTO(shape, xcb_shape_combine_checked)
		XL_LOAD_PROTO(shape, xcb_shape_combine)
		XL_LOAD_PROTO(shape, xcb_shape_offset_checked)
		XL_LOAD_PROTO(shape, xcb_shape_offset)
		XL_LOAD_PROTO(shape, xcb_shape_query_extents)
		XL_LOAD_PROTO(shape, xcb_shape_query_extents_unchecked)
		XL_LOAD_PROTO(shape, xcb_shape_query_extents_reply)
		XL_LOAD_PROTO(shape, xcb_shape_select_input_checked)
		XL_LOAD_PROTO(shape, xcb_shape_select_input)
		XL_LOAD_PROTO(shape, xcb_shape_input_selected)
		XL_LOAD_PROTO(shape, xcb_shape_input_selected_unchecked)
		XL_LOAD_PROTO(shape, xcb_shape_input_selected_reply)
		XL_LOAD_PROTO(shape, xcb_shape_get_rectangles)
		XL_LOAD_PROTO(shape, xcb_shape_get_rectangles_unchecked)
		XL_LOAD_PROTO(shape, xcb_shape_get_rectangles_rectangles)
		XL_LOAD_PROTO(shape, xcb_shape_get_rectangles_rectangles_length)
		XL_LOAD_PROTO(shape, xcb_shape_get_rectangles_reply)

		if (!validateFunctionList(&_xcb_shape_first_fn, &_xcb_shape_last_fn)) {
			log::source().error("XcbLibrary", "Fail to load libxcb-shape function");
		} else {
			_shape = move(shape);
		}
	}

	if (auto errors = Dso("libxcb-errors.so")) {
		XL_LOAD_PROTO(errors, xcb_errors_context_new)
		XL_LOAD_PROTO(errors, xcb_errors_context_free)
		XL_LOAD_PROTO(errors, xcb_errors_get_name_for_major_code)
		XL_LOAD_PROTO(errors, xcb_errors_get_name_for_minor_code)
		XL_LOAD_PROTO(errors, xcb_errors_get_name_for_core_event)
		XL_LOAD_PROTO(errors, xcb_errors_get_name_for_xge_event)
		XL_LOAD_PROTO(errors, xcb_errors_get_name_for_xcb_event)
		XL_LOAD_PROTO(errors, xcb_errors_get_name_for_error)

		if (!validateFunctionList(&_xcb_errors_first_fn, &_xcb_errors_last_fn)) {
			log::source().error("XcbLibrary", "Fail to load libxcb-errors function");
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

} // namespace stappler::xenolith::platform
