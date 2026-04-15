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

#ifndef CORE_RUNTIME_INCLUDE_C_SYS___SPRT_SPRT_H_
#define CORE_RUNTIME_INCLUDE_C_SYS___SPRT_SPRT_H_

#include <sprt/c/bits/__sprt_time_t.h>
#include <sprt/c/bits/__sprt_uint32_t.h>
#include <sprt/c/bits/__sprt_uint64_t.h>

// Default nanoseconds timeout
typedef __SPRT_ID(uint64_t) __SPRT_ID(sprt_timeout_t);

typedef __SPRT_ID(uint32_t) __SPRT_ID(sprt_lock_flags_t);

/*
	sprt_qlock_t is a platform-independent FUTEX API implementation.
	It uses uint32_t as a value for all platforms.

	sprt_qlock_t is small, fast and minimal in it's capabilites.
	Potentialiy, it has no priority inversion mitigation, use with care.

	Implementation:
	- Windows - WaitOnAddress
	- Linux/Android - Futex (v1)
	- MacOS - os_sync_wait_on_address
*/
typedef __SPRT_ID(uint32_t) __SPRT_ID(sprt_qlock_t);

/*
	sprt_rlock_t is a basic primitive for more complex mutexes,
	that supports recursive locking and priority inversion mitigation.

	sprt_rlock_t is only a building block, not a mutex itself
	(see sprt/runtime/thread/rmutex.h for the complete mutex implementation)

	Note that sprt_rlock_t may not implement PI by itself, but serves as
	a platform for it's implemntation by more complex types

	Implementation:
	- Windows - WaitOnAddress (WinAPI Priority boost)
	- Linux/Android - Futex (v1) with PI
	- MacOS - os_sync_wait_on_address (custom PI implementation with QoS system API)
*/
typedef union __SPRT_ID(sprt_rlock_u) {
	__SPRT_ID(uint64_t) u64;
	struct {
		__SPRT_ID(uint32_t) u32_1;
		__SPRT_ID(uint32_t) u32_2;
	};
} __SPRT_ID(sprt_rlock_t);

// Infinite timeout value
#define __SPRT_SPRT_TIMEOUT_INFINITE __SPRT_UINT64_MAX

#define __SPRT_SPRT_LOCK_FLAG_NONE 0

// By default, SPRT's lock primitives are process-private;
// use this flag, if you need to use primitive in cross-process context
#define __SPRT_SPRT_LOCK_FLAG_SHARED 1

// Enables OS level priority inheritance if possible
#define __SPRT_SPRT_LOCK_FLAG_PI 2

// Use realtime system clock for timeout instead of monotonic clock
#define __SPRT_SPRT_LOCK_FLAG_CLOCK_REALTIME 4

// Did OS implements extended locking logic by itself?
// This flag changes locking protocol to use sprt_rlock_t
// (see sprt/runtime/thread/rmutex.h)
#if SPRT_LINUX || SPRT_ANDROID
#define __SPRT_SPRT_RLOCK_PI_REQUIRES_EXTENDED_CALL 1
#else
#define __SPRT_SPRT_RLOCK_PI_REQUIRES_EXTENDED_CALL 0
#endif

__SPRT_BEGIN_DECL

// Check for runtime support of the combination of flags for sprt_qlock_t
// Returns 0 if flags are supported, -1 otherwise
SPRT_API int __SPRT_ID(sprt_qlock_supports)(__SPRT_ID(sprt_lock_flags_t));

SPRT_API int __SPRT_ID(sprt_qlock_wait)(__SPRT_ID(sprt_qlock_t) * value,
		__SPRT_ID(sprt_qlock_t) expected, __SPRT_ID(sprt_timeout_t), __SPRT_ID(sprt_lock_flags_t));

SPRT_API int __SPRT_ID(
		sprt_qlock_wake_one)(__SPRT_ID(sprt_qlock_t) * value, __SPRT_ID(sprt_lock_flags_t));

SPRT_API int __SPRT_ID(
		sprt_qlock_wake_all)(__SPRT_ID(sprt_qlock_t) * value, __SPRT_ID(sprt_lock_flags_t));

SPRT_API __SPRT_ID(sprt_timeout_t) __SPRT_ID(sprt_qlock_now)(__SPRT_ID(sprt_lock_flags_t));

SPRT_API __SPRT_ID(clockid_t) __SPRT_ID(sprt_qlock_getclock)(__SPRT_ID(sprt_lock_flags_t));

// Check for runtime support of the combination of flags for sprt_rlock_t
// Usage of flags compination, for that sprt_rlock_supports returns -1 leads to undefined behavior
SPRT_API int __SPRT_ID(sprt_rlock_supports)(__SPRT_ID(sprt_lock_flags_t));

SPRT_API int __SPRT_ID(sprt_rlock_wait)(__SPRT_ID(sprt_rlock_t) * value,
		__SPRT_ID(sprt_rlock_t) * expected, __SPRT_ID(sprt_timeout_t),
		__SPRT_ID(sprt_lock_flags_t));

SPRT_API int __SPRT_ID(
		sprt_rlock_try_wait)(__SPRT_ID(sprt_rlock_t) * value, __SPRT_ID(sprt_lock_flags_t));

SPRT_API int __SPRT_ID(
		sprt_rlock_wake)(__SPRT_ID(sprt_rlock_t) * value, __SPRT_ID(sprt_lock_flags_t));

SPRT_API __SPRT_ID(sprt_timeout_t) __SPRT_ID(sprt_rlock_now)(__SPRT_ID(sprt_lock_flags_t));

SPRT_API __SPRT_ID(clockid_t) __SPRT_ID(sprt_rlock_getclock)(__SPRT_ID(sprt_lock_flags_t));

// OS version API

/*
	Returns current OS kernel version in classic format, like:
	Linux/Android: 5.15.0-101-generic
	Windows: 11.0
	MacOS: 14.4

	Useful to check if some API is available at runtime
*/
SPRT_API const char *__SPRT_ID(sprt_get_os_kernel_version)();

/*
	Effectively performs strverscmp(sprt_get_os_kernel_version(), <version>);
*/
SPRT_API int __SPRT_ID(sprt_compare_os_kernel_version)(const char *version);

__SPRT_END_DECL

#endif // CORE_RUNTIME_INCLUDE_C_SYS___SPRT_SPRT_H_
