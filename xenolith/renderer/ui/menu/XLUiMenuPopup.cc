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

#include "XLInheritedStyle.h" // isInlineRtl: which side a submenu opens on
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

// Surface colour for a menu with no stylesheet in scope; an unstyled ui::Panel is opaque white.
static constexpr Color4B s_menuSurfaceColor = Color4B(0x20, 0x20, 0x26, 0xFF);

static basic2d::SceneContent2d *MenuPopup_contentForWindow(core::RenderServerChannel *w) {
	auto director = getWindowDirector(w);
	auto scene = director ? director->getScene() : nullptr;
	return scene ? dynamic_cast<basic2d::SceneContent2d *>(scene->getContent()) : nullptr;
}

/* `gravity` names the edge of the menu placed on the anchor point, not the opening direction, so
"below" is gravity Top (the Wayland backend inverts it again for xdg_positioner). Shared by
placementForNode and placementForPoint. */
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
		// a submenu: opens off the row's top-right corner and flips left at the screen edge
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
	sprt::window::WindowPlacement ret;
	ret.offset = offset;

	// corners, conversion through the scene content and the Y flip are shared with dropdowns and
	// hints
	ret.anchorRect = placementAnchorRect(anchor);

	MenuPopup_applySide(ret, side);

	return ret;
}

sprt::window::WindowPlacement placementForPoint(NotNull<Node> space, const Vec2 &location,
		MenuSide side, IVec2 offset) {
	sprt::window::WindowPlacement ret;
	ret.offset = offset;

	// empty anchor rect: backends read a zero-sized anchor as "this point"
	ret.anchorRect = placementAnchorPoint(space, space->convertToWorldSpace(location));

	MenuPopup_applySide(ret, side);

	return ret;
}

// Builds a menu surface, for the root and every submenu. Handles measurement, chain and keyboard;
// the popup mechanics (stylesheet, panel, placement, outside tap) are ui::openPopupSurface's.
static Rc<SubWindow> MenuPopup_open(NotNull<core::RenderServerChannel> window,
		const sprt::window::WindowPlacement &placement, NotNull<MenuSource> source,
		MenuConfig &&config, MenuPopupChain *parent) {
	auto content = MenuPopup_contentForWindow(window);
	if (!content) {
		log::source().warn("MenuPopup", "a menu needs a parent window with a SceneContent2d");
		return nullptr;
	}

	auto director = getWindowDirector(window);
	auto app = director ? director->getApplication() : nullptr;
	auto controller = app ? app->getExtension<font::FontController>() : nullptr;
	if (!controller) {
		log::source().warn("MenuPopup", "a menu needs a font controller to measure itself");
		return nullptr;
	}

	const float density = content->getInputDensity();

	/* The extent must be known before any node exists, so it comes from measurement. MenuSystem
	runs the same measurement once the surface has a size, so both boxes agree. */
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
	// copied, not moved: the chain keeps the config and passes copies to submenus
	surfaceConfig.onClose = config.onClose;

	// clicking away closes the whole chain, not just this level
	surfaceConfig.onOutsideTap = [](NotNull<SubWindow> surface, NotNull<Panel> panel) {
		if (auto chain = MenuPopupChain::findForNode(panel)) {
			chain->dismissChain();
		} else {
			surface->dismiss();
		}
	};

	// capture by value: on the native path the builder runs when the popup scene is created,
	// possibly after the opener is gone
	surfaceConfig.content = [source = Rc<MenuSource>(source), config = config, parent](
									NotNull<SubWindow> surface, NotNull<Panel> panel) mutable {
		auto chain =
				panel->addSystem(Rc<MenuPopupChain>::create(surface, parent, sp::move(config)));

		auto menu = panel->addSystem(Rc<MenuSystem>::create(source, chain->getConfig().style));

		menu->setSubmenuHandler([chain](NotNull<MenuSourceButton> item, NotNull<Node> row) -> bool {
			return chain->openSubmenu(item, row);
		});

		// hovering another row closes the submenu; dismissChild with nothing open is a no-op
		menu->setSubmenuCloseHandler([chain] { chain->dismissChild(); });

		menu->setHoverConfig(chain->getConfig().hover);

		// keeps the level above from closing this one while the pointer is in it
		menu->setPointerEnterHandler([chain] { chain->handlePointerEntered(); });

		if (chain->getConfig().keyboard) {
			menu->setKeyboardEnabled(true);
			auto &highlight = chain->getConfig().highlight;
			if (!highlight.empty()) {
				menu->setHighlighted(source->getItem(highlight));
			}
		}

		// close before the command runs, so an action that opens another surface leaves no menu
		// behind
		menu->setWillActivateCallback([chain](NotNull<MenuSourceItem> item) {
			if (!item->isKeepOpen()) {
				chain->dismissChain();
			}
		});

		menu->setActivateCallback([chain](NotNull<MenuSourceItem> item) {
			// the root's callback: a chain reports as one menu
			if (auto &cb = chain->getRoot()->getConfig().onActivate) {
				cb(item);
			}
		});
	};

	return openPopupSurface(window, placement, sp::move(surfaceConfig));
}

