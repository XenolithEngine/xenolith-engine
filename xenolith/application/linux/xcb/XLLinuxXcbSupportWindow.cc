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

#include "XLLinuxXcbSupportWindow.h"

#include <X11/keysym.h>

// use GLFW mappings as a fallback for XKB
uint32_t _glfwKeySym2Unicode(unsigned int keysym);

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

XcbSupportWindow::~XcbSupportWindow() { invalidate(); }

XcbSupportWindow::XcbSupportWindow(NotNull<XcbConnection> conn, NotNull<XkbLibrary> xkb,
		int screenNbr)
: _connection(conn), _xcb(conn->getXcb()), _xkb(xkb) {

	if (_xkb.lib && _xkb.lib->hasX11() && _xcb->hasXkb()) {
		_xkb.initXcb(_xcb, _connection->getConnection());
	}

	_maxRequestSize = _xcb->xcb_get_maximum_request_length(_connection->getConnection());
	_safeReqeustSize = std::min(_maxRequestSize, _safeReqeustSize);

	xcb_randr_query_version_cookie_t randrVersionCookie;
	xcb_xfixes_query_version_cookie_t xfixesVersionCookie;
	xcb_shape_query_version_cookie_t shapeVersionCookie;

	if (_xcb->hasRandr()) {
		auto ext = _xcb->xcb_get_extension_data(_connection->getConnection(), _xcb->xcb_randr_id);
		if (ext && ext->present) {
			_randr.enabled = true;
			_randr.firstEvent = ext->first_event;
			randrVersionCookie = _xcb->xcb_randr_query_version(_connection->getConnection(),
					XcbLibrary::RANDR_MAJOR_VERSION, XcbLibrary::RANDR_MINOR_VERSION);
		}
	}

	if (_xcb->hasXfixes()) {
		auto ext = _xcb->xcb_get_extension_data(_connection->getConnection(), _xcb->xcb_xfixes_id);
		if (ext && ext->present) {
			_xfixes.enabled = true;
			_xfixes.firstEvent = ext->first_event;
			_xfixes.firstError = ext->first_error;

			xfixesVersionCookie = _xcb->xcb_xfixes_query_version(_connection->getConnection(),
					XcbLibrary::XFIXES_MAJOR_VERSION, XcbLibrary::XFIXES_MINOR_VERSION);
		}
	}

	if (_xcb->hasShape()) {
		auto ext = _xcb->xcb_get_extension_data(_connection->getConnection(), _xcb->xcb_shape_id);
		if (ext && ext->present) {
			_shape.enabled = true;
			_shape.firstEvent = ext->first_event;
			_shape.firstError = ext->first_error;

			shapeVersionCookie = _xcb->xcb_shape_query_version(_connection->getConnection());
		}
	}

	// make fake window for clipboard
	uint32_t mask = XCB_CW_EVENT_MASK;
	uint32_t values[2];
	values[0] = XCB_EVENT_MASK_PROPERTY_CHANGE | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT
			| XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY;

	_window = _xcb->xcb_generate_id(_connection->getConnection());

	_xcb->xcb_create_window(_connection->getConnection(),
			XCB_COPY_FROM_PARENT, // depth (same as root)
			_window, // window Id
			_connection->getDefaultScreen()->root, // parent window
			0, 0, 100, 100,
			0, // border_width
			XCB_WINDOW_CLASS_INPUT_ONLY, // class
			_connection->getDefaultScreen()->root_visual, // visual
			mask, values);

	if (_randr.enabled) {
		if (auto versionReply = _connection->perform(_xcb->xcb_randr_query_version_reply,
					randrVersionCookie)) {
			_randr.majorVersion = versionReply->major_version;
			_randr.minorVersion = versionReply->minor_version;
			_randr.initialized = true;

			_xcb->xcb_randr_select_input(_connection->getConnection(), _window,
					XCB_RANDR_NOTIFY_MASK_SCREEN_CHANGE | XCB_RANDR_NOTIFY_MASK_CRTC_CHANGE
							| XCB_RANDR_NOTIFY_MASK_OUTPUT_CHANGE);
		}
	}

	if (_xfixes.enabled) {
		if (auto versionReply = _connection->perform(_xcb->xcb_xfixes_query_version_reply,
					xfixesVersionCookie)) {
			_xfixes.majorVersion = versionReply->major_version;
			_xfixes.minorVersion = versionReply->minor_version;
			_xfixes.initialized = true;

			_xcb->xcb_xfixes_select_selection_input(_connection->getConnection(), _window,
					_connection->getAtom(XcbAtomIndex::CLIPBOARD),
					XCB_XFIXES_SELECTION_EVENT_MASK_SET_SELECTION_OWNER
							| XCB_XFIXES_SELECTION_EVENT_MASK_SELECTION_WINDOW_DESTROY
							| XCB_XFIXES_SELECTION_EVENT_MASK_SELECTION_CLIENT_CLOSE);
		}
	}

	if (_shape.enabled) {
		if (auto versionReply = _connection->perform(_xcb->xcb_shape_query_version_reply,
					shapeVersionCookie)) {
			_shape.majorVersion = versionReply->major_version;
			_shape.minorVersion = versionReply->minor_version;
			_shape.initialized = true;
		}
	}

	// try XSETTINGS
	_xsettings.selection = _connection->getAtom(toString("_XSETTINGS_S", screenNbr), true);
	_xsettings.property = _connection->getAtom(XcbAtomIndex::_XSETTINGS_SETTINGS);
	if (_xsettings.selection && _xsettings.property) {
		auto cookie =
				_xcb->xcb_get_selection_owner(_connection->getConnection(), _xsettings.selection);
		auto reply = _connection->perform(_xcb->xcb_get_selection_owner_reply, cookie);
		if (reply && reply->owner) {
			_xsettings.owner = reply->owner;
			const uint32_t values[] = {
				XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_PROPERTY_CHANGE};
			_xcb->xcb_change_window_attributes(_connection->getConnection(), reply->owner,
					XCB_CW_EVENT_MASK, values);
			readXSettings();
		}
	}
}

