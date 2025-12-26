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

#include "XLContextController.h"
#include "XLContextNativeWindow.h"
#include "XLContext.h"
#include "XLAppWindow.h"
#include "XLDirector.h"
#include "XLAppThread.h"

#if LINUX
#include "linux/XLLinuxContextController.h"
#endif

#if MACOS
#include "macos/XLMacosContextController.h"
#endif

#if WIN32
#include "windows/XLWindowsContextController.h"
#endif

#if ANDROID
#include "android/XLAndroidContextController.h"
#endif

#if MODULE_XENOLITH_BACKEND_VK
#include "XLVkInstance.h"
#endif

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

Rc<ContextController> ContextController::create(NotNull<Context> ctx, ContextConfig &&info) {
#if LINUX
	return Rc<LinuxContextController>::create(ctx, move(info));
#endif
#if MACOS
	return Rc<MacosContextController>::create(ctx, move(info));
#endif
#if WIN32
	return Rc<WindowsContextController>::create(ctx, move(info));
#endif
#if ANDROID
	return Rc<AndroidContextController>::create(ctx, move(info));
#endif
	log::source().error("ContextController", "Unknown platform");
	return nullptr;
}

void ContextController::acquireDefaultConfig(ContextConfig &config, NativeContextHandle *handle) {
#if LINUX
	LinuxContextController::acquireDefaultConfig(config, handle);
#endif
#if MACOS
	MacosContextController::acquireDefaultConfig(config, handle);
#endif
#if WIN32
	WindowsContextController::acquireDefaultConfig(config, handle);
#endif
#if ANDROID
	AndroidContextController::acquireDefaultConfig(config);
#endif

	auto cfgSymbol = SharedModule::acquireTypedSymbol<Context::SymbolMakeConfigSignature>(
			buildconfig::MODULE_APPCOMMON_NAME, Context::SymbolMakeConfigName);
	if (cfgSymbol) {
		cfgSymbol(config);
	}
}

bool ContextController::init(NotNull<Context> ctx) {
	_context = ctx;
	return true;
}

int ContextController::run(NotNull<ContextContainer> с) { return _resultCode; }

void ContextController::retainPollDepth() { ++_pollDepth; }
void ContextController::releasePollDepth() {
	if (_pollDepth-- == 1) {
		notifyPendingWindows();
	}
}

bool ContextController::configureWindow(NotNull<WindowInfo> w) {
	return _context->configureWindow(w);
}

void ContextController::notifyWindowCreated(NotNull<NativeWindow> w) {
	_context->handleNativeWindowCreated(w);
	_activeWindows.emplace(w);
}

void ContextController::notifyWindowConstraintsChanged(NotNull<NativeWindow> w,
		core::UpdateConstraintsFlags flags) {
	if (isWithinPoll()) {
		_resizedWindows.emplace_back(w, flags);
	} else {
		_context->handleNativeWindowConstraintsChanged(w, flags);
	}
}
void ContextController::notifyWindowInputEvents(NotNull<NativeWindow> w,
		Vector<core::InputEventData> &&ev) {
	_context->handleNativeWindowInputEvents(w, sp::move(ev));
}

void ContextController::notifyWindowTextInput(NotNull<NativeWindow> w,
		const core::TextInputState &state) {
	_context->handleNativeWindowTextInput(w, state);
}

bool ContextController::notifyWindowClosed(NotNull<NativeWindow> w, WindowCloseOptions opts) {
	if (isWithinPoll()) {
		_closedWindows.emplace_back(w, opts);
		return !hasFlag(w->getInfo()->state, WindowState::CloseGuard);
	} else if (!hasFlag(opts, WindowCloseOptions::IgnoreExitGuard)
			&& hasFlag(w->getInfo()->state, WindowState::CloseGuard)) {
		return false;
	} else {
		if (hasFlag(opts, WindowCloseOptions::CloseInPlace)) {
			auto it = _activeWindows.find(w.get());
			if (it != _activeWindows.end()) {
				w->unmapWindow();
				_context->handleNativeWindowDestroyed(w);
				_activeWindows.erase(it);
			}
		}
		return true;
	}
}

void ContextController::notifyWindowAllocated(NotNull<NativeWindow> w) { _allWindows.emplace(w); }

void ContextController::notifyWindowDeallocated(NotNull<NativeWindow> w) {
	auto it = _allWindows.find(w);
	if (it != _allWindows.end()) {
		_allWindows.erase(it);
		if (_allWindows.empty()) {
			handleAllWindowsClosed();
		}
	}
}

