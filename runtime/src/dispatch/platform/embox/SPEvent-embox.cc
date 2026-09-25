/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
SPDX-License-Identifier: MIT
**/

// Embox dispatch reactor — timers + atomics + CLOCK_MONOTONIC spin. See SPEvent-embox.h.

#include "SPEvent-embox.h"
#include "../fd/SPEventFile.h"
#include "../fd/SPEventStatWatch.h"

#include <sprt/runtime/log.h>
#include <sprt/c/__sprt_errno.h>
#include <sprt/cxx/algorithm>
#include <sprt/cxx/mutex>

#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>

#if SPRT_EMBOX && !SPRT_EMBOX_USER
// xenolith-os drivers/xlfutex. Weak, as in core/embox/sprt_lock.cc: an image
// without it links both null and the looper polls as it always did.
extern "C" int xl_futex_wait(void *addr, unsigned size, uint64_t expected, int64_t timeout_ns)
		__attribute__((weak));
extern "C" int xl_futex_wake(void *addr, int nr) __attribute__((weak));
#endif

namespace sprt::dispatch {

#if SPRT_EMBOX && !SPRT_EMBOX_USER
// Whether the idle wait sleeps on the wakeup word. The same switch as the lock
// backend's: the image has the futex, and SPRT_EMBOX_FUTEX is not "0". A plain
// cached word, not a function-local static -- see core/embox/sprt_lock.cc for
// the guard that recursed through the lock it was guarding.
static int embox_futex_state = 0; // 0 unknown, 1 futex, 2 poll

static bool embox_futex_wait_enabled() {
	int state = __atomic_load_n(&embox_futex_state, __ATOMIC_RELAXED);
	if (state == 0) {
		bool futex = xl_futex_wait && xl_futex_wake;
		if (futex) {
			// "0" polls everywhere; "lock" keeps the futex for sprt_lock only and
			// polls here. Anything else: futex.
			auto env = ::getenv("SPRT_EMBOX_FUTEX");
			futex = !(env && (::strcmp(env, "0") == 0 || ::strcmp(env, "lock") == 0));
		}
		state = futex ? 1 : 2;
		__atomic_store_n(&embox_futex_state, state, __ATOMIC_RELAXED);
	}
	return state == 1;
}
#endif

static constexpr int64_t EMBOX_DEADLINE_NONE = INT64_MAX;

static int64_t embox_timespec_ns(const struct timespec &ts) {
	return static_cast<int64_t>(ts.tv_sec) * 1'000'000'000ll + ts.tv_nsec;
}

static int64_t embox_now_ns() {
	struct timespec ts;
	if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
		int64_t n = embox_timespec_ns(ts);
		if (n != 0) {
			return n;
		}
	}
	// bcm2711 mailbox bring-up has been seen with CLOCK_MONOTONIC stuck at 0
	// while CLOCK_REALTIME (and sleep()) still advance. Timers and spinWait
	// must not freeze the looper in that case — the scene would never present.
	if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
		return embox_timespec_ns(ts);
	}
	return 0;
}