void XcbSupportWindow::invalidate() {
	if (_xcb && _connection) {
		if (_keys.keysyms) {
			_xcb->xcb_key_symbols_free(_keys.keysyms);
			_keys.keysyms = nullptr;
		}
		if (_window) {
			_xcb->xcb_destroy_window(_connection->getConnection(), _window);
			_window = 0;
		}
	}

	_xcb = nullptr;
	_connection = nullptr;
}

Rc<XcbDisplayConfigManager> XcbSupportWindow::makeDisplayConfigManager() const {
	if (_randr.initialized && _randr.majorVersion == 1 && _randr.minorVersion >= 5) {
		return Rc<XcbDisplayConfigManager>::create(_connection, nullptr);
	}
	return nullptr;
}

Status XcbSupportWindow::readFromClipboard(Rc<ClipboardRequest> &&req) {
	if (!_owner) {
		auto reply = _connection->perform(_xcb->xcb_get_selection_owner_reply,
				_xcb->xcb_get_selection_owner(_connection->getConnection(),
						_connection->getAtom(XcbAtomIndex::CLIPBOARD)));
		if (reply) {
			_owner = reply->owner;
		}
	}

	if (!_owner) {
		// there is no clipboard
		req->dataCallback(Status::Declined, BytesView(), StringView());
		return Status::Declined;
	}

	if (_owner == _window) {
		Vector<StringView> views;
		views.reserve(_data->types.size());
		for (auto &it : _data->types) { views.emplace_back(it); }
		auto type = req->typeCallback(views);
		if (type.empty() || std::find(views.begin(), views.end(), type) == views.end()) {
			req->dataCallback(Status::Ok, BytesView(), StringView());
		} else {
			auto data = _data->encodeCallback(type);
			req->dataCallback(Status::Ok, data, type);
		}
	} else {
		if (_requests.empty() && _waiters.empty()) {
			// acquire list of formats
			_xcb->xcb_convert_selection(_connection->getConnection(), _window,
					_connection->getAtom(XcbAtomIndex::CLIPBOARD),
					_connection->getAtom(XcbAtomIndex::TARGETS),
					_connection->getAtom(XcbAtomIndex::XENOLITH_CLIPBOARD), XCB_CURRENT_TIME);
			_xcb->xcb_flush(_connection->getConnection());
		}

		_requests.emplace_back(sp::move(req));
	}
	return Status::Ok;
}

Status XcbSupportWindow::probeClipboard(Rc<ClipboardProbe> &&probe) {
	if (!_owner) {
		auto reply = _connection->perform(_xcb->xcb_get_selection_owner_reply,
				_xcb->xcb_get_selection_owner(_connection->getConnection(),
						_connection->getAtom(XcbAtomIndex::CLIPBOARD)));
		if (reply) {
			_owner = reply->owner;
		}
	}

	if (!_owner) {
		// there is no clipboard
		probe->typeCallback(Status::Declined, SpanView<StringView>());
		return Status::Declined;
	}

	if (_owner == _window) {
		Vector<StringView> views;
		views.reserve(_data->types.size());
		for (auto &it : _data->types) { views.emplace_back(it); }
		probe->typeCallback(Status::Ok, views);
		return Status::Ok;
	} else {
		if (_requests.empty() && _waiters.empty()) {
			// acquire list of formats
			_xcb->xcb_convert_selection(_connection->getConnection(), _window,
					_connection->getAtom(XcbAtomIndex::CLIPBOARD),
					_connection->getAtom(XcbAtomIndex::TARGETS),
					_connection->getAtom(XcbAtomIndex::XENOLITH_CLIPBOARD), XCB_CURRENT_TIME);
			_xcb->xcb_flush(_connection->getConnection());
		}

		_probes.emplace_back(sp::move(probe));
	}
	return Status::Ok;
}

