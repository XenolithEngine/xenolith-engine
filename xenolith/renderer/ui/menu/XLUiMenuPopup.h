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

Everything here is about the SURFACE. What the menu contains is the MenuSource, and how it is laid
out is the MenuStyle inside this - openMenu measures the source with that style and asks the window
system for exactly the extent the measurement produced, which is why the two can never disagree. */
struct SP_PUBLIC MenuConfig {
	MenuStyle style;

	/* The stylesheet the menu's own scene loads, used on the NATIVE path only.

	A native popup is a scene of its own: the ui::StyleSystem carrying the application's sheet lives
	on the PARENT window's content and does not reach it, so without this every node in the menu
	comes up unstyled. On the overlay path the menu is pushed under that same content and inherits
	the outer sheet, and declaring one here is harmless - the resolver applies outer sheets first.

	Empty means "no sheet": the menu then paints itself with its own neutral colours, the way
	ui::TooltipSystem's stock hint does. */
	String stylesheet;
	FileCategory stylesheetCategory = FileCategory::Bundled;

	// The same thing as a literal, for a menu whose look is declared in code (a test stand, an
	// auxiliary window that does not ship a .css). Applied after `stylesheet`, so the two compose.
	String stylesheetSource;

	String title;
	String idPrefix;

	/* The keyboard. A menu that is a surface of its own owns it - there is nothing else in that
	scene to take it from - so this defaults to true and MenuSystem::setKeyboardEnabled is called
	for the menu the popup builds. Turn it off for a surface that is only ever pointed at.

	`highlight` names the row the keyboard starts on. A ui::Select passes its current value here:
	a list opened from the keyboard that begins anywhere but at the current value is a list the
	user has to find their place in. */
	String highlight;
	bool keyboard = true;

	/* How the POINTER drives the chain: whether a hovered row opens its submenu, and after how
	long. Carried down to every submenu, like `style`, so that one menu answers the pointer the same
	way at every level. */
	MenuHoverConfig hover;

	// Fired after an item's own callback has run and after the menu chain has been taken down.
	Function<void(NotNull<MenuSourceItem>)> onActivate;

	// Fired once, however the menu went away.
	Function<void()> onClose;

	sprt::window::WindowCreationFlags flags = sprt::window::WindowCreationFlags::None;

	bool preferNative = true;
};

/** Resolve where a menu opening off `anchor` should be placed.

This is the arithmetic every hand-written menu in this tree has got wrong at least once, in one
place:

- the anchor rect is built from the node's four CORNERS in world space, not from its origin and
  size, because the node may be rotated or scaled;
- it is then converted into the scene CONTENT's space, which undoes the density scale - scene space
  is physical pixels, while WindowPlacement is in the window's logical points, and on a HiDPI
  display the two differ by a factor of two;
- and it is flipped into WindowPlacement's Y-DOWN space at the end.

`gravity` in the result names which edge OF THE MENU lands on the anchor point, not the direction
the menu opens - see the note in windows.adoc. */
SP_PUBLIC sprt::window::WindowPlacement placementForNode(NotNull<Node> anchor,
		MenuSide = MenuSide::Below, IVec2 offset = IVec2{0, 0});

/** The same, resolved from a POINT rather than from a node - what a CONTEXT menu opens off.

`location` is in `space`'s own coordinates; `space` is only there to say which node's transform and
which scene the point belongs to, so a canvas passes itself and the location the press arrived at.

The anchor rect comes out EMPTY, which every backend reads as "this exact point". Everything else -
the conversion through the scene content that undoes the density scale, the Y flip, and which edge
of the menu lands on the anchor - is shared with placementForNode rather than spelled again. */
SP_PUBLIC sprt::window::WindowPlacement placementForPoint(NotNull<Node> space, const Vec2 &location,
		MenuSide = MenuSide::Below, IVec2 offset = IVec2{0, 0});

/** Open `source` as a popup surface at `placement`.

Native subwindow where the platform has them, in-scene overlay where it does not - the caller never
branches on it, and headless is on the native side, so a menu is a separately renderable window
with no display in play.

The returned object IS the handle: keep the Rc for as long as the menu should stay open. */
SP_PUBLIC Rc<SubWindow> openMenu(NotNull<AppWindow>, const sprt::window::WindowPlacement &,
		NotNull<MenuSource>, MenuConfig &&);

// openMenu with the placement resolved from a node - the common case, and the one that gets the
// coordinate spaces right.
SP_PUBLIC Rc<SubWindow> openMenuForNode(NotNull<AppWindow>, NotNull<Node> anchor,
		NotNull<MenuSource>, MenuConfig &&, MenuSide = MenuSide::Below);

/** One link of an open menu chain, attached to the panel of a menu surface.

A submenu is another popup, parented to the menu it opened from (the window system allows a Popup
under a Popup for exactly this). The links form a strictly downward ownership chain - a parent link
holds its child SURFACE, a child link points back at its parent with a raw pointer - so there is no
cycle to break, and a parent that goes away takes its descendants with it through handleExit.

Attached by openMenu; an application does not create one. */
class SP_PUBLIC MenuPopupChain : public System {
public:
	static uint64_t Id;

	// The nearest chain link at or above `node`, i.e. "which open menu is this node in".
	static MenuPopupChain *findForNode(Node *);

	virtual ~MenuPopupChain() = default;

	// The config is carried down to submenus, so that a chain looks like one menu rather than a
	// family of them.
	virtual bool init(NotNull<SubWindow>, MenuPopupChain *parent, MenuConfig &&);

	virtual void handleExit() override;

	// The surface this menu lives in. Raw: the surface owns the node this system is on.
	SubWindow *getSurface() const { return _surface; }

	// What this level was opened with. The ROOT's is the one that carries the application's
	// onActivate: a chain reports as one menu.
	const MenuConfig &getConfig() const { return _config; }

	MenuPopupChain *getParent() const { return _parent; }
	SubWindow *getChild() const { return _child; }

	// The row the open child belongs to, or null when nothing is open. This is the ONE record of
	// which submenu is up: MenuSystem keeps none, because a copy of this would go stale.
	MenuSourceButton *getChildItem() const { return _childItem; }

	// The root of the chain - the menu the user opened first.
	MenuPopupChain *getRoot();

	/* Open `item`'s submenu beside `row`. Takes down whatever OTHER submenu was open here first.
	False when the item has no submenu, or the surface it would hang off is gone.

	IDEMPOTENT for the item that is already open, and that is not an optimization: the pointer
	leaving a submenu back onto the row that opened it asks again, so does a second click, and
	rebuilding the level would flicker it and lose whatever the user had opened below it. */
	virtual bool openSubmenu(NotNull<MenuSourceButton>, NotNull<Node> row);

	virtual void dismissChild();

	/* The pointer has arrived over THIS level; the levels above must forget whatever they had
	pending.

	Each of them armed a close the moment the pointer left the row that opened the level below - the
	diagonal trip into a submenu crosses the rows under its opener, and every one of those is such a
	hover. The close waits longer than the open so that the trip is survivable, but waiting is not
	enough on its own: a pointer that took longer than the delay, or that stopped on the way, would
	still have the level taken down from under it. Arriving here is the definite answer that the
	trip succeeded, and it is the level that was arrived at that gives it.

	Nothing is re-armed here. The pointer going back to a row of a level above arms that level's
	close again, which is the only thing that should. */
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

	// What openMenu was called with, minus the callbacks the chain replaces. A submenu is opened
	// with a copy of it.
	MenuConfig _config;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_MENU_XLUIMENUPOPUP_H_
