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

#include "XLLinuxXcbConnection.h"
#include "XLLinuxXcbSupportWindow.h"
#include "XLLinuxXcbDisplayConfigManager.h"
#include "XLLinuxXcbWindow.h"
#include "XLLinuxXcbLibrary.h"

#include "SPPlatformUnistd.h"

#define XL_X11_DEBUG 0

#if XL_X11_DEBUG
#define XL_X11_LOG(...) log::format(log::Debug, "XCB", SP_LOCATION, __VA_ARGS__)
#else
#define XL_X11_LOG(...)
#endif

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

void XcbConnection::ReportError(int error) {
	switch (error) {
	case XCB_CONN_ERROR:
		stappler::log::source().error("XcbView",
				"XCB_CONN_ERROR: socket error, pipe error or other stream error");
		break;
	case XCB_CONN_CLOSED_EXT_NOTSUPPORTED:
		stappler::log::source().error("XcbView",
				"XCB_CONN_CLOSED_EXT_NOTSUPPORTED: extension is not supported");
		break;
	case XCB_CONN_CLOSED_MEM_INSUFFICIENT:
		stappler::log::source().error("XcbView", "XCB_CONN_CLOSED_MEM_INSUFFICIENT: out of memory");
		break;
	case XCB_CONN_CLOSED_REQ_LEN_EXCEED:
		stappler::log::source().error("XcbView",
				"XCB_CONN_CLOSED_REQ_LEN_EXCEED: too large request");
		break;
	case XCB_CONN_CLOSED_PARSE_ERR:
		stappler::log::source().error("XcbView",
				"XCB_CONN_CLOSED_PARSE_ERR: error during parsing display string");
		break;
	case XCB_CONN_CLOSED_INVALID_SCREEN:
		stappler::log::
				error("XcbView",
						"XCB_CONN_CLOSED_INVALID_SCREEN: server does not have a screen matching "
						"the " "display");
		break;
	case XCB_CONN_CLOSED_FDPASSING_FAILED:
		stappler::log::source().error("XcbView",
				"XCB_CONN_CLOSED_FDPASSING_FAILED: fail to pass some FD");
		break;
	}
}

XcbConnection::~XcbConnection() {
	if (_support) {
		_support->invalidate();
		_support = nullptr;
	}

	if (_errors) {
		_xcb->xcb_errors_context_free(_errors);
		_errors = nullptr;
	}

	if (_cursorContext) {
		_xcb->xcb_cursor_context_free(_cursorContext);
		_cursorContext = nullptr;
	}

	if (_connection) {
		_xcb->xcb_disconnect(_connection);
		_connection = nullptr;
	}
}

