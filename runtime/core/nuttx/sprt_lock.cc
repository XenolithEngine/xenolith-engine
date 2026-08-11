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

// NuttX lock backend — STUB for M3.
//
// NuttX has no futex: the wait/wake primitives the qlock/rlock contract wants
// have to be built on sem_t + pthread_cond_t, or on nxsem (the in-kernel
// semaphore available in flat build). The real implementation is M5 (it is
// exercised by tests/runtime threads tests, which need the rest of the Looper
// / pthread stack anyway, so building it now would be untestable).
//
// For M3 the goal is to get libsprt.a to link with NuttX-platform code paths
// resolving, so this stub returns "supported for process-private, no wait" —
// the same shape as a spin-only backend. A single-threaded flat build (the
// only thing M3/M4 produce) never reaches the blocking paths, and the wake
// primitives are no-ops because nothing ever blocks.
//
// When M5 lands this file gets the sem_t+cond wait/wake machinery the way
// linux/sprt_lock.cc has the futex one.

#ifndef __SPRT_BUILD
#define __SPRT_BUILD 1
#endif

#include <sprt/c/sys/__sprt_sprt.h>
#include <sprt/c/sys/__sprt_futex.h>
#include <sprt/c/cross/__sprt_sysid.h>
#include <sprt/c/__sprt_errno.h>

namespace sprt {

// Process-private only: NuttX flat build has a single address space, there is
// no cross-process shared-lock notion.
static int sprt_qlock_supports(__SPRT_ID(sprt_lock_flags_t) flags) {
	if (flags == 0) {
		return 0;
	}
	return -1;
}

// STUB: no waiter queue available yet. Return ETIMEDOUT immediately so callers
// that genuinely need to block fall through to their own retry/abort path
// rather than busy-waiting forever. Single-threaded M3/M4 builds never reach
// here.
static int sprt_qlock_wait(__SPRT_ID(sprt_qlock_t) * value, __SPRT_ID(sprt_qlock_t) expected,
		__SPRT_ID(sprt_timeout_t) timeout, __SPRT_ID(sprt_lock_flags_t) flags) {
	__sprt_errno = ETIMEDOUT;
	return -1;
}

static int sprt_qlock_wake_one(__SPRT_ID(sprt_qlock_t) * value,
		__SPRT_ID(sprt_lock_flags_t) flags) {
	return 0;
}

static int sprt_qlock_wake_all(__SPRT_ID(sprt_qlock_t) * value,
		__SPRT_ID(sprt_lock_flags_t) flags) {
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
	__sprt_errno = ETIMEDOUT;
	return -1;
}

static int sprt_rlock_try_wait(__SPRT_ID(sprt_rlock_t) * value,
		__SPRT_ID(sprt_lock_flags_t) flags) {
	__sprt_errno = EBUSY;
	return -1;
}

static int sprt_rlock_wake(__SPRT_ID(sprt_rlock_t) * value,
		__SPRT_ID(sprt_lock_flags_t) flags) {
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
