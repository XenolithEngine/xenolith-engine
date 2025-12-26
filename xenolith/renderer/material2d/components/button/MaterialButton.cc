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

#include "MaterialButton.h"
#include "MaterialLabel.h"
#include "XLScene.h"
#include "MaterialSceneContent.h"
#include "MaterialFloatingMenu.h"
#include "XLInputListener.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::material2d {

static SurfaceStyle Button_getSurfaceStyle(NodeStyle style, ColorRole role, uint32_t schemeTag) {
	return SurfaceStyle(style, Elevation::Level1, ShapeStyle::Full, role, schemeTag);
}

bool Button::init(NodeStyle style, ColorRole role, uint32_t schemeTag) {
	return init(Button_getSurfaceStyle(style, role, schemeTag));
}

bool Button::init(const SurfaceStyle &style) {
	if (!Surface::init(style)) {
		return false;
	}

	_labelText = addChild(Rc<TypescaleLabel>::create(TypescaleRole::LabelLarge), ZOrder(1));
	_labelText->setAnchorPoint(Anchor::MiddleLeft);
	_labelText->setLocaleEnabled(true);
	_labelText->setContentSizeDirtyCallback([this] { updateSizeFromContent(); });

	_labelValue = addChild(Rc<TypescaleLabel>::create(TypescaleRole::LabelLarge), ZOrder(1));
	_labelValue->setAnchorPoint(Anchor::MiddleLeft);
	_labelValue->setLocaleEnabled(true);
	_labelValue->setContentSizeDirtyCallback([this] { updateSizeFromContent(); });

	_leadingIcon = addChild(Rc<IconSprite>::create(IconName::None), ZOrder(1));
	_leadingIcon->setAnchorPoint(Anchor::MiddleLeft);
	_leadingIcon->setContentSize(Size2(18.0f, 18.0f));

	_trailingIcon = addChild(Rc<IconSprite>::create(IconName::None), ZOrder(1));
	_trailingIcon->setAnchorPoint(Anchor::MiddleLeft);
	_trailingIcon->setContentSize(Size2(18.0f, 18.0f));

	_inputListener = addSystem(Rc<InputListener>::create());
	_inputListener->addMouseOverRecognizer([this](const GestureData &data) {
		_mouseOver = (data.event == GestureEvent::Began);
		updateActivityState();
		return true;
	});
	_inputListener->addPressRecognizer([this](const GesturePress &press) {
		if (!_enabled || (_menuButtonListener->getSubscription() && !isMenuSourceButtonEnabled())) {
			return false;
		}

		if (press.event == GestureEvent::Began) {
			_touchLocation = press.location();
			_longPressInit = false;
			_pressed = true;
			updateActivityState();
		} else if (press.event == GestureEvent::Activated) {
			_longPressInit = true;
			handleLongPress();
			_inputListener->setExclusiveForTouch(press.getId());
		} else if (press.event == GestureEvent::Ended || press.event == GestureEvent::Cancelled) {
			_pressed = false;
			updateActivityState();
			if (press.event == GestureEvent::Ended) {
				_inputListener->setExclusiveForTouch(press.getId());
				if (_longPressInit) {
				} else {
					handleTap();
				}
			}
		}
		return true;
	}, LongPressInterval);

	_inputListener->addTapRecognizer([this](const GestureTap &tap) {
		if (!_enabled) {
			return false;
		}
		if (tap.count == 2) {
			_inputListener->setExclusiveForTouch(tap.getId());
			handleDoubleTap();
		}
		return true;
	});

	_menuButtonListener = addSystem(Rc<DataListener<MenuSourceButton>>::create(
			[this](SubscriptionFlags flags) { updateMenuButtonSource(); }));

	return true;
}

void Button::handleContentSizeDirty() {
	Surface::handleContentSizeDirty();

	if (_followContentSize && hasContent()) {
		updateSizeFromContent();
	}

	layoutContent();
}

