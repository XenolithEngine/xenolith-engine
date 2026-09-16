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

#include "XLUiVectorField.h"
#include "XLUiLayoutSystem.h"
#include "XLInteractiveComponent.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

// Padding of the fallback placement, in points; unused with a LayoutSystem.
static constexpr float s_vectorPadding = 4.0f;
static constexpr float s_vectorGap = 8.0f;
static constexpr float s_vectorLabelGap = 4.0f;

// Between the last component and the row's unit.
static constexpr float s_vectorUnitGap = 6.0f;

// x, y, z, w, and then the bare index.
static constexpr StringView s_vectorDefaultLabels[] = {
	StringView("x"),
	StringView("y"),
	StringView("z"),
	StringView("w"),
};

/* One component: overrides NumberField's virtual hooks to notify the row of focus and validity
changes, which a plain NumberField only answers when asked. */
class VectorField::Component : public NumberField {
public:
	virtual ~Component() = default;

	virtual bool init(NotNull<VectorField> owner, uint32_t index) {
		if (!NumberField::init()) {
			return false;
		}
		_owner = owner;
		_index = index;
		return true;
	}

protected:
	using NumberField::init;

	virtual void setInvalid(bool value, StringView message) override {
		NumberField::setInvalid(value, message);
		if (_owner) {
			_owner->updateValidity();
		}
	}

	/* The focus edge, not the request: called when `_focused` flips to what the platform granted
	   (TextInput::handleTextInput). Hover and active also come through here, hence the guard. */
	virtual void updateInteractiveState() override {
		NumberField::updateInteractiveState();
		if (_reportedFocus != isFocused()) {
			_reportedFocus = isFocused();
			if (_owner) {
				_owner->handleComponentFocus(_index, _reportedFocus);
			}
		}
	}

	VectorField *_owner = nullptr;
	uint32_t _index = 0;
	bool _reportedFocus = false;
};

VectorField::~VectorField() { }

bool VectorField::init() { return init(DefaultArity); }

bool VectorField::init(uint32_t arity) {
	if (!Panel::init()) {
		return false;
	}

	/* The InteractiveComponent must exist from the start: without one the state reads as 0, so
	`:disabled` would match and isEnabled() would report false. */
	applyControlEnabled(this, true);

	setType("vector-field");
	removeStyleClass("xl-ui-panel");
	addStyleClass("xl-ui-vector-field");
	registerStyleAppliers("vector-field");

	rebuildComponents(sprt::max(arity, uint32_t(1)));

	// See handleRowNavigate. Hotkeys are delivered out of band, so no key mask or touch filter.
	_keyListener = addSystem(Rc<InputListener>::create());

	auto &hk = EngineHotkeys::get();
	auto bind = [this](HotkeyId id, bool backwards) {
		_keyListener->addHotkey(id, [this, backwards](HotkeyId, const InputEvent &) {
			return handleRowNavigate(backwards);
		}, HotkeyFlags::FocusedOnly | HotkeyFlags::Repeatable);
	};

	bind(hk.focusNext, false);
	bind(hk.focusPrev, true);

	updateInteractiveState();


	return true;
}

void VectorField::handleContentSizeDirty() {
	Panel::handleContentSizeDirty();

	// A LayoutSystem (from `display:flex` or added by hand) owns the children's geometry.
	if (getSystemByType<LayoutSystem>()) {
		return;
	}

	const float height = _contentSize.height;
	const float width = _contentSize.width;
	if (height <= 0.0f || width <= 0.0f || _components.empty()) {
		return;
	}

	// The row's unit is placed first and the components share the rest. Measured now, since a label
	// shapes itself on its own update, after this pass.
	float right = width - s_vectorPadding;
	if (_unitLabel && _unitLabel->isVisible()) {
		_unitLabel->tryUpdateLabel();
		_unitLabel->setAnchorPoint(Anchor::MiddleRight);
		_unitLabel->setPosition(Vec2(right, height / 2.0f));
		right -= _unitLabel->getContentSize().width + s_vectorUnitGap;
	}

	const auto count = uint32_t(_components.size());
	const float inner = (right - s_vectorPadding) - s_vectorGap * float(count - 1);
	const float slot = sprt::max(inner / float(count), 0.0f);

	float left = s_vectorPadding;
	for (uint32_t i = 0; i < count; ++i) {
		float fieldLeft = left;
		if (i < _labels.size() && _labels[i] && _labels[i]->isVisible()) {
			// Measured now: a label shapes itself on its own update, after this pass.
			_labels[i]->tryUpdateLabel();
			_labels[i]->setAnchorPoint(Anchor::MiddleLeft);
			_labels[i]->setPosition(Vec2(fieldLeft, height / 2.0f));
			fieldLeft += _labels[i]->getContentSize().width + s_vectorLabelGap;
		}

		auto field = _components[i];
		field->setAnchorPoint(Anchor::MiddleLeft);
		field->setPosition(Vec2(fieldLeft, height / 2.0f));
		field->setContentSize(Size2(sprt::max(left + slot - fieldLeft, 0.0f), height));

		left += slot + s_vectorGap;
	}
}