XcbConnection::XcbConnection(NotNull<XcbLibrary> xcb, NotNull<XkbLibrary> xkb, StringView d)
: _xcb(xcb) {
	_connection = _xcb->xcb_connect(
			d.empty() ? nullptr : (d.terminated() ? d.data() : d.str<Interface>().data()),
			&_screenNbr); // always not null

	_setup = _xcb->xcb_get_setup(_connection);

	_socket = _xcb->xcb_get_file_descriptor(_connection); // assume it's non-blocking

	int screenNbr = _screenNbr;
	auto iter = _xcb->xcb_setup_roots_iterator(_setup);
	for (; iter.rem; --screenNbr, _xcb->xcb_screen_next(&iter)) {
		if (screenNbr == 0) {
			_screen = iter.data;
			break;
		}
	}

	if (_xcb->hasSync()) {
		auto ext = _xcb->xcb_get_extension_data(_connection, _xcb->xcb_sync_id);
		if (ext && ext->present) {
			_syncEnabled = true;
		}
	}

	// read atoms from connection
	xcb_intern_atom_cookie_t atomCookies[sizeof(s_atomRequests) / sizeof(XcbAtomInfo)];

	size_t i = 0;
	for (auto &it : s_atomRequests) {
		atomCookies[i] = _xcb->xcb_intern_atom(_connection, it.onlyIfExists ? 1 : 0, it.name.size(),
				it.name.data());
		++i;
	}

	if (_xcb->xcb_cursor_context_new(_connection, _screen, &_cursorContext) < 0) {
		log::source().warn("XcbConnection", "Fail to load cursor context");
		_cursorContext = nullptr;
	}

	memcpy(_atoms, s_atomRequests, sizeof(s_atomRequests));

	_support = Rc<XcbSupportWindow>::create(this, xkb, screenNbr);

	i = 0;
	for (auto &it : atomCookies) {
		auto reply = perform(_xcb->xcb_intern_atom_reply, it);
		if (reply) {
			_atoms[i].value = reply->atom;
			_namedAtoms.emplace(_atoms[i].name.str<Interface>(), reply->atom);
			_atomNames.emplace(reply->atom, _atoms[i].name.str<Interface>());
		} else {
			_atoms[i].value = 0;
		}
		++i;
	}

	xcb_get_property_cookie_t netSupportedCookie;
	if (auto a = getAtom(XcbAtomIndex::_NET_SUPPORTED)) {
		netSupportedCookie = _xcb->xcb_get_property(_connection, 0, _screen->root, a,
				XCB_GET_PROPERTY_TYPE_ANY, 0, maxOf<uint32_t>() / 4);
	}

	_xcb->xcb_errors_context_new(_connection, &_errors);

	_displayConfig = _support->makeDisplayConfigManager();

	if (netSupportedCookie.sequence) {
		auto reply = perform(_xcb->xcb_get_property_reply, netSupportedCookie);
		if (reply) {
			auto atoms = (xcb_atom_t *)_xcb->xcb_get_property_value(reply);
			auto len = _xcb->xcb_get_property_value_length(reply) / sizeof(xcb_atom_t);

			_capabilitiesByAtoms = makeSpanView(atoms, len).vec<Interface>();
			std::sort(_capabilitiesByAtoms.begin(), _capabilitiesByAtoms.end());

			getAtomNames(_capabilitiesByAtoms, [&](SpanView<StringView> strs) {
				_capabilitiesByNames = strs.vec<Interface>();
				std::sort(_capabilitiesByNames.begin(), _capabilitiesByNames.end());
			});
		}
	}
}

Rc<DisplayConfigManager> XcbConnection::makeDisplayConfigManager(
		Function<void(NotNull<DisplayConfigManager>)> &&cb) {
	if (_displayConfig) {
		_displayConfig->setCallback(sp::move(cb));
	}
	return _displayConfig;
}

template <typename Event>
static bool XcbConnection_forwardToWindow(StringView eventName,
		const Map<xcb_window_t, XcbWindow *> &windows, xcb_window_t window, Event *event,
		void (XcbWindow::*ptr)(Event *), Set<XcbWindow *> *eventWindows = nullptr) {
	auto wIt = windows.find(window);
	if (wIt != windows.end()) {
		(*wIt->second.*ptr)(event);
		if (eventWindows) {
			eventWindows->emplace(wIt->second);
		}
		return true;
	}
	log::source().warn("XcbConnection", "No window ", window, " attached for event ", eventName);
	return false;
}

template <typename Event>
static bool XcbConnection_forwardToWindow(StringView eventName,
		const Map<xcb_window_t, XcbWindow *> &windows, xcb_window_t window, Event *event,
		const Callback<void(Event *, XcbWindow *)> &cb) {
	auto wIt = windows.find(window);
	if (wIt != windows.end()) {
		cb(event, wIt->second);
		return true;
	}
	log::source().warn("XcbConnection", "No window ", window, " attached for event ", eventName);
	return false;
}

void XcbConnection::setSystemNotificationHandler(Function<void(SystemNotification)> &&cb) {
	_onSystemNotification = sp::move(cb);
}

