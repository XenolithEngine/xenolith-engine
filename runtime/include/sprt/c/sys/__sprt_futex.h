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

#ifndef CORE_RUNTIME_INCLUDE_C_SYS___SPRT_FUTEX_H_
#define CORE_RUNTIME_INCLUDE_C_SYS___SPRT_FUTEX_H_

#include <sprt/c/cross/__sprt_config.h>
#include <sprt/c/bits/__sprt_uint32_t.h>
#include <sprt/c/bits/__sprt_time_t.h>

/*
	Here we define the Linux library implementation of Futex and Futex2

	It is important to note that the Futex and Futex2 implementations
	are not compatible with each other. If you use futex2_wait you cannot
	use futex_wake to wake up, you must use futex2_wake.

	Note that libc at the time of writing this API does not have support
	functions for futex; it is not an umbrella implementation, but its
	own implementation for Stappler Runtime.

	This implementation is needed primarily because we do not expose 'syscall'
	to third-party applications because it cannot be done in a portable way.
*/

// clang-format off

// Futex version 1 API
#define __SPRT_FUTEX_WAIT				0
#define __SPRT_FUTEX_WAKE				1
#define __SPRT_FUTEX_FD					2
#define __SPRT_FUTEX_REQUEUE			3
#define __SPRT_FUTEX_CMP_REQUEUE		4
#define __SPRT_FUTEX_WAKE_OP			5
#define __SPRT_FUTEX_LOCK_PI			6
#define __SPRT_FUTEX_UNLOCK_PI			7
#define __SPRT_FUTEX_TRYLOCK_PI			8
#define __SPRT_FUTEX_WAIT_BITSET		9
#define __SPRT_FUTEX_WAKE_BITSET		10
#define __SPRT_FUTEX_WAIT_REQUEUE_PI	11
#define __SPRT_FUTEX_CMP_REQUEUE_PI		12

#define __SPRT_FUTEX_PRIVATE_FLAG		128
#define __SPRT_FUTEX_CLOCK_REALTIME		256
#define __SPRT_FUTEX_CMD_MASK	~(__SPRT_FUTEX_PRIVATE_FLAG | __SPRT_FUTEX_CLOCK_REALTIME)
#define __SPRT_FUTEX_FLAG_MASK	(__SPRT_FUTEX_PRIVATE_FLAG | __SPRT_FUTEX_CLOCK_REALTIME)

#define __SPRT_FUTEX_WAITERS	0x80000000
#define __SPRT_FUTEX_OWNER_DIED	0x40000000
#define __SPRT_FUTEX_TID_MASK	0x3fffffff

#define __SPRT_FUTEX_BITSET_MATCH_ANY	0xffffffff

// Futex version 2 API
#define __SPRT_FUTEX2_SIZE_U8 0x00
#define __SPRT_FUTEX2_SIZE_U16 0x01
#define __SPRT_FUTEX2_SIZE_U32 0x02
#define __SPRT_FUTEX2_SIZE_U64 0x03
#define __SPRT_FUTEX2_NUMA 0x04
#define __SPRT_FUTEX2_MPOL 0x08
#define __SPRT_FUTEX2_PRIVATE __SPRT_FUTEX_PRIVATE_FLAG
#define __SPRT_FUTEX2_SIZE_MASK	0x03

#if __SPRT_CONFIG_HAVE_FUTEX || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS

// clang-format on

__SPRT_BEGIN_DECL

// FUTEX_WAKE(2const): https://man7.org/linux/man-pages/man2/FUTEX_WAIT.2const.html
SPRT_API long __SPRT_ID(futex_wake)(volatile __SPRT_ID(uint32_t) * uaddr, __SPRT_ID(uint32_t) flags,
		__SPRT_ID(uint32_t) nr_wake);


// FUTEX_WAIT(2const): https://man7.org/linux/man-pages/man2/FUTEX_WAIT.2const.html
SPRT_API long __SPRT_ID(futex_wait)(volatile __SPRT_ID(uint32_t) * uaddr, __SPRT_ID(uint32_t) flags,
		__SPRT_ID(uint32_t) val, __SPRT_TIMESPEC_NAME *timespec);


// FUTEX_WAIT_BITSET(2const): https://man7.org/linux/man-pages/man2/FUTEX_WAIT_BITSET.2const.html
SPRT_API long __SPRT_ID(futex_wait_bitset)(volatile __SPRT_ID(uint32_t) * uaddr,
		__SPRT_ID(uint32_t) flags, __SPRT_ID(uint32_t) val, __SPRT_TIMESPEC_NAME *timespec,
		__SPRT_ID(uint32_t) bitset);


// FUTEX_WAIT_BITSET(2const): https://man7.org/linux/man-pages/man2/FUTEX_WAIT_BITSET.2const.html
SPRT_API long __SPRT_ID(futex_wake_bitset)(volatile __SPRT_ID(uint32_t) * uaddr,
		__SPRT_ID(uint32_t) flags, __SPRT_ID(uint32_t) val, __SPRT_ID(uint32_t) bitset);


// FUTEX_LOCK_PI(2const): https://man7.org/linux/man-pages/man2/FUTEX_LOCK_PI.2const.html
SPRT_API long __SPRT_ID(futex_lock_pi)(volatile __SPRT_ID(uint32_t) * uaddr,
		__SPRT_ID(uint32_t) flags, __SPRT_TIMESPEC_NAME *timespec);


// FUTEX_TRYLOCK_PI(2const): https://man7.org/linux/man-pages/man2/FUTEX_TRYLOCK_PI.2const.html
SPRT_API long __SPRT_ID(
		futex_trylock_pi)(volatile __SPRT_ID(uint32_t) * uaddr, __SPRT_ID(uint32_t) flags);


// FUTEX_UNLOCK_PI(2const): https://man7.org/linux/man-pages/man2/FUTEX_UNLOCK_PI.2const.html
SPRT_API long __SPRT_ID(
		futex_unlock_pi)(volatile __SPRT_ID(uint32_t) * uaddr, __SPRT_ID(uint32_t) flags);
//long syscall(SYS_futex, uint32_t *uaddr, FUTEX_UNLOCK_PI);


// Futex2 API

SPRT_API long __SPRT_ID(futex2_wake)(volatile __SPRT_ID(uint32_t) * uaddr,
		__SPRT_ID(uint32_t) bitset, int nr_wake, __SPRT_ID(uint32_t) flags);

SPRT_API long __SPRT_ID(futex2_wait)(volatile __SPRT_ID(uint32_t) * uaddr, __SPRT_ID(uint32_t) val,
		__SPRT_ID(uint32_t) bitset, __SPRT_ID(uint32_t) flags, __SPRT_TIMESPEC_NAME *timespec,
		__SPRT_ID(clockid_t) clockid);

__SPRT_END_DECL

#endif

#endif
