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

#include "XLCommon.h"

#include "AuxBaseScene.h"

#include "XL2dSceneContent.h"
#include "XLAppWindow.h"
#include "XLDirector.h"
#include "XLAction.h"
#include "XLUiAuxSession.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

bool AuxBaseScene::init(NotNull<AppThread> app, NotNull<core::RenderServerChannel> window,
		const core::FrameConstraints &constraints, StringView id) {
	if (!Scene2d::init(app, window, constraints)) {
		return false;
	}

	_id = id.str<mem_std::Interface>();
	_appWindow = static_cast<AppWindow *>(window.get());

	_content = Rc<basic2d::SceneContent2d>::create();
	setContent(_content);
	// Aux surfaces are tiny — FPS/debug chrome just clutters the menu.
	setFpsVisible(false);

	return true;
}

void AuxBaseScene::pushContentLayout() {
	if (!_content) {
		return;
	}

	auto builder = SceneRegistry::take(_id);
	_content->pushLayout(buildContent(sprt::move(builder)));

	// Timed phases / hover-stress Sequences only advance while frames are produced. Aux windows
	// are otherwise on-demand and freeze without pointer motion — keep a quiet pump running.
	runAction(Rc<RenderContinuously>::create());
}

void AuxBaseScene::showSceneTooltip(StringView text, Vec2 anchorSceneYUp) {
	if (!_appWindow) {
		return;
	}
	const float height = getContent() ? getContent()->getContentSize().height : 0.0f;
	ui::AuxSession::instance().showTip(_appWindow, text, anchorSceneYUp, height);
}

void AuxBaseScene::dismissSceneTooltip() { ui::AuxSession::instance().dismissTip(); }

void AuxBaseScene::closeThisWindow() {
	if (!_appWindow) {
		return;
	}
	// Ordered tip dismiss before this popup closes (cascade would tear it down anyway).
	ui::AuxSession::instance().dismissTip();
	Rc<AppWindow> w = _appWindow;
	if (auto dir = w->getDirector()) {
		if (auto app = dir->getApplication()) {
			app->performOnAppThread([w = sprt::move(w)] {
				if (w) {
					w->hide();
				}
			}, w.get());
			return;
		}
	}
}

} // namespace stappler::xenolith::app
