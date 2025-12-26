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

#ifndef XENOLITH_APPLICATION_LINUX_XCB_XLLINUXXCBWINDOW_H_
#define XENOLITH_APPLICATION_LINUX_XCB_XLLINUXXCBWINDOW_H_

#include "XLContextInfo.h" // IWYU pragma: keep
#include "platform/XLContextNativeWindow.h"
#include "XLLinuxXcbConnection.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

class LinuxContextController;

enum class XcbMoveResize {
	SizeTopLeft = 0,
	SizeTop = 1,
	SizeTopRight = 2,
	SizeRight = 3,
	SizeBottomRight = 4,
	SizeBottom = 5,
	SizeBottomLeft = 6,
	SizeLeft = 7,
	Move = 8,
	SizeKeyboard = 9,
	MoveKeyboard = 10,
	Cancel = 11,
	Menu = 256,
};

enum class XcbConstraints : uint32_t {
	TopTiled = 1 << 0,
	TopResizable = 1 << 1,
	RightTiled = 1 << 2,
	RightResizable = 1 << 3,
	BottomTiled = 1 << 4,
	BottomResizable = 1 << 5,
	LeftTiled = 1 << 6,
	LeftResizable = 1 << 7
};

SP_DEFINE_ENUM_AS_MASK(XcbConstraints)

class XcbWindow final : public NativeWindow {
public:
	virtual ~XcbWindow();

	XcbWindow();

	bool init(NotNull<XcbConnection>, Rc<WindowInfo> &&, NotNull<LinuxContextController>);

	void handleExpose(xcb_expose_event_t *);
	void handleConfigureNotify(xcb_configure_notify_event_t *);
	void handlePropertyNotify(xcb_property_notify_event_t *);
	void handleButtonPress(xcb_button_press_event_t *);
	void handleButtonRelease(xcb_button_release_event_t *);
	void handleMotionNotify(xcb_motion_notify_event_t *);
	void handleEnterNotify(xcb_enter_notify_event_t *);
	void handleLeaveNotify(xcb_leave_notify_event_t *);
	void handleFocusIn(xcb_focus_in_event_t *);
	void handleFocusOut(xcb_focus_out_event_t *);
	void handleKeyPress(xcb_key_press_event_t *);
	void handleKeyRelease(xcb_key_release_event_t *);

	void handleSyncRequest(xcb_timestamp_t, xcb_sync_int64_t);
	void handleCloseRequest();

	void notifyScreenChange();

	void handleSettingsUpdated();

	xcb_window_t getWindow() const { return _xinfo.window; }
	xcb_connection_t *getConnection() const;

	virtual void mapWindow() override;
	virtual void unmapWindow() override;
	virtual bool close() override;

	virtual void handleFramePresented(NotNull<core::PresentationFrame>) override;

	virtual core::FrameConstraints exportConstraints() const override;

	virtual Extent2 getExtent() const override;

	virtual Rc<core::Surface> makeSurface(NotNull<core::Instance>) override;

	virtual bool enableState(WindowState) override;
	virtual bool disableState(WindowState) override;

	void startGrip(XcbMoveResize, int32_t x, int32_t y, int32_t button);

protected:
	virtual bool updateTextInput(const TextInputRequest &,
			TextInputFlags flags = TextInputFlags::RunIfDisabled) override;

	virtual void cancelTextInput() override;

	void updateWindowAttributes();
	void configureWindow(xcb_rectangle_t r, uint16_t border_width);

	uint32_t getCurrentFrameRate() const;

	virtual Status setFullscreenState(FullscreenInfo &&) override;

	xcb_rectangle_t getContentRect(xcb_rectangle_t boundingRect) const;
	void updateContentRect(xcb_rectangle_t);
	void configureOutputWindow();

	void updateShadows();
	void generateShadowPixmaps(uint32_t width, uint32_t inset);

	virtual void setCursor(WindowCursor) override;

	void updateUserTime(uint32_t);
	void cancelPointerEvents();

	Rc<XcbConnection> _connection;

	XcbLibrary *_xcb = nullptr;

	xcb_screen_t *_defaultScreen = nullptr;

	XcbWindowInfo _xinfo;

	WindowLayerFlags _buttonGripFlags = WindowLayerFlags::None;
	bool _pendingExpose = false;
	xcb_timestamp_t _lastInputTime = 0;
	xcb_timestamp_t _lastSyncTime = 0;

	uint32_t _forcedFrames = 0;
	uint16_t _borderWidth = 0;
	uint16_t _frameRate = 60'000;
	float _density = 0.0f;

	String _wmClass;

	Map<MonitorId, xenolith::ModeInfo> _capturedModes;
	std::bitset<64> _buttons;

	float _shadowCurrentValue = 0.0f;
};

} // namespace stappler::xenolith::platform

#endif /* XENOLITH_APPLICATION_LINUX_XCB_XLLINUXXCBWINDOW_H_ */
