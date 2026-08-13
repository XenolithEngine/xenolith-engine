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

static int64_t nuttx_now_ns() {
	struct timespec ts;
	if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
		int64_t n = nuttx_timespec_ns(ts);
		if (n != 0) {
			return n;
		}
	}
	// bcm2711 mailbox bring-up has been seen with CLOCK_MONOTONIC stuck at 0
	// while CLOCK_REALTIME (and sleep()) still advance. Timers and spinWait
	// must not freeze the looper in that case — the scene would never present.
	if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
		return nuttx_timespec_ns(ts);
	}
	return 0;
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

void NuttxData::spinWait(int timeoutMs) {
	if (timeoutMs <= 0) {
		return;
	}
	int64_t start = nuttx_now_ns();
	int64_t until = start + int64_t(timeoutMs) * 1'000'000ll;
	unsigned spins = 0;
	while (nuttx_now_ns() < until) {
		if (__atomic_load_n(&_wakeupReq, __ATOMIC_SEQ_CST) != 0) {
			break;
		}
		sched_yield();
		// Clock not advancing: yield-spin would be infinite. usleep works on
		// the init task (the tick is alive).
		if (++spins >= 64 && nuttx_now_ns() <= start) {
			::usleep(static_cast<unsigned>(timeoutMs) * 1000u);
			break;
		}
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
		fireThreadHandles(&ctx);
		if (ctx.state != RunContext::Running) break;
		fireExpired(&ctx);
		if (ctx.state != RunContext::Running) break;

		drainWakeup();
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
