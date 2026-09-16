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
#include "AuxTooltipScene.h"

#include "XL2dLabel.h"
#include "XL2dLayer.h"
#include "XL2dSceneContent.h"
#include "XL2dSceneLayout.h"
#include "XLAppWindow.h"
#include "XLContext.h"
#include "XLDirector.h"
#include "XLEntryPoint.h"
#include "XLUiButton.h"
#include "XLUiSubWindow.h"
#include "XLUiSubWindowSession.h"
#include "XLUiTooltipSystem.h"
#include "AuxPopupScene.h"
#include "XLSceneInspector.h"
#include "XLAction.h"
#include "XLInputListener.h"
#include "AuxSelfTest.h"

#include <sprt/runtime/window/window_info.h>
#include <cstdlib>

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

using ui::Button;
using ui::SubWindow;

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
		const core::FrameConstraints &constraints) {
	if (!basic2d::Scene2d::init(app, window, constraints)) {
		return false;
	}

	_appWindow = static_cast<AppWindow *>(window.get());
	if (auto info = _appWindow ? _appWindow->getInfo() : nullptr) {
		_myWindowId = info->id;
	}

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

	_btnPopup = layout->addChild(Rc<Button>::create("Open Popup"));
	_btnPopup->setAnchorPoint(Anchor::TopLeft);
	_btnPopup->setContentSize(Size2(240.0f, 40.0f));
	_btnPopup->setColor(Color::Indigo_500);
	_btnPopup->setCallback([this] {
		auto p = _btnPopup->getPosition();
		auto sz = _btnPopup->getContentSize();
		openMenuAt(Vec2(p.x + sz.width, p.y - sz.height));
	});

	_btnTooltip = layout->addChild(Rc<Button>::create("Show Tooltip"));
	_btnTooltip->setAnchorPoint(Anchor::TopLeft);
	_btnTooltip->setContentSize(Size2(240.0f, 40.0f));
	_btnTooltip->setColor(Color::Indigo_500);
	_btnTooltip->setCallback([this] {
		auto p = _btnTooltip->getPosition();
		auto sz = _btnTooltip->getContentSize();
		openTooltipAt(Vec2(p.x + sz.width * 0.5f, p.y), "Tip: shown from the button");
	});

	// A target hard against the bottom edge: its hint has no room below, so FlipY has to put it
	// above instead. This is the case that distinguishes a placement actually being resolved from
	// one being dropped at the raw anchor point.
	_edgeLabel = layout->addChild(Rc<basic2d::Label>::create());
	_edgeLabel->setString("bottom edge — tip must flip above");
	_edgeLabel->setFontSize(14);
	_edgeLabel->setColor(Color::White);
	_edgeLabel->setAnchorPoint(Anchor::BottomLeft);
	ui::setTooltip(_edgeLabel, "Tip: flipped above the edge");

	// Hover tooltip on the heading — the whole thing, dwell included, is TooltipSystem's. What used
	// to be here by hand (a tagged Sequence, an _hoverArmed flag, a leave that had to NOT dismiss)
	// is now its problem, and the node only says what the hint says.
	ui::setTooltip(_heading, "Tip: hovering the heading");

	// A second hint with a factory of its own and a pointer anchor, so the harness exercises both
	// halves of what a node may override.
	ui::setTooltip(_btnPopup,
			ui::TooltipInfo{
				.text = "Tip: opens the menu",
				.factory = [](NotNull<ui::SubWindow> surface,
								   const ui::TooltipRequest &req) -> Rc<basic2d::SceneLayout2d> {
		auto layout = ui::TooltipSystem::buildDefaultTooltip(surface, req);
		// Named so an inspector dump can tell a custom hint from the stock one.
		layout->setName("aux-tip-custom");
		return layout;
	},
				.placement = ui::TooltipPlacement{.anchorMode = ui::TooltipAnchorMode::Pointer},
			});


	content->pushLayout(layout);
	return true;
}

