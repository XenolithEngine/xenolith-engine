/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
    10|    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
**/

// Embox futex-style lock backend.
//
// Embox has no futex(2); the Xenolith image carries one of its own
// (xenolith-os board/embox-qemu/drivers/xlfutex): xl_futex_wait() and
// xl_futex_wake(), which the application reaches directly because it is linked
// into the kernel image. They sleep on the calling thread's schedee and wake it
// by schedee, so they work from the init TASK as well as from a pthread --
// which is the case that sank the first backend: it emulated wait/wake with
// pthread_cond, a wait from a task never woke, and AppThread::run() sat forever
// in thread_t::create's InternalInit wait (qtimeline -> qlock_wait).
//
// The two are declared weak. An image without them -- the Pi 4 today -- links
// with both null and gets the poll below, and so does any image run with
// SPRT_EMBOX_FUTEX=0 in the environment: the fallback is the A/B for measuring
// what the futex bought, and it must keep working.
//
// The poll: re-check the word and usleep(1ms). sched_yield from the init TASK
// does not run a newly created pthread (FIFO / higher-priority init), so it has
// to be a real sleep. Wake is a no-op there: the waiter re-checks the word.

#ifndef __SPRT_BUILD
#define __SPRT_BUILD 1
#endif

#include <sprt/c/sys/__sprt_sprt.h>
#include <sprt/c/sys/__sprt_futex.h>
#include <sprt/c/cross/__sprt_sysid.h>
#include <sprt/c/__sprt_errno.h>

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// xenolith-os drivers/xlfutex/xl_futex.h. Negative Embox errno on failure:
// -EAGAIN (the word changed), -ETIMEDOUT, -EINTR; the wait answers 0 when woken.
extern "C" int xl_futex_wait(void *addr, unsigned size, uint64_t expected, int64_t timeout_ns)
		__attribute__((weak));
extern "C" int xl_futex_wake(void *addr, int nr) __attribute__((weak));

