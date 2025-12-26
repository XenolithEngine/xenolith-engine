/**
Copyright (c) 2020 Roman Katuntsev <sbkarr@stappler.org>
Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
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

#ifndef XENOLITH_APPLICATION_XLEVENT_H_
#define XENOLITH_APPLICATION_XLEVENT_H_

#include "XLCommon.h" // IWYU pragma: keep
#include "SPEventBus.h"

#define XL_DECLARE_EVENT_CLASS(class, event) \
	STAPPLER_VERSIONIZED_NAMESPACE::xenolith::EventHeader class::event(#class "." #event);

#define XL_DECLARE_EVENT(class, catName, event) \
	STAPPLER_VERSIONIZED_NAMESPACE::xenolith::EventHeader class::event(catName "." #event);

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class Event;

using EventId = event::BusEventCategory;

class SP_PUBLIC EventHeader {
public:
	EventHeader() = delete;
	EventHeader(StringView eventName);
	~EventHeader();

	EventHeader(const EventHeader &other) = default;
	EventHeader(EventHeader &&other) = default;

	EventHeader &operator=(const EventHeader &other) = default;
	EventHeader &operator=(EventHeader &&other) = default;

	EventId getEventId() const;
	StringView getName() const;

	operator EventId() const;

	bool operator==(const Event &event) const;

	template <typename T>
	inline void operator()(Ref *object, T &&value) const {
		send(object, std::forward<T>(value));
	}

	inline void operator()(Ref *object) const { send(object); }

protected:
	void send(Ref *object, int64_t value) const;
	void send(Ref *object, uint64_t value) const;
	void send(Ref *object, int32_t value) const;
	void send(Ref *object, uint32_t value) const;
	void send(Ref *object, double value) const;
	void send(Ref *object, bool value) const;
	void send(Ref *object, Ref *value) const;
	void send(Ref *object, const char *value) const;
	void send(Ref *object, const String &value) const;
	void send(Ref *object, const StringView &value) const;
	void send(Ref *object, const BytesView &value) const;
	void send(Ref *object, Value &&value) const;
	void send(Ref *object = nullptr) const;

	EventId _category = EventId::zero();
};

class SP_PUBLIC Event : public event::BusEvent {
public:
	static event::Bus *getBus();

	virtual ~Event() = default;

	Event(const EventHeader &header, Ref *object, Value &&dataVal, Ref *objVal);
	Event(const EventHeader &header, Ref *object);

	EventId getEventId() const;

	bool is(const EventHeader &eventHeader) const;
	bool operator==(const EventHeader &eventHeader) const;

	template <class T = Ref>
	inline T *getObject() const {
		static_assert(std::is_convertible<T *, Ref *>::value,
				"Invalid Type for stappler::Event target!");
		return static_cast<T *>(_object);
	}

	const Value &getDataValue() const { return _dataValue; }
	Ref *getObjectValue() const { return _objectValue; }

protected:
	Ref *_object = nullptr;
	Value _dataValue;
	Rc<Ref> _objectValue;
};

} // namespace stappler::xenolith

#endif /* XENOLITH_APPLICATION_XLEVENT_H_ */
