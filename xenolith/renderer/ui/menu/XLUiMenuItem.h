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

/** One command row: a Button (hover, press, `:hover`/`:active`/`:disabled`) plus a subtitle, an
accelerator, a trailing icon and the menu's shared column geometry.

Button's `_label` is the title and `_icon` the leading icon; the other nodes exist only while they
have something to show. The row does not measure itself: MenuSystem assigns size and columns for
the whole menu.

CSS type "menu-item"; children `label`, `menu-item-subtitle`, `menu-item-shortcut`, `icon`,
`menu-item-trailing`. Style classes: `checked` while the item is on, `disabled` from Button,
`submenu` when the row opens one. */
class SP_PUBLIC MenuItem : public Button {
public:
	virtual ~MenuItem() = default;

	virtual bool init(NotNull<MenuSystem>, NotNull<MenuSourceButton>);

	virtual void handleContentSizeDirty() override;

	// Hovering a row moves the keyboard highlight onto it, so there is one current row.
	virtual void handleComponentsDirty(const ComponentMask &) override;

	MenuSourceButton *getItem() const { return _item; }

	// Re-read everything from the model; setters are equality-guarded, so it is cheap.
	virtual void updateFromSource();

	/* The shared column geometry of the menu this row belongs to, plus this row's own wrapped text
	heights. Stamped by MenuSystem before it commits the row's size. */
	virtual void setRowGeometry(const MenuStyle &, const MenuMetrics &, const MenuMetrics::Row &);

	basic2d::Label *getSubtitleLabel() const { return _subtitle; }
	basic2d::Label *getShortcutLabel() const { return _shortcut; }
	basic2d::IconSprite *getTrailingIcon() const { return _trailing; }

protected:
	using Button::init;

	// Places the children from the assigned columns; measures nothing.
	virtual void layoutContent();

	// Created on first use, so rows without these build no nodes.
	basic2d::Label *acquireSubtitle();
	basic2d::Label *acquireShortcut();
	basic2d::IconSprite *acquireTrailing();

	// non-owning: the system outlives the nodes it built; an Rc would be a cycle
	MenuSystem *_system = nullptr;
	Rc<MenuSourceButton> _item;

	basic2d::Label *_subtitle = nullptr;
	basic2d::Label *_shortcut = nullptr;
	basic2d::IconSprite *_trailing = nullptr;

	// Edge tracker: the hover component is reported on every dirty pass, so only transitions
	// are acted on.
	bool _hoverApplied = false;

	MenuStyle _style;
	float _leadingColumn = 0.0f;
	float _textColumn = 0.0f;
	float _shortcutColumn = 0.0f;
	float _trailingColumn = 0.0f;
	float _titleHeight = 0.0f;
	float _subtitleHeight = 0.0f;
};

/** The rule between two groups of commands.

A Node with the line as a child: the row takes MenuStyle::separatorHeight while the line is a
thin panel centred in it.

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
