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

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

// The class a refused value is painted with. There is no `:invalid` pseudo-class in the engine's
// CSS subset, so this is the only way to say it in a stylesheet - and it is the same word
// ui::FormSystem marks a rejected field with.

// Between the number and its unit. The same gap ui::VectorField leaves between a component and its
// label, because the two read as one row when they sit side by side.
static constexpr float s_numberUnitGap = 4.0f;

NumberField::~NumberField() { }

bool NumberField::init() {
	if (!TextInput::init()) {
		return false;
	}

	setType("number-field");
	removeStyleClass("xl-ui-text-input");
	addStyleClass("xl-ui-number-field");
	// The same appliers TextInput registers for itself, under this type as well: a number field
	// must be stylable without every rule having to say `text-input.number`.
	registerStyleAppliers("number-field");

	// A hint to the platform IME, and only a hint - the filtering is handleInputChar's job. It is
	// still worth setting: on a touch platform it is what raises a numeric keypad.
	setInputType(TextInputType::Number_Decimial);

	// PageUp/PageDown are not in TextInput's key mask, so they get a recognizer of their own rather
	// than a second copy of the base mask that would have to be kept in step with it.
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

	// The value itself is truncated, not just its spelling: a field that says 3 and holds 3.5 is
	// two different answers to the same question.
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

	// Nothing is refused retroactively: a value the program set before the range existed stays,
	// and the range starts governing what is typed and dragged from here on. Clamping here would
	// change a stored value as a side effect of describing it.
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
			// Never named a unit, so there is nothing to build and nothing to hide.
			return;
		}
		// ZOrder above the viewport: the two never overlap, but the order has to be said rather
		// than inherited from the order of construction.
		_unitLabel = addChild(Rc<basic2d::Label>::create(), ZOrder(2));
		// Its own type, shared with ui::VectorField's, so one rule styles the unit wherever it
		// appears; a sheet that needs to tell them apart writes `number-field > field-unit`.
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
	// Measured NOW, before the base sizes the viewport against it: a label shapes itself on its own
	// update, which runs AFTER this pass, so without asking for it here its width would read zero
	// and the number would run underneath it. Same reason, same call, as ui::VectorField's
	// component labels.
	if (_unitLabel && _unitLabel->isVisible()) {
		_unitLabel->tryUpdateLabel();
		_unitInset = _unitLabel->getContentSize().width + s_numberUnitGap;
	} else {
		_unitInset = 0.0f;
	}

	TextInput::handleContentSizeDirty();

	if (_unitLabel && _unitLabel->isVisible()) {
		TextInputStyleComponent defaultStyle;
		const TextInputStyleComponent *style = &defaultStyle;
		if (auto c = getComponent<TextInputStyleComponent>()) {
			style = c;
		}

		// Against the inner edge of the padding, on the viewport's centre line. Not a child of the
		// container, so the container's scissor never clips it.
		_unitLabel->setAnchorPoint(Anchor::MiddleRight);
		_unitLabel->setPosition(Vec2(sprt::max(_contentSize.width - style->padding.right, 0.0f),
				_contentSize.height / 2.0f));
	}
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

	// The echo of this write comes back through handleTextInput, and the field agreeing with itself
	// is not an edit: without the guard the restored text would be re-read as one.
	_inUpdate = true;
	setText(text);
	_inUpdate = false;
}

double NumberField::stepped(double base, double steps) const {
	double value = base + _step * steps;
	if (_integer) {
		value = sprt::trunc(value);
	}
	// The drag and the arrows CLAMP, unlike typing. A gesture has no wrong state to be in, and
	// stopping at the end of the range is what the range is for.
	if (_hasRange) {
		value = sprt::clamp(value, _min, _max);
	}
	return value;
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

	// An empty field is not a refusal - it is a field somebody is in the middle of retyping. It
	// holds its value and says nothing.
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

	// The WHOLE text has to be the number: "12ab" reading as 12 would accept a value nobody typed.
	reader.skipChars<StringView::CharGroup<CharGroupId::WhiteSpace>>();
	if (!reader.empty()) {
		setInvalid(true, StringView("not a number"));
		return false;
	}

	if (_integer && parsed != sprt::trunc(parsed)) {
		setInvalid(true, StringView("must be a whole number"));
		return false;
	}

	// Typed out of range is REFUSED, where dragged out of range is clamped. See the class comment:
	// correcting what somebody typed shows them a number they did not write.
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

	// The platform can take input away without anyone calling blur() (Escape cancels it), and a
	// field left holding text that does not parse would say one thing on screen and another
	// through getValue().
	if (!_focused && !_valid) {
		updateText();
		setInvalid(false, StringView());
	}
}

void NumberField::setText(WideStringView str) {
	TextInput::setText(str);

	// Not while updateText() is the one writing: that is the field agreeing with itself.
	if (!_inUpdate) {
		commit();
	}
}

void NumberField::blur() {
	TextInput::blur();

	// What is on screen and what the field holds must agree the moment it stops being edited.
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
	// The exponent and the fractional part are only a spelling of a real number, and formatValue
	// may produce either - a filter that refused them would break parse(format(v)) == v.
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

	// Up and Down are the step here, where TextInput reads them as "to the start / to the end of
	// the line". A single-line number has nowhere to go vertically, and the step is what those keys
	// mean on every numeric field there has ever been.
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
	// A focused field is dragged to select text, and the base class already does that. Scrubbing
	// only takes over where there is no selection to make.
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

	// Accumulated, not applied per frame: a movement shorter than one step has to be remembered,
	// or a slow drag rounds to nothing on every frame and the value never moves.
	_dragTravel += delta.x;

	const double steps = double(sprt::trunc(_dragTravel / _dragSensitivity));
	auto value = stepped(_dragOrigin, steps);
	if (value != _value) {
		// Not silent: a drag with no live feedback is a drag nobody can aim. Grouping the whole
		// gesture into one undo entry is the owner's business, not the widget's.
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
