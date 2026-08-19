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

	/* `gravity` names which edge OF THE MENU lands on the anchor point, not the direction the menu
	opens - so "hang below" is gravity Top, and the Wayland backend inverts it again for
	xdg_positioner. */
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

	return ret;
}

// The one place a menu surface is built, for the root and for every submenu alike.
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

	const float parentHeight = content->getContentSize().height;

	SubWindow::Config surfaceConfig;
	surfaceConfig.type = sprt::window::WindowType::Popup;
	surfaceConfig.flags = config.flags;
	surfaceConfig.placement = placement;
	surfaceConfig.size = size;
	surfaceConfig.title = config.title.empty() ? StringView("Menu") : StringView(config.title);
	surfaceConfig.idPrefix =
			config.idPrefix.empty() ? StringView("menu") : StringView(config.idPrefix);
	surfaceConfig.preferNative = config.preferNative;

	if (config.onClose) {
		surfaceConfig.onClose = [cb = config.onClose](NotNull<SubWindow>) { cb(); };
	}

	// Everything the builder reads is captured BY VALUE: on the native path it does not run until
	// the popup's scene is created, by which time whatever opened the menu may be long gone.
	surfaceConfig.content =
			[source = Rc<MenuSource>(source), config = config, size, parentHeight, parent](
					NotNull<SubWindow> surface) mutable -> Rc<basic2d::SceneLayout2d> {
		auto layout = Rc<basic2d::SceneLayout2d>::create();
		layout->setName("menu-layout");

		const bool native = surface->isNative();

		if (native && (!config.stylesheet.empty() || !config.stylesheetSource.empty())) {
			// A native popup is a scene of its own and the parent window's sheet does not reach it.
			StyleSystem *style = nullptr;
			if (!config.stylesheet.empty()) {
				style = layout->addSystem(Rc<StyleSystem>::create(
						FileInfo{config.stylesheet, config.stylesheetCategory}));
			}
			if (!config.stylesheetSource.empty()) {
				if (style) {
					style->addStyle(config.stylesheetSource);
				} else {
					layout->addSystem(Rc<StyleSystem>::create(StringView(config.stylesheetSource)));
				}
			}
			layout->addSystem(Rc<StyleResolver>::create(true));
		}

		/* A SceneLayout2d paints nothing, so the menu's surface is this Panel - and it has to be
		RenderingLevel::Solid: opaque geometry is drawn first and writes depth, while the surface
		pass only TESTS against it, so a Panel left at the default Surface level cannot cover the
		labels of whatever is underneath it on the overlay path. */
		auto panel = layout->addChild(Rc<Panel>::create());
		panel->setName("menu");
		panel->setType("menu");
		panel->addStyleClass("xl-ui-menu");
		panel->setRenderingLevel(RenderingLevel::Solid);
		panel->setPathColor(s_menuSurfaceColor, false);
		panel->setAnchorPoint(Anchor::TopLeft);
		panel->setContentSize(Size2(float(size.width), float(size.height)));

		if (native) {
			// The surface IS the menu: the panel fills whatever extent the window system settled
			// on, which is not necessarily the one that was asked for.
			panel->setPosition(Vec2(0.0f, float(size.height)));
			layout->setContentSizeDirtyCallback([layout = layout.get(), panel] {
				const auto s = layout->getContentSize();
				if (s.width > 0.0f && s.height > 0.0f) {
					panel->setPosition(Vec2(0.0f, s.height));
					panel->setContentSize(s);
				}
			});
		} else {
			/* The overlay path. SceneContent2d::pushOverlay stretches the layout it is given over
			the whole parent content and puts its origin at the bottom left - right for an overlay
			and wrong for a menu - so the panel is placed inside it, at the placement the surface
			already resolved. That rect is Y-down from the content's top; the scene is Y-up. */
			panel->addStyleClass("overlay");
			const auto rect = surface->getOverlayRect();
			panel->setPosition(Vec2(float(rect.x), parentHeight - float(rect.y)));
		}

		auto chain =
				panel->addSystem(Rc<MenuPopupChain>::create(surface, parent, sp::move(config)));

		auto menu = panel->addSystem(Rc<MenuSystem>::create(source, chain->getConfig().style));

		menu->setSubmenuHandler([chain](NotNull<MenuSourceButton> item, NotNull<Node> row) -> bool {
			return chain->openSubmenu(item, row);
		});

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

		if (!native) {
			/* A native Popup is dismissed by the window system when the user clicks away from it.
			An overlay has no such contract, so the layout - which covers the whole parent content -
			listens for a press outside the panel and takes the chain down itself. */
			auto listener = layout->addSystem(Rc<InputListener>::create());
			listener->addTouchRecognizer([chain, panel](const GestureData &data) {
				if (data.event == GestureEvent::Began && !panel->isTouched(data.location())) {
					chain->dismissChain();
				}
				return true;
			});
			listener->setSwallowEvents(EventMaskTouch);
		}

		return layout;
	};

	// Through the session rather than SubWindow::open directly: it is what drops a live tooltip
	// before the menu takes over.
	if (auto session = SubWindowSession::get(window)) {
		return session->openPopup(sp::move(surfaceConfig));
	}
	return SubWindow::open(window, sp::move(surfaceConfig));
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

	_child = MenuPopup_open(parentWindow, placementForNode(row, MenuSide::Right), source,
			sp::move(config), this);
	return _child != nullptr;
}

void MenuPopupChain::dismissChild() {
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
