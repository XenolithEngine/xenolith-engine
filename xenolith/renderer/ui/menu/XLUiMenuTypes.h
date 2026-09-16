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

// State flags of a menu item. Disabled is explicit, independent of whether a callback is set.
enum class MenuItemFlags : uint32_t {
	None = 0,

	// Rendered but inert: the CSS `:disabled` state; a hotkey bound to it declines and the
	// combination continues down the walk.
	Disabled = 1 << 0,

	// Not built: takes no row and is not in the metrics.
	Hidden = 1 << 1,

	// A toggle that is on. Uses the leading column, which is reserved for every row of the menu.
	Checked = 1 << 2,

	// Activating it does not close the menu; for toggles flipped several in a row.
	KeepOpen = 1 << 3,
};

SP_DEFINE_ENUM_AS_MASK(MenuItemFlags)

/* Layout inputs of a menu, as numbers rather than CSS: the popup extent must be known before any
node exists (SubWindow::Config::size). Colours, font weights and corners come from CSS (`menu`). */
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

	// The natural width is clamped into this range; maxWidth makes long titles wrap.
	float minWidth = 160.0f;
	float maxWidth = 420.0f;

	uint16_t fontSize = 14;
	uint16_t subtitleFontSize = 12;
	uint16_t shortcutFontSize = 12;

	// Draw the accelerator column; off on touch builds.
	bool showShortcuts = true;

	// Wrap onto more lines instead of being clipped at maxWidth.
	bool wrapTitle = true;
	bool wrapSubtitle = true;

	bool operator==(const MenuStyle &) const = default;
};

/* When hovering opens and closes a submenu. Not part of MenuStyle: it does not affect measurement.

The close delay is longer than the open delay, and any hover in the meantime cancels the close, so
the pointer can cross rows below the opener on its way into the submenu. */
struct SP_PUBLIC MenuHoverConfig {
	// Off: a submenu row opens only on click or Right; hover only highlights.
	bool openSubmenu = true;

	TimeInterval openDelay = TimeInterval::milliseconds(220);
	TimeInterval closeDelay = TimeInterval::milliseconds(400);

	bool operator==(const MenuHoverConfig &) const = default;
};

/* The resolved geometry of one menu, the only place widths and heights are decided: the popup
surface extent, the answer to a `fit-content` ancestor, and each row's height.

Columns are shared by all rows so icons and accelerators line up; the text column, and so where
text wraps, is known only after every item is measured. */
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

	// One entry per visible item, in source order.
	Vector<Row> rows;

	// x of the text column's left edge, in the menu's content box. The other columns follow from
	// it.
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
