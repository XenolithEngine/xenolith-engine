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

#ifndef XENOLITH_APPLICATION_NODES_XLEVENTLISTENER_H_
#define XENOLITH_APPLICATION_NODES_XLEVENTLISTENER_H_

#include "SPEventBus.h"
#include "XLEvent.h"
#include "XLSystem.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

using EventCallback = Function<void(const Event &)>;

class SP_PUBLIC EventDelegate : public event::BusDelegate {
public:
	virtual ~EventDelegate() = default;

	bool init(Ref *, const EventHeader &h, BusEventCallback &&);
	bool init(Ref *, SpanView<EventId>, BusEventCallback &&);
	bool init(Ref *, Vector<EventId> &&, BusEventCallback &&);

	void enable(event::Looper *);
	void disable();
};

class SP_PUBLIC EventListener : public System {
public:
	virtual ~EventListener();

	virtual bool init() override;

	virtual void handleEnter(Scene *) override;
	virtual void handleExit() override;

	virtual void handleRemoved() override;

	event::BusDelegate *listenForEvent(const EventHeader &h, EventCallback &&,
			bool removeAfterEvent = false);

	event::BusDelegate *listenForEventWithObject(const EventHeader &h, Ref *obj, EventCallback &&,
			bool removeAfterEvent = false);

	void removeDelegate(event::BusDelegate *);

	void clear();

protected:
	Set<Rc<EventDelegate>> _listeners;
};

} // namespace stappler::xenolith

#endif /* XENOLITH_APPLICATION_NODES_XLEVENTLISTENER_H_ */