Status XcbSupportWindow::writeToClipboard(Rc<ClipboardData> &&data) {
	Vector<xcb_atom_t> atoms{
		_connection->getAtom(XcbAtomIndex::TARGETS),
		_connection->getAtom(XcbAtomIndex::TIMESTAMP),
		_connection->getAtom(XcbAtomIndex::MULTIPLE),
		_connection->getAtom(XcbAtomIndex::SAVE_TARGETS),
	};

	_connection->getAtoms(data->types, [&](SpanView<xcb_atom_t> a) {
		for (auto &it : a) { atoms.emplace_back(it); }
	});

	if (std::find(data->types.begin(), data->types.end(), "text/plain") != data->types.end()) {
		atoms.emplace_back(_connection->getAtom(XcbAtomIndex::UTF8_STRING));
		atoms.emplace_back(_connection->getAtom(XcbAtomIndex::TEXT));
		atoms.emplace_back(XCB_ATOM_STRING);
	}

	_data = move(data);
	_typeAtoms = sp::move(atoms);

	_xcb->xcb_set_selection_owner(_connection->getConnection(), _window,
			_connection->getAtom(XcbAtomIndex::CLIPBOARD), XCB_CURRENT_TIME);

	auto cookie = _xcb->xcb_get_selection_owner(_connection->getConnection(),
			_connection->getAtom(XcbAtomIndex::CLIPBOARD));
	auto reply = _connection->perform(_xcb->xcb_get_selection_owner_reply, cookie);
	if (reply->owner != _window) {
		_data = nullptr;
		_typeAtoms.clear();
	}
	return Status::Ok;
}

void XcbSupportWindow::continueClipboardProcessing() {
	if (!_waiters.empty()) {
		auto firstType = _waiters.begin()->first;
		_xcb->xcb_convert_selection(_connection->getConnection(), _window,
				_connection->getAtom(XcbAtomIndex::CLIPBOARD), firstType,
				_connection->getAtom(XcbAtomIndex::XENOLITH_CLIPBOARD), XCB_CURRENT_TIME);
		_xcb->xcb_flush(_connection->getConnection());
	} else if (!_requests.empty()) {
		_xcb->xcb_convert_selection(_connection->getConnection(), _window,
				_connection->getAtom(XcbAtomIndex::CLIPBOARD),
				_connection->getAtom(XcbAtomIndex::TARGETS),
				_connection->getAtom(XcbAtomIndex::XENOLITH_CLIPBOARD), XCB_CURRENT_TIME);
		_xcb->xcb_flush(_connection->getConnection());
	}
}

void XcbSupportWindow::finalizeClipboardWaiters(BytesView data, xcb_atom_t type) {
	auto wIt = _waiters.equal_range(type);
	auto typeName = _connection->getAtomName(type);
	if (typeName == "STRING" || typeName == "UTF8_STRING" || typeName == "TEXT") {
		typeName = StringView("text/plain");
	}
	for (auto it = wIt.first; it != wIt.second; ++it) {
		it->second->dataCallback(Status::Ok, data, typeName);
	}

	_waiters.erase(wIt.first, wIt.second);
}

