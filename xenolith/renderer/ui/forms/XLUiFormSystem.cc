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

	// SingleFocus enables filtering; canHandleEventWithListener makes it one focused field
	setFlags(Flags::SingleFocus);

	// Keyboard only, so touch is never filtered and every widget stays clickable. Never Exclusive
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

	// Nested: each segment but the last is a dictionary; emplace() creates missing levels
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

	// Recompute every mark, so fixed fields lose theirs
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
				// Show the outline on the rejected field
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

	// Anchor on the pending request first, so several Tabs within one frame compose; then on
	// `from` (a field navigating without focus); then on the committed focus.
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

	// Set after focusField(), which clears both
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

	// Move the `:default` bit with the slot
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
		// Use the button's own action; without an activate slot fall back to submit()
		if (_defaultButton->activate()) {
			return true;
		}
	}
	return submit();
}

bool FormSystem::focusField(NotNull<FormInputListener> field) {
	// A direct request is neither navigation nor keyboard-visible; focusNext() sets both after this
	_navigateBackwards = false;
	_focusVisible = false;

	// Deferred: setFocus records the request, applied on the next commit
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

	// Walk backwards for document order (see the class comment)
	_tabRing.clear();
	for (size_t i = listeners.size(); i > 0; --i) {
		if (auto field = dynamic_cast<FormInputListener *>(listeners[i - 1])) {
			if (field->isEnabled() && field->isFocusable()) {
				_tabRing.emplace_back(field);
			}
		}
	}

	// Recomputed with the ring, including when it is empty
	updateDefaultButton();

	if (_tabRing.empty()) {
		// No reachable fields: explicitly unfocus the previous field
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
		// The focused field left the ring: move focus to the requested field or the first one.
		// A never-focused form (`_focusedListener == 0`) must not take focus, or it raises the OS
		// keyboard.
		target = next ? next : _tabRing.front().get();
	}

	const bool requestedCurrent = next && next == current;

	_nextListener = 0;

	if (!target) {
		_focusedField = current;
		_focusedListener = current ? current->getId() : 0;

		// A request for the already focused field still updates `:focus-visible` (a tap hides
		// the outline)
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

	// Focus in first, then out: TextInputManager::run() for the new field deactivates the old
	// handler, so its blur() posts no releaseTextInput whose echo would cancel the new handler.
	const bool backwards = _navigateBackwards;

	// Consumed by this commit
	_navigateBackwards = false;

	// Before the events, new chain first, so shared ancestors keep the focus-within marker
	updateFocusWithinChain(previousFocused ? previousFocused->getOwner() : nullptr,
			target->getOwner());

	target->applyFocus(true, this, backwards);

	if (previousFocused) {
		previousFocused->applyFocus(false, this, backwards);
	}
}

} // namespace stappler::xenolith::ui
