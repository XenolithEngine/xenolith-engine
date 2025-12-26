/**
 Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#include "XLSimpleButton.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::simpleui {

bool Button::init(Function<void()> &&cb) {
	if (!Layer::init(Color::Grey_200)) {
		return false;
	}

	_callback = sp::move(cb);

	_listener = addSystem(Rc<InputListener>::create());
	_listener->addMouseOverRecognizer([this](const GestureData &data) {
		switch (data.event) {
		case GestureEvent::Began: handleFocusEnter(); break;
		case GestureEvent::Ended:
		case GestureEvent::Cancelled: handleFocusLeave(); break;
		default: break;
		}
		return true;
	}, false);
	_listener->addTouchRecognizer([this](const GestureData &ev) -> bool {
		if (ev.event == GestureEvent::Began) {
			if (isTouched(ev.input->currentLocation)) {
				_listener->setExclusiveForTouch(ev.getId());
				return true;
			} else {
				return false;
			}
		} else if (ev.event == GestureEvent::Ended) {
			if (isTouched(ev.input->currentLocation)) {
				handleTouch();
			}
		}
		return true;
	});

	_listener->setCursor(WindowCursor::Pointer);

	updateState();

	return true;
}

void Button::setEnabled(bool value) {
	if (isEnabled() != value) {
		_state |= InputNodeState::Enabled;
		updateState();
	}
}

void Button::setCallback(Function<void()> &&cb) { _callback = cb; }

void Button::handleFocusEnter() {
	stopAllActions();
	if (_callback) {
		runAction(Rc<TintTo>::create(0.2f, Color::Red_200));
	}
}

void Button::handleFocusLeave() {
	stopAllActions();
	if (_callback) {
		runAction(Rc<TintTo>::create(0.2f, isEnabled() ? Color::Grey_400 : Color::Grey_200));
	}
}

void Button::handleTouch() {
	if (_callback) {
		_callback();
	}
}

void Button::updateState() {
	auto enabled = isEnabled() && _callback != nullptr;
	if (!hasFlag(_state, InputNodeState::Hovered)) {
		if (_running) {
			stopAllActions();
			runAction(Rc<TintTo>::create(0.2f, enabled ? Color::Grey_400 : Color::Grey_200));
		} else {
			setColor(enabled ? Color::Grey_400 : Color::Grey_200);
		}
	}
}

bool ButtonWithLabel::init(StringView str, Function<void()> &&cb) {
	if (!Button::init(sp::move(cb))) {
		return false;
	}

	_label = addChild(Rc<Label>::create(), ZOrder(1));
	_label->setAnchorPoint(Anchor::Middle);
	_label->setAlignment(font::TextAlign::Center);
	_label->setFontSize(20);
	_label->setString(str);
	_label->setPersistentGlyphData(true);

	return true;
}

void ButtonWithLabel::handleContentSizeDirty() {
	Button::handleContentSizeDirty();

	_label->setPosition(_contentSize / 2.0f);
	_label->setWidth(_contentSize.width - 8.0f);
}

void ButtonWithLabel::setString(StringView str) { _label->setString(str); }

} // namespace stappler::xenolith::simpleui
