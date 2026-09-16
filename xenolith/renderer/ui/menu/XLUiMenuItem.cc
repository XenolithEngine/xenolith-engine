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

#include "XLUiMenuItem.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

// Mark for a checked item without its own leading icon; uses the leading column.
static constexpr IconName s_menuCheckIcon = IconName::Navigation_check_solid;

// Trailing mark of a submenu row that declares no trailing icon.
static constexpr IconName s_menuSubmenuIcon = IconName::Navigation_chevron_right_solid;

/* Text colour for a menu with no stylesheet in scope (no ui::StyleSystem at all): the default
surface is dark and a Label's default ink is black. An inherited CSS `color` still overrides it. */
static constexpr Color4F s_menuTextColor = Color4F(0.91f, 0.91f, 0.93f, 1.0f);
static constexpr Color4F s_menuSecondaryTextColor = Color4F(0.60f, 0.60f, 0.64f, 1.0f);

// --- MenuItem ----------------------------------------------------------------------------------

bool MenuItem::init(NotNull<MenuSystem> system, NotNull<MenuSourceButton> item) {
	if (!Button::init(ButtonType::General, nullptr)) {
		return false;
	}

	_system = system;
	_item = item;

	// own CSS type with the surface appliers, so rules need not say `button.menu-item`
	setType("menu-item");
	removeStyleClass("xl-ui-button");
	addStyleClass("xl-ui-menu-item");
	registerStyleAppliers("menu-item");

	if (_label) {
		_label->removeStyleClass("xl-ui-button-label");
		_label->addStyleClass("xl-ui-menu-item-label");
		// menu text does not change while open, so keep the shaped glyphs
		_label->setPersistentGlyphData(true);
		_label->setAlignment(font::TextAlign::Left);
		_label->setColor(s_menuTextColor, false);
	}

	if (_icon) {
		_icon->removeStyleClass("xl-ui-button-icon");
		_icon->addStyleClass("xl-ui-menu-item-icon");
		_icon->setColor(s_menuTextColor, false);
	}

	// activation goes through Button, whose `_enabled` gate keeps disabled rows from the system
	setCallback([this] {
		if (_system && _item) {
			_system->handleItemActivated(_item);
		}
	});

	updateFromSource();
	return true;
}

void MenuItem::updateFromSource() {
	if (!_item) {
		return;
	}

	// the name is the CSS id, and what tests address the row by
	setName(_item->getName());

	setString(_item->getTitle());
	setEnabled(_item->isEnabled());

	applyControlChecked(this, _item->isChecked());

	const auto leading = _item->getLeadingIcon();
	// a checked item with no icon of its own shows the mark; one with an icon keeps it
	setIcon(leading != IconName::None ? leading
									  : (_item->isChecked() ? s_menuCheckIcon : IconName::None));

	const bool submenu = _item->hasSubmenu();
	if (submenu) {
		addStyleClass("submenu");
	} else {
		removeStyleClass("submenu");
	}

	auto trailing = _item->getTrailingIcon();
	if (trailing == IconName::None && submenu) {
		trailing = s_menuSubmenuIcon;
	}
	if (trailing != IconName::None) {
		acquireTrailing()->setIconName(trailing);
		_trailing->setVisible(true);
	} else if (_trailing) {
		_trailing->setVisible(false);
	}

	auto subtitle = _item->getSubtitle();
	if (!subtitle.empty()) {
		acquireSubtitle()->setString(subtitle);
		_subtitle->setVisible(true);
	} else if (_subtitle) {
		_subtitle->setVisible(false);
	}

	if (_item->hasShortcut()) {
		StringStream text;
		_item->encodeShortcut([&](StringView str) { text << str; });
		acquireShortcut()->setString(text.str());
		_shortcut->setVisible(true);
	} else if (_shortcut) {
		_shortcut->setVisible(false);
	}
}

void MenuItem::setRowGeometry(const MenuStyle &style, const MenuMetrics &metrics,
		const MenuMetrics::Row &row) {
	_style = style;
	_leadingColumn = metrics.leadingColumn;
	_textColumn = metrics.textColumn;
	_shortcutColumn = metrics.shortcutColumn;
	_trailingColumn = metrics.trailingColumn;
	_titleHeight = row.titleHeight;
	_subtitleHeight = row.subtitleHeight;

	if (_label) {
		_label->setFontSize(style.fontSize);
	}
	if (_subtitle) {
		_subtitle->setFontSize(style.subtitleFontSize);
	}
	if (_shortcut) {
		_shortcut->setFontSize(style.shortcutFontSize);
	}
	if (_icon) {
		_icon->setContentSize(Size2(style.iconSize, style.iconSize));
	}
	if (_trailing) {
		_trailing->setContentSize(Size2(style.iconSize, style.iconSize));
	}

	// the size may be unchanged (e.g. only the checked flag flipped), so handleContentSizeDirty may
	// not run; place the children here too
	layoutContent();
}

void MenuItem::handleContentSizeDirty() {
	// not Button's: its fallback centering would also write the positions this row owns
	Panel::handleContentSizeDirty();
	layoutContent();
}

void MenuItem::handleComponentsDirty(const ComponentMask &mask) {
	Button::handleComponentsDirty(mask);

	if (!mask.contains(InteractiveComponent::Id.value)) {
		return;
	}

	bool hovered = false;
	if (auto ic = getComponent<InteractiveComponent>()) {
		hovered = ic->hoverCounter > 0;
	}
	if (hovered == _hoverApplied) {
		return;
	}
	_hoverApplied = hovered;

	// entering edge only: leaving a row keeps the highlight, so the keyboard can continue from it
	if (hovered && _system && _item) {
		_system->handleItemHovered(_item);
	}
}