void AuxRootScene::handlePresented(Director *dir) {
	basic2d::Scene2d::handlePresented(dir);
	layoutRootPanel();

	// Not in init(): acquireForNode walks to the scene's content node, and in init() this scene is
	// not attached yet, so it would find nothing and warn.
	if (auto *tips = ui::TooltipSystem::acquireForNode(getContent())) {
		auto config = tips->getConfig();
		config.hoverDelay = TimeInterval::milliseconds(500);
		tips->setConfig(config);
	}

	registerCommands();

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
	if (_edgeLabel) {
		_edgeLabel->setPosition(Vec2(24.0f, 4.0f));
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

	// The menu's level travels with the window request, in the scene builder itself - that is what
	// removed the id-keyed registry this used to need.
	SubWindow::Config config;
	config.type = sprt::window::WindowType::Popup;
	config.placement = placement;
	config.size = size;
	config.title = StringView("auxui Menu");
	config.idPrefix = StringView("menu");
	config.scene = [](NotNull<SubWindow> surface, NotNull<AppThread> app,
							 NotNull<core::RenderServerChannel> window,
							 const core::FrameConstraints &c) -> Rc<Scene> {
		return AuxPopupScene::create(app, window, c, surface, 1);
	};

	// The session drops any live tip before the menu takes over.
	if (auto session = ui::SubWindowSession::get(_appWindow)) {
		_menu = session->openPopup(sp::move(config));
	}
}

// Dialog and Utility have no buttons in the panel: they are exercised from the inspector, which
// is what lets a headless run drive them and then look at the resulting graph.
void AuxRootScene::registerCommands() {
	auto content = getContent();
	if (!content || !_appWindow) {
		return;
	}

	auto openSurface = [this](sprt::window::WindowType type, bool modal, StringView label) {
		ui::SubWindow::Config config;
		config.type = type;
		config.flags = sprt::window::WindowCreationFlags::AllowClose
				| sprt::window::WindowCreationFlags::AllowMove;
		if (modal) {
			config.flags |= sprt::window::WindowCreationFlags::Modal;
		}
		config.size = Extent2(360, 200);
		config.title = label;
		config.idPrefix = type == sprt::window::WindowType::Dialog ? StringView("dialog")
																   : StringView("utility");
		config.content = [label = label.str<Interface>()](
								 NotNull<ui::SubWindow> surface) -> Rc<basic2d::SceneLayout2d> {
			auto layout = Rc<basic2d::SceneLayout2d>::create();
			layout->setContentSize(Size2(360.0f, 200.0f));
			layout->setName("aux-dialog");

			auto bg = layout->addChild(Rc<basic2d::Layer>::create());
			bg->setAnchorPoint(Anchor::BottomLeft);
			bg->setPosition(Vec2::ZERO);
			bg->setContentSize(Size2(360.0f, 200.0f));
			bg->setColor(Color(0x24303A));

			auto title = layout->addChild(Rc<basic2d::Label>::create());
			title->setString(label);
			title->setFontSize(18);
			title->setColor(Color::White);
			title->setAnchorPoint(Anchor::Middle);
			title->setPosition(Vec2(180.0f, 140.0f));

			auto close = layout->addChild(Rc<ui::Button>::create("Close"));
			close->setAnchorPoint(Anchor::Middle);
			close->setPosition(Vec2(180.0f, 60.0f));
			close->setContentSize(Size2(120.0f, 32.0f));
			close->setColor(Color(0x3A3A3A));
			close->setCallback([surface = Rc<ui::SubWindow>(surface)] { surface->dismiss(); });

			return layout;
		};
		return ui::SubWindow::open(_appWindow, sp::move(config));
	};

	inspector::addCommand(content, "open-dialog", "Open a modal Dialog surface",
			[this, openSurface](Value &&, Function<void(Value &&)> &&done) {
		_dialog = openSurface(sprt::window::WindowType::Dialog, true, "auxui Dialog");
		Value r;
		r.setBool(_dialog != nullptr, "opened");
		r.setBool(_dialog && _dialog->isNative(), "native");
		if (_dialog) {
			r.setString(_dialog->getId(), "id");
		}
		done(sp::move(r));
	});

	inspector::addCommand(content, "open-utility", "Open a Utility palette surface",
			[this, openSurface](Value &&, Function<void(Value &&)> &&done) {
		_utility = openSurface(sprt::window::WindowType::Utility, false, "auxui Utility");
		Value r;
		r.setBool(_utility != nullptr, "opened");
		r.setBool(_utility && _utility->isNative(), "native");
		done(sp::move(r));
	});

	inspector::addCommand(content, "dismiss-dialog", "Dismiss only the Dialog surface",
			[this](Value &&, Function<void(Value &&)> &&done) {
		Value r;
		r.setBool(_dialog != nullptr, "had");
		if (_dialog) {
			_dialog->dismiss();
			_dialog = nullptr;
		}
		done(sp::move(r));
	});

	inspector::addCommand(content, "dismiss-utility", "Dismiss only the Utility surface",
			[this](Value &&, Function<void(Value &&)> &&done) {
		Value r;
		r.setBool(_utility != nullptr, "had");
		if (_utility) {
			_utility->dismiss();
			_utility = nullptr;
		}
		done(sp::move(r));
	});

	inspector::addCommand(content, "dismiss-aux", "Dismiss the Dialog / Utility surfaces",
			[this](Value &&, Function<void(Value &&)> &&done) {
		Value r;
		r.setBool(_dialog != nullptr, "hadDialog");
		if (_dialog) {
			_dialog->dismiss();
			_dialog = nullptr;
		}
		if (_utility) {
			_utility->dismiss();
			_utility = nullptr;
		}
		done(sp::move(r));
	});
}

void AuxRootScene::openTooltipAt(Vec2 anchorWorld, StringView text) {
	if (!_appWindow) {
		return;
	}
	const float parentH = getContent() ? getContent()->getContentSize().height : 0.0f;
	if (auto session = ui::SubWindowSession::get(_appWindow)) {
		session->showTip(text, anchorWorld, parentH);
	}
}

void AuxRootScene::dismissTooltip() {
	if (auto session = ui::SubWindowSession::get(_appWindow)) {
		session->dismissTip();
	}
}

} // namespace stappler::xenolith::app

// Only the root window comes through the process-wide factory now: Popup/Tooltip windows name
// their own scene in the window data (SubWindow::Config::scene), so there is nothing to dispatch.
DEFINE_PRIMARY_SCENE_CLASS(STAPPLER_VERSIONIZED_NAMESPACE::xenolith::app::AuxRootScene)
