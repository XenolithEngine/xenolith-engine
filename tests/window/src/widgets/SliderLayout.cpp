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

#include "widgets/SliderLayout.h"
#include "XLUiStyleResolver.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

namespace {

/* The three parts, and nothing else. The sizes here are the ones slider-check.py duplicates: the
handle's is what makes the travel `220 - 16`, and the check computes the same number rather than
asking the widget for it.

`slider-thumb` is sized from CSS on purpose - it is the one piece of this widget's geometry the
widget does NOT own, and a stand that declared it in C++ would be proving the wrong thing. */
static constexpr auto s_sliderCss = StringView(R"css(
slider {
	width: 220px;
	height: 20px;
	background-color: #292929;
	border-radius: 4px;
}
slider.vertical {
	width: 20px;
	height: 220px;
}
slider-fill {
	background-color: #fcb400;
	border-radius: 4px;
}
slider-thumb {
	width: 16px;
	height: 16px;
	border-radius: 8px;
	background-color: #e8e8e8;
}
slider:focus > slider-thumb {
	background-color: #fcb400;
}
slider.locked > slider-thumb {
	background-color: #6a6a6a;
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

bool SliderLayout::init() {
	if (!TestLayout::init()) {
		return false;
	}

	setStyleSheet(s_sliderCss);
	addSystem(Rc<ui::StyleResolver>::create(true));

	_formSystem = addSystem(Rc<ui::FormSystem>::create());

	// Distinct z-orders: the tab ring is document order, which is z-order, and sortAllChildren is
	// an unstable sort.
	auto makeSlider = [&](StringView name, ZOrder z) {
		auto slider = addChild(Rc<ui::Slider>::create(), z);
		slider->setName(name);
		slider->setCallback([this, name = name.str<Interface>()](int64_t index) {
			// emplace/find rather than operator[]: the runtime's map returns an access token from
			// it, which traps on a key that is not there.
			auto it = _callbacks.find(name);
			if (it == _callbacks.end()) {
				_callbacks.emplace(name, 1);
			} else {
				++it->second;
			}

			auto last = _lastIndex.find(name);
			if (last == _lastIndex.end()) {
				_lastIndex.emplace(name, index);
			} else {
				last->second = index;
			}
		});
		return slider;
	};

	_steps = makeSlider("steps", ZOrder(1));
	_steps->setRange(0.0, 100.0, 5.0);
	_steps->setInteger(true);
	_steps->setIndex(0, true);

	_real = makeSlider("real", ZOrder(2));
	_real->setRange(0.0, 1.0, 0.25);
	_real->setIndex(0, true);

	// The author's maximum is 10 and the notches are 0, 3, 6, 9. Nothing here trims either number.
	_unreachable = makeSlider("unreachable", ZOrder(3));
	_unreachable->setRange(0.0, 10.0, 3.0);
	_unreachable->setInteger(true);
	_unreachable->setIndex(0, true);

	_vertical = makeSlider("vertical", ZOrder(4));
	_vertical->setVertical(true);
	_vertical->setRange(0.0, 100.0, 5.0);
	_vertical->setInteger(true);
	_vertical->setIndex(0, true);

	_form = makeSlider("form-slider", ZOrder(5));
	_form->setRange(0.0, 100.0, 5.0);
	_form->setInteger(true);
	_form->setIndex(4, true); // value 20
	ui::addFormField(_form);

	return true;
}

void SliderLayout::handleContentSizeDirty() {
	TestLayout::handleContentSizeDirty();

	const float top = getWorkTop() - 40.0f;
	Node *rows[] = {_steps, _real, _unreachable, _form};
	for (size_t i = 0; i < 4; ++i) {
		if (!rows[i]) {
			continue;
		}
		rows[i]->setAnchorPoint(Vec2(0.0f, 1.0f));
		rows[i]->setPosition(Vec2(RowLeft, top - float(i) * RowStride));
	}

	// The vertical one stands beside the column: it is 220 points TALL, and putting it in the row
	// stride would have it overlap three of its neighbours.
	if (_vertical) {
		_vertical->setAnchorPoint(Vec2(0.0f, 1.0f));
		_vertical->setPosition(Vec2(RowLeft + TrackWidth + 60.0f, top));
	}
}

ui::Slider *SliderLayout::getTarget(const Value &args) const {
	auto name = args.getString("target");
	if (name == "real") {
		return _real;
	} else if (name == "unreachable") {
		return _unreachable;
	} else if (name == "vertical") {
		return _vertical;
	} else if (name == "form-slider") {
		return _form;
	}
	return _steps;
}

Value SliderLayout::encodeSlider(ui::Slider *slider) const {
	Value ret;
	if (!slider) {
		return ret;
	}

	ret.setInteger(slider->getIndex(), "index");
	ret.setInteger(slider->getMaxIndex(), "maxIndex");
	ret.setDouble(slider->getValue(), "value");

	// What the LAST notch is worth. `maxValue` is what the author declared; when the two differ,
	// the end of the scale is not reachable - and saying so is the widget's job rather than
	// quietly moving one of them.
	ret.setDouble(slider->getValueAt(slider->getMaxIndex()), "lastValue");
	ret.setDouble(slider->getMin(), "min");
	ret.setDouble(slider->getMax(), "max");
	ret.setDouble(slider->getStep(), "step");

	ret.setBool(slider->isInteger(), "integer");
	ret.setBool(slider->isVertical(), "vertical");
	ret.setBool(slider->isEnabled(), "enabled");
	ret.setBool(slider->isFocused(), "focused");
	ret.setBool(slider->isDragging(), "dragging");
	ret.setBool(ui::isEditLocked(slider), "locked");
	ret.setString(ui::getEditLockReason(slider), "lockReason");

	// The classes the sheet paints from - checked as classes rather than as colours, because a
	// colour is a screenshot and a class is a fact.
	Value classes;
	if (auto set = slider->getStyleClasses()) {
		for (auto &it : *set) { classes.addString(it); }
	}
	ret.setValue(sp::move(classes), "classes");

	auto name = slider->getName().str<Interface>();
	auto cb = _callbacks.find(name);
	ret.setInteger(cb == _callbacks.end() ? 0 : int64_t(cb->second), "callbacks");
	auto last = _lastIndex.find(name);
	if (last != _lastIndex.end()) {
		ret.setInteger(last->second, "lastIndex");
	}
	return ret;
}

Value SliderLayout::encodeState() const {
	Value ret;
	ret.setValue(encodeSlider(_steps), "steps");
	ret.setValue(encodeSlider(_real), "real");
	ret.setValue(encodeSlider(_unreachable), "unreachable");
	ret.setValue(encodeSlider(_vertical), "vertical");
	ret.setValue(encodeSlider(_form), "form");
	if (_formSystem) {
		ret.setValue(_formSystem->collect(), "collected");

		// The tab ring by name, and who holds focus. "The slider is one stop of the ring" is a
		// claim about this list and cannot be read off any single widget.
		Value ring;
		for (auto &it : _formSystem->getTabRing()) { ring.addString(it->getFieldName()); }
		ret.setValue(sp::move(ring), "ring");
		if (auto focused = _formSystem->getFocusedField()) {
			ret.setString(focused->getFieldName(), "formFocus");
		}
	}
	return ret;
}

void SliderLayout::registerCommands() {
	addCommand("state", "Report every slider: index, value, scale, flags and callback count",
			[this](Value &&) { return encodeState(); });

	addCommand("metrics",
			"Measured geometry of one slider: track, handle, travel and where the fill ends - "
			"{target}",
			[this](Value &&args) {
		auto slider = getTarget(args);
		if (!slider) {
			return ackValue(false);
		}
		Value ret;
		ret.setDouble(slider->getContentSize().width, "trackWidth");
		ret.setDouble(slider->getContentSize().height, "trackHeight");
		if (auto thumb = slider->getThumb()) {
			ret.setDouble(thumb->getContentSize().width, "thumbWidth");
			ret.setDouble(thumb->getContentSize().height, "thumbHeight");
			ret.setDouble(thumb->getPosition().x, "thumbX");
			ret.setDouble(thumb->getPosition().y, "thumbY");
		}
		if (auto fill = slider->getFill()) {
			ret.setDouble(fill->getContentSize().width, "fillWidth");
			ret.setDouble(fill->getContentSize().height, "fillHeight");
		}
		ret.setDouble(slider->getTravel(), "travel");
		return ret;
	});

	addCommand("set-index", "Move to a notch programmatically: {target, value, silent}",
			[this](Value &&args) {
		auto slider = getTarget(args);
		if (!slider) {
			return ackValue(false);
		}
		const Value &a = args;
		slider->setIndex(a.getInteger("value"), a.getBool("silent"));
		return ackValue(true);
	});

	addCommand("set-value", "Assign a value; the nearest notch takes it: {target, value, silent}",
			[this](Value &&args) {
		auto slider = getTarget(args);
		if (!slider) {
			return ackValue(false);
		}
		const Value &a = args;
		slider->setValue(a.getDouble("value"), a.getBool("silent"));
		return ackValue(true);
	});

	addCommand("set-range", "Redeclare the scale: {target, min, max, step}", [this](Value &&args) {
		auto slider = getTarget(args);
		if (!slider) {
			return ackValue(false);
		}
		const Value &a = args;
		return ackValue(
				slider->setRange(a.getDouble("min"), a.getDouble("max"), a.getDouble("step")));
	});

	addCommand("focus", "Focus a slider, or blur it: {target, value}", [this](Value &&args) {
		auto slider = getTarget(args);
		if (!slider) {
			return ackValue(false);
		}
		const Value &a = args;
		if (!a.isBool("value") || a.getBool("value")) {
			slider->focus();
		} else {
			slider->blur();
		}
		return ackValue(true);
	});

	addCommand("set-enabled", "Turn a slider on or off: {target, value}", [this](Value &&args) {
		auto slider = getTarget(args);
		if (!slider) {
			return ackValue(false);
		}
		slider->setEnabled(static_cast<const Value &>(args).getBool("value"));
		return ackValue(true);
	});

	addCommand("lock", "Lock a slider with a reason, or unlock it: {target, reason}",
			[this](Value &&args) {
		auto slider = getTarget(args);
		if (!slider) {
			return ackValue(false);
		}
		auto reason = static_cast<const Value &>(args).getString("reason");
		if (reason.empty()) {
			ui::clearEditLock(slider);
		} else {
			ui::setEditLock(slider, reason);
		}
		return ackValue(true);
	});

	addCommand("assign", "Push a value through the FORM rather than through the widget: {value}",
			[this](Value &&args) {
		if (!_formSystem) {
			return ackValue(false);
		}
		Value v;
		v.setValue(static_cast<const Value &>(args).getValue("value"), "form-slider");
		_formSystem->assign(v);
		return ackValue(true);
	});

	addCommand("reset", "Reset the form, which is how a field's `clear` slot is reached",
			[this](Value &&) {
		if (!_formSystem) {
			return ackValue(false);
		}
		_formSystem->reset();
		return ackValue(true);
	});

	addCommand("reset-counters", "Zero every slider's callback count", [this](Value &&) {
		_callbacks.clear();
		_lastIndex.clear();
		return ackValue(true);
	});
}

} // namespace stappler::xenolith::app
