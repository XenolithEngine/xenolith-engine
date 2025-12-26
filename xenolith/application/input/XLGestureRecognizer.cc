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

#include "XLGestureRecognizer.h"
#include "XLInputListener.h"
#include "XLNode.h"
#include "XLDirector.h"
#include "XLInputDispatcher.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

const Vec2 &GestureScroll::location() const { return pos; }

void GestureScroll::cleanup() {
	pos = Vec2::ZERO;
	amount = Vec2::ZERO;
}


void GestureTap::cleanup() {
	id = maxOf<uint32_t>();
	time.clear();
	count = 0;
}

void GesturePress::cleanup() {
	id = maxOf<uint32_t>();
	limit.clear();
	time.clear();
	tickCount = 0;
}

void GestureSwipe::cleanup() {
	firstTouch = Vec2::ZERO;
	secondTouch = Vec2::ZERO;
	midpoint = Vec2::ZERO;
	delta = Vec2::ZERO;
	velocity = Vec2::ZERO;
}

void GesturePinch::cleanup() {
	first = Vec2::ZERO;
	second = Vec2::ZERO;
	center = Vec2::ZERO;
	startDistance = 0.0f;
	prevDistance = 0.0f;
	distance = 0.0f;
	scale = 0.0f;
	velocity = 0.0f;
}

bool GestureRecognizer::init() { return true; }

bool GestureRecognizer::canHandleEvent(const InputEvent &event) const {
	if (_eventMask.test(toInt(event.data.event))) {
		if (!_buttonMask.any() || _buttonMask.test(toInt(event.data.getButton()))) {
			return true;
		}
	}
	return false;
}

InputEventState GestureRecognizer::handleInputEvent(const InputEvent &event, float density) {
	if (!_eventMask.test(toInt(event.data.event))) {
		return InputEventState::Declined;
	}

	if (_buttonMask.any() && !_buttonMask.test(toInt(event.data.getButton()))) {
		return InputEventState::Declined;
	}

	_density = density;

	switch (event.data.event) {
	case InputEventName::Begin:
	case InputEventName::KeyPressed: return addEvent(event, density); break;
	case InputEventName::Move:
	case InputEventName::KeyRepeated: return renewEvent(event, density); break;
	case InputEventName::End:
	case InputEventName::KeyReleased: return removeEvent(event, true, density); break;
	case InputEventName::Cancel:
	case InputEventName::KeyCanceled: return removeEvent(event, false, density); break;
	default: break;
	}
	return InputEventState::Processed;
}

void GestureRecognizer::onEnter(InputListener *) { }

void GestureRecognizer::onExit() { }

uint32_t GestureRecognizer::getEventCount() const { return (uint32_t)_events.size(); }

bool GestureRecognizer::hasEvent(const InputEvent &event) const {
	if (_events.size() == 0) {
		return false;
	}

	for (auto &pEvent : _events) {
		if (pEvent.data.id == event.data.id) {
			return true;
		}
	}

	return false;
}

GestureRecognizer::EventMask GestureRecognizer::getEventMask() const { return _eventMask; }

void GestureRecognizer::update(uint64_t dt) { }

Vec2 GestureRecognizer::getLocation() const {
	if (_events.size() > 0) {
		return Vec2(_events.back().currentLocation);
	} else {
		return Vec2::ZERO;
	}
}

void GestureRecognizer::cancel() {
	auto eventsToRemove = _events;
	for (auto &event : eventsToRemove) { removeEvent(event, false, _density); }
}

bool GestureRecognizer::canAddEvent(const InputEvent &event) const {
	if (_events.size() < _maxEvents) {
		for (auto &it : _events) {
			if (event.data.id == it.data.id) {
				return false;
			}
		}
		return true;
	} else {
		return false;
	}
}

InputEventState GestureRecognizer::addEvent(const InputEvent &event, float density) {
	if (_events.size() < _maxEvents) {
		for (auto &it : _events) {
			if (event.data.id == it.data.id) {
				return InputEventState::Declined;
			}
		}
		_events.emplace_back(event);
		return InputEventState::Processed;
	} else {
		return InputEventState::Declined;
	}
}

