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

#include "XLUiMenuPopup.h"
#include "XLUiPopupSurface.h"
#include "XLUiSubWindowSession.h"
#include "XLUiStyleSystem.h"
#include "XLUiStyleResolver.h"
#include "XL2dSceneLayout.h"
#include "XLAppWindow.h"
#include "XLDirector.h"
#include "XLScene.h"
#include "XLSceneContent.h"

#include <cmath>

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

uint64_t MenuPopupChain::Id = System::GetNextSystemId();

// The menu's own paint, for a surface that reached the screen with no stylesheet in scope - a
// native popup in an application that never passed MenuConfig::stylesheet. The same reasoning as
// TooltipSystem's stock hint: an unstyled menu must still be readable, and ui::Panel with nothing
// declared is opaque WHITE.
static constexpr Color4B s_menuSurfaceColor = Color4B(0x20, 0x20, 0x26, 0xFF);

static basic2d::SceneContent2d *MenuPopup_contentForWindow(AppWindow *w) {
	auto director = w ? w->getDirector() : nullptr;
	auto scene = director ? director->getScene() : nullptr;
	return scene ? dynamic_cast<basic2d::SceneContent2d *>(scene->getContent()) : nullptr;
}

/* `gravity` names which edge OF THE MENU lands on the anchor point, not the direction the menu
opens - so "hang below" is gravity Top, and the Wayland backend inverts it again for
xdg_positioner.

Lifted out of placementForNode so that a placement built from a POINT answers with the same four
sides; a second spelling of this table would put a context menu on the other edge of the cursor. */
static void MenuPopup_applySide(sprt::window::WindowPlacement &ret, MenuSide side) {
	using namespace sprt::window;

	switch (side) {
	case MenuSide::Below:
		ret.anchor = WindowAnchor::BottomLeft;
		ret.gravity = WindowAnchor::TopLeft;
		ret.adjustment = WindowPlacementAdjustment::FlipY | WindowPlacementAdjustment::SlideX
				| WindowPlacementAdjustment::SlideY;
		break;
	case MenuSide::Above:
		ret.anchor = WindowAnchor::TopLeft;
		ret.gravity = WindowAnchor::BottomLeft;
		ret.adjustment = WindowPlacementAdjustment::FlipY | WindowPlacementAdjustment::SlideX
				| WindowPlacementAdjustment::SlideY;
		break;
	case MenuSide::Right:
		// A submenu: it opens off the row's top-right corner and flips to its left at the screen
		// edge, which is the one adjustment a vertical menu cannot do without.
		ret.anchor = WindowAnchor::TopRight;
		ret.gravity = WindowAnchor::TopLeft;
		ret.adjustment = WindowPlacementAdjustment::FlipX | WindowPlacementAdjustment::FlipY
				| WindowPlacementAdjustment::SlideX | WindowPlacementAdjustment::SlideY;
		break;
	case MenuSide::Left:
		ret.anchor = WindowAnchor::TopLeft;
		ret.gravity = WindowAnchor::TopRight;
		ret.adjustment = WindowPlacementAdjustment::FlipX | WindowPlacementAdjustment::FlipY
				| WindowPlacementAdjustment::SlideX | WindowPlacementAdjustment::SlideY;
		break;
	}
}

