/**
 Copyright (c) 2025 Stappler LLC <admin@stappler.dev>
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

#ifndef CORE_EVENT_PLATFORM_SPEVENT_EPOLL_H_
#define CORE_EVENT_PLATFORM_SPEVENT_EPOLL_H_

#include "SPEventQueue.h"
#include "SPPlatformUnistd.h" // IWYU pragma: keep
#include "detail/SPEventQueueData.h"

#include "../fd/SPEventSignalFd.h"
#include "../fd/SPEventEventFd.h"

namespace STAPPLER_VERSIONIZED stappler::event {

struct SP_PUBLIC EPollData : public PlatformQueueData {
	Rc<SignalFdHandle> _signalFd;
	Rc<EventFdHandle> _eventFd;

	int _epollFd = -1;

	mem_pool::Vector<struct epoll_event> _events;

	uint32_t _receivedEvents = 0;
	uint32_t _processedEvents = 0;

	Status add(int fd, const epoll_event &ev);
	Status remove(int fd);

	Status runPoll(TimeInterval);
	uint32_t processEvents();

	Status submit();
	uint32_t poll();
	uint32_t wait(TimeInterval);
	Status run(TimeInterval, WakeupFlags, TimeInterval wakeupTimeout);

	Status wakeup(WakeupFlags);

	Status suspendHandles();
	Status doWakeupInterrupt(WakeupFlags, bool externalCall);

	void runInternalHandles();

	void cancel();

	EPollData(QueueRef *, Queue::Data *data, const QueueInfo &info, SpanView<int> sigs);
	~EPollData();
};

} // namespace stappler::event

#endif /* CORE_EVENT_PLATFORM_SPEVENT_EPOLL_H_ */
