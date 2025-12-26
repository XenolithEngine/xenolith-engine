/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>

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

#include "XL2dInputField.h"
#include "XLInputListener.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

bool InputField::init() {
	if (!Node::init()) {
		return false;
	}

	setCascadeOpacityEnabled(true);

	_node = addChild(Rc<InputLabelContainer>::create(), ZOrder(1));
	_node->setAnchorPoint(Vec2(0.0f, 0.0f));
	_node->setLabel(makeLabel());
	_label = _node->getLabel();

	_placeholder = addChild(Rc<Label>::create());
	_placeholder->setPosition(Vec2(0.0f, 0.0f));
	_placeholder->setColor(Color::Grey_500);
	_placeholder->setLocaleEnabled(true);
	_placeholder->setAnchorPoint(Anchor::MiddleLeft);

	/*_menu = Rc<InputMenu>::create(std::bind(&InputField::onMenuCut, this), std::bind(&InputField::onMenuCopy, this),
			std::bind(&InputField::onMenuPaste, this));
	_menu->setAnchorPoint(Anchor::MiddleBottom);
	_menu->setVisible(false);*/

	_inputListener = addInputListener(Rc<InputListener>::create());
	_inputListener->setTouchFilter([this] (const InputEvent &vec, const InputListener::DefaultEventFilter &def) {
		if (!_label->isActive()) {
			return def(vec);
		} else {
			return true;
		}
	});
	_inputListener->addPressRecognizer([this] (const GesturePress &g) {
		if (g.event == GestureEvent::Began) {
			return onPressBegin(g.pos);
		} else if (g.event == GestureEvent::Activated) {
			return onLongPress(g.pos, g.time, g.tickCount);
		} else if (g.event == GestureEvent::Ended) {
			return onPressEnd(g.pos);
		} else {
			return onPressCancel(g.pos);
		}
	}, TimeInterval::milliseconds(425), true);
	_inputListener->addSwipeRecognizer([this] (const GestureSwipe &s) {
		if (s.event == GestureEvent::Began) {
			if (onSwipeBegin(s.midpoint, s.delta / s.density)) {
				auto ret = onSwipe(s.midpoint, s.delta / s.density);
				_hasSwipe = ret;
				updateMenu();
				_inputListener->setExclusiveForTouch(s.input->data.id);
				return ret;
			}
			return false;
		} else if (s.event == GestureEvent::Activated) {
			return onSwipe(s.midpoint, s.delta / s.density);
		} else {
			auto ret = onSwipeEnd(s.velocity / s.density);
			_hasSwipe = false;
			updateMenu();
			return ret;
		}
	});

	return true;
}

void InputField::handleContentSizeDirty() {
	Node::handleContentSizeDirty();

	_node->setContentSize(_contentSize);
	_label->setPosition(Vec2(0.0f, _contentSize.width / 2.0f));
	_placeholder->setPosition(Vec2(0.0f, _contentSize.width / 2.0f));
}

bool InputField::visitGeometry(FrameInfo &info, NodeFlags parentFlags) {
	if (!_visible) {
		return false;
	}

	if ((parentFlags & NodeFlags::TransformDirty) != NodeFlags::None) {
		//if (_menu->isVisible()) {
		//	setMenuPosition(_menuPosition);
		//}
	}

	return Node::visitGeometry(info, parentFlags);
}

void InputField::handleEnter(Scene *scene) {
	Node::handleEnter(scene);
	//_scene->pushFloatNode(_menu, 1);
	_label->setDelegate(this);
}
void InputField::handleExit() {
	_label->setDelegate(nullptr);
	//_scene->popFloatNode(_menu);
	Node::handleExit();
}

void InputField::setInputCallback(const Callback &cb) {
	_onInput = cb;
}
const InputField::Callback &InputField::getInputCallback() const {
	return _onInput;
}

bool InputField::handleInputChar(char16_t c) {
	if (_charFilter) {
		return _charFilter(c);
	}
	return true;
}
void InputField::handleActivated(bool value) {
	//_gestureListener->setSwallowTouches(value);
}
void InputField::handlePointer(bool value) {
	updateMenu();
}
void InputField::handleCursor(const Cursor &) {
	updateMenu();
}

void InputField::setMenuPosition(const Vec2 &pos) {
	/*_menu->updateMenu();
	_menuPosition = pos;
	auto width = _menu->getContentSize().width;

	Vec2 menuPos = pos;
	if (menuPos.x - width / 2.0f - 8.0f < 0.0f) {
		menuPos.x = width / 2.0f + 8.0f;
	} else if (menuPos.x + width / 2.0f + 8.0f > _contentSize.width) {
		menuPos.x = _contentSize.width - width / 2.0f - 8.0f;
	}

	menuPos =  convertToWorldSpace(menuPos) / screen::density();

	auto sceneSize = Scene::getRunningScene()->getViewSize();
	auto top = menuPos.y + _menu->getContentSize().height - (_menu->getAnchorPoint().y * _menu->getContentSize().height);
	auto bottom = menuPos.y - (_menu->getAnchorPoint().y * _menu->getContentSize().height);
	auto keyboardSize = ime::getKeyboardRect().size / screen::density();

	if (bottom < metrics::horizontalIncrement() / 2.0f + keyboardSize.height) {
		menuPos.y = metrics::horizontalIncrement() / 2.0f + keyboardSize.height + (_menu->getAnchorPoint().y * _menu->getContentSize().height);
	} else if (top > sceneSize.height - metrics::horizontalIncrement()) {
		menuPos.y = sceneSize.height - metrics::horizontalIncrement() - _menu->getContentSize().height + (_menu->getAnchorPoint().y * _menu->getContentSize().height);
	}
	_menu->setPosition(menuPos);*/
}

