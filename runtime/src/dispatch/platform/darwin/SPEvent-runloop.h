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

#include <sprt/runtime/dispatch/queue.h>
#include <sprt/runtime/dispatch/handle.h>
#include "../../detail/SPRuntimeDispatchQueueData.h"

#include <sys/darwin.h>

namespace sprt::dispatch {

static constexpr bool RUNLOOP_THREAD_NONBLOCK = false;

struct RunLoopData;

struct SPRT_API RunLoopTimerSource {
	_CFRunLoopTimerRef timer = nullptr;
	TimeInterval timeout;
	TimeInterval interval;
	uint32_t count = 0;
	uint32_t value = 0;

	bool init(const TimerInfo &info);
	void cancel();

	double getNextInterval() const;
};

class SPRT_API RunLoopTimerHandle : public TimerHandle {
public:
	virtual ~RunLoopTimerHandle() = default;

	bool init(HandleClass *, TimerInfo &&);

	Status rearm(RunLoopData *, RunLoopTimerSource *);
	Status disarm(RunLoopData *, RunLoopTimerSource *);

	void notify(RunLoopData *, RunLoopTimerSource *source, const NotifyData &);

	virtual bool reset(TimerInfo &&) override;
};

struct SPRT_API RunLoopData : public PlatformQueueData {
	_CFRunLoopRef _runLoop = nullptr;
	_CFStringRef _runMode = nullptr;

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

// Timer-driven child-process handle. The RunLoop backend has no pollable-fd or
// process-exit primitive (CFFileDescriptor/CFSocket/EVFILT_PROC are unavailable
// here), so — exactly like the inline file handle — a repeating reactor timer
// drives the work: each fire drains the merged stdout/stderr pipe into the user
// reader and waitpid(WNOHANG)s the child. The handle completes once, with the
// exit code in the completion value, when the child is reaped. The shared
// ProcessState (reader / read fd) is held as the handle's userdata.
class SPRT_API RunLoopProcessHandle : public ProcessHandle {
public:
	virtual ~RunLoopProcessHandle() = default;

	bool init(HandleClass *, int pid, CompletionHandle<ProcessHandle> &&);

	// schedule the driver timer (called from runFn)
	void start();

	// one drain + reap step (driver-timer fire)
	void poll();

	// teardown: stop the driver timer and close the pipe (cancelFn)
	void terminate();

	virtual NativeHandle getNativeHandle() const override;

protected:
	// defer the completion out of the driver-timer callback and fire it
	void finish();

	int _pid = -1;
	bool _finishing = false; // exit detected (or teardown begun): stop polling
	bool _reaped = false; // child reaped via the exit path; terminate() must not kill a recycled pid
	Rc<Handle> _driver; // repeating poll timer
};

// HandleClass setup for RunLoopProcessHandle. Custom (no reactor rearm/notify):
// runFn starts the driver timer, cancelFn tears it down; suspend/resume are plain
// bookkeeping (the driver timer is itself suspended/resumed by the queue).
void setupRunLoopProcessHandleClass(QueueHandleClassInfo *info, HandleClass *cl);

// Full spawn for the RunLoop backend: posix child + timer-driven reader/reaper.
Rc<ProcessHandle> spawnProcessRunLoop(QueueData *data, HandleClass *processClass, ProcessInfo &&info,
		Ref *ref);

struct SPRT_API RunLoopThreadSource {
	bool init();
	void cancel();
};

class SPRT_API RunLoopThreadHandle : public ThreadHandle {
public:
	virtual ~RunLoopThreadHandle() = default;

	bool init(HandleClass *);

	Status rearm(RunLoopData *, RunLoopThreadSource *);
	Status disarm(RunLoopData *, RunLoopThreadSource *);

	void notify(RunLoopData *, RunLoopThreadSource *, const NotifyData &);

	virtual Status perform(Rc<Task> &&task) override;
	virtual Status perform(dispatch::Function<void()> &&func, Ref *target, StringView tag) override;

protected:
	sprt::mutex _mutex;
};

} // namespace sprt::dispatch

#endif /* CORE_EVENT_PLATFORM_DARWIN_SPEVENT_RUNLOOP_H_ */
