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

#include "SPRTWinHeadlessController.h"

#if __SPRT_RUNTIME_CONFIG_HAVE_WINDOW

#include "SPRTWinHeadlessWindow.h"
#include <sprt/runtime/window/context.h>
#include <sprt/runtime/window/display_config.h> // complete type for Rc<DisplayConfigManager> member
#include <sprt/runtime/platform.h> // getAppConfig
#include <sprt/runtime/log.h>

namespace sprt::window {

Rc<HeadlessContextController> HeadlessContextController::create(NotNull<Context> ctx,
		ContextConfig &&cfg, NotNull<dispatch::Looper> looper) {
	return Rc<HeadlessContextController>::create(ctx, sprt::move(cfg), looper);
}

void HeadlessContextController::acquireDefaultConfig(ContextConfig &config, NativeContextHandle *) {
	if (config.instance && config.instance->api == gapi::InstanceApi::None) {
		// Vulkan and Software both implement a pseudo-swapchain and work here; WebGPU and Metal
		// still require a real surface. Vulkan stays the default because it is the backend every
		// build enables - an explicit `--gapi soft` is honoured, since this only fills in None.
		config.instance->api = gapi::InstanceApi::Vulkan;
	}

	if (!config.context) {
		config.context = Rc<ContextInfo>::alloc();
	}

	// Nothing outlives the window here: when the pseudo-window is closed (by the `quit` command or
	// by the application itself) the process must return from Looper::run and exit.
	config.context->flags |= ContextFlags::Headless | ContextFlags::DestroyWhenAllWindowsClosed;

	auto &cfg = getAppConfig();
	if (!cfg.bundleName.empty()) {
		config.context->bundleName = cfg.bundleName.str<String>();
	}
	if (!cfg.appName.empty()) {
		config.context->appName = cfg.appName.str<String>();
	}

	if (config.loop) {
		config.loop->defaultFormat = ImageFormat::B8G8R8A8_UNORM;
	}

	if (config.window && config.window->imageFormat == ImageFormat::Undefined) {
		config.window->imageFormat = ImageFormat::B8G8R8A8_UNORM;
	}
}

HeadlessContextController::~HeadlessContextController() { }

bool HeadlessContextController::init(NotNull<Context> ctx, ContextConfig &&config,
		NotNull<dispatch::Looper> looper) {
	if (!ContextController::init(ctx, looper)) {
		return false;
	}

	_contextInfo = sprt::move(config.context);
	_windowInfo = sprt::move(config.window);
	_instanceInfo = sprt::move(config.instance);
	_loopInfo = sprt::move(config.loop);

	if (!_contextInfo) {
		_contextInfo = Rc<ContextInfo>::alloc();
	}
	if (!_windowInfo) {
		_windowInfo = Rc<WindowInfo>::alloc();
	}

	// The flag drives device selection in Context::makeLoop (no VK_KHR_swapchain, no presentation
	// support requirement). acquireDefaultConfig normally sets it, but a controller constructed
	// straight from a hand-built config may have missed that path.
	_contextInfo->flags |= ContextFlags::Headless;

	/* THE DECORATION METRICS, because WindowCapabilities::UserSpaceDecorations is advertised and
	the eight resize edges are laid out from these numbers.

	`resizeInset` is the whole of it, and 6pt is the Windows figure rather than the Linux one on
	purpose. WindowDecorations hangs each resize bar OUTSIDE the content box, `inset` back in - so
	with an inset of zero every one of them lands outside the surface and cannot be pressed at all.
	That works on X11 because the surface is inset within a larger window whose margin is the
	shadow; here, as on Win32, the surface IS the window, so the grips have to be inside it.

	`borderRadius` and `shadowWidth` stay ZERO, which is not an omission: a pseudo-window has
	nothing behind it and no compositor to blend against, so a rounded corner would cut pixels out
	of the captured frame with nothing to show through, and a shadow would be drawn onto the
	application's own ground. A headless frame is therefore the frame a squared-off desktop window
	presents - which is also what keeps a capture comparable with the windowed one. */
	_themeInfo.decorations.resizeInset = 6.0f;

	return true;
}

bool HeadlessContextController::hasPointerDevice() const {
	// Through the live Context, not through _contextInfo: that member is a staging slot the
	// controller MOVES into the Context at handleConfigurationChanged and then nulls, so by the
	// time any window is created it is empty and reading it here silently answers the default.
	// It is still the only source before the handover, hence both.
	auto info = _context ? _context->getInfo() : _contextInfo.get();
	return !info || !hasFlag(info->flags, ContextFlags::HeadlessNoPointer);
}

WindowCapabilities HeadlessContextController::getCapabilities() const {
	// What this controller really provides. Everything else on the list needs a window system:
	// server-side decorations, cursors, fullscreen and mode switching, an OS icon, native dialogs.
	//
	// Subwindows is not a courtesy bit: createWindow really does build an auxiliary window with its
	// own pseudo-swapchain, so ui::SubWindow takes the native path here and a headless run
	// exercises the same code a desktop one does.
	// WindowPosition: the pseudo-window owns its own geometry outright, so the position a caller
	// asks for is the position it gets - which is what makes a save/restore round trip testable
	// with no window system in play.
	// UserSpaceDecorations is not one either, and it is the cheapest of the three to be honest
	// about: the capability says the window system draws NO frame and the application draws its
	// own, and that is what a pseudo-window is by construction - there is no title bar, no border
	// and no shadow anywhere but in the application's own surface. So the flag survives
	// Context::configureWindow, SceneContent builds the decorations node, and a headless frame is
	// the frame that application draws for itself. The other half - what a press on one of those
	// grips DOES - is HeadlessWindow's; see the note there, and see init() for the one theme value
	// that decides whether the grips can be reached at all.
	return WindowCapabilities::Subwindows | WindowCapabilities::WindowPosition
			| WindowCapabilities::UserSpaceDecorations;
}

void HeadlessContextController::openUrl(StringView url) {
	// There is no desktop session to hand a URL to.
	oslog::vpwarn(__SPRT_LOCATION, "HeadlessContextController", "openUrl is not available");
}

Status HeadlessContextController::readFromClipboard(Rc<ClipboardRequest> &&req) {
	if (!_clipboard) {
		req->dataCallback(Status::ErrorNotFound, BytesView(), StringView());
		return Status::Ok;
	}

	Vector<StringView> types;
	for (auto &it : _clipboard->types) { types.emplace_back(it); }

	auto type = req->typeCallback(types);
	if (type.empty()) {
		// The requester wants none of what is on offer; that is a normal outcome, not a failure
		req->dataCallback(Status::Declined, BytesView(), StringView());
		return Status::Ok;
	}

	auto data = _clipboard->encodeCallback(type);
	req->dataCallback(Status::Ok, data, type);
	return Status::Ok;
}

Status HeadlessContextController::probeClipboard(Rc<ClipboardProbe> &&probe) {
	if (!_clipboard) {
		probe->typeCallback(Status::ErrorNotFound, SpanView<StringView>());
		return Status::Ok;
	}

	Vector<StringView> types;
	for (auto &it : _clipboard->types) { types.emplace_back(it); }

	probe->typeCallback(Status::Ok, types);
	return Status::Ok;
}

Status HeadlessContextController::writeToClipboard(Rc<ClipboardData> &&data) {
	// Holding the Rc is what keeps the encode callback (and whatever it captured) alive for as long
	// as this data is the clipboard's, which is the contract every backend follows
	_clipboard = sprt::move(data);
	return Status::Ok;
}

void HeadlessContextController::notifyWindowInputEvents(NotNull<NativeWindow> w,
		Vector<InputEventData> &&ev) {
	bool pointer = false;
	bool pressed = false;
	for (auto &it : ev) {
		switch (it.event) {
		case InputEventName::MouseMove:
		case InputEventName::Scroll:
		case InputEventName::Move: pointer = true; break;
		case InputEventName::Begin:
			pointer = true;
			pressed = true;
			break;
		default: break;
		}
	}

	// An event was injected into this window, which is what "the pointer is over it" means when
	// there is no pointer to move.
	if (pointer) {
		setPointerWindow(w);
	}
	if (pressed) {
		// Click-to-focus, before the press is delivered - the order a window manager uses, and the
		// order the base class's popup dismissal below expects. Clicking a menu row is not a focus
		// change: setFocusedWindow refuses Popup and Tooltip outright.
		setFocusedWindow(w);
	}

	ContextController::notifyWindowInputEvents(w, sprt::move(ev));
}

void HeadlessContextController::notifyWindowDeallocated(NotNull<NativeWindow> w) {
	// unmapWindow normally clears these, but a window that was never mapped never went through it.
	if (_focusedWindow == w) {
		_focusedWindow = nullptr;
	}
	if (_pointerWindow == w) {
		_pointerWindow = nullptr;
	}
	for (auto it = _stack.begin(); it != _stack.end(); ++it) {
		if (*it == w) {
			_stack.erase(it);
			break;
		}
	}

	ContextController::notifyWindowDeallocated(w);
}

IRect HeadlessContextController::getVirtualScreenRect() const {
	IRect ret;
	bool hasRoot = false;
	for (auto *w : _allWindows) {
		auto wi = w->getInfo();
		if (!wi || wi->type != WindowType::Root) {
			continue;
		}
		if (!hasRoot) {
			ret = wi->rect;
			hasRoot = true;
			continue;
		}

		const auto x0 = sprt::min(ret.x, wi->rect.x);
		const auto y0 = sprt::min(ret.y, wi->rect.y);
		const auto x1 = sprt::max(ret.x + int32_t(ret.width), wi->rect.x + int32_t(wi->rect.width));
		const auto y1 =
				sprt::max(ret.y + int32_t(ret.height), wi->rect.y + int32_t(wi->rect.height));
		ret = IRect(x0, y0, uint32_t(x1 - x0), uint32_t(y1 - y0));
	}

	if (!hasRoot) {
		// Asked before the root window exists - the configured geometry is the best answer there
		// is.
		ret = _windowInfo ? _windowInfo->rect : IRect(0, 0, 1'024, 768);
	}
	return ret;
}

void HeadlessContextController::raiseWindow(NativeWindow *w) {
	for (auto it = _stack.begin(); it != _stack.end(); ++it) {
		if (*it == w) {
			_stack.erase(it);
			break;
		}
	}
	_stack.emplace_back(w);
}

NativeWindow *HeadlessContextController::getTopmostFocusable(NativeWindow *except) const {
	for (auto it = _stack.rbegin(); it != _stack.rend(); ++it) {
		auto *wi = (*it)->getInfo();
		if (*it != except && wi && wi->type != WindowType::Popup
				&& wi->type != WindowType::Tooltip) {
			return *it;
		}
	}
	return nullptr;
}

void HeadlessContextController::setFocusedWindow(NativeWindow *w) {
	if (w) {
		auto *wi = w->getInfo();
		if (!wi || wi->type == WindowType::Popup || wi->type == WindowType::Tooltip) {
			// Neither is ever the key window: a menu is an override-redirect surface on X11 and a
			// WS_EX_NOACTIVATE popup on Win32, and a tip takes no input at all. The window a menu
			// hangs off keeps focus for as long as the menu is up - which is also what stops the
			// menu from dismissing itself through notifyWindowFocusLost the moment it opens.
			return;
		}
	}

	if (_focusedWindow == w) {
		return;
	}

	auto *prev = _focusedWindow;

	// Assigned before either notification: updateState dispatches the WindowState event straight
	// back into this controller, and re-entry must see the outcome, not the transition.
	_focusedWindow = w;

	if (prev) {
		static_cast<HeadlessWindow *>(prev)->updateFocusState(false);
	}
	if (w) {
		raiseWindow(w);
		static_cast<HeadlessWindow *>(w)->updateFocusState(true);
	}
}

void HeadlessContextController::setPointerWindow(NativeWindow *w) {
	if (_pointerWindow == w) {
		return;
	}

	auto *prev = _pointerWindow;
	_pointerWindow = w;

	if (prev) {
		static_cast<HeadlessWindow *>(prev)->updatePointerState(false);
	}
	if (w) {
		static_cast<HeadlessWindow *>(w)->updatePointerState(true);
	}
}

void HeadlessContextController::handleWindowMapped(NotNull<HeadlessWindow> w) {
	raiseWindow(w);

	auto *info = w->getInfo();

	// A newly mapped window comes up focused, the way a window manager maps one - except a palette,
	// which must not take activation away from the window it belongs to. Popups and tips are
	// refused by setFocusedWindow outright.
	if (info && info->type != WindowType::Utility) {
		setFocusedWindow(w);
	}

	// Nothing is tracking a pointer position here, so the first window on screen simply gets it;
	// after that it moves only where input is actually injected.
	if (!_pointerWindow) {
		setPointerWindow(w);
	}
}

void HeadlessContextController::handleWindowUnmapped(NotNull<HeadlessWindow> w) {
	for (auto it = _stack.begin(); it != _stack.end(); ++it) {
		if (*it == w) {
			_stack.erase(it);
			break;
		}
	}

	// Focus and pointer go back where the window system would put them: to the window that owns
	// this one, if it is still on screen, and to whatever is topmost otherwise.
	NativeWindow *next = nullptr;
	if (_focusedWindow == w || _pointerWindow == w) {
		auto *info = w->getInfo();
		next = info && !info->parent.empty() ? findWindow(info->parent) : nullptr;
		if (!next || !next->isMapped()) {
			next = getTopmostFocusable(w);
		}
	}

	if (_focusedWindow == w) {
		_focusedWindow = nullptr;
		w->updateFocusState(false);
		setFocusedWindow(next);
	}
	if (_pointerWindow == w) {
		_pointerWindow = nullptr;
		w->updatePointerState(false);
		setPointerWindow(next);
	}
}

bool HeadlessContextController::loadWindow(Rc<WindowInfo> &&wInfo) {
	auto window = Rc<HeadlessWindow>::create(this, sprt::move(wInfo));
	if (!window) {
		return false;
	}

	notifyWindowCreated(window);
	return true;
}

int HeadlessContextController::run(NotNull<ContextContainer> container) {
	_context->handleConfigurationChanged(sprt::move(_contextInfo));
	_contextInfo = nullptr;

	_looper->performOnThread([this] {
		auto instance = _context->makeInstance(_instanceInfo);
		if (!instance) {
			oslog::vperror(__SPRT_LOCATION, "HeadlessContextController",
					"Fail to load graphics instance");
			_resultCode = -1;
			destroy();
			return;
		}

		auto loop = _context->makeLoop(instance, _loopInfo);
		if (!loop) {
			oslog::vperror(__SPRT_LOCATION, "HeadlessContextController",
					"Fail to load device loop");
			_resultCode = -1;
			destroy();
			return;
		}

		_context->handleGraphicsLoaded(loop);

		// Drives the context lifecycle Created -> Active, which is what creates and runs the app
		// thread. It MUST happen before the window is created, or makeAppWindow dereferences a
		// null _application.
		if (!resume()) {
			oslog::vperror(__SPRT_LOCATION, "HeadlessContextController", "Fail to resume Context");
			_resultCode = -1;
			destroy();
			return;
		}

		createWindow(sprt::move(_windowInfo));
	}, nullptr);

	_looper->run();

	return ContextController::run(container);
}

} // namespace sprt::window

#endif // __SPRT_RUNTIME_CONFIG_HAVE_WINDOW
