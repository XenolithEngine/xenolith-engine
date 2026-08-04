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

#include "XLUiAuxSession.h"

#include "XL2dLabel.h"
#include "XL2dLayer.h"
#include "XL2dSceneLayout.h"
#include "XLAppWindow.h"
#include "XLAppThread.h"
#include "XLContext.h"
#include "XLDirector.h"

#include <cmath>

#include <sprt/runtime/window/controller.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

static constexpr float kTipHeight = 34.0f;
static constexpr float kTipFontSize = 13.0f;
static constexpr float kTipPadding = 12.0f;

// Rough advance-width estimate: the tip is built before it is measured, and the exact metrics
// would need a font query on the app thread for a box that is clamped anyway.
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

AuxSession &AuxSession::instance() {
	static AuxSession s_instance;
	return s_instance;
}

AuxSession::AuxSession() {
	_life = Rc<Lifetime>::alloc();
	_life->session = this;
}

AuxSession::~AuxSession() {
	cancelHideTimer();
	if (_life) {
		_life->session = nullptr;
	}
}

sprt::window::WindowPlacement AuxSession::makePlacement(Vec2 anchorSceneYUp,
		float sceneHeight) const {
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

bool AuxSession::isParentUsable(AppWindow *parent) const {
	if (!parent || parent->isInCloseRequest()) {
		return false;
	}
	auto *info = parent->getInfo();
	if (!info || info->id.empty()) {
		return false;
	}
	auto *ctx = parent->getContext();
	auto *controller = ctx ? ctx->getController() : nullptr;
	return controller && controller->findWindow(info->id);
}

void AuxSession::showTip(NotNull<AppWindow> parent, StringView text, Vec2 anchorSceneYUp,
		float sceneHeight, TimeInterval hideDelay) {
	if (!isParentUsable(parent.get())) {
		return;
	}

	// Same tip already up: refresh the hide timer instead of a dismiss/recreate flap.
	if (_tipState == TipState::Ready && _parent == parent.get() && _tipText == text
			&& AuxWindow::hasOverlay(_tipId)) {
		armHideTimer(hideDelay);
		return;
	}

	replaceOverlayTip(parent, text, anchorSceneYUp, sceneHeight, hideDelay);
}

void AuxSession::replaceOverlayTip(NotNull<AppWindow> parent, StringView text, Vec2 anchorSceneYUp,
		float sceneHeight, TimeInterval hideDelay) {
	clearTip();

	_parent = parent.get();
	_tipText = text.str<Interface>();
	_hideDelay = hideDelay;

	const auto size = measureTipSize(_tipText);
	const auto placement = makePlacement(anchorSceneYUp, sceneHeight);

	_tipId = AuxWindow::showTooltip(_parent, placement,
			Extent2(uint32_t(size.width), uint32_t(size.height)),
			[str = _tipText, size](StringView) { return buildTipLayout(str, size); }, "Tip");

	if (_tipId.empty() || !AuxWindow::hasOverlay(_tipId)) {
		clearTip();
		return;
	}

	_tipState = TipState::Ready;
	armHideTimer(_hideDelay);
}

void AuxSession::clearTip() {
	cancelHideTimer();
	if (!_tipId.empty()) {
		AuxWindow::dismissOverlay(_tipId);
	}
	_tipState = TipState::Idle;
	_tipId.clear();
	_tipText.clear();
	_parent = nullptr;
}

void AuxSession::dismissTip() { clearTip(); }

void AuxSession::armHideTimer(TimeInterval hideDelay) {
	if (!_parent || _tipState != TipState::Ready) {
		return;
	}
	auto director = _parent->getDirector();
	auto app = director ? director->getApplication() : nullptr;
	auto looper = app ? app->getLooper() : nullptr;
	if (!looper) {
		return;
	}
	cancelHideTimer();
	_hideTimer = looper->scheduleTimer(sprt::dispatch::TimerInfo{
		.completion = sprt::dispatch::TimerInfo::Completion::create<Lifetime>(_life,
				[](Lifetime *life, sprt::dispatch::TimerHandle *, uint32_t, Status status) {
		if (!isSuccessful(status) || !life || !life->session || !life->session->_parent) {
			return;
		}
		auto *parent = life->session->_parent;
		auto director = parent->getDirector();
		auto app = director ? director->getApplication() : nullptr;
		if (!app) {
			return;
		}
		app->performOnAppThread([life = Rc<Lifetime>(life)] {
			if (auto *session = life->session) {
				session->_hideTimer = nullptr;
				session->clearTip();
			}
		}, parent);
	}),
		.timeout = hideDelay,
		.interval = hideDelay,
		.count = 1,
	});
}

void AuxSession::cancelHideTimer() {
	if (_hideTimer) {
		_hideTimer->cancel();
		_hideTimer = nullptr;
	}
}

String AuxSession::openPopup(NotNull<AppWindow> parent,
		const sprt::window::WindowPlacement &placement, Extent2 size,
		AuxWindow::ContentBuilder &&builder, StringView title) {
	if (!isParentUsable(parent.get())) {
		return String();
	}
	// The tip is an overlay on the same scene — drop it before the popup takes over.
	clearTip();
	return AuxWindow::openPopup(parent, placement, size, sp::move(builder), title);
}

} // namespace stappler::xenolith::ui
