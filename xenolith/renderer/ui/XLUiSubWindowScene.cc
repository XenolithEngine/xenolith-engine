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

#include "XLUiSubWindowScene.h"

#include "XLAction.h"
#include "XLAppWindow.h"
#include "XLScheduler.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

bool SubWindowScene::init(NotNull<AppThread> app, NotNull<core::RenderServerChannel> window,
		const core::FrameConstraints &constraints, NotNull<SubWindow> subWindow,
		SubWindow::ContentBuilder &&builder) {
	// Adopt a queue the opener prewarmed on the window data, avoiding a render-queue compile;
	// otherwise build its own graph.
	auto appWindow = dynamic_cast<AppWindow *>(window.get());
	auto sceneInfo = appWindow ? appWindow->getSceneInfo() : nullptr;
	auto queue = sceneInfo ? sceneInfo->getQueue() : nullptr;

	// Set before the base init: buildQueueResources runs inside it and needs the window kind.
	_subWindow = subWindow.get();

	if (queue) {
		if (!Scene2d::init(app, window, Rc<core::Queue>(queue), constraints)) {
			return false;
		}
	} else if (!Scene2d::init(app, window, constraints)) {
		return false;
	}

	_builder = sp::move(builder);

	_content = Rc<basic2d::SceneContent2d>::create();
	setContent(_content);

	// Auxiliary surfaces are small, and the FPS/debug chrome is AlwaysDirty, which would keep the
	// window rendering forever.
	setFpsVisible(false);

	return true;
}

void SubWindowScene::buildQueueResources(QueueInfo &info, core::Queue::Builder &builder) {
	Scene2d::buildQueueResources(info, builder);

	/* An undecorated surface is its panel, so the clear colour shows only in corners a
	`border-radius` rounds away. Clear to transparent, so a blending compositor
	(Context::handleAppWindowSurfaceUpdate requests premultiplied alpha for these types) shows what
	is behind; otherwise the corners are black. Dialog and Utility windows keep the default. */
	if (_subWindow
			&& (_subWindow->getType() == sprt::window::WindowType::Popup
					|| _subWindow->getType() == sprt::window::WindowType::Tooltip)) {
		info.backgroundColor = Color4F(0.0f, 0.0f, 0.0f, 0.0f);
	}
}

void SubWindowScene::handleEnter(Scene *scene) {
	Scene2d::handleEnter(scene);

	_renderStartedAt = Time::now();
}

void SubWindowScene::handlePresented(Director *dir) {
	Scene2d::handlePresented(dir);

	_lastPresentedAt = Time::now();
	log::source().debug("SubWindowScene", "first present after ",
			(_lastPresentedAt - _renderStartedAt).toMillis(), "ms");

	if (_contentPushed || !_content) {
		return;
	}
	_contentPushed = true;

	// Built here rather than in init(): the Director is wired, so content reaching back through
	// the scene (a submenu, closing itself) finds a usable window.
	if (_builder) {
		if (auto layout = _builder(_subWindow)) {
			_content->pushLayout(layout);
		}
	}
}

} // namespace stappler::xenolith::ui
