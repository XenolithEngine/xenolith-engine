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

#include "XLLinuxContextController.h"
#include "SPEvent.h"
#include "XLContext.h"
#include "XLContextInfo.h"
#include "XLCoreInstance.h"
#include "SPEventLooper.h"
#include "SPEventPollHandle.h"
#include "linux/dbus/XLLinuxDBusController.h"
#include "linux/wayland/XLLinuxWaylandWindow.h"
#include "linux/xcb/XLLinuxXcbWindow.h"
#include "platform/XLContextController.h"
#include "platform/XLContextNativeWindow.h"

#if MODULE_XENOLITH_BACKEND_VK
#include "XLVkInstance.h"
#endif

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

#if MODULE_XENOLITH_BACKEND_VK
static vk::SurfaceBackendMask checkPresentationSupport(LinuxContextController *c,
		const vk::Instance *instance, VkPhysicalDevice device, uint32_t queueIdx) {

	vk::SurfaceBackendMask ret;
	if (instance->getSurfaceBackends().test(toInt(vk::SurfaceBackend::Wayland))) {
		if (auto display = c->getWaylandDisplay()) {
			auto supports = instance->vkGetPhysicalDeviceWaylandPresentationSupportKHR(device,
					queueIdx, display->display);
			if (supports) {
				ret.set(toInt(vk::SurfaceBackend::Wayland));
			}
		}
	}

	if (instance->getSurfaceBackends().test(toInt(vk::SurfaceBackend::Xcb))) {
		if (auto conn = c->getXcbConnection()) {
			auto xcbConn = conn->getConnection();
			auto screen = conn->getDefaultScreen();
			auto supports = instance->vkGetPhysicalDeviceXcbPresentationSupportKHR(device, queueIdx,
					xcbConn, screen->root_visual);
			if (supports) {
				ret.set(toInt(vk::SurfaceBackend::Xcb));
			}
		}
	}
	return ret;
}
#endif

void LinuxContextController::acquireDefaultConfig(ContextConfig &config,
		NativeContextHandle *handle) {
	if (config.instance->api == core::InstanceApi::None) {
		config.instance->api = core::InstanceApi::Vulkan;
	}

	if (config.context) {
		config.context->flags |= ContextFlags::DestroyWhenAllWindowsClosed;
		if (auto bundleName = getAppconfigBundleName()) {
			config.context->bundleName = bundleName;
		}
		if (auto appName = getAppconfigBundleName()) {
			config.context->appName = appName;
		}
		if (auto verCode = getAppconfigVersionIndex()) {
			config.context->appVersionCode = verCode;
			config.context->appVersion = toString(getAppconfigVersionApi(), ".",
					getAppconfigVersionRev(), ".", getAppconfigVersionBuild());
		}
	}

	if (config.loop) {
		config.loop->defaultFormat = core::ImageFormat::B8G8R8A8_UNORM;
	}

	if (config.window) {
		if (config.window->imageFormat == core::ImageFormat::Undefined) {
			config.window->imageFormat = core::ImageFormat::B8G8R8A8_UNORM;
		}
		config.window->flags |=
				WindowCreationFlags::Regular | WindowCreationFlags::PreferServerSideDecoration;
	}
}

bool LinuxContextController::init(NotNull<Context> ctx, ContextConfig &&config) {
	if (!ContextController::init(ctx)) {
		return false;
	}

	_contextInfo = move(config.context);
	_windowInfo = move(config.window);
	_instanceInfo = move(config.instance);
	_loopInfo = move(config.loop);

	_xcb = Rc<XcbLibrary>::create();
	_xkb = Rc<XkbLibrary>::create();
	_dbus = Rc<dbus::Library>::create();
	_wayland = Rc<WaylandLibrary>::create();

	_looper = event::Looper::acquire(
			event::LooperInfo{.workersCount = _contextInfo->mainThreadsCount});

	handleThemeInfoChanged(ThemeInfo{ThemeInfo::SchemeDefault.str<Interface>()});

	return true;
}

