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

#include "XLInputDispatcher.h"
#include "XLInputListener.h"
#include "XLAppWindow.h"
#include "XLNode.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

InputListenerStorage::~InputListenerStorage() { clear(); }

InputListenerStorage::InputListenerStorage(PoolRef *p) : PoolRef(p) {
	perform([&, this] {
		_preSceneEvents = new (_pool) memory::vector<Rec>;
		_sceneEvents = new (_pool) memory::vector<Rec>;
		_postSceneEvents = new (_pool) memory::vector<Rec>;

		_focus = new (_pool) memory::map<FocusGroup *, memory::vector<Rec *>>;
		_focus->set_memory_persistent(true);

		_sceneEvents->reserve(256);
	});
}

void InputListenerStorage::clear() {
	perform([&, this] {
		_focus->clear();
		_preSceneEvents->clear();
		_sceneEvents->clear();
		_postSceneEvents->clear();
		_order = 0;
	});
}

void InputListenerStorage::reserve(const InputListenerStorage *st) {
	_preSceneEvents->reserve(st->_preSceneEvents->size());
	_sceneEvents->reserve(st->_sceneEvents->size());
	_postSceneEvents->reserve(st->_postSceneEvents->size());
}

void InputListenerStorage::addListener(NotNull<InputListener> input, FocusGroup *focus,
		WindowLayer &&layer) {
	perform([&, this] {
		Rec *record = nullptr;
		auto p = input->getPriority();
		if (p == 0) {
			record =
					&_sceneEvents->emplace_back(Rec{input.get(), focus, sp::move(layer), ++_order});
		} else if (p < 0) {
			auto lb = std::lower_bound(_postSceneEvents->begin(), _postSceneEvents->end(),
					Rec{input.get(), focus}, [](const Rec &l, const Rec &r) {
				return l.listener->getPriority() < r.listener->getPriority();
			});

			if (lb == _postSceneEvents->end()) {
				record = &_postSceneEvents->emplace_back(
						Rec{input.get(), focus, sp::move(layer), ++_order});
			} else {
				record = &*_postSceneEvents->emplace(lb,
						Rec{input.get(), focus, sp::move(layer), ++_order});
			}
		} else {
			auto lb = std::lower_bound(_preSceneEvents->begin(), _preSceneEvents->end(),
					Rec{input.get(), focus}, [](const Rec &l, const Rec &r) {
				return l.listener->getPriority() < r.listener->getPriority();
			});

			if (lb == _preSceneEvents->end()) {
				record = &_preSceneEvents->emplace_back(
						Rec{input.get(), focus, sp::move(layer), ++_order});
			} else {
				record = &*_preSceneEvents->emplace(lb,
						Rec{input.get(), focus, sp::move(layer), ++_order});
			}
		}
		if (focus) {
			auto it = _focus->find(focus);
			if (it == _focus->end()) {
				it = _focus->emplace(focus, memory::vector<Rec *>()).first;
				it->second.reserve_block_optimal();
			}
			it->second.emplace_back(record);
		}
	});
}

void InputListenerStorage::sort() {
	for (auto &it : *_focus) {
		std::sort(it.second.begin(), it.second.end(), [](const Rec *l, const Rec *r) {
			auto lp = l->listener->getPriority();
			auto rp = r->listener->getPriority();
			// у кого приоритет больше - идёт первым
			if (lp > rp) {
				return true;
			} else if (lp < rp) {
				return false;
			} else {
				// при равном приоритете раньше тот, кто добавлен позже
				return l->order > r->order;
			}
		});
	}
}

SpanView<InputListenerStorage::Rec *> InputListenerStorage::getFocusGroupListener(
		FocusGroup *group) const {
	auto it = _focus->find(group);
	if (it != _focus->end()) {
		return it->second;
	}
	return SpanView<Rec *>();
}

bool InputDispatcher::init(PoolRef *pool, WindowState state) {
	_pool = pool;
	_windowState = state;
	return true;
}

void InputDispatcher::update(const UpdateTime &time) { _currentTime = time.global; }

Rc<InputListenerStorage> InputDispatcher::acquireNewStorage() {
	Rc<InputListenerStorage> req;
	if (_tmpEvents) {
		req = move(_tmpEvents);
		_tmpEvents = nullptr;
	} else {
		req = Rc<InputListenerStorage>::alloc(_pool);
	}
	if (_events) {
		req->reserve(_events);
	}
	return req;
}

