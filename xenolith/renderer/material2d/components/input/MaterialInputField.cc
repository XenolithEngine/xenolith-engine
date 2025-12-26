/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
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

#include "MaterialInputField.h"
#include "MaterialInputTextContainer.h"
#include "MaterialEasing.h"
#include "XLDirector.h"
#include "XL2dLayer.h"
#include "XLInputListener.h"
#include "XLCoreTextInput.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::material2d {

InputField::~InputField() { }

bool InputField::init(InputFieldStyle fieldStyle) {
	SurfaceStyle style;
	switch (fieldStyle) {
	case InputFieldStyle::Filled:
		style.nodeStyle = NodeStyle::Filled;
		style.colorRole = ColorRole::SurfaceVariant;
		break;
	case InputFieldStyle::Outlined:
		style.nodeStyle = NodeStyle::Outlined;
		style.shapeStyle = ShapeStyle::ExtraSmall;
		break;
	}

	return init(fieldStyle, style);
}

bool InputField::init(InputFieldStyle fieldStyle, const SurfaceStyle &surfaceStyle) {
	if (!Surface::init(surfaceStyle)) {
		return false;
	}

	_container = addChild(Rc<InputTextContainer>::create(), ZOrder(1));
	_container->setAnchorPoint(Anchor::BottomLeft);

	_labelText = addChild(Rc<TypescaleLabel>::create(TypescaleRole::BodyLarge), ZOrder(1));
	_labelText->setAnchorPoint(Anchor::MiddleLeft);

	_supportingText = addChild(Rc<TypescaleLabel>::create(TypescaleRole::BodySmall), ZOrder(1));
	_supportingText->setAnchorPoint(Anchor::TopLeft);

	_leadingIcon = addChild(Rc<IconSprite>::create(IconName::None), ZOrder(1));
	_leadingIcon->setAnchorPoint(Anchor::MiddleLeft);
	_leadingIcon->setContentSize(Size2(24.0f, 24.0f));

	_trailingIcon = addChild(Rc<IconSprite>::create(IconName::None), ZOrder(1));
	_trailingIcon->setAnchorPoint(Anchor::MiddleRight);
	_trailingIcon->setContentSize(Size2(24.0f, 24.0f));

	_indicator = addChild(
			Rc<Surface>::create(SurfaceStyle(ColorRole::OnSurfaceVariant, NodeStyle::Filled)),
			ZOrder(1));
	_indicator->setAnchorPoint(Anchor::BottomLeft);

	_inputListener = addSystem(Rc<InputListener>::create());

	_inputListener->setTouchFilter(
			[this](const InputEvent &event, const InputListener::DefaultEventFilter &cb) {
		if (cb(event)) {
			return true;
		}

		if (_container->getTouchedCursor(event.currentLocation)) {
			return true;
		}

		return false;
	});

	_inputListener->addMouseOverRecognizer([this](const GestureData &data) {
		_mouseOver = (data.event == GestureEvent::Began);
		updateActivityState();
		return true;
	});
	_inputListener->addTapRecognizer([this](const GestureTap &tap) {
		return handleTap(tap.input->currentLocation);
	}, InputListener::makeButtonMask({InputMouseButton::Touch}), 1);

	_inputListener->addPressRecognizer([this](const GesturePress &press) {
		switch (press.event) {
		case GestureEvent::Began: return handlePressBegin(press.location()); break;
		case GestureEvent::Activated:
			return handleLongPress(press.location(), press.tickCount);
			break;
		case GestureEvent::Ended: return handlePressEnd(press.location()); break;
		case GestureEvent::Cancelled: return handlePressCancel(press.location()); break;
		}
		return false;
	}, TimeInterval::milliseconds(425), true);

	_inputListener->addSwipeRecognizer([this](const GestureSwipe &swipe) {
		switch (swipe.event) {
		case GestureEvent::Began:
			if (handleSwipeBegin(swipe.input->originalLocation, swipe.delta / swipe.density)) {
				return handleSwipe(swipe.input->originalLocation, swipe.delta / swipe.density,
						swipe.velocity / swipe.density);
			}
			return false;
			break;
		case GestureEvent::Activated:
			return handleSwipe(swipe.location(), swipe.delta / swipe.density,
					swipe.velocity / swipe.density);
			break;
		case GestureEvent::Ended:
		case GestureEvent::Cancelled: return handleSwipeEnd(swipe.velocity / swipe.density); break;
		}
		return false;
	});

	_focusInputListener = addSystem(Rc<InputListener>::create());
	_focusInputListener->setPriority(1);
	_focusInputListener->addTapRecognizer([this](const GestureTap &tap) {
		if (_handler.isActive()) {
			_handler.cancel();
		}
		_focusInputListener->setEnabled(false);
		return true;
	}, InputListener::makeButtonMask({InputMouseButton::Touch}), 1);
	_focusInputListener->setTouchFilter(
			[this](const InputEvent &event, const InputListener::DefaultEventFilter &cb) {
		if (!_container->isTouched(event.currentLocation, 8.0f)) {
			return true;
		}
		return false;
	});
	_focusInputListener->setEnabled(false);

	_handler.onData = std::bind(&InputField::handleTextInput, this, std::placeholders::_1);

	return true;
}

