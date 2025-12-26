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

#include "XLEventListener.h"
#include "SPEventBus.h"
#include "XLAppThread.h"
#include "XLScene.h"
#include "XLDirector.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

bool EventDelegate::init(Ref *owner, const EventHeader &ev, BusEventCallback &&cb) {
	_owner = owner;
	_categories.emplace_back(ev.getEventId());
	_looper = nullptr;
	_callback = sp::move(cb);
	return true;
}

bool EventDelegate::init(Ref *owner, SpanView<EventId> evs, BusEventCallback &&cb) {
	_owner = owner;
	_categories = evs.vec<Interface>();
	_looper = nullptr;
	_callback = sp::move(cb);
	return true;
}

bool EventDelegate::init(Ref *owner, Vector<EventId> &&evs, BusEventCallback &&cb) {
	_owner = owner;
	_categories = sp::move(evs);
	_looper = nullptr;
	_callback = sp::move(cb);
	return true;
}

void EventDelegate::enable(event::Looper *looper) {
	_looper = looper;
	Event::getBus()->addListener(this);
}

void EventDelegate::disable() {
	Event::getBus()->removeListener(this);
	_looper = nullptr;
}

EventListener::~EventListener() { }

bool EventListener::init() {
	if (!System::init()) {
		return false;
	}

	_systemFlags = SystemFlags::HandleOwnerEvents | SystemFlags::HandleSceneEvents;

	return true;
}

void EventListener::handleEnter(Scene *scene) {
	System::handleEnter(scene);

	for (auto &it : _listeners) { it->enable(scene->getDirector()->getApplication()->getLooper()); }
}

void EventListener::handleExit() {
	for (auto &it : _listeners) { it->disable(); }
	System::handleExit();
}

void EventListener::handleRemoved() {
	clear();
	System::handleRemoved();
}

event::BusDelegate *EventListener::listenForEvent(const EventHeader &h, EventCallback &&callback,
		bool removeAfterEvent) {
	auto d = Rc<EventDelegate>::create(this, h,
			[this, callback = sp::move(callback), removeAfterEvent](event::Bus &bus,
					const event::BusEvent &event, event::BusDelegate &d) {
		if (_enabled && _owner && _running) {
			auto refId = retain();
			callback(static_cast<const Event &>(event));
			if (removeAfterEvent) {
				_listeners.erase(static_cast<EventDelegate *>(&d));
				bus.removeListener(&d);
				d.invalidate();
			}
			release(refId);
		}
	});
	_listeners.emplace(d);

	if (_running && _owner) {
		d->enable(_owner->getDirector()->getApplication()->getLooper());
	}

	return d;
}

event::BusDelegate *EventListener::listenForEventWithObject(const EventHeader &h, Ref *obj,
		EventCallback &&callback, bool removeAfterEvent) {
	auto d = Rc<EventDelegate>::create(this, h,
			[this, callback = sp::move(callback), removeAfterEvent, obj](event::Bus &bus,
					const event::BusEvent &event, event::BusDelegate &d) {
		if (_enabled && _owner && callback) {
			auto &ev = static_cast<const Event &>(event);
			if (ev.getObject() == obj) {
				callback(ev);
				if (removeAfterEvent) {
					bus.removeListener(&d);
					_listeners.erase(static_cast<EventDelegate *>(&d));
					d.invalidate();
				}
			}
		}
	});
	_listeners.emplace(d);

	if (_running && _owner) {
		d->enable(_owner->getDirector()->getApplication()->getLooper());
	}

	return d;
}

void EventListener::removeDelegate(event::BusDelegate *d) {
	auto it = _listeners.find(d);
	if (it != _listeners.end()) {
		Rc<EventDelegate> l = (*it);

		_listeners.erase(it);

		if (l->getLooper()) {
			l->invalidate();
		}
		if (l->getBus()) {
			l->disable();
		}
	}
}

void EventListener::clear() {
	auto l = sp::move(_listeners);
	_listeners.clear();

	for (auto &it : l) {
		if (it->getLooper()) {
			it->invalidate();
		}
		if (it->getBus()) {
			it->disable();
		}
	}

	l.clear();
}

} // namespace stappler::xenolith