uint32_t XcbConnection::poll() {
	uint32_t ret = 0;

	xcb_generic_event_t *e;

	Set<XcbWindow *> eventWindows;

	while ((e = _xcb->xcb_poll_for_event(_connection))) {
		auto et = e->response_type & 0x7f;
		switch (et) {
		case 0: {
			auto err = reinterpret_cast<xcb_generic_error_t *>(e);
			printError("Connection error", err);
			log::source().error("XcbConnection", "X11 error: ", int(err->error_code));
			break;
		}
		case XCB_EXPOSE:
			XcbConnection_forwardToWindow("XCB_EXPOSE", _windows, ((xcb_expose_event_t *)e)->window,
					(xcb_expose_event_t *)e, &XcbWindow::handleExpose, &eventWindows);
			break;
		case XCB_PROPERTY_NOTIFY: {
			auto ev = reinterpret_cast<xcb_property_notify_event_t *>(e);
			auto it = _windows.find(ev->window);
			if (it != _windows.end()) {
				it->second->handlePropertyNotify(ev);
			} else {
				_support->handlePropertyNotify(ev);
			}
			break;
		}
		case XCB_VISIBILITY_NOTIFY: XL_X11_LOG("XCB_VISIBILITY_NOTIFY"); break;
		case XCB_MAP_NOTIFY: XL_X11_LOG("XCB_MAP_NOTIFY"); break;
		case XCB_REPARENT_NOTIFY: XL_X11_LOG("XCB_REPARENT_NOTIFY"); break;
		case XCB_COLORMAP_NOTIFY: XL_X11_LOG("XCB_COLORMAP_NOTIFY"); break;
		case XCB_CONFIGURE_REQUEST: XL_X11_LOG("XCB_CONFIGURE_REQUEST"); break;
		case XCB_RESIZE_REQUEST: XL_X11_LOG("XCB_RESIZE_REQUEST"); break;

		case XCB_SELECTION_NOTIFY:
			if (reinterpret_cast<xcb_selection_notify_event_t *>(e)->requestor
					== _support->getWindow()) {
				_support->handleSelectionNotify(
						reinterpret_cast<xcb_selection_notify_event_t *>(e));
			}
			break;
		case XCB_SELECTION_CLEAR:
			if (reinterpret_cast<xcb_selection_clear_event_t *>(e)->owner
					== _support->getWindow()) {
				_support->handleSelectionClear(reinterpret_cast<xcb_selection_clear_event_t *>(e));
			}
			break;
		case XCB_SELECTION_REQUEST:
			_support->handleSelectionRequest((xcb_selection_request_event_t *)e);
			break;
		case XCB_BUTTON_PRESS:
			XcbConnection_forwardToWindow("XCB_BUTTON_PRESS", _windows,
					((xcb_button_press_event_t *)e)->event, (xcb_button_press_event_t *)e,
					&XcbWindow::handleButtonPress, &eventWindows);
			break;
		case XCB_BUTTON_RELEASE:
			XcbConnection_forwardToWindow("XCB_BUTTON_RELEASE", _windows,
					((xcb_button_release_event_t *)e)->event, (xcb_button_release_event_t *)e,
					&XcbWindow::handleButtonRelease, &eventWindows);
			break;
		case XCB_MOTION_NOTIFY:
			XcbConnection_forwardToWindow("XCB_MOTION_NOTIFY", _windows,
					((xcb_motion_notify_event_t *)e)->event, (xcb_motion_notify_event_t *)e,
					&XcbWindow::handleMotionNotify, &eventWindows);
			break;

		case XCB_ENTER_NOTIFY:
			XcbConnection_forwardToWindow("XCB_ENTER_NOTIFY", _windows,
					((xcb_enter_notify_event_t *)e)->event, (xcb_enter_notify_event_t *)e,
					&XcbWindow::handleEnterNotify, &eventWindows);
			break;
		case XCB_LEAVE_NOTIFY:
			XcbConnection_forwardToWindow("XCB_LEAVE_NOTIFY", _windows,
					((xcb_leave_notify_event_t *)e)->event, (xcb_leave_notify_event_t *)e,
					&XcbWindow::handleLeaveNotify, &eventWindows);
			break;
		case XCB_FOCUS_IN:
			XcbConnection_forwardToWindow("XCB_FOCUS_IN", _windows,
					((xcb_focus_in_event_t *)e)->event, (xcb_focus_in_event_t *)e,
					&XcbWindow::handleFocusIn, &eventWindows);
			// Update key mappings in case layout was changed
			_support->updateKeysymMapping();
			break;
		case XCB_FOCUS_OUT:
			XcbConnection_forwardToWindow("XCB_FOCUS_OUT", _windows,
					((xcb_focus_out_event_t *)e)->event, (xcb_focus_out_event_t *)e,
					&XcbWindow::handleFocusOut, &eventWindows);
			break;
		case XCB_KEY_PRESS:
			XcbConnection_forwardToWindow("XCB_KEY_PRESS", _windows,
					((xcb_key_press_event_t *)e)->event, (xcb_key_press_event_t *)e,
					&XcbWindow::handleKeyPress, &eventWindows);
			break;
		case XCB_KEY_RELEASE:
			XcbConnection_forwardToWindow("XCB_KEY_RELEASE", _windows,
					((xcb_key_release_event_t *)e)->event, (xcb_key_release_event_t *)e,
					&XcbWindow::handleKeyRelease, &eventWindows);
			break;
		case XCB_CONFIGURE_NOTIFY:
			XcbConnection_forwardToWindow("XCB_CONFIGURE_NOTIFY", _windows,
					((xcb_configure_notify_event_t *)e)->event, (xcb_configure_notify_event_t *)e,
					&XcbWindow::handleConfigureNotify, &eventWindows);
			break;
		case XCB_CLIENT_MESSAGE:
			XcbConnection_forwardToWindow("XCB_CLIENT_MESSAGE", _windows,
					((xcb_client_message_event_t *)e)->window, (xcb_client_message_event_t *)e,
					Callback<void(xcb_client_message_event_t *, XcbWindow *)>(
							[&](xcb_client_message_event_t *event, XcbWindow *w) {
				if (event->type == _atoms[toInt(XcbAtomIndex::WM_PROTOCOLS)].value) {
					if (event->data.data32[0]
							== _atoms[toInt(XcbAtomIndex::WM_DELETE_WINDOW)].value) {
						w->handleCloseRequest();
					} else if (event->data.data32[0]
							== _atoms[toInt(XcbAtomIndex::_NET_WM_SYNC_REQUEST)].value) {
						xcb_sync_int64_t value;
						value.lo = event->data.data32[2];
						value.hi = static_cast<int32_t>(event->data.data32[3]);
						w->handleSyncRequest(event->data.data32[1], value);
					} else if (event->data.data32[0]
							== _atoms[toInt(XcbAtomIndex::_NET_WM_PING)].value) {
						if (event->window == _screen->root) {
							return;
						}

						xcb_client_message_event_t reply = *event;
						reply.response_type = XCB_CLIENT_MESSAGE;
						reply.window = _screen->root;
						_xcb->xcb_send_event(_connection, 0, _screen->root,
								XCB_EVENT_MASK_STRUCTURE_NOTIFY
										| XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT,
								(const char *)&reply);
						_xcb->xcb_flush(_connection);
					} else {
						log::source().error("XcbView", "Unknown protocol message: ", event->window,
								" of type ", event->type, ": ", event->data.data32[0]);
					}
				} else {
					log::source().error("XcbView", "Unknown client message: ", event->window,
							" of type ", event->type, ": ", event->data.data32[0]);
				}
			}));
			break;
		case XCB_MAPPING_NOTIFY:
			_support->handleMappingNotify(reinterpret_cast<xcb_mapping_notify_event_t *>(e));
			break;
		default: _support->handleExtensionEvent(et, e); break;
		}

		/* Free the Generic Event */
		::free(e);

		++ret;
	}

	for (auto &it : eventWindows) { it->dispatchPendingEvents(); }

	return ret;
}

