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
		if (!_enabled) {
			return false;
		}
		if (tap.event == GestureEvent::Activated) {
			setChecked(!_checked);
		}
		return true;
	}, InputTapInfo{makeButtonMask({InputMouseButton::Touch, InputMouseButton::MouseLeft}), 1});

	return true;
}

void Checkbox::setChecked(bool c, bool silent) {
	if (_checked == c) {
		return;
	}
	_checked = c;
	_check->setVisible(c);
	if (c) {
		addStyleClass("checked");
	} else {
		removeStyleClass("checked");
	}
	if (!silent && _callback) {
		_callback(c);
	}
}

void Checkbox::setEnabled(bool e) {
	if (_enabled == e) {
		return;
	}
	_enabled = e;
	if (e) {
		removeStyleClass("disabled");
	} else {
		addStyleClass("disabled");
	}
	setOpacity(e ? 1.0f : 0.4f);
}

} // namespace stappler::xenolith::ui
