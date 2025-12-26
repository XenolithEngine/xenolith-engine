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

#include "XLEvent.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

struct EventBus {
	static void initialize(void *ptr) { reinterpret_cast<EventBus *>(ptr)->init(); }
	static void terminate(void *ptr) { reinterpret_cast<EventBus *>(ptr)->term(); }

	EventBus() {
		addInitializer(this, initialize, terminate);

		bus = Rc<event::Bus>::alloc();
	}

	void init() { }
	void term() { bus = nullptr; }

	event::BusEventCategory allocateCategory(StringView name) {
		sprt_passert(bus, "Bus should be initialized");
		return bus->allocateCategory(name);
	}

	void dispatchEvent(NotNull<event::BusEvent> ev) {
		sprt_passert(bus, "Bus should be initialized");
		bus->dispatchEvent(ev);
	}

	StringView getCategoryName(event::BusEventCategory id) {
		sprt_passert(bus, "Bus should be initialized");
		return bus->getCategoryName(id);
	}

	Rc<event::Bus> bus;
};

static EventBus *getEventBus() {
	static EventBus s_eventBus;
	return &s_eventBus;
}


static void EventHeader_send(const EventHeader &header, Ref *object, Value &&dataVal = Value(),
		Ref *objVal = nullptr) {
	auto ev = Rc<Event>::alloc(header, object, move(dataVal), objVal);
	getEventBus()->dispatchEvent(ev.get());
}

EventHeader::EventHeader(StringView name) : _category(getEventBus()->allocateCategory(name)) {
	sprt_passert(!name.empty(), "Event should have name");
}

EventHeader::~EventHeader() { }

EventId EventHeader::getEventId() const { return _category; }

StringView EventHeader::getName() const { return getEventBus()->getCategoryName(_category); }

EventHeader::operator EventId() const { return _category; }

bool EventHeader::operator==(const Event &event) const { return event.getCategory() == _category; }

void EventHeader::send(Ref *object, int64_t value) const {
	EventHeader_send(*this, object, Value(value));
}
void EventHeader::send(Ref *object, uint64_t value) const {
	EventHeader_send(*this, object, Value(value));
}
void EventHeader::send(Ref *object, int32_t value) const {
	EventHeader_send(*this, object, Value(value));
}
void EventHeader::send(Ref *object, uint32_t value) const {
	EventHeader_send(*this, object, Value(value));
}
void EventHeader::send(Ref *object, double value) const {
	EventHeader_send(*this, object, Value(value));
}
void EventHeader::send(Ref *object, bool value) const {
	EventHeader_send(*this, object, Value(value));
}
void EventHeader::send(Ref *object, Ref *value) const {
	EventHeader_send(*this, object, Value(0), value);
}
void EventHeader::send(Ref *object, const char *value) const {
	EventHeader_send(*this, object, Value(value));
}
void EventHeader::send(Ref *object, const String &value) const {
	EventHeader_send(*this, object, Value(value));
}
void EventHeader::send(Ref *object, const StringView &value) const {
	EventHeader_send(*this, object, Value(value));
}
void EventHeader::send(Ref *object, const BytesView &value) const {
	EventHeader_send(*this, object, Value(value));
}
void EventHeader::send(Ref *object, Value &&value) const {
	EventHeader_send(*this, object, Value(value));
}
void EventHeader::send(Ref *object) const { EventHeader_send(*this, object); }

event::Bus *Event::getBus() { return getEventBus()->bus; }

EventId Event::getEventId() const { return getCategory(); }

bool Event::is(const EventHeader &eventHeader) const {
	return getEventId() == eventHeader.getEventId();
}

bool Event::operator==(const EventHeader &eventHeader) const { return is(eventHeader); }

Event::Event(const EventHeader &header, Ref *object)
: BusEvent(header.getEventId()), _object(object) { }

Event::Event(const EventHeader &header, Ref *object, Value &&dataVal, Ref *objVal)
: BusEvent(header.getEventId())
, _object(object)
, _dataValue(move(dataVal))
, _objectValue(objVal) { }

} // namespace stappler::xenolith
