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

#define __SPRT_BUILD

#include <sprt/cxx/__utility/common.h>
#include <sprt/cxx/atomic>
#include <sprt/c/sys/__sprt_sprt.h>
#include <sprt/c/sys/__sprt_futex.h>
#include <sprt/c/__sprt_time.h>
#include <sprt/c/__sprt_stdio.h>
#include <sprt/c/__sprt_errno.h>
#include <sprt/runtime/enum.h>
#include <sprt/runtime/thread/qonce.h>
#include <sprt/c/sys/__sprt_utsname.h>

#include "private/SPRTSpecific.h"

#if SPRT_LINUX || SPRT_ANDROID
#include <unistd.h>
#include <time.h>
#endif

namespace sprt {

#if SPRT_LINUX || SPRT_ANDROID
static bool isAbove5_14() {
	static sprt::qonce s_isAbove5_14Once;
	static bool s_isAbove5_14Value = false;

	s_isAbove5_14Once(
			[] { s_isAbove5_14Value = (__sprt_sprt_compare_os_kernel_version("5.14") >= 0); });

	return s_isAbove5_14Value;
}
#endif


__SPRT_C_FUNC int __SPRT_ID(sprt_qlock_supports)(__SPRT_ID(sprt_lock_flags_t) flags) {
	if (flags == 0) {
		return 0;
	}
#if SPRT_LINUX || SPRT_ANDROID
	// SPRT supports 5.10+ kernels, __SPRT_FUTEX_CLOCK_REALTIME requires 5.4 for FUTEX_WAIT
	if ((flags & ~(__SPRT_SPRT_LOCK_FLAG_SHARED | __SPRT_SPRT_LOCK_FLAG_CLOCK_REALTIME)) == 0) {
		return 0;
	}
#endif
	return -1;
}

__SPRT_C_FUNC int __SPRT_ID(sprt_qlock_wait)(__SPRT_ID(sprt_qlock_t) * value,
		__SPRT_ID(sprt_qlock_t) expected, __SPRT_ID(sprt_timeout_t) timeout,
		__SPRT_ID(sprt_lock_flags_t) flags) {
	int result = 0;
#if SPRT_LINUX || SPRT_ANDROID
	int _flags = __SPRT_FUTEX_PRIVATE_FLAG;
	if (hasFlag(flags, __SPRT_ID(sprt_lock_flags_t)(__SPRT_SPRT_LOCK_FLAG_SHARED))) {
		_flags &= ~__SPRT_FUTEX_PRIVATE_FLAG;
	}
	if (hasFlag(flags, __SPRT_ID(sprt_lock_flags_t)(__SPRT_SPRT_LOCK_FLAG_CLOCK_REALTIME))) {
		_flags &= ~__SPRT_FUTEX_CLOCK_REALTIME;
	}
	if (timeout == __SPRT_SPRT_TIMEOUT_INFINITE) {
		result = ::syscall(SYS_FUTEX_V1, value,
				__SPRT_FUTEX_WAIT | (_flags & __SPRT_FUTEX_FLAG_MASK), expected, nullptr);
	} else {
		struct timespec ts{
			static_cast<decltype(sprt::declval<struct timespec>().tv_sec)>(timeout / 1'000'000),
			static_cast<decltype(sprt::declval<struct timespec>().tv_nsec)>(timeout % 1'000'000),
		};

		result = ::syscall(SYS_FUTEX_V1, value,
				__SPRT_FUTEX_WAIT | (_flags & __SPRT_FUTEX_FLAG_MASK), expected, &ts);
	}

#elif SPRT_MACOS
	if (timeout == __SPRT_SPRT_TIMEOUT_INFINITE) {
		result = os_sync_wait_on_address((void *)value, expected, sizeof(uint32_t),
				OS_SYNC_WAIT_ON_ADDRESS_NONE);
	} else {
		result = os_sync_wait_on_address_with_timeout((void *)value, expected, sizeof(uint32_t),
				OS_SYNC_WAIT_ON_ADDRESS_NONE, OS_CLOCK_MACH_ABSOLUTE_TIME, timeout);
	}

#elif SPRT_WINDOWS
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
#else
#error not implemented
#endif
#if DEBUG
	if (result != 0 && __sprt_errno != EAGAIN && __sprt_errno != ETIMEDOUT) {
		__sprt_printf("sprt_qlock_wait error: %d\n", __sprt_errno);
	}
#endif
	return result;
}

__SPRT_C_FUNC int __SPRT_ID(
		sprt_qlock_wake_one)(__SPRT_ID(sprt_qlock_t) * value, __SPRT_ID(sprt_lock_flags_t) flags) {
	int result = 0;
#if SPRT_LINUX || SPRT_ANDROID
	int _flags = __SPRT_FUTEX_PRIVATE_FLAG;
	if (hasFlag(flags, __SPRT_ID(sprt_lock_flags_t)(__SPRT_SPRT_LOCK_FLAG_SHARED))) {
		_flags &= ~__SPRT_FUTEX_PRIVATE_FLAG;
	}
	result = ::syscall(SYS_FUTEX_V1, value, __SPRT_FUTEX_WAKE | (_flags & __SPRT_FUTEX_FLAG_MASK),
			1);
	if (result > 0) {
		result = 0;
	}
#elif SPRT_MACOS
	result = os_sync_wake_by_address_any((void *)value, sizeof(uint32_t),
			OS_SYNC_WAKE_BY_ADDRESS_NONE);

#elif SPRT_WINDOWS
	WakeByAddressSingle((void *)value);
#else
#error not implemented
#endif
#if DEBUG
	if (result != 0 && __sprt_errno != EAGAIN && __sprt_errno != ETIMEDOUT) {
		__sprt_printf("sprt_qlock_wait error: %d\n", __sprt_errno);
	}
#endif
	return result;
}

__SPRT_C_FUNC int __SPRT_ID(
		sprt_qlock_wake_all)(__SPRT_ID(sprt_qlock_t) * value, __SPRT_ID(sprt_lock_flags_t) flags) {
	int result = 0;
#if SPRT_LINUX || SPRT_ANDROID
	int _flags = __SPRT_FUTEX_PRIVATE_FLAG;
	if (hasFlag(flags, __SPRT_ID(sprt_lock_flags_t)(__SPRT_SPRT_LOCK_FLAG_SHARED))) {
		_flags &= ~__SPRT_FUTEX_PRIVATE_FLAG;
	}
	result = ::syscall(SYS_FUTEX_V1, value, __SPRT_FUTEX_WAKE | (_flags & __SPRT_FUTEX_FLAG_MASK),
			__SPRT_INT_MAX);
	if (result > 0) {
		result = 0;
	}
#elif SPRT_MACOS
	result = os_sync_wake_by_address_all((void *)value, sizeof(uint32_t),
			OS_SYNC_WAKE_BY_ADDRESS_NONE);

#elif SPRT_WINDOWS
	WakeByAddressAll((void *)value);
#else
#error not implemented
#endif
#if DEBUG
	if (result != 0 && __sprt_errno != EAGAIN && __sprt_errno != ETIMEDOUT) {
		__sprt_printf("sprt_qlock_wait error: %d\n", __sprt_errno);
	}
#endif
	return result;
}

__SPRT_C_FUNC int __SPRT_ID(sprt_rlock_supports)(__SPRT_ID(sprt_lock_flags_t) flags) {
	if (flags == 0) {
		return 0;
	}
#if SPRT_LINUX || SPRT_ANDROID
	// SPRT supports 5.10+ kernels, __SPRT_FUTEX_CLOCK_REALTIME requires 5.14 for __SPRT_FUTEX_LOCK_PI2
	if (isAbove5_14()) {
		if ((flags
					& ~(__SPRT_SPRT_LOCK_FLAG_SHARED | __SPRT_SPRT_LOCK_FLAG_PI
							| __SPRT_SPRT_LOCK_FLAG_CLOCK_REALTIME))
				== 0) {
			return 0;
		}
	} else {
		// Before 5.14 it's SPRT_LOCK_FLAG_PI or SPRT_LOCK_FLAG_CLOCK_REALTIME, not both
		if ((flags & (__SPRT_SPRT_LOCK_FLAG_PI | __SPRT_SPRT_LOCK_FLAG_CLOCK_REALTIME))
				== (__SPRT_SPRT_LOCK_FLAG_PI | __SPRT_SPRT_LOCK_FLAG_CLOCK_REALTIME)) {
			return -1;
		}
		if ((flags
					& ~(__SPRT_SPRT_LOCK_FLAG_SHARED | __SPRT_SPRT_LOCK_FLAG_PI
							| __SPRT_SPRT_LOCK_FLAG_CLOCK_REALTIME))
				== 0) {
			return 0;
		}
	}
#endif
	return -1;
}

__SPRT_C_FUNC int __SPRT_ID(sprt_rlock_wait)(__SPRT_ID(sprt_rlock_t) * value,
		__SPRT_ID(sprt_rlock_t) * expected, __SPRT_ID(sprt_timeout_t) timeout,
		__SPRT_ID(sprt_lock_flags_t) flags) {
#if SPRT_LINUX || SPRT_ANDROID
	// FUTEX_LOCK_PI(2const): https://man7.org/linux/man-pages/man2/FUTEX_LOCK_PI.2const.html
	bool above_5_14 = isAbove5_14();
	int _flags = __SPRT_FUTEX_PRIVATE_FLAG;
	if (hasFlag(flags, __SPRT_ID(sprt_lock_flags_t)(__SPRT_SPRT_LOCK_FLAG_SHARED))) {
		_flags &= ~__SPRT_FUTEX_PRIVATE_FLAG;
	}
	if (above_5_14) {
		if (hasFlag(flags, __SPRT_ID(sprt_lock_flags_t)(__SPRT_SPRT_LOCK_FLAG_CLOCK_REALTIME))) {
			_flags &= ~__SPRT_FUTEX_CLOCK_REALTIME;
		}
	}
	if (timeout == __SPRT_SPRT_TIMEOUT_INFINITE) {
		if (hasFlag(flags, __SPRT_ID(sprt_lock_flags_t)(__SPRT_SPRT_LOCK_FLAG_PI))) {
			return ::syscall(SYS_FUTEX_V1, &value->u32_2,
					(above_5_14 ? __SPRT_FUTEX_LOCK_PI2 : __SPRT_FUTEX_LOCK_PI)
							| (_flags & __SPRT_FUTEX_FLAG_MASK),
					0, nullptr);
		} else {
			return ::syscall(SYS_FUTEX_V1, &value->u32_2,
					__SPRT_FUTEX_WAIT | (_flags & __SPRT_FUTEX_FLAG_MASK), expected->u32_2,
					nullptr);
		}
	} else {
		timeout += __sprt_sprt_rlock_now(flags);
		struct timespec ts{
			static_cast<decltype(sprt::declval<struct timespec>().tv_sec)>(timeout / 1'000'000),
			static_cast<decltype(sprt::declval<struct timespec>().tv_nsec)>(timeout % 1'000'000),
		};

		if (hasFlag(flags, __SPRT_ID(sprt_lock_flags_t)(__SPRT_SPRT_LOCK_FLAG_PI))) {
			return ::syscall(SYS_FUTEX_V1, &value->u32_2,
					(above_5_14 ? __SPRT_FUTEX_LOCK_PI2 : __SPRT_FUTEX_LOCK_PI)
							| (_flags & __SPRT_FUTEX_FLAG_MASK),
					0, &ts);
		} else {
			return ::syscall(SYS_FUTEX_V1, &value->u32_2,
					__SPRT_FUTEX_WAIT | (_flags & __SPRT_FUTEX_FLAG_MASK), expected->u32_2, &ts);
		}
	}

#elif SPRT_MACOS
	if (timeout == __SPRT_SPRT_TIMEOUT_INFINITE) {
		return os_sync_wait_on_address((void *)&value->u64, expected->u64, sizeof(uint64_t),
				OS_SYNC_WAIT_ON_ADDRESS_NONE);
	} else {
		return os_sync_wait_on_address_with_timeout((void *)&value->u64, expected->u64,
				sizeof(uint64_t), OS_SYNC_WAIT_ON_ADDRESS_NONE, OS_CLOCK_MACH_ABSOLUTE_TIME,
				timeout);
	}
	return 0;

#elif SPRT_WINDOWS
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
#else
#error not implemented
#endif
}

__SPRT_C_FUNC int __SPRT_ID(
		sprt_rlock_try_wait)(__SPRT_ID(sprt_rlock_t) * value, __SPRT_ID(sprt_lock_flags_t) flags) {
#if SPRT_LINUX || SPRT_ANDROID
	if (hasFlag(flags, __SPRT_ID(sprt_lock_flags_t)(__SPRT_SPRT_LOCK_FLAG_PI))) {
		int _flags = __SPRT_FUTEX_PRIVATE_FLAG;
		if (hasFlag(flags, __SPRT_ID(sprt_lock_flags_t)(__SPRT_SPRT_LOCK_FLAG_SHARED))) {
			_flags &= ~__SPRT_FUTEX_PRIVATE_FLAG;
		}
		return ::syscall(SYS_FUTEX_V1, &value->u32_2,
				__SPRT_FUTEX_TRYLOCK_PI | (_flags & __SPRT_FUTEX_FLAG_MASK));
	}
	return 0;
#elif SPRT_WINDOWS || SPRT_MACOS
	return 0;
#else
#error not implemented
#endif
}

__SPRT_C_FUNC int __SPRT_ID(
		sprt_rlock_wake)(__SPRT_ID(sprt_rlock_t) * value, __SPRT_ID(sprt_lock_flags_t) flags) {
#if SPRT_LINUX || SPRT_ANDROID
	int _flags = __SPRT_FUTEX_PRIVATE_FLAG;
	if (hasFlag(flags, __SPRT_ID(sprt_lock_flags_t)(__SPRT_SPRT_LOCK_FLAG_SHARED))) {
		_flags &= ~__SPRT_FUTEX_PRIVATE_FLAG;
	}

	if (hasFlag(flags, __SPRT_ID(sprt_lock_flags_t)(__SPRT_SPRT_LOCK_FLAG_PI))) {
		return ::syscall(SYS_FUTEX_V1, &value->u32_2,
				__SPRT_FUTEX_UNLOCK_PI | (_flags & __SPRT_FUTEX_FLAG_MASK));
	} else {
		_atomic::storeSeq(&value->u32_2, uint32_t(0));
		return ::syscall(SYS_FUTEX_V1, &value->u32_2,
				__SPRT_FUTEX_WAKE | (_flags & __SPRT_FUTEX_FLAG_MASK), 1);
	}

#elif SPRT_MACOS
	_atomic::storeSeq(&value->u64, uint64_t(0));
	return os_sync_wake_by_address_one((void *)&value->u64, sizeof(uint64_t),
			OS_SYNC_WAKE_BY_ADDRESS_NONE);

#elif SPRT_WINDOWS
	_atomic::storeSeq(&value->u64, uint64_t(0));
	WakeByAddressSingle((void *)&value->u64);
	return 0;
#else
#error not implemented
#endif
}

__SPRT_C_FUNC __SPRT_ID(sprt_timeout_t)
		__SPRT_ID(sprt_qlock_now)(__SPRT_ID(sprt_lock_flags_t) flags) {
	return __sprt_clock_gettime_nsec_np(__sprt_sprt_qlock_getclock(flags));
}

__SPRT_C_FUNC __SPRT_ID(clockid_t)
		__SPRT_ID(sprt_qlock_getclock)(__SPRT_ID(sprt_lock_flags_t) flags) {
#if SPRT_MACOS
	return __SPRT_CLOCK_UPTIME_RAW;
#else
	if (hasFlag(flags, __SPRT_ID(sprt_lock_flags_t)(__SPRT_SPRT_LOCK_FLAG_CLOCK_REALTIME))) {
		return __SPRT_CLOCK_REALTIME;
	} else {
		return __SPRT_CLOCK_MONOTONIC;
	}
#endif
}

__SPRT_C_FUNC __SPRT_ID(sprt_timeout_t)
		__SPRT_ID(sprt_rlock_now)(__SPRT_ID(sprt_lock_flags_t) flags) {
	return __sprt_clock_gettime_nsec_np(__sprt_sprt_rlock_getclock(flags));
}

__SPRT_C_FUNC __SPRT_ID(clockid_t)
		__SPRT_ID(sprt_rlock_getclock)(__SPRT_ID(sprt_lock_flags_t) flags) {
#if SPRT_MACOS
	return __SPRT_CLOCK_UPTIME_RAW;
#elif SPRT_LINUX || SPRT_ANDROID
	if (!hasFlag(flags, __SPRT_ID(sprt_lock_flags_t)(__SPRT_SPRT_LOCK_FLAG_PI))) {
		// same as qlock
		if (hasFlag(flags, __SPRT_ID(sprt_lock_flags_t)(__SPRT_SPRT_LOCK_FLAG_CLOCK_REALTIME))) {
			return __SPRT_CLOCK_REALTIME;
		} else {
			return __SPRT_CLOCK_MONOTONIC;
		}
	} else if (isAbove5_14()) {
		// after 5.14 we use FUTEX_LOCK_PI2, which uses CLOCK_MONOTONIC by default,
		// but supports CLOCK_REALTIME with extra flag
		if (hasFlag(flags, __SPRT_ID(sprt_lock_flags_t)(__SPRT_SPRT_LOCK_FLAG_CLOCK_REALTIME))) {
			return __SPRT_CLOCK_REALTIME;
		} else {
			return __SPRT_CLOCK_MONOTONIC;
		}
	} else {
		// before 5.14 we use FUTEX_LOCK_PI, which uses CLOCK_REALTIME
		return __SPRT_CLOCK_REALTIME;
	}
#else
	return __SPRT_CLOCK_MONOTONIC;
#endif
}

static __sprt_utsname s_unameBuf;
static sprt::qonce s_nameBUfOnce;

__SPRT_C_FUNC const char *__SPRT_ID(sprt_get_os_kernel_version)() {
	s_nameBUfOnce([] { __sprt_uname(&s_unameBuf); });

	return s_unameBuf.release;
}

__SPRT_C_FUNC int __SPRT_ID(sprt_compare_os_kernel_version)(const char *cmp) {
	return __sprt_strverscmp(__sprt_sprt_get_os_kernel_version(), cmp);
}

} // namespace sprt