void InputField::handleEnter(Scene *scene) { Surface::handleEnter(scene); }

void InputField::handleExit() {
	Surface::handleExit();
	_container->setCursorCallback(nullptr);
}

void InputField::handleContentSizeDirty() {
	Surface::handleContentSizeDirty();

	_supportingText->setPosition(Vec2(16.0f, -4.0f));
	_supportingText->setWidth(_contentSize.width - 32.0f);

	_leadingIcon->setPosition(Vec2(12.0f, _contentSize.height / 2.0f));
	_trailingIcon->setPosition(Vec2(_contentSize.width - 12.0f, _contentSize.height / 2.0f));

	float xOffset = 16.0f;
	float containerWidth = _contentSize.width - 16.0f * 2.0f;

	if (getLeadingIconName() != IconName::None) {
		xOffset += _leadingIcon->getContentSize().width + 12.0f;
		containerWidth -= _leadingIcon->getContentSize().width + 12.0f;
	}

	if (getTrailingIconName() != IconName::None) {
		containerWidth -= _trailingIcon->getContentSize().width + 12.0f;
	}

	_container->setContentSize(Size2(containerWidth, _contentSize.height - 32.0f));
	_container->setPosition(Vec2(xOffset, 10.0f));

	if (_focused) {
		_labelText->setAnchorPoint(Anchor::TopLeft);
		_labelText->setPosition(Vec2(xOffset, _contentSize.height - 9.0f));
		_indicator->setContentSize(Size2(_contentSize.width, 2.0f));
	} else {
		if (!_inputState.empty()) {
			_labelText->setAnchorPoint(Anchor::TopLeft);
			_labelText->setPosition(Vec2(xOffset, _contentSize.height - 9.0f));
		} else {
			_labelText->setAnchorPoint(Anchor::MiddleLeft);
			_labelText->setPosition(Vec2(xOffset, _contentSize.height / 2.0f));
		}
		_indicator->setContentSize(Size2(_contentSize.width, 1.0f));
	}

	stopAllActionsByTag(InputEnabledActionTag);
}

void InputField::setLabelText(StringView text) { _labelText->setString(text); }

StringView InputField::getLabelText() const { return _labelText->getString8(); }

void InputField::setSupportingText(StringView text) { _supportingText->setString(text); }

StringView InputField::getSupportingText() const { return _supportingText->getString8(); }

void InputField::setLeadingIconName(IconName name) {
	if (name != getLeadingIconName()) {
		_leadingIcon->setIconName(name);
		_contentSizeDirty = true;
	}
}

IconName InputField::getLeadingIconName() const { return _leadingIcon->getIconName(); }

void InputField::setTrailingIconName(IconName name) {
	if (name != getTrailingIconName()) {
		_trailingIcon->setIconName(name);
		_contentSizeDirty = true;
	}
}

IconName InputField::getTrailingIconName() const { return _trailingIcon->getIconName(); }

void InputField::setEnabled(bool value) {
	if (_enabled != value) {
		_enabled = value;
		updateActivityState();
	}
}

bool InputField::isEnabled() const { return _enabled; }

void InputField::setInputType(TextInputType type) {
	if (_inputState.type != type) {
		_inputState.type = type;
		if (_handler.isActive()) {
			_handler.update(TextInputRequest{
				.string = _inputState.string,
				.cursor = _inputState.cursor,
				.marked = _inputState.marked,
				.type = _inputState.type,
			});
		}
	}
}

void InputField::setPasswordMode(InputFieldPasswordMode mode) { _passwordMode = mode; }

void InputField::updateActivityState() {
	auto style = getStyleTarget();
	if (!_enabled) {
		style.activityState = ActivityState::Disabled;
	} else if (_focused) {
		style.activityState = ActivityState::Enabled;
	} else if (_mouseOver) {
		style.activityState = ActivityState::Hovered;
	} else {
		style.activityState = ActivityState::Enabled;
	}
	setStyle(style, _activityAnimationDuration);
}

bool InputField::handleTap(const Vec2 &pt) {
	if (!isEnabled()) {
		return false;
	}

	return true;
}

bool InputField::handlePressBegin(const Vec2 &pt) {
	if (!isEnabled()) {
		return false;
	}

	if (_leadingIcon && getLeadingIconName() != IconName::None && _leadingIcon->isTouched(pt, 12)) {
		return false;
	}

	if (_trailingIcon && getTrailingIconName() != IconName::None
			&& _trailingIcon->isTouched(pt, 12)) {
		return false;
	}

	_inputListener->setExclusive();
	_isLongPress = false;
	return true;
}

