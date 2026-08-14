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

#ifndef CORE_EVENT_PLATFORM_EMBOX_SPEVENT_EMBOX_H_
#define CORE_EVENT_PLATFORM_EMBOX_SPEVENT_EMBOX_H_

// Embox dispatch reactor — timer list + atomics + a CLOCK_MONOTONIC spin.
//
// Embox has poll() but no epoll/uring/eventfd. A self-pipe wakeup looks right
// on paper, but Embox pipes are 1 KiB (`CONFIG_DEV_PIPE_SIZE`) and both
// write() and read() block even after fcntl(O_NONBLOCK). After ~1k cross-thread
// posts the writer sleeps forever and the scene freezes. poll() from a pthread
// also ignores its timeout. So the loop never waits on a fd: it caps each idle
// stretch at 16 ms, yields, and observes `_wakeupReq` / ThreadHandle::pending.
//
// Limitations (acceptable for the M5 milestone, single-thread xenolith hello):
//   * No file/socket/process handles — those need native Embox readiness hooks
//     and arrive together with the M6 graphics window surface. The reactor
//     itself is complete enough for Looper + timers + cross-thread perform.
//   * No signals — Embox flat-build signal delivery goes through the kernel,
//     not through readable fds.

#include <sprt/runtime/dispatch/queue.h>

#include "../../detail/SPRuntimeDispatchHandleClass.h"
#include "../../detail/SPRuntimeDispatchQueueData.h"

namespace sprt::dispatch {

class EmboxThreadHandle;

// One armed timer, held in the reactor's deadline list.
struct EmboxTimerEntry {
	int64_t deadline = 0; // absolute monotonic ns of the next fire
	Handle *handle = nullptr;
};

struct SPRT_API EmboxData : public PlatformQueueData {
	// Pending wakeup/cancel request from any thread. Mirrors the wasm wakeword
	// bits: WakeupPresent / WakeupCancel. The idle spin watches this instead of
	// a pipe (see file comment).
	alignas(4) int32_t _wakeupReq = 0;

	// Active timers, scanned for the nearest deadline each loop iteration.
	Queue::Vector<EmboxTimerEntry> _timers;

	// Inboxes live in this object, not a pool vector. A smashed size field
	// (seen as i=23 with this=NULL in takePending after FreeType) would
	// walk off a 1-slot buffer into zeros.
	static constexpr size_t MaxThreadHandles = 8;
	EmboxThreadHandle *_threadHandles[MaxThreadHandles] = {};
	size_t _threadHandleCount = 0;

	static constexpr int32_t WakeupPresent = int32_t(1) << 30;
	static constexpr int32_t WakeupCancel = int32_t(1) << 29;

	EmboxData(QueueRef *, Queue::Data *data, const QueueInfo &info);
	~EmboxData();

	// Timer list management (called on the loop thread from the timer handle).
	void pushTimer(int64_t deadline, Handle *);
	void removeTimer(Handle *);
	int64_t nearestDeadline() const;

	// Fire every timer whose deadline has passed, dispatching each through
	// QueueData::notify. Returns the count.
	uint32_t fireExpired(RunContext *);

	void registerThreadHandle(EmboxThreadHandle *);
	void unregisterThreadHandle(EmboxThreadHandle *);
	uint32_t fireThreadHandles(RunContext *);

	// Cross-thread wakeup: set `_wakeupReq` so the idle spin returns.
	void notifyWakeup();
	// Loop-thread side: apply any pending wakeup/cancel request.
	void drainWakeup();
	// Idle until `timeoutMs` or `_wakeupReq`. Never poll()/nanosleep() a pipe.
	void spinWait(int timeoutMs);

	Status submit();
	uint32_t poll();
	uint32_t wait(TimeInterval);
	Status run(TimeInterval, QueueWakeupInfo &&);
	Status wakeup(WakeupFlags);
	void cancel();

	// monotonic clock helper
	static int64_t nowMonotonic();
};

// POD schedule state placement-constructed into Handle::_data.
struct EmboxTimerSource {
	int64_t firstTimeout = 0; // ns: delay to the first fire
	int64_t interval = 0; // ns: period between fires (0 => one-shot)
	int64_t deadline = 0; // ns: absolute deadline of the next fire (set at arm)
	uint32_t count = 0; // target number of fires (TimerHandle::Infinite)
	uint32_t value = 0; // fires so far

	void cancel() { }
};

class EmboxTimerHandle : public TimerHandle {
public:
	bool init(HandleClass *cl, TimerInfo &&info);
	virtual bool reset(TimerInfo &&info) override;

	Status rearm(EmboxData *reactor, EmboxTimerSource *src);
	Status disarm(EmboxData *reactor, EmboxTimerSource *src);
	void notify(EmboxData *reactor, EmboxTimerSource *src, const NotifyData &nd);
};

// POD state for a ThreadHandle. `pending` is set (from any thread) when work is
// posted and cleared (on the loop thread) when the reactor drains it.
struct EmboxThreadSource {
	int32_t pending = 0;

	void cancel() { }
};

class EmboxThreadHandle : public ThreadHandle {
public:
	virtual ~EmboxThreadHandle() = default;

	bool init(HandleClass *);

	Status rearm(EmboxData *, EmboxThreadSource *);
	Status disarm(EmboxData *, EmboxThreadSource *);
	void notify(EmboxData *, EmboxThreadSource *, const NotifyData &);

	virtual Status perform(Rc<Task> &&task) override;
	virtual Status perform(dispatch::Function<void()> &&func, Ref *target, StringView tag) override;

	bool takePending();

protected:
	void signal();

	sprt::mutex _mutex;
	EmboxData *_embox = nullptr;
};

struct SPRT_API Queue::Data : public QueueData {
	HandleClass _emboxTimerClass;
	HandleClass _emboxThreadClass;

	Data(QueueRef *q, const QueueInfo &info);
};

// Wire a HandleClass so create/run/suspend/resume/cancel/notify route through
// the engine handle's arm/disarm/notify (mirrors setupWasmHandleClass).
template <typename HandleType, typename SourceType>
void setupEmboxHandleClass(QueueHandleClassInfo *info, HandleClass *cl, bool suspendable) {
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
				reinterpret_cast<EmboxData *>(platformData->_platformQueue), source);
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
					reinterpret_cast<EmboxData *>(platformData->_platformQueue), source);
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
						reinterpret_cast<EmboxData *>(platformData->_platformQueue), source);
			}
			return status;
		};
	}

	cl->notifyFn = [](HandleClass *cl, Handle *handle, uint8_t data[Handle::DataSize],
						   const NotifyData &n) {
		auto platformData = static_cast<Queue::Data *>(cl->info->data);
		auto source = reinterpret_cast<SourceType *>(data);

		static_cast<HandleType *>(handle)->notify(
				reinterpret_cast<EmboxData *>(platformData->_platformQueue), source, n);
	};
}

} // namespace sprt::dispatch

#endif  // CORE_EVENT_PLATFORM_EMBOX_SPEVENT_EMBOX_H_
