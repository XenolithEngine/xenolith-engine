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

#include "private/window/linux/SPRTWinLinuxController.h"
#include "private/window/linux/SPRTWinLinuxXcbConnection.h"
#include "private/window/linux/SPRTWinLinuxXcbWindow.h"
#include "private/window/linux/SPRTWinLinuxXcbLibrary.h"
#include "private/window/linux/SPRTWinLinuxWaylandDisplay.h"
#include "private/window/linux/SPRTWinLinuxWaylandWindow.h"
#include "private/window/linux/SPRTWinLinuxWaylandLibrary.h"
#include "private/window/linux/SPRTWinLinuxWaylandSeat.h"
#include "private/window/linux/SPRTWinLinuxWaylandDataDevice.h"
#include "private/window/linux/SPRTWinLinuxWaylandKdeDisplayConfigManager.h"
#include "private/window/linux/SPRTWinLinuxXkbLibrary.h"

#include <sprt/runtime/verutils.h>

#include <stdlib.h>

namespace sprt::window {

void LinuxContextController::acquireDefaultConfig(ContextConfig &config,
		NativeContextHandle *handle) {
	if (config.instance->api == gapi::InstanceApi::None) {
		config.instance->api = gapi::InstanceApi::Vulkan;
	}

	if (config.context) {
		config.context->flags |= ContextFlags::DestroyWhenAllWindowsClosed;

		auto &cfg = getAppConfig();

		if (!cfg.bundleName.empty()) {
			config.context->bundleName = cfg.bundleName.str<String>();
		}
		if (!cfg.appName.empty()) {
			config.context->appName = cfg.appName.str<String>();
		}
		if (cfg.versionCode) {
			config.context->appVersionCode = cfg.versionCode;
			config.context->appVersion =
					StreamTraits<char>::toString(SPRT_VERSION_MAJOR(config.context->appVersionCode),
							".", SPRT_VERSION_MINOR(config.context->appVersionCode), ".",
							SPRT_VERSION_PATCH(config.context->appVersionCode));
		}
	}

	if (config.loop) {
		config.loop->defaultFormat = ImageFormat::B8G8R8A8_UNORM;
	}

	if (config.window) {
		if (config.window->imageFormat == ImageFormat::Undefined) {
			config.window->imageFormat = ImageFormat::B8G8R8A8_UNORM;
		}
		config.window->flags |=
				WindowCreationFlags::Regular | WindowCreationFlags::PreferServerSideDecoration;
	}
}

bool LinuxContextController::init(NotNull<Context> ctx, ContextConfig &&config,
		NotNull<LooperAdapter> looper) {
	if (!ContextController::init(ctx, looper)) {
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

	handleThemeInfoChanged(ThemeInfo{ThemeInfo::SchemeDefault.str<String>()});

	return true;
}

int LinuxContextController::run(NotNull<ContextContainer> container) {
	_context->handleConfigurationChanged(move(_contextInfo));

	_contextInfo = nullptr;
	_dbusController = Rc<dbus::Controller>::create(_dbus, _looper, this);

	_looper->performOnThread([this] {
		_dbusController->setup();

		Rc<gapi::Instance> instance;

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
					log::vperror(__SPRT_LOCATION, "LinuxContextController",
							"Fail to connect to X server or Wayland server");
					_resultCode = -1;
					destroy();
					return;
				}
			}

			instance = _context->makeInstance(_instanceInfo);
			if (instance && !hasFlag(_context->getInfo()->flags, ContextFlags::Headless)
					&& _waylandDisplay) {
				// Try to load with Wayland only, then check if we have device to present images
				// If not, try to use XWayland
				if (!instance->isPresentationSupported()) {
					// Load x11 server for XWayland, then recreate instance
					if (_xcb && _xkb) {
						_xcbConnection = Rc<XcbConnection>::create(_xcb, _xkb);
					}
					instance = _context->makeInstance(_instanceInfo);
				}
			}
		} else if (StringView(sessionType) == "x11") {
			// X11 session

			if (_xcb && _xkb) {
				_xcbConnection = Rc<XcbConnection>::create(_xcb, _xkb);
			}
			instance = _context->makeInstance(_instanceInfo);
		} else {
			log::vperror(__SPRT_LOCATION, "LinuxContextController",
									"No X11 or Wayland session detected; If there were, please "
									"consider to " "set XDG_SESSION_TYPE appropiriately");

			// maybe, on a rainy day, we implement DirectFramebuffer or something but for now in't time to...
			destroy();
			return;
		}

		if (!instance) {
			log::vperror(__SPRT_LOCATION, "LinuxContextController", "Fail to load gAPI instance");
			_resultCode = -1;
			destroy();
			return;
		} else {
			if (auto loop = _context->makeLoop(instance, _loopInfo)) {
				_context->handleGraphicsLoaded(loop);
			}
		}

		if (_xcbConnection) {
			_xcbConnection->setSystemNotificationHandler([this](SystemNotification notification) {
				_context->handleSystemNotification(notification);
			});

			_xcbPollHandle = _looper->listenPollableHandle(_xcbConnection->getSocket(),
					filesystem::PollFlags::In | filesystem::PollFlags::AllowMulti,
					[this](int fd, filesystem::PollFlags flags) {
				retainPollDepth();
				_xcbConnection->poll();
				releasePollDepth();

				notifyPendingWindows();

				return Status::Ok;
			}, this);
		}

		if (_waylandDisplay) {
			_waylandPollHandle = _looper->listenPollableHandle(_waylandDisplay->getFd(),
					filesystem::PollFlags::In | filesystem::PollFlags::AllowMulti,
					[this](int fd, filesystem::PollFlags flags) {
				if (hasFlag(flags, filesystem::PollFlags::Err)) {
					return Status::ErrorCancelled;
				}
				if (hasFlag(flags, filesystem::PollFlags::Out)) {
					_waylandDisplay->flush();
				}
				if (hasFlag(flags, filesystem::PollFlags::In)) {
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
		return _xcbConnection->readFromClipboard(sprt::move(req));
	}
	if (_waylandDisplay) {
		return _waylandDisplay->readFromClipboard(sprt::move(req));
	}
	return Status::ErrorNotSupported;
}

Status LinuxContextController::probeClipboard(Rc<ClipboardProbe> &&probe) {
	if (_xcbConnection) {
		return _xcbConnection->probeClipboard(sprt::move(probe));
	}
	if (_waylandDisplay) {
		return _waylandDisplay->probeClipboard(sprt::move(probe));
	}
	return Status::ErrorNotSupported;
}

Status LinuxContextController::writeToClipboard(Rc<ClipboardData> &&data) {
	if (_xcbConnection) {
		return _xcbConnection->writeToClipboard(sprt::move(data));
	}
	if (_waylandDisplay) {
		return _waylandDisplay->writeToClipboard(sprt::move(data));
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
		ContextController::handleThemeInfoChanged(sprt::move(newThemeInfo));
	}
}

void LinuxContextController::openUrl(StringView str) {
	auto string = toString("xdg-open '", str, "'");
	auto ret = ::system(string.data());
	if (ret != 0) {
		log::vperror(__SPRT_LOCATION, "LinuxContextController", "Fail to open URL: ", str);
	}
}

SurfaceSupportInfo LinuxContextController::getSupportInfo() const {
	SurfaceSupportInfo info;
	if (_waylandDisplay) {
		info.backendMask.set(toInt(SurfaceBackend::Wayland));
		info.wayland.display = _waylandDisplay->display;
	}
	if (_xcbConnection) {
		info.backendMask.set(toInt(SurfaceBackend::Xcb));
		info.xcb.connection = _xcbConnection->getConnection();
		info.xcb.visual_id = _xcbConnection->getDefaultScreen()->root_visual;
	}
	return info;
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
				log::vperror(__SPRT_LOCATION, "LinuxContextController", "Fail to resume Context");
				destroy();
			}

			// check if root window is defined
			if (_windowInfo) {
				if (!loadWindow()) {
					log::vperror(__SPRT_LOCATION, "LinuxContextController",
							"Fail to load root native window");
					destroy();
				}
			}
		}, this);
	}
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
		_looper->wakeup(true);
	}
}

} // namespace sprt::window