int LinuxContextController::run(NotNull<ContextContainer> container) {
	_context->handleConfigurationChanged(move(_contextInfo));

	_contextInfo = nullptr;
	_dbusController = Rc<dbus::Controller>::create(_dbus, _looper, this);

	_looper->performOnThread([this] {
		_dbusController->setup();

		Rc<core::Instance> instance;

		auto sessionType = ::getenv("SP_SESSION_TYPE");
		if (!sessionType) {
			sessionType = ::getenv("XDG_SESSION_TYPE");
		}

		if (StringView(sessionType) == "wayland") {
			if (_wayland && _xkb) {
				_waylandDisplay = Rc<WaylandDisplay>::create(_wayland, _xkb);
			}

			if (!_waylandDisplay) {
				if (_xcb && _xkb) {
					_xcbConnection = Rc<XcbConnection>::create(_xcb, _xkb);
				}

				if (!_xcbConnection) {
					log::source().error("LinuxContextController",
							"Fail to connect to X server or Wayland server");
					_resultCode = -1;
					destroy();
					return;
				}
			}

			instance = loadInstance();
			if (instance && !hasFlag(_context->getInfo()->flags, ContextFlags::Headless)
					&& _waylandDisplay) {
				// Try to load with Wayland only, then check if we have device to present images
				// If not, try to use XWayland
				bool supportsPresentation = false;
				if (instance) {
					for (auto &it : instance->getAvailableDevices()) {
						if (it.supportsPresentation) {
							supportsPresentation = true;
							break;
						}
					}
				}
				if (!supportsPresentation) {
					// Load x11 server for XWayland, then recreate instance
					if (_xcb && _xkb) {
						_xcbConnection = Rc<XcbConnection>::create(_xcb, _xkb);
					}
					instance = loadInstance();
				}
			}
		} else if (StringView(sessionType) == "x11") {
			// X11 session

			if (_xcb && _xkb) {
				_xcbConnection = Rc<XcbConnection>::create(_xcb, _xkb);
			}
			instance = loadInstance();
		} else {
			log::source()
				.error("LinuxContextController",
									"No X11 or Wayland session detected; If there were, please "
									"consider to " "set XDG_SESSION_TYPE appropiriately");

			// maybe, on a rainy day, we implement DirectFramebuffer or something but for now in't time to...
			destroy();
			return;
		}

		if (!instance) {
			log::source().error("LinuxContextController", "Fail to load gAPI instance");
			_resultCode = -1;
			destroy();
			return;
		} else {
			if (auto loop = makeLoop(instance)) {
				_context->handleGraphicsLoaded(loop);
			}
		}

		if (_xcbConnection) {
			_xcbConnection->setSystemNotificationHandler([this](SystemNotification notification) {
				_context->handleSystemNotification(notification);
			});

			_xcbPollHandle = _looper->listenPollableHandle(_xcbConnection->getSocket(),
					event::PollFlags::In | event::PollFlags::AllowMulti,
					[this](int fd, event::PollFlags flags) {
				retainPollDepth();
				_xcbConnection->poll();
				releasePollDepth();

				notifyPendingWindows();

				return Status::Ok;
			}, this);
		}

		if (_waylandDisplay) {
			_waylandPollHandle = _looper->listenPollableHandle(_waylandDisplay->getFd(),
					event::PollFlags::In | event::PollFlags::AllowMulti,
					[this](int fd, event::PollFlags flags) {
				if (hasFlag(flags, event::PollFlags::Err)) {
					return Status::ErrorCancelled;
				}
				if (hasFlag(flags, event::PollFlags::Out)) {
					_waylandDisplay->flush();
				}
				if (hasFlag(flags, event::PollFlags::In)) {
					retainPollDepth();
					_waylandDisplay->poll();
					releasePollDepth();
				}

				notifyPendingWindows();

				return Status::Ok;
			}, this);
		}
	}, this);

	_looper->run();

	return ContextController::run(container);
}

bool LinuxContextController::isCursorSupported(WindowCursor cursor, bool serverSize) const {
	if (_xcbConnection) {
		return _xcbConnection->isCursorSupported(cursor);
	} else if (_waylandDisplay) {
		return _waylandDisplay->isCursorSupported(cursor, serverSize);
	}
	return false;
}

WindowCapabilities LinuxContextController::getCapabilities() const {
	if (_xcbConnection) {
		return _xcbConnection->getCapabilities();
	} else if (_waylandDisplay) {
		return _waylandDisplay->getCapabilities();
	}
	return WindowCapabilities::None;
}

void LinuxContextController::notifyScreenChange(NotNull<DisplayConfigManager> info) {
	if (_waylandDisplay) {
		_waylandDisplay->notifyScreenChange();
	}
	if (_xcbConnection) {
		_xcbConnection->notifyScreenChange();
	}

	_context->handleSystemNotification(SystemNotification::DisplayChanged);
}

Status LinuxContextController::readFromClipboard(Rc<ClipboardRequest> &&req) {
	if (_xcbConnection) {
		return _xcbConnection->readFromClipboard(sp::move(req));
	}
	if (_waylandDisplay) {
		return _waylandDisplay->readFromClipboard(sp::move(req));
	}
	return Status::ErrorNotSupported;
}