void MenuItem::layoutContent() {
	const float height = _contentSize.height;
	if (height <= 0.0f || _contentSize.width <= 0.0f) {
		return;
	}

	float x = _style.paddingHorizontal;

	if (_leadingColumn > 0.0f) {
		if (_icon && _icon->isVisible()) {
			_icon->setAnchorPoint(Anchor::MiddleLeft);
			_icon->setPosition(Vec2(x, height / 2.0f));
		}
		// reserved even when this row leaves it empty, so titles line up
		x += _leadingColumn + _style.gap;
	}

	// title and subtitle are centered vertically as one block
	const float textHeight = _titleHeight + _subtitleHeight;
	float top = height - (height - textHeight) / 2.0f;

	if (_label && _label->isVisible()) {
		_label->setAnchorPoint(Anchor::TopLeft);
		_label->setPosition(Vec2(x, top));
		// wrap at the same width the metrics used
		_label->setWidth(_style.wrapTitle ? _textColumn : 0.0f);
		if (!_style.wrapTitle) {
			_label->setMaxWidth(_textColumn);
		}
		top -= _titleHeight;
	}

	if (_subtitle && _subtitle->isVisible()) {
		_subtitle->setAnchorPoint(Anchor::TopLeft);
		_subtitle->setPosition(Vec2(x, top));
		_subtitle->setWidth(_style.wrapSubtitle ? _textColumn : 0.0f);
		if (!_style.wrapSubtitle) {
			_subtitle->setMaxWidth(_textColumn);
		}
	}

	x += _textColumn + _style.gap;

	if (_shortcutColumn > 0.0f) {
		if (_shortcut && _shortcut->isVisible()) {
			// right-aligned in its column so accelerators line up; never wraps
			_shortcut->setAnchorPoint(Anchor::MiddleRight);
			_shortcut->setPosition(Vec2(x + _shortcutColumn, height / 2.0f));
		}
		x += _shortcutColumn + _style.gap;
	}

	if (_trailingColumn > 0.0f && _trailing && _trailing->isVisible()) {
		_trailing->setAnchorPoint(Anchor::MiddleRight);
		_trailing->setPosition(Vec2(_contentSize.width - _style.paddingHorizontal, height / 2.0f));
	}
}

basic2d::Label *MenuItem::acquireSubtitle() {
	if (!_subtitle) {
		_subtitle = addChild(Rc<basic2d::Label>::create(), ZOrder(1));
		_subtitle->setType("menu-item-subtitle");
		_subtitle->addStyleClass("xl-ui-menu-item-subtitle");
		_subtitle->setPersistentGlyphData(true);
		_subtitle->setAlignment(font::TextAlign::Left);
		_subtitle->setColor(s_menuSecondaryTextColor, false);
		_subtitle->setFontSize(_style.subtitleFontSize);
	}
	return _subtitle;
}

basic2d::Label *MenuItem::acquireShortcut() {
	if (!_shortcut) {
		_shortcut = addChild(Rc<basic2d::Label>::create(), ZOrder(3));
		_shortcut->setType("menu-item-shortcut");
		_shortcut->addStyleClass("xl-ui-menu-item-shortcut");
		_shortcut->setPersistentGlyphData(true);
		_shortcut->setAlignment(font::TextAlign::Right);
		_shortcut->setColor(s_menuSecondaryTextColor, false);
		_shortcut->setFontSize(_style.shortcutFontSize);
	}
	return _shortcut;
}

basic2d::IconSprite *MenuItem::acquireTrailing() {
	if (!_trailing) {
		_trailing = addChild(Rc<basic2d::IconSprite>::create(), ZOrder(4));
		_trailing->setType("menu-item-trailing");
		_trailing->addStyleClass("xl-ui-menu-item-trailing");
		_trailing->setColor(s_menuSecondaryTextColor, false);
		_trailing->setContentSize(Size2(_style.iconSize, _style.iconSize));
	}
	return _trailing;
}

// --- MenuSeparator -----------------------------------------------------------------------------

bool MenuSeparator::init(NotNull<MenuSystem> system, NotNull<MenuSourceItem> item) {
	if (!Node::init()) {
		return false;
	}

	_system = system;
	_item = item;

	setType("menu-separator-row");
	setName(item->getName());

	_line = addChild(Rc<Panel>::create());
	_line->setType("menu-separator");
	_line->addStyleClass("xl-ui-menu-separator");
	_line->setAnchorPoint(Anchor::MiddleLeft);
	// an unstyled Panel is opaque white; neutral grey until a stylesheet sets it
	_line->setPathColor(Color4B(128, 128, 128, 96), false);

	return true;
}

void MenuSeparator::handleContentSizeDirty() {
	Node::handleContentSizeDirty();

	if (_line) {
		_line->setPosition(Vec2(_inset, _contentSize.height / 2.0f));
		_line->setContentSize(
				Size2(sprt::max(_contentSize.width - _inset * 2.0f, 0.0f), _thickness));
	}
}

void MenuSeparator::setThickness(float value) {
	if (_thickness == value) {
		return;
	}
	_thickness = value;
	handleContentSizeDirty();
}

void MenuSeparator::setInset(float value) {
	if (_inset == value) {
		return;
	}
	_inset = value;
	handleContentSizeDirty();
}

} // namespace stappler::xenolith::ui