void InputDispatcher::commitStorage(AppWindow *window, Rc<InputListenerStorage> &&storage) {
	_tmpEvents = move(_events);
	_events = move(storage);
	if (_tmpEvents) {
		_tmpEvents->clear();
	}

	// Sort focus groups
	_events->sort();

	_events->foreachFocusGroup(
			[](NotNull<FocusGroup> group, SpanView<InputListenerStorage::Rec *> l) {
		Vector<InputListener *> listeners;
		for (auto &it : l) { listeners.emplace_back(it->listener); }
		group->updateWithListeners(listeners);
		return true;
	}, nullptr);

	Vector<WindowLayer> layers;
	_events->foreachListener([&](const InputListenerStorage::Rec &rec) {
		if (rec.layer) {
			layers.emplace_back(rec.layer);
		}
		return true;
	}, nullptr);

	window->updateLayers(sp::move(layers));
}

void InputDispatcher::handleInputEvent(const InputEventData &event) {
	if (!_events) {
		return;
	}

	switch (event.event) {
	case InputEventName::None:
	case InputEventName::Max: return; break;
	case InputEventName::Begin: {
		auto v = _activeEvents.find(event.id);
		if (v == _activeEvents.end()) {
			v = _activeEvents
						.emplace(event.id,
								EventHandlersInfo{getEventInfo(event), Vector<Rc<InputListener>>()})
						.first;
		} else {
			// Cancel event with same id
			v->second.clear(true);
			v->second.event = getEventInfo(event);
		}

		v->second.addListenersFromStorage(_events);
		v->second.handle(true);
		break;
	}
	case InputEventName::Move: {
		auto v = _activeEvents.find(event.id);
		if (v != _activeEvents.end()) {
			updateEventInfo(v->second.event, event);
			v->second.handle(true);
		}
		break;
	}
	case InputEventName::End:
	case InputEventName::Cancel: {
		auto v = _activeEvents.find(event.id);
		if (v != _activeEvents.end()) {
			updateEventInfo(v->second.event, event);
			v->second.handle(false);
			v->second.clear(false);
			_activeEvents.erase(v);
		}
		break;
	}
	case InputEventName::MouseMove: {
		_pointerLocation = event.getLocation();

		EventHandlersInfo handlers{getEventInfo(event)};
		handlers.addListenersFromStorage(_events);
		handlers.handle(false);

		for (auto &it : _activeEvents) {
			if ((it.second.event.data.input.modifiers & InputModifier::Unmanaged)
					== InputModifier::None) {
				it.second.event.data.input.x = event.input.x;
				it.second.event.data.input.y = event.input.y;
				it.second.event.data.event = InputEventName::Move;
				it.second.event.data.input.modifiers = event.input.modifiers;
				handleInputEvent(it.second.event.data);
			}
		}
		break;
	}
	case InputEventName::Scroll: {
		EventHandlersInfo handlers{getEventInfo(event)};
		handlers.addListenersFromStorage(_events);
		handlers.handle(false);
		break;
	}
	case InputEventName::WindowState: {
		_windowState = event.window.state;
		EventHandlersInfo handlers{getEventInfo(event)};
		handlers.addListenersFromStorage(_events);
		handlers.handle(false);

		bool hasFocus = hasFlag(handlers.event.data.window.state, core::WindowState::Focused);
		bool inBackground =
				hasFlag(handlers.event.data.window.state, core::WindowState::Background);
		bool hasPointer = hasFlag(handlers.event.data.window.state, core::WindowState::Pointer);

		if (!hasFocus || inBackground || !hasPointer) {
			cancelTouchEvents(event.input.x, event.input.y, event.input.modifiers);
		}

		if (!hasFocus || inBackground) {
			cancelKeyEvents(event.input.x, event.input.y, event.input.modifiers);
		}

		break;
	}
	case InputEventName::KeyPressed: {
		auto v = resetKey(event);
		v->addListenersFromStorage(_events);
		v->handle(true);
		break;
	}
	case InputEventName::KeyRepeated: handleKey(event, false); break;
	case InputEventName::KeyReleased:
	case InputEventName::KeyCanceled: handleKey(event, true); break;
	}
}

Vector<InputEventData> InputDispatcher::getActiveEvents() const {
	Vector<InputEventData> eventsTmp;
	eventsTmp.reserve(_activeEvents.size());
	for (auto &it : _activeEvents) { eventsTmp.emplace_back(it.second.event.data); }
	return eventsTmp;
}

void InputDispatcher::setListenerExclusive(const InputListener *l) {
	for (auto &it : _activeEvents) { setListenerExclusive(it.second, l); }
	for (auto &it : _activeKeys) { setListenerExclusive(it.second, l); }
}

void InputDispatcher::setListenerExclusiveForTouch(const InputListener *l, uint32_t id) {
	auto it = _activeEvents.find(id);
	if (it != _activeEvents.end()) {
		setListenerExclusive(it->second, l);
	}
}

