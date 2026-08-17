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

// NuttX futex-style lock backend.
//
// NuttX has no futex(2). The first implementation emulated wait/wake with
// pthread_cond. That deadlocks when CONFIG_INIT_ENTRYPOINT is a kernel
// task, not a pthread: pthread_cond_wait/signal from a task never wakes.
// AppThread::run() then sits forever in thread_t::create's InternalInit wait
// (qtimeline → qlock_wait) and the scene never presents.
//
// Wait polls the word and usleep(1ms). sched_yield from the init TASK does
// not run a newly created pthread (FIFO / higher-priority init). Wake is a
// no-op: the waiter re-checks the word.

#ifndef __SPRT_BUILD
#define __SPRT_BUILD 1
#endif

#include <sprt/c/sys/__sprt_sprt.h>
#include <sprt/c/sys/__sprt_futex.h>
#include <sprt/c/cross/__sprt_sysid.h>
#include <sprt/c/__sprt_errno.h>

#include <time.h>
#include <unistd.h>

namespace sprt {

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

static void nuttxWaitTick() {
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
	struct timespec ts;
	int hasDeadline = toAbsTimeout(timeout, ts);

	while (__atomic_load_n(value, __ATOMIC_SEQ_CST) == expected) {
		if (hasDeadline && deadlineExpired(ts)) {
			__sprt_errno = ETIMEDOUT;
			return -1;
		}
		nuttxWaitTick();
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
	struct timespec ts;
	int hasDeadline = toAbsTimeout(timeout, ts);

	while (__atomic_load_n(&value->u64, __ATOMIC_SEQ_CST) == expected->u64) {
		if (hasDeadline && deadlineExpired(ts)) {
			__sprt_errno = ETIMEDOUT;
			return -1;
		}
		nuttxWaitTick();
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