sprt::window::WindowPlacement placementForNode(NotNull<Node> anchor, MenuSide side, IVec2 offset) {
	using namespace sprt::window;

	WindowPlacement ret;
	ret.offset = offset;

	auto scene = anchor->getScene();
	auto content = scene ? scene->getContent() : nullptr;
	if (!content) {
		return ret;
	}

	const auto size = anchor->getContentSize();

	/* Four corners, not origin+size: the node may be rotated or scaled, and the rect the menu hangs
	off has to be the one that is actually on screen.

	Both conversions are load-bearing. convertToWorldSpace alone answers in SCENE space, which is
	physical pixels - Scene scales its whole subtree by the density - while WindowPlacement is in
	the window's logical points. On a HiDPI display the two differ by a factor of two, and mixing
	them puts the menu somewhere off the window entirely. */
	Vec2 corners[4] = {
		content->convertToNodeSpace(anchor->convertToWorldSpace(Vec2::ZERO)),
		content->convertToNodeSpace(anchor->convertToWorldSpace(Vec2(size.width, 0.0f))),
		content->convertToNodeSpace(anchor->convertToWorldSpace(Vec2(0.0f, size.height))),
		content->convertToNodeSpace(anchor->convertToWorldSpace(Vec2(size.width, size.height))),
	};

	Vec2 low = corners[0];
	Vec2 high = corners[0];
	for (auto &it : corners) {
		low.x = sprt::min(low.x, it.x);
		low.y = sprt::min(low.y, it.y);
		high.x = sprt::max(high.x, it.x);
		high.y = sprt::max(high.y, it.y);
	}

	// Scene nodes are Y-up; WindowPlacement is Y-down from the parent content's top-left.
	const float topYDown = content->getContentSize().height - high.y;

	ret.anchorRect = IRect(int32_t(std::lround(low.x)), int32_t(std::lround(topYDown)),
			uint32_t(std::lround(high.x - low.x)), uint32_t(std::lround(high.y - low.y)));

	MenuPopup_applySide(ret, side);

	return ret;
}

sprt::window::WindowPlacement placementForPoint(NotNull<Node> space, const Vec2 &location,
		MenuSide side, IVec2 offset) {
	sprt::window::WindowPlacement ret;
	ret.offset = offset;

	auto scene = space->getScene();
	auto content = scene ? scene->getContent() : nullptr;
	if (!content) {
		return ret;
	}

	// A point has no corners, so what placementForNode measures is skipped and what it CONVERTS is
	// not: the two-step through the content is what undoes the scene's density scale, and a menu
	// placed without it lands off the window on a HiDPI display.
	const auto at = content->convertToNodeSpace(space->convertToWorldSpace(location));

	// An empty rect. Every backend reads a zero-sized anchor as "this point", which is what a
	// context menu means, and it keeps the four sides answering as they do for a node.
	ret.anchorRect = IRect(int32_t(std::lround(at.x)),
			int32_t(std::lround(content->getContentSize().height - at.y)), 0, 0);

	MenuPopup_applySide(ret, side);

	return ret;
}