void XcbSupportWindow::handleSelectionNotify(xcb_selection_notify_event_t *event) {
	if (event->property == _connection->getAtom(XcbAtomIndex::XENOLITH_CLIPBOARD)) {
		if (event->target == _connection->getAtom(XcbAtomIndex::TARGETS)) {
			auto cookie = _xcb->xcb_get_property_unchecked(_connection->getConnection(), 1, _window,
					_connection->getAtom(XcbAtomIndex::XENOLITH_CLIPBOARD), XCB_ATOM_ATOM, 0,
					maxOf<uint32_t>() / 4);
			auto reply = _connection->perform(_xcb->xcb_get_property_reply, cookie);
			if (reply) {
				auto targets = (xcb_atom_t *)_xcb->xcb_get_property_value(reply);
				auto len = _xcb->xcb_get_property_value_length(reply) / sizeof(xcb_atom_t);

				// resolve required types
				_connection->getAtomNames(SpanView(targets, len), [&](SpanView<StringView> types) {
					// Hide system types from user
					Vector<StringView> safeTypes;
					for (auto &it : types) {
						if (it == "UTF8_STRING" || it == "STRING") {
							safeTypes.emplace_back("text/plain");
						} else if (it != "TARGETS" && it != "MULTIPLE" && it != "SAVE_TARGETS"
								&& it != "TIMESTAMP" && it != "COMPOUND_TEXT") {
							safeTypes.emplace_back(it);
						}
					}

					for (auto &it : _probes) { it->typeCallback(Status::Ok, safeTypes); }
					_probes.clear();

					for (auto &it : _requests) {
						auto type = it->typeCallback(safeTypes);
						// check if type is in list of available types
						if (!type.empty()
								&& std::find(types.begin(), types.end(), type) != types.end()) {
							if (type == "text/plain") {
								auto tIt = std::find(types.begin(), types.end(), "UTF8_STRING");
								if (tIt != types.end()) {
									_waiters.emplace(
											_connection->getAtom(XcbAtomIndex::UTF8_STRING),
											sp::move(it));
								} else {
									tIt = std::find(types.begin(), types.end(), "STRING");
									if (tIt != types.end()) {
										_waiters.emplace(XCB_ATOM_STRING, sp::move(it));
									} else {
										_waiters.emplace(_connection->getAtom(type), sp::move(it));
									}
								}
							} else {
								_waiters.emplace(_connection->getAtom(type), sp::move(it));
							}
						} else {
							// notify that we have no matched type
							it->dataCallback(Status::Declined, BytesView(), StringView());
						}
					}
				});

				_requests.clear();
			}
		} else {
			auto wIt = _waiters.equal_range(event->target);

			if (wIt.first != wIt.second) {
				auto cookie = _xcb->xcb_get_property_unchecked(_connection->getConnection(), 1,
						_window, _connection->getAtom(XcbAtomIndex::XENOLITH_CLIPBOARD),
						XCB_GET_PROPERTY_TYPE_ANY, 0, maxOf<uint32_t>() / 4);
				auto reply = _connection->perform(_xcb->xcb_get_property_reply, cookie);
				if (reply) {
					if (reply->type == _connection->getAtom(XcbAtomIndex::INCR)) {
						// wait for an incremental content
						_incr = true;
						_incrType = event->target;
						_incrBuffer.clear();
						return;
					} else {
						finalizeClipboardWaiters(
								BytesView((const uint8_t *)_xcb->xcb_get_property_value(reply),
										_xcb->xcb_get_property_value_length(reply)),
								_incrType);
					}
				} else {
					_waiters.erase(wIt.first, wIt.second);
				}
			} else {
				log::source().error("XcbConnection", "No requests waits for a ",
						_connection->getAtomName(event->target), " clipboard target");

				// remove property for a type
				auto cookie = _xcb->xcb_get_property_unchecked(_connection->getConnection(), 1,
						_window, _connection->getAtom(XcbAtomIndex::XENOLITH_CLIPBOARD),
						XCB_GET_PROPERTY_TYPE_ANY, 0, 0);
				auto reply = _connection->perform(_xcb->xcb_get_property_reply, cookie);
				if (reply) {
					reply.clear();
				}
			}
		}
	}
	continueClipboardProcessing();
}

void XcbSupportWindow::handleSelectionClear(xcb_selection_clear_event_t *ev) {
	if (ev->owner == _window && ev->selection == _connection->getAtom(XcbAtomIndex::CLIPBOARD)) {
		_data = nullptr;
		_typeAtoms.clear();
	}
}

void XcbSupportWindow::handlePropertyNotify(xcb_property_notify_event_t *ev) {
	if (ev->window == _window && ev->atom == _connection->getAtom(XcbAtomIndex::XENOLITH_CLIPBOARD)
			&& ev->state == XCB_PROPERTY_NEW_VALUE && _incr) {
		auto cookie = _xcb->xcb_get_property_unchecked(_connection->getConnection(), 1, _window,
				_connection->getAtom(XcbAtomIndex::XENOLITH_CLIPBOARD), XCB_GET_PROPERTY_TYPE_ANY,
				0, maxOf<uint32_t>());
		auto reply = _connection->perform(_xcb->xcb_get_property_reply, cookie);
		if (reply) {
			auto len = _xcb->xcb_get_property_value_length(reply);

			if (len > 0) {
				auto data = (const uint8_t *)_xcb->xcb_get_property_value(reply);
				_incrBuffer.emplace_back(BytesView(data, len).bytes<Interface>());
			} else {
				Bytes data;
				size_t size = 0;
				for (auto &it : _incrBuffer) { size += it.size(); }

				data.resize(size);

				size = 0;
				for (auto &it : _incrBuffer) {
					memcpy(data.data() + size, it.data(), it.size());
					size += it.size();
				}

				finalizeClipboardWaiters(data, _incrType);
				_incrBuffer.clear();
				_incr = false;
				// finalize transfer
			}
		} else {
			finalizeClipboardWaiters(BytesView(), _incrType);
			_incrBuffer.clear();
			_incr = false;
		}
	} else if (auto t = getTransfer(ev->window, ev->atom)) {
		if (ev->state == XCB_PROPERTY_DELETE) {
			if (t->chunks.empty()) {
				// write zero-length prop to end transfer
				_xcb->xcb_change_property(_connection->getConnection(), XCB_PROP_MODE_REPLACE,
						t->requestor, t->property, t->type, 8, 0, nullptr);

				const uint32_t values[] = {XCB_EVENT_MASK_NO_EVENT};
				_xcb->xcb_change_window_attributes(_connection->getConnection(), t->requestor,
						XCB_CW_EVENT_MASK, values);

				_xcb->xcb_flush(_connection->getConnection());
				cancelTransfer(ev->window, ev->atom);
			} else {
				auto &chunk = t->chunks.front();
				_xcb->xcb_change_property(_connection->getConnection(), XCB_PROP_MODE_APPEND,
						t->requestor, t->property, t->type, 8, chunk.size(), chunk.data());
				_xcb->xcb_flush(_connection->getConnection());
				t->chunks.pop_front();
				++t->current;
			}
		}
	} else if (ev->window == _xsettings.owner && ev->atom == _xsettings.property) {
		readXSettings();
	}
}