Status LinuxContextController::probeClipboard(Rc<ClipboardProbe> &&probe) {
	if (_xcbConnection) {
		return _xcbConnection->probeClipboard(sp::move(probe));
	}
	if (_waylandDisplay) {
		return _waylandDisplay->probeClipboard(sp::move(probe));
	}
	return Status::ErrorNotSupported;
}

Status LinuxContextController::writeToClipboard(Rc<ClipboardData> &&data) {
	if (_xcbConnection) {
		return _xcbConnection->writeToClipboard(sp::move(data));
	}
	if (_waylandDisplay) {
		return _waylandDisplay->writeToClipboard(sp::move(data));
	}
	return Status::ErrorNotImplemented;
}

void LinuxContextController::handleThemeInfoChanged(ThemeInfo &&newThemeInfo) {
	newThemeInfo.decorations.borderRadius = 16.0f;
	newThemeInfo.decorations.shadowWidth = 20.0f;
	newThemeInfo.decorations.shadowOffset = Vec2(0.0f, 3.0f);
	if (_themeInfo != newThemeInfo) {
		if (_waylandDisplay) {
			_waylandDisplay->updateThemeInfo(newThemeInfo);
		}
		ContextController::handleThemeInfoChanged(sp::move(newThemeInfo));
	}
}

void LinuxContextController::openUrl(StringView str) {
	auto string = toString("xdg-open '", str, "'");
	auto ret = ::system(string.data());
	if (ret != 0) {
		slog().error("LinuxContextController", "Fail to open URL: ", str);
	}
}

void LinuxContextController::tryStart() {
	if (_dbusController && _dbusController->isConnectied() && (_xcbConnection || _waylandDisplay)) {
		auto dcm = _dbusController->makeDisplayConfigManager(
				[this](NotNull<DisplayConfigManager> m) { notifyScreenChange(m); });
		if (dcm) {
			_displayConfigManager = dcm;
		}

		if (!_displayConfigManager) {
			if (_xcbConnection) {
				_displayConfigManager = _xcbConnection->makeDisplayConfigManager(
						[this](NotNull<DisplayConfigManager> m) { notifyScreenChange(m); });
			} else if (_waylandDisplay) {
				_displayConfigManager = _waylandDisplay->makeDisplayConfigManager(
						[this](NotNull<DisplayConfigManager> m) { notifyScreenChange(m); });
			}
		}

		_looper->performOnThread([this] {
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
		}, this);
	}
}

Rc<core::Instance> LinuxContextController::loadInstance() {
	Rc<core::Instance> instance;
#if MODULE_XENOLITH_BACKEND_VK
	auto instanceInfo = move(_instanceInfo);
	_instanceInfo = nullptr;

	auto instanceBackendInfo = Rc<vk::InstanceBackendInfo>::create();
	instanceBackendInfo->setup = [this](vk::InstanceData &data, const vk::InstanceInfo &info) {
		auto ctxInfo = _context->getInfo();

		if (_xcbConnection) {
			if (info.availableBackends.test(toInt(vk::SurfaceBackend::Xcb))) {
				data.enableBackends.set(toInt(vk::SurfaceBackend::Xcb));
			}
		}
		if (_waylandDisplay) {
			if (info.availableBackends.test(toInt(vk::SurfaceBackend::Wayland))) {
				data.enableBackends.set(toInt(vk::SurfaceBackend::Wayland));
			}
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

bool LinuxContextController::loadWindow() {
	Rc<NativeWindow> window;
	auto wInfo = move(_windowInfo);

	if (configureWindow(wInfo)) {
		if (_waylandDisplay) {
			window = Rc<WaylandWindow>::create(_waylandDisplay, move(wInfo), this);
			_waylandDisplay->flush();
			if (window) {
				_activeWindows.emplace(window);
			}
		}
		if (!window && _xcbConnection) {
			window = Rc<XcbWindow>::create(_xcbConnection, move(wInfo), this);
			if (window) {
				notifyWindowCreated(window);
			}
		}

		if (window) {
			return true;
		}
	}

	return false;
}

void LinuxContextController::handleContextWillDestroy() {
	if (_xcbPollHandle) {
		_xcbPollHandle->cancel();
		_xcbPollHandle = nullptr;
	}

	if (_dbusController) {
		_dbusController->cancel();
		_dbusController = nullptr;
	}

	ContextController::handleContextWillDestroy();
}

void LinuxContextController::handleContextDidDestroy() {
	ContextController::handleContextDidDestroy();

	_xcbConnection = nullptr;

	if (_looper) {
		_looper->wakeup(event::WakeupFlags::Graceful);
	}
}

} // namespace stappler::xenolith::platform
