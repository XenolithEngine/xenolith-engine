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

#ifndef XENOLITH_RENDERER_UI_MENU_XLUIMENUTYPES_H_
#define XENOLITH_RENDERER_UI_MENU_XLUIMENUTYPES_H_

#include "XLUiConfig.h" // IWYU pragma: keep

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

class MenuSourceItem;

// What the model says ABOUT an item, as opposed to what the item is.
//
// Disabled is an explicit flag on purpose. The removed material2d model derived it from "has no
// callback", which made a submenu-only entry indistinguishable from a greyed-out command and left
// no way to grey out an item that does have one.
enum class MenuItemFlags : uint32_t {
	None = 0,

	// Rendered, reachable, and does nothing: the CSS `:disabled` state, and a hotkey bound to it
	// declines so the combination goes on down the walk.
	Disabled = 1 << 0,

	// Not built at all - it does not occupy a row, and it is not in the metrics.
	Hidden = 1 << 1,

	// A toggle that is on. Occupies the leading column, so a menu with one checkable item indents
	// every row alike rather than shifting when the mark appears.
	Checked = 1 << 2,

	// Activating it does NOT take the menu down. For the toggles a user flips several of in a row.
	KeepOpen = 1 << 3,
};

SP_DEFINE_ENUM_AS_MASK(MenuItemFlags)

/* Everything the LAYOUT of a menu needs, as numbers rather than CSS.

These are inputs of the measurement, not paint: the surface's Extent2 has to be known before a
single node exists (SubWindow::Config::size), so they cannot come from a stylesheet that is only
consulted once the nodes are there. Colours, fonts weights and corners are CSS's - see the `menu`
type and its children.

The same argument as DockSystem::setSplitterThickness, and the same split. */
struct SP_PUBLIC MenuStyle {
	// Row floor. A row grows past it when its text wraps.
	float itemMinHeight = 32.0f;
	float separatorHeight = 9.0f;

	// Inside the menu surface.
	float paddingVertical = 6.0f;
	float paddingHorizontal = 12.0f;

	// Inside a row, above and below the text block.
	float itemPaddingVertical = 6.0f;

	// Between the columns.
	float gap = 12.0f;

	float iconSize = 18.0f;

	// The width is clamped into this range after the natural width is known. maxWidth is what turns
	// a long title into a wrapped one instead of a menu running off the screen.
	float minWidth = 160.0f;
	float maxWidth = 420.0f;

	uint16_t fontSize = 14;
	uint16_t subtitleFontSize = 12;
	uint16_t shortcutFontSize = 12;

	// Draw the accelerator column at all. Off on a touch build, where there is no keyboard to
	// describe.
	bool showShortcuts = true;

	// Wrap onto more lines instead of being clipped at maxWidth.
	bool wrapTitle = true;
	bool wrapSubtitle = true;

	bool operator==(const MenuStyle &) const = default;
};

/* When the POINTER opens a submenu, and when it takes one down.

Not part of MenuStyle, and deliberately so: these are not inputs of the measurement, and changing
one does not move a single row. They are the two numbers that make hover navigation usable rather
than twitchy.

THE TWO DELAYS ARE NOT THE SAME NUMBER, and no desktop menu has ever made them one. Opening is a
decision the user made by stopping on a row. Closing may be no decision at all: the pointer on its
way INTO an open submenu crosses the rows below the one that opened it, and every one of those is a
hover that would otherwise take the submenu away from under it. So the close waits longer, and any
hover arriving in the meantime cancels it - which is what makes the diagonal trip survivable
without tracking the direction the pointer came from. */
struct SP_PUBLIC MenuHoverConfig {
	// Off returns the old behaviour exactly: a submenu row opens on a click and on Right, and a
	// hover means nothing but the highlight.
	bool openSubmenu = true;

	TimeInterval openDelay = TimeInterval::milliseconds(220);
	TimeInterval closeDelay = TimeInterval::milliseconds(400);

	bool operator==(const MenuHoverConfig &) const = default;
};

/* The resolved geometry of one menu: the SINGLE place a width or a height is decided.

One call answers all three questions that would otherwise drift apart: how big a popup surface to
ask the window system for (before any node exists), what a `fit-content` ancestor should be told
about an inline menu, and how tall each row has to be for its text to fit.

THE COLUMNS ARE SHARED BY EVERY ROW. That is what lines the icons and the accelerators up, and it
is why the text column - and therefore where the text wraps - can only be known after every item
has been measured. */
struct SP_PUBLIC MenuMetrics {
	struct Row {
		// Non-owning: the metrics are consumed within the pass that produced them.
		MenuSourceItem *item = nullptr;

		float height = 0.0f;

		// Wrapped heights of the two texts, so the builder does not measure them a second time.
		float titleHeight = 0.0f;
		float subtitleHeight = 0.0f;
	};

	// Zero means "no such column", and a zero column takes its gap with it.
	float leadingColumn = 0.0f;
	float textColumn = 0.0f;
	float shortcutColumn = 0.0f;
	float trailingColumn = 0.0f;

	Size2 size;

	// One entry per VISIBLE item, in source order.
	Vector<Row> rows;

	// x of the text column's left edge, in the menu's content box. The other columns follow from it.
	float textColumnOffset(const MenuStyle &style) const {
		return style.paddingHorizontal + (leadingColumn > 0.0f ? leadingColumn + style.gap : 0.0f);
	}
};

// Which side of the anchor a menu opens on. Resolved into a WindowPlacement by placementForNode.
enum class MenuSide {
	Below, // under the anchor, left edges aligned - a menu bar entry
	Above,
	Right, // to the right of the anchor, top edges aligned - a submenu
	Left,
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_MENU_XLUIMENUTYPES_H_