bool XcbConnection::hasErrors() const {
	auto err = _xcb->xcb_connection_has_error(_connection);
	if (err != 0) {
		ReportError(err);
		return true;
	}
	return false;
}

core::InputKeyCode XcbConnection::getKeyCode(xcb_keycode_t code) const {
	return _support->getKeyCode(code);
}

xcb_keysym_t XcbConnection::getKeysym(xcb_keycode_t code, uint16_t state, bool resolveMods) const {
	return _support->getKeysym(code, state, resolveMods);
}

void XcbConnection::fillTextInputData(core::InputEventData &data, xcb_keycode_t code,
		uint16_t state, bool textInputEnabled, bool compose) {
	_support->fillTextInputData(data, code, state, textInputEnabled, compose);
}

xcb_atom_t XcbConnection::getAtom(XcbAtomIndex index) const { return _atoms[toInt(index)].value; }

xcb_atom_t XcbConnection::getAtom(StringView name, bool onlyIfExists) const {
	auto it = _namedAtoms.find(name);
	if (it != _namedAtoms.end()) {
		return it->second;
	}

	auto cookie = _xcb->xcb_intern_atom(_connection, onlyIfExists, name.size(), name.data());

	xcb_generic_error_t *error = nullptr;
	auto reply = _xcb->xcb_intern_atom_reply(_connection, cookie, &error);
	if (error || !reply) {
		printError(toString("Fail to xcb_intern_atom_reply for '", name, "'"), error);
		if (error) {
			::free(error);
		}
	}

	if (reply) {
		auto atom = _namedAtoms.emplace(name.str<Interface>(), reply->atom).first->second;
		_atomNames.emplace(reply->atom, name.str<Interface>());
		free(reply);
		return atom;
	}

	return 0;
}

