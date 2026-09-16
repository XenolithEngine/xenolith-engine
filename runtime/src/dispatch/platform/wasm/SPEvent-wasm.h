/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

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

#ifndef CORE_EVENT_PLATFORM_WASM_SPEVENT_WASM_H_
#define CORE_EVENT_PLATFORM_WASM_SPEVENT_WASM_H_

#include <sprt/runtime/dispatch/queue.h>

#include "../../detail/SPRuntimeDispatchHandleClass.h"
#include "../../detail/SPRuntimeDispatchQueueData.h"

namespace sprt::dispatch {

class WasmThreadHandle;

// One armed timer, held in the reactor's deadline list. `handle` is a raw
// pointer: an armed handle is retained by the queue (HandleClass::run) and is
// always removed from this list on disarm/cancel before it can be released, so
// the pointer never dangles.
struct WasmTimerEntry {
	int64_t deadline = 0; // absolute monotonic ns of the next fire
	Handle *handle = nullptr;
};

// Backend for wasm: a pure userspace reactor with no OS fds. Blocking rides on
// a single futex word (memory.atomic.wait32) whose relative timeout doubles as
// the timer mechanism (it sleeps until the nearest deadline). Cross-thread posts
// and Queue::wakeup break the wait by bumping the word and memory.atomic.notify.
// See wasm-dispatch-design memory / wasm-port-draft.adoc.
struct SPRT_API WasmData : public PlatformQueueData {
	// Futex generation counter (the "wakeword"). Lives in the shared linear
	// memory, so every worker thread sees it. Bumped + notified to break wait32.
	alignas(4) int32_t _wakeword = 0;

	// A pending Queue::wakeup()/cancel() request from any thread. 0 = none; the
	// present/cancel markers let None (=0 flags) be distinguished from "no req".
	alignas(4) int32_t _wakeupReq = 0;

	// Active timers, scanned for the nearest deadline each loop iteration.
	Queue::Vector<WasmTimerEntry> _timers;

	// Registered cross-thread inboxes (ThreadHandles). Only the loop thread
	// touches this list (register/unregister/fire all run on it).
	Queue::Vector<WasmThreadHandle *> _threadHandles;

	static constexpr int32_t WakeupPresent = int32_t(1) << 30;
	static constexpr int32_t WakeupCancel = int32_t(1) << 29;

	WasmData(QueueRef *, Queue::Data *data, const QueueInfo &info);
	~WasmData();

	// Timer list management (called on the loop thread from the timer handle).
	void pushTimer(int64_t deadline, Handle *);
	void removeTimer(Handle *);
	int64_t nearestDeadline() const;

	// Fire every timer whose deadline has passed, dispatching each through
	// QueueData::notify (so completions + queued tasks run). Returns the count.
	uint32_t fireExpired(RunContext *);

	// ThreadHandle registry (loop thread only).
	void registerThreadHandle(WasmThreadHandle *);
	void unregisterThreadHandle(WasmThreadHandle *);
	// Dispatch every ThreadHandle that has pending cross-thread work. Returns count.
	uint32_t fireThreadHandles(RunContext *);

	// Futex: bump the generation and wake any waiter (loop thread / another
	// worker blocked in wait32).
	void bumpAndNotify();

	// Apply a pending wakeup/cancel request on the loop thread.
	void drainWakeup();

	Status submit();
	uint32_t poll();
	uint32_t wait(TimeInterval);
	Status run(TimeInterval, WakeupFlags, TimeInterval wakeupTimeout);
	Status wakeup(WakeupFlags);
	void cancel();
};

// POD schedule state placement-constructed into Handle::_data.
struct WasmTimerSource {
	int64_t firstTimeout = 0; // ns: delay to the first fire
	int64_t interval = 0; // ns: period between fires (0 => one-shot)
	int64_t deadline = 0; // ns: absolute deadline of the next fire (set at arm)
	uint32_t count = 0; // target number of fires (TimerHandle::Infinite)
	uint32_t value = 0; // fires so far

	void cancel() { }
};

class WasmTimerHandle : public TimerHandle {
public:
	bool init(HandleClass *, TimerInfo &&);
	virtual bool reset(TimerInfo &&) override;

	// Arm: compute the deadline and push into the reactor list.
	Status rearm(WasmData *, WasmTimerSource *);
	// Disarm: remove from the reactor list.
	Status disarm(WasmData *, WasmTimerSource *);
	// One fire batch delivered by the reactor.
	void notify(WasmData *, WasmTimerSource *, const NotifyData &);
};

// POD state for a ThreadHandle. `pending` is set (from any thread) when work is
// posted and cleared (on the loop thread) when the reactor drains it. It is the
// fd-less replacement for the eventfd readiness the other backends poll.
struct WasmThreadSource {
	int32_t pending = 0;