Rc<AppWindow> ContextController::makeAppWindow(NotNull<AppThread> app, NotNull<NativeWindow> w) {
	return Rc<AppWindow>::create(_context, app, w);
}

void ContextController::initializeComponent(NotNull<ContextComponent> comp) {
	switch (_state) {
	case ContextState::None: break;
	case ContextState::Created: break;
	case ContextState::Started: comp->handleStart(_context); break;
	case ContextState::Active: comp->handleResume(_context); break;
	}
}

Status ContextController::readFromClipboard(Rc<ClipboardRequest> &&req) {
	req->dataCallback(Status::ErrorNotImplemented, BytesView(), StringView());
	return Status::ErrorNotImplemented;
}

Status ContextController::probeClipboard(Rc<ClipboardProbe> &&) {
	return Status::ErrorNotImplemented;
}

Status ContextController::writeToClipboard(Rc<ClipboardData> &&) {
	return Status::ErrorNotImplemented;
}

Rc<ScreenInfo> ContextController::getScreenInfo() const {
	Rc<ScreenInfo> info = Rc<ScreenInfo>::create();

	if (_displayConfigManager) {
		_displayConfigManager->exportScreenInfo(info);
	}

	return info;
}

void ContextController::handleSystemNotification(SystemNotification n) {
	_context->handleSystemNotification(n);
}

void ContextController::handleNetworkStateChanged(NetworkFlags flags) {
	if (flags != _networkFlags) {
		_networkFlags = flags;
		_context->handleNetworkStateChanged(_networkFlags);
	}
}

void ContextController::handleThemeInfoChanged(ThemeInfo &&theme) {
	_themeInfo = sp::move(theme);
	_context->handleThemeInfoChanged(_themeInfo);
}

void ContextController::handleStateChanged(ContextState prevState, ContextState newState) {
	if (prevState == newState) {
		return;
	}

	auto refId = retain();

	switch (newState) {
	case ContextState::None:
		handleContextWillDestroy();
		handleContextDidDestroy();
		break;
	case ContextState::Created:
		if (prevState > newState) {
			handleContextWillStop();
			handleContextDidStop();
		} else {
			// should not happen - context controller should be created in this state
		}
		break;
	case ContextState::Started:
		if (prevState > newState) {
			handleContextWillPause();
			handleContextDidPause();
		} else {
			handleContextWillStart();
			handleContextDidStart();
		}
		break;
	case ContextState::Active:
		handleContextWillResume();
		handleContextDidResume();
		break;
	}

	release(refId);
}

void ContextController::handleContextWillDestroy() {
	if (!_context) {
		return;
	}

	_context->handleWillDestroy();
	poll();
}

void ContextController::handleContextDidDestroy() {
	if (!_context) {
		return;
	}

	_state = ContextState::None;
	_networkFlags = NetworkFlags::None;
	_contextInfo = nullptr;
	_windowInfo = nullptr;
	_instanceInfo = nullptr;
	_loopInfo = nullptr;
	_context->handleDidDestroy();
	poll();
	if (_looper) {
		_looper->wakeup(event::WakeupFlags::Graceful);
	}
	_context = nullptr;
	_looper = nullptr;
}

void ContextController::handleContextWillStop() {
	if (!_context) {
		return;
	}

	_context->handleWillStop();
	poll();
}

void ContextController::handleContextDidStop() {
	if (!_context) {
		return;
	}

	_state = ContextState::Created;
	_context->handleDidStop();
	poll();
}

void ContextController::handleContextWillPause() {
	if (!_context) {
		return;
	}

	_context->handleWillPause();
	poll();
}

void ContextController::handleContextDidPause() {
	if (!_context) {
		return;
	}

	_state = ContextState::Started;
	_context->handleDidPause();
	poll();
}

void ContextController::handleContextWillResume() {
	if (!_context) {
		return;
	}

	_context->handleWillResume();
}

void ContextController::handleContextDidResume() {
	if (!_context) {
		return;
	}

	_state = ContextState::Active;
	_context->handleDidResume();

	// repeat state notifications if they were missed in paused mode
	_context->handleNetworkStateChanged(_networkFlags);
	_context->handleThemeInfoChanged(_themeInfo);
}

void ContextController::handleContextWillStart() {
	if (!_context) {
		return;
	}

	_context->handleWillStart();
}
void ContextController::handleContextDidStart() {
	if (!_context) {
		return;
	}

	_state = ContextState::Started;
	_context->handleDidStart();
}

