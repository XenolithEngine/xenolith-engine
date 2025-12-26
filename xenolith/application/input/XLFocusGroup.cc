/**
 Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#include "XLFocusGroup.h"
#include "XLInputListener.h"
#include "XLNode.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

uint64_t FocusGroup::Id = System::GetNextSystemId();

bool FocusGroup::init() {
	if (!System::init()) {
		return false;
	}

	_frameTag = FocusGroup::Id;
	_systemFlags |= SystemFlags::AddToFrameStack;

	return true;
}

bool FocusGroup::canHandleEvent(const InputEvent &ev) {
	return _eventMask.test(toInt(ev.data.event));
}

bool FocusGroup::canHandleEventWithListener(const InputEvent &data,
		NotNull<InputListener> listener) {
	if (hasFlag(_flags, Flags::SingleFocus)) {
		return listener->getId() == _focusedListener;
	} else {
		return true;
	}
}

uint32_t FocusGroup::getPriority() const { return _priority; }

bool FocusGroup::isParentGroup(FocusGroup *group) const {
	if (!getOwner()) {
		return false;
	}
	auto owner = getOwner()->getParent();
	while (owner) {
		auto g = owner->getSystemByType<FocusGroup>();
		if (g && g == group) {
			return true;
		}
		owner = owner->getParent();
	}
	return false;
}

void FocusGroup::setEventMask(EventMask &&mask) { _eventMask = sp::move(mask); }

void FocusGroup::setFlags(Flags flags) { _flags = flags; }

bool FocusGroup::setFocus(InputListener *listener) {
	_nextListener = listener->getId();
	return true;
}

void FocusGroup::updateWithListeners(SpanView<InputListener *> listeners) {
	if (listeners.empty()) {
		_focusedListener = 0;
		_nextListener = 0;
		return;
	}

	auto currentIt = std::find_if(listeners.begin(), listeners.end(),
			[&](InputListener *l) { return l->getId() == _focusedListener; });

	if (currentIt == listeners.end()) {
		// we lost focused listener

		auto nextIt = std::find_if(listeners.begin(), listeners.end(),
				[&](InputListener *l) { return l->getId() == _nextListener; });

		if (nextIt != listeners.end()) {
			_focusedListener = (*nextIt)->getId();
			(*nextIt)->handleFocusIn(this);
		} else {
			_focusedListener = listeners.front()->getId();
			listeners.front()->handleFocusIn(this);
		}
		_nextListener = 0;
	} else if (_nextListener && _focusedListener != _nextListener) {
		auto nextIt = std::find_if(listeners.begin(), listeners.end(),
				[&](InputListener *l) { return l->getId() == _nextListener; });

		if (nextIt != listeners.end()) {
			_focusedListener = (*nextIt)->getId();
			(*currentIt)->handleFocusOut(this);
			(*nextIt)->handleFocusIn(this);
		}

		_nextListener = 0;
	}
}

} // namespace stappler::xenolith
