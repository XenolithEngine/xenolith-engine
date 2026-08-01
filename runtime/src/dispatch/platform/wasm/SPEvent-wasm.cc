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

#ifndef CORE_EVENT_PLATFORM_WASM_SPEVENT_WASM_CC_
#define CORE_EVENT_PLATFORM_WASM_SPEVENT_WASM_CC_

#include "SPEvent-wasm.h"

#include "../fd/SPEventFile.h"
#include "../fd/SPEventStatWatch.h"

#include <sprt/runtime/dispatch/task.h>
#include <sprt/runtime/log.h>

extern "C" {
// T1 host import (see runtime/core/wasm/clock_gettime.cc): nanoseconds for the
// given clock id. clkid 1 == CLOCK_MONOTONIC (performance.now()+timeOrigin).
__attribute__((import_module("sprt"), import_name("clock_now"))) double __sprt_host_clock_now(
		int clkid);
}

namespace sprt::dispatch {

// Monotonic "now" in nanoseconds. f64 keeps ns precision well past any realistic
// uptime (2^53 ns ~= 104 days), so a direct cast to int64 is exact for deadlines.
static inline int64_t wasm_now_ns() {
	return static_cast<int64_t>(__sprt_host_clock_now(1));
}

static constexpr int64_t WASM_DEADLINE_NONE = 0x7fff'ffff'ffff'ffffLL;

//
// WasmTimerHandle
//

bool WasmTimerHandle::init(HandleClass *cl, TimerInfo &&info) {
	if (!Handle::init(cl, info.completion)) {
		return false;
	}

	auto s = reinterpret_cast<WasmTimerSource *>(_data);

	int64_t timeoutNs = static_cast<int64_t>(info.timeout.toMicroseconds()) * 1000;
	int64_t intervalNs = static_cast<int64_t>(info.interval.toMicroseconds()) * 1000;

	// Mirror timerfd normalization: a one-shot repeats never; a count-based timer
	// without an explicit interval repeats every `timeout`.
	if (info.count == 1) {
		intervalNs = timeoutNs;
	} else if (intervalNs == 0) {
		intervalNs = timeoutNs;
	}

	s->firstTimeout = timeoutNs ? timeoutNs : intervalNs;
	s->interval = intervalNs;
	s->count = info.count;
	s->value = 0;
	s->deadline = 0;
	return true;
}

bool WasmTimerHandle::reset(TimerInfo &&info) {
	if (info.completion) {
		_completion = move(info.completion);
		_userdata = nullptr;
	}

	auto s = reinterpret_cast<WasmTimerSource *>(_data);

	int64_t timeoutNs = static_cast<int64_t>(info.timeout.toMicroseconds()) * 1000;
	int64_t intervalNs = static_cast<int64_t>(info.interval.toMicroseconds()) * 1000;
	if (info.count == 1) {
		intervalNs = timeoutNs;
	} else if (intervalNs == 0) {
		intervalNs = timeoutNs;
	}

	s->firstTimeout = timeoutNs ? timeoutNs : intervalNs;
	s->interval = intervalNs;
	s->count = info.count;
	s->value = 0;
	s->deadline = 0;

	return Handle::reset();
}

Status WasmTimerHandle::rearm(WasmData *w, WasmTimerSource *s) {
	auto status = prepareRearm();
	if (status == Status::Ok) {
		s->deadline = wasm_now_ns() + s->firstTimeout;
		w->pushTimer(s->deadline, this);
	}
	return status;
}

Status WasmTimerHandle::disarm(WasmData *w, WasmTimerSource *s) {
	auto status = prepareDisarm();
	if (status == Status::Ok) {
		w->removeTimer(this);
	}
	return status;
}

void WasmTimerHandle::notify(WasmData *w, WasmTimerSource *s, const NotifyData &) {
	if (_status != Status::Ok) {
		return;
	}

	int64_t now = wasm_now_ns();
	uint32_t count = s->count;

	// How many periods have elapsed since this fire's deadline (>= 1). This
	// replicates read(timerfd) returning the accumulated expiration count, so a
	// busy loop does not lose ticks.
	uint32_t fired = 1;
	if (s->interval > 0 && now > s->deadline) {
		fired = static_cast<uint32_t>(1 + (now - s->deadline) / s->interval);
	}

	uint32_t current = s->value + fired;
	if (count != TimerHandle::Infinite && current > count) {
		current = count;
	}
	s->value = current;

	if (count != TimerHandle::Infinite && current >= count) {
		// Final fire: cancel(Done) finalizes and delivers the completion once,
		// matching the epoll/uring timers (they deliver the terminal fire as a
		// Done completion with value 0, not the cumulative count).
		cancel(Status::Done);
	} else {
		sendCompletion(current, Status::Ok);
		// Advance the deadline by the elapsed periods (no drift) and re-arm.
		s->deadline += static_cast<int64_t>(fired) * s->interval;
		w->pushTimer(s->deadline, this);
	}
}

//
// WasmThreadHandle — cross-thread inbox (eventfd replacement)
//

bool WasmThreadHandle::init(HandleClass *cl) {
	// The source (WasmThreadSource) is placement-constructed by the class createFn.
	return ThreadHandle::init(cl);
}

bool WasmThreadHandle::takePending() {
	auto s = reinterpret_cast<WasmThreadSource *>(_data);
	return __atomic_exchange_n(&s->pending, 0, __ATOMIC_SEQ_CST) != 0;
}

void WasmThreadHandle::signal() {
	auto s = reinterpret_cast<WasmThreadSource *>(_data);
	// Set pending BEFORE bumping the wakeword so the loop, on waking, always sees
	// the work (the wakeword bump is what breaks its wait32; see WasmData::run).
	__atomic_store_n(&s->pending, 1, __ATOMIC_SEQ_CST);
	if (_wasm) {
		_wasm->bumpAndNotify();
	}
}

Status WasmThreadHandle::rearm(WasmData *w, WasmThreadSource *s) {
	auto status = prepareRearm();
	if (status == Status::Ok) {
		_wasm = w;
		__atomic_store_n(&s->pending, 0, __ATOMIC_SEQ_CST);
		w->registerThreadHandle(this);
	}
	return status;
}

Status WasmThreadHandle::disarm(WasmData *w, WasmThreadSource *s) {
	auto status = prepareDisarm();
	if (status == Status::Ok) {
		w->unregisterThreadHandle(this);
	}
	return status;
}

void WasmThreadHandle::notify(WasmData *w, WasmThreadSource *s, const NotifyData &) {
	if (_status != Status::Ok) {
		return;
	}
	// Drain the cross-thread queue into the PerformEngine and run it. performAll
	// swaps the queues out under the lock, then unlocks and runs the callbacks.
	_mutex.lock();
	performAll([&](uint32_t) { _mutex.unlock(); });
}

Status WasmThreadHandle::perform(Rc<Task> &&task) {
	{
		sprt::unique_lock lock(_mutex);
		_outputQueue.emplace_back(move(task));
	}
	signal();
	return Status::Ok;
}

Status WasmThreadHandle::perform(dispatch::Function<void()> &&func, Ref *target, StringView tag) {
	{
		sprt::unique_lock lock(_mutex);
		_outputCallbacks.emplace_back(CallbackInfo{sprt::move(func), target, tag});
	}
	signal();
	return Status::Ok;
}

//
// WasmData reactor
//

WasmData::WasmData(QueueRef *q, Queue::Data *data, const QueueInfo &info)
: PlatformQueueData(q, data, info.flags) {
	// _timers binds to the queue's persistent pool (active during Queue::Data ctor).
}

WasmData::~WasmData() { _timers.clear(); }

void WasmData::pushTimer(int64_t deadline, Handle *h) {
	auto &e = _timers.emplace_back();
	e.deadline = deadline;
	e.handle = h;
}

void WasmData::removeTimer(Handle *h) {
	for (size_t i = 0; i < _timers.size(); ++i) {
		if (_timers[i].handle == h) {
			_timers[i] = _timers.back();
			_timers.pop_back();
			return;
		}
	}
}

int64_t WasmData::nearestDeadline() const {
	int64_t m = WASM_DEADLINE_NONE;
	for (auto &e : _timers) {
		if (e.deadline < m) {
			m = e.deadline;
		}
	}
	return m;
}

void WasmData::registerThreadHandle(WasmThreadHandle *h) { _threadHandles.emplace_back(h); }

void WasmData::unregisterThreadHandle(WasmThreadHandle *h) {
	for (size_t i = 0; i < _threadHandles.size(); ++i) {
		if (_threadHandles[i] == h) {
			_threadHandles[i] = _threadHandles.back();
			_threadHandles.pop_back();
			return;
		}
	}
}

uint32_t WasmData::fireThreadHandles(RunContext *) {
	uint32_t count = 0;
	// Snapshot size; notify() drains one inbox and can run arbitrary callbacks,
	// but a handle never unregisters itself mid-notify (only cancel does, off the
	// dispatch path), so a plain index walk with a per-handle retain is safe.
	for (size_t i = 0; i < _threadHandles.size(); ++i) {
		auto h = _threadHandles[i];
		if (h->takePending()) {
			auto refId = sprt::retain(h);
			NotifyData nd;
			_data->notify(h, nd);
			sprt::release(h, refId);
			++count;
		}
	}
	return count;
}

uint32_t WasmData::fireExpired(RunContext *ctx) {
	int64_t now = wasm_now_ns();
	auto &batch = ctx->eventBatch;
	batch.clear();

	// Snapshot every due timer (pinning it with a retain) and remove it from the
	// active list before running any callback: a completion can re-enter the loop
	// or re-arm the same timer, and cancel() during notify releases the queue's
	// reference, so the pin keeps the handle alive across its own dispatch.
	for (size_t i = 0; i < _timers.size();) {
		if (_timers[i].deadline <= now) {
			auto h = _timers[i].handle;
			auto &slot = batch.emplace_back();
			slot.handle = h;
			slot.flags = 0;
			slot.refId = h ? sprt::retain(h) : uint64_t(0);
			_timers[i] = _timers.back();
			_timers.pop_back();
		} else {
			++i;
		}
	}

	uint32_t count = 0;
	NotifyData data;
	for (auto &slot : batch) {
		if (slot.handle) {
			data.result = 0;
			data.queueFlags = 0;
			data.userFlags = 0;

			_data->notify(slot.handle, data);

			sprt::release(slot.handle, slot.refId);
			slot.handle = nullptr;
			++count;
		}
	}
	batch.clear();
	return count;
}

void WasmData::bumpAndNotify() {
	__atomic_add_fetch(&_wakeword, 1, __ATOMIC_SEQ_CST);
	__builtin_wasm_memory_atomic_notify(reinterpret_cast<int *>(&_wakeword), 0x7fff'ffff);
}

void WasmData::drainWakeup() {
	int32_t req = __atomic_exchange_n(&_wakeupReq, 0, __ATOMIC_SEQ_CST);
	if ((req & WakeupPresent) && _runContext) {
		if (req & WakeupCancel) {
			stopRootContext(WakeupFlags::ContextDefault, true);
		} else {
			auto flags = WakeupFlags(req & ~(WakeupPresent | WakeupCancel));
			stopContext(_runContext, flags, true);
		}
	}
}

// Relative wait timeout in ns for wait32: -1 = block forever, 0 = don't block.
// Bounded by both the caller's interval and the nearest timer deadline.
static int64_t wasm_rel_timeout(TimeInterval ival, int64_t nearest, int64_t now) {
	int64_t relIval;
	if (!ival) {
		relIval = 0;
	} else if (ival == TimeInterval::Infinite) {
		relIval = -1;
	} else {
		relIval = static_cast<int64_t>(ival.toMicroseconds()) * 1000;
	}

	int64_t relTimer = (nearest == WASM_DEADLINE_NONE) ? -1 : (nearest > now ? nearest - now : 0);

	if (relIval < 0) {
		return relTimer;
	}
	if (relTimer < 0) {
		return relIval;
	}
	return relIval < relTimer ? relIval : relTimer;
}

Status WasmData::submit() { return Status::Ok; }

uint32_t WasmData::poll() {
	RunContext ctx;
	pushContext(&ctx, RunContext::Poll);

	uint32_t result = fireExpired(&ctx);
	result += fireThreadHandles(&ctx);
	drainWakeup();

	popContext(&ctx);
	return result;
}

uint32_t WasmData::wait(TimeInterval ival) {
	RunContext ctx;
	pushContext(&ctx, RunContext::Wait);

	int32_t gen = __atomic_load_n(&_wakeword, __ATOMIC_SEQ_CST);
	uint32_t result = fireExpired(&ctx);
	result += fireThreadHandles(&ctx);
	if (result == 0 && ctx.state == RunContext::Running) {
		int64_t rel = wasm_rel_timeout(ival, nearestDeadline(), wasm_now_ns());
		if (rel != 0) {
			__builtin_wasm_memory_atomic_wait32(reinterpret_cast<int *>(&_wakeword), gen, rel);
		}
		drainWakeup();
		result = fireExpired(&ctx) + fireThreadHandles(&ctx);
	}

	popContext(&ctx);
	return result;
}

Status WasmData::run(TimeInterval ival, WakeupFlags wakeupFlags, TimeInterval wakeupTimeout) {
	RunContext ctx;
	ctx.wakeupStatus = Status::Suspended;
	ctx.runWakeupFlags = wakeupFlags;

	Rc<Handle> timerHandle;
	if (ival && ival != TimeInterval::Infinite) {
		// Bounded run: a one-shot timer stops the context when the interval expires.
		struct WakeupParams : Ref {
			RunContext *ctx = nullptr;
			WakeupFlags wakeupFlags = WakeupFlags::None;
		};

		auto p = Rc<WakeupParams>::alloc();
		p->ctx = &ctx;
		p->wakeupFlags = wakeupFlags;

		timerHandle = _queue->get()->scheduleTimer(
				TimerInfo{
					.completion = TimerInfo::Completion::create<WasmData>(this,
							[](WasmData *data, TimerHandle *handle, uint32_t value, Status status) {
			if (status == Status::Done) {
				auto p = static_cast<WakeupParams *>(handle->getUserdata());
				data->stopContext(p->ctx, p->wakeupFlags, false);
			}
		}),
					.timeout = ival,
					.interval = TimeInterval(),
					.count = 1,
				},
				p);
	}

	pushContext(&ctx, RunContext::Run);

	while (ctx.state == RunContext::Running) {
		// Capture the wakeword generation up front: any cross-thread post or
		// wakeup after this point bumps it, so the wait32 below returns at once
		// instead of sleeping through freshly-queued work (condvar-over-futex).
		int32_t gen = __atomic_load_n(&_wakeword, __ATOMIC_SEQ_CST);

		fireExpired(&ctx);
		if (ctx.state != RunContext::Running) {
			break;
		}

		fireThreadHandles(&ctx);
		if (ctx.state != RunContext::Running) {
			break;
		}

		drainWakeup();
		if (ctx.state != RunContext::Running) {
			break;
		}

		int64_t now = wasm_now_ns();
		int64_t nearest = nearestDeadline();
		int64_t rel = (nearest == WASM_DEADLINE_NONE) ? -1 : (nearest > now ? nearest - now : 0);
		if (rel == 0) {
			// A timer is already due; loop back to fire it without blocking.
			continue;
		}

		__builtin_wasm_memory_atomic_wait32(reinterpret_cast<int *>(&_wakeword), gen, rel);
	}

	if (timerHandle) {
		timerHandle->cancel();
		timerHandle = nullptr;
	}

	popContext(&ctx);
	return ctx.wakeupStatus;
}

Status WasmData::wakeup(WakeupFlags flags) {
	__atomic_store_n(&_wakeupReq, static_cast<int32_t>(toInt(flags)) | WakeupPresent,
			__ATOMIC_SEQ_CST);
	bumpAndNotify();
	return Status::Ok;
}

void WasmData::cancel() {
	__atomic_store_n(&_wakeupReq, WakeupPresent | WakeupCancel, __ATOMIC_SEQ_CST);
	bumpAndNotify();
}

//
// Queue::Data — pick the wasm engine and wire the callback table
//

Queue::Data::Data(QueueRef *q, const QueueInfo &info) : QueueData(q, info.flags) {
	if (hasFlag(info.engineMask, QueueEngine::Wasm)) {
		setupWasmHandleClass<WasmTimerHandle, WasmTimerSource>(&_info, &_wasmTimerClass, true);
		setupWasmHandleClass<WasmThreadHandle, WasmThreadSource>(&_info, &_wasmThreadClass, true);
		setupInlineFileHandleClass(&_info, &_wasmFileInlineClass);
		setupStatWatchClass(&_info, &_wasmWatchClass);

		auto w = new (memory::pool::acquire()) WasmData(_info.queue, this, info);

		_submit = [](void *ptr) { return static_cast<WasmData *>(ptr)->submit(); };
		_poll = [](void *ptr) { return static_cast<WasmData *>(ptr)->poll(); };
		_wait = [](void *ptr, TimeInterval ival) {
			return static_cast<WasmData *>(ptr)->wait(ival);
		};
		_run = [](void *ptr, TimeInterval ival, QueueWakeupInfo &&winfo) {
			return static_cast<WasmData *>(ptr)->run(ival, winfo.flags, winfo.timeout);
		};
		_wakeup = [](void *ptr, WakeupFlags flags) {
			return static_cast<WasmData *>(ptr)->wakeup(flags);
		};
		_cancel = [](void *ptr) { static_cast<WasmData *>(ptr)->cancel(); };
		_destroy = [](void *ptr) { delete static_cast<WasmData *>(ptr); };

		_timer = [](QueueData *d, void *ptr, TimerInfo &&info) -> Rc<TimerHandle> {
			auto data = static_cast<Queue::Data *>(d);
			return Rc<WasmTimerHandle>::create(&data->_wasmTimerClass, move(info));
		};

		_thread = [](QueueData *d, void *ptr) -> Rc<ThreadHandle> {
			auto data = static_cast<Queue::Data *>(d);
			return Rc<WasmThreadHandle>::create(&data->_wasmThreadClass);
		};

		// Async file I/O over the synchronous VFS: the portable inline FileHandle,
		// driven by a repeating reactor timer (one bounded read/write per fire).
		// There is no native async regular-file path on wasm, so this is always used.
		_makeFileHandle = [](QueueData *d, void *ptr, Rc<FileState> &&state) -> Rc<FileHandle> {
			auto data = static_cast<Queue::Data *>(d);
			return makeFileInlineHandle(d, &data->_wasmFileInlineClass, sprt::move(state));
		};

		// File-watch over the synchronous VFS: no change notifications exist, so
		// the portable stat-polling watch (a repeating reactor timer) is used.
		_watchFile = [](QueueData *d, void *ptr, WatchInfo &&info, Ref *ref) -> Rc<WatchHandle> {
			auto data = static_cast<Queue::Data *>(d);
			return makeStatWatchHandle(d, &data->_wasmWatchClass, sprt::move(info), ref);
		};

		// _listenHandle / _spawnProcess / _socketPoll stay null (sockets and
		// processes stay ENOSYS in the browser sandbox) - so
		// listenSocket/connectSocket return nullptr cleanly here; a future
		// client-side transport would go through a JS WebSocket/WebTransport
		// bridge over OpenSSL memory BIOs, not through these syscalls.

		_platformQueue = w;
		_engine = QueueEngine::Wasm;
	}
}

} // namespace sprt::dispatch

namespace sprt::dispatch::platform {

Rc<QueueRef> getThreadQueue(QueueInfo &&info) {
	info.engineMask = QueueEngine::Wasm;
	return Queue::create(move(info));
}

} // namespace sprt::dispatch::platform

#endif /* CORE_EVENT_PLATFORM_WASM_SPEVENT_WASM_CC_ */