InputEventState GestureRecognizer::removeEvent(const InputEvent &event, bool success,
		float density) {
	if (_events.size() == 0) {
		return InputEventState::Declined;
	}

	uint32_t index = maxOf<uint32_t>();
	auto pEvent = getTouchById(event.data.id, &index);
	if (pEvent && index < _events.size()) {
		_events.erase(_events.begin() + index);
		return InputEventState::Processed;
	} else {
		return InputEventState::Declined;
	}
}

InputEventState GestureRecognizer::renewEvent(const InputEvent &event, float density) {
	if (_events.size() == 0) {
		return InputEventState::Declined;
	}

	uint32_t index = maxOf<uint32_t>();
	auto pEvent = getTouchById(event.data.id, &index);
	if (pEvent && index < _events.size()) {
		_events[index] = event;
		return InputEventState::Processed;
	} else {
		return InputEventState::Declined;
	}
}

InputEvent *GestureRecognizer::getTouchById(uint32_t id, uint32_t *index) {
	InputEvent *pTouch = nullptr;
	uint32_t i = 0;
	for (i = 0; i < _events.size(); i++) {
		pTouch = &_events.at(i);
		if (pTouch->data.id == id) {
			if (index) {
				*index = i;
			}
			return pTouch;
		}
	}
	return nullptr;
}

bool GestureTouchRecognizer::init(InputCallback &&cb, InputTouchInfo &&info) {
	if (!GestureRecognizer::init()) {
		return false;
	}

	if (cb) {
		_maxEvents = 10;
		_buttonMask = info.buttonMask;
		_callback = sp::move(cb);
		_info = sp::move(info);
		_eventMask.set(toInt(InputEventName::Begin));
		_eventMask.set(toInt(InputEventName::Move));
		_eventMask.set(toInt(InputEventName::End));
		_eventMask.set(toInt(InputEventName::Cancel));
		return true;
	}
	return false;
}

bool GestureTouchRecognizer::canHandleEvent(const InputEvent &event) const {
	if (GestureRecognizer::canHandleEvent(event)) {
		if (_buttonMask.test(toInt(event.data.getButton()))) {
			return true;
		}
	}
	return false;
}

void GestureTouchRecognizer::removeRecognizedEvent(uint32_t id) {
	for (auto it = _events.begin(); it != _events.end(); it++) {
		if ((*it).data.id == id) {
			_events.erase(it);
			break;
		}
	}
}

InputEventState GestureTouchRecognizer::addEvent(const InputEvent &event, float density) {
	if (!_buttonMask.test(toInt(event.data.getButton()))) {
		return InputEventState::Declined;
	}

	if (GestureRecognizer::addEvent(event, density) != InputEventState::Declined) {
		_event.event = GestureEvent::Began;
		_event.input = &event;
		if (!_callback(_event)) {
			removeRecognizedEvent(event.data.id);
			_event.event = GestureEvent::Cancelled;
			_event.input = nullptr;
			return InputEventState::Declined;
		}
		return InputEventState::Captured;
	}
	return InputEventState::Declined;
}

InputEventState GestureTouchRecognizer::removeEvent(const InputEvent &event, bool successful,
		float density) {
	if (GestureRecognizer::removeEvent(event, successful, density) != InputEventState::Declined) {
		_event.event = successful ? GestureEvent::Ended : GestureEvent::Cancelled;
		_event.input = &event;
		_callback(_event);
		_event.event = GestureEvent::Cancelled;
		_event.input = nullptr;
		return InputEventState::Processed;
	}
	return InputEventState::Declined;
}

InputEventState GestureTouchRecognizer::renewEvent(const InputEvent &event, float density) {
	if (GestureRecognizer::renewEvent(event, density) != InputEventState::Declined) {
		_event.event = GestureEvent::Activated;
		_event.input = &event;
		if (!_callback(_event)) {
			removeRecognizedEvent(event.data.id);
			_event.event = GestureEvent::Cancelled;
			_event.input = nullptr;
			return InputEventState::Declined;
		}
		return InputEventState::Processed;
	}
	return InputEventState::Declined;
}


