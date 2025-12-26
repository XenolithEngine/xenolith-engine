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

#ifndef XENOLITH_APPLICATION_LINUX_XCB_XLLINUXXCBCLIPBOARD_H_
#define XENOLITH_APPLICATION_LINUX_XCB_XLLINUXXCBCLIPBOARD_H_

#include "XLLinuxXcbConnection.h"

#if LINUX

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

class XcbSupportWindow : public Ref {
public:
	virtual ~XcbSupportWindow();

	XcbSupportWindow(NotNull<XcbConnection>, NotNull<XkbLibrary> xkb, int screenNbr);

	void invalidate();

	Rc<XcbDisplayConfigManager> makeDisplayConfigManager() const;

	xcb_window_t getWindow() const { return _window; }

	Status readFromClipboard(Rc<ClipboardRequest> &&req);
	Status probeClipboard(Rc<ClipboardProbe> &&probe);
	Status writeToClipboard(Rc<ClipboardData> &&data);

	void cancelTransfer(xcb_window_t w, xcb_atom_t p);

	void continueClipboardProcessing();
	void finalizeClipboardWaiters(BytesView, xcb_atom_t);
	void handleSelectionNotify(xcb_selection_notify_event_t *);
	void handleSelectionClear(xcb_selection_clear_event_t *);
	void handlePropertyNotify(xcb_property_notify_event_t *);
	void handleMappingNotify(xcb_mapping_notify_event_t *);
	void handleExtensionEvent(int, xcb_generic_event_t *);

	xcb_atom_t writeClipboardSelection(xcb_window_t requestor, xcb_atom_t target,
			xcb_atom_t targetProperty);
	void handleSelectionRequest(xcb_selection_request_event_t *);

	void handleSelectionUpdateNotify(xcb_xfixes_selection_notify_event_t *);

	Value getSettingsValue(StringView) const;
	uint32_t getUnscaledDpi() const { return (_xsettings.udpi == 0) ? 122'880 : _xsettings.udpi; }
	uint32_t getDpi() const { return (_xsettings.dpi == 0) ? 122'880 : _xsettings.dpi; }

	void updateKeysymMapping();

	core::InputKeyCode getKeyCode(xcb_keycode_t code) const;
	xcb_keysym_t getKeysym(xcb_keycode_t code, uint16_t state, bool resolveMods) const;

	void fillTextInputData(core::InputEventData &, xcb_keycode_t, uint16_t state,
			bool textInputEnabled, bool compose);

protected:
	struct RandrInfo {
		bool enabled = true;
		bool initialized = false;
		uint8_t firstEvent = 0;

		uint32_t majorVersion = 0;
		uint32_t minorVersion = 0;
	};

	struct XfixesInfo {
		bool enabled = true;
		bool initialized = false;
		uint8_t firstEvent = 0;
		uint8_t firstError = 0;

		uint32_t majorVersion = 0;
		uint32_t minorVersion = 0;
	};

	struct ShapeInfo {
		bool enabled = true;
		bool initialized = false;
		uint8_t firstEvent = 0;
		uint8_t firstError = 0;

		uint32_t majorVersion = 0;
		uint32_t minorVersion = 0;
	};

	struct ClipboardTransfer {
		xcb_window_t requestor;
		xcb_atom_t property;
		xcb_atom_t type;

		Rc<ClipboardData> data;
		uint32_t current = 0;
		std::deque<Bytes> chunks;
	};

	struct SettingsValue {
		Value value;
		uint32_t serial = 0;
	};

	struct XSettingsInfo {
		xcb_window_t owner = 0;
		xcb_atom_t selection = 0;
		xcb_atom_t property = 0;
		uint32_t serial = 0;

		uint32_t udpi = 0;
		uint32_t dpi = 0;

		Map<String, SettingsValue> settings;
	};

	struct KeyInfo {
		uint16_t numlock = 0;
		uint16_t shiftlock = 0;
		uint16_t capslock = 0;
		uint16_t modeswitch = 0;

		xcb_key_symbols_t *keysyms = nullptr;
	};

	ClipboardTransfer *addTransfer(xcb_window_t w, xcb_atom_t p, ClipboardTransfer &&t);

	ClipboardTransfer *getTransfer(xcb_window_t w, xcb_atom_t p);

	void readXSettings();

	XcbConnection *_connection = nullptr;
	XcbLibrary *_xcb = nullptr;
	xcb_window_t _window = 0;
	xcb_window_t _owner = 0;

	uint32_t _safeReqeustSize = 128_KiB;
	uint32_t _maxRequestSize = 0;

	RandrInfo _randr;
	XfixesInfo _xfixes;
	ShapeInfo _shape;
	XSettingsInfo _xsettings;
	KeyInfo _keys;
	XkbInfo _xkb;

	Vector<Rc<ClipboardRequest>> _requests;
	Vector<Rc<ClipboardProbe>> _probes;
	std::multimap<xcb_atom_t, Rc<ClipboardRequest>> _waiters;
	Vector<Bytes> _incrBuffer;
	xcb_atom_t _incrType = 0;
	bool _incr = false;

	Rc<ClipboardData> _data;
	Vector<xcb_atom_t> _typeAtoms;
	xcb_timestamp_t _selectionTimestamp = XCB_CURRENT_TIME;

	Map<uint64_t, ClipboardTransfer> _transfers;
};

} // namespace stappler::xenolith::platform

#endif

#endif // XENOLITH_APPLICATION_LINUX_XCB_XLLINUXXCBCLIPBOARD_H_