void InputDispatcher::setListenerExclusiveForKey(const InputListener *l, InputKeyCode id) {
	auto it = _activeKeys.find(id);
	if (it != _activeKeys.end()) {
		setListenerExclusive(it->second, l);
	}
}

bool InputDispatcher::hasActiveInput() const {
	return !_activeEvents.empty() || !_activeKeys.empty();
}

void InputDispatcher::resetWindowState(WindowState state, bool propagate) {
	_windowState = state;

	cancelTouchEvents(nan(), nan(), InputModifier::None);
	cancelKeyEvents(nan(), nan(), InputModifier::None);

	if (propagate) {
		core::InputEventData stateUpdate{
				0,
				core::InputEventName::WindowState,
				{.input = {core::InputMouseButton::None, core::InputModifier::None, nan(), nan()}},
				{.window = {state, WindowState::All}},
		};

		handleInputEvent(stateUpdate);
	}
}

InputEvent InputDispatcher::getEventInfo(const InputEventData &event) const {
	auto loc = event.getLocation();
	return InputEvent{event, loc, loc, loc, _currentTime, _currentTime, _currentTime,
		event.getModifiers(), event.getModifiers()};
}

void InputDispatcher::updateEventInfo(InputEvent &event, const InputEventData &data) const {
	event.previousLocation = event.currentLocation;
	event.currentLocation = data.getLocation();

	event.previousTime = event.currentTime;
	event.currentTime = _currentTime;

	event.previousModifiers = event.data.getModifiers();

	event.data.event = data.event;
	if (event.data.hasInput()) {
		event.data.input.x = data.input.x;
		event.data.input.y = data.input.y;
		event.data.input.button = data.input.button;
		event.data.input.modifiers = data.input.modifiers;
	}

	if (event.data.isPointEvent()) {
		event.data.point.valueX = data.point.valueX;
		event.data.point.valueY = data.point.valueY;
		event.data.point.density = data.point.density;
	} else if (event.data.isKeyEvent()) {
		event.data.key.keychar = data.key.keychar;
		event.data.key.keycode = data.key.keycode;
		event.data.key.keysym = data.key.keysym;
	}
}

void InputDispatcher::EventHandlersInfo::handle(bool removeOnFail) {
	processed.clear();
	if (exclusive) {
		processed.emplace_back(exclusive.get());
		auto res = exclusive->handleEvent(event);
		if (res == InputEventState::Declined) {
			exclusive = nullptr;
		}
	} else {
		Vector<Rc<InputListener>> listenerToRemove;
		auto vec = listeners;
		for (auto &it : vec) {
			processed.emplace_back(it.get());
			auto res = it->handleEvent(event);
			if (res == InputEventState::Captured && !exclusive) {
				setExclusive(it);
			}

			if (exclusive) {
				if (std::find(processed.begin(), processed.end(), exclusive.get())
						== processed.end()) {
					auto res = exclusive->handleEvent(event);
					if (res == InputEventState::Declined) {
						exclusive = nullptr;
					}
				}
				break;
			}

			if (removeOnFail && res == InputEventState::Declined) {
				listenerToRemove.emplace_back(it);
			}
		}

		for (auto &it : listenerToRemove) {
			auto iit = std::find(listeners.begin(), listeners.end(), it);
			if (iit != listeners.end()) {
				listeners.erase(iit);
			}
		}
	}
	processed.clear();
}

void InputDispatcher::EventHandlersInfo::clear(bool cancel) {
	if (cancel) {
		event.data.event = isKeyEvent ? InputEventName::KeyCanceled : InputEventName::Cancel;
		handle(false);
	}

	listeners.clear();
	exclusive = nullptr;
	exclusiveGroup = nullptr;
}

void InputDispatcher::EventHandlersInfo::setExclusive(const InputListener *l) {
	if (exclusive) {
		return;
	}

	auto v = std::find(listeners.begin(), listeners.end(), l);
	if (v != listeners.end()) {
		exclusive = *v;

		auto event = this->event;
		event.data.event = isKeyEvent ? InputEventName::KeyCanceled : InputEventName::Cancel;
		for (auto &iit : listeners) {
			if (iit.get() != l) {
				iit->handleEvent(event);
			}
		}
		listeners.clear();
	}
}

