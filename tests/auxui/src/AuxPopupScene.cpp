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

#include "AuxPopupScene.h"

#include "XL2dLabel.h"
#include "XL2dLayer.h"
#include "XL2dSceneLayout.h"
#include "XLAppWindow.h"
#include "XLAppThread.h"
#include "XLDirector.h"
#include "XLSimpleButton.h"
#include "XLUiAuxWindow.h"
#include "XLAction.h"
#include "XLInputListener.h"

#include <cmath>
#include <cstdlib>

#include <sprt/runtime/window/window_info.h>
#include <sprt/runtime/dispatch/handle.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

using simpleui::ButtonWithLabel;
using ui::AuxWindow;

static constexpr float kMenuWidth = 220.0f;
static constexpr float kItemHeight = 30.0f;
static constexpr float kItemStride = 34.0f;
static constexpr float kHeaderHeight = 40.0f;
static constexpr float kBottomPadding = 10.0f;
static constexpr float kSidePadding = 12.0f;

// Item rows a menu at `level` carries: two plain entries, a tooltip probe, "More" for every level
// that still has one below it, and the closer.
static uint32_t getMenuItemCount(uint32_t level) {
	return (level < AuxPopupScene::kMaxLevel) ? 5 : 4;
}

Size2 AuxPopupScene::getMenuSize(uint32_t level) {
	return Size2(kMenuWidth,
			kHeaderHeight + float(getMenuItemCount(level)) * kItemStride + kBottomPadding);
}

static String getMenuTitle(uint32_t level) {
	return (level <= 1) ? String("Menu") : toString("Submenu L", level);
}

// Each level advertises a different hint, so it is obvious which window a tooltip belongs to.
static String getTooltipText(uint32_t level) {
	switch (level) {
	case 1: return String("Tip: top-level menu"); break;
	case 2: return String("Tip: second level submenu"); break;
	case 3: return String("Tip: third level, getting deep"); break;
	default: break;
	}
	return toString("Tip: level ", level, " is the last one");
}

void AuxPopupScene::openSubmenu() {
	if (!_appWindow || _level >= kMaxLevel) {
		return;
	}

	const auto childLevel = _level + 1;
	const auto size = getMenuSize(_level);
	const auto childSize = getMenuSize(childLevel);

	// Anchor on the right edge, level with the "More" row that opened it.
	const float moreTopY = size.height - kHeaderHeight - kItemStride * 2.0f;
	const int32_t yDown = int32_t(std::lround(double(size.height) - double(moreTopY)));

	sprt::window::WindowPlacement placement;
	placement.anchorRect = IRect(int32_t(size.width), yDown, 0, 0);
	placement.anchor = sprt::window::WindowAnchor::TopLeft;
	placement.gravity = sprt::window::WindowAnchor::TopLeft;
	placement.adjustment = sprt::window::WindowPlacementAdjustment::FlipX
			| sprt::window::WindowPlacementAdjustment::FlipY
			| sprt::window::WindowPlacementAdjustment::SlideX
			| sprt::window::WindowPlacementAdjustment::SlideY;

	// The AuxWindow builder cannot reach the new scene, so it declines and the real content comes
	// from SceneRegistry below — that builder is handed the scene and can therefore wire "More"
	// and the tooltip probe to it. Registering after open() is safe: the id is known here, and the
	// child scene reads the registry later on this same (app) thread.
	auto id = ui::AuxWindow::openPopup(_appWindow, placement,
			Extent2(uint32_t(childSize.width), uint32_t(childSize.height)),
			[](StringView) { return nullptr; }, toString("auxui ", getMenuTitle(childLevel)));

	if (id.empty()) {
		return;
	}

	SceneRegistry::set(id, [childLevel](AuxBaseScene *scene, StringView) {
		return static_cast<AuxPopupScene *>(scene)->buildMenuPanel(childLevel);
	});
}

