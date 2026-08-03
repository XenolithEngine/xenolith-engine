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
	return true;
}

WindowCapabilities HeadlessContextController::getCapabilities() const {
	return WindowCapabilities::None;
}

void HeadlessContextController::openUrl(StringView url) {
	// There is no desktop session to hand a URL to.
	oslog::vpwarn(__SPRT_LOCATION, "HeadlessContextController", "openUrl is not available");
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
			oslog::vperror(__SPRT_LOCATION, "HeadlessContextController", "Fail to load device loop");
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
