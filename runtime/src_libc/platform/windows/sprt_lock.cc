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

#include <sprt/c/sys/__sprt_sprt.h>
#include <sprt/c/sys/__sprt_futex.h>
#include <sprt/c/cross/__sprt_sysid.h>
#include <sprt/c/__sprt_errno.h>

#include "private/SPRTSpecific.h"

namespace sprt {

static int sprt_qlock_wait(__SPRT_ID(sprt_qlock_t) * value, __SPRT_ID(sprt_qlock_t) expected,
		__SPRT_ID(sprt_timeout_t) timeout, __SPRT_ID(sprt_lock_flags_t) flags) {
	int result = 0;
	if (timeout == __SPRT_SPRT_TIMEOUT_INFINITE) {
		if (!WaitOnAddress(value, &expected, sizeof(uint32_t), INFINITE)) {
			result = -1;
			__sprt_errno = platform::lastErrorToErrno(_GetLastError());
		}
	} else {
		if (!WaitOnAddress(value, &expected, sizeof(uint32_t), timeout / 1'000'000)) {
			result = -1;
			__sprt_errno = platform::lastErrorToErrno(_GetLastError());
		}
	}
	return result;
}

static int sprt_qlock_wake_one(__SPRT_ID(sprt_qlock_t) * value,
		__SPRT_ID(sprt_lock_flags_t) flags) {
	int result = 0;
	WakeByAddressSingle((void *)value);
	return result;
}

static int sprt_qlock_wake_all(__SPRT_ID(sprt_qlock_t) * value,
		__SPRT_ID(sprt_lock_flags_t) flags) {
	int result = 0;
	WakeByAddressAll((void *)value);
	return result;
}

static int sprt_rlock_wait(__SPRT_ID(sprt_rlock_t) * value, __SPRT_ID(sprt_rlock_t) * expected,
		__SPRT_ID(sprt_timeout_t) timeout, __SPRT_ID(sprt_lock_flags_t) flags) {
	if (timeout == __SPRT_SPRT_TIMEOUT_INFINITE) {
		if (!WaitOnAddress(&value->u64, &expected->u64, sizeof(uint64_t), INFINITE)) {
			__sprt_errno = platform::lastErrorToErrno(_GetLastError());
		}
	} else {
		if (!WaitOnAddress(&value->u64, &expected->u64, sizeof(uint64_t), timeout / 1'000'000)) {
			__sprt_errno = platform::lastErrorToErrno(_GetLastError());
		}
	}
	return 0;
}

static int sprt_rlock_try_wait(__SPRT_ID(sprt_rlock_t) * value,
		__SPRT_ID(sprt_lock_flags_t) flags) {
	return 0;
}

static int sprt_rlock_wake(__SPRT_ID(sprt_rlock_t) * value, __SPRT_ID(sprt_lock_flags_t) flags) {
	_atomic::storeSeq(&value->u64, uint64_t(0));
	WakeByAddressSingle((void *)&value->u64);
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