void XcbSupportWindow::handleMappingNotify(xcb_mapping_notify_event_t *ev) {
	if (_keys.keysyms) {
		_xcb->xcb_refresh_keyboard_mapping(_keys.keysyms, ev);
	}
}

void XcbSupportWindow::handleExtensionEvent(int et, xcb_generic_event_t *e) {
	if (et == _xkb.firstEvent) {
		switch (e->pad0) {
		case XCB_XKB_NEW_KEYBOARD_NOTIFY: _xkb.initXcb(_xcb, _connection->getConnection()); break;
		case XCB_XKB_MAP_NOTIFY: _xkb.updateXkbMapping(_connection->getConnection()); break;
		case XCB_XKB_STATE_NOTIFY: {
			xcb_xkb_state_notify_event_t *ev = (xcb_xkb_state_notify_event_t *)e;
			_xkb.lib->xkb_state_update_mask(_xkb.state, ev->baseMods, ev->latchedMods,
					ev->lockedMods, ev->baseGroup, ev->latchedGroup, ev->lockedGroup);
			break;
		}
		}
	} else if (et == _randr.firstEvent) {
		switch (e->pad0) {
		case XCB_RANDR_SCREEN_CHANGE_NOTIFY:
			log::source().debug("XcbConnection", "XCB_RANDR_SCREEN_CHANGE_NOTIFY");
			break;
		case XCB_RANDR_NOTIFY: _connection->handleScreenUpdate(); break;
		default: break;
		}
	} else if (et == _xfixes.firstEvent) {
		switch (e->pad0) {
		case XCB_XFIXES_SELECTION_NOTIFY:
			handleSelectionUpdateNotify(reinterpret_cast<xcb_xfixes_selection_notify_event_t *>(e));
			break;
		default: break;
		}
	} else {
		//XL_X11_LOG("Unknown event: %d", et);
	}
}

xcb_atom_t XcbSupportWindow::writeClipboardSelection(xcb_window_t requestor, xcb_atom_t target,
		xcb_atom_t targetProperty) {
	if (!_data) {
		return XCB_ATOM_NONE;
	}

	StringView type;
	if (target == XCB_ATOM_STRING || target == _connection->getAtom(XcbAtomIndex::UTF8_STRING)) {
		type = StringView("text/plain");
	} else {
		type = _connection->getAtomName(target);
	}

	auto it = std::find(_data->types.begin(), _data->types.end(), type);
	if (it == _data->types.end()) {
		return XCB_ATOM_NONE;
	}

	auto data = _data->encodeCallback(type);
	if (data.empty()) {
		return XCB_ATOM_NONE;
	}

	if (data.size() > _safeReqeustSize) {
		// start incr transfer

		auto t = addTransfer(requestor, targetProperty,
				ClipboardTransfer{
					requestor,
					targetProperty,
					target,
					_data,
					0,
				});

		if (!t) {
			return XCB_ATOM_NONE;
		}

		uint32_t dataSize = uint32_t(data.size());
		BytesView dataView(data);

		while (!dataView.empty()) {
			auto block = dataView.readBytes(_safeReqeustSize);
			t->chunks.emplace_back(block.bytes<Interface>());
		}

		// subscribe on target widnow's events
		const uint32_t values[] = {
			XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_PROPERTY_CHANGE};
		_xcb->xcb_change_window_attributes(_connection->getConnection(), requestor,
				XCB_CW_EVENT_MASK, values);

		auto incr = _connection->getAtom(XcbAtomIndex::INCR);
		_xcb->xcb_change_property(_connection->getConnection(), XCB_PROP_MODE_REPLACE, requestor,
				targetProperty, incr, 32, 1, &dataSize);
		return target;
	} else {
		_xcb->xcb_change_property(_connection->getConnection(), XCB_PROP_MODE_REPLACE, requestor,
				targetProperty, target, 8, data.size(), data.data());
		return target;
	}
}

