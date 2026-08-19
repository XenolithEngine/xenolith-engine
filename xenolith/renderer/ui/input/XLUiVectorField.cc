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
#include "XLUiInteractiveComponent.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

// The fallback placement's breathing room, in points. Only the fallback: a styled row gets its
// LayoutSystem from `display:flex` and none of this runs.
static constexpr float s_vectorPadding = 4.0f;
static constexpr float s_vectorGap = 8.0f;
static constexpr float s_vectorLabelGap = 4.0f;

// x, y, z, w, and then the bare index. Four names because four is where the convention ends: a
// six-component row has no letters everyone agrees on, so its parts are numbered instead of being
// given invented ones.
static constexpr StringView s_vectorDefaultLabels[] = {
	StringView("x"),
	StringView("y"),
	StringView("z"),
	StringView("w"),
};

/* One component, and the two things only its owner needs to hear.

A ui::NumberField says when a value was ACCEPTED (setValueCallback) and nothing else: whether it is
currently holding text it refused, and whether it has the keyboard, are answered by asking, not by
telling - which is right for a field somebody put on a panel and wrong for one inside a row that
has to summarize its parts. Both hooks are already virtual, so this is an override rather than an
addition to the public widget. */
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

	/* The focus EDGE, which is not the same thing as the focus request.
	   TextInput::focus() only asks the platform; `_focused` follows what the platform granted, and
	   this is the one method called on that flip (XLUiTextInput.cc, handleTextInput). Hover and
	   active come through here too, hence the guard. */
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

	setType("vector-field");
	removeStyleClass("xl-ui-panel");
	addStyleClass("xl-ui-vector-field");
	registerStyleAppliers("vector-field");

	rebuildComponents(sprt::max(arity, uint32_t(1)));

	// See handleRowNavigate: the row answers a Tab that arrived while no component held the
	// keyboard. Hotkeys are delivered out of band, so this listener needs no key mask and no touch
	// filter - the same reason ui::FormInputListener's needs none.
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

	// A LayoutSystem - from `display:flex` or added by hand - owns the children's geometry, and
	// the placement below would be a second writer of the same positions. Same rule as
	// ui::Select's.
	if (getSystemByType<LayoutSystem>()) {
		return;
	}

	const float height = _contentSize.height;
	const float width = _contentSize.width;
	if (height <= 0.0f || width <= 0.0f || _components.empty()) {
		return;
	}

	const auto count = uint32_t(_components.size());
	const float inner = width - s_vectorPadding * 2.0f - s_vectorGap * float(count - 1);
	const float slot = sprt::max(inner / float(count), 0.0f);

	float left = s_vectorPadding;
	for (uint32_t i = 0; i < count; ++i) {
		float fieldLeft = left;
		if (i < _labels.size() && _labels[i] && _labels[i]->isVisible()) {
			// Measured NOW: a label shapes itself on its own update, which is after this, so its
			// width would be zero here and the component would be placed over the top of it.
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

	// The difference between "not told" and "told none": an empty list is an instruction to show
	// no labels at all, not a request for the defaults.
	_labelsExplicit = true;
	updateLabels();
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
	if (_enabled == value) {
		return;
	}
	_enabled = value;
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
		// silent on the component: the row reports the whole vector once below, and a per-component
		// callback would report the same assignment as several changes
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
	if (component >= _components.size() || !_enabled) {
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
		// A tap already put the caret somewhere in the row and the form is only catching up with
		// it. Moving the caret now would take it away from what was clicked.
		return;
	}
	if (_components.empty()) {
		return;
	}
	focus(backwards ? uint32_t(_components.size()) - 1 : 0);
}

void VectorField::rebuildComponents(uint32_t arity) {
	// Values first: they are what has to survive the rebuild, and reading them out of the nodes
	// after those nodes are gone is not possible.
	_values.resize(arity, 0.0);

	for (auto &it : _components) { it->removeFromParent(); }
	for (auto &it : _labels) { it->removeFromParent(); }
	_components.clear();
	_labels.clear();

	// Nothing holds the keyboard: the node that did has just left the scene, and its focus-out
	// reaches a widget that no longer knows the index.
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
		field->setEnabled(_enabled);
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
		bool dirty = state->updateState(_enabled ? (state->state | InteractiveState::Enabled)
												 : (state->state & ~InteractiveState::Enabled));
		// The counter is cumulative, so it is pushed on an edge and never twice. The components
		// paint their own `:focus`; this one is the ROW's, and it is on whenever any part of it
		// holds the keyboard.
		const bool focus = _focused >= 0 && _enabled;
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
			// The component is NAMED, because "past the maximum 999" about a row of four numbers
			// does not say which one to fix.
			message = mem_std::toString(getDefaultLabel(i), ": ",
					_components[i]->getValidationMessage());
			break;
		}
	}

	_message = sp::move(message);

	const bool invalid = !_message.empty();
	if (invalid != _invalidApplied) {
		_invalidApplied = invalid;
		if (invalid) {
			// Deliberately the same class ui::FormSystem marks a rejected field with: one look for
			// one meaning. A row that stops refusing therefore also clears a stale mark left by a
			// failed submit, which is what an author fixing the field expects to see.
			addStyleClass("invalid");
		} else {
			removeStyleClass("invalid");
		}
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
			// The step that was asked for has landed. A LATER one has not, and must go on being
			// the anchor - which is why this is not simply cleared on any focus change.
			_pending = -1;
		}
	} else if (_focused == int32_t(index)) {
		// Only when it is still ours: moving from one component to the next raises the new one's
		// focus before the old one's echo arrives, and clearing on that echo would lose it.
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
	/* Where the step is measured FROM, in order of how up to date each answer is.

	`index` - the component whose listener got the key - is the least trustworthy of the three:
	focus can leave it before the whole key batch has been dispatched. `_focused` is right whenever
	a component actually holds the keyboard. `_pending` is the only right answer in the window
	between a step being asked for and the platform granting it, which is where handleRowNavigate
	calls in from. */
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

	// Off the end of the row: this is navigation between FIELDS, and it is not this widget's to
	// answer. Inside a form the adapter hands it to the form; standalone, the row gives focus up,
	// which is what a lone ui::TextInput does with a Tab.
	if (_navigateCallback) {
		return _navigateCallback(backwards);
	}

	blur();
	return true;
}

bool VectorField::handleRowNavigate(bool backwards) {
	// Only while the row is the one being walked. Without the guard this would answer a Tab meant
	// for whatever else has the keyboard - the listener is entitled to the key, not to the intent.
	if (_focused < 0 && _pending < 0) {
		return false;
	}

	// The anchor is resolved inside, and the index is only the last resort of three.
	return handleComponentNavigate(uint32_t(sprt::max(_focused, 0)), backwards);
}

String VectorField::getDefaultLabel(uint32_t index) const {
	if (index < sizeof(s_vectorDefaultLabels) / sizeof(s_vectorDefaultLabels[0])) {
		return s_vectorDefaultLabels[index].str<Interface>();
	}
	return mem_std::toString(index);
}

} // namespace stappler::xenolith::ui
