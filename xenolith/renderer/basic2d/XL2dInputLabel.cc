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

#include "XL2dInputLabel.h"
#include "XL2dVectorSprite.h"
#include "XLDirector.h"
#include "XLAction.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

InputLabelDelegate::~InputLabelDelegate() { }
bool InputLabelDelegate::handleInputChar(char16_t) { return true; }
bool InputLabelDelegate::handleInputString(const WideStringView &str, const Cursor &c) { return true; }

void InputLabelDelegate::handleCursor(const Cursor &) { }
void InputLabelDelegate::handleInput() { }

void InputLabelDelegate::handleActivated(bool) { }
void InputLabelDelegate::handleError(Error) { }

void InputLabelDelegate::handlePointer(bool) { }


InputLabel::Selection::~Selection() { }

bool InputLabel::Selection::init() {
	if (!Sprite::init()) {
		return false;
	}

	return true;
}

void InputLabel::Selection::clear() {
	_vertexes.clear();
}

void InputLabel::Selection::emplaceRect(const Rect &rect) {
	_vertexes.addQuad().setGeometry(
			Vec4(rect.origin.x, _contentSize.height - rect.origin.y - rect.size.height, 0.0f, 0.0f), rect.size);
}

void InputLabel::Selection::updateColor() {
	Sprite::updateColor();
}

bool InputLabel::init() {
	return init(nullptr, DescriptionStyle());
}

bool InputLabel::init(StringView str) {
	return init(nullptr, DescriptionStyle(), str);
}

bool InputLabel::init(StringView str, float w, TextAlign a) {
	return init(nullptr, DescriptionStyle(), str, w, a);
}

