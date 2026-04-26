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

#include <sprt/c/sys/__sprt_sprt.h>
#include <sprt/c/sys/__sprt_futex.h>
#include <sprt/cxx/__utility/common.h>
#include <sprt/cxx/atomic>
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

#if SPRT_LINUX || SPRT_ANDROID
#include "../platform/linux/sprt_lock.cc"
#elif SPRT_MACOS
#include "../platform/darwin/sprt_lock.cc"
#elif SPRT_WINDOWS
#include "../platform/windows/sprt_lock.cc"
#else
#error Not implemented
#endif
namespace sprt {

__SPRT_C_FUNC int __SPRT_ID(sprt_qlock_supports)(__SPRT_ID(sprt_lock_flags_t) flags) {
	if (flags == 0) {
		return 0;
	}

	return sprt_qlock_supports(flags);
}

__SPRT_C_FUNC int __SPRT_ID(sprt_qlock_wait)(__SPRT_ID(sprt_qlock_t) * value,
		__SPRT_ID(sprt_qlock_t) expected, __SPRT_ID(sprt_timeout_t) timeout,
		__SPRT_ID(sprt_lock_flags_t) flags) {
	int result = sprt_qlock_wait(value, expected, timeout, flags);
#if DEBUG
	if (result != 0) {
		auto err = __sprt_errno;
		if (err != EAGAIN && err != ETIMEDOUT) {
			__sprt_printf("sprt_qlock_wake_one error: %d\n", err);
		}
	}
#endif
	return result;
}

__SPRT_C_FUNC int __SPRT_ID(
		sprt_qlock_wake_one)(__SPRT_ID(sprt_qlock_t) * value, __SPRT_ID(sprt_lock_flags_t) flags) {
	int result = sprt_qlock_wake_one(value, flags);
#if DEBUG
	if (result != 0) {
		auto err = __sprt_errno;
		if (err != EAGAIN && err != ENOENT && err != ETIMEDOUT) {
			__sprt_printf("sprt_qlock_wake_one error: %d\n", err);
		}
	}
#endif
	return result;
}

__SPRT_C_FUNC int __SPRT_ID(
		sprt_qlock_wake_all)(__SPRT_ID(sprt_qlock_t) * value, __SPRT_ID(sprt_lock_flags_t) flags) {
	int result = sprt_qlock_wake_all(value, flags);
#if DEBUG
	if (result != 0) {
		auto err = __sprt_errno;
		if (err != EAGAIN && err != ENOENT && err != ETIMEDOUT) {
			__sprt_printf("sprt_qlock_wake_one error: %d\n", err);
		}
	}
#endif
	return result;
}

__SPRT_C_FUNC int __SPRT_ID(sprt_rlock_supports)(__SPRT_ID(sprt_lock_flags_t) flags) {
	if (flags == 0) {
		return 0;
	}

	return sprt_rlock_supports(flags);
}

__SPRT_C_FUNC int __SPRT_ID(sprt_rlock_wait)(__SPRT_ID(sprt_rlock_t) * value,
		__SPRT_ID(sprt_rlock_t) * expected, __SPRT_ID(sprt_timeout_t) timeout,
		__SPRT_ID(sprt_lock_flags_t) flags) {
	int result = sprt_rlock_wait(value, expected, timeout, flags);
#if DEBUG
	if (result != 0 && __sprt_errno != EAGAIN && __sprt_errno != ETIMEDOUT) {
		__sprt_printf("sprt_rlock_wait error: %d\n", __sprt_errno);
	}
#endif
	return result;
}

__SPRT_C_FUNC int __SPRT_ID(
		sprt_rlock_try_wait)(__SPRT_ID(sprt_rlock_t) * value, __SPRT_ID(sprt_lock_flags_t) flags) {
	return sprt_rlock_try_wait(value, flags);
}

__SPRT_C_FUNC int __SPRT_ID(
		sprt_rlock_wake)(__SPRT_ID(sprt_rlock_t) * value, __SPRT_ID(sprt_lock_flags_t) flags) {
	int result = sprt_rlock_wake(value, flags);
#if DEBUG
	if (result != 0 && __sprt_errno != EAGAIN && __sprt_errno != ETIMEDOUT) {
		__sprt_printf("sprt_rlock_wake error: %d\n", __sprt_errno);
	}
#endif
	return result;
}

__SPRT_C_FUNC __SPRT_ID(sprt_timeout_t)
		__SPRT_ID(sprt_qlock_now)(__SPRT_ID(sprt_lock_flags_t) flags) {
	return __sprt_clock_gettime_nsec_np(__sprt_sprt_qlock_getclock(flags));
}

__SPRT_C_FUNC __SPRT_ID(clockid_t)
		__SPRT_ID(sprt_qlock_getclock)(__SPRT_ID(sprt_lock_flags_t) flags) {
	return sprt_qlock_getclock(flags);
}

__SPRT_C_FUNC __SPRT_ID(sprt_timeout_t)
		__SPRT_ID(sprt_rlock_now)(__SPRT_ID(sprt_lock_flags_t) flags) {
	return __sprt_clock_gettime_nsec_np(__sprt_sprt_rlock_getclock(flags));
}

__SPRT_C_FUNC __SPRT_ID(clockid_t)
		__SPRT_ID(sprt_rlock_getclock)(__SPRT_ID(sprt_lock_flags_t) flags) {
	return sprt_rlock_getclock(flags);
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
