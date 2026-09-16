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

#include "XLUiChip.h"

#include "XLInheritedStyle.h" // placeInline*: the row follows the inline direction
#include "XLUiLayoutSystem.h"
#include "XLInteractiveComponent.h"
#include "XLInputListener.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

// Fallback metrics in points, used only without a LayoutSystem (no `display:flex`).
static constexpr float s_chipPaddingLeft = 8.0f;
static constexpr float s_chipPaddingRight = 4.0f;
static constexpr float s_chipGap = 4.0f;
static constexpr float s_chipVerticalPadding = 3.0f;
static constexpr float s_chipMinHeight = 24.0f;
static constexpr float s_chipIconSize = 16.0f;

static constexpr IconName s_chipRemoveIcon = IconName::Navigation_close_solid;
static constexpr float s_chipRemoveIconSize = 12.0f;

/* The remove button's fallback box. The width is `icon + 2 * 8` because ui::Button's fallback
insets its icon by 8pt; any other width draws the cross off-centre. */
static constexpr float s_chipRemoveWidth = s_chipRemoveIconSize + 16.0f;
static constexpr float s_chipRemoveHeight = 18.0f;

Chip::~Chip() { }

bool Chip::init() {
	if (!Badge::init()) {
		return false;
	}

	/* The InteractiveComponent must exist before anything reads isEnabled(): a node without one
	reads as state 0, which matches `:disabled`. */
	applyControlEnabled(this, true);

	// Retype the node and the inherited label so `badge` rules do not apply to chips.
	setType("chip");
	removeStyleClass("xl-ui-badge");
	addStyleClass("xl-ui-chip");
	registerStyleAppliers("chip");

	if (_label) {
		_label->removeStyleClass("xl-ui-badge-label");
		_label->addStyleClass("xl-ui-chip-label");
		// left-aligned: the text sits between the icon and the remove button
		_label->setAlignment(font::TextAlign::Left);
	}

	_icon = addChild(Rc<basic2d::IconSprite>::create(), ZOrder(1));
	_icon->setType("icon");
	_icon->addStyleClass("xl-ui-chip-icon");
	_icon->setContentSize(Size2(s_chipIconSize, s_chipIconSize));
	_icon->setVisible(false);

	_remove = addChild(Rc<Button>::create([this] {
		if (_removeCallback) {
			_removeCallback(this);
		}
	}),
			ZOrder(2));
	_remove->setType("button");
	_remove->setName("remove");
	_remove->addStyleClass("xl-ui-chip-remove");
	_remove->setIcon(s_chipRemoveIcon);
	if (auto glyph = _remove->getIconSprite()) {
		glyph->setContentSize(Size2(s_chipRemoveIconSize, s_chipRemoveIconSize));
	}

	_listener = addSystem(Rc<InputListener>::create());

	_listener->addTapRecognizer([this](const GestureTap &tap) {
		if (tap.event == GestureEvent::Activated) {
			if (!isEnabled()) {
				return false;
			}
			// a tap on the remove button is not a tap on the chip
			if (isOverRemoveButton(tap.location())) {
				return false;
			}
			if (_tapCallback) {
				_tapCallback(this);
			}
			return true;
		}
		return true;
	}, InputTapInfo{makeButtonMask({InputMouseButton::Touch, InputMouseButton::MouseLeft}), 1});

	_listener->addMouseOverRecognizer([this](const GestureData &data) {
		switch (data.event) {
		case GestureEvent::Began: _hoverApplied = true; break;
		case GestureEvent::Ended:
		case GestureEvent::Cancelled: _hoverApplied = false; break;
		default: break;
		}
		updateInteractiveState();
		return true;
	}, false);

	// used by ui::ChipRow wrapping and by `flex-basis: fit-content` in a flex container
	setMeasureCallback([this](const MeasureConstraints &c, Size2 &result) {
		result = measureNatural();
		// MaxContent means "do not wrap at all", so it is the one mode that ignores the constraint.
		if (c.mode != MeasureMode::MaxContent && c.maxWidth != maxOf<float>()) {
			result.width = sprt::min(result.width, c.maxWidth);
		}
		return true;
	});

	updateInteractiveState();


	return true;
}

void Chip::handleContentSizeDirty() { Badge::handleContentSizeDirty(); }

void Chip::handleLayoutChildren() {
	Badge::handleLayoutChildren();
	placeInlineParts();
}