void InputDispatcher::EventHandlersInfo::addListenersFromStorage(
		NotNull<InputListenerStorage> storage) {
	storage->foreachListener([&](const InputListenerStorage::Rec &l) {
		if (l.listener->getOwner() && l.listener->canHandleEvent(event)) {
			if (l.focus && l.focus->canHandleEvent(event)) {
				if (hasFlag(l.focus->getFlags(), FocusGroup::Flags::Exclusive)) {
					// stop iteration, clear listeners, then use listeners from exclusive focus group
					if (!exclusiveGroup || l.focus->getPriority() > exclusiveGroup->getPriority()
							|| (l.focus->getPriority() <= exclusiveGroup->getPriority()
									&& l.focus->isParentGroup(exclusiveGroup))) {
						exclusiveGroup = l.focus;
					}
				} else if (!exclusiveGroup
						&& l.focus->canHandleEventWithListener(event, l.listener)) {
					listeners.emplace_back(l.listener);
				}
			} else if (!exclusiveGroup) {
				listeners.emplace_back(l.listener);
			}
		}
		return true;
	}, nullptr);

	if (exclusiveGroup) {
		listeners.clear();
		storage->foreachListener([&](const InputListenerStorage::Rec &l) {
			if (l.listener->getOwner() && l.listener->canHandleEvent(event)) {
				if (!l.focus || l.focus->canHandleEventWithListener(event, l.listener)) {
					listeners.emplace_back(l.listener);
				}
			}
			return true;
		}, exclusiveGroup);
	}
}

void InputDispatcher::setListenerExclusive(EventHandlersInfo &info, const InputListener *l) const {
	info.setExclusive(l);
}

void InputDispatcher::clearKey(const InputEventData &event) {
	if (event.key.keycode == InputKeyCode::Unknown) {
		auto v = _activeKeySyms.find(event.key.keysym);
		if (v != _activeKeySyms.end()) {
			v->second.clear(true);
			_activeKeySyms.erase(v);
		}
	} else {
		auto v = _activeKeys.find(event.key.keycode);
		if (v != _activeKeys.end()) {
			v->second.clear(true);
			_activeKeys.erase(v);
		}
	}
}

InputDispatcher::EventHandlersInfo *InputDispatcher::resetKey(const InputEventData &event) {
	if (event.key.keycode == InputKeyCode::Unknown) {
		auto v = _activeKeySyms.find(event.key.keysym);
		if (v == _activeKeySyms.end()) {
			v = _activeKeySyms
						.emplace(event.key.keysym,
								EventHandlersInfo{getEventInfo(event), Vector<Rc<InputListener>>()})
						.first;
		} else {
			v->second.clear(true);
			v->second.event = getEventInfo(event);
		}
		v->second.isKeyEvent = true;
		return &v->second;
	} else {
		auto v = _activeKeys.find(event.key.keycode);
		if (v == _activeKeys.end()) {
			v = _activeKeys
						.emplace(event.key.keycode,
								EventHandlersInfo{getEventInfo(event), Vector<Rc<InputListener>>()})
						.first;
		} else {
			v->second.clear(true);
			v->second.event = getEventInfo(event);
		}
		v->second.isKeyEvent = true;
		return &v->second;
	}
}

void InputDispatcher::handleKey(const InputEventData &event, bool clear) {
	if (event.key.keycode == InputKeyCode::Unknown) {
		auto v = _activeKeySyms.find(event.key.keysym);
		if (v != _activeKeySyms.end()) {
			updateEventInfo(v->second.event, event);
			v->second.handle(!clear);
			if (clear) {
				v->second.clear(false);
				_activeKeySyms.erase(v);
			}
		}
	} else {
		auto v = _activeKeys.find(event.key.keycode);
		if (v != _activeKeys.end()) {
			updateEventInfo(v->second.event, event);
			v->second.handle(!clear);
			if (clear) {
				v->second.clear(false);
				_activeKeys.erase(v);
			}
		}
	}
}

void InputDispatcher::cancelTouchEvents(float x, float y, InputModifier mods) {
	auto tmpEvents = _activeEvents;
	for (auto &it : tmpEvents) {
		it.second.event.data.input.x = x;
		it.second.event.data.input.y = y;
		it.second.event.data.event = InputEventName::Cancel;
		it.second.event.data.input.modifiers = mods;
		handleInputEvent(it.second.event.data);
	}
	_activeEvents.clear();
}

void InputDispatcher::cancelKeyEvents(float x, float y, InputModifier mods) {
	auto tmpEvents = _activeKeys;
	for (auto &it : tmpEvents) {
		it.second.event.data.input.x = x;
		it.second.event.data.input.y = y;
		it.second.event.data.event = InputEventName::KeyCanceled;
		it.second.event.data.input.modifiers = mods;
		handleInputEvent(it.second.event.data);
	}
	_activeKeys.clear();
}

} // namespace stappler::xenolith
