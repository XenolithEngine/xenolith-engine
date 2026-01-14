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

#ifndef CORE_EVENT_DETAIL_SPEVENTQUEUEDATA_H_
#define CORE_EVENT_DETAIL_SPEVENTQUEUEDATA_H_

#include "SPEventQueue.h"
#include "SPEventHandleClass.h"
#include "SPTime.h"

struct _linux_timespec {
	int64_t tv_sec; /* seconds */
	int64_t tv_nsec; /* nanoseconds */
};

struct _linux_itimerspec {
	_linux_timespec it_interval; /* timer period */
	_linux_timespec it_value; /* timer expiration */
};

namespace STAPPLER_VERSIONIZED stappler::event {

struct PlatformQueueData;

// PerformEngine can be used for resumable nested 'perform' variants
// Action, that performed within engine, can safely call Queue::run, that also can cause 'perform'
struct SP_PUBLIC PerformEngine : public mem_pool::AllocBase {
	struct Block : AllocPool {
		Block *next = nullptr;
		Rc<thread::Task> task;
		mem_std::Function<void()> fn;
		Rc<Ref> ref;
		StringView tag;
	};

	uint32_t _performEnabled = 0;
	memory::pool_t *_pool = nullptr;
	memory::pool_t *_tmpPool = nullptr;
	Block *_pendingBlocksFront = nullptr;
	Block *_pendingBlocksTail = nullptr;
	Block *_emptyBlocks = nullptr;
	uint32_t _blocksAllocated = 0;
	uint32_t _blocksWaiting = 0;
	uint32_t _blocksFree = 0;

	Status perform(Rc<thread::Task> &&);
	Status perform(mem_std::Function<void()> &&, Ref * = nullptr, StringView tag = StringView());

	uint32_t runAllTasks(memory::pool_t *);

	void cleanup();

	PerformEngine(memory::pool_t *);
};

struct SP_PUBLIC QueueData : public PerformEngine {
	using SubmitCallback = Status (*)(void *);
	using PollCallback = uint32_t (*)(void *);
	using WaitCallback = uint32_t (*)(void *, TimeInterval ival);
	using RunCallback = Status (*)(void *, TimeInterval ival, QueueWakeupInfo &&info);
	using WakeupCallback = Status (*)(void *, WakeupFlags);
	using CancelCallback = void (*)(void *);
	using DestroyCallback = void (*)(void *);

	using TimerCallback = Rc<TimerHandle> (*)(QueueData *, void *, TimerInfo &&info);
	using ThreadCallback = Rc<ThreadHandle> (*)(QueueData *, void *);
	using ListenHandleCallback = Rc<PollHandle> (*)(QueueData *, void *, NativeHandle, PollFlags,
			CompletionHandle<PollHandle> &&);

	QueueHandleClassInfo _info;
	QueueFlags _flags = QueueFlags::None;
	QueueEngine _engine = QueueEngine::None;

	bool _running = true;

	mem_pool::Set<Rc<Handle>> _pendingHandles;
	mem_pool::Set<Rc<Handle>> _suspendableHandles;

	PlatformQueueData *_platformQueue = nullptr;

	SubmitCallback _submit = nullptr;
	PollCallback _poll = nullptr;
	WaitCallback _wait = nullptr;
	RunCallback _run = nullptr;
	WakeupCallback _wakeup = nullptr;
	CancelCallback _cancel = nullptr;
	DestroyCallback _destroy = nullptr;
	TimerCallback _timer = nullptr;
	ThreadCallback _thread = nullptr;
	ListenHandleCallback _listenHandle = nullptr;

	thread::Thread::Id _threadId;

	NativeHandle _handle = NativeHandle(0);

	bool isValid() const { return _platformQueue != nullptr; }

	bool isRunning() const { return _running; }
	bool isWithinNotify() const { return _performEnabled > 0; }

	// returns number of operations suspended
	uint32_t suspendAll();

	// returns number of operations suspended
	uint32_t resumeAll();

	Status runHandle(Handle *);

	void cancel(Handle *);

	void cleanup();

	void notify(Handle *, const NotifyData &);

	void notifySuspendedAll();

	Status submit();

	uint32_t poll();
	uint32_t wait(TimeInterval ival);

	Status run(TimeInterval ival, QueueWakeupInfo &&info);
	Status wakeup(WakeupFlags flags);

	void cancel();

	Rc<TimerHandle> scheduleTimer(TimerInfo &&);
	Rc<PollHandle> listenHandle(NativeHandle, PollFlags, CompletionHandle<PollHandle> &&);
	Rc<ThreadHandle> addThreadHandle();

	~QueueData();

	QueueData(QueueRef *, QueueFlags);
};

struct SP_PUBLIC PlatformQueueData;

struct alignas(32) PlatformQueueData : public mem_pool::AllocBase {
	struct RunContext {
		enum CallMode {
			Poll,
			Wait,
			Run,
		};

		enum State {
			Running,
			Signaled, // next control function should send CFRunLoopStop
			Stopping, // context should wait until all handles will become suspended or wakeup timeout expires
			Stopped, // CFRunLoopStop was sent
		};

		CallMode mode = Poll;
		State state = Running;
		PlatformQueueData *queue = nullptr;

		WakeupFlags runWakeupFlags = WakeupFlags::None;
		uint32_t wakeupCounter = 0;
		Status wakeupStatus = Status::Suspended;
		TimeInterval wakeupTimeout;

		RunContext *prev = nullptr;
		uint32_t nevents = 0;

		_linux_timespec wakeupTimespec;
	};

	using StopContextCallback = void (*)(RunContext *);
	using SuspendCallback = Status (*)(RunContext *);
	using SuspendedCallback = void (*)(RunContext *);

	QueueRef *_queue = nullptr;
	Queue::Data *_data = nullptr;
	QueueFlags _flags = QueueFlags::None;
	RunContext *_runContext = nullptr;

	StopContextCallback _stopContext = nullptr;
	SuspendCallback _suspend = nullptr;
	SuspendedCallback _suspended = nullptr;

	Status suspendHandles(RunContext *);
	Status stopContext(RunContext *, WakeupFlags, bool external);
	Status stopRootContext(WakeupFlags, bool external);

	void pushContext(RunContext *, RunContext::CallMode);
	void popContext(RunContext *);

	bool hasContext(void *);

	void handleSuspendedAll();

	PlatformQueueData(QueueRef *, Queue::Data *data, QueueFlags);
};

} // namespace stappler::event

#endif /* CORE_EVENT_DETAIL_SPEVENTQUEUEDATA_H_ */
