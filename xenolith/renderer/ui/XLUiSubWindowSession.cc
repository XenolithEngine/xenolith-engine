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

#include "XLUiSubWindowSession.h"

#include "XL2dLabel.h"
#include "XL2dLayer.h"
#include "XL2dSceneContent.h"
#include "XL2dSceneLayout.h"
#include "XLAppThread.h"
#include "XLAppWindow.h"
#include "XLDirector.h"
#include "XLScene.h"

#include <cmath>

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

static constexpr float kTipHeight = 34.0f;
static constexpr float kTipFontSize = 13.0f;
static constexpr float kTipPadding = 12.0f;

// Rough advance-width estimate: the tip is built before it is measured, and the exact metrics
// would need a font query for a box that is clamped anyway.
static Size2 measureTipSize(StringView text) {
	const float textWidth = float(text.size()) * kTipFontSize * 0.58f;
	return Size2(sprt::max(120.0f, sprt::min(360.0f, textWidth + kTipPadding * 2.0f)), kTipHeight);
}

static Rc<basic2d::SceneLayout2d> buildTipLayout(StringView text, Size2 size) {
	auto layout = Rc<basic2d::SceneLayout2d>::create();
	layout->setContentSize(size);
	layout->setName("aux-tip");

	auto bg = layout->addChild(Rc<basic2d::Layer>::create());
	bg->setAnchorPoint(Anchor::BottomLeft);
	bg->setPosition(Vec2::ZERO);
	bg->setContentSize(size);
	bg->setColor(Color(0x101014));

	auto label = layout->addChild(Rc<basic2d::Label>::create());
	label->setString(text);
	label->setFontSize(uint16_t(kTipFontSize));
	label->setColor(Color::White);
	label->setAnchorPoint(Anchor::Middle);
	label->setPosition(Vec2(size.width / 2.0f, size.height / 2.0f));

	return layout;
}

static sprt::window::WindowPlacement makeTipPlacement(Vec2 anchorSceneYUp, float sceneHeight) {
	sprt::window::WindowPlacement placement;
	// Scene nodes are Y-up, WindowPlacement is Y-down from the parent content top-left.
	const int32_t yDown = int32_t(std::lround(double(sceneHeight) - double(anchorSceneYUp.y)));
	placement.anchorRect = IRect(int32_t(std::lround(anchorSceneYUp.x)), yDown, 0, 0);
	placement.anchor = sprt::window::WindowAnchor::TopLeft;
	placement.gravity = sprt::window::WindowAnchor::BottomLeft;
	placement.adjustment = sprt::window::WindowPlacementAdjustment::FlipY
			| sprt::window::WindowPlacementAdjustment::SlideX
			| sprt::window::WindowPlacementAdjustment::SlideY;
	return placement;
}

SubWindowSession *SubWindowSession::get(NotNull<AppWindow> window) {
	auto director = window->getDirector();
	auto scene = director ? director->getScene() : nullptr;
	auto content = scene ? scene->getContent() : nullptr;
	if (!content) {
		return nullptr;
	}

	if (auto session = content->getSystemByType<SubWindowSession>()) {
		return session;
	}
	return content->addSystem(Rc<SubWindowSession>::create());
}

SubWindowSession::~SubWindowSession() {
	cancelHideTimer();
	if (_life) {
		_life->session = nullptr;
	}
}

bool SubWindowSession::init() {
	if (!System::init()) {
		return false;
	}
	_life = Rc<Lifetime>::alloc();
	_life->session = this;
	return true;
}

void SubWindowSession::handleExit() {
	// The window is going away; the tip is an overlay in the very scene being torn down.
	clearTip();
	System::handleExit();
}

AppWindow *SubWindowSession::getWindow() const {
	auto owner = getOwner();
	auto scene = owner ? owner->getScene() : nullptr;
	auto director = scene ? scene->getDirector() : nullptr;
	auto server = director ? director->getRenderServer() : nullptr;
	return server ? dynamic_cast<AppWindow *>(server) : nullptr;
}

void SubWindowSession::showTip(StringView text, Vec2 anchorSceneYUp, float sceneHeight,
		TimeInterval hideDelay) {
	auto window = getWindow();
	if (!window || window->isInCloseRequest()) {
		return;
	}

	// Same tip already up: refresh the hide timer instead of a dismiss/recreate flap.
	if (hasTip() && _tipText == text) {
		armHideTimer(hideDelay);
		return;
	}

	clearTip();

	_tipText = text.str<Interface>();
	_hideDelay = hideDelay;

	const auto size = measureTipSize(_tipText);

	_tip = SubWindow::showTooltip(window, makeTipPlacement(anchorSceneYUp, sceneHeight),
			Extent2(uint32_t(size.width), uint32_t(size.height)),
			[str = _tipText, size](NotNull<SubWindow>) { return buildTipLayout(str, size); },
			"Tip");

	if (!hasTip()) {
		clearTip();
		return;
	}

	armHideTimer(_hideDelay);
}

void SubWindowSession::dismissTip() { clearTip(); }

void SubWindowSession::clearTip() {
	cancelHideTimer();
	if (_tip) {
		auto tip = sp::move(_tip);
		_tip = nullptr;
		tip->dismiss();
	}
	_tipText.clear();
}

void SubWindowSession::armHideTimer(TimeInterval hideDelay) {
	auto window = getWindow();
	auto director = window ? window->getDirector() : nullptr;
	auto app = director ? director->getApplication() : nullptr;
	auto looper = app ? app->getLooper() : nullptr;
	if (!looper) {
		return;
	}

	cancelHideTimer();
	_hideTimer = looper->scheduleTimer(sprt::dispatch::TimerInfo{
		.completion = sprt::dispatch::TimerInfo::Completion::create<Lifetime>(_life,
				[](Lifetime *life, sprt::dispatch::TimerHandle *, uint32_t, Status status) {
		if (!isSuccessful(status) || !life || !life->session) {
			return;
		}
		auto *session = life->session;
		auto window = session->getWindow();
		auto director = window ? window->getDirector() : nullptr;
		auto app = director ? director->getApplication() : nullptr;
		if (!app) {
			return;
		}
		app->performOnAppThread([life = Rc<Lifetime>(life)] {
			if (auto *session = life->session) {
				session->_hideTimer = nullptr;
				session->clearTip();
			}
		}, window);
	}),
		.timeout = hideDelay,
		.interval = hideDelay,
		.count = 1,
	});
}

void SubWindowSession::cancelHideTimer() {
	if (_hideTimer) {
		_hideTimer->cancel();
		_hideTimer = nullptr;
	}
}

Rc<SubWindow> SubWindowSession::openPopup(SubWindow::Config &&config) {
	auto window = getWindow();
	if (!window || window->isInCloseRequest()) {
		return nullptr;
	}
	clearTip();
	return SubWindow::open(window, sp::move(config));
}

} // namespace stappler::xenolith::ui