StringView XcbConnection::getAtomName(xcb_atom_t atom) const {
	auto it = _atomNames.find(atom);
	if (it != _atomNames.end()) {
		return it->second;
	}

	auto cookie = _xcb->xcb_get_atom_name_unchecked(_connection, atom);
	auto reply = _xcb->xcb_get_atom_name_reply(_connection, cookie, nullptr);
	if (reply) {
		auto data = _xcb->xcb_get_atom_name_name(reply);
		auto ret = String(data, _xcb->xcb_get_atom_name_name_length(reply));
		auto it = _atomNames.emplace(atom, ret).first;
		::free(reply);
		return it->second;
	}
	return StringView();
}

bool XcbConnection::hasCapability(XcbAtomIndex index) const {
	if (auto a = getAtom(index)) {
		return hasCapability(a);
	}
	return false;
}

bool XcbConnection::hasCapability(xcb_atom_t atom) const {
	auto lb = std::lower_bound(_capabilitiesByAtoms.begin(), _capabilitiesByAtoms.end(), atom);
	if (lb != _capabilitiesByAtoms.end() && *lb == atom) {
		return true;
	}
	return false;
}

bool XcbConnection::hasCapability(StringView str) const {
	auto lb = std::lower_bound(_capabilitiesByNames.begin(), _capabilitiesByNames.end(), str);
	if (lb != _capabilitiesByNames.end() && *lb == str) {
		return true;
	}
	return false;
}

void XcbConnection::getAtomNames(SpanView<xcb_atom_t> ids,
		const Callback<void(SpanView<StringView>)> &cb) {
	Vector<StringView> names;
	names.resize(ids.size());

	Vector<std::tuple<xcb_get_atom_name_cookie_t, xcb_atom_t, StringView *>> cookies;

	uint32_t idx = 0;
	for (auto &id : ids) {
		auto iit = _atomNames.find(id);
		if (iit != _atomNames.end()) {
			names[idx] = iit->second;
		} else {
			cookies.emplace_back(_xcb->xcb_get_atom_name_unchecked(_connection, id), id,
					&names[idx]);
		}
		++idx;
	}

	for (auto &it : cookies) {
		auto reply = _xcb->xcb_get_atom_name_reply(_connection, std::get<0>(it), nullptr);
		if (reply) {
			auto data = _xcb->xcb_get_atom_name_name(reply);
			auto ret = String(data, _xcb->xcb_get_atom_name_name_length(reply));
			auto aIt = _atomNames.emplace(std::get<1>(it), ret).first;
			_namedAtoms.emplace(ret, std::get<1>(it));
			*std::get<2>(it) = aIt->second;
			::free(reply);
		}
	}

	cb(names);
}

void XcbConnection::getAtoms(SpanView<String> names,
		const Callback<void(SpanView<xcb_atom_t>)> &cb) {
	Vector<StringView> views;
	views.reserve(names.size());
	for (auto &it : names) { views.emplace_back(it); }
	getAtoms(views, cb);
}

void XcbConnection::getAtoms(SpanView<StringView> names,
		const Callback<void(SpanView<xcb_atom_t>)> &cb) {
	Vector<xcb_atom_t> atoms;
	atoms.resize(names.size());

	Vector<std::tuple<xcb_intern_atom_cookie_t, StringView, xcb_atom_t *>> cookies;

	uint32_t idx = 0;
	for (auto &name : names) {
		auto iit = _namedAtoms.find(name);
		if (iit != _namedAtoms.end()) {
			atoms[idx] = iit->second;
		} else {
			cookies.emplace_back(_xcb->xcb_intern_atom(_connection, 0, name.size(), name.data()),
					name, &atoms[idx]);
		}
		++idx;
	}

	for (auto &it : cookies) {
		auto reply = _xcb->xcb_intern_atom_reply(_connection, std::get<0>(it), nullptr);
		if (reply) {
			auto atom = reply->atom;
			auto aIt = _namedAtoms.emplace(std::get<1>(it).str<Interface>(), atom).first;
			_atomNames.emplace(atom, std::get<1>(it).str<Interface>());
			*std::get<2>(it) = aIt->second;
			::free(reply);
		}
	}

	cb(atoms);
}

const char *XcbConnection::getErrorMajorName(uint8_t major) const {
	return _xcb->xcb_errors_get_name_for_major_code(_errors, major);
}

