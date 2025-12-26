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

#include "XLWindowsContextController.h"
#include "XLWindowsDisplayConfigManager.h"
#include "XLWindowsWindow.h"
#include "XLWindowsMessageWindow.h"
#include "XLWindowsWindowClass.h"

#if MODULE_XENOLITH_BACKEND_VK
#include "XLVkInstance.h"
#endif

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

#if MODULE_XENOLITH_BACKEND_VK
static vk::SurfaceBackendMask checkPresentationSupport(WindowsContextController *c,
		const vk::Instance *instance, VkPhysicalDevice device, uint32_t queueIdx) {
	vk::SurfaceBackendMask ret;
	if (instance->getSurfaceBackends().test(toInt(vk::SurfaceBackend::Win32))) {
		auto supports = instance->vkGetPhysicalDeviceWin32PresentationSupportKHR(device, queueIdx);
		if (supports) {
			ret.set(toInt(vk::SurfaceBackend::Win32));
		}
	}

	return ret;
}
#endif

void WindowsContextController::acquireDefaultConfig(ContextConfig &config, NativeContextHandle *) {
	if (config.instance->api == core::InstanceApi::None) {
		config.instance->api = core::InstanceApi::Vulkan;
#if DEBUG
		config.instance->flags |= core::InstanceFlags::Validation;
#endif
	}

	if (config.context) {
		config.context->flags |= ContextFlags::DestroyWhenAllWindowsClosed;
	}

	if (config.loop) {
		config.loop->defaultFormat = core::ImageFormat::B8G8R8A8_UNORM;
	}

	if (config.window) {
		if (config.window->imageFormat == core::ImageFormat::Undefined) {
			config.window->imageFormat = core::ImageFormat::B8G8R8A8_UNORM;
		}
		config.window->flags |= WindowCreationFlags::Regular
				| WindowCreationFlags::PreferServerSideDecoration
				| WindowCreationFlags::PreferNativeDecoration;
	}
}

bool WindowsContextController::init(NotNull<Context> ctx, ContextConfig &&config) {
	if (!ContextController::init(ctx)) {
		return false;
	}

	_contextInfo = move(config.context);
	_windowInfo = move(config.window);
	_instanceInfo = move(config.instance);
	_loopInfo = move(config.loop);

	_looper = event::Looper::acquire(
			event::LooperInfo{.workersCount = _contextInfo->mainThreadsCount});

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
		auto instance = loadInstance();
		if (!instance) {
			log::source().error("LinuxContextController", "Fail to load gAPI instance");
			_resultCode = -1;
			destroy();
			return;
		} else {
			if (auto loop = makeLoop(instance)) {
				_context->handleGraphicsLoaded(loop);
				loop = nullptr;

				if (!resume()) {
					log::source().error("LinuxContextController", "Fail to resume Context");
					destroy();
				}

				// check if root window is defined
				if (_windowInfo) {
					if (!loadWindow()) {
						log::source().error("LinuxContextController",
								"Fail to load root native window");
						destroy();
					}
				}
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
		return _messageWindow->readFromClipboard(sp::move(req));
	}
	return Status::ErrorIncompatibleDevice;
}

Status WindowsContextController::writeToClipboard(Rc<ClipboardData> &&data) {
	if (_messageWindow) {
		return _messageWindow->writeToClipboard(sp::move(data));
	}
	return Status::ErrorIncompatibleDevice;
}

Rc<core::Instance> WindowsContextController::loadInstance() {
	Rc<core::Instance> instance;
#if MODULE_XENOLITH_BACKEND_VK
	auto instanceInfo = move(_instanceInfo);
	_instanceInfo = nullptr;

	auto instanceBackendInfo = Rc<vk::InstanceBackendInfo>::create();
	instanceBackendInfo->setup = [this](vk::InstanceData &data, const vk::InstanceInfo &info) {
		auto ctxInfo = _context->getInfo();

		if (info.availableBackends.test(toInt(vk::SurfaceBackend::Win32))) {
			data.enableBackends.set(toInt(vk::SurfaceBackend::Win32));
		}

		data.applicationName = ctxInfo->appName;
		data.applicationVersion = ctxInfo->appVersion;
		data.checkPresentationSupport = [this](const vk::Instance *inst, VkPhysicalDevice device,
												uint32_t queueIdx) {
			return checkPresentationSupport(this, inst, device, queueIdx);
		};
		return true;
	};

	instanceInfo->backend = move(instanceBackendInfo);

	instance = core::Instance::create(move(instanceInfo));
#else
	log::source().error("LinuxContextController", "No available gAPI backends found");
	_resultCode = -1;
#endif
	return instance;
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

} // namespace stappler::xenolith::platform