// The one place a menu surface is built, for the root and for every submenu alike.
//
// What is a MENU here is the measurement, the chain and the keyboard; everything a popup surface
// has to do to be one - the sheet its own scene needs, the panel's level and placement, the tap
// that closes it - is left to ui::openPopupSurface.
static Rc<SubWindow> MenuPopup_open(NotNull<AppWindow> window,
		const sprt::window::WindowPlacement &placement, NotNull<MenuSource> source,
		MenuConfig &&config, MenuPopupChain *parent) {
	auto content = MenuPopup_contentForWindow(window);
	if (!content) {
		log::source().warn("MenuPopup", "a menu needs a parent window with a SceneContent2d");
		return nullptr;
	}

	auto director = window->getDirector();
	auto app = director ? director->getApplication() : nullptr;
	auto controller = app ? app->getExtension<font::FontController>() : nullptr;
	if (!controller) {
		log::source().warn("MenuPopup", "a menu needs a font controller to measure itself");
		return nullptr;
	}

	const float density = content->getInputDensity();

	/* The extent has to be settled BEFORE any node exists - it is what the window request carries -
	so it comes from the measurement rather than from the nodes. The very same call runs again
	inside MenuSystem once the surface has a size, against the same source and the same style, so
	the box the window was created for and the box that is drawn cannot drift apart. */
	MeasureConstraints constraints;
	constraints.maxWidth = content->getContentSize().width;
	constraints.maxHeight = content->getContentSize().height;

	const auto metrics =
			MenuSystem::measure(controller, source, config.style, constraints, density);
	if (metrics.size.width <= 0.0f || metrics.rows.empty()) {
		return nullptr;
	}

	const Extent2 size(uint32_t(std::lround(metrics.size.width)),
			uint32_t(std::lround(metrics.size.height)));

	PopupSurfaceConfig surfaceConfig;
	surfaceConfig.stylesheet = config.stylesheet;
	surfaceConfig.stylesheetCategory = config.stylesheetCategory;
	surfaceConfig.stylesheetSource = config.stylesheetSource;
	surfaceConfig.title = config.title.empty() ? String("Menu") : config.title;
	surfaceConfig.idPrefix = config.idPrefix.empty() ? String("menu") : config.idPrefix;
	surfaceConfig.size = size;
	surfaceConfig.layoutName = String("menu-layout");
	surfaceConfig.panelName = String("menu");
	surfaceConfig.panelType = String("menu");
	surfaceConfig.panelClass = String("xl-ui-menu");
	surfaceConfig.fallbackColor = s_menuSurfaceColor;
	surfaceConfig.flags = config.flags;
	surfaceConfig.preferNative = config.preferNative;
	// COPIED, not moved: the chain keeps the config and hands a copy of it to every submenu, so a
	// callback taken out here would be missing from every level below this one.
	surfaceConfig.onClose = config.onClose;

	// Clicking away takes the WHOLE CHAIN down, not this link: a submenu left standing over a menu
	// that is gone is something the user has to dismiss by hand.
	surfaceConfig.onOutsideTap = [](NotNull<SubWindow> surface, NotNull<Panel> panel) {
		if (auto chain = MenuPopupChain::findForNode(panel)) {
			chain->dismissChain();
		} else {
			surface->dismiss();
		}
	};

	// Everything the builder reads is captured BY VALUE: on the native path it does not run until
	// the popup's scene is created, by which time whatever opened the menu may be long gone.
	surfaceConfig.content = [source = Rc<MenuSource>(source), config = config, parent](
									NotNull<SubWindow> surface, NotNull<Panel> panel) mutable {
		auto chain =
				panel->addSystem(Rc<MenuPopupChain>::create(surface, parent, sp::move(config)));

		auto menu = panel->addSystem(Rc<MenuSystem>::create(source, chain->getConfig().style));

		menu->setSubmenuHandler([chain](NotNull<MenuSourceButton> item, NotNull<Node> row) -> bool {
			return chain->openSubmenu(item, row);
		});

		// The pair of it, which is what a hover on another row asks for. dismissChild with nothing
		// open is free, so the menu never has to know whether there is anything to take down.
		menu->setSubmenuCloseHandler([chain] { chain->dismissChild(); });

		menu->setHoverConfig(chain->getConfig().hover);

		// What keeps this level from being closed by the level above while the pointer is in it
		menu->setPointerEnterHandler([chain] { chain->handlePointerEntered(); });

		if (chain->getConfig().keyboard) {
			menu->setKeyboardEnabled(true);
			auto &highlight = chain->getConfig().highlight;
			if (!highlight.empty()) {
				menu->setHighlighted(source->getItem(highlight));
			}
		}

		// Before the command runs: an action is free to put another surface up in this one's place,
		// and a menu still on screen behind it is something the user then has to dismiss by hand.
		menu->setWillActivateCallback([chain](NotNull<MenuSourceItem> item) {
			if (!item->isKeepOpen()) {
				chain->dismissChain();
			}
		});

		menu->setActivateCallback([chain](NotNull<MenuSourceItem> item) {
			// The root's callback, not this level's: a chain reports as one menu.
			if (auto &cb = chain->getRoot()->getConfig().onActivate) {
				cb(item);
			}
		});
	};

	return openPopupSurface(window, placement, sp::move(surfaceConfig));
}

Rc<SubWindow> openMenu(NotNull<AppWindow> window, const sprt::window::WindowPlacement &placement,
		NotNull<MenuSource> source, MenuConfig &&config) {
	return MenuPopup_open(window, placement, source, sp::move(config), nullptr);
}

Rc<SubWindow> openMenuForNode(NotNull<AppWindow> window, NotNull<Node> anchor,
		NotNull<MenuSource> source, MenuConfig &&config, MenuSide side) {
	return openMenu(window, placementForNode(anchor, side), source, sp::move(config));
}

// --- MenuPopupChain ----------------------------------------------------------------------------