const char *XcbConnection::getErrorMinorName(uint8_t major, uint16_t minor) const {
	return _xcb->xcb_errors_get_name_for_minor_code(_errors, major, minor);
}

const char *XcbConnection::getErrorName(uint8_t errorCode) const {
	return _xcb->xcb_errors_get_name_for_error(_errors, errorCode, nullptr);
}

xcb_visualid_t XcbConnection::getVisualByDepth(uint16_t depth) const {
	xcb_depth_iterator_t depth_iter;

	depth_iter = _xcb->xcb_screen_allowed_depths_iterator(_screen);
	for (; depth_iter.rem; _xcb->xcb_depth_next(&depth_iter)) {
		if (depth_iter.data->depth != depth) {
			continue;
		}

		xcb_visualtype_iterator_t visual_iter;

		visual_iter = _xcb->xcb_depth_visuals_iterator(depth_iter.data);
		if (!visual_iter.rem) {
			continue;
		}
		return visual_iter.data->visual_id;
	}
	return 0;
}

bool XcbConnection::createWindow(const WindowInfo *winfo, XcbWindowInfo &xinfo) const {
	auto doCreate = [&](xcb_visualid_t visual, xcb_colormap_t colormap, xcb_window_t parent,
							uint32_t depth, xcb_rectangle_t rect, bool overrideRedirect,
							uint32_t eventMask) -> xcb_window_t {
		uint32_t mask =
				XCB_CW_OVERRIDE_REDIRECT | XCB_CW_SAVE_UNDER | XCB_CW_EVENT_MASK | XCB_CW_COLORMAP;
		if (visual != _screen->root_visual) {
			mask |= XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL;
		}

		uint32_t idx = 0;
		uint32_t values[6];

		if (visual != _screen->root_visual) {
			values[idx++] = _screen->black_pixel;
			values[idx++] = _screen->black_pixel;
		}

		values[idx++] = overrideRedirect;
		values[idx++] = 0; // XCB_CW_SAVE_UNDER
		values[idx++] = eventMask;
		values[idx++] = colormap;

		auto window = _xcb->xcb_generate_id(_connection);

		_xcb->xcb_create_window(_connection, depth, window, parent, rect.x, rect.y, rect.width,
				rect.height,
				0, // border_width
				XCB_WINDOW_CLASS_INPUT_OUTPUT,
				visual, // visual
				mask, values);
		return window;
	};

	xinfo.colormap = _xcb->xcb_generate_id(_connection);
	_xcb->xcb_create_colormap(_connection, XCB_COLORMAP_ALLOC_NONE, xinfo.colormap, xinfo.parent,
			xinfo.visual);

	xinfo.window = doCreate(xinfo.visual, xinfo.colormap, xinfo.parent, xinfo.depth,
			xinfo.boundingRect, xinfo.overrideRedirect, xinfo.eventMask);

	if (!xinfo.title.empty()) {
		_xcb->xcb_change_property(_connection, XCB_PROP_MODE_REPLACE, xinfo.window,
				XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8, xinfo.title.size(), xinfo.title.data());
	}
	if (!xinfo.icon.empty()) {
		_xcb->xcb_change_property(_connection, XCB_PROP_MODE_REPLACE, xinfo.window,
				XCB_ATOM_WM_ICON_NAME, XCB_ATOM_STRING, 8, xinfo.icon.size(), xinfo.icon.data());
	}
	if (!xinfo.wmClass.empty()) {
		_xcb->xcb_change_property(_connection, XCB_PROP_MODE_REPLACE, xinfo.window,
				XCB_ATOM_WM_CLASS, XCB_ATOM_STRING, 8, xinfo.wmClass.size(), xinfo.wmClass.data());
	}

	char buf[512] = {0};
	if (::gethostname(buf, 512) == 0) {
		_xcb->xcb_change_property(_connection, XCB_PROP_MODE_REPLACE, xinfo.window,
				XCB_ATOM_WM_CLIENT_MACHINE, XCB_ATOM_STRING, 8, strlen(buf), buf);
	}

	auto pid = ::getpid();
	if (auto a = getAtom(XcbAtomIndex::_NET_WM_PID)) {
		_xcb->xcb_change_property(_connection, XCB_PROP_MODE_REPLACE, xinfo.window, a,
				XCB_ATOM_CARDINAL, 32, 1, &pid);
	}

	uint32_t nProtocols = 0;
	xcb_atom_t protocolAtoms[2];

	if (xinfo.overrideClose && _atoms[toInt(XcbAtomIndex::WM_DELETE_WINDOW)].value) {
		protocolAtoms[nProtocols++] = _atoms[toInt(XcbAtomIndex::WM_DELETE_WINDOW)].value;
	}

	if (_atoms[toInt(XcbAtomIndex::_NET_WM_PING)].value) {
		protocolAtoms[nProtocols++] = _atoms[toInt(XcbAtomIndex::_NET_WM_PING)].value;
	}

	if (_syncEnabled && xinfo.enableSync
			&& _atoms[toInt(XcbAtomIndex::_NET_WM_SYNC_REQUEST)].value) {
		xinfo.syncValue.hi = 0;
		xinfo.syncValue.lo = 0;

		xinfo.syncCounter = _xcb->xcb_generate_id(_connection);
		_xcb->xcb_sync_create_counter(_connection, xinfo.syncCounter, xinfo.syncValue);
		_xcb->xcb_change_property(_connection, XCB_PROP_MODE_REPLACE, xinfo.window,
				_atoms[toInt(XcbAtomIndex::_NET_WM_SYNC_REQUEST_COUNTER)].value, XCB_ATOM_CARDINAL,
				32, 1, &xinfo.syncCounter);
	}

	if (nProtocols && _atoms[toInt(XcbAtomIndex::WM_PROTOCOLS)].value) {
		_xcb->xcb_change_property(_connection, XCB_PROP_MODE_REPLACE, xinfo.window,
				_atoms[toInt(XcbAtomIndex::WM_PROTOCOLS)].value, XCB_ATOM_ATOM, 32, nProtocols,
				protocolAtoms);
	}

	xinfo.outputWindow = doCreate(xinfo.visual, xinfo.colormap, xinfo.window, xinfo.depth,
			xinfo.contentRect, true, 0);

	_xcb->xcb_change_property(_connection, XCB_PROP_MODE_REPLACE, xinfo.outputWindow,
			XCB_ATOM_WM_TRANSIENT_FOR, XCB_ATOM_WINDOW, 32, 1, &xinfo.window);

	if (hasCapability(XcbAtomIndex::_GTK_FRAME_EXTENTS)) {
		auto padding = FrameExtents::getExtents(xinfo.boundingRect, xinfo.contentRect);
		_xcb->xcb_change_property(getConnection(), XCB_PROP_MODE_REPLACE, xinfo.window,
				getAtom(XcbAtomIndex::_GTK_FRAME_EXTENTS), XCB_ATOM_CARDINAL, 32, 4, &padding);
	}

	xinfo.decorationGc = _xcb->xcb_generate_id(_connection);

	uint32_t values[2] = {XCB_SUBWINDOW_MODE_CLIP_BY_CHILDREN, 0};

	_xcb->xcb_create_gc(_connection, xinfo.decorationGc, xinfo.window, XCB_GC_SUBWINDOW_MODE,
			values);

	_xcb->xcb_flush(_connection);

	if (!hasErrors()) {
		return true;
	}

	xinfo.window = 0;
	xinfo.syncCounter = 0;

	return false;
}

