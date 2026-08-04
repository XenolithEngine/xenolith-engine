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

#include "AuxRootScene.h"
#include "AuxPopupScene.h"
#include "AuxTooltipScene.h"

#include "XL2dLabel.h"
#include "XL2dLayer.h"
#include "XL2dSceneContent.h"
#include "XL2dSceneLayout.h"
#include "XLAppWindow.h"
#include "XLContext.h"
#include "XLDirector.h"
#include "XLEntryPoint.h"
#include "XLSimpleButton.h"
#include "XLUiAuxWindow.h"
#include "XLUiAuxSession.h"
#include "XLAction.h"
#include "XLInputListener.h"
#include "AuxSelfTest.h"

#include <sprt/runtime/window/window_info.h>
#include <cstdlib>

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

using simpleui::ButtonWithLabel;
using ui::AuxWindow;

static Rc<Scene> auxui_makeScene(NotNull<AppThread> app,
		NotNull<core::RenderServerChannel> window, const core::FrameConstraints &constraints) {
	auto appWindow = static_cast<AppWindow *>(window.get());
	auto info = appWindow->getInfo();

	if (info) {
		if (info->type == sprt::window::WindowType::Popup) {
			return AuxPopupScene::create(app, window, constraints, info->id);
		}
		if (info->type == sprt::window::WindowType::Tooltip) {
			return AuxTooltipScene::create(app, window, constraints, info->id);
		}
	}

	return AuxRootScene::create(app, window, constraints,
			info ? StringView(info->id) : StringView());
}

static sprt::window::WindowPlacement placementAt(Vec2 anchorSceneYUp, float parentHeight,
		sprt::window::WindowAnchor a, sprt::window::WindowAnchor g) {
	sprt::window::WindowPlacement placement;
	// Scene nodes are Y-up; WindowPlacement / xdg_positioner is Y-down from parent top-left.
	const int32_t yDown = int32_t(std::lround(double(parentHeight) - double(anchorSceneYUp.y)));
	placement.anchorRect = IRect(int32_t(std::lround(anchorSceneYUp.x)), yDown, 0, 0);
	placement.anchor = a;
	placement.gravity = g;
	placement.adjustment = sprt::window::WindowPlacementAdjustment::FlipX
			| sprt::window::WindowPlacementAdjustment::FlipY
			| sprt::window::WindowPlacementAdjustment::SlideX
			| sprt::window::WindowPlacementAdjustment::SlideY;
	return placement;
}

bool AuxRootScene::init(NotNull<AppThread> app, NotNull<core::RenderServerChannel> window,
		const core::FrameConstraints &constraints, StringView id) {
	if (!basic2d::Scene2d::init(app, window, constraints)) {
		return false;
	}

	_myWindowId = id.str<mem_std::Interface>();
	_appWindow = static_cast<AppWindow *>(window.get());

	auto content = Rc<basic2d::SceneContent2d>::create();
	setContent(content);

	auto layout = Rc<basic2d::SceneLayout2d>::create();

	_bg = layout->addChild(Rc<basic2d::Layer>::create());
	_bg->setAnchorPoint(Anchor::TopLeft);
	_bg->setColor(Color(0x202020));

	_heading = layout->addChild(Rc<basic2d::Label>::create());
	_heading->setString("auxui — Popup/Tooltip scaffold");
	_heading->setFontSize(20);
	_heading->setColor(Color::White);
	_heading->setAnchorPoint(Anchor::TopLeft);

	_btnPopup = layout->addChild(Rc<ButtonWithLabel>::create("Open Popup"));
	_btnPopup->setAnchorPoint(Anchor::TopLeft);
	_btnPopup->setContentSize(Size2(240.0f, 40.0f));
	_btnPopup->setColor(Color::Indigo_500);
	_btnPopup->setCallback([this] {
		auto p = _btnPopup->getPosition();
		auto sz = _btnPopup->getContentSize();
		openMenuAt(Vec2(p.x + sz.width, p.y - sz.height));
	});

	_btnTooltip = layout->addChild(Rc<ButtonWithLabel>::create("Show Tooltip"));
	_btnTooltip->setAnchorPoint(Anchor::TopLeft);
	_btnTooltip->setContentSize(Size2(240.0f, 40.0f));
	_btnTooltip->setColor(Color::Indigo_500);
	_btnTooltip->setCallback([this] {
		_hoverArmed = false;
		stopAllActionsByTag(kHeadingHoverTipTag);
		auto p = _btnTooltip->getPosition();
		auto sz = _btnTooltip->getContentSize();
		openTooltipAt(Vec2(p.x + sz.width * 0.5f, p.y), "Tip: shown from the button");
	});

	// Hover-delay tooltip on the heading: pointer enter starts a 0.5s timer.
	// Do not dismiss on Ended — the tip window steals mouse-over from the heading and would
	// flash-close mid-present (same flap as the popup "Hover for tip" row). Hide timer closes it.
	auto hover = _heading->addSystem(Rc<InputListener>::create());
	hover->addMouseOverRecognizer([this](const GestureData &data) {
		if (data.event == GestureEvent::Began) {
			_hoverArmed = true;
			stopAllActionsByTag(kHeadingHoverTipTag);
			auto seq = Rc<Sequence>::create(0.5f, [this] {
				if (_hoverArmed && _heading) {
					auto p = _heading->getPosition();
					openTooltipAt(Vec2(p.x, p.y - 8.0f), "Tip: hovering the heading");
				}
			});
			seq->setTag(kHeadingHoverTipTag);
			runAction(seq);
		} else if (data.event == GestureEvent::Ended || data.event == GestureEvent::Cancelled) {
			_hoverArmed = false;
			stopAllActionsByTag(kHeadingHoverTipTag);
		}
		return true;
	});

	content->pushLayout(layout);
	return true;
}