Rc<basic2d::SceneLayout2d> AuxPopupScene::buildMenuPanel(uint32_t level) {
	_level = level;

	const auto size = getMenuSize(level);

	auto layout = Rc<basic2d::SceneLayout2d>::create();
	layout->setContentSize(size);
	_menuLayout = layout;

	auto bg = layout->addChild(Rc<basic2d::Layer>::create());
	bg->setAnchorPoint(Anchor::BottomLeft);
	bg->setPosition(Vec2::ZERO);
	bg->setContentSize(size);
	bg->setColor(Color(0x2A2A2E));

	auto accent = layout->addChild(Rc<basic2d::Layer>::create());
	accent->setAnchorPoint(Anchor::BottomLeft);
	accent->setPosition(Vec2::ZERO);
	accent->setContentSize(Size2(4.0f, size.height));
	accent->setColor(Color(0xC9A227));

	auto title = layout->addChild(Rc<basic2d::Label>::create());
	title->setString(getMenuTitle(level));
	title->setFontSize(18);
	title->setColor(Color::White);
	title->setAnchorPoint(Anchor::TopLeft);
	title->setPosition(Vec2(kSidePadding, size.height - 10.0f));

	struct Item {
		String text;
		Function<void()> cb;
	};

	Vector<Item> items;
	items.push_back(Item{String("Item one"), nullptr});
	items.push_back(Item{String("Item two"), nullptr});
	if (level < kMaxLevel) {
		items.push_back(Item{String("More \u25B8"), [this] { openSubmenu(); }});
	}
	items.push_back(Item{String("Hover for tip"), nullptr});
	items.push_back(Item{String("Just close"), [this] { closeThisWindow(); }});

	const float itemWidth = size.width - kSidePadding * 2.0f;
	float y = size.height - kHeaderHeight;
	for (auto &it : items) {
		auto item = layout->addChild(Rc<ButtonWithLabel>::create(it.text));
		item->setAnchorPoint(Anchor::TopLeft);
		item->setPosition(Vec2(kSidePadding, y));
		item->setContentSize(Size2(itemWidth, kItemHeight));
		item->setColor(Color(0x3A3A3A));
		if (it.cb) {
			item->setCallback(sprt::move(it.cb));
		} else if (it.text == "Hover for tip") {
			// Hover, not click: a hint should follow the pointer, and a menu row that opens a
			// window on click would be indistinguishable from a real command. Anchor the tooltip
			// just under the row it belongs to.
			const float anchorY = y - kItemHeight;
			auto hover = item->addSystem(Rc<InputListener>::create());
			// onlyFocused=false: a Popup is deliberately never the key window (so Root keeps
			// focus), which means it never carries WindowState::Focused. The default
			// onlyFocused=true would gate the recognizer on that flag and the hover callback would
			// never fire inside a menu.
			hover->addMouseOverRecognizer(
					[this, level, anchorY](const GestureData &data) {
						if (data.event == GestureEvent::Began) {
							// Same text already up → AuxSession only refreshes the hide timer
							// (does not dismiss+recreate). Do not dismiss on Ended: the tip can
							// sit under the row and flap isTouched; hide timer is the closer.
							showSceneTooltip(getTooltipText(level), Vec2(kSidePadding, anchorY));
						}
						return true;
					},
					InputMouseOverInfo(0.0f, false));
		}
		y -= kItemStride;
	}

	return layout;
}

Rc<basic2d::SceneLayout2d> AuxPopupScene::buildContent(SceneRegistry::Builder &&builder) {
	// A builder registered through AuxWindow wins (fully custom content).
	if (auto aux = AuxWindow::takeContentBuilder(_id)) {
		if (auto layout = aux(_id)) {
			return layout;
		}
	}
	// Then the SceneRegistry one — this is how a parent menu tells us which level we are.
	if (builder) {
		if (auto layout = builder(this, _id)) {
			return layout;
		}
	}
	// Nothing registered: we are the menu Root opened, i.e. level 1.
	return buildMenuPanel(1);
}

