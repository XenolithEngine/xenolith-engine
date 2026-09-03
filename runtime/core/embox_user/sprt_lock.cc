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

// Embox EL0 futex-style lock backend.
//
// futex(98) is M2/K6, so an EL0 thread has no way to block on a word. Wait polls
// the word; wake is a no-op because the waiter re-checks it. Exactly the shape of
// the hosted Embox backend (core/embox/sprt_lock.cc), which reached the same
// place from the other direction: Embox has no futex(2) at all.
//
// One difference, and it is the reason this is a separate file rather than a
// reuse: the hosted backend's wait tick sleeps for a millisecond, which really
// deschedules. Here it cannot -- nanosleep is itself a spin (libc_impl
// embox_user/time.cc) -- so descheduling would be a lie dressed as a syscall.
// The tick is a `yield` hint, and the kernel's timer interrupt does the
// preempting regardless of what EL0 executes.
//
// K6 replaces this whole file with real futex waits.

#ifndef __SPRT_BUILD
#define __SPRT_BUILD 1
#endif

#include <sprt/c/sys/__sprt_sprt.h>
#include <sprt/c/sys/__sprt_futex.h>
#include <sprt/c/cross/__sprt_sysid.h>
#include <sprt/c/__sprt_errno.h>

#include <sprt/c/__sprt_time.h>

namespace sprt {

// MONOTONIC rather than the hosted backend's REALTIME: a wall-clock step must
// not turn a bounded wait into an unbounded one, and on a board whose clock is
// set from the network after boot that step really happens. The FLAG_CLOCK_REALTIME
// selector further down is a different thing -- it is what the CALLER asked for,
// and it is reported unchanged.
static int toAbsTimeout(__SPRT_ID(sprt_timeout_t) timeout, struct __SPRT_TIMESPEC_NAME &ts) {
	if (timeout == __SPRT_SPRT_TIMEOUT_INFINITE) {
		return 0;
	}
	struct __SPRT_TIMESPEC_NAME now;
	__sprt_clock_gettime(__SPRT_CLOCK_MONOTONIC, &now);
	uint64_t ns = static_cast<uint64_t>(now.tv_sec) * 1000000000ull + now.tv_nsec + timeout;
	ts.tv_sec = ns / 1000000000ull;
	ts.tv_nsec = ns % 1000000000ull;
	return 1;
}

static bool deadlineExpired(const struct __SPRT_TIMESPEC_NAME &ts) {
	struct __SPRT_TIMESPEC_NAME now;
	__sprt_clock_gettime(__SPRT_CLOCK_MONOTONIC, &now);
	return now.tv_sec > ts.tv_sec || (now.tv_sec == ts.tv_sec && now.tv_nsec >= ts.tv_nsec);
}

static void emboxWaitTick() {
	// No syscall to make here: sched_yield(124) and futex(98) are both M2. The
	// timer interrupt preempts this thread on its own schedule, so a `yield` hint
	// is the whole of what EL0 can contribute.
	__asm__ __volatile__("yield" ::: "memory");
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
	struct __SPRT_TIMESPEC_NAME ts;
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

static int sprt_qlock_wake_one(__SPRT_ID(sprt_qlock_t) *, __SPRT_ID(sprt_lock_flags_t)) {
	return 0;
}

static int sprt_qlock_wake_all(__SPRT_ID(sprt_qlock_t) *, __SPRT_ID(sprt_lock_flags_t)) {
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
	struct __SPRT_TIMESPEC_NAME ts;
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

static int sprt_rlock_wake(__SPRT_ID(sprt_rlock_t) *, __SPRT_ID(sprt_lock_flags_t)) { return 0; }

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