MenuPopupChain *MenuPopupChain::findForNode(Node *node) {
	while (node) {
		if (auto chain = node->getSystemByType<MenuPopupChain>()) {
			return chain;
		}
		node = node->getParent();
	}
	return nullptr;
}

bool MenuPopupChain::init(NotNull<SubWindow> surface, MenuPopupChain *parent, MenuConfig &&config) {
	if (!System::init()) {
		return false;
	}

	_surface = surface;
	_parent = parent;
	_config = sp::move(config);

	_frameTag = MenuPopupChain::Id;
	setSystemFlags(SystemFlags::HandleOwnerEvents | SystemFlags::HandleSceneEvents);
	return true;
}

void MenuPopupChain::handleExit() {
	// This menu is going away; so does everything it opened. That is what makes dismissing the root
	// take the whole chain down without anyone walking it.
	dismissChild();
	System::handleExit();
}

MenuPopupChain *MenuPopupChain::getRoot() {
	auto ret = this;
	while (ret->_parent) { ret = ret->_parent; }
	return ret;
}

bool MenuPopupChain::openSubmenu(NotNull<MenuSourceButton> item, NotNull<Node> row) {
	if (!_surface) {
		return false;
	}

	/* Already up for this very row: say yes and change nothing. The pointer coming back out of a
	submenu onto the row that opened it asks again on every entering edge, and so does a second
	click - rebuilding here would flicker the level and drop everything opened below it. */
	if (_child && _childItem == item.get() && _child->isOpen()) {
		return true;
	}

	// getSubmenu, not getBuiltSubmenu: this IS the moment a lazy factory is meant to run.
	auto source = item->getSubmenu();
	if (!source) {
		return false;
	}

	dismissChild();

	/* Which window the submenu hangs off decides which coordinate space its placement is in, and
	the two must match. On the native path the row lives in this popup's OWN scene, so the child is
	parented to this popup's window (a Popup under a Popup, which is what the type is for). On the
	overlay path the row lives in the parent window's scene, and so does the child. */
	auto parentWindow = _surface->isNative() ? _surface->getWindow() : _surface->getParent();
	if (!parentWindow) {
		return false;
	}

	// The submenu inherits the look, and reports through the root: a chain is one menu.
	MenuConfig config;
	config.style = _config.style;
	config.stylesheet = _config.stylesheet;
	config.stylesheetCategory = _config.stylesheetCategory;
	config.stylesheetSource = _config.stylesheetSource;
	config.title = _config.title;
	config.idPrefix = _config.idPrefix.empty() ? String("submenu") : _config.idPrefix;
	config.flags = _config.flags;
	config.preferNative = _config.preferNative;
	// Inherited, unlike `highlight`: that one names a row of THIS menu and means nothing here.
	config.keyboard = _config.keyboard;
	config.hover = _config.hover;

	_child = MenuPopup_open(parentWindow, placementForNode(row, MenuSide::Right), source,
			sp::move(config), this);
	_childItem = _child ? Rc<MenuSourceButton>(item.get()) : nullptr;
	return _child != nullptr;
}

void MenuPopupChain::handlePointerEntered() {
	// EVERY level above, not only the one that opened this: a pointer that reached a third level
	// crossed the second and the first, and each of them armed a close on the way past.
	for (auto parent = _parent; parent; parent = parent->_parent) {
		auto owner = parent->getOwner();
		if (auto menu = owner ? owner->getSystemByType<MenuSystem>() : nullptr) {
			menu->cancelSubmenuDelay();
		}
	}
}

void MenuPopupChain::dismissChild() {
	_childItem = nullptr;
	if (auto child = sp::move(_child)) {
		_child = nullptr;
		child->dismiss();
	}
}

void MenuPopupChain::dismissChain() {
	// Our own surface, held for the duration: dismissing the root destroys the parent that owns
	// this surface, and with it the node this system lives on.
	Rc<SubWindow> self(_surface);

	auto root = getRoot();
	if (auto surface = root->_surface) {
		Rc<SubWindow> keepRoot(surface);
		keepRoot->dismiss();
	}
}

} // namespace stappler::xenolith::ui
