/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

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

#include <sprt/c/__sprt_stdlib.h> // getenv for the XENOLITH_HEADLESS override

#if __SPRT_RUNTIME_CONFIG_HAVE_WINDOW

#if SPRT_LINUX
#include "../linux/SPRTWinLinuxController.h"
#endif

#if SPRT_ANDROID
#include "../android/SPRTWinAndroidContextController.h"
#endif

#if SPRT_WINDOWS
#include "../windows/SPRTWinWindowsContextController.h"
#endif

#if SPRT_MACOS
#include "../macos/SPRTWinMacosContextController.h"
#endif

#if SPRT_IOS
#include "../ios/SPRTWinIosContextController.h"
#endif

#if SPRT_WASM
#include "../wasm/SPRTWinWasmController.h"
#endif

#if SPRT_NUTTX
#include "../nuttx/SPRTWinNuttxController.h"
#endif

#if SPRT_EMBOX
#include "../embox/SPRTWinEmboxController.h"
#endif

#include "../headless/SPRTWinHeadlessController.h"

namespace sprt::window {

namespace {

// The only controller selected at runtime rather than by platform: headless has to be able to
// replace the native controller on any target.
bool isHeadlessRequested(const ContextConfig &config) {
	if (config.context && hasFlag(config.context->flags, ContextFlags::Headless)) {
		return true;
	}
	if (auto env = __sprt_getenv("XENOLITH_HEADLESS")) {
		auto value = StringView(env);
		return value == "1" || value == "true" || value == "yes";
	}
	return false;
}

} // namespace

Rc<ContextController> ContextController::create(NotNull<Context> ctx, ContextConfig &&info,
		NotNull<dispatch::Looper> a) {
	if (isHeadlessRequested(info)) {
		return HeadlessContextController::create(ctx, move(info), a);
	}
#if SPRT_LINUX
	return LinuxContextController::create(ctx, move(info), a);
#endif
#if SPRT_MACOS
	return MacosContextController::create(ctx, move(info), a);
#endif
#if SPRT_IOS
	return IosContextController::create(ctx, move(info), a);
#endif
#if SPRT_WINDOWS
	return WindowsContextController::create(ctx, move(info), a);
#endif
#if SPRT_ANDROID
	return AndroidContextController::create(ctx, move(info), a);
#endif
#if SPRT_WASM
	return WasmContextController::create(ctx, move(info), a);
#endif
#if SPRT_NUTTX
	return NuttxContextController::create(ctx, move(info), a);
#endif
#if SPRT_EMBOX
	return EmboxContextController::create(ctx, move(info), a);
#endif
	oslog::vperror(__SPRT_LOCATION, "ContextController", "Unknown platform");
	return nullptr;
}

void ContextController::acquireDefaultConfig(ContextConfig &config, NativeContextHandle *handle) {
	// Runs after the command line was parsed (see ContextConfig(argc, argv)), so --headless is
	// already visible here and the native defaults must not be applied on top of it.
	if (isHeadlessRequested(config)) {
		HeadlessContextController::acquireDefaultConfig(config, handle);
		return;
	}
#if SPRT_LINUX
	LinuxContextController::acquireDefaultConfig(config, handle);
#endif
#if SPRT_MACOS
	MacosContextController::acquireDefaultConfig(config, handle);
#endif
#if SPRT_IOS
	IosContextController::acquireDefaultConfig(config, handle);
#endif
#if SPRT_WINDOWS
	WindowsContextController::acquireDefaultConfig(config, handle);
#endif
#if SPRT_ANDROID
	AndroidContextController::acquireDefaultConfig(config);
#endif
#if SPRT_NUTTX
	NuttxContextController::acquireDefaultConfig(config, handle);
#endif
#if SPRT_EMBOX
	EmboxContextController::acquireDefaultConfig(config, handle);
#endif
}

bool ContextController::init(NotNull<Context> ctx, NotNull<dispatch::Looper> a) {
	_context = ctx;
	_looper = a;
	return true;
}

int ContextController::run(NotNull<ContextContainer> с) { return _resultCode; }

void ContextController::retainPollDepth() { ++_pollDepth; }
void ContextController::releasePollDepth() {
	if (_pollDepth == 0) {
		return;
	}
	if (_pollDepth-- == 1) {
		notifyPendingWindows();
	}
}

bool ContextController::configureWindow(NotNull<WindowInfo> w) {
	if (!_context->configureWindow(w)) {
		return false;
	}

	// Normalize immutable size constraints and clamp the initial size into them, so the window
	// is born within bounds even before the WM/OS enforces them on first map/resize.
	if (w->maxExtent.width != 0 && w->minExtent.width != 0
			&& w->maxExtent.width < w->minExtent.width) {
		oslog::vperror(__SPRT_LOCATION, "ContextController",
				"WindowInfo::maxExtent.width is below minExtent.width, raising to minExtent");
		w->maxExtent.width = w->minExtent.width;
	}
	if (w->maxExtent.height != 0 && w->minExtent.height != 0
			&& w->maxExtent.height < w->minExtent.height) {
		oslog::vperror(__SPRT_LOCATION, "ContextController",
				"WindowInfo::maxExtent.height is below minExtent.height, raising to minExtent");
		w->maxExtent.height = w->minExtent.height;
	}

	auto clamped =
			clampWindowExtent(Extent2(w->rect.width, w->rect.height), w->minExtent, w->maxExtent);
	w->rect.width = clamped.width;
	w->rect.height = clamped.height;

	// `id` keys Director preservation and per-window caches - it should be unique
	// among live windows
	if (w->id.empty()) {
		w->id = StringView("window").str<String>();
	}
	if (findWindow(w->id)) {
		auto base = w->id;
		uint32_t counter = 1;
		do {
			w->id = StreamTraits<char>::toString<String>(StringView(base), "-", counter++);
		} while (findWindow(w->id));
	}
	return true;
}

NativeWindow *ContextController::findWindow(StringView id) const {
	for (auto &it : _allWindows) {
		if (it->getInfo() && it->getInfo()->id == id) {
			return it;
		}
	}
	return nullptr;
}

Status ContextController::createWindow(Rc<WindowInfo> &&info) {
	if (!info) {
		return Status::ErrorInvalidArguemnt;
	}

	emitWindowDiag(StreamTraits<char>::toString<String>(
			"createWindow type=", getWindowTypeName(info->type), " id='", info->id, "' parent='",
			info->parent, "' rect=", info->rect.width, "x", info->rect.height));

	if (info->type != WindowType::Root) {
		if (!hasFlag(getCapabilities(), WindowCapabilities::Subwindows)) {
			return Status::ErrorNotSupported;
		}
		if (info->parent.empty() || !findWindow(info->parent)) {
			oslog::vperror(__SPRT_LOCATION, "ContextController",
					"Parent window is not defined or not found for a non-Root window");
			return Status::ErrorInvalidArguemnt;
		}
	}

	// One live window per (parent, type): auxiliary windows are never pooled, so re-opening the
	// same slot closes the old one instead of stacking a second surface on it.
	if (info->type == WindowType::Popup || info->type == WindowType::Tooltip) {
		closeWindows("same-slot", [&](const WindowInfo &wi) {
			return wi.parent == info->parent && wi.type == info->type;
		});
	}

	if (!configureWindow(info)) {
		return Status::ErrorInvalidArguemnt;
	}

	if (!loadWindow(move(info))) {
		emitWindowDiag("loadWindow failed");
		return Status::ErrorNotSupported;
	}
	return Status::Ok;
}

uint32_t ContextController::closeWindows(StringView reason,
		const callback<bool(const WindowInfo &)> &pred) {
	// A close request cascades into children, so a list collected up front can hold freed pointers
	// by the time we reach them. Re-scan after every request; `requested` keeps a window whose
	// close is deferred (queued for the end of the poll) or refused from spinning the loop.
	Set<NativeWindow *> requested;
	uint32_t closed = 0;
	for (;;) {
		NativeWindow *target = nullptr;
		for (auto *w : _allWindows) {
			auto wi = w->getInfo();
			if (wi && requested.find(w) == requested.end() && pred(*wi)) {
				target = w;
				break;
			}
		}
		if (!target) {
			break;
		}
		++closed;
		emitWindowDiag(StreamTraits<char>::toString<String>("close id='", target->getInfo()->id,
				"' reason=", reason));
		requested.emplace(target);
		target->close();
	}
	return closed;
}

void ContextController::emitWindowDiag(StringView line) const {
	// One destination only: when the app layer installs a sink it forwards to its own log, and
	// writing to both duplicates every line.
	if (_windowDiagSink) {
		_windowDiagSink(line);
	} else {
		oslog::vpdebug(__SPRT_LOCATION, "WindowDiag", line);
	}
}

void ContextController::dismissPopupChain(NotNull<NativeWindow> w) {
	auto *info = w->getInfo();
	if (!info || (info->type != WindowType::Popup && info->type != WindowType::Tooltip)) {
		return;
	}

	// Climb to the outermost auxiliary window: its close cascades down the whole chain.
	NativeWindow *top = w;
	for (;;) {
		auto *ti = top->getInfo();
		if (!ti || ti->parent.empty()) {
			break;
		}
		auto *parent = findWindow(ti->parent);
		auto *pi = parent ? parent->getInfo() : nullptr;
		if (!pi || (pi->type != WindowType::Popup && pi->type != WindowType::Tooltip)) {
			break;
		}
		top = parent;
	}

	const auto topId = top->getInfo()->id;
	closeWindows("popup-chain-dismissed", [&](const WindowInfo &wi) { return wi.id == topId; });
}

void ContextController::dismissChildPopups(NotNull<NativeWindow> parent, StringView reason) {
	auto *info = parent->getInfo();
	if (!info || info->id.empty()) {
		return;
	}
	const auto parentId = info->id;
	closeWindows(reason, [&](const WindowInfo &wi) {
		return wi.parent == parentId
				&& (wi.type == WindowType::Popup || wi.type == WindowType::Tooltip);
	});
}

void ContextController::notifyWindowFocusLost(NotNull<NativeWindow> w) {
	auto *info = w->getInfo();
	if (!info || _focusDismissScheduled) {
		return;
	}
	// Nothing to dismiss unless a menu is actually up.
	bool anyPopup = false;
	for (auto *win : _allWindows) {
		auto *wi = win->getInfo();
		if (wi && (wi->type == WindowType::Popup || wi->type == WindowType::Tooltip)) {
			anyPopup = true;
			break;
		}
	}
	if (!anyPopup) {
		return;
	}

	_focusDismissScheduled = true;
	_looper->performOnThread([this] {
		_focusDismissScheduled = false;
		// Focus that moved to another window of ours (the popup taking a Wayland grab, or the
		// user switching between our own windows) is not a dismiss.
		for (auto *win : _allWindows) {
			auto *wi = win->getInfo();
			if (wi && hasFlag(wi->state, WindowState::Focused)) {
				return;
			}
		}
		closeWindows("focus-lost", [](const WindowInfo &wi) {
			return wi.type == WindowType::Popup || wi.type == WindowType::Tooltip;
		});
	}, this);
}

void ContextController::notifyWindowCreated(NotNull<NativeWindow> w) {
	_context->handleNativeWindowCreated(w);
	_activeWindows.emplace(w);

	// A modal Dialog blocks its parent through the very same counter the system dialogs use: the
	// OS-side hints (xdg_dialog_v1, _NET_WM_STATE_MODAL, EnableWindow, a macOS child window) are
	// advisory, and the engine is what actually stops input from reaching the parent.
	if (auto info = w->getInfo()) {
		if (info->type == WindowType::Dialog && hasFlag(info->flags, WindowCreationFlags::Modal)) {
			if (auto parent = findWindow(info->parent)) {
				retainModalBlock(parent);
			}
		}
	}
}

void ContextController::notifyWindowConstraintsChanged(NotNull<NativeWindow> w,
		UpdateConstraintsFlags flags) {
	if (isWithinPoll()) {
		_resizedWindows.emplace_back(w, flags);
	} else {
		_context->handleNativeWindowConstraintsChanged(w, flags);
	}
}
void ContextController::notifyWindowGeometryChanged(NotNull<NativeWindow> w) {
	// Straight through, unlike the constraints path: there is nothing to coalesce for the poll loop
	// (the context compares the snapshot against the app-thread mirror and drops a no-op) and
	// nothing here that must not run inside a poll.
	_context->handleNativeWindowGeometryChanged(w);
}

void ContextController::notifyWindowInputEvents(NotNull<NativeWindow> w,
		Vector<InputEventData> &&ev) {
	// An xdg_popup grab is owner-relative: the compositor dismisses the menu only for input that
	// lands outside the client, and hands input on our own surfaces straight to us. Closing the
	// menu when the press belongs to another window of ours is therefore the application's job —
	// on every backend, so it lives here rather than in one of them.
	//
	// A press on a window closes the auxiliary windows *owned by it*: on the Root that is the
	// whole menu, on a menu level it is the submenus below it, which is exactly how a press in a
	// parent menu is expected to behave. The press that opens a menu is delivered before that
	// menu exists, so it has nothing to dismiss.
	bool pressed = false;
	for (auto &it : ev) {
		if (it.event == InputEventName::Begin) {
			pressed = true;
			break;
		}
	}
	if (pressed) {
		dismissChildPopups(w, "owner-pressed");
	}

	if (isModalBlocked(w)) {
		// A modal dialog owns this window's input. Drop pointer and key events, but let
		// WindowState through: the application still has to learn that it was resized, minimized
		// or unfocused while the dialog was up.
		auto out = ev.begin();
		for (auto &it : ev) {
			if (it.event == InputEventName::WindowState) {
				*out++ = sprt::move(it);
			}
		}
		ev.erase(out, ev.end());

		// Poking a blocked window should surface the dialog. Where the OS gives the dialog a real
		// parent it has already done this and the backends no-op.
		if (pressed) {
			raiseWindowDialogs(w);
		}
		if (ev.empty()) {
			return;
		}
	} else {
		trackHeldInput(w, ev);
	}

	_context->handleNativeWindowInputEvents(w, sprt::move(ev));
}

void ContextController::notifyWindowTextInput(NotNull<NativeWindow> w,
		const TextInputState &state) {
	_context->handleNativeWindowTextInput(w, state);
}

bool ContextController::notifyWindowClosed(NotNull<NativeWindow> w, WindowCloseOptions opts) {
	auto info = w->getInfo();

	// Cascade first, so the WM sees the whole tree come down even if the parent itself was torn
	// down off-thread. Closing a child unwinds its own subtree through this same function.
	if (info && !info->id.empty()) {
		const auto parentId = info->id;
		closeWindows("parent-closing", [&](const WindowInfo &wi) { return wi.parent == parentId; });
	}

	if (!hasFlag(opts, WindowCloseOptions::IgnoreExitGuard)
			&& hasFlag(w->getInfo()->state, WindowState::CloseGuard)) {
		// The application owns the decision (an "unsaved changes" prompt and the like).
		return false;
	}

	if (isWithinPoll()) {
		// Retiring a window in the middle of an event dispatch would free structures the platform
		// loop is still walking, so hand it to the end of the iteration. The Rc keeps it alive
		// until then even if this was the last reference.
		_closedWindows.emplace_back(Rc<NativeWindow>(w), opts);
		return true;
	}

	if (hasFlag(opts, WindowCloseOptions::CloseInPlace)) {
		performWindowTeardown(w);
	}
	return true;
}

void ContextController::performWindowTeardown(NotNull<NativeWindow> w) {
	auto it = _activeWindows.find(w.get());
	if (it == _activeWindows.end()) {
		// Already retired — the cascade re-enters this for windows a parent has taken down.
		return;
	}

	// Erase first so re-entry through handleNativeWindowDestroyed (which closes the AppWindow,
	// which asks the native window to close again) terminates instead of recursing. The local Rc
	// keeps the window alive for the rest of this function.
	Rc<NativeWindow> hold = *it;
	_activeWindows.erase(it);

	// Symmetric to notifyWindowCreated: releasing here rather than in notifyWindowClosed means a
	// window whose close is refused or deferred keeps blocking its parent, which is correct — it is
	// still on screen.
	if (auto info = hold->getInfo()) {
		if (info->type == WindowType::Dialog && hasFlag(info->flags, WindowCreationFlags::Modal)) {
			if (auto parent = findWindow(info->parent)) {
				releaseModalBlock(parent);
			}
		}
	}

	// Before unmapping: a dialog parented to this window must not outlive it, and its backend
	// still needs the native parent handle (HWND / xid / NSWindow) to dismiss itself cleanly.
	// Every pending callback is answered with ErrorCancelled rather than dropped.
	cancelWindowDialogs(hold, Status::ErrorCancelled);

	hold->unmapWindow();
	if (_context) {
		_context->handleNativeWindowDestroyed(hold);
	}
}

void ContextController::notifyWindowAllocated(NotNull<NativeWindow> w) {
	_allWindows.emplace(w); //
}

void ContextController::notifyWindowDeallocated(NotNull<NativeWindow> w) {
	// Belt and braces: performWindowTeardown already drained these, but a window that never
	// became active never went through it, and a stale entry here would outlive the object.
	cancelWindowDialogs(w, Status::ErrorCancelled);
	_heldInput.erase(w.get());

	auto it = _allWindows.find(w);
	if (it != _allWindows.end()) {
		_allWindows.erase(it);

		// The exit trigger is "no Root remains", not "nothing remains": an auxiliary window must
		// neither keep the app alive on its own nor quit it when dismissed.
		bool anyRoot = false;
		for (auto *win : _allWindows) {
			auto wi = win->getInfo();
			if (wi && wi->type == WindowType::Root) {
				anyRoot = true;
				break;
			}
		}
		if (!anyRoot) {
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

	auto refId = sprt::retain(this);

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

	sprt::release(this, refId);
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
	_looper->wakeup(dispatch::WakeupFlags::Graceful);
	_looper = nullptr;
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

	// Losing focus dismisses menus and hints, the same as any native menu does. A Tooltip that has
	// not mapped yet is left alone: tearing it down mid-first-frame strands its swapchain.
	closeWindows("app-deactivated", [this](const WindowInfo &wi) {
		if (wi.type == WindowType::Popup) {
			return true;
		}
		if (wi.type != WindowType::Tooltip) {
			return false;
		}
		auto *w = findWindow(wi.id);
		return w && w->isMapped();
	});

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
		// Re-check the guard: the application may have raised it while the close was queued.
		if (!hasFlag(it.second, WindowCloseOptions::IgnoreExitGuard)
				&& hasFlag(it.first->getInfo()->state, WindowState::CloseGuard)) {
			continue;
		}
		if (hasFlag(it.second, WindowCloseOptions::CloseInPlace)) {
			performWindowTeardown(it.first);
		}
	}
}

} // namespace sprt::window

#endif
