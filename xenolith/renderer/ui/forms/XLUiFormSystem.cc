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

#include "XLUiFormSystem.h"
#include "XLFocusWithin.h"
#include "XLNode.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

bool FormSystem::init() {
	// Sets _frameTag = FocusGroup::Id and AddToFrameStack, which is how a descendant's
	// InputListener finds this group during its visit
	if (!FocusGroup::init()) {
		return false;
	}

	// SingleFocus is what makes the group filter at all; the filtering itself is overridden below,
	// so that "single" means one focused FIELD rather than one focused listener.
	setFlags(Flags::SingleFocus);

	// Keyboard only. The dispatcher consults canHandleEventWithListener only for events the group
	// claims, so leaving touch out of the mask is what keeps every widget clickable regardless of
	// which field currently holds focus. Never Exclusive - see the class comment.
	setEventMask(EventMask(EventMaskKeyboard));

	// OR, not assign: FocusGroup::init already put AddToFrameStack in there
	setSystemFlags(
			getSystemFlags() | SystemFlags::HandleOwnerEvents | SystemFlags::HandleSceneEvents);

	return true;
}

void FormSystem::handleAdded(Node *owner) { System::handleAdded(owner); }

void FormSystem::handleRemoved() {
	_fields.clear();
	_tabRing.clear();
	_focusedField = nullptr;
	System::handleRemoved();
}

FormSystem *FormSystem::findForNode(Node *node) {
	while (node) {
		if (auto form = node->getSystemByType<FormSystem>()) {
			return form;
		}
		node = node->getParent();
	}
	return nullptr;
}

void FormSystem::setValueMode(FormValueMode mode) { _valueMode = mode; }

void FormSystem::setSubmitCallback(SubmitCallback &&cb) { _submitCallback = sp::move(cb); }

void FormSystem::setResetCallback(ResetCallback &&cb) { _resetCallback = sp::move(cb); }

void FormSystem::setInvalidCallback(InvalidCallback &&cb) { _invalidCallback = sp::move(cb); }

void FormSystem::addField(NotNull<FormInputListener> field) {
	for (auto &it : _fields) {
		if (it == field) {
			return;
		}
	}
	_fields.emplace_back(field);
}

void FormSystem::removeField(NotNull<FormInputListener> field) {
	for (auto it = _fields.begin(); it != _fields.end(); ++it) {
		if (*it == field) {
			_fields.erase(it);
			return;
		}
	}
}

FormInputListener *FormSystem::getField(StringView name) const {
	for (auto &it : _fields) {
		if (it->getFieldName() == name) {
			return it;
		}
	}
	return nullptr;
}

void FormSystem::writeValue(Value &target, StringView name, Value &&value, FormValueMode mode) {
	if (mode == FormValueMode::Flat) {
		target.setValue(sp::move(value), name);
		return;
	}

	// Nested: every segment but the last names a dictionary to descend into. emplace() promotes an
	// EMPTY value to a dictionary in place, so an intermediate level is created on first use.
	Value *current = &target;
	StringView r(name);
	while (!r.empty()) {
		auto segment = r.readUntil<StringView::Chars<'.'>>();
		if (r.is('.')) {
			++r;
		}
		if (segment.empty()) {
			continue;
		}
		if (r.empty()) {
			current->setValue(sp::move(value), segment);
			return;
		}
		current = &current->emplace(segment);
	}
}

const Value &FormSystem::readValue(const Value &source, StringView name, FormValueMode mode) {
	if (mode == FormValueMode::Flat) {
		return source.getValue(name);
	}

	const Value *current = &source;
	StringView r(name);
	while (!r.empty()) {
		auto segment = r.readUntil<StringView::Chars<'.'>>();
		if (r.is('.')) {
			++r;
		}
		if (segment.empty()) {
			continue;
		}
		current = &current->getValue(segment);
		if (current->isNull()) {
			return Value::Null;
		}
	}
	return *current;
}

Value FormSystem::collect() const {
	Value ret;
	for (auto &it : _fields) {
		if (it->getRole() != FormFieldRole::Field
				|| hasFlag(it->getFieldFlags(), FormFieldFlags::Transient)) {
			continue;
		}
		if (!it->getSlots().collect || it->getFieldName().empty()) {
			continue;
		}
		writeValue(ret, it->getFieldName(), it->collect(), _valueMode);
	}
	return ret;
}

void FormSystem::assign(const Value &value) {
	for (auto &it : _fields) {
		if (it->getRole() != FormFieldRole::Field || it->getFieldName().empty()) {
			continue;
		}
		auto &v = readValue(value, it->getFieldName(), _valueMode);
		if (!v.isNull()) {
			it->assign(v);
		}
	}
}

void FormSystem::reset() {
	for (auto &it : _fields) {
		if (it->getRole() == FormFieldRole::Field) {
			it->clear();
		}
	}
	if (_resetCallback) {
		_resetCallback();
	}
}