bool GestureTapRecognizer::init(InputCallback &&cb, InputTapInfo &&info) {
	if (!GestureRecognizer::init()) {
		return false;
	}

	if (cb) {
		_maxEvents = 1;
		_buttonMask = info.buttonMask;
		_callback = sp::move(cb);
		_info = sp::move(info);
		_eventMask.set(toInt(InputEventName::Begin));
		_eventMask.set(toInt(InputEventName::Move));
		_eventMask.set(toInt(InputEventName::End));
		_eventMask.set(toInt(InputEventName::Cancel));
		return true;
	}
	return false;
}

void GestureTapRecognizer::update(uint64_t dt) {
	GestureRecognizer::update(dt);

	auto now = Time::now();
	if (_gesture.count > 0 && _gesture.time - now > TapIntervalAllowed) {
		_gesture.event = GestureEvent::Activated;
		_gesture.input = _events.empty() ? &_tmpEvent : &_events.front();
		_callback(_gesture);
		_gesture.event = GestureEvent::Cancelled;
		_gesture.input = nullptr;
		_gesture.time = Time();
		_gesture.cleanup();
	}
}

void GestureTapRecognizer::cancel() {
	GestureRecognizer::cancel();
	_gesture.cleanup();
}

InputEventState GestureTapRecognizer::addEvent(const InputEvent &ev, float density) {
	if (_gesture.count > 0
			&& _gesture.pos.distance(ev.currentLocation) > TapDistanceAllowedMulti * density) {
		_gesture.cleanup();
		return InputEventState::Declined;
	}
	if (GestureRecognizer::addEvent(ev, density) != InputEventState::Declined) {
		auto count = _gesture.count;
		auto time = _gesture.time;
		_gesture.cleanup();
		if (time - Time::now() < TapIntervalAllowed) {
			_gesture.count = count;
			_gesture.time = time;
		}
		_gesture.id = ev.data.id;
		_gesture.pos = ev.currentLocation;
		return InputEventState::Processed;
	}
	return InputEventState::Declined;
}

InputEventState GestureTapRecognizer::removeEvent(const InputEvent &ev, bool successful,
		float density) {
	InputEventState ret = InputEventState::Declined;
	if (GestureRecognizer::removeEvent(ev, successful, density) != InputEventState::Declined) {
		_tmpEvent = ev;
		if (successful
				&& _gesture.pos.distance(ev.currentLocation) <= TapDistanceAllowed * density) {
			if (!registerTap()) {
				ret = _info.exclusive ? InputEventState::DelayedCaptured
									  : InputEventState::DelayedProcessed;
			} else {
				ret = _info.exclusive ? InputEventState::Captured : InputEventState::Processed;
			}
		} else {
			ret = InputEventState::Processed;
		}
	}
	return ret;
}

InputEventState GestureTapRecognizer::renewEvent(const InputEvent &ev, float density) {
	auto ret = GestureRecognizer::renewEvent(ev, density);
	if (ret != InputEventState::Declined) {
		if (_gesture.pos.distance(ev.currentLocation) > TapDistanceAllowed * density) {
			return removeEvent(ev, false, density);
		}
	}
	return ret;
}

bool GestureTapRecognizer::registerTap() {
	auto currentTime = Time::now();

	if (currentTime < _gesture.time + TapIntervalAllowed) {
		_gesture.count++;
	} else {
		_gesture.count = 1;
	}

	_gesture.time = currentTime;
	if (_gesture.count == _info.maxTapCount) {
		_gesture.event = GestureEvent::Activated;
		_gesture.input = _events.empty() ? &_tmpEvent : &_events.front();
		_callback(_gesture);
		_gesture.event = GestureEvent::Cancelled;
		_gesture.input = nullptr;
		_gesture.cleanup();
		return true;
	} else {
		return false;
	}
}

bool GesturePressRecognizer::init(InputCallback &&cb, InputPressInfo &&info) {
	if (!GestureRecognizer::init()) {
		return false;
	}

	if (cb) {
		_maxEvents = 1;
		_buttonMask = info.buttonMask;
		_callback = sp::move(cb);
		_info = sp::move(info);

		// enable all touch events
		_eventMask.set(toInt(InputEventName::Begin));
		_eventMask.set(toInt(InputEventName::Move));
		_eventMask.set(toInt(InputEventName::End));
		_eventMask.set(toInt(InputEventName::Cancel));
		return true;
	}
	return false;
}

