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

#include "XLUiNumberField.h"

#include "XLInheritedStyle.h" // placeInlineEnd

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

// Gap between the number and its unit, matching ui::VectorField.
static constexpr float s_numberUnitGap = 4.0f;

double scrubSteps(float travel, float sensitivity) {
	if (sensitivity <= 0.0f) {
		return 0.0;
	}
	return double(sprt::trunc(travel / sensitivity));
}

double scrubValue(double base, double steps, double step, bool integer, const ScrubRange &range) {
	double value = base + step * steps;
	if (integer) {
		value = sprt::trunc(value);
	}
	if (range.has) {
		value = sprt::clamp(value, range.min, range.max);
	}
	return value;
}

NumberField::~NumberField() { }

bool NumberField::init() {
	if (!TextInput::init()) {
		return false;
	}

	setType("number-field");
	removeStyleClass("xl-ui-text-input");
	addStyleClass("xl-ui-number-field");
	// TextInput's appliers, registered under this type too
	registerStyleAppliers("number-field");

	// IME hint only (raises a numeric keypad); filtering is in handleInputChar
	setInputType(TextInputType::Number_Decimial);

	// PageUp/PageDown are not in TextInput's key mask, so they get their own recognizer
	InputKeyMask keys;
	keys.set(toInt(InputKeyCode::PAGE_UP));
	keys.set(toInt(InputKeyCode::PAGE_DOWN));
	_listener->addKeyRecognizer([this](const GestureData &data) { return handleKey(data); },
			InputKeyInfo{sp::move(keys)});

	updateText();
	return true;
}

void NumberField::setInteger(bool value) {
	if (_integer == value) {
		return;
	}
	_integer = value;
	setInputType(_integer ? TextInputType::Number_Numbers : TextInputType::Number_Decimial);

	// truncate the value itself, not only its text
	if (_integer) {
		setValue(sprt::trunc(_value), true);
	} else {
		updateText();
	}
}

void NumberField::setRange(double min, double max) {
	if (max < min) {
		sprt::swap(min, max);
	}
	_min = min;
	_max = max;
	_hasRange = true;

	// the held value is not clamped; the range applies to what is typed and dragged from now on
	commit();
}

void NumberField::clearRange() {
	if (!_hasRange) {
		return;
	}
	_hasRange = false;
	commit();
}

void NumberField::setStep(double value) {
	if (value <= 0.0) {
		return;
	}
	_step = value;
}

void NumberField::setValue(double value, bool silent) {
	if (_integer) {
		value = sprt::trunc(value);
	}
	const bool changed = value != _value;
	_value = value;

	updateText();
	setInvalid(false, StringView());

	if (changed && !silent && _valueCallback) {
		_valueCallback(_value);
	}
}

void NumberField::setValueCallback(ValueCallback &&cb) { _valueCallback = sp::move(cb); }

void NumberField::setUnit(StringView value) {
	if (StringView(_unit) == value) {
		return;
	}
	_unit = value.str<Interface>();

	if (!_unitLabel) {
		if (_unit.empty()) {
			// no label yet and nothing to show
			return;
		}
		// explicit ZOrder above the viewport
		_unitLabel = addChild(Rc<basic2d::Label>::create(), ZOrder(2));
		// type shared with ui::VectorField; use `number-field > field-unit` to target this one
		_unitLabel->setType("field-unit");
		_unitLabel->addStyleClass("xl-ui-field-unit");
		_unitLabel->setAlignment(font::TextAlign::Left);
	}

	_unitLabel->setString(_unit);
	_unitLabel->setVisible(!_unit.empty());
	_contentSizeDirty = true;
}

Padding NumberField::getViewportInset() const { return Padding().setRight(_unitInset); }

void NumberField::handleContentSizeDirty() {
	// Shape the label now, before the base sizes the viewport: its own update runs after this pass
	// and its width would read zero.
	if (_unitLabel && _unitLabel->isVisible()) {
		_unitLabel->tryUpdateLabel();
		_unitInset = _unitLabel->getContentSize().width + s_numberUnitGap;
	} else {
		_unitInset = 0.0f;
	}

	TextInput::handleContentSizeDirty();

}

void NumberField::handleLayoutChildren() {
	TextInput::handleLayoutChildren();
	placeUnitLabel();
}

void NumberField::placeUnitLabel() {
	if (!_unitLabel || !_unitLabel->isVisible()) {
		return;
	}

	TextInputStyleComponent defaultStyle;
	const TextInputStyleComponent *style = &defaultStyle;
	if (auto c = getComponent<TextInputStyleComponent>()) {
		style = c;
	}

	/* At the inner padding edge on the inline end, on the viewport's centre line; not a child of
	the container, so its scissor does not clip it. Runs from handleLayoutChildren: the resolved
	direction is not yet current in handleContentSizeDirty. */
	const bool rtl = isInlineRtl(this);
	const float endPad = rtl ? style->padding.left : style->padding.right;
	placeInlineEnd(_unitLabel, endPad, _contentSize.height / 2.0f, _contentSize.width, rtl);
}

void NumberField::setDragEnabled(bool value) {
	if (_dragEnabled == value) {
		return;
	}
	_dragEnabled = value;
	if (!_dragEnabled) {
		_dragging = false;
	}
}

