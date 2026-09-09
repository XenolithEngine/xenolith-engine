/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
SPDX-License-Identifier: MIT
**/
#include "SPRTWinWasmController.h"

#if SPRT_WASM

#include "SPRTWinWasmWindow.h"
#include <sprt/runtime/window/context.h>
#include <sprt/runtime/window/display_config.h> // complete type for Rc<DisplayConfigManager> member
#include <sprt/runtime/dispatch/event.h>
#include <sprt/runtime/dispatch/handle.h>
#include <sprt/runtime/log.h>

namespace sprt::window {

Rc<WasmContextController> WasmContextController::create(NotNull<Context> ctx, ContextConfig &&cfg,
		NotNull<dispatch::Looper> looper) {
	return Rc<WasmContextController>::create(ctx, sprt::move(cfg), looper);
}

WasmContextController::~WasmContextController() {
	if (_pollTimer) {
		_pollTimer->cancel();
		_pollTimer = nullptr;
	}
}

bool WasmContextController::init(NotNull<Context> ctx, ContextConfig &&config,
		NotNull<dispatch::Looper> looper) {
	if (!ContextController::init(ctx, looper)) {
		return false;
	}
	_contextInfo = sprt::move(config.context);
	_windowInfo = sprt::move(config.window);
	_instanceInfo = sprt::move(config.instance);
	_loopInfo = sprt::move(config.loop);

	// The browser graphics API is always WebGPU (navigator.gpu); there is no --gapi to parse
	// from a wasm argv, so pin it here regardless of the parsed/default config.
	if (!_instanceInfo) {
		_instanceInfo = Rc<gapi::InstanceInfo>::alloc();
	}
	_instanceInfo->api = gapi::InstanceApi::WebGPU;
	return true;
}

WindowCapabilities WasmContextController::getCapabilities() const { return WindowCapabilities::None; }

void WasmContextController::openUrl(StringView) { } // TODO: host import -> window.open

void WasmContextController::onHostPoll(WasmContextController *c, dispatch::TimerHandle *,
		uint32_t, Status status) {
	if (!status::isSuccessful(status) || !c) {
		return;
	}
	c->retainPollDepth();
	c->notifyPendingWindows();
	c->releasePollDepth();
}

bool WasmContextController::loadWindow(Rc<WindowInfo> &&wInfo) {
	auto window = Rc<WasmWindow>::create(this, sprt::move(wInfo), getCapabilities());
	if (window) {
		notifyWindowCreated(window);
		_activeWindows.emplace(window);
		return true;
	}
	return false;
}

int WasmContextController::run(NotNull<ContextContainer> container) {
	_context->handleConfigurationChanged(sprt::move(_contextInfo));
	_contextInfo = nullptr;

	_looper->performOnThread([this] {
		auto instance = _context->makeInstance(_instanceInfo);
		if (!instance) {
			oslog::vperror(__SPRT_LOCATION, "WasmContextController", "Fail to load WebGPU instance");
			_resultCode = -1;
			destroy();
			return;
		}
		if (auto loop = _context->makeLoop(instance, _loopInfo)) {
			_context->handleGraphicsLoaded(loop);
		}

		// Drive the context lifecycle Created -> Active: this is what creates and runs the
		// app thread (Context::handleWillStart/handleDidStart). It MUST happen before the
		// window is created, or makeAppWindow dereferences a null _application.
		if (!resume()) {
			oslog::vperror(__SPRT_LOCATION, "WasmContextController", "Fail to resume Context");
			_resultCode = -1;
			destroy();
			return;
		}
		createWindow(sprt::move(_windowInfo));

		// ~8 ms: one frame of resize/input latency, cheap enough that the looper already
		// wakes for presentation. Infinite so it lives for the window's lifetime.
		_pollTimer = _looper->scheduleTimer(dispatch::TimerInfo{
			.completion = dispatch::TimerInfo::Completion::create<WasmContextController>(this,
					&WasmContextController::onHostPoll),
			.timeout = dispatch::TimeInterval::milliseconds(8),
			.interval = dispatch::TimeInterval::milliseconds(8),
			.count = dispatch::TimerInfo::Infinite,
		});
	}, nullptr);

	_looper->run();

	return ContextController::run(container);
}

} // namespace sprt::window

#endif
