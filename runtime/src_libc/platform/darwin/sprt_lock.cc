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

#include <os/os_sync_wait_on_address.h>

namespace sprt {

static int sprt_qlock_supports(__SPRT_ID(sprt_lock_flags_t) flags) {
	// Shared locks supported with OS_SYNC_WAIT_ON_ADDRESS_SHARED / OS_SYNC_WAKE_BY_ADDRESS_SHARED
	if ((flags & ~__SPRT_SPRT_LOCK_FLAG_SHARED) == 0) {
		return 0;
	}
	return -1;
}

static int sprt_qlock_wait(__SPRT_ID(sprt_qlock_t) * value, __SPRT_ID(sprt_qlock_t) expected,
		__SPRT_ID(sprt_timeout_t) timeout, __SPRT_ID(sprt_lock_flags_t) flags) {
	int result = 0;
	os_sync_wait_on_address_flags_t _flags = OS_SYNC_WAIT_ON_ADDRESS_NONE;
	if (hasFlag(flags, __SPRT_ID(sprt_lock_flags_t)(__SPRT_SPRT_LOCK_FLAG_SHARED))) {
		_flags = os_sync_wait_on_address_flags_t(_flags | OS_SYNC_WAIT_ON_ADDRESS_SHARED);
	}
	if (timeout == __SPRT_SPRT_TIMEOUT_INFINITE) {
		result = os_sync_wait_on_address((void *)value, expected, sizeof(uint32_t), _flags);
	} else {
		result = os_sync_wait_on_address_with_timeout((void *)value, expected, sizeof(uint32_t),
				_flags, OS_CLOCK_MACH_ABSOLUTE_TIME, timeout);
	}
	return result;
}

static int sprt_qlock_wake_one(__SPRT_ID(sprt_qlock_t) * value,
		__SPRT_ID(sprt_lock_flags_t) flags) {
	int result = 0;
	os_sync_wake_by_address_flags_t _flags = OS_SYNC_WAKE_BY_ADDRESS_NONE;
	if (hasFlag(flags, __SPRT_ID(sprt_lock_flags_t)(__SPRT_SPRT_LOCK_FLAG_SHARED))) {
		_flags = os_sync_wake_by_address_flags_t(_flags | OS_SYNC_WAKE_BY_ADDRESS_SHARED);
	}
	result = os_sync_wake_by_address_any((void *)value, sizeof(uint32_t), _flags);
	return result;
}

static int sprt_qlock_wake_all(__SPRT_ID(sprt_qlock_t) * value,
		__SPRT_ID(sprt_lock_flags_t) flags) {
	int result = 0;
	os_sync_wake_by_address_flags_t _flags = OS_SYNC_WAKE_BY_ADDRESS_NONE;
	if (hasFlag(flags, __SPRT_ID(sprt_lock_flags_t)(__SPRT_SPRT_LOCK_FLAG_SHARED))) {
		_flags = os_sync_wake_by_address_flags_t(_flags | OS_SYNC_WAKE_BY_ADDRESS_SHARED);
	}
	result = os_sync_wake_by_address_all((void *)value, sizeof(uint32_t), _flags);
	return result;
}

static int sprt_rlock_supports(__SPRT_ID(sprt_lock_flags_t) flags) {
	// Shared locks supported with OS_SYNC_WAIT_ON_ADDRESS_SHARED / OS_SYNC_WAKE_BY_ADDRESS_SHARED
	if ((flags & ~__SPRT_SPRT_LOCK_FLAG_SHARED) == 0) {
		return 0;
	}
	return -1;
}

static int sprt_rlock_wait(__SPRT_ID(sprt_rlock_t) * value, __SPRT_ID(sprt_rlock_t) * expected,
		__SPRT_ID(sprt_timeout_t) timeout, __SPRT_ID(sprt_lock_flags_t) flags) {
	os_sync_wait_on_address_flags_t _flags = OS_SYNC_WAIT_ON_ADDRESS_NONE;
	if (hasFlag(flags, __SPRT_ID(sprt_lock_flags_t)(__SPRT_SPRT_LOCK_FLAG_SHARED))) {
		_flags = os_sync_wait_on_address_flags_t(_flags | OS_SYNC_WAIT_ON_ADDRESS_SHARED);
	}
	if (timeout == __SPRT_SPRT_TIMEOUT_INFINITE) {
		return os_sync_wait_on_address((void *)&value->u64, expected->u64, sizeof(uint64_t),
				_flags);
	} else {
		return os_sync_wait_on_address_with_timeout((void *)&value->u64, expected->u64,
				sizeof(uint64_t), _flags, OS_CLOCK_MACH_ABSOLUTE_TIME, timeout);
	}
	return 0;
}

static int sprt_rlock_try_wait(__SPRT_ID(sprt_rlock_t) * value,
		__SPRT_ID(sprt_lock_flags_t) flags) {
	return 0;
}

static int sprt_rlock_wake(__SPRT_ID(sprt_rlock_t) * value, __SPRT_ID(sprt_lock_flags_t) flags) {
	os_sync_wake_by_address_flags_t _flags = OS_SYNC_WAKE_BY_ADDRESS_NONE;
	if (hasFlag(flags, __SPRT_ID(sprt_lock_flags_t)(__SPRT_SPRT_LOCK_FLAG_SHARED))) {
		_flags = os_sync_wake_by_address_flags_t(_flags | OS_SYNC_WAKE_BY_ADDRESS_SHARED);
	}
	_atomic::storeSeq(&value->u64, uint64_t(0));
	return os_sync_wake_by_address_any((void *)&value->u64, sizeof(uint64_t), _flags);
}

static __SPRT_ID(clockid_t) sprt_qlock_getclock(__SPRT_ID(sprt_lock_flags_t) flags) {
	return __SPRT_CLOCK_UPTIME_RAW;
}

static __SPRT_ID(clockid_t) sprt_rlock_getclock(__SPRT_ID(sprt_lock_flags_t) flags) {
	return __SPRT_CLOCK_UPTIME_RAW;
}

} // namespace sprt