namespace sprt {

// Whether the image has the futex, and whether the environment asked for the
// poll instead. Cached in a plain word and NOT in a function-local static: the
// static's guard is itself a lock, and on Embox that lock waits through this
// very backend -- two threads racing to initialise it recursed into the guard
// and aborted the kiosk at start ("__cxa_guard_acquire detected recursive
// initialization"). Two threads computing the answer at once agree on it.
static int emboxFutexState = 0; // 0 unknown, 1 futex, 2 poll

static bool emboxHasFutex() {
	int state = __atomic_load_n(&emboxFutexState, __ATOMIC_RELAXED);
	if (state == 0) {
		bool futex = xl_futex_wait && xl_futex_wake;
		if (futex) {
			// "0" polls everywhere; "looper" keeps the futex for the dispatch
			// looper only (SPEvent-embox.cc) and polls here. Anything else: futex.
			auto env = ::getenv("SPRT_EMBOX_FUTEX");
			futex = !(env && (::strcmp(env, "0") == 0 || ::strcmp(env, "looper") == 0));
		}
		state = futex ? 1 : 2;
		__atomic_store_n(&emboxFutexState, state, __ATOMIC_RELAXED);
	}
	return state == 1;
}

static int64_t emboxFutexTimeout(__SPRT_ID(sprt_timeout_t) timeout) {
	return (timeout > static_cast<__SPRT_ID(sprt_timeout_t)>(INT64_MAX)) ? -1
																		  : static_cast<int64_t>(timeout);
}

// The futex answer in this backend's contract: 0, or -1 with errno.
static int emboxFutexResult(int res) {
	if (res == 0) {
		return 0;
	}
	__sprt_errno = -res;
	return -1;
}

static int toAbsTimeout(__SPRT_ID(sprt_timeout_t) timeout, struct timespec &ts) {
	if (timeout == __SPRT_SPRT_TIMEOUT_INFINITE) {
		return 0;
	}
	struct timespec now;
	clock_gettime(CLOCK_REALTIME, &now);
	uint64_t ns = static_cast<uint64_t>(now.tv_sec) * 1000000000ull + now.tv_nsec + timeout;
	ts.tv_sec = ns / 1000000000ull;
	ts.tv_nsec = ns % 1000000000ull;
	return 1;
}

static bool deadlineExpired(const struct timespec &ts) {
	struct timespec now;
	clock_gettime(CLOCK_REALTIME, &now);
	return now.tv_sec > ts.tv_sec || (now.tv_sec == ts.tv_sec && now.tv_nsec >= ts.tv_nsec);
}

static void emboxWaitTick() {
	// sched_yield() from the high-priority init TASK is a no-op if the waiter
	// is FIFO or nobody of equal priority is ready. usleep actually deschedules
	// so a newly created pthread (AppThread) can run and signal the word.
	::usleep(1000);
}

static int sprt_qlock_supports(__SPRT_ID(sprt_lock_flags_t) flags) {
	if (flags == 0) {
		return 0;
	}
	return -1;
}

static int sprt_qlock_wait(__SPRT_ID(sprt_qlock_t) * value, __SPRT_ID(sprt_qlock_t) expected,
		__SPRT_ID(sprt_timeout_t) timeout, __SPRT_ID(sprt_lock_flags_t) flags) {
	(void)flags;
	if (emboxHasFutex()) {
		return emboxFutexResult(xl_futex_wait(value, 4, expected, emboxFutexTimeout(timeout)));
	}

	struct timespec ts;
	int hasDeadline = toAbsTimeout(timeout, ts);

	while (__atomic_load_n(value, __ATOMIC_SEQ_CST) == expected) {
		if (hasDeadline && deadlineExpired(ts)) {
			__sprt_errno = ETIMEDOUT;
			return -1;
		}
		emboxWaitTick();
	}
	return 0;
}

static int sprt_qlock_wake_one(__SPRT_ID(sprt_qlock_t) * value, __SPRT_ID(sprt_lock_flags_t)) {
	if (emboxHasFutex()) {
		xl_futex_wake(value, 1);
	}
	return 0;
}

static int sprt_qlock_wake_all(__SPRT_ID(sprt_qlock_t) * value, __SPRT_ID(sprt_lock_flags_t)) {
	if (emboxHasFutex()) {
		xl_futex_wake(value, INT_MAX);
	}
	return 0;
}

static int sprt_rlock_supports(__SPRT_ID(sprt_lock_flags_t) flags) {
	if (flags == 0) {
		return 0;
	}
	return -1;
}

static int sprt_rlock_wait(__SPRT_ID(sprt_rlock_t) * value, __SPRT_ID(sprt_rlock_t) * expected,
		__SPRT_ID(sprt_timeout_t) timeout, __SPRT_ID(sprt_lock_flags_t) flags) {
	(void)flags;
	if (emboxHasFutex()) {
		// All eight bytes: the waiters bit of a non-Linux rmutex is in the upper
		// half, and a wait that compared only the owner's tid would sleep through
		// an unlock-and-relock by the same owner.
		return emboxFutexResult(
				xl_futex_wait(&value->u64, 8, expected->u64, emboxFutexTimeout(timeout)));
	}

	struct timespec ts;
	int hasDeadline = toAbsTimeout(timeout, ts);

	while (__atomic_load_n(&value->u64, __ATOMIC_SEQ_CST) == expected->u64) {
		if (hasDeadline && deadlineExpired(ts)) {
			__sprt_errno = ETIMEDOUT;
			return -1;
		}
		emboxWaitTick();
	}
	return 0;
}

static int sprt_rlock_try_wait(__SPRT_ID(sprt_rlock_t) *, __SPRT_ID(sprt_lock_flags_t)) {
	__sprt_errno = EBUSY;
	return -1;
}

static int sprt_rlock_wake(__SPRT_ID(sprt_rlock_t) * value, __SPRT_ID(sprt_lock_flags_t)) {
	if (emboxHasFutex()) {
		// As the Linux and Darwin backends do: the word is released before the
		// wake, so the woken thread finds it free.
		__atomic_store_n(&value->u64, uint64_t(0), __ATOMIC_SEQ_CST);
		xl_futex_wake(&value->u64, 1);
	}
	return 0;
}

static __SPRT_ID(clockid_t) sprt_qlock_getclock(__SPRT_ID(sprt_lock_flags_t) flags) {
	if (hasFlag(flags, __SPRT_ID(sprt_lock_flags_t)(__SPRT_SPRT_LOCK_FLAG_CLOCK_REALTIME))) {
		return __SPRT_CLOCK_REALTIME;
	} else {
		return __SPRT_CLOCK_MONOTONIC;
	}
}

static __SPRT_ID(clockid_t) sprt_rlock_getclock(__SPRT_ID(sprt_lock_flags_t) flags) {
	if (hasFlag(flags, __SPRT_ID(sprt_lock_flags_t)(__SPRT_SPRT_LOCK_FLAG_CLOCK_REALTIME))) {
		return __SPRT_CLOCK_REALTIME;
	} else {
		return __SPRT_CLOCK_MONOTONIC;
	}
}

} // namespace sprt