void InputField::setMaxChars(size_t value) {
	_label->setMaxChars(value);
}
size_t InputField::getMaxChars() const {
	return _label->getMaxChars();
}

void InputField::setInputType(TextInputType t) {
	_label->setInputType(t);
}
TextInputType InputField::getInputType() const {
	return _label->getInputType();
}

void InputField::setPasswordMode(PasswordMode mode) {
	_label->setPasswordMode(mode);
}
InputField::PasswordMode InputField::getPasswordMode() {
	return _label->getPasswordMode();
}

void InputField::setAllowAutocorrect(bool value) {
	_label->setAllowAutocorrect(value);
}
bool InputField::isAllowAutocorrect() const {
	return _label->isAllowAutocorrect();
}

void InputField::setEnabled(bool value) {
	_label->setEnabled(value);
}
bool InputField::isEnabled() const {
	return _label->isEnabled();
}

void InputField::setNormalColor(const Color &color) {
	_normalColor = color;
	_label->setCursorColor(color);
}
const Color &InputField::getNormalColor() const {
	return _normalColor;
}

void InputField::setErrorColor(const Color &color) {
	_errorColor = color;
}
const Color &InputField::getErrorColor() const {
	return _errorColor;
}

bool InputField::empty() const {
	return _label->empty();
}

void InputField::setPlaceholder(const WideStringView &str) {
	_placeholder->setString(str);
}
void InputField::setPlaceholder(const StringView &str) {
	_placeholder->setString(str);
}
WideStringView InputField::getPlaceholder() const {
	return _label->getString();
}

void InputField::setString(const WideString &str) {
	if (_label->empty() != str.empty()) {
		_contentSizeDirty = true;
	}
	_label->setString(str);
}
void InputField::setString(const String &str) {
	if (_label->empty() != str.empty()) {
		_contentSizeDirty = true;
	}
	_label->setString(str);
}
WideStringView InputField::getString() const {
	return _label->getString();
}

InputLabel *InputField::getLabel() const {
	return _label;
}

void InputField::setCharFilter(const CharFilter &cb) {
	_charFilter = cb;
}
const InputField::CharFilter &InputField::getCharFilter() const {
	return _charFilter;
}

void InputField::acquireInput() {
	_label->acquireInput();
}
void InputField::releaseInput() {
	_label->releaseInput();
}
bool InputField::isInputActive() const {
	return _label->isActive();
}

bool InputField::onPressBegin(const Vec2 &vec) {
	return _label->onPressBegin(vec);
}
bool InputField::onLongPress(const Vec2 &vec, const TimeInterval &time, int count) {
	return _label->onLongPress(vec, time, count);
}

bool InputField::onPressEnd(const Vec2 &vec) {
	if (!_label->isActive()) {
		if (!_label->onPressEnd(vec)) {
			if (!_label->isActive() && isTouched(vec)) {
				acquireInput();
				return true;
			}
			return false;
		}
		return true;
	} else {
		if (_placeholder->isVisible() && _placeholder->isTouched(vec, 8.0f)) {
			releaseInput();
			return true;
		}
		if (!_label->onPressEnd(vec)) {
			if (!_node->isTouched(vec)) {
				releaseInput();
				return true;
			} else {
				_label->setCursor(Cursor(uint32_t(_label->getCharsCount())));
			}
			return false;
		} else if (_label->empty() && !_node->isTouched(vec)) {
			releaseInput();
		}
		return true;
	}
}
bool InputField::onPressCancel(const Vec2 &vec) {
	return _label->onPressCancel(vec);
}

bool InputField::onSwipeBegin(const Vec2 &vec, const Vec2 &) {
	return _label->onSwipeBegin(vec);
}

bool InputField::onSwipe(const Vec2 &vec, const Vec2 &delta) {
	return _label->onSwipe(vec, delta);
}

bool InputField::onSwipeEnd(const Vec2 &vel) {
	return _label->onSwipeEnd(vel);
}

void InputField::onMenuCut() {
	//Device::getInstance()->copyStringToClipboard(_label->getSelectedString());
	_label->eraseSelection();
}
void InputField::onMenuCopy() {
	//Device::getInstance()->copyStringToClipboard(_label->getSelectedString());
	//_menu->setVisible(false);
}
void InputField::onMenuPaste() {
	//_label->pasteString(Device::getInstance()->getStringFromClipboard());
}

void InputField::updateMenu() {
	/*auto c = _label->getCursor();
	if (!_hasSwipe && _label->isPointerEnabled() && (c.length > 0 || Device::getInstance()->isClipboardAvailable())) {
		_menu->setCopyMode(c.length > 0);
		_menu->setVisible(true);
		onMenuVisible();
	} else {
		_menu->setVisible(false);
		onMenuHidden();
	}*/
}

void InputField::onMenuVisible() {

}
void InputField::onMenuHidden() {

}

Rc<InputLabel> InputField::makeLabel() {
	auto label = Rc<InputLabel>::create();
	label->setPosition(Vec2(0.0f, 0.0f));
	label->setCursorColor(_normalColor);
	label->setAnchorPoint(Anchor::MiddleLeft);
	return label;
}

}