void XcbSupportWindow::handleSelectionRequest(xcb_selection_request_event_t *event) {
	xcb_selection_notify_event_t notify;
	notify.response_type = XCB_SELECTION_NOTIFY;
	notify.pad0 = 0;
	notify.sequence = 0;
	notify.time = event->time;
	notify.requestor = event->requestor;
	notify.selection = event->selection;
	notify.target = event->target;
	notify.property = XCB_ATOM_NONE;

	if (event->target == _connection->getAtom(XcbAtomIndex::TARGETS)) {
		// The list of supported targets was requested
		_xcb->xcb_change_property(_connection->getConnection(), XCB_PROP_MODE_REPLACE,
				event->requestor, event->property, XCB_ATOM_ATOM, 32, _typeAtoms.size(),
				(unsigned char *)_typeAtoms.data());
		notify.property = event->property;
	} else if (event->target == _connection->getAtom(XcbAtomIndex::TIMESTAMP)) {
		// The list of supported targets was requested
		_xcb->xcb_change_property(_connection->getConnection(), XCB_PROP_MODE_REPLACE,
				event->requestor, event->property, XCB_ATOM_INTEGER, 32, 1,
				(unsigned char *)&_selectionTimestamp);
		notify.property = event->property;
	} else if (event->target == _connection->getAtom(XcbAtomIndex::MULTIPLE)) {
		auto cookie = _xcb->xcb_get_property(_connection->getConnection(), 0, event->requestor,
				event->property, _connection->getAtom(XcbAtomIndex::ATOM_PAIR), 0,
				maxOf<uint32_t>() / 4);
		auto reply = _connection->perform(_xcb->xcb_get_property_reply, cookie);
		if (reply) {
			xcb_atom_t *requests = (xcb_atom_t *)_xcb->xcb_get_property_value(reply);
			auto count = uint32_t(_xcb->xcb_get_property_value_length(reply)) / sizeof(xcb_atom_t);

			for (uint32_t i = 0; i < count; i += 2) {
				auto it = std::find(_typeAtoms.begin(), _typeAtoms.end(), requests[i]);
				if (it == _typeAtoms.end()) {
					requests[i + 1] = XCB_ATOM_NONE;
				} else {
					requests[i + 1] =
							writeClipboardSelection(event->requestor, requests[i], requests[i + 1]);
				}
			}

			_xcb->xcb_change_property(_connection->getConnection(), XCB_PROP_MODE_REPLACE,
					event->requestor, event->property,
					_connection->getAtom(XcbAtomIndex::ATOM_PAIR), 32, count, requests);

			notify.property = event->property;
		}
	} else if (event->target == _connection->getAtom(XcbAtomIndex::SAVE_TARGETS)) {
		_xcb->xcb_change_property(_connection->getConnection(), XCB_PROP_MODE_REPLACE,
				event->requestor, event->property, _connection->getAtom(XcbAtomIndex::XNULL), 32, 0,
				nullptr);
		notify.property = event->property;
	} else {
		auto it = std::find(_typeAtoms.begin(), _typeAtoms.end(), event->target);

		if (it != _typeAtoms.end()) {
			notify.target =
					writeClipboardSelection(event->requestor, event->target, event->property);
			if (notify.target != XCB_ATOM_NONE) {
				notify.property = event->property;
			}
		}
	}

	_xcb->xcb_send_event(_connection->getConnection(), false, event->requestor,
			XCB_EVENT_MASK_NO_EVENT, // SelectionNotify events go without mask
			(const char *)&notify);
	_xcb->xcb_flush(_connection->getConnection());
}

void XcbSupportWindow::handleSelectionUpdateNotify(xcb_xfixes_selection_notify_event_t *ev) {
	if (ev->selection != _connection->getAtom(XcbAtomIndex::CLIPBOARD)) {
		return;
	}

	_owner = ev->owner;

	if (ev->owner == _window) {
		_selectionTimestamp = ev->selection_timestamp;
	} else if (ev->owner == XCB_WINDOW_NONE) {
		_data = nullptr;
		_typeAtoms.clear();
	}

	if (_connection) {
		_connection->handleClipboardChanged();
	}
}

Value XcbSupportWindow::getSettingsValue(StringView key) const {
	auto it = _xsettings.settings.find(key);
	if (it != _xsettings.settings.end()) {
		return it->second.value;
	}
	return Value();
}

