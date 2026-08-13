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

#include "window/SecondaryWindow.h"

#include "app/AppIcon.h"
#include "XL2dSceneContent.h"
#include "XLAppWindow.h"
#include "XLContext.h"
#include "XLDirector.h"

#include <sprt/runtime/window/window_info.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

Rc<WindowSceneInfo> SecondaryWindow::open(NotNull<AppWindow> anyWindow, StringView id, Extent2 size,
		ContentBuilder &&builder, WindowSceneInfo::CloseCallback &&onClose,
		Rc<core::Queue> &&queue) {
	if (!builder || id.empty()) {
		log::source().error("SecondaryWindow", "open: id and builder are required");
		return nullptr;
	}

	auto ctx = anyWindow->getContext();
	if (!ctx) {
		return nullptr;
	}

	// The builder travels WITH the window instead of being parked in a table keyed by `id`. That
	// also removes a latent hazard: ContextController re-uniques a colliding id, so the id the
	// scene factory used to look up was not necessarily the one asked for here.
	auto sceneInfo = Rc<WindowSceneInfo>::create(
			[builder = sp::move(builder), id = id.str<Interface>()](NotNull<AppThread> app,
					NotNull<core::RenderServerChannel> window,
					const core::FrameConstraints &c) mutable -> Rc<Scene> {
		return Rc<SecondaryScene>::create(app, window, c, id, sp::move(builder));
	}, sp::move(onClose));

	if (queue) {
		sceneInfo->setQueue(sp::move(queue));
	}

	auto info = Rc<sprt::window::WindowInfo>::create();
	info->id = id.str<Interface>();
	info->title = toString("testapp ", id);
	// Root: no parent, and no WindowCapabilities::Subwindows required - which is what makes this
	// work on the headless controller, where subwindows are not supported at all.
	info->type = sprt::window::WindowType::Root;
	info->rect = IRect(0, 0, int32_t(size.width), int32_t(size.height));
	info->flags = sprt::window::WindowCreationFlags::None;
	// Same icon as the root window: the `multi-window` layout is where you can see that every
	// window carries its own, not just the first one.
	info->icon = getAppIcon();
	info->appData = sceneInfo;

	ctx->createWindow(sp::move(info));
	return sceneInfo;
}

basic2d::Scene2d *SecondaryWindow::getScene(WindowSceneInfo *handle) {
	auto window = handle ? handle->getWindow() : nullptr;
	auto director = window ? window->getDirector() : nullptr;
	return director ? dynamic_cast<basic2d::Scene2d *>(director->getScene()) : nullptr;
}

void SecondaryWindow::close(WindowSceneInfo *handle) {
	if (auto window = handle ? handle->getWindow() : nullptr) {
		window->close(true);
	}
}

bool SecondaryScene::init(NotNull<AppThread> app, NotNull<core::RenderServerChannel> window,
		const core::FrameConstraints &constraints, StringView id,
		SecondaryWindow::ContentBuilder &&builder) {
	// Adopt the queue the opener prewarmed, if any - see QueueCache.
	auto appWindow = dynamic_cast<AppWindow *>(window.get());
	auto sceneInfo = appWindow ? appWindow->getSceneInfo() : nullptr;
	auto queue = sceneInfo ? sceneInfo->getQueue() : nullptr;

	if (queue) {
		if (!Scene2d::init(app, window, Rc<core::Queue>(queue), constraints)) {
			return false;
		}
	} else if (!Scene2d::init(app, window, constraints)) {
		return false;
	}

	_windowId = id.str<Interface>();

	auto content = Rc<basic2d::SceneContent2d>::create();
	content->setDefaultLights();
	if (auto layout = builder ? builder(_windowId) : nullptr) {
		content->pushLayout(layout);
	}
	setContent(content);

	// The frame counter is AlwaysDirty: it would keep this window producing frames forever and make
	// two runs impossible to compare. A secondary window exists to be observed, not to animate.
	setFpsVisible(false);
	return true;
}

void SecondaryScene::handleEnter(Scene *scene) {
	Scene2d::handleEnter(scene);
	log::source().info("SecondaryWindow", "scene entered id=", _windowId);
}

void SecondaryScene::handleExit() {
	log::source().info("SecondaryWindow", "scene exited id=", _windowId);
	Scene2d::handleExit();
}

} // namespace stappler::xenolith::app