void Button::setFollowContentSize(bool value) {
	if (value != _followContentSize) {
		_followContentSize = value;
		_contentSizeDirty = true;
		if (_followContentSize) {
			updateSizeFromContent();
		}
	}
}

bool Button::isFollowContentSize() const { return _followContentSize; }

void Button::setSwallowEvents(bool value) {
	if (value) {
		_inputListener->setSwallowEvents(InputListener::EventMaskTouch);
	} else {
		_inputListener->clearSwallowEvents(InputListener::EventMaskTouch);
	}
}

bool Button::isSwallowEvents() const {
	return _inputListener->isSwallowAllEvents(InputListener::EventMaskTouch);
}

void Button::setEnabled(bool value) {
	if (value != _enabled) {
		_enabled = value;
		_inputListener->setEnabled(_enabled);
		updateActivityState();
	}
}

void Button::setSelected(bool val) {
	if (val != _selected) {
		_selected = val;
		updateActivityState();
	}
}

bool Button::isSelected() const { return _selected; }

bool Button::isMenuSourceButtonEnabled() const {
	if (!_menuButtonListener) {
		return false;
	}

	return _menuButtonListener->getSubscription()->getCallback() != nullptr
			|| _menuButtonListener->getSubscription()->getNextMenu();
}

void Button::setNodeMask(NodeMask mask) {
	if (_nodeMask != mask) {
		_nodeMask = mask;
		updateSizeFromContent();
		_contentSizeDirty = true;
	}
}

void Button::setText(StringView text) {
	_labelText->setString(text);
	updateSizeFromContent();
	_contentSizeDirty = true;
}

StringView Button::getText() const { return _labelText->getString8(); }

void Button::setTextValue(StringView text) {
	_labelValue->setString(text);
	updateSizeFromContent();
	_contentSizeDirty = true;
}

StringView Button::getTextValue() const { return _labelValue->getString8(); }

void Button::setIconSize(float value) {
	if (value != getIconSize()) {
		_leadingIcon->setContentSize(Size2(value, value));
		_trailingIcon->setContentSize(Size2(value, value));
		updateSizeFromContent();
	}
}

float Button::getIconSize() const { return _leadingIcon->getContentSize().width; }

void Button::setLeadingIconName(IconName name, float progress) {
	if (name != getLeadingIconName()) {
		_leadingIcon->setIconName(name);
		_leadingIcon->setProgress(progress);
		updateSizeFromContent();
	}
}

IconName Button::getLeadingIconName() const { return _leadingIcon->getIconName(); }

void Button::setLeadingIconProgress(float progress, float animation) {
	if (animation > 0.0f) {
		_leadingIcon->animate(progress, animation);
	} else {
		_leadingIcon->setProgress(progress);
	}
}

float Button::getLeadingIconProgress() const { return _leadingIcon->getProgress(); }

void Button::setTrailingIconName(IconName name) {
	if (name != getTrailingIconName()) {
		_trailingIcon->setIconName(name);
		updateSizeFromContent();
	}
}

IconName Button::getTrailingIconName() const { return _trailingIcon->getIconName(); }

void Button::setTrailingIconProgress(float progress, float animation) {
	if (animation > 0.0f) {
		_trailingIcon->animate(progress, animation);
	} else {
		_trailingIcon->setProgress(progress);
	}
}

float Button::getTrailingIconProgress() const { return _trailingIcon->getProgress(); }

void Button::setTapCallback(Function<void()> &&cb) { _callbackTap = sp::move(cb); }

void Button::setLongPressCallback(Function<void()> &&cb) { _callbackLongPress = sp::move(cb); }

void Button::setDoubleTapCallback(Function<void()> &&cb) { _callbackDoubleTap = sp::move(cb); }

