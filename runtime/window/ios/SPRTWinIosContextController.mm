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

#define __SPRT_BUILD 1

#include <sprt/runtime/log.h>

#include "SPRTWinIosContextController.h"

// Full definitions of the base controller's Rc<> members (NativeWindow,
// DisplayConfigManager) are needed so their destructors can be instantiated here.
#include <sprt/runtime/window/native_window.h>
#include <sprt/runtime/window/display_config.h>

#if SPRT_IOS

namespace sprt::window {

void IosContextController::acquireDefaultConfig(ContextConfig &config, NativeContextHandle *) {
	// iOS renders through MoltenVK (Vulkan-on-Metal), mirroring the macOS defaults.
	if (config.instance->api == gapi::InstanceApi::None) {
		config.instance->api = gapi::InstanceApi::Vulkan;
	}

	if (config.context) {
		config.context->flags |= ContextFlags::DestroyWhenAllWindowsClosed;
	}

	if (config.loop) {
		config.loop->defaultFormat = ImageFormat::B8G8R8A8_UNORM;
	}

	if (config.window) {
		if (config.window->imageFormat == ImageFormat::Undefined) {
			config.window->imageFormat = ImageFormat::B8G8R8A8_UNORM;
		}
	}
}

Rc<IosContextController> IosContextController::create(NotNull<Context> ctx, ContextConfig &&cfg,
		NotNull<dispatch::Looper> looper) {
	return Rc<IosContextController>::create(ctx, sprt::move(cfg), looper);
}

IosContextController::~IosContextController() { }

bool IosContextController::init(NotNull<Context> ctx, ContextConfig &&config,
		NotNull<dispatch::Looper> looper) {
	if (!ContextController::init(ctx, looper)) {
		return false;
	}

	_contextInfo = move(config.context);
	_windowInfo = move(config.window);
	_instanceInfo = move(config.instance);
	_loopInfo = move(config.loop);

	return true;
}

int IosContextController::run(NotNull<ContextContainer> ctx) {
	_container = ctx;

	// @TODO: preliminary stub. A real implementation drives a UIApplication /
	// UIWindow + CAMetalLayer here and pumps the iOS run loop.
	oslog::vpwarn(__SPRT_LOCATION, "IosContextController",
			"iOS window controller is not implemented yet (preliminary stub)");

	return ContextController::run(ctx);
}

bool IosContextController::isCursorSupported(WindowCursor, bool) const {
	// iOS has no system cursor.
	return false;
}

WindowCapabilities IosContextController::getCapabilities() const { return WindowCapabilities::None; }

void IosContextController::openUrl(StringView) {
	// @TODO: route through -[UIApplication openURL:options:completionHandler:]
	oslog::vpwarn(__SPRT_LOCATION, "IosContextController",
			"openUrl is not implemented yet (preliminary stub)");
}

SurfaceSupportInfo IosContextController::getSupportInfo() const {
	SurfaceSupportInfo ret;
	ret.backendMask.set(toInt(SurfaceBackend::IOS));
	ret.backendMask.set(toInt(SurfaceBackend::Metal));
	return ret;
}

} // namespace sprt::window

#endif // SPRT_IOS
