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

#ifndef CORE_EVENT_SPEVENTLOOP_H_
#define CORE_EVENT_SPEVENTLOOP_H_

#include "SPEventQueue.h"
#include "SPThreadPool.h"

namespace STAPPLER_VERSIONIZED stappler::event {

class Bus;

struct SP_PUBLIC LooperInfo {
	StringView name = StringView("Main");
	uint16_t workersCount =
			uint16_t(std::thread::hardware_concurrency()); // 0 if no workers required
	thread::ThreadPoolFlags workersFlags = thread::ThreadPoolFlags::LazyInit;
	QueueEngine engineMask = QueueEngine::Any;
};

/* Looper - common event processing primitive,
 * that uses OS-dependent implementation for threaded tasks and timers
 *
 * Only one Looper per thread is allowed
 */
class SP_PUBLIC Looper final : public Ref {
public:
	// acquire looper for a current thread
	// LooperInfo will be assigned to Looper only on first call
	static Looper *acquire(LooperInfo && = LooperInfo());

	static Looper *acquire(LooperInfo &&, QueueInfo &&);

	static Looper *getIfExists();

	virtual ~Looper();

	Rc<TimerHandle> scheduleTimer(TimerInfo &&, Ref * = nullptr);

	// Schedule function on timeout
	// Please, do not try to reset this timer, just schedule another one
	Rc<Handle> schedule(TimeInterval, mem_std::Function<void(Handle *, bool success)> &&,
			Ref * = nullptr);

	// Value in completion is PollFlags
	// Uses Handle userdata slot for the Ref
	Rc<PollHandle> listenPollableHandle(NativeHandle, PollFlags, CompletionHandle<PollHandle> &&,
			Ref * = nullptr);

	// Uses Handle userdata slot for a private data
	Rc<PollHandle> listenPollableHandle(NativeHandle, PollFlags,
			mem_std::Function<Status(NativeHandle, PollFlags)> &&, Ref * = nullptr);

	// Perform task on this thread (only Complete callback will be executed)
	// If current thread is looper thread - performs in place
	Status performOnThread(Rc<thread::Task> &&task, bool immediate = false);

	// Perform function on this thread
	// If current thread is looper thread - performs in place
	Status performOnThread(mem_std::Function<void()> &&func, Ref *target, bool immediate = false,
			StringView tag = SP_FUNC);

	// Perform task in workers pool (if there is one)
	Status performAsync(Rc<thread::Task> &&task, bool first = false);

	// Perform function in workers pool (if there is one)
	Status performAsync(mem_std::Function<void()> &&, Ref * = nullptr, bool first = false,
			StringView tag = SP_FUNC);

	// Perform Handle in queue (if supported)
	Status performHandle(Handle *);

	// non-blocking poll
	uint32_t poll();

	// wait until next event, or timeout
	uint32_t wait(TimeInterval = TimeInterval());

	// Run for some time or infinite
	// QueueWakeupFlags can be defined for a wakeup on timer
	//
	// Done when timeout expired
	// Ok on graceful wakeup
	// Suspended on forced wakeup
	// ErrorCancelled when graceful wakeup failed on timeout
	//
	// You can set QueueWakeupInfo for timeout wakeup mode
	Status run(TimeInterval = TimeInterval::Infinite, QueueWakeupInfo && = QueueWakeupInfo());

	// wakeup looper from `run`
	// returns ErrorNotImplemented if requested parameters is not supported
	Status wakeup(WakeupFlags = WakeupFlags::ContextDefault);

	uint16_t getWorkersCount() const;

	memory::pool_t *getThreadMemPool() const;

	const event::Queue *getQueue() const;

	thread::ThreadPool *getThreadPool() const;

	bool isOnThisThread() const;

	void attachBus(Bus *);
	void detachBus(Bus *);

protected:
	friend class Rc<Looper>; // for Rc<>::alloc

	struct Data;

	Looper(LooperInfo &&, Rc<QueueRef> &&);

	std::mutex _mutex;
	std::atomic<bool> _active;
	Data *_data = nullptr;
};

} // namespace stappler::event

#endif /* CORE_EVENT_SPEVENTLOOP_H_ */
