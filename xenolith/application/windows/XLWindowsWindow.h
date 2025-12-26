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

#ifndef XENOLITH_APPLICATION_WINDOWS_XLWINDOWSWINDOW_H_
#define XENOLITH_APPLICATION_WINDOWS_XLWINDOWSWINDOW_H_

#include "XLWindows.h" // IWYU pragma: keep
#include "platform/XLContextNativeWindow.h"
#include "platform/XLDisplayConfigManager.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

class WindowsContextController;
class WindowClass;

class WindowsWindow : public NativeWindow {
public:
	virtual ~WindowsWindow();

	virtual bool init(NotNull<WindowsContextController>, Rc<WindowInfo> &&);

	virtual void mapWindow() override;
	virtual void unmapWindow() override;
	virtual bool close() override;

	virtual void handleFramePresented(NotNull<core::PresentationFrame>) override;

	virtual core::SurfaceInfo getSurfaceOptions(const core::Device &,
			NotNull<core::Surface>) const override;

	virtual core::FrameConstraints exportConstraints(core::FrameConstraints &&) const override;

	virtual Extent2 getExtent() const override;

	virtual Rc<core::Surface> makeSurface(NotNull<core::Instance>) override;

	virtual core::PresentationOptions getPreferredOptions() const override;

	virtual bool enableState(WindowState) override;
	virtual bool disableState(WindowState) override;

	virtual void openWindowMenu(Vec2 pos) override;

	HWND getWindow() const { return _window; }

	void handleDisplayChanged(const DisplayConfig *);

	Status handleDestroy();
	Status handleMove(IVec2);
	Status handleResize(Extent2, WindowState state, WindowState mask);

	enum ActivateStatus {
		Activate = WA_ACTIVE,
		ClickActivate = WA_CLICKACTIVE,
		Deacivate = WA_INACTIVE
	};

	Status handleActivate(ActivateStatus);
	Status handleFocus(bool focusGain);
	Status handleEnabled(bool enabled);

	Status handlePaint();
	Status handleClose();

	Status handleEraseBackground();
	Status handleWindowVisible(bool visible);
	Status handleSetCursor();

	enum StyleType {
		Style = GWL_STYLE,
		ExtendedStyle = GWL_EXSTYLE,
	};

	Status handleStyleChanging(StyleType, STYLESTRUCT *);
	Status handleStyleChanged(StyleType, const STYLESTRUCT *);

	Status handleWindowDecorations(bool enabled, NCCALCSIZE_PARAMS *, RECT *);
	LRESULT handleWindowDecorationsActivate(WPARAM, LPARAM);
	Status handleWindowDecorationsPaint(WPARAM, LPARAM);
	Status handleDecorationsMouseMove(IVec2);
	Status handleDecorationsMouseLeave();

	Status handleKeyPress(core::InputKeyCode, int scancode, char32_t c);
	Status handleKeyRepeat(core::InputKeyCode, int scancode, char32_t c, int count);
	Status handleKeyRelease(core::InputKeyCode, int scancode, char32_t c);
	Status handleChar(char32_t);

	Status handleMouseMove(IVec2, bool nonclient);
	Status handleMouseLeave();
	Status handleMouseEvent(IVec2, core::InputMouseButton, core::InputEventName);
	Status handleMouseWheel(Vec2);
	Status handleMouseCaptureChanged();

	Status handlePositionChanging(WINDOWPOS *);
	Status handlePositionChanged(const WINDOWPOS *);
	Status handleSizing(core::ViewConstraints, RECT *);
	Status handleMoving(RECT *);
	Status handleMoveResize(bool enter);

	Status handleDpiChanged(Vec2, const RECT *);

	Status handleMinMaxInfo(MINMAXINFO *);

	LRESULT handleHitTest(WPARAM, LPARAM);

	void pushCommand(WPARAM);
	void popCommand(WPARAM);

protected:
	virtual bool updateTextInput(const TextInputRequest &,
			TextInputFlags flags = TextInputFlags::RunIfDisabled) override;

	virtual void cancelTextInput() override;

	virtual void setCursor(WindowCursor) override;

	virtual Status setFullscreenState(FullscreenInfo &&) override;

	char32_t makeKeyChar(char32_t);

	void enableMouseTracked(bool nonclient);

	struct State {
		DWORD style = 0;
		DWORD exstyle = 0;
		IVec2 position;
		Extent2 extent;
		IRect frame;
		bool isFullscreen = false;
	};

	void updateWindowState(const State &);

	Rc<WindowClass> _class;
	HWND _window = nullptr;
	WideString _wTitle;
	State _currentState;
	State _savedState;
	Extent2 _commitedExtent;
	uint32_t _frameRate = 60'000;
	float _density = 1.0f;

	bool _mouseTrackedClient = false;
	bool _mouseTrackedNonClient = false;
	Vec2 _pointerLocation;
	core::InputModifier _enabledModifiers = core::InputModifier::None;
	char32_t _highSurrogate = 0;
	uint32_t _pointerButtonCapture = 0;
	WindowCursor _currentCursor = WindowCursor::Default;

	Vector<WPARAM> _activeCommands;
};

} // namespace stappler::xenolith::platform

#endif // XENOLITH_APPLICATION_WINDOWS_XLWINDOWSWINDOW_H_
