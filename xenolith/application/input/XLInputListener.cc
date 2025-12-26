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

#include "XLInputListener.h"
#include "XLAction.h"
#include "XLDirector.h"
#include "XLInputDispatcher.h"
#include "XLGestureRecognizer.h"
#include "XLAppWindow.h"
#include "XLScene.h"
#include "XLFrameContext.h"
#include "XLFocusGroup.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

static std::atomic<uint64_t> s_inputListenerId = 1;

InputListener::EventMask EventMaskTouch = makeEventMask({
	InputEventName::Begin,
	InputEventName::Move,
	InputEventName::End,
	InputEventName::Cancel,
	InputEventName::Scroll,
});

InputListener::EventMask EventMaskKeyboard = makeEventMask({
	InputEventName::KeyPressed,
	InputEventName::KeyRepeated,
	InputEventName::KeyReleased,
	InputEventName::KeyCanceled,
});

InputButtonMask makeButtonMask(std::initializer_list<InputMouseButton> &&il) {
	InputButtonMask ret;
	for (auto &it : il) { ret.set(toInt(it)); }
	return ret;
}

InputButtonMask makeButtonMask(InputMouseButton val) {
	InputButtonMask ret;
	ret.set(toInt(val));
	return ret;
}

InputEventMask makeEventMask(std::initializer_list<InputEventName> &&il) {
	InputEventMask ret;
	for (auto &it : il) { ret.set(toInt(it)); }
	return ret;
}

InputEventMask makeEventMask(InputEventName val) {
	InputEventMask ret;
	ret.set(toInt(val));
	return ret;
}

InputKeyMask makeKeyMask(std::initializer_list<InputKeyCode> &&il) {
	InputKeyMask ret;
	for (auto &it : il) { ret.set(toInt(it)); }
	return ret;
}

InputKeyMask makeKeyMask(InputKeyCode val) {
	InputKeyMask ret;
	ret.set(toInt(val));
	return ret;
}

bool InputListener::init(int32_t priority) {
	if (!System::init()) {
		return false;
	}

	_priority = priority;
	return true;
}

void InputListener::handleEnter(Scene *scene) {
	System::handleEnter(scene);

	_id = s_inputListenerId.fetch_add(1);
	_hasFocus = false;
	_scene = scene;

	for (auto &it : _recognizers) {
		if (it->requiresUpdate()) {
			scheduleUpdate();
		}
		it->onEnter(this);
	}
}

void InputListener::handleExit() {
	for (auto &it : _recognizers) { it->onExit(); }

	if (_hasFocus) {
		handleFocusOut(nullptr);
	}
	_scene = nullptr;

	System::handleExit();
}

void InputListener::handleVisitSelf(FrameInfo &info, Node *node, NodeVisitFlags flags) {
	System::handleVisitSelf(info, node, flags);

	if (_enabled) {
		auto g = info.getSystem<FocusGroup>(FocusGroup::Id);

		if (_windowLayer) {
			WindowLayer layer{
				TransformRect(Rect(Vec2(0, 0), node->getContentSize()),
						info.modelTransformStack.back()),
				_windowLayer.cursor,
				_windowLayer.flags,
			};
			info.input->addListener(this, g, sp::move(layer));
		} else {
			info.input->addListener(this, g, WindowLayer(_windowLayer));
		}
	}
}

void InputListener::update(const UpdateTime &dt) {
	for (auto &it : _recognizers) { it->update(dt.delta); }
}

uint64_t InputListener::getId() const { return _id; }

void InputListener::setCursor(WindowCursor cursor) { _windowLayer.cursor = cursor; }

void InputListener::setLayerFlags(WindowLayerFlags flags) { _windowLayer.flags = flags; }

void InputListener::setOwner(Node *owner) { _owner = owner; }

void InputListener::setPriority(int32_t p) { _priority = p; }

void InputListener::setExclusive() {
	if (_scene) {
		_scene->getDirector()->getInputDispatcher()->setListenerExclusive(this);
	}
}

void InputListener::setExclusiveForTouch(uint32_t eventId) {
	if (_scene) {
		_scene->getDirector()->getInputDispatcher()->setListenerExclusiveForTouch(this, eventId);
	}
}

void InputListener::setSwallowEvents(EventMask &&mask) {
	if (_swallowEvents.any()) {
		_swallowEvents |= mask;
	} else {
		_swallowEvents = sp::move(mask);
	}
}

void InputListener::setSwallowEvents(const EventMask &mask) {
	if (_swallowEvents.any()) {
		_swallowEvents |= mask;
	} else {
		_swallowEvents = mask;
	}
}