void ContextController::handleAllWindowsClosed() {
	if (_context
			&& hasFlag(_context->getInfo()->flags, ContextFlags::DestroyWhenAllWindowsClosed)) {
		if (_displayConfigManager && _displayConfigManager->hasSavedMode()) {
			_displayConfigManager->restoreMode([this](Status) {
				_looper->performOnThread([this] {
					_displayConfigManager->invalidate();
					destroy();
				}, this);
			}, this);
		} else {
			_looper->performOnThread([this] {
				if (_displayConfigManager) {
					_displayConfigManager->invalidate();
				}
				destroy();
			}, this);
		}
	}
}

bool ContextController::start() {
	switch (_state) {
	case ContextState::Created:
		handleStateChanged(_state, ContextState::Started);
		return true;
		break;
	case ContextState::None:
	case ContextState::Started:
	case ContextState::Active: break;
	}
	return false;
}

bool ContextController::resume() {
	switch (_state) {
	case ContextState::Created:
		if (start()) {
			handleStateChanged(_state, ContextState::Active);
			return true;
		}
		break;
	case ContextState::Started:
		handleStateChanged(_state, ContextState::Active);
		return true;
		break;
	case ContextState::None:
	case ContextState::Active: break;
	}
	return false;
}

bool ContextController::pause() {
	switch (_state) {
	case ContextState::Active:
		handleStateChanged(_state, ContextState::Started);
		return true;
		break;
	case ContextState::None:
	case ContextState::Started:
	case ContextState::Created: break;
	}
	return false;
}

bool ContextController::stop() {
	switch (_state) {
	case ContextState::Started:
		handleStateChanged(_state, ContextState::Created);
		return true;
		break;
	case ContextState::Active:
		if (pause()) {
			handleStateChanged(_state, ContextState::Created);
			return true;
		}
		break;
	case ContextState::None:
	case ContextState::Created: break;
	}
	return false;
}

bool ContextController::destroy() {
	if (_state == ContextState::None) {
		return false;
	}

	switch (_state) {
	case ContextState::Active:
		if (pause() && stop()) {
			handleStateChanged(_state, ContextState::None);
		}
		break;
	case ContextState::Started:
		if (stop()) {
			handleStateChanged(_state, ContextState::None);
		}
		break;
	case ContextState::Created: handleStateChanged(_state, ContextState::None); break;
	case ContextState::None: break;
	}

	return true;
}

Value ContextController::saveContextState() { return _context->saveState(); }

Rc<core::Loop> ContextController::makeLoop(NotNull<core::Instance> instance) {
#if MODULE_XENOLITH_BACKEND_VK
	if (instance->getApi() == core::InstanceApi::Vulkan) {
		if (!_loopInfo->backend) {
			auto isHeadless = hasFlag(_context->getInfo()->flags, ContextFlags::Headless);
			auto data = Rc<vk::LoopBackendInfo>::alloc();
			data->deviceSupportCallback = [isHeadless](const vk::DeviceInfo &dev) {
				if (isHeadless) {
					return true;
				} else {
					return dev.supportsPresentation()
							&& std::find(dev.availableExtensions.begin(),
									   dev.availableExtensions.end(),
									   String(VK_KHR_SWAPCHAIN_EXTENSION_NAME))
							!= dev.availableExtensions.end();
				}
			};
			data->deviceExtensionsCallback =
					[isHeadless](const vk::DeviceInfo &dev) -> Vector<StringView> {
				Vector<StringView> ret;
				if (!isHeadless) {
					ret.emplace_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
				}
				return ret;
			};
			_loopInfo->backend = data;
		}
	}
#endif

	auto loop = instance->makeLoop(_looper, move(_loopInfo));
	_loopInfo = nullptr;
	return loop;
}

void ContextController::poll() {
	if (_looper) {
		_looper->poll();
	}
}

void ContextController::notifyPendingWindows() {
	for (auto &it : _activeWindows) { it->dispatchPendingEvents(); }

	auto tmpResized = sp::move(_resizedWindows);
	_resizedWindows.clear();

	for (auto &it : tmpResized) {
		ContextController::notifyWindowConstraintsChanged(it.first, it.second);
	}

	auto tmpClosed = sp::move(_closedWindows);
	_closedWindows.clear();

	for (auto &it : tmpClosed) {
		// Close windows
		if (hasFlag(it.first->getInfo()->state, WindowState::CloseGuard)) {
			ContextController::notifyWindowClosed(it.first, it.second);
		} else {
			it.first->close();
		}
	}
}

} // namespace stappler::xenolith::platform