void XcbConnection::attachWindow(xcb_window_t window, XcbWindow *iface) {
	_windows.emplace(window, iface);
}

void XcbConnection::detachWindow(xcb_window_t window) { _windows.erase(window); }

void XcbConnection::notifyScreenChange() {
	for (auto &it : _windows) { it.second->notifyScreenChange(); }
}

xcb_cursor_t XcbConnection::loadCursor(WindowCursor c) {
	auto it = _cursors.find(c);
	if (it != _cursors.end()) {
		return it->second;
	}

	xcb_cursor_t cursor = XCB_CURSOR_NONE;
	auto list = getCursorNames(c);
	for (auto &it : list) {
		cursor = _xcb->xcb_cursor_load_cursor(_cursorContext,
				it.terminated() ? it.data() : it.str<Interface>().data());
		if (cursor != XCB_CURSOR_NONE) {
			_cursors.emplace(c, cursor);
			return cursor;
		}
	}
	if (cursor == XCB_CURSOR_NONE && c != WindowCursor::Default) {
		cursor = loadCursor(WindowCursor::Default);
	}
	_cursors.emplace(c, cursor);
	return cursor;
}

bool XcbConnection::isCursorSupported(WindowCursor cursor) {
	if (loadCursor(cursor) != XCB_CURSOR_NONE) {
		return true;
	}
	return false;
}