void AuxRootScene::handlePresented(Director *dir) {
	basic2d::Scene2d::handlePresented(dir);
	layoutRootPanel();

	runAction(Rc<RenderContinuously>::create());

	if (const char *selftest = std::getenv("AUXUI_SELFTEST"); selftest && selftest[0] == '1') {
		AuxSelfTest::instance().startScenario(_appWindow, this);
		return;
	}

	const char *autoTooltip = std::getenv("AUXUI_AUTO_TOOLTIP");
	const bool withTooltip = autoTooltip && (autoTooltip[0] == '1' || autoTooltip[0] == '2');

	if (withTooltip) {
		// AUXUI_AUTO_TOOLTIP=1: button tip only.
		// AUXUI_AUTO_TOOLTIP=2: heading tip then button tip (retarget, no EndOfLife between).
		if (autoTooltip[0] == '2') {
			runAction(Rc<Sequence>::create(0.3f, [this] {
				if (_heading) {
					auto p = _heading->getPosition();
					openTooltipAt(Vec2(p.x, p.y - 8.0f), "Tip: hovering the heading");
				}
			}));
			runAction(Rc<Sequence>::create(0.9f, [this] {
				if (_btnTooltip) {
					auto p = _btnTooltip->getPosition();
					auto sz = _btnTooltip->getContentSize();
					openTooltipAt(Vec2(p.x + sz.width * 0.5f, p.y), "Tip: shown from the button");
				}
			}));
		} else {
			runAction(Rc<Sequence>::create(0.3f, [this] {
				if (_btnTooltip) {
					auto p = _btnTooltip->getPosition();
					auto sz = _btnTooltip->getContentSize();
					openTooltipAt(Vec2(p.x + sz.width * 0.5f, p.y), "Tip: shown from the button");
				}
			}));
		}
	}

	if (const char *env = std::getenv("AUXUI_AUTO_POPUP"); env && env[0] == '1') {
		// Give the scripted tooltip time to present before the menu evicts it.
		runAction(Rc<Sequence>::create(withTooltip ? 1.5f : 0.4f, [this] {
			if (!_btnPopup) {
				return;
			}
			auto p = _btnPopup->getPosition();
			auto sz = _btnPopup->getContentSize();
			openMenuAt(Vec2(p.x + sz.width, p.y - sz.height));
		}));
	}
}

void AuxRootScene::buildQueueResources(QueueInfo &info, core::Queue::Builder &) {
	info.backgroundColor = Color4F(0.12f, 0.12f, 0.12f, 1.0f);
}

void AuxRootScene::layoutRootPanel() {
	auto cs = getContentSize();
	if (_bg) {
		_bg->setContentSize(cs);
	}
	if (_heading) {
		_heading->setPosition(Vec2(24.0f, cs.height - 24.0f));
	}
	if (_btnPopup) {
		_btnPopup->setPosition(Vec2(24.0f, cs.height - 80.0f));
	}
	if (_btnTooltip) {
		_btnTooltip->setPosition(Vec2(24.0f, cs.height - 132.0f));
	}
}

void AuxRootScene::openMenuAt(Vec2 anchorWorld) {
	if (!_appWindow) {
		return;
	}

	const float parentH = getContent() ? getContent()->getContentSize().height : 0.0f;

	auto placement = placementAt(anchorWorld, parentH, sprt::window::WindowAnchor::BottomRight,
			sprt::window::WindowAnchor::TopLeft);
	// The menu Root opens is level 1; its geometry has to match what that scene will lay out.
	const auto menuSize = AuxPopupScene::getMenuSize(1);
	auto size = Extent2(uint32_t(menuSize.width), uint32_t(menuSize.height));

	// Builder returns nullptr: AuxPopupScene owns the default menu (needs scene-bound More/close).
	// AuxSession dismisses any live tip before opening the popup.
	ui::AuxSession::instance().openPopup(_appWindow, placement, size,
			[](StringView) { return nullptr; }, "auxui Menu");
}

void AuxRootScene::openTooltipAt(Vec2 anchorWorld, StringView text) {
	if (!_appWindow) {
		return;
	}
	const float parentH = getContent() ? getContent()->getContentSize().height : 0.0f;
	ui::AuxSession::instance().showTip(_appWindow, text, anchorWorld, parentH);
}

void AuxRootScene::dismissTooltip() { ui::AuxSession::instance().dismissTip(); }

} // namespace stappler::xenolith::app

DEFINE_SCENE_FACTORY(STAPPLER_VERSIONIZED_NAMESPACE::xenolith::app::auxui_makeScene)