bool VectorField::setArity(uint32_t arity) {
	if (arity == 0) {
		return false;
	}
	if (arity == uint32_t(_components.size())) {
		return true;
	}
	rebuildComponents(arity);
	return true;
}

void VectorField::setLabels(SpanView<StringView> labels) {
	_labelStrings.clear();
	_labelStrings.reserve(labels.size());
	for (auto &it : labels) { _labelStrings.emplace_back(it.str<Interface>()); }

	// An empty list means no labels, not the defaults.
	_labelsExplicit = true;
	updateLabels();
}

void VectorField::setUnit(StringView value) {
	if (StringView(_unit) == value) {
		return;
	}
	_unit = value.str<Interface>();

	if (!_unitLabel) {
		if (_unit.empty()) {
			return;
		}
		_unitLabel = addChild(Rc<basic2d::Label>::create(), ZOrder(1));
		// The type ui::NumberField's unit uses, so one rule styles both.
		_unitLabel->setType("field-unit");
		_unitLabel->addStyleClass("xl-ui-field-unit");
		_unitLabel->setAlignment(font::TextAlign::Left);
	}

	_unitLabel->setString(_unit);
	_unitLabel->setVisible(!_unit.empty());
	_contentSizeDirty = true;
}

void VectorField::setInteger(bool value) {
	_integer = value;
	for (auto &it : _components) { it->setInteger(value); }
	updateValidity();
}

void VectorField::setRange(double min, double max) {
	_hasRange = true;
	_min = min;
	_max = max;
	for (auto &it : _components) { it->setRange(min, max); }
	updateValidity();
}

void VectorField::clearRange() {
	_hasRange = false;
	for (auto &it : _components) { it->clearRange(); }
	updateValidity();
}

void VectorField::setStep(double value) {
	if (value <= 0.0) {
		return;
	}
	_step = value;
	for (auto &it : _components) { it->setStep(value); }
}

void VectorField::setDragEnabled(bool value) {
	_dragEnabled = value;
	for (auto &it : _components) { it->setDragEnabled(value); }
}

void VectorField::setDragSensitivity(float value) {
	_dragSensitivity = value;
	for (auto &it : _components) { it->setDragSensitivity(value); }
}

void VectorField::setEnabled(bool value) {
	// The edit lock overrides the request and remembers it for unlock.
	value = resolveEditLock(this, value);
	if (isEnabled() == value) {
		return;
	}
	applyControlEnabled(this, value);
	for (auto &it : _components) { it->setEnabled(value); }
	updateInteractiveState();
}

NumberField *VectorField::getComponentAt(uint32_t index) const {
	return index < _components.size() ? _components[index] : nullptr;
}

bool VectorField::setValue(SpanView<double> values, bool silent) {
	if (values.size() != _components.size()) {
		return false;
	}

	for (uint32_t i = 0; i < uint32_t(values.size()); ++i) {
		// silent on the component: the row reports the whole vector once below
		_components[i]->setValue(values[i], true);
		_values[i] = values[i];
	}

	if (!silent && _valueCallback) {
		_valueCallback(_values);
	}
	return true;
}

bool VectorField::setComponentValue(uint32_t index, double value, bool silent) {
	if (index >= _components.size()) {
		return false;
	}

	_components[index]->setValue(value, true);
	_values[index] = value;

	if (!silent && _valueCallback) {
		_valueCallback(_values);
	}
	return true;
}

double VectorField::getComponentValue(uint32_t index) const {
	return index < _values.size() ? _values[index] : 0.0;
}

void VectorField::setValueCallback(ValueCallback &&cb) { _valueCallback = sp::move(cb); }

void VectorField::setFocusCallback(FocusCallback &&cb) { _focusCallback = sp::move(cb); }

void VectorField::setNavigateCallback(NavigateCallback &&cb) { _navigateCallback = sp::move(cb); }

void VectorField::focus(uint32_t component) {
	if (component >= _components.size() || !isEnabled()) {
		return;
	}
	_components[component]->focus();
}

void VectorField::blur() {
	_pending = -1;
	if (_focused < 0 || uint32_t(_focused) >= _components.size()) {
		return;
	}
	_components[_focused]->blur();
}

void VectorField::focusFromNavigation(bool backwards) {
	if (_focused >= 0) {
		// A tap already put the caret in the row; the form is only catching up.
		return;
	}
	if (_components.empty()) {
		return;
	}
	focus(backwards ? uint32_t(_components.size()) - 1 : 0);
}