bool InputField::handleLongPress(const Vec2 &pt, uint32_t tickCount) {
	if (!isEnabled() || !_rangeSelectionAllowed) {
		return false;
	}

	if (_container->handleLongPress(pt, tickCount)) {
		_isLongPress = true;
		if (!_focused) {
			acquireInputFromContainer();
		}
		return true;
	}
	return false;
}

bool InputField::handlePressEnd(const Vec2 &pt) {
	if (_container->isTouched(pt, 8.0f)) {
		if (!_isLongPress) {
			if (!_focused) {
				acquireInput(pt);
			} else {
				updateCursorForLocation(pt);
			}
		}
	}
	_isLongPress = false;
	return true;
}

bool InputField::handlePressCancel(const Vec2 &) {
	_isLongPress = false;
	return false;
}

bool InputField::handleSwipeBegin(const Vec2 &pt, const Vec2 &delta) {
	if (!isEnabled()) {
		return false;
	}

	if (_focused) {
		if (_container->handleSwipeBegin(pt)) {
			_pointerSwipeCaptured = true;
			return true;
		}
	}

	if (_container->hasHorizontalOverflow() && _container->isTouched(pt, 8.0f)) {
		_inputListener->setExclusive();
		_containerSwipeCaptured = true;
		return true;
	}

	return false;
}

bool InputField::handleSwipe(const Vec2 &pt, const Vec2 &delta, const Vec2 &v) {
	if (_pointerSwipeCaptured) {
		return _container->handleSwipe(pt, delta);
	}

	if (_containerSwipeCaptured) {
		_container->moveHorizontalOverflow(delta.x);
		return true;
	}

	return false;
}

bool InputField::handleSwipeEnd(const Vec2 &pt) {
	if (_pointerSwipeCaptured) {
		auto ret = _container->handleSwipeEnd(pt);
		_pointerSwipeCaptured = false;
		return ret;
	}

	if (_containerSwipeCaptured) {
		_containerSwipeCaptured = false;
		return true;
	}
	return false;
}

void InputField::updateInputEnabled() {
	if (!_running) {
		_contentSizeDirty = true;
		return;
	}

	stopAllActionsByTag(InputEnabledActionTag);

	bool populated = (!_inputState.empty());

	auto labelAnchor = _labelText->getAnchorPoint();
	auto labelPos = _labelText->getPosition();
	auto indicatorSize = _indicator->getContentSize();

	auto targetLabelAnchor = Anchor::MiddleLeft;
	auto targetLabelPos = Vec3(labelPos.x, _contentSize.height / 2.0f, 0.0f);
	auto targetIndicatorSize = Size2(indicatorSize.width, _focused ? 2.0f : 1.0f);

	auto sourceLabelSize = _labelText->getFontSize();
	auto targetLabelSize = font::FontSize(16);

	auto sourceBlendValue = _labelText->getBlendColorValue();
	auto targetBlendValue = _focused ? 1.0f : 0.0f;

	if (populated || _focused) {
		targetLabelAnchor = Anchor::TopLeft;
		targetLabelPos = Vec3(labelPos.x, _contentSize.height - 9.0f, 0.0f);
		targetLabelSize = font::FontSize(12);
	}

	runAction(makeEasing(Rc<ActionProgress>::create(_activityAnimationDuration,
								 [=, this](float p) {
		_labelText->setAnchorPoint(progress(labelAnchor, targetLabelAnchor, p));
		_labelText->setPosition(progress(labelPos, targetLabelPos, p));
		_labelText->setFontSize(progress(sourceLabelSize, targetLabelSize, p));
		_indicator->setContentSize(progress(indicatorSize, targetIndicatorSize, p));
	}),
					  EasingType::Standard),
			InputEnabledActionTag);

	runAction(makeEasing(Rc<ActionProgress>::create(_activityAnimationDuration,
								 [=, this](float p) {
		_labelText->setBlendColor(ColorRole::Primary,
				progress(sourceBlendValue, targetBlendValue, p));
	}),
					  EasingType::Standard),
			InputEnabledLabelActionTag);

	auto indicatorStyle = _indicator->getStyleTarget();
	indicatorStyle.colorRole = _focused ? ColorRole::Primary : ColorRole::OnSurfaceVariant;

	_indicator->setStyle(indicatorStyle, _activityAnimationDuration);
}

void InputField::acquireInputFromContainer() {
	_handler.run(_director->getTextInputManager(),
			TextInputRequest{
				.string = _inputState.string,
				.cursor = _container->getCursor(),
				.marked = TextCursor::InvalidCursor,
				.type = _inputState.type,
			});
	_focusInputListener->setEnabled(true);
}

