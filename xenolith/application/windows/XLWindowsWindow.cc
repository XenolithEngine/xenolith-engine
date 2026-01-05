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

#include "XLWindowsWindow.h"
#include "XLWindowsWindowClass.h"
#include "XLWindowsContextController.h"
#include "XLAppWindow.h"

#if MODULE_XENOLITH_BACKEND_VK
#include "XLVkPresentationEngine.h" // IWYU pragma: keep
#endif

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

WindowsWindow::~WindowsWindow() {
	if (_window) {
		SetWindowLongPtrW(_window, GWLP_USERDATA, 0);
		DestroyWindow(_window);
		_window = nullptr;
	}
}

bool WindowsWindow::init(NotNull<WindowsContextController> c, Rc<WindowInfo> &&info) {
	if (!NativeWindow::init(c, move(info), c->getCapabilities())) {
		return false;
	}

	auto dcm = c->getDisplayConfigManager();
	auto cfg = dcm->getCurrentConfig();
	if (cfg) {
		for (auto &it : cfg->monitors) {
			auto &c = it.getCurrent();
			_frameRate = std::max(_frameRate, uint32_t(c.mode.rate));
		}
	}

	_wTitle = string::toUtf16<Interface>(_info->title);

	_class = c->acquuireWindowClass(string::toUtf16<Interface>(_info->id));

	RECT rect = {0, 0, long(_info->rect.width), long(_info->rect.height)};

	if (hasFlag(_info->flags, WindowCreationFlags::UserSpaceDecorations)) {
		_currentState.style =
				WS_MAXIMIZEBOX | WS_SYSMENU | WS_THICKFRAME | WS_CAPTION | WS_CLIPCHILDREN;
		_currentState.exstyle = WS_EX_APPWINDOW;
		_info->state |= WindowState::AllowedMove | WindowState::AllowedResize
				| WindowState::AllowedClose | WindowState::AllowedWindowMenu
				| WindowState::AllowedMinimize | WindowState::AllowedMaximizeHorz
				| WindowState::AllowedMaximizeVert | WindowState::AllowedFullscreen;
	} else {
		_currentState.style = WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SYSMENU | WS_CAPTION
				| WS_THICKFRAME | WS_CLIPCHILDREN;
		_currentState.exstyle = WS_EX_APPWINDOW | WS_EX_OVERLAPPEDWINDOW;
	}

	_info->state |= WindowState::Enabled;

	AdjustWindowRect(&rect, _currentState.style, FALSE);

	_currentState.position = IVec2(rect.left, rect.top);
	_currentState.extent = Extent2(rect.right - rect.left, rect.bottom - rect.top);

	_window = CreateWindowExW(_currentState.exstyle, // Optional window styles.
			(wchar_t *)_class->getName().data(), // Window class
			(wchar_t *)_wTitle.data(), // Window text
			_currentState.style, // Window style

			// Size and position
			CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top,

			NULL, // Parent window
			NULL, // Menu
			_class->getModule(), // Instance handle
			nullptr);

	if (_window) {
		SetWindowLongPtrW(_window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

		if (hasFlag(_info->flags, WindowCreationFlags::UserSpaceDecorations)) {
			// To force-enable rounded corners and shadows - uncomment this
			/*DWM_WINDOW_CORNER_PREFERENCE pref = DWMWCP_ROUND;
			if (::DwmSetWindowAttribute(_window, DWMWA_WINDOW_CORNER_PREFERENCE, &pref,
						sizeof(DWM_WINDOW_CORNER_PREFERENCE))
					!= S_OK) {
				log::error("WondowsWindow", "Fail to set DWMWA_WINDOW_CORNER_PREFERENCE");
			}*/

			RECT rcClient;
			GetWindowRect(_window, &rcClient);

			SetWindowLongW(_window, GWL_STYLE, _currentState.style);
			SetWindowPos(_window, 0, rcClient.left, rcClient.top, _currentState.extent.width,
					_currentState.extent.height, SWP_FRAMECHANGED);
		}

		_density = float(GetDpiForWindow(_window)) / float(USER_DEFAULT_SCREEN_DPI);
	}

	return _window != nullptr;
}

void WindowsWindow::mapWindow() {
	ShowWindow(_window, SW_SHOW);
	SetForegroundWindow(_window);
	SetActiveWindow(_window);
	SetFocus(_window);
}

void WindowsWindow::unmapWindow() { }

bool WindowsWindow::close() {
	if (!_controller->notifyWindowClosed(this)) {
		if (hasFlag(_info->state, WindowState::CloseGuard)) {
			updateState(0, _info->state | WindowState::CloseRequest);
		}
		return false;
	}
	return true;
}

void WindowsWindow::handleFramePresented(NotNull<core::PresentationFrame> frame) {
	auto e = frame->getFrameConstraints().extent;
	_commitedExtent = Extent2(e.width, e.height);
}

core::SurfaceInfo WindowsWindow::getSurfaceOptions(const core::Device &dev,
		NotNull<core::Surface> surface) const {
	if (hasFlag(_info->state, WindowState::Fullscreen)
			&& hasFlag(_info->fullscreen.flags, core::FullscreenFlags::Exclusive)) {
		// try to acquire surface info for exclusive fullscreen first
		if (auto hmon = MonitorFromWindow(_window, MONITOR_DEFAULTTONEAREST)) {
			//auto fsMode = core::FullScreenExclusiveMode::Allowed;
			auto fsMode = core::FullScreenExclusiveMode::ApplicationControlled;
			auto info = surface->getSurfaceOptions(dev, fsMode, hmon);
			// check if exclusive fullscreen actually available
			if (info.fullscreenMode == fsMode && info.fullscreenHandle == hmon) {
				return info;
			}
		}
	}

	// default surface info acquisition
	return surface->getSurfaceOptions(dev, core::FullScreenExclusiveMode::Default, nullptr);
}

core::FrameConstraints WindowsWindow::exportConstraints(core::FrameConstraints &&ct) const {
	auto c = NativeWindow::exportConstraints(sp::move(ct));

	c.extent = Extent3(_currentState.extent, 1);
	if (c.density == 0.0f) {
		c.density = 1.0f;
	}
	if (_density != 0.0f) {
		c.density *= _density;
		c.surfaceDensity = _density;
	}

	c.frameInterval = 1'000'000'000ULL / _frameRate;

	return c;
}

Extent2 WindowsWindow::getExtent() const { return _currentState.extent; }

Rc<core::Surface> WindowsWindow::makeSurface(NotNull<core::Instance> cinstance) {
#if MODULE_XENOLITH_BACKEND_VK
	if (cinstance->getApi() != core::InstanceApi::Vulkan) {
		return nullptr;
	}

	auto instance = static_cast<vk::Instance *>(cinstance.get());

	VkSurfaceKHR surface = VK_NULL_HANDLE;
	VkWin32SurfaceCreateInfoKHR createInfo{VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR, nullptr,
		0, _class->getModule(), _window};

	if (instance->vkCreateWin32SurfaceKHR(instance->getInstance(), &createInfo, nullptr, &surface)
			!= VK_SUCCESS) {
		return nullptr;
	}
	return Rc<vk::Surface>::create(instance, surface, this);
#else
	log::source().error("WindowsWindow", "No available GAPI found for a surface");
	return nullptr;
#endif
}

core::PresentationOptions WindowsWindow::getPreferredOptions() const {
	return core::PresentationOptions();
}

bool WindowsWindow::enableState(WindowState state) {
	if (NativeWindow::enableState(state)) {
		return true;
	}
	if (state == WindowState::Maximized) {
		SendMessageW(_window, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
		return true;
	} else if (state == WindowState::Minimized) {
		SendMessageW(_window, WM_SYSCOMMAND, SC_MINIMIZE, 0);
		return true;
	} else if (state == WindowState::DemandsAttention) {
		FlashWindow(_window, TRUE);
		return true;
	}
	return false;
}

bool WindowsWindow::disableState(WindowState state) {
	if (NativeWindow::disableState(state)) {
		return true;
	}
	if (state == WindowState::Maximized) {
		SendMessageW(_window, WM_SYSCOMMAND, SC_RESTORE, 0);
		return true;
	} else if (state == WindowState::DemandsAttention) {
		FlashWindow(_window, FALSE);
		return true;
	}
	return false;
}

void WindowsWindow::openWindowMenu(Vec2 pos) {
	HMENU hMenu = GetSystemMenu(_window, FALSE);
	if (hMenu) {
		MENUITEMINFO mii;
		mii.cbSize = sizeof(MENUITEMINFO);
		mii.fMask = MIIM_STATE;
		mii.fType = 0;

		mii.fState = MF_ENABLED;
		SetMenuItemInfoW(hMenu, SC_RESTORE, FALSE, &mii);
		SetMenuItemInfoW(hMenu, SC_SIZE, FALSE, &mii);
		SetMenuItemInfoW(hMenu, SC_MOVE, FALSE, &mii);
		SetMenuItemInfoW(hMenu, SC_MAXIMIZE, FALSE, &mii);
		SetMenuItemInfoW(hMenu, SC_MINIMIZE, FALSE, &mii);

		mii.fState = MF_GRAYED;

		WINDOWPLACEMENT wp;
		GetWindowPlacement(_window, &wp);

		switch (wp.showCmd) {
		case SW_SHOWMAXIMIZED:
			SetMenuItemInfoW(hMenu, SC_SIZE, FALSE, &mii);
			SetMenuItemInfoW(hMenu, SC_MOVE, FALSE, &mii);
			SetMenuItemInfoW(hMenu, SC_MAXIMIZE, FALSE, &mii);
			SetMenuDefaultItem(hMenu, SC_CLOSE, FALSE);
			break;
		case SW_SHOWMINIMIZED:
			SetMenuItemInfoW(hMenu, SC_MINIMIZE, FALSE, &mii);
			SetMenuDefaultItem(hMenu, SC_RESTORE, FALSE);
			break;
		case SW_SHOWNORMAL:
			SetMenuItemInfoW(hMenu, SC_RESTORE, FALSE, &mii);
			SetMenuDefaultItem(hMenu, SC_CLOSE, FALSE);
			break;
		}

		RECT winrect;
		GetWindowRect(_window, &winrect);

		if (!pos.isValid()) {
			winrect.left += _pointerLocation.x;
			winrect.top += (winrect.bottom - winrect.top) - _pointerLocation.y;
		} else {
			winrect.left += pos.x;
			winrect.top += (winrect.bottom - winrect.top) - pos.y;
		}

		LPARAM cmd = TrackPopupMenu(hMenu, (TPM_RIGHTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD),
				winrect.left, winrect.top, NULL, _window, NULL);

		if (cmd) {
			PostMessageW(_window, WM_SYSCOMMAND, cmd, 0);
		}
	}
}

void WindowsWindow::handleDisplayChanged(const DisplayConfig *cfg) {
	if (hasFlag(_info->state, WindowState::Fullscreen)) {
		auto display = cfg->getLogical(_info->fullscreen.id);
		if (!display) {
			// target monitor not found, exit form fullscreen
			_controller->getLooper()->performOnThread([this] {
				updateState(0, _info->state & ~WindowState::Fullscreen);
				updateWindowState(_savedState);
			}, this);
		} else if (display->rect != _currentState.frame) {
			_currentState.frame = display->rect;
			updateWindowState(_currentState);
		}

		if (display) {
			SetForegroundWindow(_window);
			SetActiveWindow(_window);
			SetFocus(_window);
			EnableWindow(_window, TRUE);
		}
	}
}

Status WindowsWindow::handleDestroy() {
	XL_WIN32_LOG(std::source_location::current().function_name());
	PostQuitMessage(0);
	return Status::Ok;
}

Status WindowsWindow::handleMove(IVec2 pos) {
	XL_WIN32_LOG(std::source_location::current().function_name());
	_currentState.position = pos;
	return Status::Propagate;
}

Status WindowsWindow::handleResize(Extent2 e, WindowState state, WindowState mask) {
	XL_WIN32_LOG(std::source_location::current().function_name(), " ", e.width, " ", e.height);
	if (_currentState.extent != e) {
		_currentState.extent = e;
		_controller->notifyWindowConstraintsChanged(this,
				core::UpdateConstraintsFlags::DeprecateSwapchain);
	}
	auto newState = (_info->state & ~mask) | state;
	if (newState != _info->state) {
		updateState(0, newState);
	}
	return Status::Propagate;
}

Status WindowsWindow::handleActivate(ActivateStatus st) {
	XL_WIN32_LOG(std::source_location::current().function_name());

	if (hasFlag(_info->flags, WindowCreationFlags::UserSpaceDecorations)) {
		MARGINS margins;

		margins.cxLeftWidth = -1;
		margins.cxRightWidth = -1;
		margins.cyBottomHeight = -1;
		margins.cyTopHeight = -1;

		auto hr = DwmExtendFrameIntoClientArea(_window, &margins);
		if (!SUCCEEDED(hr)) {
			log::error("WondowsWindow", "Fail to set DwmExtendFrameIntoClientArea");
		}
	}

	return Status::Ok;
}

Status WindowsWindow::handleFocus(bool focusGain) {
	XL_WIN32_LOG(std::source_location::current().function_name(), " ", focusGain);

	if (focusGain) {
		updateState(0, _info->state | WindowState::Focused);
	} else {
		updateState(0, _info->state & ~WindowState::Focused);
	}
	return Status::Propagate;
}

Status WindowsWindow::handleEnabled(bool enabled) {
	XL_WIN32_LOG(std::source_location::current().function_name());

	if (enabled) {
		updateState(0, _info->state | WindowState::Enabled);
	} else {
		updateState(0, _info->state & ~WindowState::Enabled);
	}
	return Status::Propagate;
}

Status WindowsWindow::handlePaint() {
	if (hasFlag(_info->state, WindowState::Resizing)) {
		//XL_WIN32_LOG(std::source_location::current().function_name());
		if (_appWindow) {
			_appWindow->getPresentationEngine()->waitUntilFramePresentation();
			return Status::Ok;
		}
	}
	return Status::Propagate;
}

Status WindowsWindow::handleClose() {
	XL_WIN32_LOG(std::source_location::current().function_name());

	if (!_controller->notifyWindowClosed(this)) {
		if (hasFlag(_info->state, WindowState::CloseGuard)) {
			updateState(0, _info->state | WindowState::CloseRequest);
		}
	}
	return Status::Ok;
}

Status WindowsWindow::handleEraseBackground() {
	//XL_WIN32_LOG(std::source_location::current().function_name());
	emitAppFrame();
	return Status::Propagate;
}

Status WindowsWindow::handleWindowVisible(bool visible) {
	XL_WIN32_LOG(std::source_location::current().function_name());
	if (visible) {
		updateState(0, _info->state & ~WindowState::Minimized);
	} else {
		updateState(0, _info->state | WindowState::Minimized);
	}
	return Status::Propagate;
}

Status WindowsWindow::handleSetCursor() {
	switch (_currentCursor) {
	case WindowCursor::Undefined:
	case WindowCursor::ContextMenu:
	case WindowCursor::VerticalText:
	case WindowCursor::Cell:
	case WindowCursor::Alias:
	case WindowCursor::Copy:
	case WindowCursor::Grab:
	case WindowCursor::Grabbing:
	case WindowCursor::ZoomIn:
	case WindowCursor::ZoomOut:
	case WindowCursor::DndAsk:
	case WindowCursor::RightPtr:
	case WindowCursor::Target:
	case WindowCursor::Default: SetCursor(LoadCursorW(0, IDC_ARROW)); break;
	case WindowCursor::Pointer: SetCursor(LoadCursorW(0, IDC_HAND)); break;

	case WindowCursor::Help: SetCursor(LoadCursorW(0, IDC_HELP)); break;
	case WindowCursor::Progress: SetCursor(LoadCursorW(0, IDC_APPSTARTING)); break;
	case WindowCursor::Wait: SetCursor(LoadCursorW(0, IDC_WAIT)); break;
	case WindowCursor::Crosshair: SetCursor(LoadCursorW(0, IDC_CROSS)); break;
	case WindowCursor::Text: SetCursor(LoadCursorW(0, IDC_IBEAM)); break;
	case WindowCursor::Move: SetCursor(LoadCursorW(0, IDC_SIZEALL)); break;
	case WindowCursor::NoDrop: SetCursor(LoadCursorW(0, IDC_NO)); break;
	case WindowCursor::NotAllowed: SetCursor(LoadCursorW(0, IDC_NO)); break;

	case WindowCursor::AllScroll: SetCursor(LoadCursorW(0, IDC_SIZEALL)); break;
	case WindowCursor::Pencil: SetCursor(LoadCursorW(0, MAKEINTRESOURCE(32'631))); break;

	case WindowCursor::ResizeRight:
	case WindowCursor::ResizeLeft:
	case WindowCursor::ResizeLeftRight:
	case WindowCursor::ResizeCol: SetCursor(LoadCursorW(0, IDC_SIZEWE)); break;
	case WindowCursor::ResizeTop:
	case WindowCursor::ResizeBottom:
	case WindowCursor::ResizeTopBottom:
	case WindowCursor::ResizeRow: SetCursor(LoadCursorW(0, IDC_SIZENS)); break;
	case WindowCursor::ResizeTopLeft:
	case WindowCursor::ResizeBottomRight:
	case WindowCursor::ResizeTopLeftBottomRight: SetCursor(LoadCursorW(0, IDC_SIZENWSE)); break;
	case WindowCursor::ResizeTopRight:
	case WindowCursor::ResizeBottomLeft:
	case WindowCursor::ResizeTopRightBottomLeft: SetCursor(LoadCursorW(0, IDC_SIZENESW)); break;
	case WindowCursor::ResizeAll: SetCursor(LoadCursorW(0, IDC_SIZEALL)); break;
	case WindowCursor::Max: break;
	}
	return Status::Ok;
}

static StringView getWindowStyleFlagName(DWORD value) {
	switch (value) {
	case WS_OVERLAPPED: return "WS_OVERLAPPED"; break;
	case WS_POPUP: return "WS_POPUP"; break;
	case WS_CHILD: return "WS_CHILD"; break;
	case WS_MINIMIZE: return "WS_MINIMIZE"; break;
	case WS_VISIBLE: return "WS_VISIBLE"; break;
	case WS_DISABLED: return "WS_DISABLED"; break;
	case WS_CLIPSIBLINGS: return "WS_CLIPSIBLINGS"; break;
	case WS_CLIPCHILDREN: return "WS_CLIPCHILDREN"; break;
	case WS_MAXIMIZE: return "WS_MAXIMIZE"; break;
	case WS_CAPTION: return "WS_CAPTION"; break;
	case WS_BORDER: return "WS_BORDER"; break;
	case WS_DLGFRAME: return "WS_DLGFRAME"; break;
	case WS_VSCROLL: return "WS_VSCROLL"; break;
	case WS_HSCROLL: return "WS_HSCROLL"; break;
	case WS_SYSMENU: return "WS_SYSMENU"; break;
	case WS_THICKFRAME: return "WS_THICKFRAME"; break;
	case WS_MINIMIZEBOX: return "WS_MINIMIZEBOX"; break;
	case WS_MAXIMIZEBOX: return "WS_MAXIMIZEBOX"; break;
	}
	return StringView();
}

SPUNUSED static String getWindowStyleName(DWORD value) {
	StringStream out;
	for (auto v : flags(value)) { out << " " << getWindowStyleFlagName(v); }
	return out.str();
}

static StringView getWindowExStyleFlagName(DWORD value) {
	switch (value) {
	case WS_EX_DLGMODALFRAME: return "WS_EX_DLGMODALFRAME"; break;
	case WS_EX_NOPARENTNOTIFY: return "WS_EX_NOPARENTNOTIFY"; break;
	case WS_EX_TOPMOST: return "WS_EX_TOPMOST"; break;
	case WS_EX_ACCEPTFILES: return "WS_EX_ACCEPTFILES"; break;
	case WS_EX_TRANSPARENT: return "WS_EX_TRANSPARENT"; break;
	case WS_EX_MDICHILD: return "WS_EX_MDICHILD"; break;
	case WS_EX_TOOLWINDOW: return "WS_EX_TOOLWINDOW"; break;
	case WS_EX_WINDOWEDGE: return "WS_EX_WINDOWEDGE"; break;
	case WS_EX_CLIENTEDGE: return "WS_EX_CLIENTEDGE"; break;
	case WS_EX_CONTEXTHELP: return "WS_EX_CONTEXTHELP"; break;
	case WS_EX_RIGHT: return "WS_EX_RIGHT"; break;
	case WS_EX_LEFT: return "WS_EX_LEFT"; break;
	case WS_EX_RTLREADING: return "WS_EX_RTLREADING"; break;
	case WS_EX_LEFTSCROLLBAR: return "WS_EX_LEFTSCROLLBAR"; break;
	case WS_EX_CONTROLPARENT: return "WS_EX_CONTROLPARENT"; break;
	case WS_EX_STATICEDGE: return "WS_EX_STATICEDGE"; break;
	case WS_EX_APPWINDOW: return "WS_EX_APPWINDOW"; break;
	case WS_EX_LAYERED: return "WS_EX_LAYERED"; break;
	case WS_EX_NOINHERITLAYOUT: return "WS_EX_NOINHERITLAYOUT"; break;
	case WS_EX_NOREDIRECTIONBITMAP: return "WS_EX_NOREDIRECTIONBITMAP"; break;
	case WS_EX_LAYOUTRTL: return "WS_EX_LAYOUTRTL"; break;
	case WS_EX_COMPOSITED: return "WS_EX_COMPOSITED"; break;
	case WS_EX_NOACTIVATE: return "WS_EX_NOACTIVATE"; break;
	}
	return StringView();
}

SPUNUSED static String getWindowExStyleName(DWORD value) {
	StringStream out;
	for (auto v : flags(value)) { out << " " << getWindowExStyleFlagName(v); }
	return out.str();
}

Status WindowsWindow::handleStyleChanging(StyleType type, STYLESTRUCT *style) {
	XL_WIN32_LOG(std::source_location::current().function_name(),
			type == StyleType::Style ? getWindowStyleName(style->styleNew)
									 : getWindowExStyleName(style->styleNew));
	return Status::Propagate;
}

Status WindowsWindow::handleStyleChanged(StyleType type, const STYLESTRUCT *style) {
	XL_WIN32_LOG(std::source_location::current().function_name());
	switch (type) {
	case StyleType::Style: _currentState.style = style->styleNew; break;
	case StyleType::ExtendedStyle: _currentState.exstyle = style->styleNew; break;
	}
	return Status::Propagate;
}

Status WindowsWindow::handleWindowDecorations(bool enabled, NCCALCSIZE_PARAMS *params, RECT *) {
	if (enabled && hasFlag(_info->flags, WindowCreationFlags::UserSpaceDecorations)) {
		XL_WIN32_LOG(std::source_location::current().function_name(), " ", params->rgrc[0].left,
				" ", params->rgrc[0].top, " ", params->rgrc[0].right, " ", params->rgrc[0].bottom,
				" - ", params->rgrc[1].left, " ", params->rgrc[1].top, " ", params->rgrc[1].right,
				" ", params->rgrc[1].bottom, " - ", params->rgrc[2].left, " ", params->rgrc[2].top,
				" ", params->rgrc[2].right, " ", params->rgrc[2].bottom);

		if (!_activeCommands.empty() && _activeCommands.back() == SC_MAXIMIZE) {
			HMONITOR hMon = MonitorFromWindow(_window, MONITOR_DEFAULTTONEAREST);
			MONITORINFO mi = {sizeof(mi)};
			GetMonitorInfoW(hMon, &mi);
			params->rgrc[0].left = mi.rcWork.left;
			params->rgrc[0].top = mi.rcWork.top;
			params->rgrc[0].right = mi.rcWork.right;
			params->rgrc[0].bottom = mi.rcWork.bottom;
			return Status::Ok;
		}

		return Status::Ok;
	}
	return Status::Propagate;
}

LRESULT WindowsWindow::handleWindowDecorationsActivate(WPARAM wParam, LPARAM lParam) {
	if (hasFlag(_info->flags, WindowCreationFlags::UserSpaceDecorations)) {
		// Documentation says, that
		// "If this parameter is set to -1, DefWindowProc does not repaint the nonclient area to reflect the state change"
		// It's wrong, it will be repainted!
		// So, just return TRUE
		// return DefWindowProc(_window, WM_NCACTIVATE, wParam, -1);

		//return TRUE;
		/*LRESULT ret = TRUE;
		DwmDefWindowProc(_window, WM_NCACTIVATE, wParam, lParam, &ret);
		return ret;*/
	}
	return DefWindowProc(_window, WM_NCACTIVATE, wParam, lParam);
}

Status WindowsWindow::handleWindowDecorationsPaint(WPARAM, LPARAM) {
	if (hasFlag(_info->flags, WindowCreationFlags::UserSpaceDecorations)) {
		return Status::Propagate;
	}
	return Status::Propagate;
}

Status WindowsWindow::handleDecorationsMouseMove(IVec2 ipos) {
	if (hasFlag(_info->flags, WindowCreationFlags::UserSpaceDecorations)) {
		auto pos = IVec2(ipos.x - _currentState.position.x, ipos.y - _currentState.position.y);
		handleMouseMove(pos, true);
	}
	return Status::Propagate;
}

Status WindowsWindow::handleDecorationsMouseLeave() {
	if (hasFlag(_info->flags, WindowCreationFlags::UserSpaceDecorations)) {
		_mouseTrackedNonClient = false;
		if (!_mouseTrackedClient && !_mouseTrackedNonClient) {
			updateState(0, _info->state & ~WindowState::Pointer);
		}
	}
	return Status::Propagate;
}

Status WindowsWindow::handleKeyPress(core::InputKeyCode keyCode, int scancode, char32_t c) {
	_enabledModifiers = KeyCodes::getKeyMods();

	c = makeKeyChar(c);

	auto ev = core::InputEventData{
		uint32_t(keyCode),
		core::InputEventName::KeyPressed,
		{{
			core::InputMouseButton::Touch,
			_enabledModifiers,
			_pointerLocation.x,
			_pointerLocation.y,
		}},
	};
	ev.key.keycode = keyCode;
	ev.key.compose = core::InputKeyComposeState::Nothing;
	ev.key.keysym = scancode;
	ev.key.keychar = c;

	_pendingEvents.emplace_back(sp::move(ev));
	return Status::Ok;
}

Status WindowsWindow::handleKeyRepeat(core::InputKeyCode keyCode, int scancode, char32_t c,
		int count) {
	_enabledModifiers = KeyCodes::getKeyMods();

	c = makeKeyChar(c);

	auto ev = core::InputEventData{
		uint32_t(keyCode),
		core::InputEventName::KeyRepeated,
		{{
			core::InputMouseButton::Touch,
			_enabledModifiers,
			_pointerLocation.x,
			_pointerLocation.y,
		}},
	};
	ev.key.keycode = keyCode;
	ev.key.compose = core::InputKeyComposeState::Nothing;
	ev.key.keysym = scancode;
	ev.key.keychar = c;

	if (!count) {
		_pendingEvents.emplace_back(sp::move(ev));
	} else {
		Vector<core::InputEventData> data;
		for (int i = count; i >= 0; --i) { _pendingEvents.emplace_back(sp::move(ev)); }
	}

	return Status::Ok;
}

Status WindowsWindow::handleKeyRelease(core::InputKeyCode keyCode, int scancode, char32_t c) {
	c = makeKeyChar(c);

	auto ev = core::InputEventData{
		uint32_t(keyCode),
		core::InputEventName::KeyReleased,
		{{
			core::InputMouseButton::Touch,
			_enabledModifiers,
			_pointerLocation.x,
			_pointerLocation.y,
		}},
	};
	ev.key.keycode = keyCode;
	ev.key.compose = c ? core::InputKeyComposeState::Forced : core::InputKeyComposeState::Nothing;
	ev.key.keysym = scancode;
	ev.key.keychar = c;

	_enabledModifiers = KeyCodes::getKeyMods();

	_pendingEvents.emplace_back(sp::move(ev));
	return Status::Ok;
}

Status WindowsWindow::handleChar(char32_t c) {
	_enabledModifiers = KeyCodes::getKeyMods();
	c = makeKeyChar(c);

	if (c && _textInput && isTextInputEnabled()) {
		_textInput->insertText(string::toUtf16<Interface>(c), core::InputKeyComposeState::Forced);
	}
	return Status::Ok;
}

Status WindowsWindow::handleMouseMove(IVec2 pos, bool nonclient) {
	_enabledModifiers = KeyCodes::getKeyMods();

	enableMouseTracked(nonclient);

	auto loc = Vec2(pos.x, int32_t(_currentState.extent.height) - pos.y - 1);
	if (loc == _pointerLocation) {
		return Status::Ok;
	}

	_pointerLocation = loc;

	_pendingEvents.emplace_back(core::InputEventData({
		maxOf<uint32_t>(),
		core::InputEventName::MouseMove,
		{{
			core::InputMouseButton::None,
			_enabledModifiers,
			float(_pointerLocation.x),
			float(_pointerLocation.y),
		}},
	}));
	return Status::Ok;
}

Status WindowsWindow::handleMouseLeave() {
	_mouseTrackedClient = false;
	if (!_mouseTrackedClient && !_mouseTrackedNonClient) {
		updateState(0, _info->state & ~WindowState::Pointer);
	}
	return Status::Ok;
}

Status WindowsWindow::handleMouseEvent(IVec2 pos, core::InputMouseButton btn,
		core::InputEventName ev) {
	switch (ev) {
	case core::InputEventName::Begin:
		if (_pointerButtonCapture++ == 0) {
			::SetCapture(_window);
		}
		break;
	case core::InputEventName::End:
		if (_pointerButtonCapture-- == 1) {
			::ReleaseCapture();
		}
		break;
	default: break;
	}

	_enabledModifiers = KeyCodes::getKeyMods();

	_pendingEvents.emplace_back(core::InputEventData({
		uint32_t(btn),
		ev,
		{{
			btn,
			_enabledModifiers,
			float(_pointerLocation.x),
			float(_pointerLocation.y),
		}},
	}));
	return Status::Ok;
}

Status WindowsWindow::handleMouseWheel(Vec2 value) {
	_enabledModifiers = KeyCodes::getKeyMods();

	core::InputMouseButton btn = core::InputMouseButton::None;
	if (value.x > 0) {
		btn = core::InputMouseButton::MouseScrollRight;
	} else if (value.x < 0) {
		btn = core::InputMouseButton::MouseScrollLeft;
	}

	if (value.y > 0) {
		btn = core::InputMouseButton::MouseScrollDown;
	} else if (value.y < 0) {
		btn = core::InputMouseButton::MouseScrollUp;
	}

	core::InputEventData event({
		toInt(btn),
		core::InputEventName::Scroll,
		{{
			btn,
			_enabledModifiers,
			_pointerLocation.x,
			_pointerLocation.y,
		}},
	});

	event.point.valueX = value.x * 10.0f;
	event.point.valueY = value.y * 10.0f;

	_pendingEvents.emplace_back(sp::move(event));
	return Status::Ok;
}

Status WindowsWindow::handleMouseCaptureChanged() {
	if (_pointerButtonCapture > 0) {
		::ReleaseCapture();
		_pointerButtonCapture = 0;
	}
	return Status::Ok;
}

Status WindowsWindow::handlePositionChanging(WINDOWPOS *pos) {
	XL_WIN32_LOG(std::source_location::current().function_name());

	// Commented code below can lock live resizing until commited extent differs from requested

	/*if (hasFlag(_info->state, WindowState::Resizing)) {
		if (_currentState.extent != _commitedExtent) {
			pos->x = _currentState.frame.x;
			pos->y = _currentState.frame.y;
			pos->cx = _currentState.frame.width;
			pos->cy = _currentState.frame.height;
			return Status::Ok;
		}
	}*/

	if (!_activeCommands.empty() && _activeCommands.back() == SC_MAXIMIZE) {
		HMONITOR hMon = MonitorFromWindow(_window, MONITOR_DEFAULTTONEAREST);
		MONITORINFO mi = {sizeof(mi)};
		GetMonitorInfoW(hMon, &mi);
		pos->x = mi.rcWork.left;
		pos->y = mi.rcWork.top;
		pos->cx = mi.rcWork.right - mi.rcWork.left;
		pos->cy = mi.rcWork.bottom - mi.rcWork.top;
		pos->flags = SWP_FRAMECHANGED | SWP_NOCOPYBITS | SWP_SHOWWINDOW;
		return Status::Ok;
	} else if (_currentState.isFullscreen) {
		auto cfg = _controller->getDisplayConfigManager()->getCurrentConfig();
		auto display = cfg->getLogical(_info->fullscreen.id);
		if (display) {
			pos->x = display->rect.x;
			pos->y = display->rect.y;
			pos->cx = display->rect.width;
			pos->cy = display->rect.height;
			return Status::Ok;
		} else {
			pos->x = _savedState.frame.x;
			pos->y = _savedState.frame.y;
			pos->cx = _savedState.frame.width;
			pos->cy = _savedState.frame.height;

			// target monitor not found, exit form fullscreen
			_controller->getLooper()->performOnThread([this] {
				updateWindowState(_savedState);
				updateState(0, _info->state & ~WindowState::Fullscreen);
			}, this);
			return Status::Ok;
		}
	}
	return Status::Propagate;
}

Status WindowsWindow::handlePositionChanged(const WINDOWPOS *pos) {
	XL_WIN32_LOG(std::source_location::current().function_name(), " ", pos->x, " ", pos->y, " ",
			pos->cx, " ", pos->cy);
	_currentState.frame = IRect{pos->x, pos->y, uint32_t(pos->cx), uint32_t(pos->cy)};
	return Status::Propagate;
}

Status WindowsWindow::handleSizing(core::ViewConstraints, RECT *rect) {
	XL_WIN32_LOG(std::source_location::current().function_name(), " ", rect->left, " ", rect->top,
			" ", rect->right - rect->left, " ", rect->bottom - rect->top);
	return Status::Propagate;
}

Status WindowsWindow::handleMoving(RECT *) {
	XL_WIN32_LOG(std::source_location::current().function_name());
	return Status::Propagate;
}

Status WindowsWindow::handleMoveResize(bool enter) {
	XL_WIN32_LOG(std::source_location::current().function_name());
	if (enter) {
		updateState(0, _info->state | WindowState::Resizing);
	} else {
		updateState(0, _info->state & ~WindowState::Resizing);
	}
	return Status::Ok;
}

Status WindowsWindow::handleDpiChanged(Vec2 scale, const RECT *rect) {
	_density = std::max(scale.x, scale.y);

	auto mon = MonitorFromWindow(_window, MONITOR_DEFAULTTONEAREST);
	auto cfg = _controller->getDisplayConfigManager()->getCurrentConfig();

	uint32_t rate = 0;

	for (auto &it : cfg->logical) {
		if (it.xid == mon) {
			for (auto &mId : it.monitors) {
				auto v = cfg->getMonitor(mId);
				if (v) {
					rate = std::max(v->getCurrent().mode.rate, rate);
				}
			}
			break;
		}
	}

	if (rate == 0) {
		rate = 60'000;
	}

	_frameRate = rate;
	_controller->notifyWindowConstraintsChanged(this, core::UpdateConstraintsFlags::None);

	return Status::Propagate;
}

Status WindowsWindow::handleMinMaxInfo(MINMAXINFO *info) {
	// update to corrent maximize size
	HMONITOR hMon = MonitorFromWindow(_window, MONITOR_DEFAULTTONEAREST);
	MONITORINFO mi = {sizeof(mi)};
	GetMonitorInfoW(hMon, &mi);

	info->ptMaxSize.x = mi.rcWork.right - mi.rcWork.left;
	info->ptMaxSize.y = mi.rcWork.bottom - mi.rcWork.top;
	info->ptMaxPosition.x = mi.rcWork.left;
	info->ptMaxPosition.y = mi.rcWork.top;

	XL_WIN32_LOG(std::source_location::current().function_name(), " ", info->ptMaxSize.x, " ",
			info->ptMaxSize.y, " ", info->ptMaxPosition.x, " ", info->ptMaxPosition.y, " ",
			info->ptMinTrackSize.x, " ", info->ptMinTrackSize.y, " ", info->ptMaxTrackSize.x, " ",
			info->ptMaxTrackSize.y);

	return Status::Ok;
}

LRESULT WindowsWindow::handleHitTest(WPARAM wParam, LPARAM lParam) {
	if (!hasFlag(_info->flags, WindowCreationFlags::UserSpaceDecorations)) {
		return DefWindowProcW(_window, WM_NCHITTEST, wParam, lParam);
	}

	LRESULT res = DefWindowProcW(_window, WM_NCHITTEST, wParam, lParam);
	auto pos = Vec2(GET_X_LPARAM(lParam) - _currentState.position.x,
			(_currentState.extent.height - (GET_Y_LPARAM(lParam) - _currentState.position.y)) - 1);
	bool hasGripGuard = false;
	for (auto &it : _layers) {
		if (hasFlag(it.flags, WindowLayerFlags::GripMask) && it.rect.containsPoint(pos)) {
			switch (it.flags & WindowLayerFlags::GripMask) {
			case WindowLayerFlags::GripGuard: hasGripGuard = true; break;
			case WindowLayerFlags::MoveGrip:
				//XL_WIN32_LOG("HTCAPTION ", pos, " ", it.rect);
				res = HTCAPTION;
				break;
			case WindowLayerFlags::ResizeTopLeftGrip:
				//XL_WIN32_LOG("HTTOPLEFT ", pos, " ", it.rect);
				res = HTTOPLEFT;
				break;
			case WindowLayerFlags::ResizeTopGrip:
				//XL_WIN32_LOG("HTTOP ", pos, " ", it.rect);
				res = HTTOP;
				break;
			case WindowLayerFlags::ResizeTopRightGrip:
				//XL_WIN32_LOG("HTTOPRIGHT ", pos, " ", it.rect);
				res = HTTOPRIGHT;
				break;
			case WindowLayerFlags::ResizeRightGrip:
				//XL_WIN32_LOG("HTRIGHT ", pos, " ", it.rect);
				res = HTRIGHT;
				break;
			case WindowLayerFlags::ResizeBottomRightGrip:
				//XL_WIN32_LOG("HTBOTTOMRIGHT ", pos, " ", it.rect);
				res = HTBOTTOMRIGHT;
				break;
			case WindowLayerFlags::ResizeBottomGrip:
				//XL_WIN32_LOG("HTBOTTOM ", pos, " ", it.rect);
				res = HTBOTTOM;
				break;
			case WindowLayerFlags::ResizeBottomLeftGrip:
				//XL_WIN32_LOG("HTBOTTOMLEFT ", pos, " ", it.rect);
				res = HTBOTTOMLEFT;
				break;
			case WindowLayerFlags::ResizeLeftGrip:
				//XL_WIN32_LOG("HTLEFT ", pos, " ", it.rect);
				res = HTLEFT;
				break;
			default: break;
			}
		}
	}
	if (hasGripGuard) {
		switch (res) {
		case HTTOPLEFT:
		case HTTOP:
		case HTTOPRIGHT:
		case HTRIGHT:
		case HTBOTTOMRIGHT:
		case HTBOTTOM:
		case HTBOTTOMLEFT:
		case HTLEFT: return res; break;
		default: return HTCLIENT;
		}
	}
	return res;
}

SPUNUSED static StringView getCommandName(WPARAM cmd) {
	switch (cmd) {
	case SC_CLOSE: return StringView("SC_CLOSE"); break;
	case SC_CONTEXTHELP: return StringView("SC_CONTEXTHELP"); break;
	case SC_DEFAULT: return StringView("SC_DEFAULT"); break;
	case SC_HOTKEY: return StringView("SC_HOTKEY"); break;
	case SC_HSCROLL: return StringView("SC_HSCROLL"); break;
	case SCF_ISSECURE: return StringView("SCF_ISSECURE"); break;
	case SC_KEYMENU: return StringView("SC_KEYMENU"); break;
	case SC_MAXIMIZE: return StringView("SC_MAXIMIZE"); break;
	case SC_MINIMIZE: return StringView("SC_MINIMIZE"); break;
	case SC_MONITORPOWER: return StringView("SC_MONITORPOWER"); break;
	case SC_MOUSEMENU: return StringView("SC_MOUSEMENU"); break;
	case SC_MOVE: return StringView("SC_MOVE"); break;
	case SC_NEXTWINDOW: return StringView("SC_NEXTWINDOW"); break;
	case SC_PREVWINDOW: return StringView("SC_PREVWINDOW"); break;
	case SC_RESTORE: return StringView("SC_RESTORE"); break;
	case SC_SCREENSAVE: return StringView("SC_SCREENSAVE"); break;
	case SC_SIZE: return StringView("SC_SIZE"); break;
	case SC_TASKLIST: return StringView("SC_TASKLIST"); break;
	case SC_VSCROLL: return StringView("SC_VSCROLL"); break;
	}
	return StringView();
}

void WindowsWindow::pushCommand(WPARAM cmd) {
	XL_WIN32_LOG(std::source_location::current().function_name(), " ", getCommandName(cmd));

	if (cmd == SC_MAXIMIZE) {
		WINDOWPLACEMENT placement;
		placement.length = sizeof(WINDOWPLACEMENT);
		GetWindowPlacement(_window, &placement);

		placement.ptMaxPosition.x = 10;
		placement.ptMaxPosition.y = 10;

		SetWindowPlacement(_window, &placement);
	}

	_activeCommands.emplace_back(cmd);
}

void WindowsWindow::popCommand(WPARAM cmd) {
	sprt_passert(!_activeCommands.empty() && _activeCommands.back() == cmd, "Invalid command");
	XL_WIN32_LOG(std::source_location::current().function_name(), " ", getCommandName(cmd));
	_activeCommands.pop_back();
}

bool WindowsWindow::updateTextInput(const TextInputRequest &, TextInputFlags flags) { return true; }

void WindowsWindow::cancelTextInput() { }

void WindowsWindow::setCursor(WindowCursor cursor) {
	_currentCursor = cursor;
	handleSetCursor();
}

Status WindowsWindow::setFullscreenState(FullscreenInfo &&info) {
	auto makeFullscreenState = [](State current, const LogicalDisplay *display) -> State {
		return State{
			(current.style & (~WS_BORDER) & (~WS_DLGFRAME) & (~WS_THICKFRAME) & (~WS_CAPTION))
					| WS_VISIBLE,
			(current.exstyle & (~WS_EX_WINDOWEDGE) & (~WS_EX_DLGMODALFRAME) & (~WS_EX_CLIENTEDGE)
					& (~WS_EX_STATICEDGE))
					| WS_EX_TOPMOST,
			IVec2(display->rect.x, display->rect.y),
			Extent2(display->rect.width, display->rect.height),
			display->rect,
			true,
		};
	};

	auto enable = info != FullscreenInfo::None;
	if (!enable) {
		if (hasFlag(_info->state, WindowState::Fullscreen)) {
			// disable fullscreen
			_info->fullscreen = move(info);
			updateWindowState(_savedState);
			updateState(0, _info->state & ~WindowState::Fullscreen);
			return Status::Ok;
		} else {
			return Status::Declined;
		}
	} else {
		if (info == FullscreenInfo::Current) {
			if (!hasFlag(_info->state, WindowState::Fullscreen)) {
				_savedState = _currentState;

				auto mon = MonitorFromWindow(_window, MONITOR_DEFAULTTONEAREST);
				auto cfg = _controller->getDisplayConfigManager()->getCurrentConfig();

				const LogicalDisplay *display = nullptr;
				for (auto &it : cfg->logical) {
					if (it.xid == mon) {
						display = &it;
						for (auto &mId : it.monitors) {
							auto v = cfg->getMonitor(mId);
							if (v) {
								info.mode = v->getCurrent().mode;
								info.id = mId;
							}
						}
						break;
					}
				}

				if (display) {
					_currentState = makeFullscreenState(_currentState, display);
					_info->fullscreen = move(info);
					updateWindowState(_currentState);
					updateState(0, _info->state | WindowState::Fullscreen);
					return Status::Ok;
				} else {
					return Status::ErrorInvalidArguemnt;
				}
			}
			return Status::Declined;
		}

		// fullscreen to specific monitor
		if (hasFlag(_info->state, WindowState::Fullscreen) && _info->fullscreen.id == info.id) {
			return Status::Declined;
		}

		auto cfg = _controller->getDisplayConfigManager()->getCurrentConfig();
		const LogicalDisplay *display = nullptr;
		for (auto &it : cfg->logical) {
			if (std::find(it.monitors.begin(), it.monitors.end(), info.id) != it.monitors.end()) {
				display = &it;
				break;
			}
		}
		if (!display) {
			return Status::ErrorInvalidArguemnt;
		}

		if (!hasFlag(_info->state, WindowState::Fullscreen)) {
			_savedState = _currentState;
		}

		_currentState = makeFullscreenState(_currentState, display);
		_info->fullscreen = move(info);
		updateWindowState(_currentState);
		updateState(0, _info->state | WindowState::Fullscreen);

		return Status::Ok;
	} // namespace stappler::xenolith::platform
	return Status::ErrorNotImplemented;
}

char32_t WindowsWindow::makeKeyChar(char32_t c) {
	if (c >= 0xd800 && c <= 0xdbff) {
		_highSurrogate = c;
		return 0;
	} else if (c >= 0xdc00 && c <= 0xdfff) {
		if (_highSurrogate) {
			char32_t ret = (_highSurrogate - 0xd800) << 10;
			ret += c - 0xdc00;
			ret += 0x1'0000;
			_highSurrogate = 0;
			return ret;
		} else {
			_highSurrogate = 0;
			return 0;
		}
	} else if (c != 0) {
		_highSurrogate = 0;
		return c;
	}
	return 0;
}

void WindowsWindow::enableMouseTracked(bool nonclient) {
	bool alreadyTracked = false;
	if (nonclient) {
		alreadyTracked = _mouseTrackedNonClient;
	} else {
		alreadyTracked = _mouseTrackedClient;
	}

	if (!alreadyTracked) {
		TRACKMOUSEEVENT tme;
		ZeroMemory(&tme, sizeof(tme));
		tme.cbSize = sizeof(tme);
		tme.dwFlags = TME_LEAVE;
		if (nonclient) {
			tme.dwFlags |= TME_NONCLIENT;
		}
		tme.hwndTrack = _window;
		::TrackMouseEvent(&tme);

		updateState(0, _info->state | WindowState::Pointer);
	}
}

void WindowsWindow::updateWindowState(const State &state) {
	_currentState.isFullscreen = state.isFullscreen;
	SetWindowLongPtrW(_window, GWL_STYLE, state.style);
	SetWindowLongPtrW(_window, GWL_EXSTYLE, state.exstyle);
	SetWindowPos(_window, hasFlag(state.exstyle, DWORD(WS_EX_TOPMOST)) ? HWND_TOPMOST : HWND_TOP,
			state.frame.x, state.frame.y, state.frame.width, state.frame.height,
			SWP_SHOWWINDOW | SWP_FRAMECHANGED);
	_currentState = state;
	XL_WIN32_LOG(std::source_location::current().function_name());
}

} // namespace stappler::xenolith::platform