bool FormSystem::validate(Vector<FormValidationError> &errors) const {
	for (auto &it : _fields) {
		String message;
		if (!it->validate(message)) {
			errors.emplace_back(
					FormValidationError{it->getFieldName().str<Interface>(), sp::move(message)});
		}
	}
	return errors.empty();
}

bool FormSystem::submit() {
	Vector<FormValidationError> errors;
	validate(errors);

	// The marks are recomputed wholesale, so a field that was fixed since the last attempt loses
	// its outline even when some other field is still wrong
	for (auto &it : _fields) {
		bool failed = false;
		for (auto &err : errors) {
			if (err.name == it->getFieldName()) {
				failed = true;
				break;
			}
		}
		it->setInvalid(failed);
	}

	if (!errors.empty()) {
		for (auto &it : _fields) {
			if (it->getFieldName() == errors.front().name) {
				focusField(it);
				// Nobody tapped this field - it was chosen for the author, and the outline is part
				// of saying so.
				_focusVisible = true;
				break;
			}
		}
		if (_invalidCallback) {
			_invalidCallback(errors);
		}
		return false;
	}

	if (_submitCallback) {
		_submitCallback(collect());
	}
	return true;
}

size_t FormSystem::indexOfField(const FormInputListener *field) const {
	if (!field) {
		return maxOf<size_t>();
	}
	for (size_t i = 0; i < _tabRing.size(); ++i) {
		if (_tabRing[i].get() == field) {
			return i;
		}
	}
	return maxOf<size_t>();
}

size_t FormSystem::getFocusedIndex() const { return indexOfField(_focusedField.get()); }

FormInputListener *FormSystem::getPendingField() const {
	if (!_nextListener) {
		return nullptr;
	}
	for (auto &it : _tabRing) {
		if (it->getId() == _nextListener) {
			return it.get();
		}
	}
	return nullptr;
}

size_t FormSystem::getPendingIndex() const { return indexOfField(getPendingField()); }

bool FormSystem::focusNext(bool backwards, FormInputListener *from) {

	if (_tabRing.empty()) {
		return false;
	}

	// A PENDING request wins over everything, and that is the whole point of this ordering.
	//
	// focusField only records the request; the swap happens on the next commit. So two Tabs
	// arriving in the same frame - one key batch, one dispatch cycle - would both read the same
	// committed focus and compute the same target, and the second step would be silently lost.
	// Stepping from what has already been asked for makes them compose.
	//
	// `from` is next: it anchors the step to the field that actually asked, which is what a
	// programmatic requestNavigate() on a field that does not hold focus should mean. In the key
	// path it agrees with the committed focus anyway, because canHandleEventWithListener only
	// delivers to the focused field's subtree.
	size_t index = getPendingIndex();
	if (index == maxOf<size_t>() && from) {
		index = indexOfField(from);
	}
	if (index == maxOf<size_t>()) {
		index = getFocusedIndex();
	}

	size_t target = 0;
	if (index == maxOf<size_t>()) {
		// Nothing focused yet: Tab enters at the top, Shift+Tab at the bottom
		target = backwards ? _tabRing.size() - 1 : 0;
	} else if (backwards) {
		target = (index + _tabRing.size() - 1) % _tabRing.size();
	} else {
		target = (index + 1) % _tabRing.size();
	}

	if (_tabRing.size() == 1 && target == index) {
		return true;
	}

	if (!focusField(_tabRing[target].get())) {
		return false;
	}

	// After the call, not before: focusField() clears the direction AND the visibility, because a
	// request that is not navigation must not inherit the last Tab's
	_navigateBackwards = backwards;
	_focusVisible = true;
	return true;
}

void FormSystem::updateDefaultButton() {
	FormInputListener *found = nullptr;
	for (auto &it : _tabRing) {
		if (it->getRole() == FormFieldRole::Submit) {
			found = it.get();
			break;
		}
	}

	if (found == _defaultButton.get()) {
		return;
	}

	// The bit follows the slot, so a form whose submit button was hidden or locked stops painting
	// one - rather than leaving a highlight on a button Enter can no longer reach.
	if (_defaultButton) {
		if (auto node = _defaultButton->getOwner()) {
			applyControlDefault(node, false);
		}
	}

	_defaultButton = found;

	if (_defaultButton) {
		if (auto node = _defaultButton->getOwner()) {
			applyControlDefault(node, true);
		}
	}
}

bool FormSystem::activateDefault() {
	if (_defaultButton) {
		// The button's own action, so that what happens is what the highlighted control says it
		// does. A button with no activate slot is not a reason to do nothing.
		if (_defaultButton->activate()) {
			return true;
		}
	}
	return submit();
}

bool FormSystem::focusField(NotNull<FormInputListener> field) {
	// A direct request is not navigation, and the field it lands on must not be told it was: a
	// composite widget would enter at its last part for a tap
	_navigateBackwards = false;

	// ...and it is not a keyboard walk either. focusNext() sets this back to true after the call.
	_focusVisible = false;

	// The swap is deferred: setFocus only records the request, and the group applies it on the
	// next commit, once it knows which listeners are actually live this frame
	return setFocus(field);
}

