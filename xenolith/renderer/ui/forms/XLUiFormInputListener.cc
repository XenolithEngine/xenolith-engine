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

#include "XLUiFormInputListener.h"
#include "XLUiFormSystem.h"
#include "XLUiInteractiveComponent.h"
#include "XLNode.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

bool FormInputListener::init(StringView name, FormFieldRole role) {
	// Priority 0 is load-bearing, not a default: the tab ring is the focus group's listener vector
	// reversed, and that vector is only in document order for listeners of equal priority
	if (!InputListener::init(0)) {
		return false;
	}

	_name = name.str<Interface>();
	_role = role;

	// Dispatched after the widget's own listener, so this only sees what the widget declined
	setSystemPriority(SystemPriority);

	InputKeyMask keys;
	keys.set(toInt(InputKeyCode::TAB));
	keys.set(toInt(InputKeyCode::ENTER));
	keys.set(toInt(InputKeyCode::KP_ENTER));
	keys.set(toInt(InputKeyCode::SPACE));
	keys.set(toInt(InputKeyCode::ESCAPE));

	addKeyRecognizer([this](const GestureData &data) { return handleKey(data); },
			InputKeyInfo{sp::move(keys)});

	// A key event carries the pointer's position, so the default hit test asks whether the mouse
	// happens to be over the widget - which for Tab and Enter is the wrong question entirely.
	// Focus is what decides who gets keys here; the pointer decides nothing.
	setTouchFilter([this](const InputEvent &event, const DefaultEventFilter &cb) {
		if (event.data.isKeyEvent()) {
			return isFocused();
		}
		return cb(event);
	});

	return true;
}

void FormInputListener::handleEnter(Scene *scene) {
	InputListener::handleEnter(scene);

	if (_name.empty() && _owner) {
		_name = _owner->getName().str<Interface>();
	}

	_form = FormSystem::findForNode(_owner);
	if (_form) {
		_form->addField(this);
	} else {
		slog().warn("ui::FormInputListener", "No FormSystem above the field '", _name,
				"': it will not be collected or reachable by Tab");
	}
}

void FormInputListener::handleExit() {
	if (_form) {
		_form->removeField(this);
		_form = nullptr;
	}

	// The field is leaving; whatever :focus it painted has to go with it
	if (_focusStyleApplied) {
		updateFocusStyle(false);
	}

	InputListener::handleExit();
}

void FormInputListener::setFieldName(StringView name) { _name = name.str<Interface>(); }

StringView FormInputListener::getFieldName() const { return _name; }

void FormInputListener::setRole(FormFieldRole role) { _role = role; }

void FormInputListener::setFieldFlags(FormFieldFlags flags) { _fieldFlags = flags; }

void FormInputListener::setValidator(Validator &&v) { _validator = sp::move(v); }

void FormInputListener::setSlots(FormFieldSlots &&slots) { _slots = sp::move(slots); }

bool FormInputListener::isFocusable() const { return _slots.focusable; }

Value FormInputListener::collect() const {
	if (!_slots.collect) {
		return Value();
	}
	return _slots.collect();
}

void FormInputListener::assign(const Value &value) {
	if (_slots.assign) {
		_slots.assign(value);
	}
}

void FormInputListener::clear() {
	if (_slots.clear) {
		_slots.clear();
	}
	setInvalid(false);
}

bool FormInputListener::validate(String &message) const {
	if (hasFlag(_fieldFlags, FormFieldFlags::Transient) || _role != FormFieldRole::Field) {
		return true;
	}

	auto value = collect();

	if (hasFlag(_fieldFlags, FormFieldFlags::Required)) {
		const bool empty = value.isNull() || (value.isString() && value.getString().empty())
				|| (value.isArray() && value.size() == 0)
				|| (value.isDictionary() && value.size() == 0);
		if (empty) {
			message = String("required");
			return false;
		}
	}

	if (_validator) {
		return _validator(value, message);
	}

	return true;
}

void FormInputListener::setInvalid(bool value) {
	if (_invalid == value) {
		return;
	}
	_invalid = value;

	if (!_owner || !_form) {
		return;
	}

	auto cl = _form->getInvalidStyleClass();
	if (cl.empty()) {
		return;
	}

	if (_invalid) {
		_owner->addStyleClass(cl);
	} else {
		_owner->removeStyleClass(cl);
	}
}

bool FormInputListener::activate() {
	if (_slots.activate) {
		return _slots.activate();
	}
	return false;
}

bool FormInputListener::requestNavigate(bool backwards) {
	// `this` is the anchor: the step is meant to be relative to the field that asked, not to
	// whatever the group last committed. It matters when a field navigates without holding focus;
	// in the key path the two agree
	return _form ? _form->focusNext(backwards, this) : false;
}

bool FormInputListener::requestSubmit() { return _form ? _form->submit() : false; }

bool FormInputListener::requestReset() {
	if (!_form) {
		return false;
	}
	_form->reset();
	return true;
}

void FormInputListener::applyFocus(bool value, FocusGroup *group) {
	if (value) {
		handleFocusIn(group);
	} else {
		handleFocusOut(group);
	}
}

void FormInputListener::updateFocusStyle(bool value) {
	if (!_owner || _focusStyleApplied == value) {
		return;
	}
	_focusStyleApplied = value;

	// The counter is cumulative, so it may only ever be moved on an edge - hence the guard above
	_owner->setOrUpdateComponent<InteractiveComponent>(
			[value](NotNull<InteractiveComponent> state) {
		return state->handleFocus(value ? 1 : -1);
	});
}

void FormInputListener::handleFocusIn(FocusGroup *group) {
	InputListener::handleFocusIn(group);

	if (!_slots.ownsFocusStyle) {
		updateFocusStyle(true);
	}

	if (_slots.setFocused) {
		_slots.setFocused(true);
	}
}

void FormInputListener::handleFocusOut(FocusGroup *group) {
	InputListener::handleFocusOut(group);

	if (!_slots.ownsFocusStyle) {
		updateFocusStyle(false);
	}

	if (_slots.setFocused) {
		_slots.setFocused(false);
	}
}

bool FormInputListener::handleKey(const GestureData &data) {
	if (!isFocused() || !data.input) {
		return false;
	}

	const auto &ev = data.input->data;
	if (ev.event != InputEventName::KeyPressed && ev.event != InputEventName::KeyRepeated) {
		return false;
	}

	switch (ev.key.keycode) {
	case InputKeyCode::TAB:
		return requestNavigate(hasFlag(ev.input.modifiers, InputModifier::Shift));
	case InputKeyCode::ENTER:
	case InputKeyCode::KP_ENTER:
		switch (_role) {
		case FormFieldRole::Submit: return requestSubmit();
		case FormFieldRole::Reset: return requestReset();
		case FormFieldRole::Field:
			// A widget that can act on Enter does; everything else means "I am done, submit"
			if (activate()) {
				return true;
			}
			return requestSubmit();
		}
		break;
	case InputKeyCode::SPACE:
		// Only for the widgets that have something to toggle - a text field never gets here,
		// because Space is a character and the IME claimed it long before
		return activate();
	case InputKeyCode::ESCAPE:
		if (_form && _form->isResetOnEscape()) {
			return requestReset();
		}
		break;
	default: break;
	}
	return false;
}

} // namespace stappler::xenolith::ui