Rc<SubWindow> openMenu(NotNull<core::RenderServerChannel> window,
		const sprt::window::WindowPlacement &placement, NotNull<MenuSource> source,
		MenuConfig &&config) {
	return MenuPopup_open(window, placement, source, sp::move(config), nullptr);
}

Rc<SubWindow> openMenuForNode(NotNull<core::RenderServerChannel> window, NotNull<Node> anchor,
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
	// closing a level closes everything it opened, so dismissing the root closes the chain
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

	/* Already open for this row: succeed without rebuilding, which would flicker and close nested
	levels. Re-entering the opener row and repeated clicks ask again. */
	if (_child && _childItem == item.get() && _child->isOpen()) {
		return true;
	}

	// getSubmenu, not getBuiltSubmenu: a lazy factory runs here
	auto source = item->getSubmenu();
	if (!source) {
		return false;
	}

	dismissChild();

	/* The parent window determines the placement's coordinate space. Native: the row is in this
	popup's scene, so the child is parented to this popup's window. Overlay: the row and the child
	are in the parent window's scene. */
	core::RenderServerChannel *parentWindow =
			_surface->isNative() ? _surface->getWindow() : _surface->getParent();
	if (!parentWindow) {
		return false;
	}

	// the submenu inherits the look and reports through the root
	MenuConfig config;
	config.style = _config.style;
	config.stylesheet = _config.stylesheet;
	config.stylesheetCategory = _config.stylesheetCategory;
	config.stylesheetSource = _config.stylesheetSource;
	config.title = _config.title;
	config.idPrefix = _config.idPrefix.empty() ? String("submenu") : _config.idPrefix;
	config.flags = _config.flags;
	config.preferNative = _config.preferNative;
	// inherited, unlike `highlight`, which names a row of this menu
	config.keyboard = _config.keyboard;
	config.hover = _config.hover;

	/* A submenu opens away from the inline start: to the left in a right-to-left interface. The
	direction comes from the row. */
	const auto side = isInlineRtl(row) ? MenuSide::Left : MenuSide::Right;

	_child = MenuPopup_open(parentWindow, placementForNode(row, side), source, sp::move(config),
			this);
	_childItem = _child ? Rc<MenuSourceButton>(item.get()) : nullptr;
	return _child != nullptr;
}

void MenuPopupChain::handlePointerEntered() {
	// every level above: the pointer crossed each of them and each armed a close
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
	// hold our surface: dismissing the root destroys the parent that owns it and this system's node
	Rc<SubWindow> self(_surface);

	auto root = getRoot();
	if (auto surface = root->_surface) {
		Rc<SubWindow> keepRoot(surface);
		keepRoot->dismiss();
	}
}

} // namespace stappler::xenolith::ui
