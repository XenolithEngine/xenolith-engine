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

#include "XLWindowsWindowClass.h"
#include "XLWindowsWindow.h"
#include "XLWindowsContextController.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

WindowClass::~WindowClass() { ::UnregisterClassW(_windowClass.lpszClassName, _module); }

bool WindowClass::init(WideStringView name) {
	_name = name.str<Interface>();

	_module = GetModuleHandleW(nullptr);

	_windowClass.style = CS_HREDRAW | CS_VREDRAW;
	_windowClass.lpfnWndProc = &_wndProc;
	_windowClass.cbClsExtra = 0;
	_windowClass.cbWndExtra = 0;
	_windowClass.hInstance = _module;
	_windowClass.hIcon = (HICON)LoadImage(nullptr, IDI_APPLICATION, IMAGE_ICON, 0, 0,
			LR_DEFAULTSIZE | LR_SHARED);
	_windowClass.hCursor = NULL;
	_windowClass.hbrBackground = nullptr;
	_windowClass.lpszMenuName = nullptr;
	_windowClass.lpszClassName = (wchar_t *)_name.data();

	RegisterClassW(&_windowClass);

	return true;
}

static core::ViewConstraints getViewConstraintsForSizing(WPARAM value) {
	switch (value) {
	case WMSZ_BOTTOM: return core::ViewConstraints::Bottom; break;
	case WMSZ_BOTTOMLEFT: return core::ViewConstraints::Bottom | core::ViewConstraints::Left; break;
	case WMSZ_BOTTOMRIGHT:
		return core::ViewConstraints::Bottom | core::ViewConstraints::Right;
		break;
	case WMSZ_LEFT: return core::ViewConstraints::Left; break;
	case WMSZ_RIGHT: return core::ViewConstraints::Right; break;
	case WMSZ_TOP: return core::ViewConstraints::Top; break;
	case WMSZ_TOPLEFT: return core::ViewConstraints::Top | core::ViewConstraints::Left; break;
	case WMSZ_TOPRIGHT: return core::ViewConstraints::Top | core::ViewConstraints::Right; break;
	}
	return core::ViewConstraints::None;
}

void WindowClass::attachWindow(WindowsWindow *window) {
	auto it = _windows.find(window->getWindow());
	if (it == _windows.end()) {
		_windows.emplace(window->getWindow(), window);
	} else {
		log::source().error("WindowClass", "Window already attached");
	}
}

void WindowClass::detachWindow(WindowsWindow *window) {
	auto it = _windows.find(window->getWindow());
	if (it != _windows.end()) {
		_windows.erase(it);
	} else {
		log::source().error("WindowClass", "Window is not attached");
	}
}

LRESULT WindowClass::_wndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	auto win = reinterpret_cast<WindowsWindow *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
	if (!win) {
		return ::DefWindowProcW(hwnd, uMsg, wParam, lParam);
	}

	// we can collect all events end changes in internal buffers to reduce context switched and
	// make event processing more stable
	bool shouldRetainPoll = true;

	// Ignore live-resize events to do immediate processing for them
	switch (uMsg) {
	case WM_NCHITTEST:
	case WM_NCPAINT:
	case WM_NCACTIVATE:
	case WM_NCLBUTTONDOWN:
	case WM_NCLBUTTONUP:
	case WM_NCLBUTTONDBLCLK:
	case WM_NCRBUTTONDOWN:
	case WM_NCRBUTTONUP:
	case WM_NCRBUTTONDBLCLK:
	case WM_NCMBUTTONDOWN:
	case WM_NCMBUTTONUP:
	case WM_NCMBUTTONDBLCLK:
	case WM_SYSCOMMAND:
	case WM_SYNCPAINT: shouldRetainPoll = false; break;
	}

	if (uMsg == WM_SYSCOMMAND) {
		win->pushCommand(wParam & 0xFFF0);
	}

	if (shouldRetainPoll) {
		if (win->getController()->isWithinPoll()) {
			log::source().debug("WindowClass", "Recursive processing");
		}

		win->getController()->retainPollDepth();
	}

	auto result = wndProc(win, hwnd, uMsg, wParam, lParam);

	if (shouldRetainPoll) {
		win->getController()->releasePollDepth();
	}

	if (uMsg == WM_SYSCOMMAND) {
		win->popCommand(wParam & 0xFFF0);
	}

	return result;
}