void NumberField::setDragSensitivity(float value) {
	if (value > 0.0f) {
		_dragSensitivity = value;
	}
}

String NumberField::formatValue(double value) const {
	if (_integer) {
		return toString(int64_t(value));
	}
	return toString(value);
}

void NumberField::updateText() {
	auto text = formatValue(_value);
	if (StringView(text) == getText()) {
		return;
	}

	// guard: the echo of this write comes back through handleTextInput
	_inUpdate = true;
	setText(text);
	_inUpdate = false;
}

double NumberField::stepped(double base, double steps) const {
	// the drag and the arrows clamp, unlike typing
	return scrubValue(base, steps, _step, _integer, ScrubRange{_hasRange, _min, _max});
}

void NumberField::setInvalid(bool value, StringView message) {
	_message = message.str<Interface>();
	if (_valid == !value) {
		return;
	}
	_valid = !value;
	if (value) {
		applyControlInvalid(this, true);
	} else {
		applyControlInvalid(this, false);
	}
}

bool NumberField::commit() {
	auto text = getText();

	// empty text is not a refusal: the value is kept while retyping
	if (text.empty()) {
		setInvalid(false, StringView());
		return false;
	}

	StringView reader(text);
	double parsed = 0.0;
	if (!reader.readDouble().grab(parsed)) {
		setInvalid(true, StringView("not a number"));
		return false;
	}

	// the whole text must be the number ("12ab" is refused)
	reader.skipChars<StringView::CharGroup<CharGroupId::WhiteSpace>>();
	if (!reader.empty()) {
		setInvalid(true, StringView("not a number"));
		return false;
	}

	if (_integer && parsed != sprt::trunc(parsed)) {
		setInvalid(true, StringView("must be a whole number"));
		return false;
	}

	// typed out of range is refused (dragging clamps instead)
	if (_hasRange && (parsed < _min || parsed > _max)) {
		setInvalid(true, toString("must be between ", _min, " and ", _max));
		return false;
	}

	setInvalid(false, StringView());

	if (parsed == _value) {
		return false;
	}

	_value = parsed;
	if (_valueCallback) {
		_valueCallback(_value);
	}
	return true;
}

void NumberField::handleTextInput(const TextInputState &state) {
	TextInput::handleTextInput(state);

	if (_inUpdate) {
		return;
	}

	commit();

	// the platform can end input without blur() (Escape); restore unparsable text here too
	if (!_focused && !_valid) {
		updateText();
		setInvalid(false, StringView());
	}
}

void NumberField::setText(WideStringView str) {
	TextInput::setText(str);

	// skip writes made by updateText()
	if (!_inUpdate) {
		commit();
	}
}

void NumberField::blur() {
	TextInput::blur();

	// restore the value's text when editing ends with unparsable text
	if (!_valid) {
		updateText();
		setInvalid(false, StringView());
	}
}

bool NumberField::handleInputChar(char16_t c) {
	if (c >= u'0' && c <= u'9') {
		return true;
	}
	switch (c) {
	case u'-': return true;
	// formatValue may produce an exponent or a fraction, so they are accepted for reals
	case u'+':
	case u'.':
	case u'e':
	case u'E': return !_integer;
	default: break;
	}
	return false;
}

bool NumberField::handleKey(const GestureData &data) {
	if (!_focused || !data.input) {
		return false;
	}

	const auto &ev = data.input->data;
	if (ev.event != InputEventName::KeyPressed && ev.event != InputEventName::KeyRepeated) {
		return false;
	}

	// Up/Down step the value instead of moving the caret to the line ends
	switch (ev.key.keycode) {
	case InputKeyCode::UP: setValue(stepped(_value, 1.0)); return true;
	case InputKeyCode::DOWN: setValue(stepped(_value, -1.0)); return true;
	case InputKeyCode::PAGE_UP: setValue(stepped(_value, 10.0)); return true;
	case InputKeyCode::PAGE_DOWN: setValue(stepped(_value, -10.0)); return true;
	default: break;
	}

	return TextInput::handleKey(data);
}

bool NumberField::handleSwipeBegin(const Vec2 &location) {
	// a focused field is dragged to select text; scrubbing applies only when unfocused
	if (_focused || !_dragEnabled || isReadOnly()) {
		return TextInput::handleSwipeBegin(location);
	}

	_dragging = true;
	_dragTravel = 0.0f;
	_dragOrigin = _value;
	return true;
}

bool NumberField::handleSwipe(const Vec2 &location, const Vec2 &delta) {
	if (!_dragging) {
		return TextInput::handleSwipe(location, delta);
	}

	// accumulated, so sub-step movements are not lost
	_dragTravel += delta.x;

	const double steps = scrubSteps(_dragTravel, _dragSensitivity);
	auto value = stepped(_dragOrigin, steps);
	if (value != _value) {
		// not silent: live feedback; grouping into one undo entry is the owner's job
		setValue(value);
	}
	return true;
}

bool NumberField::handleSwipeEnd() {
	if (!_dragging) {
		return TextInput::handleSwipeEnd();
	}
	_dragging = false;
	return true;
}

} // namespace stappler::xenolith::ui