void GesturePressRecognizer::cancel() {
	GestureRecognizer::cancel();
	_gesture.cleanup();
	_lastTime.clear();
}

void GesturePressRecognizer::update(uint64_t dt) {
	if ((!_notified || hasFlag(_info.flags, InputPressFlags::Continuous)) && _lastTime
			&& _events.size() > 0) {
		auto time = Time::now() - _lastTime;
		if (_gesture.time.mksec() / _info.interval.mksec()
				!= time.mksec() / _info.interval.mksec()) {
			_gesture.time = time;
			++_gesture.tickCount;
			_gesture.event = GestureEvent::Activated;
			_gesture.input = &_events.front();
			if (!_callback(_gesture)) {
				cancel();
			}
			_notified = true;
		}
	}
}

InputEventState GesturePressRecognizer::addEvent(const InputEvent &event, float density) {
	if (GestureRecognizer::canAddEvent(event)) {
		_gesture.cleanup();
		_gesture.pos = event.currentLocation;
		_gesture.time.clear();
		_gesture.limit = _info.interval;
		_gesture.event = GestureEvent::Began;
		_gesture.input = &event;
		if (_callback(_gesture)) {
			GestureRecognizer::addEvent(event, density);
			_lastTime = Time::now();
			_notified = false;
			if (hasFlag(_info.flags, InputPressFlags::Capture)) {
				return InputEventState::Captured;
			} else {
				return InputEventState::Processed;
			}
		}
	}
	return InputEventState::Declined;
}

InputEventState GesturePressRecognizer::removeEvent(const InputEvent &event, bool successful,
		float density) {
	if (GestureRecognizer::removeEvent(event, successful, density) != InputEventState::Declined) {
		float distance = event.originalLocation.distance(event.currentLocation);
		_gesture.time = Time::now() - _lastTime;
		_gesture.event = (successful && distance <= TapDistanceAllowed * density)
				? GestureEvent::Ended
				: GestureEvent::Cancelled;
		_gesture.input = &event;
		_callback(_gesture);
		_gesture.event = GestureEvent::Cancelled;
		_gesture.input = nullptr;
		_lastTime.clear();
		_gesture.cleanup();
		_notified = true;
		return InputEventState::Processed;
	}
	return InputEventState::Declined;
}

InputEventState GesturePressRecognizer::renewEvent(const InputEvent &event, float density) {
	if (GestureRecognizer::renewEvent(event, density) != InputEventState::Declined) {
		if (event.originalLocation.distance(event.currentLocation) > TapDistanceAllowed * density) {
			return removeEvent(event, false, density);
		}
		return InputEventState::Processed;
	}
	return InputEventState::Declined;
}

bool GestureSwipeRecognizer::init(InputCallback &&cb, InputSwipeInfo &&info) {
	if (!GestureRecognizer::init()) {
		return false;
	}

	if (cb) {
		_maxEvents = 2;
		_buttonMask = info.buttonMask;
		_callback = sp::move(cb);
		_info = sp::move(info);

		// enable all touch events
		_eventMask.set(toInt(InputEventName::Begin));
		_eventMask.set(toInt(InputEventName::Move));
		_eventMask.set(toInt(InputEventName::End));
		_eventMask.set(toInt(InputEventName::Cancel));
		return true;
	}
	return false;
}

void GestureSwipeRecognizer::cancel() {
	GestureRecognizer::cancel();
	_gesture.cleanup();
	_swipeBegin = false;
	_lastTime.clear();
	_currentTouch = maxOf<uint32_t>();
}

InputEventState GestureSwipeRecognizer::addEvent(const InputEvent &event, float density) {
	if (GestureRecognizer::addEvent(event, density) != InputEventState::Declined) {
		Vec2 accum;
		for (auto &it : _events) { accum = accum + it.currentLocation; }
		accum /= float(_events.size());

		_gesture.midpoint = accum;
		_currentTouch = event.data.id;
		_lastTime = Time::now();
		return InputEventState::Processed;
	} else {
		return InputEventState::Declined;
	}
}

