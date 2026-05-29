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

#ifndef CORE_RUNTIME_INCLUDE_LIBC_SYS_SPRT_H_
#define CORE_RUNTIME_INCLUDE_LIBC_SYS_SPRT_H_

#include <sprt/c/sys/__sprt_sprt.h>

typedef __SPRT_ID(clockid_t) clockid_t;
typedef __SPRT_ID(pid_t) pid_t;

typedef __SPRT_ID(sprt_timeout_t) sprt_timeout_t;

typedef __SPRT_ID(sprt_lock_flags_t) sprt_lock_flags_t;

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
typedef __SPRT_ID(sprt_qlock_t) sprt_qlock_t;

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
typedef __SPRT_ID(sprt_rlock_t) sprt_rlock_t;

/*
	sprt_plock_t - universal mutex, that protects some resource by it's pointer.

	No pre-initialization or termination required for sprt_plock_t
	(this is actually void * - any pointer), all primitives managed internally.

	plock is resursive and PI-aware
*/

typedef __SPRT_ID(sprt_plock_t) sprt_plock_t;

// Infinite timeout value
#define SPRT_TIMEOUT_INFINITE __SPRT_SPRT_TIMEOUT_INFINITE

#define SPRT_LOCK_FLAG_NONE __SPRT_SPRT_LOCK_FLAG_NONE

// By default, SPRT's lock primitives are process-private;
// use this flag, if you need to use primitive in cross-process context
#define SPRT_LOCK_FLAG_SHARED __SPRT_SPRT_LOCK_FLAG_SHARED

// Enables OS level priority inheritance if possible
#define SPRT_LOCK_FLAG_PI __SPRT_SPRT_LOCK_FLAG_PI

// Use realtime system clock for timeout instead of monotonic clock
#define SPRT_LOCK_FLAG_CLOCK_REALTIME __SPRT_SPRT_LOCK_FLAG_CLOCK_REALTIME

// Only for sprt_plock_t - mark resource as dead
// (e.g. when you delete resource, when it's lock is held)
// After this, next successful lock on this resource (if it was initiated when resource was alive)
// returns -1 with EOWNERDIED in errno
// Note that if you free/delete memory pointer with sprt_plock_t on it,
// allocator can immediately reuse sprt_plock_t address for other resource, before you call
// `sprt_plock_unlock`, and SPRT_LOCK_FLAG_RESOURCE_DIED will be asigned to new resource address.
// To mitigate this, do `sprt_plock_unlock` before free/delete resource by address, when you use
// SPRT_LOCK_FLAG_RESOURCE_DIED
#define SPRT_LOCK_FLAG_RESOURCE_DIED __SPRT_SPRT_LOCK_FLAG_RESOURCE_DIED

// Did OS implements extended locking logic by itself?
// This flag changes locking protocol to use sprt_rlock_t
// (see sprt/runtime/thread/rmutex.h)

#define SPRT_RLOCK_PI_REQUIRES_EXTENDED_CALL __SPRT_SPRT_RLOCK_PI_REQUIRES_EXTENDED_CALL

__SPRT_BEGIN_DECL

// Check for runtime support of the combination of flags for sprt_qlock_t
// Returns 0 if flags are supported, -1 otherwise
SPRT_FORCEINLINE
int sprt_qlock_supports(sprt_lock_flags_t flags) { return __sprt_sprt_qlock_supports(flags); }

SPRT_FORCEINLINE
int sprt_qlock_wait(sprt_qlock_t *value, sprt_qlock_t expected, sprt_timeout_t timeout,
		sprt_lock_flags_t flags) {
	return __sprt_sprt_qlock_wait(value, expected, timeout, flags);
}

SPRT_FORCEINLINE
int sprt_qlock_wake_one(sprt_qlock_t *value, sprt_lock_flags_t flags) {
	return __sprt_sprt_qlock_wake_one(value, flags);
}

SPRT_FORCEINLINE
int sprt_qlock_wake_all(sprt_qlock_t *value, sprt_lock_flags_t flags) {
	return __sprt_sprt_qlock_wake_all(value, flags);
}

SPRT_FORCEINLINE
sprt_timeout_t sprt_qlock_now(sprt_lock_flags_t flags) { return __sprt_sprt_qlock_now(flags); }

SPRT_FORCEINLINE
clockid_t sprt_qlock_getclock(sprt_lock_flags_t flags) { return __sprt_sprt_qlock_getclock(flags); }


