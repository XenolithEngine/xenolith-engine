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
#include "XLInteractiveComponent.h"
#include "XLNode.h"
#include "XLUiControlLock.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

bool FormInputListener::init(StringView name, FormFieldRole role) {
	// Priority 0 is required: the tab ring is the focus group's listener vector reversed, which
	// is in document order only for listeners of equal priority
	if (!InputListener::init(0)) {
		return false;
	}

	_name = name.str<Interface>();
	_role = role;

	// Dispatched after the widget's own listener, so this only sees what the widget declined
	setSystemPriority(SystemPriority);

	/* Navigation and submission are FocusedOnly hotkeys: no key mask or touch filter needed. */
	auto &hk = EngineHotkeys::get();
	auto bind = [this](HotkeyId id) {
		addHotkey(id, [this](HotkeyId id, const InputEvent &ev) {
			return handleFormHotkey(id, ev);
		}, HotkeyFlags::FocusedOnly | HotkeyFlags::Repeatable);
	};

	bind(hk.focusNext);
	bind(hk.focusPrev);
	bind(hk.formSubmit);
	bind(hk.formSubmitKeypad);
	bind(hk.formActivate);
	bind(hk.formReset);

	return true;
}

void FormInputListener::handleEnter(Scene *scene) {
	InputListener::handleEnter(scene);

	if (_name.empty() && _owner) {
		_name = _owner->getName().str<Interface>();
	}

	updateRequiredState();

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

void FormInputListener::setFieldFlags(FormFieldFlags flags) {
	_fieldFlags = flags;
	updateRequiredState();
}

// `:required` / `:optional`. Called on flag changes and on enter, since flags may be set before
// the owner exists.
void FormInputListener::updateRequiredState() {
	if (_owner) {
		applyControlRequired(_owner, hasFlag(_fieldFlags, FormFieldFlags::Required));
	}
}

void FormInputListener::setValidator(Validator &&v) { _validator = sp::move(v); }

void FormInputListener::setSlots(FormFieldSlots &&slots) { _slots = sp::move(slots); }

bool FormInputListener::isFocusable() const {
	// A locked control is not a stop in the tab ring
	return _slots.focusable && !isEditLocked(getOwner());
}

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
	if (!_owner) {
		return;
	}
	applyControlInvalid(_owner, value);
}

bool FormInputListener::activate() {
	if (_slots.activate) {
		return _slots.activate();
	}
	return false;
}

bool FormInputListener::requestNavigate(bool backwards) {
	// Step relative to this field, not the group's committed focus; they differ when a field
	// navigates without holding focus
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

void FormInputListener::applyFocus(bool value, FocusGroup *group, bool backwards) {
	_focusBackwards = backwards;
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

/* `:focus-visible` is a plain flag, written for every widget including ownsFocusStyle ones. IME
driven widgets (ownsFocusStyle) are always focus-visible, since they show a caret however focus
arrived. */
void FormInputListener::updateFocusVisibleStyle(bool value) {
	if (!_owner) {
		return;
	}

	const bool visible = value && (_slots.ownsFocusStyle || (_form && _form->isFocusVisible()));
	applyControlFocusVisible(_owner, visible);
}

void FormInputListener::handleFocusIn(FocusGroup *group) {
	InputListener::handleFocusIn(group);

	if (!_slots.ownsFocusStyle) {
		updateFocusStyle(true);
	}

	updateFocusVisibleStyle(true);

	if (_slots.setFocused) {
		_slots.setFocused(true, _focusBackwards);
	}
}

void FormInputListener::handleFocusOut(FocusGroup *group) {
	InputListener::handleFocusOut(group);

	updateFocusVisibleStyle(false);

	if (!_slots.ownsFocusStyle) {
		updateFocusStyle(false);
	}

	if (_slots.setFocused) {
		_slots.setFocused(false, _focusBackwards);
	}
}

bool FormInputListener::handleFormHotkey(HotkeyId id, const InputEvent &) {
	// FocusedOnly already restricted delivery to the focused field's subtree, but this listener
	// may sit on a field that is disabled or has lost focus within the same frame
	if (!isFocused()) {
		return false;
	}

	auto &hk = EngineHotkeys::get();

	if (id == hk.focusNext) {
		return requestNavigate(false);
	} else if (id == hk.focusPrev) {
		return requestNavigate(true);
	} else if (id == hk.formSubmit || id == hk.formSubmitKeypad) {
		switch (_role) {
		case FormFieldRole::Submit: return requestSubmit();
		case FormFieldRole::Reset: return requestReset();
		case FormFieldRole::Field:
			// The widget acts on Enter if it can; otherwise fire the default button, or submit()
			// when the form has none
			if (activate()) {
				return true;
			}
			return _form ? _form->activateDefault() : requestSubmit();
		}
	} else if (id == hk.formActivate) {
		// Text fields never get here: the IME consumes Space as a character
		return activate();
	} else if (id == hk.formReset) {
		if (_form && _form->isResetOnEscape()) {
			return requestReset();
		}
	}
	return false;
}

} // namespace stappler::xenolith::ui
