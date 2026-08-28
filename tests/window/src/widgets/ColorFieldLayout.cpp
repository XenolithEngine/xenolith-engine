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

#include "widgets/ColorFieldLayout.h"
#include "XLUiStyleResolver.h"
#include "XLInteractiveComponent.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

namespace {

// The picker's own surface is a scene of its own on the native path, and headless takes the native
// path - so what it looks like there comes from the widget's built-in metrics, not from this sheet.
static constexpr auto s_colorCss = StringView(R"css(
color-field {
	width: 260px;
	height: 36px;
	background-color: #292929;
	outline-color: rgba(255,255,255,.15);
	outline-width: 1px;
	border-radius: 6px;
}
color-field:focus {
	outline-color: #fcb400;
}
color-field:invalid {
	outline-color: #e53935;
}
color-field > text-input {
	color: #e8e8e8;
	font-size: 14px;
	--caret-color: #fcb400;
	--selection-color: rgba(252,180,0,.35);
}
text-input {
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

StringView modeName(ui::ColorField::PickerMode mode) {
	switch (mode) {
	case ui::ColorField::PickerMode::Auto: return StringView("auto");
	case ui::ColorField::PickerMode::System: return StringView("system");
	case ui::ColorField::PickerMode::Fallback: return StringView("fallback");
	}
	return StringView();
}

Value encodeRect(Node *node) {
	Value ret;
	if (!node) {
		return ret;
	}
	auto size = node->getContentSize();
	auto center = node->convertToWorldSpace(Vec2(size.width / 2.0f, size.height / 2.0f));
	ret.setDouble(double(center.x), "cx");
	ret.setDouble(double(center.y), "cy");
	ret.setDouble(double(size.width), "width");
	ret.setDouble(double(size.height), "height");
	return ret;
}

} // namespace

bool ColorFieldLayout::init() {
	if (!TestLayout::init()) {
		return false;
	}

	setStyleSheet(s_colorCss);
	addSystem(Rc<ui::StyleResolver>::create(true));

	auto makeField = [&](StringView name, ZOrder z, Node *parent) {
		auto field = parent->addChild(Rc<ui::ColorField>::create(), z);
		field->setName(name);
		field->getInput()->setCaretBlink(false);
		field->setValueCallback([this, name = name.str<Interface>()](const Color4B &value) {
			// emplace/find rather than operator[]: the runtime's map returns an access token from
			// it, which traps on a key that is not there.
			auto it = _callbacks.find(name);
			if (it == _callbacks.end()) {
				_callbacks.emplace(name, 1);
			} else {
				++it->second;
			}

			auto text = ui::ColorField::formatColor(value, true);
			auto last = _lastValue.find(name);
			if (last == _lastValue.end()) {
				_lastValue.emplace(name, text);
			} else {
				last->second = text;
			}
		});
		return field;
	};

	_plain = makeField("plain", ZOrder(1), this);
	_plain->setValue(Color4B(0x1E, 0x88, 0xE5, 0xFF), true);

	// Its own palette, so that "the surface shows what the palette holds" is a statement about the
	// palette rather than about the built-in default.
	_alpha = makeField("alpha", ZOrder(2), this);
	_alpha->setAlphaEnabled(true);
	_alpha->setValue(Color4B(0xFF, 0x00, 0xAA, 0x80), true);
	Color4B palette[] = {
		Color4B(0xFF, 0x00, 0x00, 0xFF),
		Color4B(0x00, 0xFF, 0x00, 0xFF),
		Color4B(0x00, 0x00, 0xFF, 0xFF),
	};
	_alpha->setPalette(makeSpanView(palette, 3));

	/* The form is on a node of its own - see the class comment. */
	_formPanel = addChild(Rc<Node>::create(), ZOrder(3));
	_form = _formPanel->addSystem(Rc<ui::FormSystem>::create());

	// Distinct z-orders: the tab ring is document order, which is z-order, and sortAllChildren is
	// an unstable sort.
	_formField = makeField("form-color", ZOrder(1), _formPanel);
	_formField->setValue(Color4B(0x43, 0xA0, 0x47, 0xFF), true);
	ui::addFormField(_formField);

	_neighbour = _formPanel->addChild(Rc<ui::TextInput>::create(), ZOrder(2));
	_neighbour->setName("neighbour");
	_neighbour->setCaretBlink(false);
	_neighbour->setText("abc");
	ui::addFormField(_neighbour);

	return true;
}

void ColorFieldLayout::handleContentSizeDirty() {
	TestLayout::handleContentSizeDirty();

	if (_formPanel) {
		_formPanel->setAnchorPoint(Vec2(0.0f, 0.0f));
		_formPanel->setPosition(Vec2(0.0f, 0.0f));
		_formPanel->setContentSize(_contentSize);
	}

	const float top = getWorkTop() - 40.0f;
	Node *rows[] = {_plain, _alpha, _formField, _neighbour};
	for (size_t i = 0; i < 4; ++i) {
		if (!rows[i]) {
			continue;
		}
		rows[i]->setAnchorPoint(Vec2(0.0f, 1.0f));
		rows[i]->setPosition(Vec2(48.0f, top - float(i) * 56.0f));
	}
}

ui::ColorField *ColorFieldLayout::getTarget(const Value &args) const {
	auto name = args.getString("target");
	if (name == "alpha") {
		return _alpha;
	} else if (name == "form-color") {
		return _formField;
	}
	return _plain;
}

Value ColorFieldLayout::encodeField(ui::ColorField *field) const {
	Value ret;
	if (!field) {
		return ret;
	}

	const auto value = field->getValue();
	ret.setString(field->formatValue(), "value");

	Value rgba;
	rgba.addInteger(int64_t(value.r));
	rgba.addInteger(int64_t(value.g));
	rgba.addInteger(int64_t(value.b));
	rgba.addInteger(int64_t(value.a));
	ret.setValue(sp::move(rgba), "rgba");

	ret.setString(field->getInput()->getText(), "text");
	ret.setBool(field->isValid(), "valid");
	ret.setString(field->getValidationMessage(), "message");

	// The OTHER channel: why the picker did not open. Reported separately from `message` on
	// purpose - the whole point of the split is that a capability refusal is not a bad value.
	ret.setBool(field->isUnavailable(), "unavailable");
	ret.setString(field->getUnavailableMessage(), "unavailableMessage");
	ret.setBool(field->isAlphaEnabled(), "alpha");
	ret.setBool(field->isEnabled(), "enabled");
	ret.setBool(field->isFocused(), "focused");
	ret.setString(modeName(field->getPickerMode()), "mode");

	// What `Auto` asks, and the whole reason the mode exists.
	ret.setBool(field->isSystemPickerAvailable(), "systemAvailable");
	ret.setBool(field->isOpen(), "open");
	if (auto picker = field->getPicker()) {
		ret.setString(picker->getId(), "pickerWindow");
	}

	Value palette;
	for (auto &it : field->getPalette()) {
		palette.addString(ui::ColorField::formatColor(it, false));
	}
	ret.setValue(sp::move(palette), "palette");

	Value classes;
	if (auto set = field->getStyleClasses()) {
		for (auto &it : *set) { classes.addString(it); }
	}
	ret.setValue(sp::move(classes), "classes");

	// The state a stylesheet selects on; the classes above are the widget's own words (`open`,
	// `unavailable`), which have no pseudo-class to become.
	if (auto ic = field->getComponent<InteractiveComponent>()) {
		ret.setBool(sprt::hasFlag(ic->state, InteractiveState::Invalid), "invalidState");
	} else {
		ret.setBool(false, "invalidState");
	}

	// The swatch paints the VALUE, and that is the one thing about this widget worth reading off
	// the node rather than off the model.
	for (auto &child : field->getChildren()) {
		if (child->getName() == "swatch") {
			ret.setString(ui::ColorField::formatColor(
								  static_cast<basic2d::LayerRounded *>(child.get())->getPathColor(),
								  true),
					"swatchColor");
			break;
		}
	}

	ret.setValue(encodeRect(field), "rect");
	if (auto input = field->getInput()) {
		ret.setValue(encodeRect(input), "inputRect");
	}

	auto name = field->getName().str<Interface>();
	auto cb = _callbacks.find(name);
	ret.setInteger(cb == _callbacks.end() ? 0 : int64_t(cb->second), "callbacks");
	auto last = _lastValue.find(name);
	if (last != _lastValue.end()) {
		ret.setString(last->second, "lastValue");
	}
	return ret;
}

Value ColorFieldLayout::encodeState() const {
	Value ret;
	ret.setValue(encodeField(_plain), "plain");
	ret.setValue(encodeField(_alpha), "alpha");
	ret.setValue(encodeField(_formField), "formField");

	if (_neighbour) {
		ret.setString(_neighbour->getText(), "neighbourText");
		ret.setInteger(int64_t(_neighbour->getCursor().start), "neighbourCursor");
		ret.setBool(_neighbour->isFocused(), "neighbourFocused");
	}

	if (_form) {
		ret.setValue(_form->collect(), "collected");
		if (auto focused = _form->getFocusedField()) {
			ret.setString(focused->getFieldName(), "formFocus");
		}
	}
	return ret;
}

void ColorFieldLayout::registerCommands() {
	addCommand("state",
			"Report every field: value, text, validity, picker mode and what the form " "collects",
			[this](Value &&) { return encodeState(); });

	addCommand("set", "Assign a colour as text: {target, value}", [this](Value &&args) {
		auto field = getTarget(args);
		if (!field) {
			return ackValue(false);
		}
		return ackValue(
				field->setValueFromString(static_cast<const Value &>(args).getString("value")));
	});

	addCommand("set-text",
			"Write the hex line the way typing would, without committing: " "{target, value}",
			[this](Value &&args) {
		auto field = getTarget(args);
		if (!field) {
			return ackValue(false);
		}
		field->getInput()->setText(static_cast<const Value &>(args).getString("value"));
		return ackValue(true);
	});

	addCommand("focus", "Focus the hex line, or blur it: {target, value}", [this](Value &&args) {
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

	addCommand("focus-neighbour", "Put the caret in the field beside the colours",
			[this](Value &&) {
		if (!_neighbour) {
			return ackValue(false);
		}
		_neighbour->focus();
		return ackValue(true);
	});

	addCommand("open", "Open the picker: {target}", [this](Value &&args) {
		auto field = getTarget(args);
		return ackValue(field ? field->open() : false);
	});

	addCommand("close", "Dismiss the picker: {target}", [this](Value &&args) {
		auto field = getTarget(args);
		if (!field) {
			return ackValue(false);
		}
		field->close();
		return ackValue(true);
	});

	addCommand("set-mode", "Which picker a tap opens: {target, value} - auto/system/fallback",
			[this](Value &&args) {
		auto field = getTarget(args);
		if (!field) {
			return ackValue(false);
		}
		auto value = static_cast<const Value &>(args).getString("value");
		if (value == "system") {
			field->setPickerMode(ui::ColorField::PickerMode::System);
		} else if (value == "fallback") {
			field->setPickerMode(ui::ColorField::PickerMode::Fallback);
		} else {
			field->setPickerMode(ui::ColorField::PickerMode::Auto);
		}
		return ackValue(true);
	});

	addCommand("set-alpha", "Turn the alpha channel on or off: {target, value}",
			[this](Value &&args) {
		auto field = getTarget(args);
		if (!field) {
			return ackValue(false);
		}
		field->setAlphaEnabled(static_cast<const Value &>(args).getBool("value"));
		return ackValue(true);
	});

	addCommand("assign", "Assign the form's value: {value}", [this](Value &&args) {
		if (!_form) {
			return ackValue(false);
		}
		_form->assign(static_cast<const Value &>(args).getValue("value"));
		return ackValue(true);
	});

	addCommand("roundtrip", "format(v) and what reading it back gives: {values}",
			[this](Value &&args) {
		auto field = getTarget(args);
		if (!field) {
			return ackValue(false);
		}

		/* The reversibility check runs INSIDE the widget rather than against a reimplementation of
		its formatting in the script: a pair that is not reversible is a field that changes the
		colour by being looked at. */
		Value ret;
		for (auto &it : static_cast<const Value &>(args).getValue("values").asArray()) {
			const auto in = it.getString();
			Value entry;
			entry.setString(in, "in");
			if (field->setValueFromString(in, true)) {
				entry.setString(field->formatValue(), "text");
				const auto text = field->formatValue();
				entry.setBool(field->setValueFromString(text, true), "reread");
				entry.setString(field->formatValue(), "out");
			} else {
				entry.setBool(false, "read");
			}
			ret.addValue(sp::move(entry));
		}
		return ret;
	});

	addCommand("reset-counters", "Zero every field's callback count", [this](Value &&) {
		_callbacks.clear();
		_lastValue.clear();
		return ackValue(true);
	});
}

} // namespace stappler::xenolith::app