WindowCapabilities XcbConnection::getCapabilities() const {
	WindowCapabilities caps = WindowCapabilities::ServerSideDecorations
			| WindowCapabilities::AboveBelowState | WindowCapabilities::DemandsAttentionState
			| WindowCapabilities::SkipTaskbarState | WindowCapabilities::CloseGuard
			| WindowCapabilities::UserShadowsRequired;

	if (getVisualByDepth(32) != 0 && getAtom(XcbAtomIndex::_MOTIF_WM_HINTS) != 0
			&& hasCapability(XcbAtomIndex::_GTK_EDGE_CONSTRAINTS)) {
		caps |= WindowCapabilities::UserSpaceDecorations;
	}

	if (hasCapability(XcbAtomIndex::_NET_WM_STATE_FULLSCREEN)) {
		caps |= WindowCapabilities::Fullscreen | WindowCapabilities::FullscreenWithMode;
	}

	if (hasCapability(XcbAtomIndex::_NET_WM_BYPASS_COMPOSITOR)) {
		caps |= WindowCapabilities::FullscreenExclusive;
	}

	return caps;
}

bool XcbConnection::setCursorId(xcb_window_t window, uint32_t cursorId) {
	_xcb->xcb_change_window_attributes(_connection, window, XCB_CW_CURSOR, (uint32_t[]){cursorId});
	_xcb->xcb_flush(_connection);
	return true;
}

Status XcbConnection::readFromClipboard(Rc<ClipboardRequest> &&req) {
	return _support->readFromClipboard(sp::move(req));
}

Status XcbConnection::probeClipboard(Rc<ClipboardProbe> &&probe) {
	return _support->probeClipboard(sp::move(probe));
}

Status XcbConnection::writeToClipboard(Rc<ClipboardData> &&data) {
	return _support->writeToClipboard(sp::move(data));
}

Value XcbConnection::getSettingsValue(StringView key) const {
	return _support->getSettingsValue(key);
}
uint32_t XcbConnection::getUnscaledDpi() const { return _support->getUnscaledDpi(); }
uint32_t XcbConnection::getDpi() const { return _support->getDpi(); }

static StringView XcbConnection_getCodeName(uint8_t code) {
	switch (code) {
	case 1: return "BadRequset"; break;
	case 2: return "BadValue"; break;
	case 3: return "BadWindow"; break;
	case 4: return "BadPixmap"; break;
	case 5: return "BadAtom"; break;
	case 6: return "BadCursor"; break;
	case 7: return "BadFont"; break;
	case 8: return "BadMatch"; break;
	case 9: return "BadDrawable"; break;
	case 10: return "BadAccess"; break;
	case 11: return "BadAlloc"; break;
	case 12: return "BadColormap"; break;
	case 13: return "BadGContext"; break;
	case 14: return "IDChoice"; break;
	case 15: return "Name"; break;
	case 16: return "Length"; break;
	case 17: return "Implementation"; break;
	default: break;
	};
	return StringView();
};

void XcbConnection::printError(StringView message, xcb_generic_error_t *error) const {
	if (error) {
		log::source().error("XcbConnection", message, "; code=", error->error_code, " (",
				XcbConnection_getCodeName(error->error_code),
				") " "; major=", getErrorMajorName(error->major_code),
				"; minor=", getErrorMinorName(error->major_code, error->minor_code),
				"; name=", getErrorName(error->error_code));
	} else {
		log::source().error("XcbConnection", message, "; no error reported");
	}
}

void XcbConnection::handleSettingsUpdate() {
	for (auto &it : _windows) { it.second->handleSettingsUpdated(); }

	if (_onSystemNotification) {
		_onSystemNotification(SystemNotification::ConfigurationChanged);
	}
}

void XcbConnection::handleScreenUpdate() {
	if (_displayConfig) {
		_displayConfig->update();
	}
}

void XcbConnection::handleClipboardChanged() {
	if (_onSystemNotification) {
		_onSystemNotification(SystemNotification::ClipboardChanged);
	}
}

bool XcbConnection::checkCookie(xcb_void_cookie_t cookie, StringView errMessage) {
	auto error = Ptr(_xcb->xcb_request_check(_connection, cookie));
	if (error) {
		printError(errMessage, error);
		return false;
	}
	return true;
}

} // namespace stappler::xenolith::platform