InputEventState GestureSwipeRecognizer::removeEvent(const InputEvent &event, bool successful,
		float density) {
	if (GestureRecognizer::removeEvent(event, successful, density) != InputEventState::Declined) {
		if (_events.size() > 0) {
			_currentTouch = _events.back().data.id;
			_lastTime = Time::now();
		} else {
			if (_swipeBegin) {
				_gesture.event = successful ? GestureEvent::Ended : GestureEvent::Cancelled;
				_gesture.input = &event;
				_callback(_gesture);
			}

			_gesture.event = GestureEvent::Cancelled;
			_gesture.input = nullptr;
			_gesture.cleanup();
			_swipeBegin = false;

			_currentTouch = maxOf<uint32_t>();

			_velocityX.dropValues();
			_velocityY.dropValues();

			_lastTime.clear();
		}
		return InputEventState::Processed;
	}
	return InputEventState::Declined;
}

InputEventState GestureSwipeRecognizer::renewEvent(const InputEvent &event, float density) {
	if (GestureRecognizer::renewEvent(event, density) != InputEventState::Declined) {
		if (_events.size() == 1) {
			Vec2 current = event.currentLocation;
			Vec2 prev = (_swipeBegin) ? event.previousLocation : event.originalLocation;

			_gesture.secondTouch = _gesture.firstTouch = current;
			_gesture.midpoint = current;

			_gesture.delta = current - prev;
			_gesture.density = density;

			if (!_swipeBegin && _gesture.delta.length() > _info.threshold * density) {
				_gesture.cleanup();
				if (_info.sendThreshold) {
					_gesture.delta = current - prev;
				} else {
					_gesture.delta = current - event.previousLocation;
				}
				_gesture.secondTouch = _gesture.firstTouch = current;
				_gesture.midpoint = current;

				_swipeBegin = true;
				_gesture.event = GestureEvent::Began;
				_gesture.input = &event;
				if (!_callback(_gesture)) {
					_swipeBegin = false;
					cancel();
					return InputEventState::Declined;
				}

				if (_info.sendThreshold) {
					_gesture.delta = current - event.previousLocation;
				}
			}

			if (_swipeBegin /* && _gesture.delta.length() > 0.01f */) {
				auto t = Time::now();
				float fsec = (t - _lastTime).toFloatSeconds();
				if (_gesture.delta != Vec2::ZERO && fsec > 1.0f / 500.0f) {
					auto tmd = _gesture.delta / fsec;

					float velX = _velocityX.step(tmd.x);
					float velY = _velocityY.step(tmd.y);

					_gesture.velocity = Vec2(velX, velY);
					_lastTime = t;
				}

				_gesture.event = GestureEvent::Activated;
				_gesture.input = &event;
				if (!_callback(_gesture)) {
					cancel();
					return InputEventState::Declined;
				}
			}
		} else if (_events.size() == 2) {
			Vec2 current = event.currentLocation;
			Vec2 second = _gesture.secondTouch;
			Vec2 prev = _gesture.midpoint;

			_gesture.density = density;

			if (event.data.id != _currentTouch) {
				second = _gesture.secondTouch = current;
			} else if (event.data.id == _currentTouch) {
				_gesture.firstTouch = current;
				_gesture.midpoint = _gesture.secondTouch.getMidpoint(_gesture.firstTouch);
				_gesture.delta = _gesture.midpoint - prev;

				if (!_swipeBegin && _gesture.delta.length() > _info.threshold * density) {
					_gesture.cleanup();
					_gesture.firstTouch = current;
					_gesture.secondTouch = current;
					_gesture.midpoint = _gesture.secondTouch.getMidpoint(_gesture.firstTouch);
					_gesture.delta = _gesture.midpoint - prev;

					_swipeBegin = true;
					_gesture.event = GestureEvent::Began;
					_gesture.input = &event;
					if (!_callback(_gesture)) {
						cancel();
						return InputEventState::Declined;
					}
				}

				if (_swipeBegin /* && _gesture.delta.length() > 0.01f */) {
					auto t = Time::now();
					float fsec = (t - _lastTime).toFloatSeconds();
					if (_gesture.delta != Vec2::ZERO && fsec > 1.0f / 500.0f) {
						auto tmd = _gesture.delta / fsec;

						float velX = _velocityX.step(tmd.x);
						float velY = _velocityY.step(tmd.y);

						_gesture.velocity = Vec2(velX, velY);
						_lastTime = t;
					}

					_gesture.event = GestureEvent::Activated;
					_gesture.input = &event;
					if (!_callback(_gesture)) {
						cancel();
						return InputEventState::Declined;
					}
				}
			}
		}
		return _swipeBegin ? InputEventState::Captured : InputEventState::Processed;
	} else {
		return InputEventState::Declined;
	}
}

