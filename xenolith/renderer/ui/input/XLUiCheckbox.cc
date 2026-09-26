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

#include "XLUiCheckbox.h"
#include "XL2dIconSprite.h" // IWYU pragma: keep
#include "XLUiLayoutSystem.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

Checkbox::~Checkbox() { }

bool Checkbox::init() {
	if (!Panel::init()) {
		return false;
	}
	setType("checkbox");
	removeStyleClass("xl-ui-panel");
	addStyleClass("xl-ui-checkbox");
	// the same fill / outline / border-radius appliers Panel registers for itself, under "checkbox"
	registerStyleAppliers("checkbox");

	_check = addChild(Rc<basic2d::IconSprite>::create(), ZOrder(1));
	_check->setType("icon");
	_check->setIconName(basic2d::IconName::Navigation_check_solid);
	_check->setColor(Color4F(0.10f, 0.10f, 0.10f, 1.0f)); // dark check on accent fill
	_check->setVisible(false);

	_listener = addSystem(Rc<InputListener>::create());
	_listener->addTapRecognizer([this](const GestureTap &tap) {
		if (!isEnabled()) {
			return false;
		}
		if (tap.event == GestureEvent::Activated) {
			setChecked(!isChecked());
		}
		return true;
	}, InputTapInfo{makeButtonMask({InputMouseButton::Touch, InputMouseButton::MouseLeft}), 1});

	/* The InteractiveComponent must exist from the first frame: a node without one reads as state 0
	to the style resolver, which matches `checkbox:disabled`. */
	applyControlEnabled(this, true);
	applyControlChecked(this, false);

	return true;
}

void Checkbox::setChecked(bool c, bool silent) {
	if (isChecked() == c) {
		return;
	}
	applyControlChecked(this, c);
	_check->setVisible(c);
	if (!silent && _callback) {
		_callback(c);
	}
}

void Checkbox::handleLayoutChildren() {
	Panel::handleLayoutChildren();
	placeCheck();
}

void Checkbox::placeCheck() {
	// A LayoutSystem (from `display:flex`) owns the children's geometry.
	if (!_check || getSystemByType<LayoutSystem>()) {
		return;
	}

	const float side = sprt::min(_contentSize.width, _contentSize.height);
	if (side <= 0.0f) {
		return;
	}

	// A size the stylesheet gave the icon is kept while it fits the box.
	const auto size = _check->getContentSize();
	const float icon = (size.width > 0.0f && size.width <= side) ? size.width : side;
	_check->setAnchorPoint(Anchor::Middle);
	_check->setContentSize(Size2(icon, icon));
	_check->setPosition(Vec2(_contentSize.width * 0.5f, _contentSize.height * 0.5f));
}

void Checkbox::setEnabled(bool e) {
	// the edit lock has the last word and remembers the requested value for unlocking
	e = resolveEditLock(this, e);
	if (isEnabled() == e) {
		return;
	}
	applyControlEnabled(this, e);
	// visible even when the stylesheet has no `:disabled` rule
	setOpacity(e ? 1.0f : 0.4f);
}

} // namespace stappler::xenolith::ui
