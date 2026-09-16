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

// Embox EL0 lock backend, on the kernel's futex.
//
// Until K6 this file polled: futex(98) did not exist, wake was a no-op and a
// waiter re-read the word after a `yield`. The kernel has a futex now
// (xenolith-os board/embox-qemu/drivers/xlfutex, reached from EL0 through
// syscall 98), so a wait is a wait: the thread sleeps until the word changes or
// the timeout passes, and it costs one trip through the scheduler rather than
// one per millisecond.
//
// The syscall compares FOUR bytes. That decides the recursive-mutex layout: on
// this target __rmutex_data uses the Linux 32-bit shape, whose waiters bit is
// inside the word the futex watches (sprt/runtime/thread/rmutex.h). The wide
// layout would put that bit in the upper half of a 64-bit word, and a wait that
// compared only the lower half would sleep through an unlock and relock by the
// same owner.
//
// Timeouts go to the kernel as relative nanoseconds; it refuses
// FUTEX_CLOCK_REALTIME, so a REALTIME condvar is still declined at set time
// (pthread_native_embox_user.cc, validate_condattr_setclock).

#ifndef __SPRT_BUILD
#define __SPRT_BUILD 1
#endif

#include <sprt/c/sys/__sprt_sprt.h>
#include <sprt/c/sys/__sprt_futex.h>
#include <sprt/c/cross/__sprt_sysid.h>
#include <sprt/c/__sprt_errno.h>

#include <sprt/c/__sprt_limits.h>
#include <sprt/c/__sprt_time.h>

#include "../include/__el0_syscall.h"

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

// The futex answer in this backend's contract: 0, or -1 with errno. The kernel
// returns a negated errno; EAGAIN (the word changed) and EINTR are both "look
// again", which every caller here does.
static int emboxFutexResult(long res) {
	if (res == 0) {
		return 0;
	}
	__sprt_errno = (int)-res;
	return -1;
}

static long emboxFutexWait(void *value, __SPRT_ID(uint32_t) expected,
		__SPRT_ID(sprt_timeout_t) timeout) {
	struct __SPRT_TIMESPEC_NAME ts;
	const void *uts = nullptr;

	if (timeout != __SPRT_SPRT_TIMEOUT_INFINITE) {
		ts.tv_sec = (long)(timeout / 1000000000ull);
		ts.tv_nsec = (long)(timeout % 1000000000ull);
		uts = &ts;
	}

	return __el0_futex(reinterpret_cast<__SPRT_ID(uint32_t) *>(value),
			__SPRT_FUTEX_WAIT | __SPRT_FUTEX_PRIVATE_FLAG, expected, uts, 0);
}

static long emboxFutexWake(void *value, int count) {
	return __el0_futex(reinterpret_cast<__SPRT_ID(uint32_t) *>(value),
			__SPRT_FUTEX_WAKE | __SPRT_FUTEX_PRIVATE_FLAG, (__SPRT_ID(uint32_t))count, nullptr,
			0);
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
	return emboxFutexResult(emboxFutexWait(value, expected, timeout));
}

static int sprt_qlock_wake_one(__SPRT_ID(sprt_qlock_t) * value, __SPRT_ID(sprt_lock_flags_t)) {
	emboxFutexWake(value, 1);
	return 0;
}

static int sprt_qlock_wake_all(__SPRT_ID(sprt_qlock_t) * value, __SPRT_ID(sprt_lock_flags_t)) {
	emboxFutexWake(value, __SPRT_INT_MAX);
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
	// u32_2, not u64: with the Linux layout that word holds the owner's tid and
	// the waiters bit, and it is the four bytes the kernel compares.
	return emboxFutexResult(emboxFutexWait(&value->u32_2, expected->u32_2, timeout));
}

static int sprt_rlock_try_wait(__SPRT_ID(sprt_rlock_t) *, __SPRT_ID(sprt_lock_flags_t)) {
	__sprt_errno = EBUSY;
	return -1;
}

static int sprt_rlock_wake(__SPRT_ID(sprt_rlock_t) * value, __SPRT_ID(sprt_lock_flags_t)) {
	// Released before the wake, as the Linux and Darwin backends do, so the
	// woken thread finds the lock free.
	__atomic_store_n(&value->u32_2, (__SPRT_ID(uint32_t))0, __ATOMIC_SEQ_CST);
	emboxFutexWake(&value->u32_2, 1);
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
