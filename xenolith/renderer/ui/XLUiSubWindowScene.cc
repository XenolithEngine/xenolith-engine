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

	if (queue) {
		if (!Scene2d::init(app, window, Rc<core::Queue>(queue), constraints)) {
			return false;
		}
	} else if (!Scene2d::init(app, window, constraints)) {
		return false;
	}

	_subWindow = subWindow.get();
	_builder = sp::move(builder);

	_content = Rc<basic2d::SceneContent2d>::create();
	setContent(_content);

	// Auxiliary surfaces are small; the FPS/debug chrome would be most of the window. It is also
	// AlwaysDirty, which would keep this window rendering forever.
	setFpsVisible(false);

	return true;
}

void SubWindowScene::handlePresented(Director *dir) {
	Scene2d::handlePresented(dir);

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

	// Timed phases and hover transitions only advance while frames are produced, and an auxiliary
	// window is otherwise on-demand: without this it freezes after its first frame and never
	// finishes laying its own text out.
	runAction(Rc<RenderContinuously>::create());
}

} // namespace stappler::xenolith::ui
