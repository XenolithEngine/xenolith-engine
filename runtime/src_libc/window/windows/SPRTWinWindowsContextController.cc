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

#ifndef _WINDOWS_
#define NOMINMAX 1
#define WIN32_LEAN_AND_MEAN 1
#define UNICODE 1
#define _UNICODE 1
#include <Windows.h>
#undef __deallocate
#undef DELETE
#endif

#include <shellapi.h>

#include "private/window/windows/SPRTWinWindowsContextController.h"
#include "private/window/windows/SPRTWinWindowsDisplayConfigManager.h"
#include "private/window/windows/SPRTWinWindowsMessageWindow.h"
#include "private/window/windows/SPRTWinWindowsWindow.h"
#include "private/window/windows/SPRTWinWindowsWindowClass.h"

#include <sprt/runtime/log.h>

namespace sprt::window {

void WindowsContextController::acquireDefaultConfig(ContextConfig &config, NativeContextHandle *) {
	if (config.instance->api == gapi::InstanceApi::None) {
		config.instance->api = gapi::InstanceApi::Vulkan;
#if DEBUG
		config.instance->flags |= gapi::InstanceFlags::Validation;
#endif
	}

	if (config.context) {
		config.context->flags |= ContextFlags::DestroyWhenAllWindowsClosed;
	}

	if (config.loop) {
		config.loop->defaultFormat = ImageFormat::B8G8R8A8_UNORM;
	}

	if (config.window) {
		if (config.window->imageFormat == ImageFormat::Undefined) {
			config.window->imageFormat = ImageFormat::B8G8R8A8_UNORM;
		}
		config.window->flags |= WindowCreationFlags::Regular
				| WindowCreationFlags::PreferServerSideDecoration
				| WindowCreationFlags::PreferNativeDecoration;
	}
}

Rc<WindowsContextController> WindowsContextController::create(NotNull<Context> ctx,
		ContextConfig &&cfg, NotNull<LooperAdapter> looper) {
	return Rc<WindowsContextController>::create(ctx, sprt::move(cfg), looper);
}

bool WindowsContextController::init(NotNull<Context> ctx, ContextConfig &&config,
		NotNull<LooperAdapter> looper) {
	if (!ContextController::init(ctx, looper)) {
		return false;
	}

	_contextInfo = move(config.context);
	_windowInfo = move(config.window);
	_instanceInfo = move(config.instance);
	_loopInfo = move(config.loop);

	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

	return true;
}

int WindowsContextController::run(NotNull<ContextContainer> ctx) {
	_context->handleConfigurationChanged(move(_contextInfo));

	// platform main loop

	_messageWindow = Rc<MessageWindow>::create(this);

	_displayConfigManager =
			Rc<WindowsDisplayConfigManager>::create(this, [this](NotNull<DisplayConfigManager> m) {
		_messageWindow->setWindowRect(m->getCurrentConfig()->desktopRect);

		auto cfg = m->getCurrentConfig();
		for (auto &it : _activeWindows) { it.get_cast<WindowsWindow>()->handleDisplayChanged(cfg); }

		handleSystemNotification(SystemNotification::DisplayChanged);
	});

	_looper->performOnThread([this] {
		auto instance = _context->makeInstance(_instanceInfo);
		if (!instance) {
			log::vperror(__SPRT_LOCATION, "WindowsContextController", "Fail to load gAPI instance");
			_resultCode = -1;
			destroy();
			return;
		} else {
			if (auto loop = _context->makeLoop(instance, _loopInfo)) {
				_context->handleGraphicsLoaded(loop);
				loop = nullptr;

				_looper->performOnThread([this] {
					if (!resume()) {
						log::vperror(__SPRT_LOCATION, "WindowsContextController",
								"Fail to resume Context");
						destroy();
					}

					// check if root window is defined
					if (_windowInfo) {
						if (!loadWindow()) {
							log::vperror(__SPRT_LOCATION, "WindowsContextController",
									"Fail to load root native window");
							destroy();
						}
					}
				}, this);
			}
		}
	}, this);

	_looper->run();

	destroy();

	return ContextController::run(ctx);
}

bool WindowsContextController::isCursorSupported(WindowCursor cursor, bool serverSide) const {
	switch (cursor) {
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
	case WindowCursor::Target: return false; break;

	case WindowCursor::Default:
	case WindowCursor::Pointer:

	case WindowCursor::Help:
	case WindowCursor::Progress:
	case WindowCursor::Wait:
	case WindowCursor::Crosshair:
	case WindowCursor::Text:
	case WindowCursor::Move:
	case WindowCursor::NoDrop:
	case WindowCursor::NotAllowed:

	case WindowCursor::AllScroll:
	case WindowCursor::Pencil:

	case WindowCursor::ResizeRight:
	case WindowCursor::ResizeLeft:
	case WindowCursor::ResizeLeftRight:
	case WindowCursor::ResizeCol:
	case WindowCursor::ResizeTop:
	case WindowCursor::ResizeBottom:
	case WindowCursor::ResizeTopBottom:
	case WindowCursor::ResizeRow:
	case WindowCursor::ResizeTopLeft:
	case WindowCursor::ResizeBottomRight:
	case WindowCursor::ResizeTopLeftBottomRight:
	case WindowCursor::ResizeTopRight:
	case WindowCursor::ResizeBottomLeft:
	case WindowCursor::ResizeTopRightBottomLeft:
	case WindowCursor::ResizeAll: return true; break;

	case WindowCursor::Max: break;
	}
	return false;
}

WindowCapabilities WindowsContextController::getCapabilities() const {
	return WindowCapabilities::Fullscreen | WindowCapabilities::FullscreenWithMode
			| WindowCapabilities::FullscreenExclusive
			| WindowCapabilities::FullscreenSeamlessModeSwitch | WindowCapabilities::CloseGuard
			| WindowCapabilities::EnabledState | WindowCapabilities::UserSpaceDecorations
			| WindowCapabilities::GripGuardsRequired | WindowCapabilities::AllowMoveFromMaximized
			| WindowCapabilities::DemandsAttentionState;
}

WindowClass *WindowsContextController::acquuireWindowClass(WideStringView str) {
	auto it = _classes.find(str);
	if (it != _classes.end()) {
		return it->second;
	} else {
		auto cl = Rc<WindowClass>::create(str);
		_classes.emplace(cl->getName(), cl);
		return cl;
	}
}

Status WindowsContextController::handleDisplayChanged(Extent2) {
	_displayConfigManager.get_cast<WindowsDisplayConfigManager>()->update();
	return Status::Ok;
}

void WindowsContextController::handleNetworkStateChanged(NetworkFlags flags) {
	if (_looper->isOnThisThread()) {
		ContextController::handleNetworkStateChanged(flags);
	} else {
		_looper->performOnThread(
				[this, flags] { ContextController::handleNetworkStateChanged(flags); }, this);
	}
}

void WindowsContextController::handleContextWillDestroy() {
	_messageWindow = nullptr;

	ContextController::handleContextWillDestroy();
}

Status WindowsContextController::readFromClipboard(Rc<ClipboardRequest> &&req) {
	if (_messageWindow) {
		return _messageWindow->readFromClipboard(sprt::move(req));
	}
	return Status::ErrorIncompatibleDevice;
}

Status WindowsContextController::writeToClipboard(Rc<ClipboardData> &&data) {
	if (_messageWindow) {
		return _messageWindow->writeToClipboard(sprt::move(data));
	}
	return Status::ErrorIncompatibleDevice;
}

void WindowsContextController::openUrl(StringView str) {
	str.performWithTerminated([](const char *cmd, size_t) {
		ShellExecuteA(NULL, "open", cmd, NULL, NULL, SW_SHOWNORMAL);
	});
}

SurfaceSupportInfo WindowsContextController::getSupportInfo() const {
	SurfaceSupportInfo ret;
	ret.backendMask.set(toInt(SurfaceBackend::Win32));
	return ret;
}

bool WindowsContextController::loadWindow() {
	Rc<NativeWindow> window;
	auto wInfo = move(_windowInfo);

	if (configureWindow(wInfo)) {
		window = Rc<WindowsWindow>::create(this, move(wInfo));
		if (window) {
			notifyWindowCreated(window);
			return true;
		}
	}

	return false;
}

} // namespace sprt::window