bool FormSystem::isWithinFocusedField(NotNull<InputListener> listener) const {
	if (!_focusedField) {
		return false;
	}

	auto fieldNode = _focusedField->getOwner();
	if (!fieldNode) {
		return false;
	}

	auto node = listener->getOwner();
	while (node) {
		if (node == fieldNode) {
			return true;
		}
		node = node->getParent();
	}
	return false;
}

bool FormSystem::canHandleEventWithListener(const InputEvent &, NotNull<InputListener> l) {
	if (!_focusedField) {
		// Nothing focused yet: do not starve the scene of keys while the ring is still empty
		return true;
	}
	return isWithinFocusedField(l);
}

void FormSystem::updateWithListeners(SpanView<InputListener *> listeners) {
	Rc<FormInputListener> previousFocused = _focusedField;

	// The incoming list is priority DESC then visit order DESC. Every FormInputListener keeps
	// priority 0, so walking it backwards yields document order - one entry per field node.
	_tabRing.clear();
	for (size_t i = listeners.size(); i > 0; --i) {
		if (auto field = dynamic_cast<FormInputListener *>(listeners[i - 1])) {
			if (field->isEnabled() && field->isFocusable()) {
				_tabRing.emplace_back(field);
			}
		}
	}

	// The default button is defined in terms of the ring, so it is recomputed with it - including
	// the case below, where the ring came out empty and the form has no default any more
	updateDefaultButton();

	if (_tabRing.empty()) {
		// The whole form went away, or every field became unreachable. The base class would leave
		// the departed listener believing it still has focus.
		if (previousFocused) {
			previousFocused->applyFocus(false, this);
			updateFocusWithinChain(previousFocused->getOwner(), nullptr);
		}
		_focusedField = nullptr;
		_focusedListener = 0;
		_nextListener = 0;
		_navigateBackwards = false;
		return;
	}

	auto findInRing = [&](uint64_t id) -> FormInputListener * {
		if (!id) {
			return nullptr;
		}
		for (auto &it : _tabRing) {
			if (it->getId() == id) {
				return it.get();
			}
		}
		return nullptr;
	};

	auto current = findInRing(_focusedListener);
	auto next = findInRing(_nextListener);

	FormInputListener *target = nullptr;
	if (next && next != current) {
		target = next;
	} else if (!current && _focusedListener != 0) {
		// A field HELD focus and left the ring - hidden, disabled or removed. Hand focus on rather
		// than drop it: fall back to the field that was asked for, or to the first one. The base
		// class picks listeners.front() here, which after the priority sort is whichever listener
		// sorts first and is typically not a field at all.
		//
		// The `_focusedListener != 0` guard is the difference between that and a form that has
		// never been touched: an untouched form must NOT take focus, or merely showing one would
		// raise the OS keyboard.
		target = next ? next : _tabRing.front().get();
	}

	const bool requestedCurrent = next && next == current;

	_nextListener = 0;

	if (!target) {
		_focusedField = current;
		_focusedListener = current ? current->getId() : 0;

		// A request that lands on the field which already HAS focus moves nothing - but it can
		// still change how that focus looks: tapping the field you just tabbed to is how a person
		// puts the outline away. Without this the outline would survive the tap, which is the
		// visual noise :focus-visible exists to remove.
		if (requestedCurrent && current) {
			current->updateFocusVisibleStyle(true);
		}
		return;
	}

	_focusedField = target;
	_focusedListener = target->getId();

	if (previousFocused.get() == target) {
		return;
	}

	// FOCUS IN FIRST, THEN OUT - the order matters, and not for style.
	//
	// Only one text-input handler can be active at a time, and TextInputManager::run() already
	// displaces the previous one, pushing it an `enabled = false` state synchronously. Blurring
	// the old field first instead would post a releaseTextInput, and the echo that comes back
	// (`enabled = false`) makes TextInputManager cancel WHATEVER handler is registered by then -
	// which is the new field's. The field would hold form focus with no keyboard behind it.
	//
	// Taking the new field first makes the old one's handler inactive before it is told anything,
	// so its blur() is a no-op and no release is ever posted.
	const bool backwards = _navigateBackwards;

	// Consumed here: the next commit describes whatever causes it, and a direction left standing
	// would make a tap look like a Shift+Tab
	_navigateBackwards = false;

	// Before the events, and in the same order they are: the new chain is retained first, so an
	// ancestor shared by both keeps the marker and is not restyled for a move that never left it
	updateFocusWithinChain(previousFocused ? previousFocused->getOwner() : nullptr,
			target->getOwner());

	target->applyFocus(true, this, backwards);

	if (previousFocused) {
		previousFocused->applyFocus(false, this, backwards);
	}
}

} // namespace stappler::xenolith::ui