void Button::setMenuSourceButton(Rc<MenuSourceButton> &&button) {
	if (button != _menuButtonListener->getSubscription()) {
		if (auto b = _menuButtonListener->getSubscription()) {
			b->handleNodeDetached(this);
		}
		_menuButtonListener->setSubscription(button.get());
		updateMenuButtonSource();
		if (button) {
			button->handleNodeAttached(this);
		}
	}
}

MenuSourceButton *Button::getMenuSourceButton() const {
	return _menuButtonListener->getSubscription();
}

void Button::setBlendColor(ColorRole rule, float value) {
	_labelText->setBlendColor(rule, value);
	_labelValue->setBlendColor(rule, value);
	_leadingIcon->setBlendColor(rule, value);
	_trailingIcon->setBlendColor(rule, value);
}

void Button::setBlendColor(const Color4F &color, float value) {
	_labelText->setBlendColor(color, value);
	_labelValue->setBlendColor(color, value);
	_leadingIcon->setBlendColor(color, value);
	_trailingIcon->setBlendColor(color, value);
}

ColorRole Button::getBlendColorRule() const { return _labelText->getBlendColorRule(); }

const Color4F &Button::getBlendColor() const { return _labelText->getBlendColor(); }

float Button::getBlendColorValue() const { return _labelText->getBlendColorValue(); }

bool Button::hasContent() const {
	return !_labelText->empty() || !_labelValue->empty()
			|| _leadingIcon->getIconName() != IconName::None
			|| _trailingIcon->getIconName() != IconName::None;
}

void Button::updateSizeFromContent() {
	if (!_followContentSize || !hasContent()) {
		_contentSizeDirty = true;
		return;
	}

	Size2 targetSize;
	if (!_labelText->empty() && (_nodeMask & NodeMask::LabelText) != NodeMask::None) {
		targetSize = _labelText->getContentSize();
	} else {
		targetSize.height = getIconSize();
	}
	targetSize.width = getWidthForContent();
	targetSize.height += 24.0f;

	setContentSize(targetSize);
}

void Button::updateActivityState() {
	auto style = getStyleTarget();
	if (!_enabled || (_menuButtonListener->getSubscription() && !isMenuSourceButtonEnabled())) {
		style.activityState = ActivityState::Disabled;
	} else if (_pressed || _selected) {
		style.activityState = ActivityState::Pressed;
	} else if (_mouseOver) {
		style.activityState = ActivityState::Hovered;
	} else if (_focused) {
		style.activityState = ActivityState::Focused;
	} else {
		style.activityState = ActivityState::Enabled;
	}
	setStyle(style, _activityAnimationDuration);
}

void Button::handleTap() {
	if (_callbackTap) {
		auto id = retain();
		_callbackTap();
		release(id);
	} else if (auto btn = _menuButtonListener->getSubscription()) {
		auto &cb = btn->getCallback();
		if (cb) {
			cb(this, btn);
		} else if (_floatingMenuSource) {
			if (auto content = dynamic_cast<SceneContent2d *>(_scene->getContent())) {
				auto posRight = content->convertToNodeSpace(
						convertToWorldSpace(Vec2(_contentSize.width, _contentSize.height)));

				FloatingMenu::push(content, _floatingMenuSource, posRight,
						FloatingMenu::Binding::OriginRight, nullptr);
			}
		}
	}
}

void Button::handleLongPress() {
	if (_callbackLongPress) {
		auto id = retain();
		_callbackLongPress();
		release(id);
	}
}

void Button::handleDoubleTap() {
	if (_callbackDoubleTap) {
		auto id = retain();
		_callbackDoubleTap();
		release(id);
	}
}