LRESULT WindowClass::wndProc(WindowsWindow *win, HWND hwnd, UINT uMsg, WPARAM wParam,
		LPARAM lParam) {
	auto getResultForStatus = [&](StringView event, Status st, LRESULT okStatus = 0) -> LRESULT {
		switch (st) {
		case Status::Ok: return okStatus; break;
		case Status::Declined: return -1; break;
		case Status::Propagate: return ::DefWindowProcW(hwnd, uMsg, wParam, lParam); break;
		default:
			if (toInt(st) > 0) {
				return toInt(st);
			}
			break;
		}
		log::error("WindowClass", "Fail to process event ", event, " with Status: ", st);
		return -1;
	};

	auto handleDefault = [&]() { return ::DefWindowProcW(hwnd, uMsg, wParam, lParam); };

	switch (uMsg) {
	case WM_CREATE: return handleDefault(); break;
	case WM_DESTROY: return getResultForStatus("WM_DESTROY", win->handleDestroy()); break;

	case WM_MOVE:
		return getResultForStatus("WM_MOVE",
				win->handleMove(IVec2(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam))));
		break;
	case WM_SIZE: {
		WindowState state = WindowState::None;
		if (wParam == SIZE_MAXIMIZED) {
			state |= WindowState::Maximized;
		}
		if (wParam == SIZE_MINIMIZED) {
			state |= WindowState::Minimized;
		}

		return getResultForStatus("WM_SIZE",
				win->handleResize(Extent2(LOWORD(lParam), HIWORD(lParam)), state,
						WindowState::Minimized | WindowState::Maximized));
		break;
	}
	case WM_ACTIVATE:
		return getResultForStatus("WM_ACTIVATE",
				win->handleActivate(WindowsWindow::ActivateStatus(wParam)));
		break;

	case WM_SETFOCUS: return getResultForStatus("WM_SETFOCUS", win->handleFocus(true)); break;
	case WM_KILLFOCUS: return getResultForStatus("WM_KILLFOCUS", win->handleFocus(false)); break;
	case WM_ENABLE:
		return getResultForStatus("WM_ENABLE", win->handleEnabled(wParam == TRUE));
		break;

	// TODO: integrate with text input
	case WM_SETTEXT: return handleDefault(); break;
	case WM_GETTEXT: return handleDefault(); break;
	case WM_GETTEXTLENGTH: return handleDefault(); break;

	case WM_PAINT: return getResultForStatus("WM_PAINT", win->handlePaint()); break;
	case WM_CLOSE: return getResultForStatus("WM_CLOSE", win->handleClose()); break;

	case WM_QUERYENDSESSION: return handleDefault(); break;
	case WM_QUERYOPEN: return handleDefault(); break;
	case WM_ENDSESSION: return handleDefault(); break;

	case WM_ERASEBKGND:
		return getResultForStatus("WM_ERASEBKGND ", win->handleEraseBackground());
		break;

	case WM_SHOWWINDOW:
		return getResultForStatus("WM_SHOWWINDOW ", win->handleWindowVisible(wParam == TRUE));
		break;

	case WM_SETCURSOR: return getResultForStatus("WM_SETCURSOR ", win->handleSetCursor()); break;
	case WM_COMPACTING: return handleDefault(); break;

	// TODO: track input locale changes
	case WM_INPUTLANGCHANGEREQUEST: return handleDefault(); break;
	case WM_INPUTLANGCHANGE: return handleDefault(); break;

	case WM_STYLECHANGING:
		return getResultForStatus("WM_STYLECHANGING ",
				win->handleStyleChanging(WindowsWindow::StyleType(wParam),
						reinterpret_cast<STYLESTRUCT *>(lParam)));
		break;
	case WM_STYLECHANGED:
		return getResultForStatus("WM_STYLECHANGED ",
				win->handleStyleChanged(WindowsWindow::StyleType(wParam),
						reinterpret_cast<STYLESTRUCT *>(lParam)));
		break;

	// TODO: icon support
	case WM_GETICON: return handleDefault(); break;
	case WM_SETICON: return handleDefault(); break;

	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
	case WM_KEYUP:
	case WM_SYSKEYUP: {
		core::InputKeyCode key;
		int scancode;
		WORD keyFlags = HIWORD(lParam);
		scancode = (HIWORD(lParam) & (KF_EXTENDED | 0xff));
		if (!scancode) {
			// NOTE: Some synthetic key messages have a scancode of zero
			// HACK: Map the virtual key back to a usable scancode
			scancode = MapVirtualKeyW((UINT)wParam, MAPVK_VK_TO_VSC);
		}

		// HACK: Alt+PrtSc has a different scancode than just PrtSc
		if (scancode == 0x54) {
			scancode = 0x137;
		}

		// HACK: Ctrl+Pause has a different scancode than just Pause
		if (scancode == 0x146) {
			scancode = 0x45;
		}

		// HACK: CJK IME sets the extended bit for right Shift
		if (scancode == 0x136) {
			scancode = 0x36;
		}

		key = KeyCodes::getInstance().keycodes[scancode];

		MSG next;
		bool hasNext = PeekMessageW(&next, hwnd, 0, 0, PM_NOREMOVE);

		// The Ctrl keys require special handling
		if (wParam == VK_CONTROL) {
			if (HIWORD(lParam) & KF_EXTENDED) {
				// Right side keys have the extended key bit set
				key = core::InputKeyCode::RIGHT_CONTROL;
			} else {
				// NOTE: Alt Gr sends Left Ctrl followed by Right Alt
				// HACK: We only want one event for Alt Gr, so if we detect
				//       this sequence we discard this Left Ctrl message now
				//       and later report Right Alt normally
				const DWORD time = GetMessageTime();
				if (hasNext
						&& (next.message == WM_KEYDOWN || next.message == WM_SYSKEYDOWN
								|| next.message == WM_KEYUP || next.message == WM_SYSKEYUP)) {
					if (next.wParam == VK_MENU && (HIWORD(next.lParam) & KF_EXTENDED)
							&& next.time == time) {
						// Next message is Right Alt down so discard this
						return DefWindowProcW(hwnd, uMsg, wParam, lParam);
					}
				}

				// This is a regular Left Ctrl message
				key = core::InputKeyCode::LEFT_CONTROL;
			}
		} else if (wParam == VK_PROCESSKEY) {
			// IME notifies that keys have been filtered by setting the
			// virtual key-code to VK_PROCESSKEY}
			return DefWindowProc(hwnd, uMsg, wParam, lParam);
			break;
		}

		char32_t c = 0;
		if (hasNext && (next.message == WM_CHAR || next.message == WM_SYSCHAR)) {
			c = next.wParam;
			PeekMessageW(&next, hwnd, 0, 0, PM_REMOVE);
			TranslateMessage(&next);
			if (next.wParam >= 0xd800 && next.wParam <= 0xdbff) {
				auto highSurrogate = (WCHAR)wParam;
				// remove without processing
				hasNext = PeekMessageW(&next, hwnd, 0, 0, PM_NOREMOVE);
				if (hasNext && (next.message == WM_CHAR || next.message == WM_SYSCHAR)) {
					PeekMessageW(&next, hwnd, 0, 0, PM_REMOVE);
					TranslateMessage(&next);
					if (next.wParam >= 0xdc00 && next.wParam <= 0xdfff) {
						c += (highSurrogate - 0xd800) << 10;
						c += (WCHAR)next.wParam - 0xdc00;
						c += 0x1'0000;
					}
				}
			}
		}

		Status status = Status::Propagate;
		if (wParam == VK_SNAPSHOT) {
			// HACK: Key down is not reported for the Print Screen key
			status = win->handleKeyPress(key, scancode, c);
			status = win->handleKeyRelease(key, scancode, c);
		} else {
			if ((HIWORD(lParam) & KF_UP)) {
				status = win->handleKeyRelease(key, scancode, c);
			} else {
				if ((keyFlags & KF_REPEAT) == KF_REPEAT) {
					status = win->handleKeyRepeat(key, scancode, c, LOWORD(lParam));
				} else {
					status = win->handleKeyPress(key, scancode, c);
				}
			}
		}
		return getResultForStatus("WM_KEYDOWN", status);
		break;
	}
	case WM_CHAR:
	case WM_SYSCHAR: return getResultForStatus("WM_CHAR", win->handleChar((WCHAR)wParam)); break;
	case WM_UNICHAR:
		// To enable WM_UNICHAR processing, send TRUE for UNICODE_NOCHAR
		if (wParam == UNICODE_NOCHAR) {
			return TRUE;
		}

		return getResultForStatus("WM_UNICHAR", win->handleChar(char32_t(wParam)));
		break;

	case WM_ACTIVATEAPP: return handleDefault(); break;

	case WM_MOUSEMOVE:
		return getResultForStatus("WM_MOUSEMOVE",
				win->handleMouseMove(IVec2(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)), false));
		break;
	case WM_NCMOUSEMOVE:
		return getResultForStatus("WM_NCMOUSEMOVE",
				win->handleDecorationsMouseMove(IVec2(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam))));
		break;

	case WM_LBUTTONDOWN:
		return getResultForStatus("WM_LBUTTONDOWN",
				win->handleMouseEvent(IVec2(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)),
						core::InputMouseButton::MouseLeft, core::InputEventName::Begin));
		break;
	case WM_RBUTTONDOWN:
		return getResultForStatus("WM_RBUTTONDOWN",
				win->handleMouseEvent(IVec2(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)),
						core::InputMouseButton::MouseRight, core::InputEventName::Begin));
		break;
	case WM_MBUTTONDOWN:
		return getResultForStatus("WM_MBUTTONDOWN",
				win->handleMouseEvent(IVec2(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)),
						core::InputMouseButton::MouseMiddle, core::InputEventName::Begin));
		break;
	case WM_XBUTTONDOWN:
		return getResultForStatus("WM_XBUTTONDOWN",
				win->handleMouseEvent(IVec2(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)),
						(GET_XBUTTON_WPARAM(wParam) == XBUTTON1) ? core::InputMouseButton::Mouse8
																 : core::InputMouseButton::Mouse9,
						core::InputEventName::Begin),
				TRUE);
		break;
	case WM_LBUTTONUP:
		return getResultForStatus("WM_LBUTTONUP",
				win->handleMouseEvent(IVec2(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)),
						core::InputMouseButton::MouseLeft, core::InputEventName::End));
	case WM_RBUTTONUP:
		return getResultForStatus("WM_RBUTTONUP",
				win->handleMouseEvent(IVec2(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)),
						core::InputMouseButton::MouseRight, core::InputEventName::End));
	case WM_MBUTTONUP:
		return getResultForStatus("WM_MBUTTONUP",
				win->handleMouseEvent(IVec2(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)),
						core::InputMouseButton::MouseMiddle, core::InputEventName::End));
	case WM_XBUTTONUP:
		return getResultForStatus("WM_XBUTTONUP",
				win->handleMouseEvent(IVec2(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)),
						(GET_XBUTTON_WPARAM(wParam) == XBUTTON1) ? core::InputMouseButton::Mouse8
																 : core::InputMouseButton::Mouse9,
						core::InputEventName::End),
				TRUE);

	case WM_MOUSEWHEEL:
		return getResultForStatus("WM_MOUSEWHEEL",
				win->handleMouseWheel(Vec2(0.0f, (SHORT)HIWORD(wParam) / (float)WHEEL_DELTA)));
		break;

	case WM_MOUSEHWHEEL:
		return getResultForStatus("WM_MOUSEHWHEEL",
				win->handleMouseWheel(Vec2(-((SHORT)HIWORD(wParam) / (float)WHEEL_DELTA), 0.0f)));
		break;

	case WM_MOUSELEAVE: return getResultForStatus("WM_MOUSELEAVE", win->handleMouseLeave()); break;

	case WM_CAPTURECHANGED:
		return getResultForStatus("WM_CAPTURECHANGED", win->handleMouseCaptureChanged());
		break;

	case WM_WINDOWPOSCHANGING:
		return getResultForStatus("WM_WINDOWPOSCHANGING",
				win->handlePositionChanging((WINDOWPOS *)lParam));
		break;
	case WM_WINDOWPOSCHANGED:
		return getResultForStatus("WM_WINDOWPOSCHANGED",
				win->handlePositionChanged((const WINDOWPOS *)lParam));
		break;
	case WM_SIZING:
		return getResultForStatus("WM_SIZING",
				win->handleSizing(getViewConstraintsForSizing(wParam), (RECT *)lParam), TRUE);
		break;
	case WM_MOVING:
		return getResultForStatus("WM_MOVING", win->handleMoving((RECT *)lParam), TRUE);
		break;
	case WM_ENTERSIZEMOVE:
		return getResultForStatus("WM_ENTERSIZEMOVE", win->handleMoveResize(true));
		break;
	case WM_EXITSIZEMOVE:
		return getResultForStatus("WM_EXITSIZEMOVE", win->handleMoveResize(false));
		break;
	case WM_SYSCOMMAND: return handleDefault(); break;

	case WM_GETMINMAXINFO:
		return getResultForStatus("WM_GETMINMAXINFO", win->handleMinMaxInfo((MINMAXINFO *)lParam));
		break;
	case WM_GETDPISCALEDSIZE:
		XL_WIN32_LOG("Event: WM_GETDPISCALEDSIZE");
		return handleDefault();
		break;
	case WM_DPICHANGED:
		return getResultForStatus("WM_DPICHANGED",
				win->handleDpiChanged(
						Vec2(float(LOWORD(wParam)) / float(USER_DEFAULT_SCREEN_DPI),
								float(HIWORD(wParam)) / float(USER_DEFAULT_SCREEN_DPI)),
						(RECT *)lParam));
		break;
	case WM_SETTINGCHANGE: return handleDefault(); break;

	case WM_NCCALCSIZE:
		if (wParam == TRUE) {
			return getResultForStatus("WM_NCCALCSIZE",
					win->handleWindowDecorations(true, (NCCALCSIZE_PARAMS *)lParam, nullptr));
		} else {
			return getResultForStatus("WM_NCCALCSIZE",
					win->handleWindowDecorations(false, nullptr, (RECT *)lParam));
		}
		break;
	case WM_NCHITTEST: return win->handleHitTest(wParam, lParam); break;

	case WM_NCPAINT:
		return getResultForStatus("WM_NCPAINT", win->handleWindowDecorationsPaint(wParam, lParam));
		break;
	case WM_NCACTIVATE: return win->handleWindowDecorationsActivate(wParam, lParam); break;
	case WM_NCLBUTTONDOWN:
	case WM_NCLBUTTONUP:
	case WM_NCLBUTTONDBLCLK:
	case WM_NCRBUTTONDOWN:
	case WM_NCRBUTTONUP:
	case WM_NCRBUTTONDBLCLK:
	case WM_NCMBUTTONDOWN:
	case WM_NCMBUTTONUP:
	case WM_NCMBUTTONDBLCLK: return handleDefault(); break;
	case WM_NCMOUSEHOVER: return handleDefault(); break;
	case WM_NCMOUSELEAVE:
		return getResultForStatus("WM_NCMOUSELEAVE", win->handleDecorationsMouseLeave());
		break;
	case WM_SYNCPAINT: return handleDefault(); break;

	case WM_IME_SETCONTEXT:
		XL_WIN32_LOG("Event: WM_IME_SETCONTEXT: ", wParam);
		return handleDefault();
		break;
	case WM_IME_NOTIFY:
		switch (wParam) {
		case IMN_CHANGECANDIDATE: XL_WIN32_LOG("Event: WM_IME_NOTIFY: IMN_CHANGECANDIDATE"); break;
		case IMN_CLOSECANDIDATE: XL_WIN32_LOG("Event: WM_IME_NOTIFY: IMN_CLOSECANDIDATE"); break;
		case IMN_CLOSESTATUSWINDOW:
			XL_WIN32_LOG("Event: WM_IME_NOTIFY: IMN_CLOSESTATUSWINDOW");
			break;
		case IMN_GUIDELINE: XL_WIN32_LOG("Event: WM_IME_NOTIFY: IMN_GUIDELINE"); break;
		case IMN_OPENCANDIDATE: XL_WIN32_LOG("Event: WM_IME_NOTIFY: IMN_OPENCANDIDATE"); break;
		case IMN_OPENSTATUSWINDOW:
			XL_WIN32_LOG("Event: WM_IME_NOTIFY: IMN_OPENSTATUSWINDOW");
			break;
		case IMN_SETCANDIDATEPOS: XL_WIN32_LOG("Event: WM_IME_NOTIFY: IMN_SETCANDIDATEPOS"); break;
		case IMN_SETCOMPOSITIONFONT:
			XL_WIN32_LOG("Event: WM_IME_NOTIFY: IMN_SETCOMPOSITIONFONT");
			break;
		case IMN_SETCOMPOSITIONWINDOW:
			XL_WIN32_LOG("Event: WM_IME_NOTIFY: IMN_SETCOMPOSITIONWINDOW");
			break;
		case IMN_SETCONVERSIONMODE:
			XL_WIN32_LOG("Event: WM_IME_NOTIFY: IMN_SETCONVERSIONMODE");
			break;
		case IMN_SETOPENSTATUS: XL_WIN32_LOG("Event: WM_IME_NOTIFY: IMN_SETOPENSTATUS"); break;
		case IMN_SETSENTENCEMODE: XL_WIN32_LOG("Event: WM_IME_NOTIFY: IMN_SETSENTENCEMODE"); break;
		case IMN_SETSTATUSWINDOWPOS:
			XL_WIN32_LOG("Event: WM_IME_NOTIFY: IMN_SETSTATUSWINDOWPOS");
			break;
		default: break;
		}
		return handleDefault();
		break;
	case WM_IME_REQUEST:
		switch (wParam) {
		case IMR_CANDIDATEWINDOW: XL_WIN32_LOG("Event: WM_IME_REQUEST: IMR_CANDIDATEWINDOW"); break;
		case IMR_COMPOSITIONFONT: XL_WIN32_LOG("Event: WM_IME_REQUEST: IMR_COMPOSITIONFONT"); break;
		case IMR_COMPOSITIONWINDOW:
			XL_WIN32_LOG("Event: WM_IME_REQUEST: IMR_COMPOSITIONWINDOW");
			break;
		case IMR_CONFIRMRECONVERTSTRING:
			XL_WIN32_LOG("Event: WM_IME_REQUEST: IMR_CONFIRMRECONVERTSTRING");
			break;
		case IMR_DOCUMENTFEED: XL_WIN32_LOG("Event: WM_IME_NOTIFY: IMR_DOCUMENTFEED"); break;
		case IMR_QUERYCHARPOSITION:
			XL_WIN32_LOG("Event: WM_IME_REQUEST: IMR_QUERYCHARPOSITION");
			break;
		case IMR_RECONVERTSTRING: XL_WIN32_LOG("Event: WM_IME_REQUEST: IMR_RECONVERTSTRING"); break;
		}
		return handleDefault();
		break;

	case WM_DWMNCRENDERINGCHANGED:
		XL_WIN32_LOG("Event: WM_DWMNCRENDERINGCHANGED: ", wParam);
		return 0;
		break;

	case WM_DISPLAYCHANGE:
		XL_WIN32_LOG("Event: WM_DISPLAYCHANGE");
		return handleDefault();
		break;

	case WM_MENUSELECT:
	case WM_MENUCHAR:
	case WM_ENTERIDLE: return handleDefault(); break;

	default:
		XL_WIN32_LOG("Event: ", std::hex, uMsg);
		return handleDefault();
		break;
	}

	return 0;
}

WindowsWindow *WindowClass::getWindow(HWND win) const {
	auto it = _windows.find(win);
	if (it != _windows.end()) {
		return it->second;
	}
	return nullptr;
}

} // namespace stappler::xenolith::platform