void AuxPopupScene::handlePresented(Director *dir) {
	basic2d::Scene2d::handlePresented(dir);
	pushContentLayout();

	// Scripted repro: every level opens the next one, so the whole chain (and the tooltip each
	// level shows) comes up without a single click. openSubmenu stops itself at kMaxLevel.
	if (_menuLayout) {
		if (const char *env = std::getenv("AUXUI_AUTO_SUBMENU"); env && env[0] == '1') {
			runAction(Rc<Sequence>::create(0.6f, [this] {
				showSceneTooltip(getTooltipText(_level), Vec2(kSidePadding, 0.0f));
				openSubmenu();
			}));
		}
	}

	// Hover-flap stress (the real failure mode): rapid show/dismiss of the tip, like the pointer
	// grazing "Hover for tip". Needs AUXUI_AUTO_POPUP=1 to open the menu first.
	// AUXUI_HOVER_STRESS=1 — recreate flaps (dismiss+show). =2 — also refresh-only + open L2.
	if (_menuLayout) {
		const char *stress = std::getenv("AUXUI_HOVER_STRESS");
		if (stress && (stress[0] == '1' || stress[0] == '2')) {
			runHoverStress(stress[0] == '2');
		}
	}
}

void AuxPopupScene::runHoverStress(bool alsoRefresh) {
	const auto size = getMenuSize(_level);
	const float tipTopY = size.height - kHeaderHeight - kItemStride * 3.0f;
	const Vec2 anchor(kSidePadding, tipTopY - kItemHeight);
	const auto tip = getTooltipText(_level);

	// Drive flaps with Scene Sequences (same as AUXUI_AUTO_* elsewhere). Do NOT use a bare
	// scheduleTimer CompletionHandle with a stack-local Rc: CompletionHandle stores a raw
	// pointer and does not retain — the state dies when runHoverStress returns, the handle's
	// cancel fires into a dangling pointer, and the "test" logs DONE with zero flaps.
	constexpr uint32_t kRecreatePairs = 10;
	const uint32_t recreateSteps = kRecreatePairs * 2;
	const uint32_t refreshSteps = alsoRefresh ? 11u : 0u; // 1 seed + 10 same-text refreshes
	const uint32_t totalSteps = recreateSteps + refreshSteps;

	struct HoverStressState : public Ref {
		virtual bool init() { return true; }

		void arm(float delay) {
			if (!scene) {
				return;
			}
			auto self = Rc<HoverStressState>(this);
			scene->runAction(Rc<Sequence>::create(delay, [self] { self->tick(); }));
		}

		void tick() {
			if (!scene) {
				return;
			}
			const auto i = step++;
			if (i >= totalSteps) {
				scene->dismissSceneTooltip();
				if (openNextLevel) {
					scene->openSubmenu();
				}
				scene = nullptr;
				return;
			}
			if (i < recreateSteps) {
				if ((i % 2) == 0) {
					scene->showSceneTooltip(tip, anchor);
				} else {
					scene->dismissSceneTooltip();
				}
			} else {
				scene->showSceneTooltip(tip, anchor);
			}
			arm(0.15f);
		}

		Rc<AuxPopupScene> scene;
		String tip;
		Vec2 anchor;
		uint32_t level = 0;
		uint32_t step = 0;
		uint32_t recreateSteps = 0;
		uint32_t totalSteps = 0;
		bool openNextLevel = false;
	};

	auto state = Rc<HoverStressState>::create();
	state->scene = this;
	state->tip = tip;
	state->anchor = anchor;
	state->level = _level;
	state->recreateSteps = recreateSteps;
	state->totalSteps = totalSteps;
	// After a thorough L1 run, open More so L2 gets the same stress (user repro is L1 or L2).
	state->openNextLevel = alsoRefresh && _level == 1;

	state->arm(0.5f);
}

} // namespace stappler::xenolith::app
