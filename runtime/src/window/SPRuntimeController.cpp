/**
 Copyright (c) 2026 Xenolith Team <admin@stappler.org>

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

#include <sprt/runtime/window/controller.h>
#include <sprt/runtime/window/native_window.h>
#include <sprt/runtime/window/display_config.h>
#include <sprt/runtime/log.h>

#if __SPRT_RUNTIME_CONFIG_HAVE_WINDOW

#if SPRT_LINUX
#include "private/window/linux/SPRTWinLinuxController.h"
#endif

#if SPRT_ANDROID
#include "private/window/android/SPRTWinAndroidContextController.h"
#endif

namespace sprt::window {

Rc<ContextController> ContextController::create(NotNull<Context> ctx, ContextConfig &&info,
		NotNull<LooperAdapter> a) {
#if SPRT_LINUX
	return LinuxContextController::create(ctx, move(info), a);
#endif
#if SPRT_MACOS
	return MacosContextController::create(ctx, move(info));
#endif
#if SPRT_WINDOWS
	return WindowsContextController::create(ctx, move(info));
#endif
#if SPRT_ANDROID
	return AndroidContextController::create(ctx, move(info), a);
#endif
	log::vperror(__SPRT_LOCATION, "ContextController", "Unknown platform");
	return nullptr;
}

void ContextController::acquireDefaultConfig(ContextConfig &config, NativeContextHandle *handle) {
#if SPRT_LINUX
	LinuxContextController::acquireDefaultConfig(config, handle);
#endif
#if SPRT_MACOS
	MacosContextController::acquireDefaultConfig(config, handle);
#endif
#if SPRT_WINDOWS
	WindowsContextController::acquireDefaultConfig(config, handle);
#endif
#if SPRT_ANDROID
	AndroidContextController::acquireDefaultConfig(config);
#endif
}

bool ContextController::init(NotNull<Context> ctx, NotNull<LooperAdapter> a) {
	_context = ctx;
	_looper = a;
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
		UpdateConstraintsFlags flags) {
	if (isWithinPoll()) {
		_resizedWindows.emplace_back(w, flags);
	} else {
		_context->handleNativeWindowConstraintsChanged(w, flags);
	}
}
void ContextController::notifyWindowInputEvents(NotNull<NativeWindow> w,
		memory::dynvector<InputEventData> &&ev) {
	_context->handleNativeWindowInputEvents(w, sprt::move(ev));
}

void ContextController::notifyWindowTextInput(NotNull<NativeWindow> w,
		const TextInputState &state) {
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
	_themeInfo = sprt::move(theme);
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
	_looper->poll();
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
	_looper->poll();
	_looper->wakeup();
	_context = nullptr;
}

void ContextController::handleContextWillStop() {
	if (!_context) {
		return;
	}

	_context->handleWillStop();
	_looper->poll();
}

void ContextController::handleContextDidStop() {
	if (!_context) {
		return;
	}

	_state = ContextState::Created;
	_context->handleDidStop();
	_looper->poll();
}

void ContextController::handleContextWillPause() {
	if (!_context) {
		return;
	}

	_context->handleWillPause();
	_looper->poll();
}

void ContextController::handleContextDidPause() {
	if (!_context) {
		return;
	}

	_state = ContextState::Started;
	_context->handleDidPause();
	_looper->poll();
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

SurfaceSupportInfo ContextController::getSupportInfo() const { return SurfaceSupportInfo(); }

void ContextController::notifyPendingWindows() {
	for (auto &it : _activeWindows) { it->dispatchPendingEvents(); }

	auto tmpResized = sprt::move(_resizedWindows);
	_resizedWindows.clear();

	for (auto &it : tmpResized) {
		ContextController::notifyWindowConstraintsChanged(it.first, it.second);
	}

	auto tmpClosed = sprt::move(_closedWindows);
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

} // namespace sprt::window

#endif
