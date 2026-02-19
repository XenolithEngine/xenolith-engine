/**
 Copyright (c) 2026 Xenolith Team <admin@stappler.org>

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

#ifndef CORE_RUNTIME_INCLUDE_LIBC_SYS_DARWIN_H_
#define CORE_RUNTIME_INCLUDE_LIBC_SYS_DARWIN_H_

#include <sprt/c/sys/__sprt_darwin.h>

// clang-format off

#define _OS_UNFAIR_LOCK_FLAG_NONE __SPRT_OS_UNFAIR_LOCK_FLAG_NONE
#define _OS_UNFAIR_LOCK_FLAG_ADAPTIVE_SPIN __SPRT_OS_UNFAIR_LOCK_FLAG_ADAPTIVE_SPIN

#define _OS_SYNC_WAIT_ON_ADDRESS_NONE __SPRT_OS_SYNC_WAIT_ON_ADDRESS_NONE
#define _OS_SYNC_WAIT_ON_ADDRESS_SHARED __SPRT_OS_SYNC_WAIT_ON_ADDRESS_SHARED

#define _OS_SYNC_WAKE_BY_ADDRESS_NONE __SPRT_OS_SYNC_WAKE_BY_ADDRESS_NONE
#define _OS_SYNC_WAKE_BY_ADDRESS_SHARED __SPRT_OS_SYNC_WAKE_BY_ADDRESS_SHARED

#define _OS_CLOCK_MACH_ABSOLUTE_TIME __SPRT_OS_CLOCK_MACH_ABSOLUTE_TIME

#define _OS_UNFAIR_LOCK_INIT __SPRT_OS_UNFAIR_LOCK_INIT

// clang-format on

typedef __SPRT_ID(_os_unfair_lock) _os_unfair_lock;

__SPRT_BEGIN_DECL

SPRT_FORCEINLINE void _os_unfair_lock_lock(__SPRT_ID(_os_unfair_lock) * lock) __SPRT_NOEXCEPT {
	__sprt__os_unfair_lock_lock(lock);
}

SPRT_FORCEINLINE bool _os_unfair_lock_trylock(__SPRT_ID(_os_unfair_lock) * lock) __SPRT_NOEXCEPT {
	return __sprt__os_unfair_lock_trylock(lock);
}

SPRT_FORCEINLINE void _os_unfair_lock_unlock(__SPRT_ID(_os_unfair_lock) * lock) __SPRT_NOEXCEPT {
	__sprt__os_unfair_lock_unlock(lock);
}

SPRT_FORCEINLINE void _os_unfair_lock_assert_owner(
		const __SPRT_ID(_os_unfair_lock) * lock) __SPRT_NOEXCEPT {
	__sprt__os_unfair_lock_assert_owner(lock);
}

SPRT_FORCEINLINE void _os_unfair_lock_assert_not_owner(
		const __SPRT_ID(_os_unfair_lock) * lock) __SPRT_NOEXCEPT {
	__sprt__os_unfair_lock_assert_not_owner(lock);
}

SPRT_FORCEINLINE void _os_unfair_lock_lock_with_flags(__SPRT_ID(_os_unfair_lock) * lock,
		__SPRT_ID(uint32_t) flags) {
	__sprt__os_unfair_lock_lock_with_flags(lock, flags);
}

SPRT_FORCEINLINE int _os_sync_wait_on_address(void *addr, __SPRT_ID(uint64_t) value,
		__SPRT_ID(size_t) size, __SPRT_ID(uint32_t) flags) __SPRT_NOEXCEPT {
	return __sprt__os_sync_wait_on_address(addr, value, size, flags);
}

SPRT_FORCEINLINE int _os_sync_wait_on_address_with_deadline(void *addr, __SPRT_ID(uint64_t) value,
		__SPRT_ID(size_t) size, __SPRT_ID(uint32_t) flags, __SPRT_ID(uint32_t) clockid,
		__SPRT_ID(uint64_t) deadline) __SPRT_NOEXCEPT {
	return __sprt__os_sync_wait_on_address_with_deadline(addr, value, size, flags, clockid,
			deadline);
}

SPRT_FORCEINLINE int _os_sync_wait_on_address_with_timeout(void *addr, __SPRT_ID(uint64_t) value,
		__SPRT_ID(size_t) size, __SPRT_ID(uint32_t) flags, __SPRT_ID(uint32_t) clockid,
		__SPRT_ID(uint64_t) timeout_ns) __SPRT_NOEXCEPT {
	return __sprt__os_sync_wait_on_address_with_timeout(addr, value, size, flags, clockid,
			timeout_ns);
}

SPRT_FORCEINLINE int _os_sync_wake_by_address_any(void *addr, __SPRT_ID(size_t) size,
		__SPRT_ID(uint32_t) flags) __SPRT_NOEXCEPT {
	return __sprt__os_sync_wake_by_address_any(addr, size, flags);
}

SPRT_FORCEINLINE int _os_sync_wake_by_address_all(void *addr, __SPRT_ID(size_t) size,
		__SPRT_ID(uint32_t) flags) __SPRT_NOEXCEPT {
	return __sprt__os_sync_wake_by_address_all(addr, size, flags);
}

__SPRT_END_DECL

#endif // CORE_RUNTIME_INCLUDE_LIBC_SYS_DARWIN_H_
