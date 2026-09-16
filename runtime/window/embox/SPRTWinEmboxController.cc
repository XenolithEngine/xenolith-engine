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

#include "SPRTWinEmboxController.h"

#if SPRT_EMBOX

#include "SPRTWinEmboxWindow.h"
#include <sprt/runtime/window/context.h>
#include <sprt/runtime/window/display_config.h>
#include <sprt/runtime/platform.h>
#include <sprt/runtime/log.h>

namespace sprt::window {

Rc<EmboxContextController> EmboxContextController::create(NotNull<Context> ctx, ContextConfig &&cfg,
		NotNull<dispatch::Looper> looper) {
	return Rc<EmboxContextController>::create(ctx, sprt::move(cfg), looper);
}

void EmboxContextController::acquireDefaultConfig(ContextConfig &config, NativeContextHandle *) {
	if (!config.instance) {
		config.instance = Rc<gapi::InstanceInfo>::alloc();
	}
	if (config.instance->api == gapi::InstanceApi::None) {
		config.instance->api = gapi::InstanceApi::Software;
	}

	if (!config.context) {
		config.context = Rc<ContextInfo>::alloc();
	}
	config.context->flags |= ContextFlags::DestroyWhenAllWindowsClosed;

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

EmboxContextController::~EmboxContextController() { }

bool EmboxContextController::init(NotNull<Context> ctx, ContextConfig &&config,
		NotNull<dispatch::Looper> looper) {
	if (!ContextController::init(ctx, looper)) {
		return false;
	}

	_contextInfo = sprt::move(config.context);
	_windowInfo = sprt::move(config.window);
	_instanceInfo = sprt::move(config.instance);
	_loopInfo = sprt::move(config.loop);

	if (!_instanceInfo) {
		_instanceInfo = Rc<gapi::InstanceInfo>::alloc();
	}
	_instanceInfo->api = gapi::InstanceApi::Software;
	return true;
}

WindowCapabilities EmboxContextController::getCapabilities() const {
	return WindowCapabilities::None;
}

void EmboxContextController::openUrl(StringView) {
	oslog::vpwarn(__SPRT_LOCATION, "EmboxContextController", "openUrl is not available");
}

bool EmboxContextController::loadWindow(Rc<WindowInfo> &&wInfo) {
	auto window = Rc<EmboxWindow>::create(this, sprt::move(wInfo));
	if (!window) {
		return false;
	}
	notifyWindowCreated(window);
	return true;
}

int EmboxContextController::run(NotNull<ContextContainer> container) {
	oslog::vpinfo(__SPRT_LOCATION, "EmboxContextController", "run()");

	_context->handleConfigurationChanged(sprt::move(_contextInfo));
	_contextInfo = nullptr;

	_looper->performOnThread([this] {
		auto instance = _context->makeInstance(_instanceInfo);
		if (!instance) {
			oslog::vperror(__SPRT_LOCATION, "EmboxContextController",
					"Fail to load software graphics instance");
			_resultCode = -1;
			destroy();
			return;
		}

		auto loop = _context->makeLoop(instance, _loopInfo);
		if (!loop) {
			oslog::vperror(__SPRT_LOCATION, "EmboxContextController", "Fail to load device loop");
			_resultCode = -1;
			destroy();
			return;
		}

		_context->handleGraphicsLoaded(loop);

		if (!resume()) {
			oslog::vperror(__SPRT_LOCATION, "EmboxContextController", "Fail to resume Context");
			_resultCode = -1;
			destroy();
			return;
		}

		createWindow(sprt::move(_windowInfo));
	}, nullptr, true);

	_looper->run();

	return ContextController::run(container);
}

} // namespace sprt::window

#endif // SPRT_EMBOX
