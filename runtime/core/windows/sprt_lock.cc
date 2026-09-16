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

#include <sprt/wrappers/windows/basic_api.h>

namespace sprt::platform {

int lastErrorToErrno(unsigned long winerr);

}

namespace sprt {

static int sprt_qlock_supports(__SPRT_ID(sprt_lock_flags_t) flags) {
	// Shared locks supported with OS_SYNC_WAIT_ON_ADDRESS_SHARED / OS_SYNC_WAKE_BY_ADDRESS_SHARED
	if (flags == 0) {
		return 0;
	}
	return -1;
}

static int sprt_qlock_wait(__SPRT_ID(sprt_qlock_t) * value, __SPRT_ID(sprt_qlock_t) expected,
		__SPRT_ID(sprt_timeout_t) timeout, __SPRT_ID(sprt_lock_flags_t) flags) {
	int result = 0;
	if (timeout == __SPRT_SPRT_TIMEOUT_INFINITE) {
		if (!WaitOnAddress(value, &expected, sizeof(uint32_t), INFINITE)) {
			result = -1;
			__sprt_errno = platform::lastErrorToErrno(GetLastError());
		}
	} else {
		// Round the ns timeout UP to whole milliseconds. WaitOnAddress takes a ms count,
		// and a timed wait must never return BEFORE the requested duration. Truncating
		// (timeout / 1e6) dropped the sub-ms remainder, so a wait of e.g. 249.9ms (what
		// condition_variable_any produces converting its absolute deadline back to a
		// relative one) ran ~1ms short: cv then saw now() < deadline and reported
		// no_timeout past the timeout, spinning its wait_for loop (wine exit=124).
		if (!WaitOnAddress(value, &expected, sizeof(uint32_t),
					DWORD((timeout + 999'999) / 1'000'000))) {
			result = -1;
			__sprt_errno = platform::lastErrorToErrno(GetLastError());
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
	// Return -1 on failure/timeout (mirrors sprt_qlock_wait): rmutex::_lock detects
	// errors/timeouts via WaitFn(...) != 0, so returning 0 here would swallow them and
	// spin. lastErrorToErrno maps ERROR_TIMEOUT/WAIT_TIMEOUT -> ETIMEDOUT.
	int result = 0;
	if (timeout == __SPRT_SPRT_TIMEOUT_INFINITE) {
		if (!WaitOnAddress(&value->u64, &expected->u64, sizeof(uint64_t), INFINITE)) {
			result = -1;
			__sprt_errno = platform::lastErrorToErrno(GetLastError());
		}
	} else {
		// Round ns up to whole ms (see sprt_qlock_wait): a timed wait must not return early.
		if (!WaitOnAddress(&value->u64, &expected->u64, sizeof(uint64_t),
					DWORD((timeout + 999'999) / 1'000'000))) {
			result = -1;
			__sprt_errno = platform::lastErrorToErrno(GetLastError());
		}
	}
	return result;
}

static int sprt_rlock_supports(__SPRT_ID(sprt_lock_flags_t) flags) {
	// Shared locks supported with OS_SYNC_WAIT_ON_ADDRESS_SHARED / OS_SYNC_WAKE_BY_ADDRESS_SHARED
	if (flags == 0) {
		return 0;
	}
	return -1;
}

static int sprt_rlock_try_wait(__SPRT_ID(sprt_rlock_t) * value,
		__SPRT_ID(sprt_lock_flags_t) flags) {
	// Windows has no kernel PI-futex trylock (FUTEX_TRYLOCK_PI). rmutex_base::_try_lock
	// only calls this AFTER its userspace CAS has already failed and the holder is a
	// DIFFERENT thread (the same-thread recursive case returns Propagate before reaching
	// here), and this callback isn't even given the caller's tid to write — so the lock
	// cannot be acquired. Report busy (EBUSY -> Status::ErrorBusy) so try_lock() returns
	// false. Returning 0 here falsely claimed acquisition, letting a second thread "lock" a
	// recursive_mutex already owned by another (thread.mutex.recursive lock/try_lock, wine).
	// A dead prior owner is handled separately by the robust-mutex cleanup (force_unlock
	// resets the futex word), so the next CAS succeeds without this path.
	__sprt_errno = EBUSY;
	return -1;
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