/*
	"Oneshot" API - if you just need a quick mutex
*/
SPRT_FORCEINLINE
int sprt_qlock_oneshot_lock(sprt_qlock_t *value, sprt_lock_flags_t flags) {
	return __sprt_sprt_qlock_oneshot_lock(value, flags);
}

SPRT_FORCEINLINE
int sprt_qlock_oneshot_trylock(__SPRT_ID(sprt_qlock_t) * value) {
	return __sprt_sprt_qlock_oneshot_trylock(value);
}

SPRT_FORCEINLINE
int sprt_qlock_oneshot_unlock(sprt_qlock_t *value, sprt_lock_flags_t flags) {
	return __sprt_sprt_qlock_oneshot_unlock(value, flags);
}

// Check for runtime support of the combination of flags for sprt_rlock_t
// Usage of flags compination, for that sprt_rlock_supports returns -1 leads to undefined behavior
SPRT_FORCEINLINE
int sprt_rlock_supports(sprt_lock_flags_t flags) { return __sprt_sprt_rlock_supports(flags); }

SPRT_FORCEINLINE
int sprt_rlock_wait(sprt_rlock_t *value, sprt_rlock_t *expected, sprt_timeout_t timeout,
		sprt_lock_flags_t flags) {
	return __sprt_sprt_rlock_wait(value, expected, timeout, flags);
}

SPRT_FORCEINLINE
int sprt_rlock_try_wait(sprt_rlock_t *value, sprt_lock_flags_t flags) {
	return __sprt_sprt_rlock_try_wait(value, flags);
}

SPRT_FORCEINLINE
int sprt_rlock_wake(sprt_rlock_t *value, sprt_lock_flags_t flags) {
	return __sprt_sprt_rlock_wake(value, flags);
}

SPRT_FORCEINLINE
sprt_timeout_t sprt_rlock_now(sprt_lock_flags_t flags) { return __sprt_sprt_rlock_now(flags); }

SPRT_FORCEINLINE
clockid_t sprt_rlock_getclock(sprt_lock_flags_t flags) { return __sprt_sprt_rlock_getclock(flags); }

/*
	Locks resource by it's address. Flags is ignored for now.
	If tidPtr is provided, current id will be written on lock
	Returns 0 on success;
	Returns -1 on failure; errno set to:
		- EOWNERDEAD - resource is dead (sprt_plock_unlock was called with SPRT_LOCK_FLAG_RESOURCE_DIED)
*/
SPRT_FORCEINLINE
int sprt_plock_lock(sprt_plock_t addr, sprt_lock_flags_t flags, pid_t *tidPtr) {
	return __sprt_sprt_plock_lock(addr, flags, tidPtr);
}

/*
	Tries to lock resource by it's address. Flags is ignored for now.
	If tidPtr is provided, current id will be written on lock.
	Returns 0 on success;
	Returns -1 on failure; errno set to:
		- 0 - resource is already locked
		- EOWNERDEAD - resource is dead (sprt_plock_unlock was called with SPRT_LOCK_FLAG_RESOURCE_DIED)
*/
SPRT_FORCEINLINE
int sprt_plock_trylock(sprt_plock_t addr, sprt_lock_flags_t flags, pid_t *tidPtr) {
	return __sprt_sprt_plock_trylock(addr, flags, tidPtr);
}

/*
	Unlocks resource by it's address. Only acting flag is SPRT_LOCK_FLAG_RESOURCE_DIED;
	All others are ignored;
	If tidPtr is provided, 0 will be written before unlock
	Returns 0 on success;
	Returns -1 on failure; errno set to:
		- ESRCH - no lock found for resource
*/
SPRT_FORCEINLINE
int sprt_plock_unlock(sprt_plock_t addr, sprt_lock_flags_t flags, pid_t *tidPtr) {
	return __sprt_sprt_plock_unlock(addr, flags, tidPtr);
}

// OS version API

/*
	Returns current OS kernel version in classic format, like:
	Linux/Android: 5.15.0-101-generic
	Windows: 11.0
	MacOS: 14.4

	Useful to check if some API is available at runtime
*/
SPRT_FORCEINLINE
const char *sprt_get_os_kernel_version() { return __sprt_sprt_get_os_kernel_version(); }

/*
	Effectively performs strverscmp(sprt_get_os_kernel_version(), <version>);
*/
SPRT_FORCEINLINE
int sprt_compare_os_kernel_version(const char *version) {
	return __sprt_sprt_compare_os_kernel_version(version);
}

__SPRT_END_DECL

#endif // CORE_RUNTIME_INCLUDE_LIBC_SYS_SPRT_H_
