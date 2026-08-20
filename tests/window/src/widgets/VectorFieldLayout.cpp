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

#include "widgets/VectorFieldLayout.h"
#include "XLUiStyleResolver.h"
#include "XLUiInteractiveComponent.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

namespace {

// The row itself is the surface; the components are the fields in it. `.invalid` on the ROW is the
// whole visual vocabulary of a refusal anywhere inside it - the CSS subset has no `:invalid`.
static constexpr auto s_vectorCss = StringView(R"css(
vector-field {
	width: 420px;
	height: 36px;
	background-color: #202020;
	outline-color: rgba(255,255,255,.10);
	outline-width: 1px;
	border-radius: 6px;
}
vector-field:focus {
	outline-color: #fcb400;
}
vector-field.invalid {
	outline-color: #e53935;
}
vector-field > number-field {
	height: 28px;
	background-color: #292929;
	outline-color: rgba(255,255,255,.15);
	outline-width: 1px;
	border-radius: 4px;
	padding: 0 6px;
	color: #e8e8e8;
	font-size: 14px;
	--caret-color: #fcb400;
	--selection-color: rgba(252,180,0,.35);
}
vector-field > number-field:focus {
	outline-color: #fcb400;
}
component-label {
	color: #9e9e9e;
	font-size: 13px;
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

// In SCENE space, which is what the inspector's injected pointer events are in. Reported rather
// than recomputed in the script: the placement is the widget's, and a second copy of it in Python
// would be a check against a reimplementation instead of against the widget.
Value encodeRect(Node *node) {
	Value ret;
	if (!node) {
		return ret;
	}
	auto size = node->getContentSize();
	auto origin = node->convertToWorldSpace(Vec2(0.0f, 0.0f));
	auto center = node->convertToWorldSpace(Vec2(size.width / 2.0f, size.height / 2.0f));
	ret.setDouble(double(origin.x), "x");
	ret.setDouble(double(origin.y), "y");
	ret.setDouble(double(size.width), "width");
	ret.setDouble(double(size.height), "height");
	ret.setDouble(double(center.x), "cx");
	ret.setDouble(double(center.y), "cy");
	return ret;
}

} // namespace

bool VectorFieldLayout::init() {
	if (!TestLayout::init()) {
		return false;
	}

	setStyleSheet(s_vectorCss);
	addSystem(Rc<ui::StyleResolver>::create(true));

	auto makeRow = [&](StringView name, uint32_t arity, ZOrder z, Node *parent) {
		auto field = parent->addChild(Rc<ui::VectorField>::create(arity), z);
		field->setName(name);
		for (uint32_t i = 0; i < arity; ++i) { field->getComponentAt(i)->setCaretBlink(false); }
		field->setValueCallback([this, name = name.str<Interface>()](SpanView<double> values) {
			// emplace/find rather than operator[]: the runtime's map returns an access token from
			// it, which traps on a key that is not there.
			auto it = _callbacks.find(name);
			if (it == _callbacks.end()) {
				_callbacks.emplace(name, 1);
			} else {
				++it->second;
			}

			Value last;
			for (auto &v : values) { last.addDouble(v); }

			auto lastIt = _lastValue.find(name);
			if (lastIt == _lastValue.end()) {
				_lastValue.emplace(name, sp::move(last));
			} else {
				lastIt->second = sp::move(last);
			}
		});
		return field;
	};

	_real = makeRow("real", 3, ZOrder(1), this);
	_real->setStep(0.5);
	_real->setValue(Vector<double>{1.0, 2.0, 3.0}, true);

	// The one the range is about. Whole numbers, so that "typing 1000 is refused" and "dragging
	// past 999 stops at 999" are about the range rather than about rounding.
	_ranged = makeRow("ranged", 2, ZOrder(2), this);
	_ranged->setInteger(true);
	_ranged->setRange(0.0, 999.0);
	_ranged->setValue(Vector<double>{10.0, 20.0}, true);

	/* The form is on a node of its own - see the class comment. Everything below it is a field or
	   part of one; the two rows above are deliberately outside. */
	_formPanel = addChild(Rc<Node>::create(), ZOrder(3));
	_form = _formPanel->addSystem(Rc<ui::FormSystem>::create());

	// Distinct z-orders: the tab ring is document order, which is z-order, and sortAllChildren is
	// an unstable sort.
	_formField = makeRow("form-vector", 4, ZOrder(1), _formPanel);
	_formField->setInteger(true);
	_formField->setValue(Vector<double>{1.0, 2.0, 3.0, 4.0}, true);
	ui::addFormField(_formField);

	// After the row in the ring: Tab out of the last component must land here, and Shift+Tab from
	// here must enter the row at that same component.
	_neighbour = _formPanel->addChild(Rc<ui::TextInput>::create(), ZOrder(2));
	_neighbour->setName("neighbour");
	_neighbour->setCaretBlink(false);
	_neighbour->setText("abc");
	ui::addFormField(_neighbour);

	return true;
}

void VectorFieldLayout::handleContentSizeDirty() {
	TestLayout::handleContentSizeDirty();

	if (_formPanel) {
		// The panel is only a place to hang the form: it covers the layout so that the rows in it
		// can be placed in the same coordinates as the rows outside.
		_formPanel->setAnchorPoint(Vec2(0.0f, 0.0f));
		_formPanel->setPosition(Vec2(0.0f, 0.0f));
		_formPanel->setContentSize(_contentSize);
	}

	const float top = getWorkTop() - 40.0f;
	Node *rows[] = {_real, _ranged, _formField, _neighbour};
	for (size_t i = 0; i < 4; ++i) {
		if (!rows[i]) {
			continue;
		}
		rows[i]->setAnchorPoint(Vec2(0.0f, 1.0f));
		rows[i]->setPosition(Vec2(48.0f, top - float(i) * 56.0f));
	}
}

ui::VectorField *VectorFieldLayout::getTarget(const Value &args) const {
	auto name = args.getString("target");
	if (name == "ranged") {
		return _ranged;
	} else if (name == "form-vector") {
		return _formField;
	}
	return _real;
}

Value VectorFieldLayout::encodeField(ui::VectorField *field) const {
	Value ret;
	if (!field) {
		return ret;
	}

	ret.setInteger(int64_t(field->getArity()), "arity");
	ret.setBool(field->isInteger(), "integer");
	ret.setBool(field->isValid(), "valid");
	ret.setString(field->getValidationMessage(), "message");
	ret.setInteger(int64_t(field->getFocusedComponent()), "focused");

	// The ROW's :focus, which is a pseudo-class and therefore not in the class list: the widget
	// writes InteractiveComponent's counter for it, and that is the fact a sheet paints from.
	if (auto state = field->getComponent<ui::InteractiveComponent>()) {
		ret.setBool(sprt::hasFlag(state->state, ui::InteractiveState::Focus), "focusState");
	}
	ret.setBool(field->isEnabled(), "enabled");

	Value values;
	Value texts;
	Value componentValid;
	Value rects;
	for (uint32_t i = 0; i < field->getArity(); ++i) {
		auto component = field->getComponentAt(i);
		values.addDouble(component->getValue());
		texts.addString(component->getText());
		componentValid.addBool(component->isValid());
		rects.addValue(encodeRect(component));
	}

	// Read off the LABEL NODES rather than out of the widget: what the row was told to show and
	// what it shows are the two halves of setLabels, and only the second one is worth checking.
	Value labels;
	for (auto &child : field->getChildren()) {
		if (child->getType() == "component-label") {
			auto label = static_cast<basic2d::Label *>(child.get());
			labels.addString(
					label->isVisible() ? string::toUtf8<Interface>(label->getString()) : String());
		}
	}
	ret.setValue(sp::move(labels), "labels");

	ret.setString(field->getUnit(), "unit");
	// Off the node, like the labels above, and found by its OWN type: if the unit were typed
	// `component-label` the scan above would have swallowed it and `labels` would report one more
	// entry than the row has components.
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
	ret.setValue(sp::move(values), "values");
	ret.setValue(sp::move(texts), "texts");
	ret.setValue(sp::move(componentValid), "componentValid");
	ret.setValue(sp::move(rects), "rects");

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
		ret.setValue(last->second, "lastValue");
	}
	return ret;
}

Value VectorFieldLayout::encodeState() const {
	Value ret;
	ret.setValue(encodeField(_real), "real");
	ret.setValue(encodeField(_ranged), "ranged");
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

void VectorFieldLayout::registerCommands() {
	addCommand("state",
			"Report every row: values, texts, validity, focus, classes and what the form collects",
			[this](Value &&) { return encodeState(); });

	addCommand("set", "Assign the whole vector: {target, values}", [this](Value &&args) {
		auto field = getTarget(args);
		if (!field) {
			return ackValue(false);
		}
		Vector<double> values;
		for (auto &it : static_cast<const Value &>(args).getValue("values").asArray()) {
			values.emplace_back(it.getDouble());
		}
		return ackValue(field->setValue(values));
	});

	addCommand("set-component", "Assign one component: {target, index, value}",
			[this](Value &&args) {
		auto field = getTarget(args);
		if (!field) {
			return ackValue(false);
		}
		const Value &a = args;
		return ackValue(
				field->setComponentValue(uint32_t(a.getInteger("index")), a.getDouble("value")));
	});

	addCommand("set-text", "Write a component's text the way typing would: {target, index, value}",
			[this](Value &&args) {
		auto field = getTarget(args);
		const Value &a = args;
		auto component = field ? field->getComponentAt(uint32_t(a.getInteger("index"))) : nullptr;
		if (!component) {
			return ackValue(false);
		}
		component->setText(a.getString("value"));
		return ackValue(true);
	});

	addCommand("focus", "Focus a component, or blur the row: {target, index, value}",
			[this](Value &&args) {
		auto field = getTarget(args);
		if (!field) {
			return ackValue(false);
		}
		const Value &a = args;
		if (!a.isBool("value") || a.getBool("value")) {
			field->focus(uint32_t(a.getInteger("index")));
		} else {
			field->blur();
		}
		return ackValue(true);
	});

	addCommand("focus-neighbour", "Put the caret in the field beside the rows", [this](Value &&) {
		if (!_neighbour) {
			return ackValue(false);
		}
		_neighbour->focus();
		return ackValue(true);
	});

	addCommand("set-arity", "Change how many components a row has: {target, value}",
			[this](Value &&args) {
		auto field = getTarget(args);
		if (!field) {
			return ackValue(false);
		}
		return ackValue(
				field->setArity(uint32_t(static_cast<const Value &>(args).getInteger("value"))));
	});

	addCommand("set-range", "Declare or drop the shared range: {target, min, max}",
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

	addCommand("set-labels", "Replace the component labels: {target, values} - empty removes them",
			[this](Value &&args) {
		auto field = getTarget(args);
		if (!field) {
			return ackValue(false);
		}
		Vector<StringView> labels;
		for (auto &it : static_cast<const Value &>(args).getValue("values").asArray()) {
			labels.emplace_back(it.getString());
		}
		field->setLabels(labels);
		return ackValue(true);
	});

	addCommand("assign", "Assign the form's value: {value}", [this](Value &&args) {
		if (!_form) {
			return ackValue(false);
		}
		_form->assign(static_cast<const Value &>(args).getValue("value"));
		return ackValue(true);
	});

	addCommand("set-unit", "Set or clear the row's unit: {target, value}", [this](Value &&args) {
		auto field = getTarget(args);
		if (!field) {
			return ackValue(false);
		}
		field->setUnit(static_cast<const Value &>(args).getString("value"));
		return ackValue(true);
	});

	addCommand("reset-counters", "Zero every row's callback count", [this](Value &&) {
		_callbacks.clear();
		_lastValue.clear();
		return ackValue(true);
	});
}

} // namespace stappler::xenolith::app