bool GesturePinchRecognizer::init(InputCallback &&cb, InputPinchInfo &&info) {
	if (!GestureRecognizer::init()) {
		return false;
	}

	if (cb) {
		_maxEvents = 2;
		_buttonMask = info.buttonMask;
		_callback = sp::move(cb);
		_info = sp::move(info);

		// enable all touch events
		_eventMask.set(toInt(InputEventName::Begin));
		_eventMask.set(toInt(InputEventName::Move));
		_eventMask.set(toInt(InputEventName::End));
		_eventMask.set(toInt(InputEventName::Cancel));
		return true;
	}
	return false;
}

void GesturePinchRecognizer::cancel() {
	GestureRecognizer::cancel();
	_gesture.cleanup();
	_velocity.dropValues();
	_lastTime.clear();
}

InputEventState GesturePinchRecognizer::addEvent(const InputEvent &event, float density) {
	if (GestureRecognizer::addEvent(event, density) != InputEventState::Declined) {
		if (_events.size() == 2) {
			_gesture.cleanup();
			_gesture.first = _events.at(0).currentLocation;
			_gesture.second = _events.at(1).currentLocation;
			_gesture.center = _gesture.first.getMidpoint(_gesture.second);
			_gesture.distance = _gesture.prevDistance = _gesture.startDistance =
					_gesture.first.distance(_gesture.second);
			_gesture.scale = _gesture.distance / _gesture.startDistance;
			_gesture.event = GestureEvent::Began;
			_gesture.input = &_events.at(0);
			_gesture.density = density;
			_lastTime = Time::now();
			if (_callback) {
				_callback(_gesture);
			}
			return InputEventState::Captured;
		}
		return InputEventState::Processed;
	} else {
		return InputEventState::Declined;
	}
}

InputEventState GesturePinchRecognizer::removeEvent(const InputEvent &event, bool successful,
		float density) {
	if (GestureRecognizer::removeEvent(event, successful, density) != InputEventState::Declined) {
		if (_events.size() == 1) {
			_gesture.event = successful ? GestureEvent::Ended : GestureEvent::Cancelled;
			if (_callback) {
				_callback(_gesture);
			}
			_gesture.cleanup();
			_lastTime.clear();
			_velocity.dropValues();
		}
		return InputEventState::Processed;
	}
	return InputEventState::Declined;
}

InputEventState GesturePinchRecognizer::renewEvent(const InputEvent &event, float density) {
	if (GestureRecognizer::renewEvent(event, density) != InputEventState::Declined) {
		if (_events.size() == 2) {
			auto &first = _events.at(0);
			auto &second = _events.at(1);
			if (event.data.id == first.data.id || event.data.id == second.data.id) {
				auto scale = _gesture.scale;

				_gesture.first = _events.at(0).currentLocation;
				_gesture.second = _events.at(1).currentLocation;
				_gesture.center = _gesture.first.getMidpoint(_gesture.second);
				_gesture.prevDistance =
						_events.at(0).previousLocation.distance(_events.at(1).previousLocation);
				_gesture.distance = _gesture.first.distance(_gesture.second);
				_gesture.scale = _gesture.distance / _gesture.startDistance;
				_gesture.density = density;

				auto t = Time::now();
				float tm = (float)1'000'000LL / (float)((t - _lastTime).toMicroseconds());
				if (tm > 80) {
					tm = 80;
				}
				_velocity.addValue((scale - _gesture.scale) * tm);
				_gesture.velocity = _velocity.getAverage();

				_gesture.event = GestureEvent::Activated;
				if (_callback) {
					_callback(_gesture);
				}

				_lastTime = t;
			}
			return InputEventState::Captured;
		}
		return InputEventState::Processed;
	}
	return InputEventState::Declined;
}

