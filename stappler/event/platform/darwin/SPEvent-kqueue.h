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

#ifndef CORE_EVENT_PLATFORM_DARWIN_SPEVENT_KQUEUE_H_
#define CORE_EVENT_PLATFORM_DARWIN_SPEVENT_KQUEUE_H_

#include "SPEventQueue.h"
#include "SPPlatformUnistd.h"
#include "SPEventTimerHandle.h"
#include "SPEventThreadHandle.h"
#include "detail/SPEventQueueData.h"

#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>

namespace STAPPLER_VERSIONIZED stappler::event {

static constexpr bool KQUEUE_THREAD_NONBLOCK = false;

struct SP_PUBLIC KQueueData : public PlatformQueueData {
	int _kqueueFd = -1;

	mem_pool::Vector<struct kevent> _events;

	uint32_t _receivedEvents = 0;
	uint32_t _processedEvents = 0;

	Status update(const struct kevent &ev);
	Status update(SpanView<struct kevent> ev);

	Status runPoll(TimeInterval);
	uint32_t processEvents(RunContext *);

	Status submit();
	uint32_t poll();
	uint32_t wait(TimeInterval);
	Status run(TimeInterval, WakeupFlags, TimeInterval wakeupTimeout);

	Status wakeup(WakeupFlags);

	void cancel();

	KQueueData(QueueRef *, Queue::Data *data, const QueueInfo &info, SpanView<int> sigs);
	~KQueueData();
};

struct SP_PUBLIC KQueueTimerSource {
	TimeInterval timeout;
	TimeInterval interval;
	uint32_t count = 0;
	uint32_t value = 0;
	bool oneshot = false;

	bool init(const TimerInfo &info);
	void cancel();

	uint64_t getNextInterval() const;
};

class SP_PUBLIC KQueueTimerHandle : public TimerHandle {
public:
	virtual ~KQueueTimerHandle() = default;

	bool init(HandleClass *, TimerInfo &&);

	Status rearm(KQueueData *, KQueueTimerSource *);
	Status disarm(KQueueData *, KQueueTimerSource *);

	void notify(KQueueData *, KQueueTimerSource *source, const NotifyData &);

	virtual bool reset(TimerInfo &&) override;
};

struct SP_PUBLIC KQueueThreadSource {
	bool init();
	void cancel();
};

class SP_PUBLIC KQueueThreadHandle : public ThreadHandle {
public:
	virtual ~KQueueThreadHandle() = default;

	bool init(HandleClass *);

	Status rearm(KQueueData *, KQueueThreadSource *);
	Status disarm(KQueueData *, KQueueThreadSource *);

	void notify(KQueueData *, KQueueThreadSource *, const NotifyData &);

	virtual Status perform(Rc<thread::Task> &&task) override;
	virtual Status perform(mem_std::Function<void()> &&func, Ref *target, StringView tag) override;

protected:
	std::mutex _mutex;
};

} // namespace stappler::event

#endif /* CORE_EVENT_PLATFORM_DARWIN_SPEVENT_KQUEUE_H_ */