bool InputLabel::init(font::FontController *controller, const DescriptionStyle &desc, StringView str, float width, TextAlign a) {
	if (!Label::init(controller, desc, str, width, a)) {
		return false;
	}

	_emplaceAllChars = true;

	_handler.onText = std::bind(&InputLabel::onText, this, std::placeholders::_1, std::placeholders::_2);
	_handler.onKeyboard = std::bind(&InputLabel::onKeyboard, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
	_handler.onInput = std::bind(&InputLabel::onInput, this, std::placeholders::_1);

	_cursorLayer = addChild(Rc<Layer>::create(Color::Grey_500));
	_cursorLayer->setVisible(false);
	_cursorLayer->setContentSize(Size2(1.0f, getFontHeight() / _labelDensity));
	_cursorLayer->setAnchorPoint(Vec2(0.0f, 0.0f));
	_cursorLayer->setOpacity(1.0f);

	_cursorPointer = addChild(Rc<VectorSprite>::create(Size2(24, 24)));
	_cursorPointer->setContentSize(Size2(24.0f, 24.0f));
	_cursorPointer->setColor(Color::Grey_500);
	_cursorPointer->addPath(VectorPath()
		.moveTo(12.0f, 0.0f)
		.lineTo(5.0f, 7.0f)
		.arcTo(7.0f * sqrt(2.0f), 7.0f * sqrt(2.0f), 0.0f, true, false, 19.0f, 7.0f)
		.closePath());
	_cursorPointer->setAnchorPoint(Vec2(0.5f, _cursorAnchor));
	_cursorPointer->setOpacity(OpacityValue(222));
	_cursorPointer->setVisible(false);
	_cursorPointer->setIgnoreParentState(true);

	_cursorStart = addChild(Rc<VectorSprite>::create(Size2(48, 48)));
	_cursorStart->addPath(VectorPath()
		.moveTo(48, 0)
		.lineTo(24, 0)
		.arcTo(24, 24, 0, true, false, 48, 24)
		.closePath());
	_cursorStart->setContentSize(Size2(24.0f, 24.0f));
	_cursorStart->setAnchorPoint(Vec2(1.0f, _cursorAnchor));
	_cursorStart->setColor(_selectionColor);
	_cursorStart->setOpacity(Opacity(192));
	_cursorStart->setVisible(false);
	_cursorStart->setIgnoreParentState(true);

	_cursorEnd = addChild(Rc<VectorSprite>::create(Size2(48, 48)));
	_cursorEnd->addPath(VectorPath()
		.moveTo(0, 0)
		.lineTo(0, 24)
		.arcTo(24, 24, 0, true, false, 24, 0)
		.closePath());
	_cursorEnd->setContentSize(Size2(24.0f, 24.0f));
	_cursorEnd->setAnchorPoint(Vec2(0.0f, _cursorAnchor));
	_cursorEnd->setColor(_selectionColor);
	_cursorEnd->setOpacity(192);
	_cursorEnd->setVisible(false);
	_cursorEnd->setIgnoreParentState(true);

	_cursorSelection = addChild(Rc<Selection>::create());
	_cursorSelection->setAnchorPoint(Vec2(0.0f, 0.0f));
	_cursorSelection->setPosition(Vec2(0.0f, 0.0f));
	_cursorSelection->setColor(_cursorColor);
	_cursorSelection->setOpacity(OpacityValue(64));

	setOpacity(222);

	return true;
}

bool InputLabel::init(const DescriptionStyle &style, StringView str, float w, TextAlign a) {
	return init(nullptr, style, str, w, a);
}

bool InputLabel::visitGeometry(FrameInfo &info, NodeFlags parentFlags) {
	if (!_visible) {
		return false;
	}
	if (_cursorDirty) {
		_cursorDirty = false;
		if (_delegate && _inputEnabled) {
			_delegate->handleCursor(_cursor);
		}
	}
	return Label::visitGeometry(info, parentFlags);
}

void InputLabel::handleContentSizeDirty() {
	Label::handleContentSizeDirty();

	_cursorSelection->setContentSize(_contentSize);

	updateCursor();
	updateFocus();
}

void InputLabel::handleExit() {
	Label::handleExit();
	if (_handler.isActive()) {
		releaseInput();
	}
}

Vec2 InputLabel::getCursorMarkPosition() const {
	if (_cursor.length > 0) {
		auto line = _format->getLine(_cursor.start);
		if (line) {
			auto last = std::min(_cursor.start + _cursor.length - 1, line->start + line->count - 1);
			auto startPos = getCursorPosition(_cursor.start, true);
			auto endPos = getCursorPosition(last, false);

			return Vec2((startPos.x + endPos.x) / 2.0f, startPos.y);
		}
		return Vec2::ZERO;
	} else {
		auto &pos = _cursorLayer->getPosition();
		return Vec2(pos.x, pos.y);
	}
}

void InputLabel::setCursorColor(const Color &color, bool pointer) {
	_cursorColor = color;
	_cursorLayer->setColor(color);
	if (pointer) {
		setPointerColor(color);
	}
	if (_handler.isActive()) {
		_cursorLayer->setColor(color);
	}
	//_cursorSelection->setColor(color);
}
const Color &InputLabel::getCursorColor() const {
	return _cursorColor;
}

void InputLabel::setPointerColor(const Color &color) {
	_selectionColor = color;
	if (_handler.isActive()) {
		_cursorPointer->setColor(color);
	}
	_cursorStart->setColor(color);
	_cursorEnd->setColor(color);
}
const Color &InputLabel::getPointerColor() const {
	return _selectionColor;
}

void InputLabel::setString(const StringView &str) {
	setString(string::toUtf16<Interface>(str));
}

void InputLabel::setString(const WideStringView &str) {
	updateString(str, Cursor(uint32_t(_inputString.length()), 0));
	if (_handler.isActive()) {
		_handler.setString(_inputString, _cursor);
	}
}

WideStringView InputLabel::getString() const {
	return _inputString;
}

void InputLabel::setCursor(const Cursor &c) {
	_cursor = c;
	if (_handler.isActive()) {
		_handler.setCursor(_cursor);
	}
	updateCursor();
}

const InputLabel::Cursor &InputLabel::getCursor() const {
	return _cursor;
}

void InputLabel::setInputType(InputType t) {
	_inputType = t;
}
InputLabel::InputType InputLabel::getInputType() const {
	return _inputType;
}

void InputLabel::setPasswordMode(PasswordMode p) {
	_password = p;
	updateString(_inputString, _cursor);
}
InputLabel::PasswordMode InputLabel::getPasswordMode() {
	return _password;
}

void InputLabel::setDelegate(InputLabelDelegate *d) {
	_delegate = d;
}
InputLabelDelegate *InputLabel::getDelegate() const {
	return _delegate;
}

void InputLabel::setEnabled(bool value) {
	if (value != _enabled) {
		_enabled = value;
		updateFocus();
		updateCursor();

		if (!_enabled) {
			stopActionByTag(235);
			hideLastChar();
		}
	}
}
bool InputLabel::isEnabled() const {
	return _enabled;
}

void InputLabel::setRangeAllowed(bool value) {
	_rangeAllowed = value;
}
bool InputLabel::isRangeAllowed() const {
	return _rangeAllowed;
}

void InputLabel::setAllowMultiline(bool v) {
	_allowMultiline = v;
}
bool InputLabel::isAllowMultiline() const {
	return _allowMultiline;
}

void InputLabel::setAllowAutocorrect(bool v) {
	_allowAutocorrect = v;
}
bool InputLabel::isAllowAutocorrect() const {
	return _allowAutocorrect;
}

void InputLabel::setCursorAnchor(float value) {
	if (_cursorAnchor != value) {
		_cursorAnchor = value;

		_cursorPointer->setAnchorPoint(Vec2(0.5f, _cursorAnchor));
		_cursorStart->setAnchorPoint(Vec2(1.0f, _cursorAnchor));
		_cursorEnd->setAnchorPoint(Vec2(0.0f, _cursorAnchor));
	}
}
float InputLabel::getCursorAnchor() const {
	return _cursorAnchor;
}

void InputLabel::acquireInput() {
	if (_director) {
		_cursor.start = uint32_t(getCharsCount());
		_cursor.length = 0;

		_handler.run(_director->getTextInputManager(), _inputString, _cursor, TextCursor::InvalidCursor, getInputTypeValue());
		updateCursor();
	}
}
void InputLabel::releaseInput() {
	_handler.cancel();
}

bool InputLabel::empty() const {
	return _inputString.empty();
}

bool InputLabel::isActive() const {
	return _handler.isActive() && _inputEnabled;
}

bool InputLabel::isPointerEnabled() const {
	return _pointerEnabled;
}

String InputLabel::getSelectedString() const {
	if (_cursor.length > 0) {
		return string::toUtf8<Interface>(_inputString.substr(_cursor.start, _cursor.length));
	} else {
		return String();
	}
}

void InputLabel::pasteString(const String &str) {
	pasteString(string::toUtf16<Interface>(str));
}

void InputLabel::pasteString(const WideString &str) {
	auto newString = _inputString;
	if (_cursor.length > 0) {
		newString.erase(_cursor.start, _cursor.length);
		_cursor.length = 0;
	}
	if (_cursor.start >= newString.size()) {
		newString.append(str);
	} else {
		newString.insert(_cursor.start, str);
	}

	updateString(newString, Cursor(uint32_t(_cursor.start + str.size())));
	if (_handler.isActive()) {
		_handler.setString(_inputString, _cursor);
	}
	setPointerEnabled(false);
	updateCursor();
}

void InputLabel::eraseSelection() {
	if (_cursor.length > 0) {
		auto newString = _inputString;
		newString.erase(_cursor.start, _cursor.length);
		updateString(newString, Cursor(_cursor.start));
		if (_handler.isActive()) {
			_handler.setString(_inputString, _cursor);
		}
		setPointerEnabled(false);
		updateCursor();
	}
}

/*void InputLabel::setInputTouchFilter(const Function<bool(const Vec2 &)> &cb) {
	_handler.onTouchFilter = cb;
}

const Function<bool(const Vec2 &)> &InputLabel::getInputTouchFilter() const {
	return _handler.onTouchFilter;
}*/

VectorSprite *InputLabel::getTouchedCursor(const Vec2 &vec, float padding) {
	if (_cursorPointer->isVisible() && _cursorPointer->isTouched(vec, padding)) {
		return _cursorPointer;
	}
	if (_cursorStart->isVisible() && _cursorPointer->isTouched(vec, padding)) {
		return _cursorStart;
	}
	if (_cursorEnd->isVisible() && _cursorPointer->isTouched(vec, padding)) {
		return _cursorEnd;
	}
	return nullptr;
}

bool InputLabel::onPressBegin(const Vec2 &vec) {
	if (!isEnabled()) {
		return false;
	}
	return true;
}
bool InputLabel::onLongPress(const Vec2 &vec, const TimeInterval &time, int count) {
	if (!_rangeAllowed || _inputString.empty() || _selectedCursor != nullptr || (!_inputEnabled && _director->getTextInputManager()->isInputEnabled())) {
		return false;
	}

	if (getTouchedCursor(vec) != nullptr) {
		return false;
	}

	if (count == 1) {
		_isLongPress = true;
		auto pos = convertToNodeSpace(vec);

		auto chIdx = _format->getChar(pos.x * _labelDensity, _format->height - pos.y * _labelDensity, FormatSpec::Center).first;
		if (chIdx != maxOf<uint32_t>()) {
			auto word = _format->selectWord(chIdx);
			setCursor(word);
			scheduleCursorPointer();
		}

	} else if (count == 3) {
		setCursor(Cursor(0, uint32_t(getCharsCount())));
		scheduleCursorPointer();
		return false;
	}
	return true;
}

bool InputLabel::onPressEnd(const Vec2 &vec) {
	if (!_handler.isActive() && isTouched(vec)) {
		if (_isLongPress) {
			if (_director) {
				_handler.run(_director->getTextInputManager(), _inputString, _cursor, TextCursor::InvalidCursor, getInputTypeValue());
				updateCursor();
			}
		} else {
			acquireInput();
			if (!empty()) {
				auto chIdx = getCharIndex(convertToNodeSpace(vec));
				if (chIdx.first != maxOf<uint32_t>()) {
					if (chIdx.second) {
						setCursor(Cursor(chIdx.first + 1));
					} else {
						setCursor(Cursor(chIdx.first));
					}
					scheduleCursorPointer();
				}
			}
		}
		return true;
	} else if (_handler.isActive()) {
		if (_isLongPress) {
			_isLongPress = false;
			return true;
		}
		if (!empty() && !_selectedCursor) {
			auto chIdx = getCharIndex(convertToNodeSpace(vec));
			if (chIdx.first != maxOf<uint32_t>()) {
				if (chIdx.second) {
					setCursor(Cursor(chIdx.first + 1));
				} else {
					setCursor(Cursor(chIdx.first));
				}
				scheduleCursorPointer();
				return true;
			}
			scheduleCursorPointer();
			return false;
		} else if ((empty() && !isPointerEnabled()) || _selectedCursor) {
			scheduleCursorPointer();
		}
		return true;
	}
	_isLongPress = false;
	return false;
}

bool InputLabel::onPressCancel(const Vec2 &vec) {
	if (!_handler.isActive() && _isLongPress) {
		if (_director) {
			_handler.run(_director->getTextInputManager(), _inputString, _cursor, TextCursor::InvalidCursor, getInputTypeValue());
			updateCursor();
		}
	}
	_isLongPress = false;
	return true;
}

bool InputLabel::onSwipeBegin(const Vec2 &vec) {
	if (!isEnabled()) {
		return false;
	}
	if (_handler.isInputEnabled()) {
		auto c = getTouchedCursor(vec);
		if (c != nullptr) {
			unscheduleCursorPointer();
			_selectedCursor = c;
			return true;
		}
	}
	return false;
}

bool InputLabel::onSwipe(const Vec2 &vec, const Vec2 &) {
	if (_selectedCursor) {
		auto size = _selectedCursor->getContentSize();
		auto anchor = _selectedCursor->getAnchorPoint();
		auto offset = Vec2(anchor.x * size.width - size.width / 2.0f, (anchor.y + 1.0f) * size.height);

		auto locInLabel = convertToNodeSpace(vec) + offset;

		if (_selectedCursor == _cursorPointer) {
			auto chIdx = getCharIndex(locInLabel);
			if (chIdx.first != maxOf<uint32_t>()) {
				auto cursorIdx = chIdx.first;
				if (chIdx.second) {
					++ cursorIdx;
				}

				if (_cursor.start != cursorIdx) {
					setCursor(Cursor(cursorIdx));
				}
			}
		} else if (_selectedCursor == _cursorStart) {
			uint32_t charNumber = _format->getChar(int32_t(roundf(locInLabel.x * _labelDensity)),
					_format->height - int32_t(roundf(locInLabel.y * _labelDensity)), font::FormatSpec::Prefix).first;
			if (charNumber != maxOf<uint32_t>()) {
				if (charNumber != _cursor.start && charNumber < _cursor.start + _cursor.length) {
					setCursor(Cursor(charNumber, (_cursor.start + _cursor.length) - charNumber));
				}
			}
		} else if (_selectedCursor == _cursorEnd) {
			uint32_t charNumber = _format->getChar(int32_t(roundf(locInLabel.x * _labelDensity)),
					_format->height - int32_t(roundf(locInLabel.y * _labelDensity)), font::FormatSpec::Suffix).first;
			if (charNumber != maxOf<uint32_t>()) {
				if (charNumber != _cursor.start + _cursor.length - 1 && charNumber >= _cursor.start) {
					setCursor(Cursor(_cursor.start, charNumber - _cursor.start + 1));
				}
			}
		}
		return true;
	}
	return false;
}

bool InputLabel::onSwipeEnd(const Vec2 &) {
	if (_selectedCursor) {
		_selectedCursor = nullptr;
		scheduleCursorPointer();
	}
	return false;
}

Layer *InputLabel::getCursorLayer() const {
	return _cursorLayer;
}

VectorSprite *InputLabel::getCursorPointer() const {
	return _cursorPointer;
}

VectorSprite *InputLabel::getCursorStart() const {
	return _cursorStart;
}

VectorSprite *InputLabel::getCursorEnd() const {
	return _cursorEnd;
}

void InputLabel::onText(const WideStringView &str, const Cursor &c) {
	if (updateString(str, c)) {
		setPointerEnabled(false);
		updateCursor();
		updateFocus();
	}
}

void InputLabel::onKeyboard(bool val, const Rect &, float) {
	if (val) {
		_cursorDirty = true;
	}
}
void InputLabel::onInput(bool value) {
	if (_inputEnabled != value) {
		_cursorDirty = true;
	}
	_inputEnabled = value;
	updateFocus();
	if (_delegate) {
		_delegate->handleActivated(value);
	}
}
void InputLabel::onEnded() {
	updateFocus();
}

void InputLabel::onError(Error err) {
	if (_delegate) {
		_delegate->handleError(err);
	}
}

void InputLabel::updateCursor() {
	if (_cursor.length == 0 || empty()) {
		if (_enabled) {
			Vec2 cpos;
			if (empty()) {
				cpos = Vec2(0.0f, _contentSize.height - _cursorLayer->getContentSize().height);
			} else {
				cpos = getCursorPosition(_cursor.start);
			}
			if (_inputEnabled) {
				_cursorLayer->setVisible(true);
			}
			_cursorLayer->setPosition(cpos);
			_cursorPointer->setPosition(cpos);

			_cursorSelection->clear();
		} else {
			_cursorLayer->setVisible(false);
		}
	} else {
		_cursorLayer->setVisible(false);
		_cursorStart->setPosition(getCursorPosition(_cursor.start, true));
		_cursorEnd->setPosition(getCursorPosition(_cursor.start + _cursor.length - 1, false));
		_cursorSelection->clear();
		auto rects = _format->getLabelRects(_cursor.start, _cursor.start + _cursor.length - 1, _labelDensity);
		for (auto &rect: rects) {
			_cursorSelection->emplaceRect(rect);
		}
		_cursorSelection->updateColor();
	}

	updatePointer();
	if (_delegate) {
		_delegate->handleCursor(_cursor);
	}
}

bool InputLabel::updateString(const WideStringView &str, const Cursor &c) {
	if (!_delegate || _delegate->handleInputString(str, c)) {
		auto maxChars = getMaxChars();
		if (maxChars > 0) {
			if (maxChars < str.size()) {
				if (_delegate) {
					_delegate->handleInput();
				}
				auto tmpString = WideStringView(str, 0, maxChars);
				_handler.setString(tmpString, c);
				auto ret = updateString(tmpString, _handler.getCursor());
				onError(Error::OverflowChars);
				return ret;
			}
		}

		if (_delegate) {
			for (auto &it : str) {
				if (!_delegate->handleInputChar(it)) {
					_handler.setString(_inputString, _cursor);
					_delegate->handleInput();
					onError(Error::InvalidChar);
					return false;
				}
			}
		}

		bool isInsert = str.size() > _inputString.length();

		_inputString = str.str<Interface>();
		_cursor = c;

		if (_password == PasswordMode::ShowAll || _password == PasswordMode::NotPassword) {
			Label::setString(_inputString);
		} else {
			WideString wstr; wstr.resize(_inputString.length(), u'*');
			Label::setString(wstr);
			if (isInsert) {
				showLastChar();
			}
		}

		if (isLabelDirty()) {
			updateLabel();
		}

		if (_delegate) {
			_delegate->handleInput();
		}
	}

	return true;
}

void InputLabel::updateFocus() {
	if (_inputEnabled) {
		_cursorLayer->setColor(_cursorColor);
		_cursorPointer->setColor(_selectionColor);
		_cursorLayer->setVisible(true);
	} else {
		_cursorLayer->setColor(Color::Grey_500);
		_cursorPointer->setColor(Color::Grey_500);
		_cursorLayer->setVisible(false);
		_cursorPointer->setVisible(false);
		setPointerEnabled(false);
		_cursorSelection->clear();
	}
}

void InputLabel::showLastChar() {
	stopActionByTag(235);
	if (_password == PasswordMode::ShowChar && !_inputString.empty()) {
		WideString str;
		if (_inputString.length() > 1) {
			str.resize(_inputString.length() - 1, u'*');
		}
		str += _inputString.back();
		Label::setString(str);
		stopActionByTag("InputLabelLastChar"_tag);
		runAction(Rc<Sequence>::create(2.0f, std::bind(&InputLabel::hideLastChar, this)), "InputLabelLastChar"_tag);
	}
}

void InputLabel::hideLastChar() {
	if (_password == PasswordMode::ShowChar && !_inputString.empty()) {
		WideString str; str.resize(_inputString.length(), u'*');
		Label::setString(str);
		updateCursor();
	}
}

void InputLabel::scheduleCursorPointer() {
	setPointerEnabled(true);
	stopAllActionsByTag("TextFieldCursorPointer"_tag);
	if (_cursor.length == 0) {
		runAction(Rc<Sequence>::create(3.5f, [this] {
			setPointerEnabled(false);
		}), "TextFieldCursorPointer"_tag);
	}
}

void InputLabel::unscheduleCursorPointer() {
	stopAllActionsByTag("TextFieldCursorPointer"_tag);
}

void InputLabel::setPointerEnabled(bool value) {
	if (_pointerEnabled != value) {
		_pointerEnabled = value;
		updatePointer();
		if (_delegate) {
			_delegate->handlePointer(_pointerEnabled);
		}
	}
}

void InputLabel::updatePointer() {
	if (_pointerEnabled) {
		if (_cursor.length == 0) {
			_cursorPointer->setVisible(true);
			_cursorStart->setVisible(false);
			_cursorEnd->setVisible(false);
		} else {
			_cursorPointer->setVisible(false);
			_cursorStart->setVisible(true);
			_cursorEnd->setVisible(true);
		}
	} else {
		_cursorPointer->setVisible(false);
		_cursorStart->setVisible(false);
		_cursorEnd->setVisible(false);
	}
}

TextInputType InputLabel::getInputTypeValue() const {
	int32_t ret = toInt(_inputType);
	if (_password != PasswordMode::NotPassword) {
		ret |= toInt(InputType::PasswordBit);
	}
	if (_password == PasswordMode::NotPassword && _allowAutocorrect) {
		ret |= toInt(InputType::AutoCorrectionBit);
	}
	if (_allowMultiline) {
		ret |= toInt(InputType::MultiLineBit);
	}
	return TextInputType(ret);
}


void InputLabelContainer::setLabel(Rc<InputLabel> &&l, ZOrder zIndex) {
	if (_label) {
		_label->removeFromParent();
		_label = nullptr;
	}
	if (l) {
		l->setTransformDirtyCallback(std::bind(&InputLabelContainer::onLabelPosition, this));
		_label = addChild(move(l), zIndex);
	}
}

InputLabel *InputLabelContainer::getLabel() const {
	return _label;
}

void InputLabelContainer::update(const UpdateTime &time) {
	if (!_label) {
		return;
	}

	auto labelWidth = _label->getContentSize().width;
	auto width = _contentSize.width;
	auto min = width - labelWidth - 2.0f;
	auto max = 0.0f;
	auto newpos = _label->getPosition().x;

	auto factor = std::min(32.0f, _adjustPosition);

	switch (_adjust) {
	case Left:
		newpos += (45.0f + progress(0.0f, 200.0f, factor / 32.0f)) * time.dt;
		break;
	case Right:
		newpos -= (45.0f + progress(0.0f, 200.0f, factor / 32.0f)) * time.dt;
		break;
	default:
		break;
	}

	if (newpos != _label->getPosition().x) {
		if (newpos < min) {
			newpos = min;
		} else if (newpos > max) {
			newpos = max;
		}
		_label->stopAllActionsByTag("LineFieldAdjust"_tag);
		_label->setPositionX(newpos);
		_label->onSwipe(_adjustValue, Vec2::ZERO);
	}
}

void InputLabelContainer::onCursor() {
	onLabelPosition();
}

void InputLabelContainer::onInput() {
	if (!_label) {
		return;
	}

	auto labelWidth = _label->getContentSize().width;
	auto width = getContentSize().width;
	auto cursor = _label->getCursor();
	if (cursor.start >= _label->getCharsCount()) {
		if (labelWidth > width) {
			runAdjust(width - labelWidth);
			return;
		}
	} else {
		auto pos = _label->getCursorMarkPosition();
		auto labelPos = pos.x + width / 2;
		if (labelWidth > width && labelPos > width) {
			auto min = width - labelWidth;
			auto max = 0.0f;
			auto newpos = width - labelPos;
			if (newpos < min) {
				newpos = min;
			} else if (newpos > max) {
				newpos = max;
			}

			runAdjust(newpos);
			return;
		}
	}

	runAdjust(0.0f);
}

bool InputLabelContainer::onSwipeBegin(const Vec2 &loc, const Vec2 &delta) {
	if (!_label) {
		return false;
	}

	if (_label->onSwipeBegin(loc)) {
		return true;
	}

	auto size = _label->getContentSize();
	if (size.width > _contentSize.width && fabsf(delta.x) > fabsf(delta.y)) {
		_swipeCaptured = true;
		return true;
	}

	return false;
}
bool InputLabelContainer::onSwipe(const Vec2 &loc, const Vec2 &delta) {
	if (!_label) {
		return false;
	}

	if (_swipeCaptured) {
		auto labelWidth = _label->getContentSize().width;
		auto width = _contentSize.width;
		auto min = width - labelWidth - 2.0f;
		auto max = 0.0f;
		auto newpos = _label->getPosition().x + delta.x;
		if (newpos < min) {
			newpos = min;
		} else if (newpos > max) {
			newpos = max;
		}
		_label->stopAllActionsByTag("LineFieldAdjust"_tag);
		_label->setPositionX(newpos);
		return true;
	} else {
		if (_label->onSwipe(loc, delta)) {
			auto labelWidth = _label->getContentSize().width;
			auto width = _contentSize.width;
			if (labelWidth > width) {
				auto pos = convertToNodeSpace(loc);
				if (pos.x < 24.0f) {
					scheduleAdjust(Left, loc, 24.0f - pos.x);
				} else if (pos.x > width - 24.0f) {
					scheduleAdjust(Right, loc, pos.x - (_contentSize.width - 24.0f));
				} else {
					scheduleAdjust(None, loc, 0.0f);
				}
			}
			return true;
		}
		return false;
	}
}

bool InputLabelContainer::onSwipeEnd(const Vec2 &vel) {
	if (_swipeCaptured) {
		_swipeCaptured = false;
		return true;
	} else {
		scheduleAdjust(None, Vec2(0.0f, 0.0f), 0.0f);
		return _label->onSwipeEnd(vel);
	}
}

void InputLabelContainer::onLabelPosition() {
	if (!_label) {
		return;
	}

	float pos;
	Node *node;

	if (_contentSize.width > 0.0f) {
		node = _label->getCursorLayer();
		pos = node->getPosition().x + _label->getPosition().x;
		node->setOpacity(progress(255, 0, math::clamp(math::clamp_distance(pos, 0.0f, _contentSize.width) / 8.0f, 0.0f, 1.0f)));

		node = _label->getCursorPointer();
		pos = node->getPosition().x + _label->getPosition().x;
		node->setOpacity(progress(222, 0, math::clamp(math::clamp_distance(pos, 0.0f, _contentSize.width) / 8.0f, 0.0f, 1.0f)));

		node = _label->getCursorStart();
		pos = node->getPosition().x + _label->getPosition().x;
		node->setOpacity(progress(192, 0, math::clamp(math::clamp_distance(pos, 0.0f, _contentSize.width) / 8.0f, 0.0f, 1.0f)));

		node = _label->getCursorLayer();
		pos = node->getPosition().x + _label->getPosition().x;
		node->setOpacity(progress(192, 0, math::clamp(math::clamp_distance(pos, 0.0f, _contentSize.width) / 8.0f, 0.0f, 1.0f)));
	}
}

void InputLabelContainer::runAdjust(float pos) {
	if (!_label) {
		return;
	}

	auto dist = fabs(_label->getPosition().x - pos);
	auto t = 0.1f;
	if (dist < 20.0f) {
		t = 0.1f;
	} else if (dist > 220.0f) {
		t = 0.35f;
	} else {
		t = progress(0.1f, 0.35f, (dist - 20.0f) / 200.0f);
	}
	auto a = Rc<MoveTo>::create(t, Vec2(pos, _label->getPosition().y));
	_label->stopAllActionsByTag("LineFieldAdjust"_tag);
	_label->runAction(a, "LineFieldAdjust"_tag);
}

void InputLabelContainer::scheduleAdjust(Adjust a, const Vec2 &vec, float pos) {
	_adjustValue = vec;
	_adjustPosition = pos;
	if (a != _adjust) {
		_adjust = a;
		switch (_adjust) {
		case None: unscheduleUpdate(); break;
		default: scheduleUpdate(); break;
		}
	}
}

}
