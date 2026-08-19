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
#include "XLUiLayoutSystem.h"
#include "XLUiInteractiveComponent.h"
#include "XLInputListener.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

// The fallback's metrics, in points. Only the fallback: a styled chip gets its LayoutSystem from
// `display:flex` and none of the placement below runs.
static constexpr float s_chipPaddingLeft = 8.0f;
static constexpr float s_chipPaddingRight = 4.0f;
static constexpr float s_chipGap = 4.0f;
static constexpr float s_chipVerticalPadding = 3.0f;
static constexpr float s_chipMinHeight = 24.0f;
static constexpr float s_chipIconSize = 16.0f;

static constexpr IconName s_chipRemoveIcon = IconName::Navigation_close_solid;
static constexpr float s_chipRemoveIconSize = 12.0f;

/* The remove button's fallback box.

The WIDTH is not a free choice: ui::Button's own fallback placement insets its icon by 8pt from the
left edge and centres it vertically, so a box of `icon + 2 * 8` is what puts equal air on both sides
of the glyph. Anything else draws the cross off-centre in the unstyled case. The height is free, and
is what keeps the button inside a chip rather than the other way round. */
static constexpr float s_chipRemoveWidth = s_chipRemoveIconSize + 16.0f;
static constexpr float s_chipRemoveHeight = 18.0f;

Chip::~Chip() { }

bool Chip::init() {
	if (!Badge::init()) {
		return false;
	}

	/* A chip IS a badge, and a stylesheet must not be able to tell. A rule for `badge` would
	otherwise paint every chip, and a rule for `chip` would have to know how the widget happens to
	be implemented - so the type, the class and the inherited label are all redeclared here. */
	setType("chip");
	removeStyleClass("xl-ui-badge");
	addStyleClass("xl-ui-chip");
	registerStyleAppliers("chip");

	if (_label) {
		_label->removeStyleClass("xl-ui-badge-label");
		_label->addStyleClass("xl-ui-chip-label");
		// Left, not the badge's Center: a chip's text starts after its icon and is followed by its
		// button, so there is no box to be centred in.
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
			if (!_enabled) {
				return false;
			}
			// The button is a child and answered first; without this guard a tap on the cross would
			// also read as a tap on the chip, and the row would select what it is about to remove.
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

	// One answer for two callers: ui::ChipRow wraps by it, and `flex-basis: fit-content` resolves
	// to it for a chip a stylesheet put in a flex container.
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

void Chip::handleContentSizeDirty() {
	Badge::handleContentSizeDirty();

	// A LayoutSystem - from `display:flex` or added by hand - owns the children's geometry, and the
	// placement below would be a second writer of the same positions. Same rule as ui::Select's.
	if (getSystemByType<LayoutSystem>()) {
		return;
	}

	const float height = _contentSize.height;
	const float width = _contentSize.width;
	if (height <= 0.0f || width <= 0.0f) {
		return;
	}

	float left = s_chipPaddingLeft;
	if (_icon && _icon->isVisible()) {
		_icon->setAnchorPoint(Anchor::MiddleLeft);
		_icon->setPosition(Vec2(left, height / 2.0f));
		left += _icon->getContentSize().width + s_chipGap;
	}

	float right = width - s_chipPaddingRight;
	if (_remove && _remove->isVisible()) {
		_remove->setContentSize(Size2(s_chipRemoveWidth, s_chipRemoveHeight));
		_remove->setAnchorPoint(Anchor::MiddleRight);
		_remove->setPosition(Vec2(right, height / 2.0f));
	}

	if (_label) {
		_label->setAnchorPoint(Anchor::MiddleLeft);
		_label->setPosition(Vec2(left, height / 2.0f));
		// Deliberately no setWidth: a constrained label reports the constrained width afterwards,
		// and measureNatural() would then answer with whatever the last placement squeezed it into.
		// A chip is as wide as its text; making it narrower is the ROW's business, not the box's.
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
	if (_enabled == value) {
		return;
	}
	_enabled = value;
	if (_enabled) {
		removeStyleClass("disabled");
	} else {
		addStyleClass("disabled");
	}
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
		// Shaped NOW: a Label measures zero until its own update runs, which is after this, and a
		// row that wrapped by that number would put every chip on the same line.
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
		bool dirty = state->updateState(_enabled ? (state->state | InteractiveState::Enabled)
												 : (state->state & ~InteractiveState::Enabled));
		// The counter is cumulative, so the flag is pushed on an edge and never twice.
		const bool hover = _hoverApplied && _enabled;
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
