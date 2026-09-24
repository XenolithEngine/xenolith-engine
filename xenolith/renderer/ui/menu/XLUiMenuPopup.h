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

#ifndef XENOLITH_RENDERER_UI_MENU_XLUIMENUPOPUP_H_
#define XENOLITH_RENDERER_UI_MENU_XLUIMENUPOPUP_H_

#include "XLUiMenuSystem.h"
#include "XLUiSubWindow.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

/** How a menu is opened as a surface of its own.

The surface only; contents come from MenuSource. openMenu measures the source with `style` and
requests exactly that extent. */
struct SP_PUBLIC MenuConfig {
	MenuStyle style;

	/* A stylesheet of the menu's own, replacing the inherited one; native path only. Empty means
	the parent window's sheet, which ui::openPopupSurface shares; with neither, the menu uses its
	own neutral colours. The overlay path is already inside the outer sheet's scope. */
	String stylesheet;
	FileCategory stylesheetCategory = FileCategory::Bundled;

	// Stylesheet as literal source, applied after `stylesheet`.
	String stylesheetSource;

	String title;
	String idPrefix;

	/* Whether the menu takes the keyboard (MenuSystem::setKeyboardEnabled); default true, as a
	popup has nothing else to take it from. `highlight` is the row the keyboard starts on (a
	ui::Select passes its current value). */
	String highlight;
	bool keyboard = true;

	// Hover behaviour; passed down to every submenu, like `style`.
	MenuHoverConfig hover;

	// Fired after an item's own callback has run and after the menu chain has been taken down.
	Function<void(NotNull<MenuSourceItem>)> onActivate;

	// Fired once, however the menu went away.
	Function<void()> onClose;

	sprt::window::WindowCreationFlags flags = sprt::window::WindowCreationFlags::None;

	bool preferNative = true;
};

/** Resolve where a menu opening off `anchor` should be placed.

The anchor rect comes from ui::placementAnchorRect (density scale and Y flip); this
adds the side. `gravity` in the result names the edge of the menu placed on the anchor point, not
the opening direction; see windows.adoc. */
SP_PUBLIC sprt::window::WindowPlacement placementForNode(NotNull<Node> anchor,
		MenuSide = MenuSide::Below, IVec2 offset = IVec2{0, 0});

/** The same, resolved from a point in `space`'s coordinates; what a context menu opens off.

The anchor rect is empty, which backends read as "this exact point". */
SP_PUBLIC sprt::window::WindowPlacement placementForPoint(NotNull<Node> space, const Vec2 &location,
		MenuSide = MenuSide::Below, IVec2 offset = IVec2{0, 0});

/** Open `source` as a popup surface at `placement`.

Native subwindow where available (including headless), in-scene overlay otherwise. Keep the
returned Rc for as long as the menu should stay open. */
SP_PUBLIC Rc<SubWindow> openMenu(NotNull<core::RenderServerChannel>,
		const sprt::window::WindowPlacement &, NotNull<MenuSource>, MenuConfig &&);

// openMenu with the placement resolved from a node.
SP_PUBLIC Rc<SubWindow> openMenuForNode(NotNull<core::RenderServerChannel>, NotNull<Node> anchor,
		NotNull<MenuSource>, MenuConfig &&, MenuSide = MenuSide::Below);

/** One link of an open menu chain, attached to the panel of a menu surface.

A submenu is another popup parented to its opener. A parent link holds its child surface and a
child points back with a raw pointer, so there is no cycle, and a closing parent takes its
descendants down in handleExit. Attached by openMenu. */
class SP_PUBLIC MenuPopupChain : public System {
public:
	static uint64_t Id;

	// The nearest chain link at or above `node`, i.e. "which open menu is this node in".
	static MenuPopupChain *findForNode(Node *);

	virtual ~MenuPopupChain() = default;

	// the config is passed down to submenus
	virtual bool init(NotNull<SubWindow>, MenuPopupChain *parent, MenuConfig &&);

	virtual void handleExit() override;

	// The surface this menu lives in. Raw: the surface owns the node this system is on.
	SubWindow *getSurface() const { return _surface; }

	// What this level was opened with. The root's carries the application's onActivate.
	const MenuConfig &getConfig() const { return _config; }

	MenuPopupChain *getParent() const { return _parent; }
	SubWindow *getChild() const { return _child; }

	// The row the open child belongs to, or null. The only record of the open submenu; MenuSystem
	// keeps none.
	MenuSourceButton *getChildItem() const { return _childItem; }

	// The root of the chain - the menu the user opened first.
	MenuPopupChain *getRoot();

	/* Open `item`'s submenu beside `row`, closing any other submenu at this level first. False when
	the item has no submenu or the surface is gone. Idempotent for the already open item, so the
	level is not rebuilt (no flicker, nested submenus stay open). */
	virtual bool openSubmenu(NotNull<MenuSourceButton>, NotNull<Node> row);

	virtual void dismissChild();

	/* The pointer arrived over this level: cancel pending closes on the levels above, which were
	armed when the pointer left their opener rows. Nothing is re-armed here. */
	virtual void handlePointerEntered();

	// Take the whole chain down, from the root. Safe to call from inside a row of any level.
	virtual void dismissChain();

protected:
	// Raw both ways: the surface owns this system through its layout, and the parent owns this
	// surface through its own _child.
	SubWindow *_surface = nullptr;
	MenuPopupChain *_parent = nullptr;

	Rc<SubWindow> _child;

	// Which row `_child` belongs to. Cleared with it, so the two can never disagree.
	Rc<MenuSourceButton> _childItem;

	// What openMenu was called with, minus the callbacks the chain replaces; copied to submenus.
	MenuConfig _config;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_MENU_XLUIMENUPOPUP_H_