static int64_t embox_rel_timeout(TimeInterval ival, int64_t nearest, int64_t now) {
	int64_t relIval;
	if (!ival) {
		relIval = 0;
	} else if (ival == TimeInterval::Infinite) {
		relIval = -1;
	} else {
		relIval = static_cast<int64_t>(ival.toMicroseconds()) * 1'000;
	}
	if (relIval == 0) {
		return 0;
	}
	int64_t relTimer;
	if (nearest == EMBOX_DEADLINE_NONE) {
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

EmboxData::EmboxData(QueueRef *q, Queue::Data *data, const QueueInfo &info)
: PlatformQueueData(q, data, info.flags) { }

EmboxData::~EmboxData() { }

int64_t EmboxData::nowMonotonic() { return embox_now_ns(); }

void EmboxData::pushTimer(int64_t deadline, Handle *h) {
	_timers.push_back(EmboxTimerEntry{deadline, h});
}

void EmboxData::removeTimer(Handle *h) {
	_timers.erase(sprt::remove_if(_timers.begin(), _timers.end(),
						  [h](const EmboxTimerEntry &e) { return e.handle == h; }),
			_timers.end());
}

int64_t EmboxData::nearestDeadline() const {
	int64_t best = EMBOX_DEADLINE_NONE;
	for (const auto &e : _timers) {
		if (e.deadline < best) {
			best = e.deadline;
		}
	}
	return best;
}

uint32_t EmboxData::fireExpired(RunContext *ctx) {
	int64_t now = embox_now_ns();
	uint32_t ndue = 0;
	for (size_t i = 0; i < _timers.size();) {
		if (_timers[i].deadline > now) {
			++i;
			continue;
		}
		Handle *h = _timers[i].handle;
		_timers.erase(_timers.begin() + i);
		static_cast<EmboxTimerHandle *>(h)->notify(this, nullptr, NotifyData{});
		++ndue;
	}
	return ndue;
}

void EmboxData::registerThreadHandle(EmboxThreadHandle *h) {
	if (!h) {
		return;
	}
	if (_threadHandleCount >= MaxThreadHandles) {
		oslog::vperror(__SPRT_LOCATION, "EmboxData", "thread handle table full");
		return;
	}
	_threadHandles[_threadHandleCount++] = h;
}

void EmboxData::unregisterThreadHandle(EmboxThreadHandle *h) {
	for (size_t i = 0; i < _threadHandleCount; ++i) {
		if (_threadHandles[i] == h) {
			_threadHandles[i] = _threadHandles[_threadHandleCount - 1];
			_threadHandles[_threadHandleCount - 1] = nullptr;
			--_threadHandleCount;
			return;
		}
	}
}

uint32_t EmboxData::fireThreadHandles(RunContext *) {
	uint32_t count = 0;
	const size_t n = _threadHandleCount < MaxThreadHandles ? _threadHandleCount : MaxThreadHandles;
	for (size_t i = 0; i < n; ++i) {
		auto h = _threadHandles[i];
		if (!h) {
			continue;
		}
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

bool EmboxData::registerAddressHandle(EmboxAddressWaitHandle *h) {
	if (_addressHandleCount >= MaxAddressHandles) {
		oslog::vperror(__SPRT_LOCATION, "EmboxData", "address wait table full");
		return false;
	}
	_addressHandles[_addressHandleCount++] = h;
	return true;
}

void EmboxData::unregisterAddressHandle(EmboxAddressWaitHandle *h) {
	for (size_t i = 0; i < _addressHandleCount; ++i) {
		if (_addressHandles[i] == h) {
			_addressHandles[i] = _addressHandles[_addressHandleCount - 1];
			_addressHandles[_addressHandleCount - 1] = nullptr;
			--_addressHandleCount;
			return;
		}
	}
}

uint32_t EmboxData::fireAddressHandles(RunContext *) {
	uint32_t count = 0;
	const size_t n = _addressHandleCount;
	for (size_t i = 0; i < n; ++i) {
		auto h = _addressHandles[i];
		if (!h) {
			continue;
		}
		auto value = h->load();
		if (value != h->getLastValue()) {
			auto refId = sprt::retain(h);
			NotifyData nd;
			nd.result = intptr_t(value);
			_data->notify(h, nd);
			sprt::release(h, refId);
			++count;
		}
	}
	return count;
}

bool EmboxData::hasChangedAddress() const {
	for (size_t i = 0; i < _addressHandleCount; ++i) {
		auto h = _addressHandles[i];
		if (h && h->load() != h->getLastValue()) {
			return true;
		}
	}
	return false;
}

void EmboxData::notifyWakeup() {
	__atomic_fetch_or(&_wakeupReq, WakeupPresent, __ATOMIC_SEQ_CST);
#if SPRT_EMBOX && !SPRT_EMBOX_USER
	// After the store: a sleeper that counted itself in before this load either
	// sees the new word and does not sleep, or is on the futex and gets woken.
	if (__atomic_load_n(&_sleepers, __ATOMIC_SEQ_CST) != 0 && embox_futex_wait_enabled()) {
		xl_futex_wake(&_wakeupReq, INT_MAX);
	}
#endif
}

void EmboxData::drainWakeup() {
	// Only the idle-poke bit. A stop request has to survive until run() consumes
	// it - a concurrent poll() draining the whole word would swallow it and the
	// looper would keep running.
	__atomic_fetch_and(&_wakeupReq, ~WakeupPresent, __ATOMIC_SEQ_CST);
}

void EmboxData::spinWait(int timeoutMs) {
	if (timeoutMs <= 0) {
		return;
	}
#if SPRT_EMBOX && !SPRT_EMBOX_USER
	// With the image's futex: one sleep that ends when notifyWakeup() changes the
	// word or the timeout passes, instead of a usleep(1000) per millisecond. The
	// timeout is the kernel's, so a clock that sits still does not matter here.
	if (embox_futex_wait_enabled()) {
		__atomic_add_fetch(&_sleepers, 1, __ATOMIC_SEQ_CST);
		xl_futex_wait(&_wakeupReq, 4, 0, int64_t(timeoutMs) * 1'000'000ll);
		__atomic_sub_fetch(&_sleepers, 1, __ATOMIC_SEQ_CST);
		return;
	}
#endif
	// usleep() slices, NOT a sched_yield() spin. The looper usually runs on the
	// task that owns the window, and sched_yield() there does not run a pthread
	// of equal or lower priority - core/embox/sprt_lock.cc says the same thing
	// about its own wait and polls with usleep for it. A spin here starves
	// exactly the threads whose performOnThread() work this wait exists to
	// receive: the dispatch test's two poster threads never got scheduled, and
	// Looper::run() sat in this function forever.
	//
	// The slice counter, not the deadline, is what bounds the wait: embox_now_ns()
	// can sit still (see its comment about CLOCK_MONOTONIC stuck at 0), and a
	// clock-only bound would then never expire. The deadline check is the early
	// exit for when the clock does run.
	const int64_t until = embox_now_ns() + int64_t(timeoutMs) * 1'000'000ll;
	for (int slice = 0; slice < timeoutMs; ++slice) {
		if (__atomic_load_n(&_wakeupReq, __ATOMIC_SEQ_CST) != 0 || hasChangedAddress()) {
			return;
		}
		::usleep(1'000);
		if (embox_now_ns() >= until) {
			return;
		}
	}
}

Status EmboxData::submit() { return Status::Ok; }

uint32_t EmboxData::poll() {
	RunContext ctx;
	pushContext(&ctx, RunContext::Poll);

	uint32_t result = fireExpired(&ctx);
	result += fireThreadHandles(&ctx);
	result += fireAddressHandles(&ctx);
	drainWakeup();

	popContext(&ctx);
	return result;
}

uint32_t EmboxData::wait(TimeInterval ival) {
	RunContext ctx;
	pushContext(&ctx, RunContext::Wait);

	uint32_t result = fireExpired(&ctx);
	result += fireThreadHandles(&ctx);
	result += fireAddressHandles(&ctx);
	if (result == 0 && ctx.state == RunContext::Running) {
		int64_t now = embox_now_ns();
		int64_t rel = embox_rel_timeout(ival, nearestDeadline(), now);
		if (rel != 0) {
			int timeoutMs;
			if (rel < 0) {
				timeoutMs = -1;
			} else {
				timeoutMs = static_cast<int>(rel / 1'000'000ll);
				if (rel > 0 && timeoutMs == 0) {
					timeoutMs = 1;
				}
			}
			if (timeoutMs < 0 || timeoutMs > 16) {
				timeoutMs = 16;
			}
			spinWait(timeoutMs);
		}
		drainWakeup();
		result = fireExpired(&ctx) + fireThreadHandles(&ctx) + fireAddressHandles(&ctx);
	}

	popContext(&ctx);
	return result;
}

Status EmboxData::run(TimeInterval ival, QueueWakeupInfo &&winfo) {
	RunContext ctx;
	ctx.wakeupStatus = Status::Suspended;
	ctx.runWakeupFlags = winfo.flags;

	// Absolute deadline for a timed run. embox_rel_timeout() only bounds ONE idle
	// slice and is recomputed from `now` every iteration, so on its own a
	// run(500ms) re-arms the same 500 ms forever and never returns. Other
	// backends get this from a one-shot timer that stops the context (see
	// EPollData::run); here the loop owns the deadline directly.
	int64_t runDeadline = EMBOX_DEADLINE_NONE;
	if (ival && ival != TimeInterval::Infinite) {
		runDeadline = embox_now_ns() + static_cast<int64_t>(ival.toMicroseconds()) * 1'000;
	}

	pushContext(&ctx, RunContext::Run);

	while (ctx.state == RunContext::Running) {
		if (runDeadline != EMBOX_DEADLINE_NONE && embox_now_ns() >= runDeadline) {
			// Not external: the run asked for this bound itself.
			stopContext(&ctx, ctx.runWakeupFlags, false);
			break;
		}

		fireThreadHandles(&ctx);
		if (ctx.state != RunContext::Running) {
			break;
		}
		fireAddressHandles(&ctx);
		if (ctx.state != RunContext::Running) {
			break;
		}
		fireExpired(&ctx);
		if (ctx.state != RunContext::Running) {
			break;
		}

		// A wakeup() or cancel() from another thread has to end this run, not
		// merely shorten the idle: drainWakeup() used to clear the request and
		// keep looping, so Looper::run() never returned on this platform - the
		// flag only ever reached spinWait()'s early exit. stopContext() is the
		// generic Signaled/Stopping/Stopped path every other backend reaches
		// through its own wakeup fd.
		const int32_t req = __atomic_exchange_n(&_wakeupReq, 0, __ATOMIC_SEQ_CST);
		if (req & WakeupStop) {
			WakeupFlags flags = (req & WakeupCancel)
					? WakeupFlags::None // cancel(): stop now, do not drain gracefully
					: WakeupFlags(
							  static_cast<uint32_t>(req) & static_cast<uint32_t>(WakeupFlags::All));
			if (hasFlag(flags, WakeupFlags::ContextDefault)) {
				flags = ctx.runWakeupFlags;
			}
			stopContext(&ctx, flags, true);
		}
		if (ctx.state != RunContext::Running) {
			break;
		}

		int64_t now = embox_now_ns();
		int64_t rel = embox_rel_timeout(ival, nearestDeadline(), now);
		if (rel == 0) {
			continue;
		}
		int timeoutMs;
		if (rel < 0) {
			timeoutMs = -1;
		} else {
			timeoutMs = static_cast<int>(rel / 1'000'000ll);
			if (rel > 0 && timeoutMs == 0) {
				timeoutMs = 1;
			}
		}
		// Embox poll()/nanosleep() from a pthread ignore their timeout. Cap the
		// idle stretch so posted performOnThread work is observed.
		if (timeoutMs < 0 || timeoutMs > 16) {
			timeoutMs = 16;
		}
		spinWait(timeoutMs);
	}

	popContext(&ctx);
	return ctx.wakeupStatus;
}

Status EmboxData::wakeup(WakeupFlags flags) {
	__atomic_store_n(&_wakeupReq, static_cast<int32_t>(toInt(flags)) | WakeupStop | WakeupPresent,
			__ATOMIC_SEQ_CST);
	notifyWakeup();
	return Status::Ok;
}

void EmboxData::cancel() {
	__atomic_store_n(&_wakeupReq, WakeupPresent | WakeupStop | WakeupCancel, __ATOMIC_SEQ_CST);
	notifyWakeup();
}

// --- EmboxTimerHandle ---

bool EmboxTimerHandle::init(HandleClass *cl, TimerInfo &&info) {
	if (!Handle::init(cl, info.completion)) {
		return false;
	}
	auto *src = reinterpret_cast<EmboxTimerSource *>(_data);
	src->firstTimeout = static_cast<int64_t>(info.timeout.toMicroseconds()) * 1'000;
	src->interval = static_cast<int64_t>(info.interval.toMicroseconds()) * 1'000;
	src->count = info.count;
	src->value = 0;
	src->deadline = 0;
	return true;
}

bool EmboxTimerHandle::reset(TimerInfo &&info) {
	auto *src = reinterpret_cast<EmboxTimerSource *>(_data);
	src->firstTimeout = static_cast<int64_t>(info.timeout.toMicroseconds()) * 1'000;
	src->interval = static_cast<int64_t>(info.interval.toMicroseconds()) * 1'000;
	src->count = info.count;
	return true;
}

Status EmboxTimerHandle::rearm(EmboxData *reactor, EmboxTimerSource *src) {
	int64_t now = reactor->nowMonotonic();
	int64_t delay = src->firstTimeout > 0 ? src->firstTimeout
										  : (src->interval > 0 ? src->interval : 16'000'000ll);
	src->deadline = now + delay;
	reactor->pushTimer(src->deadline, this);
	return Status::Ok;
}

Status EmboxTimerHandle::disarm(EmboxData *reactor, EmboxTimerSource *src) {
	reactor->removeTimer(this);
	return Status::Ok;
}

void EmboxTimerHandle::notify(EmboxData *reactor, EmboxTimerSource *src, const NotifyData &nd) {
	if (_status != Status::Ok) {
		return;
	}
	if (!src) {
		src = reinterpret_cast<EmboxTimerSource *>(_data);
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

		if (_status != Status::Ok) {
			return;
		}
		src->deadline += static_cast<int64_t>(fired) * src->interval;
		reactor->pushTimer(src->deadline, this);
	}
}

bool EmboxThreadHandle::init(HandleClass *cl) { return ThreadHandle::init(cl); }

bool EmboxThreadHandle::takePending() {
	auto s = reinterpret_cast<EmboxThreadSource *>(_data);
	return __atomic_exchange_n(&s->pending, 0, __ATOMIC_SEQ_CST) != 0;
}

void EmboxThreadHandle::signal() {
	auto s = reinterpret_cast<EmboxThreadSource *>(_data);
	__atomic_store_n(&s->pending, 1, __ATOMIC_SEQ_CST);
	if (_embox) {
		_embox->notifyWakeup();
	}
}

Status EmboxThreadHandle::rearm(EmboxData *n, EmboxThreadSource *s) {
	auto status = prepareRearm();
	if (status == Status::Ok) {
		_embox = n;
		// Do not clear `pending`: performOnThread may have queued work before the
		// handle was armed (Looper ctor parks it in _pendingHandles until run()).
		n->registerThreadHandle(this);
	}
	return status;
}

Status EmboxThreadHandle::disarm(EmboxData *n, EmboxThreadSource *s) {
	auto status = prepareDisarm();
	if (status == Status::Ok) {
		n->unregisterThreadHandle(this);
	}
	return status;
}

void EmboxThreadHandle::notify(EmboxData *, EmboxThreadSource *, const NotifyData &) {
	if (_status != Status::Ok) {
		return;
	}
	_mutex.lock();
	performAll([&](uint32_t) { _mutex.unlock(); });
}

Status EmboxThreadHandle::perform(Rc<Task> &&task) {
	{
		sprt::unique_lock lock(_mutex);
		_outputQueue.emplace_back(move(task));
	}
	signal();
	return Status::Ok;
}

Status EmboxThreadHandle::perform(dispatch::Function<void()> &&func, Ref *target, StringView tag) {
	{
		sprt::unique_lock lock(_mutex);
		_outputCallbacks.emplace_back(CallbackInfo{sprt::move(func), target, tag});
	}
	signal();
	return Status::Ok;
}

bool EmboxAddressWaitHandle::init(HandleClass *cl, AddressWaitInfo &&info) {
	if (!AddressWaitHandle::init(cl, move(info.completion))) {
		return false;
	}
	_address = info.address;
	_last = info.expected;
	return true;
}

Status EmboxAddressWaitHandle::rearm(EmboxData *n, EmboxAddressWaitSource *) {
	auto status = prepareRearm();
	if (status == Status::Ok) {
		if (!n->registerAddressHandle(this)) {
			_status = Status::Suspended;
			return Status::ErrorOutOfHostMemory;
		}
		// A change before arming is reported on the next loop pass, not lost.
		n->notifyWakeup();
	}
	return status;
}

Status EmboxAddressWaitHandle::disarm(EmboxData *n, EmboxAddressWaitSource *) {
	auto status = prepareDisarm();
	if (status == Status::Ok) {
		n->unregisterAddressHandle(this);
	} else if (status == Status::ErrorAlreadyPerformed) {
		return Status::Ok;
	}
	return status;
}

void EmboxAddressWaitHandle::notify(EmboxData *, EmboxAddressWaitSource *, const NotifyData &nd) {
	if (_status != Status::Ok) {
		return;
	}
	auto value = uint32_t(nd.result);
	if (value != _last) {
		_last = value;
		sendCompletion(value, Status::Ok);
	}
}

// --- Queue::Data ---

Queue::Data::Data(QueueRef *q, const QueueInfo &info) : QueueData(q, info.flags) {
	setupEmboxHandleClass<EmboxTimerHandle, EmboxTimerSource>(&_info, &_emboxTimerClass, true);
	setupEmboxHandleClass<EmboxThreadHandle, EmboxThreadSource>(&_info, &_emboxThreadClass, true);
	setupEmboxHandleClass<EmboxAddressWaitHandle, EmboxAddressWaitSource>(&_info,
			&_emboxAddressWaitClass, true);
	setupInlineFileHandleClass(&_info, &_emboxFileInlineClass);
	setupStatWatchClass(&_info, &_emboxWatchClass);

	auto *platform = new (memory::pool::acquire()) EmboxData(q, this, info);
	_platformQueue = platform;
	_engine = QueueEngine::None;

	_submit = [](void *ptr) -> Status { return static_cast<EmboxData *>(ptr)->submit(); };
	_poll = [](void *ptr) -> uint32_t { return static_cast<EmboxData *>(ptr)->poll(); };
	_wait = [](void *ptr, TimeInterval ival) -> uint32_t {
		return static_cast<EmboxData *>(ptr)->wait(ival);
	};
	_run = [](void *ptr, TimeInterval ival, QueueWakeupInfo &&winfo) -> Status {
		return static_cast<EmboxData *>(ptr)->run(ival, move(winfo));
	};
	_wakeup = [](void *ptr, WakeupFlags flags) -> Status {
		return static_cast<EmboxData *>(ptr)->wakeup(flags);
	};
	_cancel = [](void *ptr) { static_cast<EmboxData *>(ptr)->cancel(); };
	_shutdown = [](void *ptr) { static_cast<EmboxData *>(ptr)->cancel(); };

	_timer = [](QueueData *d, void *ptr, TimerInfo &&tinfo) -> Rc<TimerHandle> {
		auto data = static_cast<Queue::Data *>(d);
		return Rc<EmboxTimerHandle>::create(&data->_emboxTimerClass, move(tinfo));
	};
	_thread = [](QueueData *d, void *ptr) -> Rc<ThreadHandle> {
		auto data = static_cast<Queue::Data *>(d);
		return Rc<EmboxThreadHandle>::create(&data->_emboxThreadClass);
	};
	_addressWait = [](QueueData *d, void *ptr, AddressWaitInfo &&info,
						   Ref *ref) -> Rc<AddressWaitHandle> {
		auto data = static_cast<Queue::Data *>(d);
		auto h = Rc<EmboxAddressWaitHandle>::create(&data->_emboxAddressWaitClass, move(info));
		if (h && ref) {
			h->setUserdata(ref);
		}
		return h;
	};

	_makeFileHandle = [](QueueData *d, void *ptr, Rc<FileState> &&state) -> Rc<FileHandle> {
		auto data = static_cast<Queue::Data *>(d);
		return makeFileInlineHandle(d, &data->_emboxFileInlineClass, sprt::move(state));
	};

	// No change notifications either: the portable stat-polling watch.
	_watchFile = [](QueueData *d, void *ptr, WatchInfo &&info, Ref *ref) -> Rc<WatchHandle> {
		auto data = static_cast<Queue::Data *>(d);
		return makeStatWatchHandle(d, &data->_emboxWatchClass, sprt::move(info), ref);
	};
}

} // namespace sprt::dispatch

namespace sprt::dispatch::platform {

// Per-thread dispatch queue accessor (mirrors the linux/darwin/wasm/windows
// SPEvent-*.cc siblings). Looper::acquire calls this to obtain the queue the
// looper drives. Embox, like wasm, has no GPU/engine acceleration in the flat
// image (the soft rasterizer is the renderer), so the engine mask is None —
// matching what Queue::Data::Data above sets on _engine.
Rc<QueueRef> getThreadQueue(QueueInfo &&info) {
	info.engineMask = QueueEngine::None;
	return Queue::create(move(info));
}

} // namespace sprt::dispatch::platform
