/**
 Copyright (c) 2026 Stappler Team <admin@stappler.org>

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

#include "XLCommon.h"

#include "widgets/NumberFieldLayout.h"
#include "XLUiStyleResolver.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

namespace {

// `.invalid` is the whole visual vocabulary of a refusal: the engine's CSS subset has no
// `:invalid`, so the widget marks the node with a class and the sheet paints it.
static constexpr auto s_numberCss = StringView(R"css(
number-field {
	width: 220px;
	height: 36px;
	background-color: #292929;
	outline-color: rgba(255,255,255,.15);
	outline-width: 1px;
	border-radius: 6px;
	padding: 0 10px;
	color: #e8e8e8;
	font-size: 14px;
	--caret-color: #fcb400;
	--selection-color: rgba(252,180,0,.35);
}
number-field:focus {
	outline-color: #fcb400;
}
number-field.invalid {
	outline-color: #e53935;
}
label {
	color: #e8e8e8;
	font-size: 14px;
}
)css");

Value ackValue(bool ok) {
	Value ret;
	ret.setBool(ok, "ok");
	return ret;
}

} // namespace

bool NumberFieldLayout::init() {
	if (!TestLayout::init()) {
		return false;
	}

	setStyleSheet(s_numberCss);
	addSystem(Rc<ui::StyleResolver>::create(true));

	_form = addSystem(Rc<ui::FormSystem>::create());

	// Distinct z-orders: the tab ring is document order, which is z-order, and sortAllChildren is
	// an unstable sort.
	auto makeField = [&](StringView name, ZOrder z) {
		auto field = addChild(Rc<ui::NumberField>::create(), z);
		field->setName(name);
		field->setCaretBlink(false);
		field->setValueCallback([this, name = name.str<Interface>()](double value) {
			// emplace/find rather than operator[]: the runtime's map returns an access token from
			// it, which traps on a key that is not there.
			auto it = _callbacks.find(name);
			if (it == _callbacks.end()) {
				_callbacks.emplace(name, 1);
			} else {
				++it->second;
			}

			auto last = _lastValue.find(name);
			if (last == _lastValue.end()) {
				_lastValue.emplace(name, value);
			} else {
				last->second = value;
			}
		});
		return field;
	};

	_integer = makeField("integer", ZOrder(1));
	_integer->setInteger(true);
	_integer->setValue(10.0, true);

	_real = makeField("real", ZOrder(2));
	_real->setStep(0.5);
	_real->setValue(1.5, true);

	// The one the range is about. Whole numbers, so that "typing 1000 is refused" and "dragging
	// past 999 stops at 999" are about the range rather than about rounding.
	_ranged = makeField("ranged", ZOrder(3));
	_ranged->setInteger(true);
	_ranged->setRange(0.0, 999.0);
	_ranged->setValue(100.0, true);

	_formField = makeField("form-number", ZOrder(4));
	_formField->setInteger(true);
	_formField->setValue(7.0, true);
	ui::addFormField(_formField);

	return true;
}

void NumberFieldLayout::handleContentSizeDirty() {
	TestLayout::handleContentSizeDirty();

	const float top = getWorkTop() - 40.0f;
	Node *rows[] = {_integer, _real, _ranged, _formField};
	for (size_t i = 0; i < 4; ++i) {
		if (!rows[i]) {
			continue;
		}
		rows[i]->setAnchorPoint(Vec2(0.0f, 1.0f));
		rows[i]->setPosition(Vec2(48.0f, top - float(i) * 56.0f));
	}
}

ui::NumberField *NumberFieldLayout::getTarget(const Value &args) const {
	auto name = args.getString("target");
	if (name == "real") {
		return _real;
	} else if (name == "ranged") {
		return _ranged;
	} else if (name == "form-number") {
		return _formField;
	}
	return _integer;
}

Value NumberFieldLayout::encodeField(ui::NumberField *field) const {
	Value ret;
	if (!field) {
		return ret;
	}

	ret.setDouble(field->getValue(), "value");
	ret.setString(field->getText(), "text");
	ret.setBool(field->isValid(), "valid");
	ret.setString(field->getValidationMessage(), "message");
	ret.setBool(field->isInteger(), "integer");
	ret.setDouble(field->getStep(), "step");
	ret.setBool(field->isFocused(), "focused");
	ret.setBool(field->isDragging(), "dragging");

	ret.setString(field->getUnit(), "unit");

	// What the field was TOLD and what it SHOWS are the two halves of setUnit, and only the second
	// is worth checking - so this is read off the label node, not out of the widget's string.
	String unitText;
	for (auto &child : field->getChildren()) {
		if (child->getType() == "field-unit") {
			auto label = static_cast<basic2d::Label *>(child.get());
			if (label->isVisible()) {
				unitText = string::toUtf8<Interface>(label->getString());
			}
		}
	}
	ret.setString(unitText, "unitText");

	// The proof that the unit took room out of the TEXT rather than being drawn on top of it.
	if (auto container = field->getContainer()) {
		ret.setDouble(container->getContentSize().width, "viewportWidth");
	}

	if (field->hasRange()) {
		ret.setDouble(field->getMin(), "min");
		ret.setDouble(field->getMax(), "max");
	}

	// The class the sheet paints a refusal from - checked as a class rather than as a colour,
	// because a colour is a screenshot and a class is a fact.
	Value classes;
	if (auto set = field->getStyleClasses()) {
		for (auto &it : *set) { classes.addString(it); }
	}
	ret.setValue(sp::move(classes), "classes");

	auto name = field->getName().str<Interface>();
	auto cb = _callbacks.find(name);
	ret.setInteger(cb == _callbacks.end() ? 0 : int64_t(cb->second), "callbacks");
	auto last = _lastValue.find(name);
	if (last != _lastValue.end()) {
		ret.setDouble(last->second, "lastValue");
	}
	return ret;
}

Value NumberFieldLayout::encodeState() const {
	Value ret;
	ret.setValue(encodeField(_integer), "integer");
	ret.setValue(encodeField(_real), "real");
	ret.setValue(encodeField(_ranged), "ranged");
	ret.setValue(encodeField(_formField), "formField");
	if (_form) {
		ret.setValue(_form->collect(), "collected");
	}
	return ret;
}

void NumberFieldLayout::registerCommands() {
	addCommand("state", "Report every field: value, text, validity, message and callback count",
			[this](Value &&) { return encodeState(); });

	addCommand("set", "Assign a value programmatically: {target, value}", [this](Value &&args) {
		auto field = getTarget(args);
		if (!field) {
			return ackValue(false);
		}
		field->setValue(static_cast<const Value &>(args).getDouble("value"));
		return ackValue(true);
	});

	addCommand("set-text",
			"Write the text the way typing would, without a keyboard: {target, value}",
			[this](Value &&args) {
		auto field = getTarget(args);
		if (!field) {
			return ackValue(false);
		}
		field->setText(static_cast<const Value &>(args).getString("value"));
		return ackValue(true);
	});

	addCommand("focus", "Focus a field, or blur it: {target, value}", [this](Value &&args) {
		auto field = getTarget(args);
		if (!field) {
			return ackValue(false);
		}
		const Value &a = args;
		if (!a.isBool("value") || a.getBool("value")) {
			field->focus();
		} else {
			field->blur();
		}
		return ackValue(true);
	});

	addCommand("set-range", "Declare or drop a range: {target, min, max} - no min/max clears it",
			[this](Value &&args) {
		auto field = getTarget(args);
		if (!field) {
			return ackValue(false);
		}
		const Value &a = args;
		if (a.isDouble("min") || a.isInteger("min")) {
			field->setRange(a.getDouble("min"), a.getDouble("max"));
		} else {
			field->clearRange();
		}
		return ackValue(true);
	});

	addCommand("set-drag", "Turn the scrub gesture on or off: {target, value}",
			[this](Value &&args) {
		auto field = getTarget(args);
		if (!field) {
			return ackValue(false);
		}
		field->setDragEnabled(static_cast<const Value &>(args).getBool("value"));
		return ackValue(true);
	});

	addCommand("roundtrip",
			"parse(format(v)) for a list of values: {values} - answers what came back",
			[this](Value &&args) {
		auto field = getTarget(args);
		if (!field) {
			return ackValue(false);
		}

		// The reversibility check runs INSIDE the widget rather than against a reimplementation of
		// its formatting in the script: a pair that is not reversible is a field that changes the
		// number by being looked at, and only the widget knows how it spells one.
		Value ret;
		for (auto &it : static_cast<const Value &>(args).getValue("values").asArray()) {
			const auto value = it.getDouble();
			field->setValue(value, true);
			Value entry;
			entry.setDouble(value, "in");
			entry.setString(field->getText(), "text");
			entry.setDouble(field->getValue(), "out");
			ret.addValue(sp::move(entry));
		}
		return ret;
	});

	addCommand("set-unit", "Set or clear a field's unit label: {target, value}",
			[this](Value &&args) {
		auto field = getTarget(args);
		if (!field) {
			return ackValue(false);
		}
		field->setUnit(static_cast<const Value &>(args).getString("value"));
		return ackValue(true);
	});

	addCommand("reset-counters", "Zero every field's callback count", [this](Value &&) {
		_callbacks.clear();
		_lastValue.clear();
		return ackValue(true);
	});
}

} // namespace stappler::xenolith::app