void Chip::placeInlineParts() {
	// a LayoutSystem owns the children's geometry when present
	if (getSystemByType<LayoutSystem>()) {
		return;
	}

	const float height = _contentSize.height;
	const float width = _contentSize.width;
	if (height <= 0.0f || width <= 0.0f) {
		return;
	}

	/* Runs from handleLayoutChildren: the resolved direction is not yet current in
	handleContentSizeDirty. The icon leads and the remove button trails, from the inline edges. */
	const bool rtl = isInlineRtl(this);

	float startInset = s_chipPaddingLeft;
	if (_icon && _icon->isVisible()) {
		placeInlineStart(_icon, startInset, height / 2.0f, width, rtl);
		startInset += _icon->getContentSize().width + s_chipGap;
	}

	if (_remove && _remove->isVisible()) {
		_remove->setContentSize(Size2(s_chipRemoveWidth, s_chipRemoveHeight));
		placeInlineEnd(_remove, s_chipPaddingRight, height / 2.0f, width, rtl);
	}

	if (_label) {
		placeInlineStart(_label, startInset, height / 2.0f, width, rtl);
		_label->setAlignment(inlineStartAlign(rtl));
		// No setWidth: a constrained label would report that width back to measureNatural().
	}
}

void Chip::setIcon(IconName name) {
	if (!_icon || _icon->getIconName() == name) {
		return;
	}
	_icon->setIconName(name);
	_icon->setVisible(name != IconName::None);
	_contentSizeDirty = true;
	markMeasureDirty();
}

IconName Chip::getIcon() const { return _icon ? _icon->getIconName() : IconName::None; }

void Chip::setRemovable(bool value) {
	if (_removable == value) {
		return;
	}
	_removable = value;
	if (_remove) {
		_remove->setVisible(value);
	}
	_contentSizeDirty = true;
	markMeasureDirty();
}

void Chip::setRemoveCallback(Callback &&cb) { _removeCallback = sp::move(cb); }

void Chip::setTapCallback(Callback &&cb) { _tapCallback = sp::move(cb); }

void Chip::setSelected(bool value) {
	if (_selected == value) {
		return;
	}
	_selected = value;
	if (_selected) {
		addStyleClass("selected");
	} else {
		removeStyleClass("selected");
	}
}

void Chip::setEnabled(bool value) {
	// the edit lock has the last word and remembers the requested value for unlocking
	value = resolveEditLock(this, value);
	if (isEnabled() == value) {
		return;
	}
	applyControlEnabled(this, value);
	if (_remove) {
		_remove->setEnabled(value);
	}
	updateInteractiveState();
}

Size2 Chip::measureNatural() const {
	float width = s_chipPaddingLeft + s_chipPaddingRight;
	float height = s_chipMinHeight;

	if (_icon && _icon->isVisible()) {
		width += _icon->getContentSize().width + s_chipGap;
		height = sprt::max(height, _icon->getContentSize().height + s_chipVerticalPadding * 2.0f);
	}

	if (_label && _label->isVisible()) {
		// shape now: a Label measures zero until its own update runs
		_label->tryUpdateLabel();
		width += _label->getContentSize().width;
		height = sprt::max(height, _label->getContentSize().height + s_chipVerticalPadding * 2.0f);
	}

	if (_remove && _remove->isVisible()) {
		width += s_chipGap + s_chipRemoveWidth;
		height = sprt::max(height, s_chipRemoveHeight + s_chipVerticalPadding * 2.0f);
	}

	return Size2(width, height);
}

void Chip::updateInteractiveState() {
	setOrUpdateComponent<InteractiveComponent>([this](NotNull<InteractiveComponent> state) {
		// The Enabled bit is written by applyControlEnabled, from setEnabled.
		bool dirty = false;
		// The counter is cumulative, so the flag is pushed on an edge and never twice.
		const bool hover = _hoverApplied && sprt::hasFlag(state->state, InteractiveState::Enabled);
		if (hover != sprt::hasFlag(state->state, InteractiveState::Hover)) {
			dirty = state->handleHover(hover ? 1 : -1) || dirty;
		}
		return dirty;
	});
}

bool Chip::isOverRemoveButton(const Vec2 &location) const {
	return _remove && _remove->isVisible() && _remove->isEnabled()
			&& const_cast<Button *>(_remove)->isTouched(location, 0.0f);
}

} // namespace stappler::xenolith::ui