void XcbSupportWindow::updateKeysymMapping() {
	static auto look_for = [](uint16_t &mask, xcb_keycode_t *codes, xcb_keycode_t kc, int i) {
		if (mask == 0 && codes) {
			for (xcb_keycode_t *ktest = codes; *ktest; ktest++) {
				if (*ktest == kc) {
					mask = (uint16_t)(1 << i);
					break;
				}
			}
		}
	};

	if (_keys.keysyms) {
		_xcb->xcb_key_symbols_free(_keys.keysyms);
	}

	if (_xcb->hasKeysyms()) {
		_keys.keysyms = _xcb->xcb_key_symbols_alloc(_connection->getConnection());
	}

	if (!_keys.keysyms) {
		return;
	}

	auto modifierCookie = _xcb->xcb_get_modifier_mapping_unchecked(_connection->getConnection());

	xcb_get_keyboard_mapping_cookie_t mappingCookie;
	const xcb_setup_t *setup = _xcb->xcb_get_setup(_connection->getConnection());

	if (!_xkb.lib) {
		mappingCookie = _xcb->xcb_get_keyboard_mapping(_connection->getConnection(),
				setup->min_keycode, setup->max_keycode - setup->min_keycode + 1);
	}

	auto numlockcodes = Ptr(_xcb->xcb_key_symbols_get_keycode(_keys.keysyms, XK_Num_Lock));
	auto shiftlockcodes = Ptr(_xcb->xcb_key_symbols_get_keycode(_keys.keysyms, XK_Shift_Lock));
	auto capslockcodes = Ptr(_xcb->xcb_key_symbols_get_keycode(_keys.keysyms, XK_Caps_Lock));
	auto modeswitchcodes = Ptr(_xcb->xcb_key_symbols_get_keycode(_keys.keysyms, XK_Mode_switch));

	auto modmap_r = _connection->perform(_xcb->xcb_get_modifier_mapping_reply, modifierCookie);
	if (!modmap_r) {
		return;
	}

	xcb_keycode_t *modmap = _xcb->xcb_get_modifier_mapping_keycodes(modmap_r);

	_keys.numlock = 0;
	_keys.shiftlock = 0;
	_keys.capslock = 0;
	_keys.modeswitch = 0;

	for (int i = 0; i < 8; i++) {
		for (int j = 0; j < modmap_r->keycodes_per_modifier; j++) {
			xcb_keycode_t kc = modmap[i * modmap_r->keycodes_per_modifier + j];
			look_for(_keys.numlock, numlockcodes, kc, i);
			look_for(_keys.shiftlock, shiftlockcodes, kc, i);
			look_for(_keys.capslock, capslockcodes, kc, i);
			look_for(_keys.modeswitch, modeswitchcodes, kc, i);
		}
	}

	// only if no xkb available
	if (!_xkb.lib) {
		memset(_xkb.keycodes, 0, sizeof(core::InputKeyCode) * 256);
		// from https://stackoverflow.com/questions/18689863/obtain-keyboard-layout-and-keysyms-with-xcb
		auto keyboard_mapping =
				_connection->perform(_xcb->xcb_get_keyboard_mapping_reply, mappingCookie);

		int nkeycodes = keyboard_mapping->length / keyboard_mapping->keysyms_per_keycode;

		xcb_keysym_t *keysyms = (xcb_keysym_t *)(keyboard_mapping + 1);

		for (int keycode_idx = 0; keycode_idx < nkeycodes; ++keycode_idx) {
			_xkb.keycodes[setup->min_keycode + keycode_idx] =
					getKeysymCode(keysyms[keycode_idx * keyboard_mapping->keysyms_per_keycode]);
		}
	}
}

core::InputKeyCode XcbSupportWindow::getKeyCode(xcb_keycode_t code) const {
	return _xkb.keycodes[code];
}

xcb_keysym_t XcbSupportWindow::getKeysym(xcb_keycode_t code, uint16_t state,
		bool resolveMods) const {
	xcb_keysym_t k0, k1;

	if (!resolveMods) {
		k0 = _xcb->xcb_key_symbols_get_keysym(_keys.keysyms, code, 0);
		// resolve only numlock
		if ((state & _keys.numlock)) {
			k1 = _xcb->xcb_key_symbols_get_keysym(_keys.keysyms, code, 1);
			if (_xcb->xcb_is_keypad_key(k1)) {
				if ((state & XCB_MOD_MASK_SHIFT)
						|| ((state & XCB_MOD_MASK_LOCK) && (state & _keys.shiftlock))) {
					return k0;
				} else {
					return k1;
				}
			}
		}
		return k0;
	}

	if (state & _keys.modeswitch) {
		k0 = _xcb->xcb_key_symbols_get_keysym(_keys.keysyms, code, 2);
		k1 = _xcb->xcb_key_symbols_get_keysym(_keys.keysyms, code, 3);
	} else {
		k0 = _xcb->xcb_key_symbols_get_keysym(_keys.keysyms, code, 0);
		k1 = _xcb->xcb_key_symbols_get_keysym(_keys.keysyms, code, 1);
	}

	if (k1 == XCB_NO_SYMBOL) {
		k1 = k0;
	}

	if ((state & _keys.numlock) && _xcb->xcb_is_keypad_key(k1)) {
		if ((state & XCB_MOD_MASK_SHIFT)
				|| ((state & XCB_MOD_MASK_LOCK) && (state & _keys.shiftlock))) {
			return k0;
		} else {
			return k1;
		}
	} else if (!(state & XCB_MOD_MASK_SHIFT) && !(state & XCB_MOD_MASK_LOCK)) {
		return k0;
	} else if (!(state & XCB_MOD_MASK_SHIFT)
			&& ((state & XCB_MOD_MASK_LOCK) && (state & _keys.capslock))) {
		if (k0 >= XK_0 && k0 <= XK_9) {
			return k0;
		}
		return k1;
	} else if ((state & XCB_MOD_MASK_SHIFT)
			&& ((state & XCB_MOD_MASK_LOCK) && (state & _keys.capslock))) {
		return k1;
	} else if ((state & XCB_MOD_MASK_SHIFT)
			|| ((state & XCB_MOD_MASK_LOCK) && (state & _keys.shiftlock))) {
		return k1;
	}

	return XCB_NO_SYMBOL;
}