	void cancel() { }
};

// Cross-thread inbox. perform() (called from another worker or the JS main
// thread's proxy) enqueues into the base ThreadHandle's output queue, flags the
// source pending, and wakes the reactor by bumping its wakeword — the fd-less
// eventfd-write equivalent. The reactor dispatches it via notify() on the loop
// thread, which drains the queue into the handle's PerformEngine.
class WasmThreadHandle : public ThreadHandle {
public:
	virtual ~WasmThreadHandle() = default;

	bool init(HandleClass *);

	Status rearm(WasmData *, WasmThreadSource *);
	Status disarm(WasmData *, WasmThreadSource *);
	void notify(WasmData *, WasmThreadSource *, const NotifyData &);

	virtual Status perform(Rc<Task> &&task) override;
	virtual Status perform(dispatch::Function<void()> &&func, Ref *target, StringView tag) override;

	// Atomically consume the pending flag (loop thread).
	bool takePending();

protected:
	// Signal the reactor from any thread: flag pending + bump the wakeword.
	void signal();

	sprt::mutex _mutex;
	WasmData *_wasm = nullptr; // cached reactor for cross-thread wakeups
};

struct SPRT_API Queue::Data : public QueueData {
	HandleClass _wasmTimerClass;
	HandleClass _wasmThreadClass;
	HandleClass _wasmFileInlineClass;
	HandleClass _wasmWatchClass;

	Data(QueueRef *q, const QueueInfo &info);
};

// Wire a HandleClass so create/run/suspend/resume/cancel/notify route through the
// engine handle's arm/disarm/notify (mirrors setupEpollHandleClass).
template <typename HandleType, typename SourceType>
void setupWasmHandleClass(QueueHandleClassInfo *info, HandleClass *cl, bool suspendable) {
	cl->info = info;

	cl->createFn = [](HandleClass *cl, Handle *handle, uint8_t data[Handle::DataSize]) {
		static_assert(sizeof(SourceType) <= Handle::DataSize
				&& sprt::is_standard_layout<SourceType>::value);
		new (data) SourceType;
		return HandleClass::create(cl, handle, data);
	};
	cl->destroyFn = HandleClass::destroy;

	cl->runFn = [](HandleClass *cl, Handle *handle, uint8_t data[Handle::DataSize]) {
		auto platformData = static_cast<Queue::Data *>(cl->info->data);
		auto source = reinterpret_cast<SourceType *>(data);

		auto status = static_cast<HandleType *>(handle)->rearm(
				reinterpret_cast<WasmData *>(platformData->_platformQueue), source);
		if (status == Status::Ok || status == Status::Done) {
			return HandleClass::run(cl, handle, data);
		}
		return status;
	};

	cl->cancelFn = [](HandleClass *cl, Handle *handle, uint8_t data[Handle::DataSize], Status st) {
		auto source = reinterpret_cast<SourceType *>(data);

		source->cancel();
		source->~SourceType();

		return HandleClass::cancel(cl, handle, data, st);
	};

	if (suspendable) {
		cl->suspendFn = [](HandleClass *cl, Handle *handle, uint8_t data[Handle::DataSize]) {
			auto platformData = static_cast<Queue::Data *>(cl->info->data);
			auto source = reinterpret_cast<SourceType *>(data);

			auto status = static_cast<HandleType *>(handle)->disarm(
					reinterpret_cast<WasmData *>(platformData->_platformQueue), source);
			if (status == Status::Ok || status == Status::Done) {
				return HandleClass::suspend(cl, handle, data);
			}
			return status;
		};

		cl->resumeFn = [](HandleClass *cl, Handle *handle, uint8_t data[Handle::DataSize]) {
			auto platformData = static_cast<Queue::Data *>(cl->info->data);
			auto source = reinterpret_cast<SourceType *>(data);

			auto status = HandleClass::resume(cl, handle, data);
			if (status == Status::Ok || status == Status::Done) {
				status = static_cast<HandleType *>(handle)->rearm(
						reinterpret_cast<WasmData *>(platformData->_platformQueue), source);
			}
			return status;
		};
	}

	cl->notifyFn = [](HandleClass *cl, Handle *handle, uint8_t data[Handle::DataSize],
						   const NotifyData &n) {
		auto platformData = static_cast<Queue::Data *>(cl->info->data);
		auto source = reinterpret_cast<SourceType *>(data);

		static_cast<HandleType *>(handle)->notify(
				reinterpret_cast<WasmData *>(platformData->_platformQueue), source, n);
	};
}

} // namespace sprt::dispatch

#endif /* CORE_EVENT_PLATFORM_WASM_SPEVENT_WASM_H_ */
