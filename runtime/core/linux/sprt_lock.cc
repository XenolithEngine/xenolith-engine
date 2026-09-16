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
#include <sprt/c/cross/__sprt_syscall.h>
#include <sprt/runtime/thread/qonce.h>

#include <unistd.h>

namespace sprt {

static bool isAbove5_14() {
	static sprt::qonce s_isAbove5_14Once;
	static bool s_isAbove5_14Value = false;

	s_isAbove5_14Once(
			[] { s_isAbove5_14Value = (__sprt_sprt_compare_os_kernel_version("5.14") >= 0); });

	return s_isAbove5_14Value;
}

static int sprt_qlock_supports(__SPRT_ID(sprt_lock_flags_t) flags) {
	// SPRT supports 5.10+ kernels, __SPRT_FUTEX_CLOCK_REALTIME requires 5.4 for FUTEX_WAIT
	if ((flags & ~(__SPRT_SPRT_LOCK_FLAG_SHARED | __SPRT_SPRT_LOCK_FLAG_CLOCK_REALTIME)) == 0) {
		return 0;
	}
	return -1;
}

static int sprt_qlock_wait(__SPRT_ID(sprt_qlock_t) * value, __SPRT_ID(sprt_qlock_t) expected,
		__SPRT_ID(sprt_timeout_t) timeout, __SPRT_ID(sprt_lock_flags_t) flags) {
	int result = 0;
	int _flags = __SPRT_FUTEX_PRIVATE_FLAG;
	if (hasFlag(flags, __SPRT_ID(sprt_lock_flags_t)(__SPRT_SPRT_LOCK_FLAG_SHARED))) {
		_flags &= ~__SPRT_FUTEX_PRIVATE_FLAG;
	}
	if (timeout == __SPRT_SPRT_TIMEOUT_INFINITE) {
		// No deadline: plain FUTEX_WAIT (FUTEX_CLOCK_REALTIME is not valid for it).
		result = ::syscall(__SPRT_SYSCALL_futex, value,
				__SPRT_FUTEX_WAIT | (_flags & __SPRT_FUTEX_FLAG_MASK), expected, nullptr);
	} else {
		// FUTEX_WAIT_BITSET takes an ABSOLUTE deadline and, unlike plain FUTEX_WAIT,
		// honors FUTEX_CLOCK_REALTIME. Convert the relative timeout to an absolute
		// deadline on the matching clock (qlock_now follows qlock_getclock).
		//
		// NOTE: the futex(2) man page lists FUTEX_WAIT as accepting FUTEX_CLOCK_REALTIME
		// "(since Linux 4.5)", but that is inaccurate for the raw syscall: the kernel's
		// do_futex() gates the flag to {WAIT_BITSET, WAIT_REQUEUE_PI, LOCK_PI2} and
		// returns -ENOSYS for the bare FUTEX_WAIT opcode. Verified on Linux 6.18:
		// FUTEX_WAIT|FUTEX_CLOCK_REALTIME -> ENOSYS, while FUTEX_WAIT_BITSET honors it.
		// So a CLOCK_REALTIME timed wait MUST use FUTEX_WAIT_BITSET.
		int op = __SPRT_FUTEX_WAIT_BITSET;
		if (hasFlag(flags, __SPRT_ID(sprt_lock_flags_t)(__SPRT_SPRT_LOCK_FLAG_CLOCK_REALTIME))) {
			op |= __SPRT_FUTEX_CLOCK_REALTIME;
		}
		auto deadline = timeout + __sprt_sprt_qlock_now(flags);
		struct timespec ts{
			static_cast<decltype(sprt::declval<struct timespec>().tv_sec)>(deadline / 1'000'000'000),
			static_cast<decltype(sprt::declval<struct timespec>().tv_nsec)>(deadline % 1'000'000'000),
		};

		result = ::syscall(__SPRT_SYSCALL_futex, value, op | (_flags & __SPRT_FUTEX_FLAG_MASK),
				expected, &ts, nullptr, __SPRT_FUTEX_BITSET_MATCH_ANY);
	}
	return result;
}

static int sprt_qlock_wake_one(__SPRT_ID(sprt_qlock_t) * value,
		__SPRT_ID(sprt_lock_flags_t) flags) {
	int result = 0;
	int _flags = __SPRT_FUTEX_PRIVATE_FLAG;
	if (hasFlag(flags, __SPRT_ID(sprt_lock_flags_t)(__SPRT_SPRT_LOCK_FLAG_SHARED))) {
		_flags &= ~__SPRT_FUTEX_PRIVATE_FLAG;
	}
	result = ::syscall(__SPRT_SYSCALL_futex, value,
			__SPRT_FUTEX_WAKE | (_flags & __SPRT_FUTEX_FLAG_MASK), 1);
	if (result > 0) {
		result = 0;
	}
	return result;
}

static int sprt_qlock_wake_all(__SPRT_ID(sprt_qlock_t) * value,
		__SPRT_ID(sprt_lock_flags_t) flags) {
	int result = 0;
	int _flags = __SPRT_FUTEX_PRIVATE_FLAG;
	if (hasFlag(flags, __SPRT_ID(sprt_lock_flags_t)(__SPRT_SPRT_LOCK_FLAG_SHARED))) {
		_flags &= ~__SPRT_FUTEX_PRIVATE_FLAG;
	}
	result = ::syscall(__SPRT_SYSCALL_futex, value,
			__SPRT_FUTEX_WAKE | (_flags & __SPRT_FUTEX_FLAG_MASK), __SPRT_INT_MAX);
	if (result > 0) {
		result = 0;
	}
	return result;
}

static int sprt_rlock_supports(__SPRT_ID(sprt_lock_flags_t) flags) {
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
	return -1;
}

static int sprt_rlock_wait(__SPRT_ID(sprt_rlock_t) * value, __SPRT_ID(sprt_rlock_t) * expected,
		__SPRT_ID(sprt_timeout_t) timeout, __SPRT_ID(sprt_lock_flags_t) flags) {
	// FUTEX_LOCK_PI(2const): https://man7.org/linux/man-pages/man2/FUTEX_LOCK_PI.2const.html
	bool above_5_14 = isAbove5_14();
	int _flags = __SPRT_FUTEX_PRIVATE_FLAG;
	if (hasFlag(flags, __SPRT_ID(sprt_lock_flags_t)(__SPRT_SPRT_LOCK_FLAG_SHARED))) {
		_flags &= ~__SPRT_FUTEX_PRIVATE_FLAG;
	}
	// Carry FUTEX_CLOCK_REALTIME in the shared flags only for the PI path (LOCK_PI2,
	// >=5.14). The non-PI timed path adds it directly to its FUTEX_WAIT_BITSET op, and
	// the non-PI infinite path uses plain FUTEX_WAIT, which the kernel rejects the flag
	// on with -ENOSYS — so it must NOT appear in the shared flags here.
	if (above_5_14 && hasFlag(flags, __SPRT_ID(sprt_lock_flags_t)(__SPRT_SPRT_LOCK_FLAG_PI))) {
		if (hasFlag(flags, __SPRT_ID(sprt_lock_flags_t)(__SPRT_SPRT_LOCK_FLAG_CLOCK_REALTIME))) {
			_flags |= __SPRT_FUTEX_CLOCK_REALTIME;
		}
	}
	if (timeout == __SPRT_SPRT_TIMEOUT_INFINITE) {
		if (hasFlag(flags, __SPRT_ID(sprt_lock_flags_t)(__SPRT_SPRT_LOCK_FLAG_PI))) {
			return ::syscall(__SPRT_SYSCALL_futex, &value->u32_2,
					(above_5_14 ? __SPRT_FUTEX_LOCK_PI2 : __SPRT_FUTEX_LOCK_PI)
							| (_flags & __SPRT_FUTEX_FLAG_MASK),
					0, nullptr);
		} else {
			return ::syscall(__SPRT_SYSCALL_futex, &value->u32_2,
					__SPRT_FUTEX_WAIT | (_flags & __SPRT_FUTEX_FLAG_MASK), expected->u32_2,
					nullptr);
		}
	} else if (hasFlag(flags, __SPRT_ID(sprt_lock_flags_t)(__SPRT_SPRT_LOCK_FLAG_PI))) {
		// FUTEX_LOCK_PI/PI2 take an ABSOLUTE deadline; convert the relative timeout
		// (against the clock selected by sprt_rlock_getclock/now).
		auto deadline = timeout + __sprt_sprt_rlock_now(flags);
		struct timespec ts{
			static_cast<decltype(sprt::declval<struct timespec>().tv_sec)>(deadline / 1'000'000'000),
			static_cast<decltype(sprt::declval<struct timespec>().tv_nsec)>(deadline % 1'000'000'000),
		};

		return ::syscall(__SPRT_SYSCALL_futex, &value->u32_2,
				(above_5_14 ? __SPRT_FUTEX_LOCK_PI2 : __SPRT_FUTEX_LOCK_PI)
						| (_flags & __SPRT_FUTEX_FLAG_MASK),
				0, &ts);
	} else {
		// non-PI timed wait: FUTEX_WAIT_BITSET takes an ABSOLUTE deadline and honors
		// FUTEX_CLOCK_REALTIME (plain FUTEX_WAIT rejects that flag with ENOSYS).
		int op = __SPRT_FUTEX_WAIT_BITSET;
		if (hasFlag(flags, __SPRT_ID(sprt_lock_flags_t)(__SPRT_SPRT_LOCK_FLAG_CLOCK_REALTIME))) {
			op |= __SPRT_FUTEX_CLOCK_REALTIME;
		}
		auto deadline = timeout + __sprt_sprt_rlock_now(flags);
		struct timespec ts{
			static_cast<decltype(sprt::declval<struct timespec>().tv_sec)>(deadline / 1'000'000'000),
			static_cast<decltype(sprt::declval<struct timespec>().tv_nsec)>(deadline % 1'000'000'000),
		};

		return ::syscall(__SPRT_SYSCALL_futex, &value->u32_2,
				op | (_flags & __SPRT_FUTEX_FLAG_MASK), expected->u32_2, &ts, nullptr,
				__SPRT_FUTEX_BITSET_MATCH_ANY);
	}
}

static int sprt_rlock_try_wait(__SPRT_ID(sprt_rlock_t) * value,
		__SPRT_ID(sprt_lock_flags_t) flags) {
	if (hasFlag(flags, __SPRT_ID(sprt_lock_flags_t)(__SPRT_SPRT_LOCK_FLAG_PI))) {
		int _flags = __SPRT_FUTEX_PRIVATE_FLAG;
		if (hasFlag(flags, __SPRT_ID(sprt_lock_flags_t)(__SPRT_SPRT_LOCK_FLAG_SHARED))) {
			_flags &= ~__SPRT_FUTEX_PRIVATE_FLAG;
		}
		return ::syscall(__SPRT_SYSCALL_futex, &value->u32_2,
				__SPRT_FUTEX_TRYLOCK_PI | (_flags & __SPRT_FUTEX_FLAG_MASK));
	}
	return 0;
}

static int sprt_rlock_wake(__SPRT_ID(sprt_rlock_t) * value, __SPRT_ID(sprt_lock_flags_t) flags) {
	int _flags = __SPRT_FUTEX_PRIVATE_FLAG;
	if (hasFlag(flags, __SPRT_ID(sprt_lock_flags_t)(__SPRT_SPRT_LOCK_FLAG_SHARED))) {
		_flags &= ~__SPRT_FUTEX_PRIVATE_FLAG;
	}

	int result = 0;
	if (hasFlag(flags, __SPRT_ID(sprt_lock_flags_t)(__SPRT_SPRT_LOCK_FLAG_PI))) {
		result = ::syscall(__SPRT_SYSCALL_futex, &value->u32_2,
				__SPRT_FUTEX_UNLOCK_PI | (_flags & __SPRT_FUTEX_FLAG_MASK));
	} else {
		_atomic::storeSeq(&value->u32_2, uint32_t(0));
		result = ::syscall(__SPRT_SYSCALL_futex, &value->u32_2,
				__SPRT_FUTEX_WAKE | (_flags & __SPRT_FUTEX_FLAG_MASK), 1);
	}
	if (result > 0) {
		result = 0;
	}
	return result;
}

static __SPRT_ID(clockid_t) sprt_qlock_getclock(__SPRT_ID(sprt_lock_flags_t) flags) {
	if (hasFlag(flags, __SPRT_ID(sprt_lock_flags_t)(__SPRT_SPRT_LOCK_FLAG_CLOCK_REALTIME))) {
		return __SPRT_CLOCK_REALTIME;
	} else {
		return __SPRT_CLOCK_MONOTONIC;
	}
}

static __SPRT_ID(clockid_t) sprt_rlock_getclock(__SPRT_ID(sprt_lock_flags_t) flags) {
	if (!hasFlag(flags, __SPRT_ID(sprt_lock_flags_t)(__SPRT_SPRT_LOCK_FLAG_PI))) {
		// Non-PI uses FUTEX_WAIT_BITSET with an absolute deadline that honors the
		// realtime flag, so report the matching clock for building that deadline.
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
}

} // namespace sprt