float Button::getWidthForContent() const {
	float contentWidth = 0.0f;
	if (!_labelText->empty() && (_nodeMask & NodeMask::LabelText) != NodeMask::None) {
		contentWidth = ((_styleTarget.nodeStyle == NodeStyle::Text) ? 24.0f : 48.0f)
				+ _labelText->getContentSize().width;
		if (_styleTarget.nodeStyle == NodeStyle::Text
				&& (getLeadingIconName() != IconName::None
						|| getTrailingIconName() != IconName::None)) {
			contentWidth += 16.0f;
		}
	} else {
		contentWidth = 24.0f;
	}

	if (!_labelValue->empty() && (_nodeMask & NodeMask::LabelValue) != NodeMask::None) {
		contentWidth += _labelValue->getContentSize().width + 8.0f;
	}

	if (getLeadingIconName() != IconName::None
			&& (_nodeMask & NodeMask::LeadingIcon) != NodeMask::None) {
		contentWidth += _leadingIcon->getContentSize().width;
	}
	if (getTrailingIconName() != IconName::None
			&& (_nodeMask & NodeMask::TrailingIcon) != NodeMask::None) {
		contentWidth += _trailingIcon->getContentSize().width;
	}
	return contentWidth;
}

void Button::updateMenuButtonSource() {
	if (auto btn = _menuButtonListener->getSubscription()) {
		_selected = btn->isSelected();
		_floatingMenuSource = btn->getNextMenu();

		setLeadingIconName(btn->getNameIcon());
		setTrailingIconName(btn->getValueIcon());
		setText(btn->getName());
		setTextValue(btn->getValue());

		if (btn->getNextMenu()) {
			if (getTrailingIconName() == IconName::None) {
				setTrailingIconName(IconName::Navigation_arrow_right_solid);
			}
		}
	} else {
		_selected = false;
		_floatingMenuSource = nullptr;
	}
	updateActivityState();
}

void Button::layoutContent() {
	auto leadingIconName = getLeadingIconName();
	auto trailingIconName = getTrailingIconName();
	auto hasLabelText = !_labelText->empty();
	auto hasLabelValue = !_labelValue->empty();

	_labelText->setVisible((_nodeMask & NodeMask::LabelText) != NodeMask::None);
	_labelValue->setVisible((_nodeMask & NodeMask::LabelValue) != NodeMask::None);
	_leadingIcon->setVisible((_nodeMask & NodeMask::LeadingIcon) != NodeMask::None);
	_trailingIcon->setVisible((_nodeMask & NodeMask::TrailingIcon) != NodeMask::None);

	if ((_nodeMask & NodeMask::LabelText) == NodeMask::None) {
		hasLabelText = false;
	}

	if ((_nodeMask & NodeMask::LabelValue) == NodeMask::None) {
		hasLabelValue = false;
	}

	if ((_nodeMask & NodeMask::LeadingIcon) == NodeMask::None) {
		leadingIconName = IconName::None;
	}

	if ((_nodeMask & NodeMask::TrailingIcon) == NodeMask::None) {
		trailingIconName = IconName::None;
	}

	if (leadingIconName != IconName::None && trailingIconName == IconName::None && !hasLabelText
			&& !hasLabelValue) {
		_leadingIcon->setAnchorPoint(Anchor::Middle);
		_leadingIcon->setPosition(_contentSize / 2.0f);
	} else {
		_leadingIcon->setAnchorPoint(Anchor::MiddleLeft);

		float contentWidth = getWidthForContent();
		float offset = (_contentSize.width - contentWidth) / 2.0f;

		Vec2 target(offset + (_styleTarget.nodeStyle == NodeStyle::Text ? 12.0f : 16.0f),
				_contentSize.height / 2.0f);

		if (leadingIconName != IconName::None) {
			_leadingIcon->setPosition(target);
			target.x += 8.0f + _leadingIcon->getContentSize().width;
		} else if (_styleTarget.nodeStyle != NodeStyle::Text) {
			target.x += 8.0f;
		}

		_labelText->setPosition(target);
		target.x += _labelText->getContentSize().width + 8.0f;

		if (hasLabelValue) {
			_labelValue->setPosition(target);
			target.x += _labelValue->getContentSize().width + 8.0f;
		}

		_trailingIcon->setPosition(target);
	}
}

} // namespace stappler::xenolith::material2d
