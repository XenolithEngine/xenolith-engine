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

#ifndef CORE_EVENT_SPEVENTBUS_H_
#define CORE_EVENT_SPEVENTBUS_H_

#include "SPEventLooper.h"
#include "SPMemory.h"

namespace stappler::event {

class Bus;

using BusEventCategory = ValueWrapper<uint32_t, class BusEventCategoryFlag>;

class SP_PUBLIC BusEvent : public Ref {
public:
	virtual ~BusEvent() = default;

	BusEvent(BusEventCategory category);

	BusEventCategory getCategory() const { return _category; }

protected:
	BusEventCategory _category = BusEventCategory::zero();
};

class SP_PUBLIC BusDelegate : public Ref {
public:
	using BusEventCallback = mem_std::Function<void(Bus &, const BusEvent &, BusDelegate &)>;

	virtual ~BusDelegate();

	bool init(NotNull<Looper>, SpanView<BusEventCategory>, NotNull<Ref>, BusEventCallback &&);
	bool init(NotNull<Looper>, BusEventCategory, NotNull<Ref>, BusEventCallback &&);

	// should be called when owner is disabled
	void invalidate();

	void handleEvent(Bus &, const BusEvent &);

	Looper *getLooper() const { return _looper; }
	SpanView<BusEventCategory> getCategories() const { return _categories; }

	Ref *getOwner() const { return _owner; }
	Bus *getBus() const { return _bus; }

protected:
	friend class Bus;

	void handleAdded(Bus *);
	void handleRemoved(Bus *);

	void finalize();

	enum State {
		Pending,
		Active,
		Invalidated,
		Finalized
	};

	State _state = Pending;

	// where event will be processed
	// когда делегат добавляется к шине, шина должна сообщить об этом луперу
	// если лупер с живими делегатами завершается - он должен сообщить об этом шине
	Looper *_looper = nullptr;

	mem_std::Vector<BusEventCategory> _categories;

	BusEventCallback _callback;

	Rc<Ref> _owner;
	Rc<Bus> _bus;
};

class SP_PUBLIC Bus : public Ref {
public:
	virtual ~Bus();

	BusEventCategory allocateCategory(StringView);

	StringView getCategoryName(BusEventCategory) const;

	void addListener(NotNull<BusDelegate>);
	void removeListener(NotNull<BusDelegate>);

	void dispatchEvent(NotNull<BusEvent>);

	void invalidateLooper(Looper *);

protected:
	void doAddListener(BusDelegate *, std::unique_lock<std::mutex> &);
	void doRemoveListener(BusDelegate *, std::unique_lock<std::mutex> &);

	mutable std::mutex _mutex;
	mem_std::Vector<mem_std::String> _categories;
	mem_std::Set<Rc<BusDelegate>> _listeners;
	mem_std::Map<BusEventCategory, mem_std::HashSet<BusDelegate *>> _listenersByCategories;
	mem_std::Map<Looper *, mem_std::HashSet<BusDelegate *>> _loopers;
};

}; // namespace stappler::event

#endif /* CORE_EVENT_SPEVENTBUS_H_ */
