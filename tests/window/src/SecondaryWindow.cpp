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

#include "SecondaryWindow.h"

#include "XL2dSceneContent.h"
#include "XLAppWindow.h"
#include "XLContext.h"
#include "XLDirector.h"

#include <sprt/runtime/window/window_info.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// App-thread only, so plain statics are enough (same reasoning as ui::AuxWindow).
static Map<String, SecondaryWindow::ContentBuilder> s_builders;
static Map<String, basic2d::Scene2d *> s_scenes;

bool SecondaryWindow::open(NotNull<AppWindow> anyWindow, StringView id, Extent2 size,
		ContentBuilder &&builder) {
	if (!builder || id.empty()) {
		log::source().error("SecondaryWindow", "open: id and builder are required");
		return false;
	}

	auto ctx = anyWindow->getContext();
	if (!ctx) {
		return false;
	}

	s_builders.insert_or_assign(id.str<Interface>(), sp::move(builder));

	auto info = Rc<sprt::window::WindowInfo>::create();
	info->id = id.str<Interface>();
	info->title = toString("testapp ", id);
	// Root: no parent, and no WindowCapabilities::Subwindows required - which is what makes this
	// work on the headless controller, where subwindows are not supported at all.
	info->type = sprt::window::WindowType::Root;
	info->rect = IRect(0, 0, int32_t(size.width), int32_t(size.height));
	info->flags = sprt::window::WindowCreationFlags::None;

	ctx->createWindow(sp::move(info));
	return true;
}

auto SecondaryWindow::takeContentBuilder(StringView id) -> ContentBuilder {
	auto it = s_builders.find(id);
	if (it == s_builders.end()) {
		return nullptr;
	}
	auto builder = sp::move(it->second);
	s_builders.erase(it);
	return builder;
}

bool SecondaryWindow::isOpen(StringView id) { return getScene(id) != nullptr; }

basic2d::Scene2d *SecondaryWindow::getScene(StringView id) {
	auto it = s_scenes.find(id);
	return it != s_scenes.end() ? it->second : nullptr;
}

void SecondaryWindow::close(StringView id) {
	s_builders.erase(id.str<Interface>());
	if (auto scene = getScene(id)) {
		if (auto director = scene->getDirector()) {
			if (auto server = director->getRenderServer()) {
				static_cast<AppWindow *>(server)->close(true);
			}
		}
	}
}

void SecondaryWindow::handleSceneEntered(StringView id, basic2d::Scene2d *scene) {
	s_scenes.insert_or_assign(id.str<Interface>(), scene);
}

void SecondaryWindow::handleSceneExited(StringView id) { s_scenes.erase(id.str<Interface>()); }

bool SecondaryScene::init(NotNull<AppThread> app, NotNull<core::RenderServerChannel> window,
		const core::FrameConstraints &constraints, StringView id,
		SecondaryWindow::ContentBuilder &&builder) {
	if (!Scene2d::init(app, window, constraints)) {
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
	SecondaryWindow::handleSceneEntered(_windowId, this);
	log::source().info("SecondaryWindow", "scene entered id=", _windowId);
}

void SecondaryScene::handleExit() {
	SecondaryWindow::handleSceneExited(_windowId);
	log::source().info("SecondaryWindow", "scene exited id=", _windowId);
	Scene2d::handleExit();
}

} // namespace stappler::xenolith::app