void InputListener::setSwallowAllEvents() { _swallowEvents.set(); }

void InputListener::setSwallowEvent(InputEventName event) { _swallowEvents.set(toInt(event)); }

void InputListener::clearSwallowAllEvents() { _swallowEvents.reset(); }

void InputListener::clearSwallowEvent(InputEventName event) { _swallowEvents.reset(toInt(event)); }

void InputListener::clearSwallowEvents(const EventMask &event) { _swallowEvents &= ~event; }

bool InputListener::isSwallowAllEvents() const { return _swallowEvents.all(); }

bool InputListener::isSwallowAllEvents(const EventMask &event) const {
	return (_swallowEvents & event) == event;
}

bool InputListener::isSwallowAnyEvents(const EventMask &event) const {
	return (_swallowEvents & event).any();
}

bool InputListener::isSwallowEvent(InputEventName name) const {
	return _swallowEvents.test(toInt(name));
}

void InputListener::setTouchFilter(const EventFilter &filter) { _eventFilter = filter; }

bool InputListener::shouldSwallowEvent(const InputEvent &event) const {
	return _swallowEvents.test(toInt(event.data.event));
}

bool InputListener::canHandleEvent(const InputEvent &event) const {
	if (!_running || !_owner) {
		return false;
	}

	if (_eventMask.test(toInt(event.data.event)) && shouldProcessEvent(event)) {
		auto it = _callbacks.find(event.data.event);
		if (it != _callbacks.end()) {
			return true;
		}
		for (auto &it : _recognizers) {
			if (!_running || !_owner) {
				break;
			}

			if (it->canHandleEvent(event)) {
				return true;
			}
		}
	}
	return false;
}

InputEventState InputListener::handleEvent(const InputEvent &event) {
	InputEventState ret = InputEventState::Declined;
	auto it = _callbacks.find(event.data.event);
	if (it != _callbacks.end()) {
		switch (event.data.event) {
		case core::InputEventName::WindowState:
			if (auto f = std::get_if<Function<bool(WindowState, WindowState)>>(&it->second)) {
				ret = std::max((*f)(event.data.window.state, event.data.window.changes)
								? InputEventState::Processed
								: InputEventState::Declined,
						ret);
			}
			break;
		default: break;
		}
	}
	for (auto &it : _recognizers) {
		if (!_running || !_owner) {
			break;
		}

		auto result = it->handleInputEvent(event, _owner->getInputDensity());
		switch (result) {
		case InputEventState::Retain:
			result = InputEventState::Processed;
			retainEvent(event.data.event);
			break;
		case InputEventState::Release:
			releaseEvent(event.data.event);
			result = InputEventState::Processed;
			break;
		case InputEventState::DelayedProcessed:
			result = InputEventState::Processed;
			makeDelay();
			break;
		case InputEventState::DelayedCaptured:
			result = InputEventState::Captured;
			makeDelay();
			break;
		default: break;
		}
		if (result == InputEventState::Processed && shouldSwallowEvent(event)) {
			result = InputEventState::Captured;
		}
		ret = std::max(result, ret);
	}
	return ret;
}

bool InputListener::setFocused() {
	if (auto g = getFocusGroup()) {
		if (g->setFocus(this)) {
			return true;
		}
	}
	return false;
}

bool InputListener::isFocused() const { return _hasFocus; }

FocusGroup *InputListener::getFocusGroup() const {
	auto owner = getOwner();
	while (owner) {
		auto g = owner->getSystemByType<FocusGroup>();
		if (g) {
			return g;
		}
		owner = owner->getParent();
	}
	return nullptr;
}

GestureRecognizer *InputListener::addTouchRecognizer(InputCallback<GestureData> &&cb,
		InputTouchInfo &&info) {
	return addRecognizer(Rc<GestureTouchRecognizer>::create(sp::move(cb), sp::move(info)));
}

GestureRecognizer *InputListener::addTapRecognizer(InputCallback<GestureTap> &&cb,
		InputTapInfo &&info) {
	return addRecognizer(Rc<GestureTapRecognizer>::create(sp::move(cb), sp::move(info)));
}

GestureRecognizer *InputListener::addScrollRecognizer(InputCallback<GestureScroll> &&cb,
		InputScrollInfo &&info) {
	return addRecognizer(Rc<GestureScrollRecognizer>::create(sp::move(cb), sp::move(info)));
}

