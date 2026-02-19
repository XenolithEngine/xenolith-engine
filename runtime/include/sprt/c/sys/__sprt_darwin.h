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

#ifndef CORE_RUNTIME_INCLUDE_C_SYS___SPRT_DARWIN_H_
#define CORE_RUNTIME_INCLUDE_C_SYS___SPRT_DARWIN_H_

#include <sprt/c/cross/__sprt_config.h>
#include <sprt/c/bits/__sprt_uint32_t.h>
#include <sprt/c/bits/__sprt_uint64_t.h>
#include <sprt/c/bits/__sprt_size_t.h>

// clang-format off

#define __SPRT_OS_UNFAIR_LOCK_FLAG_NONE 0x00000000U
#define __SPRT_OS_UNFAIR_LOCK_FLAG_ADAPTIVE_SPIN 0x00040000U

#define __SPRT_OS_SYNC_WAIT_ON_ADDRESS_NONE  0x00000000U
#define __SPRT_OS_SYNC_WAIT_ON_ADDRESS_SHARED 0x00000001U

#define __SPRT_OS_SYNC_WAKE_BY_ADDRESS_NONE  0x00000000U
#define __SPRT_OS_SYNC_WAKE_BY_ADDRESS_SHARED 0x00000001U

#define __SPRT_OS_CLOCK_MACH_ABSOLUTE_TIME 32U

#define __SPRT_OS_UNFAIR_LOCK_INIT (0)

// clang-format on

typedef __SPRT_ID(uint32_t) __SPRT_ID(_os_unfair_lock);


__SPRT_BEGIN_DECL

SPRT_API void __SPRT_ID(_os_unfair_lock_lock)(__SPRT_ID(_os_unfair_lock) * lock) __SPRT_NOEXCEPT;

SPRT_API bool __SPRT_ID(_os_unfair_lock_trylock)(__SPRT_ID(_os_unfair_lock) * lock) __SPRT_NOEXCEPT;

SPRT_API void __SPRT_ID(_os_unfair_lock_unlock)(__SPRT_ID(_os_unfair_lock) * lock) __SPRT_NOEXCEPT;

SPRT_API void __SPRT_ID(_os_unfair_lock_assert_owner)(
		const __SPRT_ID(_os_unfair_lock) * lock) __SPRT_NOEXCEPT;

SPRT_API void __SPRT_ID(_os_unfair_lock_assert_not_owner)(
		const __SPRT_ID(_os_unfair_lock) * lock) __SPRT_NOEXCEPT;

SPRT_API void __SPRT_ID(_os_unfair_lock_lock_with_flags)(__SPRT_ID(_os_unfair_lock) * lock,
		__SPRT_ID(uint32_t) flags);

SPRT_API int __SPRT_ID(_os_sync_wait_on_address)(void *addr, __SPRT_ID(uint64_t) value,
		__SPRT_ID(size_t) size, __SPRT_ID(uint32_t) flags) __SPRT_NOEXCEPT;

SPRT_API int __SPRT_ID(_os_sync_wait_on_address_with_deadline)(void *addr,
		__SPRT_ID(uint64_t) value, __SPRT_ID(size_t) size, __SPRT_ID(uint32_t) flags,
		__SPRT_ID(uint32_t) clockid, __SPRT_ID(uint64_t) deadline) __SPRT_NOEXCEPT;

SPRT_API int __SPRT_ID(_os_sync_wait_on_address_with_timeout)(void *addr, __SPRT_ID(uint64_t) value,
		__SPRT_ID(size_t) size, __SPRT_ID(uint32_t) flags, __SPRT_ID(uint32_t) clockid,
		__SPRT_ID(uint64_t) timeout_ns) __SPRT_NOEXCEPT;

SPRT_API int __SPRT_ID(_os_sync_wake_by_address_any)(void *addr, __SPRT_ID(size_t) size,
		__SPRT_ID(uint32_t) flags) __SPRT_NOEXCEPT;

SPRT_API int __SPRT_ID(_os_sync_wake_by_address_all)(void *addr, __SPRT_ID(size_t) size,
		__SPRT_ID(uint32_t) flags) __SPRT_NOEXCEPT;

__SPRT_END_DECL

#endif // CORE_RUNTIME_INCLUDE_C_SYS___SPRT_DARWIN_H_
