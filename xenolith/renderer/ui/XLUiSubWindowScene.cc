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
	// Adopt the queue the opener put on the window data, if there is one: that is what makes a
	// popup or dialog open without running the render-queue compiler again. Falls back to building
	// its own graph when the opener did not prewarm one.
	auto appWindow = dynamic_cast<AppWindow *>(window.get());
	auto sceneInfo = appWindow ? appWindow->getSceneInfo() : nullptr;
	auto queue = sceneInfo ? sceneInfo->getQueue() : nullptr;

	// Set before the base init, not after: buildQueueResources runs inside it and is where this
	// scene says what to clear to, which depends on what kind of window it is
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

	// Auxiliary surfaces are small; the FPS/debug chrome would be most of the window. It is also
	// AlwaysDirty, which would keep this window rendering forever.
	setFpsVisible(false);

	return true;
}

void SubWindowScene::buildQueueResources(QueueInfo &info, core::Queue::Builder &builder) {
	Scene2d::buildQueueResources(info, builder);

	/* An undecorated surface IS its panel, so what the scene is cleared to is only ever seen where
	the panel does not reach: the four corners a `border-radius` rounds away. White - the default,
	and right for a window whose content fills it - put a bright speck at each of them.

	Transparent, so a compositor that agreed to blend this window (Context::handleAppWindowSurfaceUpdate
	asks for premultiplied alpha for exactly these types) shows whatever is behind the menu there.
	Where it would not, the surface stays opaque and the corners come out black instead - still the
	wrong pixels, but the ones that read as a shadow rather than as a defect.

	A Dialog or a Utility window is left alone: those are ordinary rectangles with a frame the window
	system draws, and clearing them to nothing would only make an unpainted corner harder to see. */
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

	// Built here rather than in init(): by now the Director is fully wired, so content that reaches
	// back through the scene (to open a submenu, to close itself) finds a usable window.
	if (_builder) {
		if (auto layout = _builder(_subWindow)) {
			_content->pushLayout(layout);
		}
	}
}

} // namespace stappler::xenolith::ui