GestureRecognizer *InputListener::addPressRecognizer(InputCallback<GesturePress> &&cb,
		InputPressInfo &&info) {
	return addRecognizer(Rc<GesturePressRecognizer>::create(sp::move(cb), sp::move(info)));
}

GestureRecognizer *InputListener::addSwipeRecognizer(InputCallback<GestureSwipe> &&cb,
		InputSwipeInfo &&info) {
	return addRecognizer(Rc<GestureSwipeRecognizer>::create(sp::move(cb), sp::move(info)));
}

GestureRecognizer *InputListener::addPinchRecognizer(InputCallback<GesturePinch> &&cb,
		InputPinchInfo &&info) {
	return addRecognizer(Rc<GesturePinchRecognizer>::create(sp::move(cb), sp::move(info)));
}

GestureRecognizer *InputListener::addMoveRecognizer(InputCallback<GestureData> &&cb,
		InputMoveInfo &&info) {
	return addRecognizer(Rc<GestureMoveRecognizer>::create(sp::move(cb), sp::move(info)));
}

GestureRecognizer *InputListener::addMouseOverRecognizer(InputCallback<GestureData> &&cb,
		InputMouseOverInfo &&info) {
	return addRecognizer(Rc<GestureMouseOverRecognizer>::create(sp::move(cb), sp::move(info)));
}

GestureKeyRecognizer *InputListener::addKeyRecognizer(InputCallback<GestureData> &&cb,
		InputKeyInfo &&info) {
	return (GestureKeyRecognizer *)addRecognizer(
			Rc<GestureKeyRecognizer>::create(sp::move(cb), sp::move(info)));
}

void InputListener::setWindowStateCallback(Function<bool(WindowState, WindowState)> &&cb) {
	if (cb) {
		_callbacks.insert_or_assign(InputEventName::WindowState, sp::move(cb));
		_eventMask.set(toInt(InputEventName::WindowState));
	} else {
		_callbacks.erase(InputEventName::WindowState);
		_eventMask.reset(toInt(InputEventName::WindowState));
	}
}

void InputListener::clear() {
	_eventMask.reset();
	_recognizers.clear();
}

void InputListener::handleFocusIn(FocusGroup *) {
	_hasFocus = true;
	if (_focusCallback) {
		_focusCallback(_hasFocus);
	}
}

void InputListener::handleFocusOut(FocusGroup *) {
	_hasFocus = false;
	if (_focusCallback) {
		_focusCallback(_hasFocus);
	}
}

bool InputListener::shouldProcessEvent(const InputEvent &event) const {
	if (_retainedEvents.find(event.data.event) != _retainedEvents.end()) {
		return true;
	}
	if (!_eventFilter) {
		return _shouldProcessEvent(event);
	} else {
		return _eventFilter(event, std::bind(&InputListener::_shouldProcessEvent, this, event));
	}
}

bool InputListener::_shouldProcessEvent(const InputEvent &event) const {
	auto node = getOwner();
	if (node && _running) {
		bool visible = node->isVisible();
		auto p = node->getParent();
		while (visible && p) {
			visible = p->isVisible();
			p = p->getParent();
		}
		if (visible
				&& (!event.data.hasLocation()
						|| node->isTouched(event.currentLocation, _touchPadding))
				&& node->getOpacity() >= _opacityFilter) {
			return true;
		}
	}
	return false;
}

void InputListener::addEventMask(const EventMask &mask) {
	for (size_t i = 0; i < mask.size(); ++i) {
		if (mask.test(i)) {
			_eventMask.set(i);
		}
	}
}

GestureRecognizer *InputListener::addRecognizer(GestureRecognizer *rec) {
	addEventMask(rec->getEventMask());
	auto ret = _recognizers.emplace_back(rec).get();
	if (_running) {
		ret->onEnter(this);
		if (ret->requiresUpdate()) {
			scheduleUpdate();
		}
	}
	return ret;
}

void InputListener::retainEvent(core::InputEventName name) {
	auto it = _retainedEvents.find(name);
	if (it != _retainedEvents.end()) {
		++it->second;
	} else {
		_retainedEvents.emplace(name, 1);
	}
}

void InputListener::releaseEvent(core::InputEventName name) {
	auto it = _retainedEvents.find(name);
	if (it != _retainedEvents.end()) {
		if (it->second == 1) {
			_retainedEvents.erase(it);
		} else {
			--it->second;
		}
	}
}

void InputListener::makeDelay() {
	if (_running) {
		_owner->runAction(Rc<RenderContinuously>::create(1.0f));
	}
}
} // namespace stappler::xenolith
