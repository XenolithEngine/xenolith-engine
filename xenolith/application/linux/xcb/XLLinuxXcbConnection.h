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

#ifndef XENOLITH_APPLICATION_LINUX_XCB_XLLINUXXCBCONNECTION_H_
#define XENOLITH_APPLICATION_LINUX_XCB_XLLINUXXCBCONNECTION_H_

#include "SPBytesReader.h"
#include "XlCoreMonitorInfo.h" // IWYU pragma: keep

#if LINUX

#include "linux/XLLinuxXkbLibrary.h"
#include "XLLinuxXcbLibrary.h"
#include "XLContextInfo.h" // IWYU pragma: keep
#include "XLCoreInput.h"
#include "platform/XLContextController.h"

#include <xcb/xcb.h>
#include <xcb/xcb_errors.h>
#include <xcb/xproto.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

class XcbWindow;
class XcbDisplayConfigManager;
class XcbSupportWindow;

struct XcbShadowCornerContext {
	uint32_t width = 0;
	uint32_t inset = 0;
	xcb_gcontext_t gc = 0;
	xcb_pixmap_t pixmap = 0;
};

struct XcbWindowInfo {
	uint8_t depth = XCB_COPY_FROM_PARENT;
	xcb_window_t parent = 0;
	xcb_visualid_t visual = 0;

	xcb_rectangle_t boundingRect;
	xcb_rectangle_t contentRect;
	uint16_t borderWidth = 0;
	uint16_t cornerRadius = 32;

	uint32_t overrideRedirect = 0;
	uint32_t eventMask = 0;

	StringView title;
	StringView icon;
	StringView wmClass;

	bool overrideClose = true;
	bool enableSync = false;
	bool closed = false;

	xcb_sync_int64_t syncValue = {0, 0};
	uint64_t syncFrameOrder = 0;

	// output
	xcb_colormap_t colormap = 0;
	xcb_window_t window = 0;
	xcb_gcontext_t decorationGc = 0;
	xcb_window_t outputWindow = 0;
	xcb_sync_counter_t syncCounter = 0;

	xcb_cursor_t cursorId = 0;

	String outputName;

	XcbShadowCornerContext shadowTopLeft;
	XcbShadowCornerContext shadowTopRight;
	XcbShadowCornerContext shadowBottomLeft;
	XcbShadowCornerContext shadowBottomRight;
};

class XcbConnection final : public Ref {
public:
	static void ReportError(int error);

	virtual ~XcbConnection();

	XcbConnection(NotNull<XcbLibrary> xcb, NotNull<XkbLibrary> xkb,
			StringView display = StringView());

	Rc<DisplayConfigManager> makeDisplayConfigManager(
			Function<void(NotNull<DisplayConfigManager>)> &&);

	void setSystemNotificationHandler(Function<void(SystemNotification)> &&);

	uint32_t poll();

	XcbLibrary *getXcb() const { return _xcb; }

	XcbDisplayConfigManager *getDisplayConfigManager() const { return _displayConfig; }

	int getSocket() const { return _socket; }

	xcb_connection_t *getConnection() const { return _connection; }
	xcb_screen_t *getDefaultScreen() const { return _screen; }

	bool hasErrors() const;

	// get code from keysym mapping table
	core::InputKeyCode getKeyCode(xcb_keycode_t code) const;
	xcb_keysym_t getKeysym(xcb_keycode_t code, uint16_t state, bool resolveMods) const;

	void fillTextInputData(core::InputEventData &, xcb_keycode_t, uint16_t state,
			bool textInputEnabled, bool compose);

	xcb_atom_t getAtom(XcbAtomIndex) const;
	xcb_atom_t getAtom(StringView, bool onlyIfExists = false) const;
	StringView getAtomName(xcb_atom_t) const;

	bool hasCapability(XcbAtomIndex) const;
	bool hasCapability(xcb_atom_t) const;
	bool hasCapability(StringView) const;

	const char *getErrorMajorName(uint8_t) const;
	const char *getErrorMinorName(uint8_t, uint16_t) const;
	const char *getErrorName(uint8_t errorCode) const;

	void getAtomNames(SpanView<xcb_atom_t>, const Callback<void(SpanView<StringView>)> &);
	void getAtoms(SpanView<String>, const Callback<void(SpanView<xcb_atom_t>)> &);
	void getAtoms(SpanView<StringView>, const Callback<void(SpanView<xcb_atom_t>)> &);

	xcb_visualid_t getVisualByDepth(uint16_t depth) const;
	bool createWindow(const WindowInfo *, XcbWindowInfo &info) const;

	void attachWindow(xcb_window_t, XcbWindow *);
	void detachWindow(xcb_window_t);

	void notifyScreenChange();

	xcb_cursor_t loadCursor(WindowCursor);

	bool isCursorSupported(WindowCursor);
	WindowCapabilities getCapabilities() const;

	bool setCursorId(xcb_window_t window, xcb_cursor_t);

	Status readFromClipboard(Rc<ClipboardRequest> &&req);
	Status probeClipboard(Rc<ClipboardProbe> &&);
	Status writeToClipboard(Rc<ClipboardData> &&);

	Value getSettingsValue(StringView) const;
	uint32_t getUnscaledDpi() const;
	uint32_t getDpi() const;

	template <typename T>
	auto wrapXcbReply(T *t) const {
		return Ptr(t, [](T *ptr) { ::free(ptr); });
	}

	template <typename Fn, typename Cookie>
	auto perform(Fn *fn, Cookie cookie) const {
		xcb_generic_error_t *error = nullptr;
		auto reply = fn(_connection, cookie, &error);
		if (error) {
			printError("Fail to perform XCB function", error);
			::free(error);
			if (reply) {
				::free(reply);
			}
			return decltype(wrapXcbReply(reply))(nullptr);
		}
		if (reply) {
			return wrapXcbReply(reply);
		} else {
			printError("Fail to perform XCB function", error);
			return decltype(wrapXcbReply(reply))(nullptr);
		}
	}

	void printError(StringView, xcb_generic_error_t *) const;

	void handleSettingsUpdate();
	void handleScreenUpdate();
	void handleClipboardChanged();

protected:
	bool checkCookie(xcb_void_cookie_t cookie, StringView errMessage);

	XcbLibrary *_xcb = nullptr;
	xcb_connection_t *_connection = nullptr;
	xcb_errors_context_t *_errors = nullptr;
	int _screenNbr = -1;
	const xcb_setup_t *_setup = nullptr;
	xcb_screen_t *_screen = nullptr;
	xcb_cursor_context_t *_cursorContext = nullptr;
	int _socket = -1;

	XcbAtomInfo _atoms[sizeof(s_atomRequests) / sizeof(XcbAtomInfo)];
	mutable Map<String, xcb_atom_t> _namedAtoms;
	mutable Map<xcb_atom_t, String> _atomNames;

	bool _syncEnabled = true;

	Map<xcb_window_t, XcbWindow *> _windows;

	Rc<XcbSupportWindow> _support;
	Rc<XcbDisplayConfigManager> _displayConfig;
	Vector<StringView> _capabilitiesByNames;
	Vector<xcb_atom_t> _capabilitiesByAtoms;
	Map<WindowCursor, xcb_cursor_t> _cursors;

	Function<void(SystemNotification)> _onSystemNotification;
};

} // namespace stappler::xenolith::platform

#endif

#endif /* XENOLITH_APPLICATION_LINUX_XCB_XLLINUXXCBCONNECTION_H_ */