void VectorField::rebuildComponents(uint32_t arity) {
	// Values first, before the nodes are rebuilt.
	_values.resize(arity, 0.0);

	for (auto &it : _components) { it->removeFromParent(); }
	for (auto &it : _labels) { it->removeFromParent(); }
	_components.clear();
	_labels.clear();

	// Nothing holds the keyboard: the focused node has just left the scene.
	_focused = -1;
	_pending = -1;

	for (uint32_t i = 0; i < arity; ++i) {
		auto label = addChild(Rc<basic2d::Label>::create(), ZOrder(1));
		label->setType("component-label");
		label->addStyleClass("xl-ui-vector-label");
		label->setAlignment(font::TextAlign::Left);
		_labels.emplace_back(label);

		auto field = addChild(Rc<Component>::create(this, i), ZOrder(1));
		field->setName(mem_std::toString("component-", i));
		field->setInteger(_integer);
		if (_hasRange) {
			field->setRange(_min, _max);
		}
		field->setStep(_step);
		field->setDragEnabled(_dragEnabled);
		field->setDragSensitivity(_dragSensitivity);
		field->setEnabled(isEnabled());
		field->setValue(_values[i], true);

		field->setValueCallback([this, i](double value) { handleComponentValue(i, value); });
		field->setNavigateCallback(
				[this, i](bool backwards) { return handleComponentNavigate(i, backwards); });

		_components.emplace_back(field);
	}

	updateLabels();
	updateArityClass();
	updateValidity();
	_contentSizeDirty = true;
}

void VectorField::updateLabels() {
	for (uint32_t i = 0; i < uint32_t(_labels.size()); ++i) {
		String text;
		if (_labelsExplicit) {
			if (i < _labelStrings.size()) {
				text = _labelStrings[i];
			}
		} else {
			text = getDefaultLabel(i);
		}
		_labels[i]->setString(text);
		_labels[i]->setVisible(!text.empty());
	}
	_contentSizeDirty = true;
}

void VectorField::updateArityClass() {
	if (!_arityClass.empty()) {
		removeStyleClass(_arityClass);
	}
	_arityClass = mem_std::toString("arity-", _components.size());
	addStyleClass(_arityClass);
}

void VectorField::updateInteractiveState() {
	setOrUpdateComponent<InteractiveComponent>([this](NotNull<InteractiveComponent> state) {
		// The Enabled bit and the `disabled` class are applyControlEnabled's, from setEnabled.
		bool dirty = false;
		// The counter is cumulative, so it is pushed on an edge. This is the row's `:focus`, on
		// whenever any component holds the keyboard.
		const bool focus = _focused >= 0 && sprt::hasFlag(state->state, InteractiveState::Enabled);
		if (focus != sprt::hasFlag(state->state, InteractiveState::Focus)) {
			dirty = state->handleFocus(focus ? 1 : -1) || dirty;
		}
		return dirty;
	});
}

void VectorField::updateValidity() {
	String message;
	for (uint32_t i = 0; i < uint32_t(_components.size()); ++i) {
		if (!_components[i]->isValid()) {
			// The component is named in the message.
			message = mem_std::toString(getDefaultLabel(i), ": ",
					_components[i]->getValidationMessage());
			break;
		}
	}

	_message = sp::move(message);

	const bool invalid = !_message.empty();
	{
		// The same class ui::FormSystem marks a rejected field with, so a row that becomes valid
		// also clears a mark left by a failed submit.
		applyControlInvalid(this, invalid);
	}
}

void VectorField::handleComponentValue(uint32_t index, double value) {
	if (index < _values.size()) {
		_values[index] = value;
	}
	if (_valueCallback) {
		_valueCallback(_values);
	}
}

void VectorField::handleComponentFocus(uint32_t index, bool focused) {
	if (focused) {
		_focused = int32_t(index);
		if (_pending == int32_t(index)) {
			// The requested step has landed. A later request stays pending.
			_pending = -1;
		}
	} else if (_focused == int32_t(index)) {
		// Only when it is still ours: the new component's focus may arrive before the old one's
		// focus-out echo.
		_focused = -1;
	} else {
		return;
	}

	updateInteractiveState();

	if (_focusCallback) {
		_focusCallback(_focused);
	}
}

bool VectorField::handleComponentNavigate(uint32_t index, bool backwards) {
	/* Where the step is measured from, most reliable first: `_pending` (a requested step not yet
	granted), `_focused`, then `index` (focus may have left it within the key batch). */
	int32_t from = int32_t(index);
	if (_pending >= 0) {
		from = _pending;
	} else if (_focused >= 0) {
		from = _focused;
	}
	const int32_t target = from + (backwards ? -1 : 1);
	if (target >= 0 && target < int32_t(_components.size())) {
		_pending = target;
		focus(uint32_t(target));
		return true;
	}

	_pending = -1;

	// Off the end of the row: navigation between fields. Inside a form the adapter hands it to the
	// form; standalone, the row gives focus up.
	if (_navigateCallback) {
		return _navigateCallback(backwards);
	}

	blur();
	return true;
}

bool VectorField::handleRowNavigate(bool backwards) {
	// Only while the row is the one being walked, not for a Tab meant for another widget.
	if (_focused < 0 && _pending < 0) {
		return false;
	}

	// The anchor is resolved inside; the index is the last resort.
	return handleComponentNavigate(uint32_t(sprt::max(_focused, 0)), backwards);
}

String VectorField::getDefaultLabel(uint32_t index) const {
	if (index < sizeof(s_vectorDefaultLabels) / sizeof(s_vectorDefaultLabels[0])) {
		return s_vectorDefaultLabels[index].str<Interface>();
	}
	return mem_std::toString(index);
}

} // namespace stappler::xenolith::ui