void XcbSupportWindow::fillTextInputData(core::InputEventData &event, xcb_keycode_t detail,
		uint16_t state, bool textInputEnabled, bool compose) {
	if (_xkb.initialized) {
		event.key.keycode = getKeyCode(detail);
		event.key.compose = core::InputKeyComposeState::Nothing;
		event.key.keysym = getKeysym(detail, state, false);
		if (textInputEnabled) {
			if (compose) {
				const auto keysym = _xkb.composeSymbol(
						_xkb.lib->xkb_state_key_get_one_sym(_xkb.state, detail), event.key.compose);
				const uint32_t cp = _xkb.lib->xkb_keysym_to_utf32(keysym);
				if (cp != 0 && keysym != XKB_KEY_NoSymbol) {
					event.key.keychar = cp;
				} else {
					event.key.keychar = 0;
				}
			} else {
				event.key.keychar = _xkb.lib->xkb_state_key_get_utf32(_xkb.state, detail);
			}
		} else {
			event.key.keychar = 0;
		}
	} else {
		auto sym = getKeysym(detail, state, false); // state-inpependent keysym
		event.key.keycode = getKeysymCode(sym);
		event.key.compose = core::InputKeyComposeState::Nothing;
		event.key.keysym = sym;
		if (textInputEnabled) {
			event.key.keychar = _glfwKeySym2Unicode(getKeysym(detail, state, true));
		} else {
			event.key.keychar = 0;
		}
	}
}

XcbSupportWindow::ClipboardTransfer *XcbSupportWindow::addTransfer(xcb_window_t w, xcb_atom_t p,
		ClipboardTransfer &&t) {
	auto id = uint64_t(w) << 32 | uint64_t(p);
	auto it = _transfers.find(id);
	if (it != _transfers.end()) {
		return nullptr;
	}
	return &_transfers.emplace(id, move(t)).first->second;
}

XcbSupportWindow::ClipboardTransfer *XcbSupportWindow::getTransfer(xcb_window_t w, xcb_atom_t p) {
	auto it = _transfers.find(uint64_t(w) << 32 | uint64_t(p));
	if (it == _transfers.end()) {
		return nullptr;
	}
	return &it->second;
}

void XcbSupportWindow::cancelTransfer(xcb_window_t w, xcb_atom_t p) {
	_transfers.erase(uint64_t(w) << 32 | uint64_t(p));
}

void XcbSupportWindow::readXSettings() {
	static auto _getPadding = [](int length, int increment) {
		return (increment - (length % increment)) % increment;
	};

	auto cookie = _xcb->xcb_get_property(_connection->getConnection(), 0, _xsettings.owner,
			_xsettings.property, 0, 0, maxOf<uint32_t>() / 4);
	auto reply = _connection->perform(_xcb->xcb_get_property_reply, cookie);
	if (reply) {
		auto data = _xcb->xcb_get_property_value(reply);
		auto len = _xcb->xcb_get_property_value_length(reply);

		Map<String, SettingsValue> settings;

		uint32_t udpi = 0;
		uint32_t dpi = 0;

		auto d = BytesView(reinterpret_cast<uint8_t *>(data), len);
		/*auto byteOrder =*/d.readUnsigned32();
		auto serial = d.readUnsigned32();
		auto nsettings = d.readUnsigned32();

		while (nsettings > 0 && !d.empty()) {
			auto type = d.readUnsigned(); //                  1  SETTING_TYPE  type
			d.readUnsigned(); //                                       1                unused
			auto len = d.readUnsigned16(); //                2  n             name-len
			auto name = d.readString(len); // n  STRING8       name
			d.readBytes(_getPadding(len, 4)); //   P               unused, p=pad(n)
			auto serial = d.readUnsigned32(); //             4   CARD32      last-change-serial

			if (type == 0) {
				auto value = d.readUnsigned32();
				settings.emplace(name.str<Interface>(),
						SettingsValue{Value(bit_cast<int32_t>(value)), serial});
				if (name == "Gdk/UnscaledDPI") {
					udpi = value;
				} else if (name == "Xft/DPI") {
					dpi = value;
				}
			} else if (type == 1) {
				auto len = d.readUnsigned32();
				auto value = d.readString(len);
				d.readBytes(_getPadding(len, 4));
				settings.emplace(name.str<Interface>(), SettingsValue{Value(value), serial});
			} else if (type == 2) {
				auto r = d.readUnsigned16();
				auto g = d.readUnsigned16();
				auto b = d.readUnsigned16();
				auto a = d.readUnsigned16();

				settings.emplace(name.str<Interface>(),
						SettingsValue{Value{Value(r), Value(g), Value(b), Value(a)}, serial});
			} else {
				break;
			}
			--nsettings;
		}

		_xsettings.serial = serial;
		_xsettings.settings = sp::move(settings);
		_xsettings.udpi = udpi;
		_xsettings.dpi = dpi;

		_connection->handleSettingsUpdate();
	}
}

} // namespace stappler::xenolith::platform
