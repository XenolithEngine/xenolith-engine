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

#ifndef XENOLITH_RENDERER_UI_MENU_XLUIMENUITEM_H_
#define XENOLITH_RENDERER_UI_MENU_XLUIMENUITEM_H_

#include "XLUiMenuSystem.h"
#include "XLUiButton.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

/** One command row.

It is a Button, so hover, press, `:hover`/`:active`/`:disabled` and the fill/outline/corner
appliers all come for free - the same reason DockTab is one. What it adds is the four extra slots a
menu row has over a button: a second line of text, an accelerator, a trailing icon, and the shared
column geometry that lines every row in the menu up with its neighbours.

Button's own `_label` IS the title and `_icon` IS the leading icon; only the subtitle, the
accelerator and the trailing icon are extra nodes, and each of them exists only while it has
something to show.

THE ROW DOES NOT MEASURE ITSELF. Its size and its column widths are handed to it by MenuSystem,
which resolved them for the whole menu at once - that is what makes the icons of different rows
line up, and the accelerators sit in one right-hand column.

CSS type "menu-item"; children `label`, `menu-item-subtitle`, `menu-item-shortcut`, `icon`,
`menu-item-trailing`. Style classes: `checked` while the item is on, `disabled` from Button,
`submenu` when the row opens one. */
class SP_PUBLIC MenuItem : public Button {
public:
	virtual ~MenuItem() = default;

	virtual bool init(NotNull<MenuSystem>, NotNull<MenuSourceButton>);

	virtual void handleContentSizeDirty() override;

	MenuSourceButton *getItem() const { return _item; }

	// Re-read everything from the model. Cheap enough to run on any change: every setter below it
	// is equality-guarded by the widget it writes to.
	virtual void updateFromSource();

	/* The shared column geometry of the menu this row belongs to, plus this row's own wrapped text
	heights. Stamped by MenuSystem before it commits the row's size. */
	virtual void setRowGeometry(const MenuStyle &, const MenuMetrics &, const MenuMetrics::Row &);

	basic2d::Label *getSubtitleLabel() const { return _subtitle; }
	basic2d::Label *getShortcutLabel() const { return _shortcut; }
	basic2d::IconSprite *getTrailingIcon() const { return _trailing; }

protected:
	using Button::init;

	// Places the row's children from the stamped columns. Not a measurement: every number it uses
	// was decided for the whole menu already.
	virtual void layoutContent();

	// Creates the node the first time there is something to put in it, so a menu with no
	// accelerators and no trailing icons builds none of either.
	basic2d::Label *acquireSubtitle();
	basic2d::Label *acquireShortcut();
	basic2d::IconSprite *acquireTrailing();

	// Non-owning: the system outlives every node it built, and an Rc here would be a cycle.
	MenuSystem *_system = nullptr;
	Rc<MenuSourceButton> _item;

	basic2d::Label *_subtitle = nullptr;
	basic2d::Label *_shortcut = nullptr;
	basic2d::IconSprite *_trailing = nullptr;

	MenuStyle _style;
	float _leadingColumn = 0.0f;
	float _textColumn = 0.0f;
	float _shortcutColumn = 0.0f;
	float _trailingColumn = 0.0f;
	float _titleHeight = 0.0f;
	float _subtitleHeight = 0.0f;
};

/** The rule between two groups of commands.

A Node rather than a Panel, with the line as a child: the row occupies MenuStyle::separatorHeight
so that the groups are spaced, while the line itself is one or two points in the middle of it. One
node cannot be both.

CSS type "menu-separator" on the line; the row itself is "menu-separator-row" and paints nothing. */
class SP_PUBLIC MenuSeparator : public Node {
public:
	virtual ~MenuSeparator() = default;

	virtual bool init(NotNull<MenuSystem>, NotNull<MenuSourceItem>);

	virtual void handleContentSizeDirty() override;

	MenuSourceItem *getItem() const { return _item; }

	virtual void setThickness(float);
	float getThickness() const { return _thickness; }

	// Horizontal inset of the line inside the row.
	virtual void setInset(float);
	float getInset() const { return _inset; }

protected:
	MenuSystem *_system = nullptr;
	Rc<MenuSourceItem> _item;
	Panel *_line = nullptr;
	float _thickness = 1.0f;
	float _inset = 0.0f;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_MENU_XLUIMENUITEM_H_
