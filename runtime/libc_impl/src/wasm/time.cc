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

// WebAssembly time backend (wasm-port-draft.adoc §3.5).
//
// nanosleep / clock_nanosleep block on a private futex cell with a timeout — no
// host import, no busy loop (LOCAL). This requires shared linear memory (the
// memory.atomic.wait instruction traps otherwise); a single-threaded / main-thread
// build routes sleeping through the spin fallback in a later milestone.
//
// localtime_r currently treats local time as UTC (offset 0); DST-aware local time
// via the tz_offset/tz_name host imports is a later milestone (see §3.5). This
// unit also brings the public libc time/errno names into scope for the generic
// builtin_time.cpp body, exactly like windows/time.cc.

#include <time.h>
#include <errno.h>

#include "../../include/__impl_libc.h"

namespace sprt {

// Block for the given duration by waiting on a private zero word that is never
// notified, so the wait always runs to its timeout.
static void __wasm_futex_sleep_ns(long long ns) {
	if (ns <= 0) {
		return;
	}
	int cell = 0;
	__builtin_wasm_memory_atomic_wait32(&cell, 0, ns);
}

__SPRT_C_FUNC int nanosleep(const struct __SPRT_TIMESPEC_NAME *req,
		struct __SPRT_TIMESPEC_NAME *rem) __SPRT_NOEXCEPT {
	if (!req) {
		__sprt_errno = EFAULT;
		return -1;
	}
	if (req->tv_sec < 0 || req->tv_nsec < 0 || req->tv_nsec >= 1'000'000'000L) {
		__sprt_errno = EINVAL;
		return -1;
	}
	__wasm_futex_sleep_ns((long long)req->tv_sec * 1'000'000'000LL + req->tv_nsec);
	if (rem) {
		rem->tv_sec = 0;
		rem->tv_nsec = 0;
	}
	return 0;
}

__SPRT_C_FUNC int clock_nanosleep(__SPRT_ID(clockid_t) clock, int flags,
		const __SPRT_TIMESPEC_NAME *req, __SPRT_TIMESPEC_NAME *rem) __SPRT_NOEXCEPT {
	if (!req) {
		__sprt_errno = EFAULT;
		return -1;
	}
	if (req->tv_sec < 0 || req->tv_nsec < 0 || req->tv_nsec >= 1'000'000'000L) {
		__sprt_errno = EINVAL;
		return -1;
	}

	long long ns;
	if (flags & __SPRT_TIMER_ABSTIME) {
		// Sleep until the absolute deadline elapses on the given clock.
		__SPRT_TIMESPEC_NAME now;
		__sprt_clock_gettime(clock, &now);
		auto diff = __sprt_timespec_diff(req, &now);
		if (diff.tv_sec < 0) {
			return 0; // deadline already in the past
		}
		ns = (long long)diff.tv_sec * 1'000'000'000LL + diff.tv_nsec;
	} else {
		ns = (long long)req->tv_sec * 1'000'000'000LL + req->tv_nsec;
	}
	__wasm_futex_sleep_ns(ns);
	if (rem && !(flags & __SPRT_TIMER_ABSTIME)) {
		rem->tv_sec = 0;
		rem->tv_nsec = 0;
	}
	return 0;
}

__SPRT_C_FUNC struct tm *localtime_r(const time_t *__restrict t,
		struct tm *__restrict tm) __SPRT_NOEXCEPT {
	// Local time == UTC for now (no tz host import wired yet).
	if (gmtime_r(t, tm)) {
		tm->tm_usec = 0;
		tm->tm_gmtoff = 0;
		tm->tm_isdst = 0;
		tm->tm_gmt_type = __sprt_gmt_local;
		tm->tm_zone = __utc;
		return tm;
	}
	return nullptr;
}

} // namespace sprt
