/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
SPDX-License-Identifier: MIT
**/

// NuttX dispatch reactor — timers + atomics + CLOCK_MONOTONIC spin. See SPEvent-nuttx.h.

#include "SPEvent-nuttx.h"

#include <sprt/runtime/log.h>
#include <sprt/c/__sprt_errno.h>
#include <sprt/cxx/algorithm>
#include <sprt/cxx/mutex>

#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <sched.h>

namespace sprt::dispatch {

static constexpr int64_t NUTTX_DEADLINE_NONE = INT64_MAX;

static int64_t nuttx_timespec_ns(const struct timespec &ts) {
	return static_cast<int64_t>(ts.tv_sec) * 1000000000ll + ts.tv_nsec;
}

static int64_t nuttx_read_ns(clockid_t id) {
	struct timespec ts;
	if (clock_gettime(id, &ts) == 0) {
		return nuttx_timespec_ns(ts);
	}
	return 0;
}

/* Which clock the looper reads, decided by MEASURING both rather than by name.

This used to be CLOCK_MONOTONIC with a fallback to CLOCK_REALTIME only when monotonic was stuck at
zero — seen during bcm2711 mailbox bring-up, where realtime and sleep() still advanced. That is not
enough, and the gap it left cost half the frame.

On raspberrypi-4b monotonic is not stuck, it is COARSE: CONFIG_USEC_PER_TICK=1000 with no
CONFIG_SCHED_TICKLESS, so it advances once a millisecond, while CLOCK_REALTIME resolves to 315ns on
the same build. Over the ~150us that 64 sched_yield() calls take, a coarse clock is
indistinguishable from a stopped one — so spinWait's "the clock is dead" guard fired on every
single wait, and every wait became a 16ms sleep that could not see a wakeup. Measured: 8.8ms of a
18.6ms frame was one performOnAppThread waiting to be noticed.

The probe reads each clock until it changes and takes the step; a clock that never moves, or that
answers zero, scores worst and loses. Once, at first use. A stopped monotonic therefore still ends
up on realtime, exactly as the old fallback intended - and a merely coarse one does too. */
static clockid_t nuttx_probe_clock() {
	auto step = [](clockid_t id) -> int64_t {
		auto start = nuttx_read_ns(id);
		if (start == 0) {
			return INT64_MAX;
		}
		// Bounded: a clock that never advances must not hang the first wait.
		for (uint32_t i = 0; i < 200000u; ++i) {
			auto now = nuttx_read_ns(id);
			if (now > start) {
				return now - start;
			}
		}
		return INT64_MAX;
	};

	// Monotonic wins ties: it is the correct clock for a duration, and realtime is taken only when
	// measurably finer. The boards where that happens have no RTC and no time sync, so there is
	// nothing to step the wall clock mid-run.
	return step(CLOCK_REALTIME) < step(CLOCK_MONOTONIC) ? CLOCK_REALTIME : CLOCK_MONOTONIC;
}

static int64_t nuttx_now_ns() {
	static const clockid_t source = nuttx_probe_clock();

	auto n = nuttx_read_ns(source);
	if (n != 0) {
		return n;
	}

	// The chosen source stopped answering. The other one is better than freezing the looper.
	return nuttx_read_ns(source == CLOCK_REALTIME ? CLOCK_MONOTONIC : CLOCK_REALTIME);
}

static int64_t nuttx_rel_timeout(TimeInterval ival, int64_t nearest, int64_t now) {
	int64_t relIval;
	if (!ival) {
		relIval = 0;
	} else if (ival == TimeInterval::Infinite) {
		relIval = -1;
	} else {
		relIval = static_cast<int64_t>(ival.toMicroseconds()) * 1000;
	}
	if (relIval == 0) {
		return 0;
	}
	int64_t relTimer;
	if (nearest == NUTTX_DEADLINE_NONE) {
		relTimer = -1;
	} else {
		relTimer = nearest - now;
		if (relTimer < 0) {
			relTimer = 0;
		}
	}
	if (relIval < 0) {
		return relTimer;
	}
	if (relTimer < 0) {
		return relIval;
	}
	return relIval < relTimer ? relIval : relTimer;
}

NuttxData::NuttxData(QueueRef *q, Queue::Data *data, const QueueInfo &info)
: PlatformQueueData(q, data, info.flags) { }

NuttxData::~NuttxData() { }

int64_t NuttxData::nowMonotonic() { return nuttx_now_ns(); }

void NuttxData::pushTimer(int64_t deadline, Handle *h) {
	_timers.push_back(NuttxTimerEntry{deadline, h});
}

void NuttxData::removeTimer(Handle *h) {
	_timers.erase(sprt::remove_if(_timers.begin(), _timers.end(),
	                  [h](const NuttxTimerEntry &e) { return e.handle == h; }),
	        _timers.end());
}

int64_t NuttxData::nearestDeadline() const {
	int64_t best = NUTTX_DEADLINE_NONE;
	for (const auto &e : _timers) {
		if (e.deadline < best) best = e.deadline;
	}
	return best;
}

uint32_t NuttxData::fireExpired(RunContext *ctx) {
	int64_t now = nuttx_now_ns();
	uint32_t ndue = 0;
	for (size_t i = 0; i < _timers.size(); ) {
		if (_timers[i].deadline > now) {
			++i;
			continue;
		}
		Handle *h = _timers[i].handle;
		_timers.erase(_timers.begin() + i);
		static_cast<NuttxTimerHandle *>(h)->notify(this, nullptr, NotifyData{});
		++ndue;
	}
	return ndue;
}

void NuttxData::registerThreadHandle(NuttxThreadHandle *h) { _threadHandles.emplace_back(h); }

void NuttxData::unregisterThreadHandle(NuttxThreadHandle *h) {
	for (size_t i = 0; i < _threadHandles.size(); ++i) {
		if (_threadHandles[i] == h) {
			_threadHandles[i] = _threadHandles.back();
			_threadHandles.pop_back();
			return;
		}
	}
}

uint32_t NuttxData::fireThreadHandles(RunContext *) {
	uint32_t count = 0;
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

void NuttxData::notifyWakeup() {
	__atomic_fetch_or(&_wakeupReq, WakeupPresent, __ATOMIC_SEQ_CST);
}

void NuttxData::drainWakeup() {
	__atomic_store_n(&_wakeupReq, 0, __ATOMIC_SEQ_CST);
}

/* Wait for a wakeup, a deadline, or the timeout - without ever going deaf.

Two rules, and the second one is the whole point:

  * a wakeup posted while this thread waits must be SEEN, not slept through. The old code, once it
    decided the clock was not advancing, called usleep for the entire remaining timeout and never
    looked at _wakeupReq again. On raspberrypi-4b that decision was wrong every time (see
    nuttx_now_ns) and every wait became a deaf 16ms sleep: measured 8.8ms average latency on a post
    from the loop thread to the app thread, 47% of an 18.6ms frame.

  * an idle looper must not burn a core. So: a short yield-spin first, because a wakeup from
    another thread usually lands within microseconds and catching it there costs one scheduler
    round trip; then sleep in slices, rechecking between them.

The slice is one scheduler tick. Worst-case wakeup latency is therefore one tick rather than the
whole timeout, and a sleep never outlasts what it can react to. Getting below a tick needs a real
blocking primitive (a semaphore posted by notifyWakeup) rather than a shorter sleep - usleep cannot
resolve finer than the tick anyway.

Bounded by the slice count as well as by the clock, so a genuinely dead clock ends the wait instead
of spinning in it. */
void NuttxData::spinWait(int timeoutMs) {
	if (timeoutMs <= 0) {
		return;
	}

	static constexpr unsigned YieldSpins = 64;
	static constexpr unsigned SliceUs = 1000;

	const int64_t start = nuttx_now_ns();
	const int64_t until = start + int64_t(timeoutMs) * 1'000'000ll;

	const auto pending = [this] {
		return __atomic_load_n(&_wakeupReq, __ATOMIC_SEQ_CST) != 0;
	};

	for (unsigned spins = 0; spins < YieldSpins; ++spins) {
		if (pending()) {
			return;
		}
		sched_yield();
	}

	const unsigned slices =
			(static_cast<unsigned>(timeoutMs) * 1000u + SliceUs - 1u) / SliceUs;
	for (unsigned i = 0; i < slices; ++i) {
		if (pending()) {
			return;
		}
		if (nuttx_now_ns() >= until) {
			return;
		}
		::usleep(SliceUs);
	}
}

Status NuttxData::submit() { return Status::Ok; }

uint32_t NuttxData::poll() {
	RunContext ctx;
	pushContext(&ctx, RunContext::Poll);

	uint32_t result = fireExpired(&ctx);
	result += fireThreadHandles(&ctx);
	drainWakeup();

	popContext(&ctx);
	return result;
}

uint32_t NuttxData::wait(TimeInterval ival) {
	RunContext ctx;
	pushContext(&ctx, RunContext::Wait);

	uint32_t result = fireExpired(&ctx);
	result += fireThreadHandles(&ctx);
	if (result == 0 && ctx.state == RunContext::Running) {
		int64_t now = nuttx_now_ns();
		int64_t rel = nuttx_rel_timeout(ival, nearestDeadline(), now);
		if (rel != 0) {
			int timeoutMs;
			if (rel < 0) {
				timeoutMs = -1;
			} else {
				timeoutMs = static_cast<int>(rel / 1000000ll);
				if (rel > 0 && timeoutMs == 0) timeoutMs = 1;
			}
			if (timeoutMs < 0 || timeoutMs > 16) {
				timeoutMs = 16;
			}
			spinWait(timeoutMs);
		}
		drainWakeup();
		result = fireExpired(&ctx) + fireThreadHandles(&ctx);
	}

	popContext(&ctx);
	return result;
}

Status NuttxData::run(TimeInterval ival, QueueWakeupInfo &&winfo) {
	RunContext ctx;
	ctx.wakeupStatus = Status::Suspended;
	ctx.runWakeupFlags = winfo.flags;

	pushContext(&ctx, RunContext::Run);

	while (ctx.state == RunContext::Running) {
		/* Drained BEFORE the handles are fired, not after.
		
		A signal that arrives while fireThreadHandles/fireExpired are running has already had its
		work queued - `pending` is set on the handle - but a drain placed after them would clear
		_wakeupReq without that work having been picked up, and the following wait would sleep out
		its whole timeout with the task sitting there. Clearing first means the flag can only be
		set again by a signal this iteration has not yet served, which is exactly what the wait
		below must not sleep through. */
		drainWakeup();

		fireThreadHandles(&ctx);
		if (ctx.state != RunContext::Running) break;
		fireExpired(&ctx);
		if (ctx.state != RunContext::Running) break;

		int64_t now = nuttx_now_ns();
		int64_t rel = nuttx_rel_timeout(ival, nearestDeadline(), now);
		if (rel == 0) {
			continue;
		}
		int timeoutMs;
		if (rel < 0) {
			timeoutMs = -1;
		} else {
			timeoutMs = static_cast<int>(rel / 1000000ll);
			if (rel > 0 && timeoutMs == 0) timeoutMs = 1;
		}
		// NuttX poll()/nanosleep() from a pthread ignore their timeout. Cap the
		// idle stretch so posted performOnThread work is observed.
		if (timeoutMs < 0 || timeoutMs > 16) {
			timeoutMs = 16;
		}
		spinWait(timeoutMs);
	}

	popContext(&ctx);
	return ctx.wakeupStatus;
}

Status NuttxData::wakeup(WakeupFlags flags) {
	__atomic_store_n(&_wakeupReq, static_cast<int32_t>(toInt(flags)) | WakeupPresent,
			__ATOMIC_SEQ_CST);
	notifyWakeup();
	return Status::Ok;
}

void NuttxData::cancel() {
	__atomic_store_n(&_wakeupReq, WakeupPresent | WakeupCancel, __ATOMIC_SEQ_CST);
	notifyWakeup();
}

// --- NuttxTimerHandle ---

bool NuttxTimerHandle::init(HandleClass *cl, TimerInfo &&info) {
	if (!Handle::init(cl, info.completion)) {
		return false;
	}
	auto *src = reinterpret_cast<NuttxTimerSource *>(_data);
	src->firstTimeout = static_cast<int64_t>(info.timeout.toMicroseconds()) * 1000;
	src->interval = static_cast<int64_t>(info.interval.toMicroseconds()) * 1000;
	src->count = info.count;
	src->value = 0;
	src->deadline = 0;
	return true;
}

bool NuttxTimerHandle::reset(TimerInfo &&info) {
	auto *src = reinterpret_cast<NuttxTimerSource *>(_data);
	src->firstTimeout = static_cast<int64_t>(info.timeout.toMicroseconds()) * 1000;
	src->interval = static_cast<int64_t>(info.interval.toMicroseconds()) * 1000;
	src->count = info.count;
	return true;
}

Status NuttxTimerHandle::rearm(NuttxData *reactor, NuttxTimerSource *src) {
	int64_t now = reactor->nowMonotonic();
	int64_t delay = src->firstTimeout > 0 ? src->firstTimeout
			: (src->interval > 0 ? src->interval : 16'000'000ll);
	src->deadline = now + delay;
	reactor->pushTimer(src->deadline, this);
	return Status::Ok;
}

Status NuttxTimerHandle::disarm(NuttxData *reactor, NuttxTimerSource *src) {
	reactor->removeTimer(this);
	return Status::Ok;
}

void NuttxTimerHandle::notify(NuttxData *reactor, NuttxTimerSource *src, const NotifyData &nd) {
	if (_status != Status::Ok) {
		return;
	}
	if (!src) {
		src = reinterpret_cast<NuttxTimerSource *>(_data);
	}
	int64_t now = reactor->nowMonotonic();
	uint32_t count = src->count;

	uint32_t fired = 1;
	if (src->interval > 0 && now > src->deadline) {
		fired = static_cast<uint32_t>(1 + (now - src->deadline) / src->interval);
	}
	uint32_t current = src->value + fired;
	if (count != TimerHandle::Infinite && current > count) {
		current = count;
	}
	src->value = current;

	if (count != TimerHandle::Infinite && current >= count) {
		cancel(Status::Done);
	} else {
		sendCompletion(current, Status::Ok);
		src->deadline += static_cast<int64_t>(fired) * src->interval;
		reactor->pushTimer(src->deadline, this);
	}
}

bool NuttxThreadHandle::init(HandleClass *cl) { return ThreadHandle::init(cl); }

bool NuttxThreadHandle::takePending() {
	auto s = reinterpret_cast<NuttxThreadSource *>(_data);
	return __atomic_exchange_n(&s->pending, 0, __ATOMIC_SEQ_CST) != 0;
}

void NuttxThreadHandle::signal() {
	auto s = reinterpret_cast<NuttxThreadSource *>(_data);
	__atomic_store_n(&s->pending, 1, __ATOMIC_SEQ_CST);
	if (_nuttx) {
		_nuttx->notifyWakeup();
	}
}

Status NuttxThreadHandle::rearm(NuttxData *n, NuttxThreadSource *s) {
	auto status = prepareRearm();
	if (status == Status::Ok) {
		_nuttx = n;
		// Do not clear `pending`: performOnThread may have queued work before the
		// handle was armed (Looper ctor parks it in _pendingHandles until run()).
		n->registerThreadHandle(this);
	}
	return status;
}

Status NuttxThreadHandle::disarm(NuttxData *n, NuttxThreadSource *s) {
	auto status = prepareDisarm();
	if (status == Status::Ok) {
		n->unregisterThreadHandle(this);
	}
	return status;
}

void NuttxThreadHandle::notify(NuttxData *, NuttxThreadSource *, const NotifyData &) {
	if (_status != Status::Ok) {
		return;
	}
	_mutex.lock();
	performAll([&](uint32_t) { _mutex.unlock(); });
}

Status NuttxThreadHandle::perform(Rc<Task> &&task) {
	{
		sprt::unique_lock lock(_mutex);
		_outputQueue.emplace_back(move(task));
	}
	signal();
	return Status::Ok;
}

Status NuttxThreadHandle::perform(dispatch::Function<void()> &&func, Ref *target, StringView tag) {
	{
		sprt::unique_lock lock(_mutex);
		_outputCallbacks.emplace_back(CallbackInfo{sprt::move(func), target, tag});
	}
	signal();
	return Status::Ok;
}

// --- Queue::Data ---

Queue::Data::Data(QueueRef *q, const QueueInfo &info) : QueueData(q, info.flags) {
	setupNuttxHandleClass<NuttxTimerHandle, NuttxTimerSource>(&_info, &_nuttxTimerClass, true);
	setupNuttxHandleClass<NuttxThreadHandle, NuttxThreadSource>(&_info, &_nuttxThreadClass, true);

	auto *platform = new (memory::pool::acquire()) NuttxData(q, this, info);
	_platformQueue = platform;
	_engine = QueueEngine::None;

	_submit = [](void *ptr) -> Status { return static_cast<NuttxData *>(ptr)->submit(); };
	_poll = [](void *ptr) -> uint32_t { return static_cast<NuttxData *>(ptr)->poll(); };
	_wait = [](void *ptr, TimeInterval ival) -> uint32_t {
		return static_cast<NuttxData *>(ptr)->wait(ival);
	};
	_run = [](void *ptr, TimeInterval ival, QueueWakeupInfo &&winfo) -> Status {
		return static_cast<NuttxData *>(ptr)->run(ival, move(winfo));
	};
	_wakeup = [](void *ptr, WakeupFlags flags) -> Status {
		return static_cast<NuttxData *>(ptr)->wakeup(flags);
	};
	_cancel = [](void *ptr) { static_cast<NuttxData *>(ptr)->cancel(); };
	_shutdown = [](void *ptr) { static_cast<NuttxData *>(ptr)->cancel(); };

	_timer = [](QueueData *d, void *ptr, TimerInfo &&tinfo) -> Rc<TimerHandle> {
		auto data = static_cast<Queue::Data *>(d);
		return Rc<NuttxTimerHandle>::create(&data->_nuttxTimerClass, move(tinfo));
	};
	_thread = [](QueueData *d, void *ptr) -> Rc<ThreadHandle> {
		auto data = static_cast<Queue::Data *>(d);
		return Rc<NuttxThreadHandle>::create(&data->_nuttxThreadClass);
	};
}

} // namespace sprt::dispatch

namespace sprt::dispatch::platform {

// Per-thread dispatch queue accessor (mirrors the linux/darwin/wasm/windows
// SPEvent-*.cc siblings). Looper::acquire calls this to obtain the queue the
// looper drives. NuttX, like wasm, has no GPU/engine acceleration in the flat
// image (the soft rasterizer is the renderer), so the engine mask is None —
// matching what Queue::Data::Data above sets on _engine.
Rc<QueueRef> getThreadQueue(QueueInfo &&info) {
	info.engineMask = QueueEngine::None;
	return Queue::create(move(info));
}

} // namespace sprt::dispatch::platform