void InputField::acquireInput(const Vec2 &targetLocation) {
	auto cursor = _container->getCursorForPosition(targetLocation);
	if (cursor == TextCursor::InvalidCursor) {
		cursor = TextCursor(static_cast<uint32_t>(_inputState.getStringView().size()), 0);
	}

	_container->setCursor(cursor);
	_container->touchPointers();
	_handler.run(_director->getTextInputManager(),
			TextInputRequest{
				.string = _inputState.string,
				.cursor = cursor,
				.marked = TextCursor::InvalidCursor,
				.type = _inputState.type,
			});
	_focusInputListener->setEnabled(true);
}

void InputField::updateCursorForLocation(const Vec2 &targetLocation) {
	auto cursor = _container->getCursorForPosition(targetLocation);
	if (cursor != TextCursor::InvalidCursor && cursor != _inputState.cursor) {
		_inputState.cursor = cursor;
		if (_handler.isActive()) {
			_handler.update(TextInputRequest{
				.string = _inputState.string,
				.cursor = _inputState.cursor,
				.marked = TextCursor::InvalidCursor,
				.type = _inputState.type,
			});
			_container->setCursor(cursor);
			_container->touchPointers();
		}
	}
}

void InputField::handleTextInput(const TextInputState &data) {
	// Update focus state if input was enabled or disabled
	if (_focused != data.enabled) {
		_focused = data.enabled;
		updateActivityState();
		updateInputEnabled();
		if (_enabled) {
			_container->setCursorCallback([this](TextCursor cursor) {
				_inputState.cursor = cursor;
				if (_handler.isActive()) {
					_handler.update(TextInputRequest{
						.string = _inputState.string,
						.cursor = _inputState.cursor,
						.marked = TextCursor::InvalidCursor,
						.type = _inputState.type,
					});
					_container->setCursor(cursor);
					_container->touchPointers();
				}
			});
		} else {
			_container->setCursorCallback(nullptr);
		}
	}
	_container->setEnabled(data.enabled);

	if (data.string == _inputState.string) {
		// only cursors was updated

		_inputState = data;
		_container->setCursor(_inputState.cursor);
		_container->handleLabelChanged();
	} else {
		// check for input errors
		// Note that errors can occur in any place in string due OS-assisted input
		auto tmp = data;
		auto err = validateInputData(tmp);

		// set current state to possibly modified input state
		if (err == InputFieldError::None) {
			// ignore tmp state if there is no error
			_inputState = data;
		} else {
			_inputState = move(tmp);
		}

		_container->setCursor(_inputState.cursor);

		auto label = _container->getLabel();

		switch (_passwordMode) {
		case InputFieldPasswordMode::NotPassword:
		case InputFieldPasswordMode::ShowAll: label->setString(_inputState.getStringView()); break;
		case InputFieldPasswordMode::ShowNone:
		case InputFieldPasswordMode::ShowChar: {
			// Update password-protected output
			WideString str;
			str.resize(_inputState.size(), u'*');
			label->setString(str);
			/*if (isInsert) {
			showLastChar();
		}*/
			break;
		}
		}

		label->tryUpdateLabel();

		_container->handleLabelChanged();

		if (err != InputFieldError::None) {
			handleError(InputFieldError::Overflow);

			// in case of an input error, we need to notify OS about new input state in our end
			_handler.update(TextInputRequest{
				.string = tmp.string,
				.cursor = tmp.cursor,
				.marked = tmp.marked,
				.type = tmp.type,
			});
		}
	}
}

bool InputField::handleInputChar(char16_t ch) { return true; }

void InputField::handleError(InputFieldError err) { }

InputFieldError InputField::validateInputData(TextInputState &state) {
	InputFieldError err = InputFieldError::None;

	auto label = _container->getLabel();

	auto maxChars = label->getMaxChars();
	if (maxChars > 0 && maxChars < state.size()) {
		auto tmpString = WideStringView(state.getStringView(), 0, maxChars);
		state.string = TextInputString::create(tmpString);
		if (state.cursor.start > state.size()) {
			state.cursor.start = static_cast<uint32_t>(state.size());
			state.cursor.length = 0;
		}
		err |= InputFieldError::Overflow;
	}

	size_t count = 0;
	for (auto &it : state.string->string) {
		if (!handleInputChar(it)) {
			auto tmpString = WideStringView(state.getStringView(), 0, count);
			state.string = TextInputString::create(tmpString);
			if (state.cursor.start > state.size()) {
				state.cursor.start = static_cast<uint32_t>(state.size());
				state.cursor.length = 0;
			}
			err |= InputFieldError::InvalidChar;
		}
		++count;
	}
	return err;
}

} // namespace stappler::xenolith::material2d
