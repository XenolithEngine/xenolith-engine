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
#include "XLUiTooltipSystem.h"

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
	// The stock hint lives in TooltipSystem, which is where its look and metrics are configurable.
	// This overload is that hint with the placement worked out by the caller.
	const TooltipConfig tipConfig;
	const auto size = TooltipSystem::measureDefaultTooltip(text, tipConfig);

	SubWindow::Config config;
	config.placement = makeTipPlacement(anchorSceneYUp, sceneHeight);
	config.size = size;
	config.title = StringView("Tip");
	// A native tip costs a swapchain for a few hundred milliseconds of hint and takes hover away
	// from the node it describes.
	config.preferNative = false;
	config.content = [str = text.str<Interface>(), size](NotNull<SubWindow> surface) {
		TooltipRequest request;
		request.text = str;
		request.size = size;
		return TooltipSystem::buildDefaultTooltip(surface, request);
	};

	showTip(sp::move(config), text, hideDelay);
}

Rc<SubWindow> SubWindowSession::showTip(SubWindow::Config &&config, StringView key,
		TimeInterval hideDelay) {
	auto window = getWindow();
	if (!window || window->isInCloseRequest()) {
		return nullptr;
	}

	// Same tip already up: refresh the hide timer instead of a dismiss/recreate flap. An empty key
	// opts out - it identifies nothing, so it can never be "the same".
	if (hasTip() && !key.empty() && _tipKey == key) {
		armHideTimer(hideDelay);
		return _tip;
	}

	clearTip();

	_tipKey = key.str<Interface>();
	_hideDelay = hideDelay;

	config.type = SubWindow::WindowType::Tooltip;

	// Chain rather than replace: the slot must be cleared however the surface went away, and a
	// caller with a close callback of its own is the normal case, not an exotic one.
	config.onClose = [life = _life, onClose = sp::move(config.onClose)](
							 NotNull<SubWindow> surface) mutable {
		if (auto *session = life ? life->session : nullptr) {
			// Only if this IS the live tip: a stale surface closing must not clear its successor.
			if (session->_tip.get() == surface.get()) {
				session->_tip = nullptr;
				session->_tipKey.clear();
				session->cancelHideTimer();
			}
		}
		if (onClose) {
			onClose(surface);
		}
	};

	_tip = SubWindow::open(window, sp::move(config));

	if (!hasTip()) {
		clearTip();
		return nullptr;
	}

	armHideTimer(_hideDelay);
	return _tip;
}

void SubWindowSession::refreshTip(TimeInterval hideDelay) {
	if (!hasTip()) {
		return;
	}
	_hideDelay = hideDelay;
	armHideTimer(hideDelay);
}

void SubWindowSession::dismissTip() { clearTip(); }

void SubWindowSession::clearTip() {
	cancelHideTimer();
	if (_tip) {
		auto tip = sp::move(_tip);
		_tip = nullptr;
		tip->dismiss();
	}
	_tipKey.clear();
}

void SubWindowSession::armHideTimer(TimeInterval hideDelay) {
	cancelHideTimer();

	// Zero means "no hide timer": the tip stays until a leave, a popup or the scene takes it down.
	if (!hideDelay) {
		return;
	}

	auto window = getWindow();
	auto director = window ? window->getDirector() : nullptr;
	auto app = director ? director->getApplication() : nullptr;
	auto looper = app ? app->getLooper() : nullptr;
	if (!looper) {
		return;
	}

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