bool GestureScrollRecognizer::init(InputCallback &&cb, InputScrollInfo &&info) {
	if (!GestureRecognizer::init()) {
		return false;
	}

	if (cb) {
		_callback = sp::move(cb);
		_info = sp::move(info);
		_eventMask.set(toInt(InputEventName::Scroll));
		return true;
	}

	return false;
}

InputEventState GestureScrollRecognizer::handleInputEvent(const InputEvent &event, float density) {
	if (!_eventMask.test(toInt(event.data.event))) {
		return InputEventState::Declined;
	}

	_gesture.event = GestureEvent::Activated;
	_gesture.input = &event;
	_gesture.pos = event.currentLocation;
	_gesture.amount = Vec2(event.data.point.valueX, event.data.point.valueY);
	if (_callback) {
		_callback(_gesture);
	}
	_gesture.event = GestureEvent::Cancelled;
	return InputEventState::Captured;
}


bool GestureMoveRecognizer::init(InputCallback &&cb, InputMoveInfo &&info) {
	if (!GestureRecognizer::init()) {
		return false;
	}

	if (cb) {
		_callback = sp::move(cb);
		_info = sp::move(info);
		_eventMask.set(toInt(InputEventName::MouseMove));
		return true;
	}

	return false;
}

bool GestureMoveRecognizer::canHandleEvent(const InputEvent &event) const {
	if (GestureRecognizer::canHandleEvent(event)) {
		if (!_info.withinNode
				|| (_listener && _listener->getOwner()
						&& _listener->getOwner()->isTouched(event.currentLocation,
								_listener->getTouchPadding()))) {
			return true;
		}
	}
	return false;
}

InputEventState GestureMoveRecognizer::handleInputEvent(const InputEvent &event, float density) {
	if (!canHandleEvent(event)) {
		return InputEventState::Declined;
	}

	if (!_eventMask.test(toInt(event.data.event))) {
		return InputEventState::Declined;
	}

	_event.event = GestureEvent::Activated;
	_event.input = &event;
	if (_callback) {
		_callback(_event);
	}
	_event.input = nullptr;
	_event.event = GestureEvent::Cancelled;
	return InputEventState::Processed;
}

void GestureMoveRecognizer::onEnter(InputListener *l) {
	GestureRecognizer::onEnter(l);
	_listener = l;
}

void GestureMoveRecognizer::onExit() {
	_listener = nullptr;
	GestureRecognizer::onExit();
}

bool GestureKeyRecognizer::init(InputCallback &&cb, InputKeyInfo &&info) {
	if (!GestureRecognizer::init()) {
		return false;
	}

	if (cb && info.keyMask.any()) {
		_callback = sp::move(cb);
		_info = sp::move(info);
		_eventMask.set(toInt(InputEventName::KeyPressed));
		_eventMask.set(toInt(InputEventName::KeyRepeated));
		_eventMask.set(toInt(InputEventName::KeyReleased));
		_eventMask.set(toInt(InputEventName::KeyCanceled));
		return true;
	}

	log::source().error("GestureKeyRecognizer", "Callback or key mask is not defined");
	return false;
}

bool GestureKeyRecognizer::canHandleEvent(const InputEvent &ev) const {
	if (GestureRecognizer::canHandleEvent(ev)) {
		if (_info.keyMask.test(toInt(ev.data.key.keycode))) {
			return true;
		}
	}
	return false;
}

bool GestureKeyRecognizer::isKeyPressed(InputKeyCode code) const {
	if (_pressedKeys.test(toInt(code))) {
		return true;
	}
	return false;
}

InputEventState GestureKeyRecognizer::addEvent(const InputEvent &event, float density) {
	if (_info.keyMask.test(toInt(event.data.key.keycode))) {
		_pressedKeys.set(toInt(event.data.key.keycode));
		return _callback(GestureData{GestureEvent::Began, &event}) ? InputEventState::Captured
																   : InputEventState::Declined;
	}
	return InputEventState::Declined;
}

InputEventState GestureKeyRecognizer::removeEvent(const InputEvent &event, bool success,
		float density) {
	if (_pressedKeys.test(toInt(event.data.key.keycode))) {
		_callback(GestureData{success ? GestureEvent::Ended : GestureEvent::Cancelled, &event});
		_pressedKeys.reset(toInt(event.data.key.keycode));
		return InputEventState::Processed;
	}
	return InputEventState::Declined;
}

