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

// The lock backend of the runtime on Tau: the module's private futex (sprt_tau.h). Waits
// take relative timeouts; a spurious wake, a changed value and an expired wait all come
// back as 0, and the lock code looks at its word again.

#ifndef __SPRT_BUILD
#define __SPRT_BUILD 1
#endif

#include <sprt/c/sys/__sprt_sprt.h>
#include <sprt/c/cross/__sprt_sysid.h>
#include <sprt/c/cross/tau_sprt/sprt_tau.h>
#include <sprt/c/__sprt_errno.h>
#include <sprt/c/__sprt_limits.h>
#include <sprt/c/__sprt_time.h>

namespace sprt {

static int tauFutexResult(int res) {
	if (res == 0 || res == -EAGAIN || res == -EINTR || res == -ETIMEDOUT) {
		return 0;
	}
	__sprt_errno = -res;
	return -1;
}

static int tauFutexWait(void *value, __SPRT_ID(uint32_t) expected, __SPRT_ID(sprt_timeout_t) timeout) {
	return __sprt_tau_futex_wait(static_cast<volatile unsigned *>(value), expected,
			timeout == __SPRT_SPRT_TIMEOUT_INFINITE ? __SPRT_TAU_TIMEOUT_INFINITE : timeout);
}

static int sprt_qlock_supports(__SPRT_ID(sprt_lock_flags_t) flags) { return flags == 0 ? 0 : -1; }

static int sprt_qlock_wait(__SPRT_ID(sprt_qlock_t) * value, __SPRT_ID(sprt_qlock_t) expected,
		__SPRT_ID(sprt_timeout_t) timeout, __SPRT_ID(sprt_lock_flags_t)) {
	return tauFutexResult(tauFutexWait(value, expected, timeout));
}

static int sprt_qlock_wake_one(__SPRT_ID(sprt_qlock_t) * value, __SPRT_ID(sprt_lock_flags_t)) {
	__sprt_tau_futex_wake(reinterpret_cast<volatile unsigned *>(value), 1);
	return 0;
}

static int sprt_qlock_wake_all(__SPRT_ID(sprt_qlock_t) * value, __SPRT_ID(sprt_lock_flags_t)) {
	__sprt_tau_futex_wake(reinterpret_cast<volatile unsigned *>(value), __SPRT_INT_MAX);
	return 0;
}

static int sprt_rlock_supports(__SPRT_ID(sprt_lock_flags_t) flags) { return flags == 0 ? 0 : -1; }

static int sprt_rlock_wait(__SPRT_ID(sprt_rlock_t) * value, __SPRT_ID(sprt_rlock_t) * expected,
		__SPRT_ID(sprt_timeout_t) timeout, __SPRT_ID(sprt_lock_flags_t)) {
	return tauFutexResult(tauFutexWait(&value->u32_2, expected->u32_2, timeout));
}

static int sprt_rlock_try_wait(__SPRT_ID(sprt_rlock_t) *, __SPRT_ID(sprt_lock_flags_t)) {
	__sprt_errno = EBUSY;
	return -1;
}

static int sprt_rlock_wake(__SPRT_ID(sprt_rlock_t) * value, __SPRT_ID(sprt_lock_flags_t)) {
	__atomic_store_n(&value->u32_2, (__SPRT_ID(uint32_t))0, __ATOMIC_SEQ_CST);
	__sprt_tau_futex_wake(reinterpret_cast<volatile unsigned *>(&value->u32_2), 1);
	return 0;
}

// Tau has one clock, the counter since boot.
static __SPRT_ID(clockid_t) sprt_qlock_getclock(__SPRT_ID(sprt_lock_flags_t)) { return __SPRT_CLOCK_MONOTONIC; }

static __SPRT_ID(clockid_t) sprt_rlock_getclock(__SPRT_ID(sprt_lock_flags_t)) { return __SPRT_CLOCK_MONOTONIC; }

} // namespace sprt
