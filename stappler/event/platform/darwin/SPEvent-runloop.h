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

#ifndef CORE_EVENT_PLATFORM_DARWIN_SPEVENT_RUNLOOP_H_
#define CORE_EVENT_PLATFORM_DARWIN_SPEVENT_RUNLOOP_H_

#include "SPEventQueue.h"
#include "SPPlatformUnistd.h"
#include "SPEventTimerHandle.h"
#include "SPEventThreadHandle.h"
#include "detail/SPEventQueueData.h"

#include "CoreFoundation/CFRunLoop.h"

namespace STAPPLER_VERSIONIZED stappler::event {

static constexpr bool RUNLOOP_THREAD_NONBLOCK = false;

struct RunLoopData;

struct SP_PUBLIC RunLoopTimerSource {
	CFRunLoopTimerRef timer = nullptr;
	TimeInterval timeout;
	TimeInterval interval;
	uint32_t count = 0;
	uint32_t value = 0;

	bool init(const TimerInfo &info);
	void cancel();

	double getNextInterval() const;
};

class SP_PUBLIC RunLoopTimerHandle : public TimerHandle {
public:
	virtual ~RunLoopTimerHandle() = default;

	bool init(HandleClass *, TimerInfo &&);

	Status rearm(RunLoopData *, RunLoopTimerSource *);
	Status disarm(RunLoopData *, RunLoopTimerSource *);

	void notify(RunLoopData *, RunLoopTimerSource *source, const NotifyData &);

	virtual bool reset(TimerInfo &&) override;
};

struct SP_PUBLIC RunLoopData : public PlatformQueueData {
	CFRunLoopRef _runLoop = nullptr;
	CFStringRef _runMode = nullptr;

	void addTimer(RunLoopTimerHandle *handle, RunLoopTimerSource *);
	void removeTimer(RunLoopTimerHandle *handle, RunLoopTimerSource *);

	void trigger(Handle *handle, NotifyData notifyData);

	uint32_t enter(RunContext *ctx, TimeInterval ival);

	Status submit();
	uint32_t poll();
	uint32_t wait(TimeInterval);
	Status run(TimeInterval, WakeupFlags, TimeInterval wakeupTimeout);

	Status wakeup(WakeupFlags);

	void cancel();

	RunLoopData(QueueRef *, Queue::Data *data, const QueueInfo &info);
	~RunLoopData();
};

struct SP_PUBLIC RunLoopThreadSource {
	bool init();
	void cancel();
};

class SP_PUBLIC RunLoopThreadHandle : public ThreadHandle {
public:
	virtual ~RunLoopThreadHandle() = default;

	bool init(HandleClass *);

	Status rearm(RunLoopData *, RunLoopThreadSource *);
	Status disarm(RunLoopData *, RunLoopThreadSource *);

	void notify(RunLoopData *, RunLoopThreadSource *, const NotifyData &);

	virtual Status perform(Rc<thread::Task> &&task) override;
	virtual Status perform(mem_std::Function<void()> &&func, Ref *target, StringView tag) override;

protected:
	std::mutex _mutex;
};

} // namespace stappler::event

#endif /* CORE_EVENT_PLATFORM_DARWIN_SPEVENT_RUNLOOP_H_ */