InputEventState GestureKeyRecognizer::renewEvent(const InputEvent &event, float density) {
	if (_pressedKeys.test(toInt(event.data.key.keycode))) {
		_callback(GestureData{GestureEvent::Activated, &event});
		return InputEventState::Processed;
	}
	return InputEventState::Declined;
}

bool GestureMouseOverRecognizer::init(InputCallback &&cb, InputMouseOverInfo &&info) {
	if (!GestureRecognizer::init()) {
		return false;
	}

	if (cb) {
		_callback = sp::move(cb);
		_info = sp::move(info);
		_eventMask.set(toInt(InputEventName::MouseMove));
		_eventMask.set(toInt(InputEventName::WindowState));
		return true;
	}

	log::source().error("GestureKeyRecognizer", "Callback or key mask is not defined");
	return false;
}

InputEventState GestureMouseOverRecognizer::handleInputEvent(const InputEvent &event,
		float density) {
	InputEventState ret = InputEventState::Processed;
	bool stateChanged = false;
	switch (event.data.event) {
	case InputEventName::WindowState:
		if (_viewHasFocus != hasFlag(event.data.window.state, core::WindowState::Focused)) {
			_viewHasFocus = hasFlag(event.data.window.state, core::WindowState::Focused);
			stateChanged = true;
		}
		if (_viewHasPointer != hasFlag(event.data.window.state, core::WindowState::Pointer)) {
			_viewHasPointer = hasFlag(event.data.window.state, core::WindowState::Pointer);
			stateChanged = true;
		}
		break;
	case InputEventName::MouseMove:
		if (auto tar = _listener->getOwner()) {
			auto v = tar->isTouched(event.currentLocation, _info.padding);
			if (_hasMouseOver != v) {
				_hasMouseOver = v;
				stateChanged = true;
				if (v) {
					ret = InputEventState::Retain;
				} else {
					ret = InputEventState::Release;
				}
			} else if (_hasMouseOver) {
				stateChanged = true;
			}
		} else {
			if (_hasMouseOver) {
				stateChanged = true;
				_hasMouseOver = false;
			}
		}
		break;
	default: break;
	}
	if (stateChanged) {
		updateState(event);
	}
	return ret;
}

void GestureMouseOverRecognizer::onEnter(InputListener *l) {
	GestureRecognizer::onEnter(l);
	_listener = l;

	auto dispatcher = l->getOwner()->getDirector()->getInputDispatcher();

	_viewHasPointer = hasFlag(dispatcher->getWindowState(), WindowState::Pointer);
	_viewHasFocus = hasFlag(dispatcher->getWindowState(), WindowState::Focused);
}

void GestureMouseOverRecognizer::onExit() {
	_listener = nullptr;
	GestureRecognizer::onExit();
}

void GestureMouseOverRecognizer::updateState(const InputEvent &event) {
	if (_listener) {
		auto dispatcher = _listener->getOwner()->getDirector()->getInputDispatcher();

		_viewHasPointer = hasFlag(dispatcher->getWindowState(), WindowState::Pointer);
		_viewHasFocus = hasFlag(dispatcher->getWindowState(), WindowState::Focused);
	}

	auto value = (!_info.onlyFocused || _viewHasFocus) && _viewHasPointer && _hasMouseOver;
	if (value != _value) {
		_value = value;
		_event.input = &event;
		_event.event = _value ? GestureEvent::Began : GestureEvent::Ended;
		_callback(_event);
	} else if (_value && event.data.event == InputEventName::MouseMove) {
		_event.input = &event;
		_event.event = GestureEvent::Moved;
		_callback(_event);
	}
}

std::ostream &operator<<(std::ostream &stream, GestureEvent ev) {
	switch (ev) {
	case GestureEvent::Began: stream << "GestureEvent::Began"; break;
	case GestureEvent::Activated: stream << "GestureEvent::Activated"; break;
	case GestureEvent::Ended: stream << "GestureEvent::Ended"; break;
	case GestureEvent::Cancelled: stream << "GestureEvent::Cancelled"; break;
	}
	return stream;
}

} // namespace stappler::xenolith
