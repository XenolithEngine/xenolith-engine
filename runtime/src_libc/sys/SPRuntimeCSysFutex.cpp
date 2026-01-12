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

#include "private/SPRTSpecific.h"

#include <sprt/c/sys/__sprt_futex.h>
#include <sprt/c/__sprt_errno.h>

namespace sprt {

#if __SPRT_CONFIG_HAVE_FUTEX

// FUTEX_WAKE(2const): https://man7.org/linux/man-pages/man2/FUTEX_WAKE.2const.html
__SPRT_C_FUNC long __SPRT_ID(futex_wake)(volatile __SPRT_ID(uint32_t) * uaddr,
		__SPRT_ID(uint32_t) flags, __SPRT_ID(uint32_t) val) {
	return ::syscall(SYS_FUTEX_V1, uaddr, __SPRT_FUTEX_WAKE | (flags & __SPRT_FUTEX_FLAG_MASK),
			val);
}

// FUTEX_WAIT(2const): https://man7.org/linux/man-pages/man2/FUTEX_WAIT.2const.html
__SPRT_C_FUNC long __SPRT_ID(futex_wait)(volatile __SPRT_ID(uint32_t) * uaddr,
		__SPRT_ID(uint32_t) flags, __SPRT_ID(uint32_t) val, __SPRT_TIMESPEC_NAME *timespec) {
	return ::syscall(SYS_FUTEX_V1, uaddr, __SPRT_FUTEX_WAIT | (flags & __SPRT_FUTEX_FLAG_MASK), val,
			timespec);
}

// FUTEX_WAIT_BITSET(2const): https://man7.org/linux/man-pages/man2/FUTEX_WAIT_BITSET.2const.html
__SPRT_C_FUNC long __SPRT_ID(futex_wait_bitset)(volatile __SPRT_ID(uint32_t) * uaddr,
		__SPRT_ID(uint32_t) flags, __SPRT_ID(uint32_t) val, __SPRT_TIMESPEC_NAME *timespec,
		__SPRT_ID(uint32_t) bitset) {
	return ::syscall(SYS_FUTEX_V1, uaddr,
			__SPRT_FUTEX_WAIT_BITSET | (flags & __SPRT_FUTEX_FLAG_MASK), val, timespec, nullptr,
			bitset);
}

// FUTEX_WAIT_BITSET(2const): https://man7.org/linux/man-pages/man2/FUTEX_WAIT_BITSET.2const.html
__SPRT_C_FUNC long __SPRT_ID(futex_wake_bitset)(volatile __SPRT_ID(uint32_t) * uaddr,
		__SPRT_ID(uint32_t) flags, __SPRT_ID(uint32_t) val, __SPRT_ID(uint32_t) bitset) {
	return ::syscall(SYS_FUTEX_V1, uaddr,
			__SPRT_FUTEX_WAKE_BITSET | (flags & __SPRT_FUTEX_FLAG_MASK), val, nullptr, nullptr,
			bitset);
}

// FUTEX_LOCK_PI(2const): https://man7.org/linux/man-pages/man2/FUTEX_LOCK_PI.2const.html
__SPRT_C_FUNC long __SPRT_ID(futex_lock_pi)(volatile __SPRT_ID(uint32_t) * uaddr,
		__SPRT_ID(uint32_t) flags, __SPRT_TIMESPEC_NAME *timespec) {
	return ::syscall(SYS_FUTEX_V1, uaddr, __SPRT_FUTEX_LOCK_PI | (flags & __SPRT_FUTEX_FLAG_MASK),
			0, timespec);
}

// FUTEX_TRYLOCK_PI(2const): https://man7.org/linux/man-pages/man2/FUTEX_TRYLOCK_PI.2const.html
__SPRT_C_FUNC long __SPRT_ID(
		futex_trylock_pi)(volatile __SPRT_ID(uint32_t) * uaddr, __SPRT_ID(uint32_t) flags) {
	return ::syscall(SYS_FUTEX_V1, uaddr,
			__SPRT_FUTEX_TRYLOCK_PI | (flags & __SPRT_FUTEX_FLAG_MASK));
}

// FUTEX_UNLOCK_PI(2const): https://man7.org/linux/man-pages/man2/FUTEX_UNLOCK_PI.2const.html
__SPRT_C_FUNC long __SPRT_ID(
		futex_unlock_pi)(volatile __SPRT_ID(uint32_t) * uaddr, __SPRT_ID(uint32_t) flags) {
	return ::syscall(SYS_FUTEX_V1, uaddr,
			__SPRT_FUTEX_UNLOCK_PI | (flags & __SPRT_FUTEX_FLAG_MASK));
}

// Futex2 API

__SPRT_C_FUNC long __SPRT_ID(futex2_wake)(volatile __SPRT_ID(uint32_t) * uaddr,
		__SPRT_ID(uint32_t) bitset, int nr_wake, __SPRT_ID(uint32_t) flags) {
	return syscall(SYS_FUTEX_V2_WAKE, uaddr, bitset, nr_wake, flags);
}

__SPRT_C_FUNC long __SPRT_ID(futex2_wait)(volatile __SPRT_ID(uint32_t) * uaddr,
		__SPRT_ID(uint32_t) val, __SPRT_ID(uint32_t) bitset, __SPRT_ID(uint32_t) flags,
		__SPRT_TIMESPEC_NAME *timespec, __SPRT_ID(clockid_t) clockid) {
	return syscall(SYS_FUTEX_V2_WAIT, uaddr, val, bitset, flags, timespec, clockid);
}

#else

// FUTEX_WAKE(2const): https://man7.org/linux/man-pages/man2/FUTEX_WAKE.2const.html
__SPRT_C_FUNC long __SPRT_ID(futex_wake)(volatile __SPRT_ID(uint32_t) * uaddr,
		__SPRT_ID(uint32_t) flags, __SPRT_ID(uint32_t) val) {
	*__sprt___errno_location() = ENOSYS;
	return -1;
}

// FUTEX_WAIT(2const): https://man7.org/linux/man-pages/man2/FUTEX_WAIT.2const.html
__SPRT_C_FUNC long __SPRT_ID(futex_wait)(volatile __SPRT_ID(uint32_t) * uaddr,
		__SPRT_ID(uint32_t) flags, __SPRT_ID(uint32_t) val, __SPRT_TIMESPEC_NAME *timespec) {
	*__sprt___errno_location() = ENOSYS;
	return -1;
}

// FUTEX_WAIT_BITSET(2const): https://man7.org/linux/man-pages/man2/FUTEX_WAIT_BITSET.2const.html
__SPRT_C_FUNC long __SPRT_ID(futex_wait_bitset)(volatile __SPRT_ID(uint32_t) * uaddr,
		__SPRT_ID(uint32_t) flags, __SPRT_ID(uint32_t) val, __SPRT_TIMESPEC_NAME *timespec,
		__SPRT_ID(uint32_t) bitset) {
	*__sprt___errno_location() = ENOSYS;
	return -1;
}

// FUTEX_WAIT_BITSET(2const): https://man7.org/linux/man-pages/man2/FUTEX_WAIT_BITSET.2const.html
__SPRT_C_FUNC long __SPRT_ID(futex_wake_bitset)(volatile __SPRT_ID(uint32_t) * uaddr,
		__SPRT_ID(uint32_t) flags, __SPRT_ID(uint32_t) val, __SPRT_ID(uint32_t) bitset) {
	*__sprt___errno_location() = ENOSYS;
	return -1;
}

// FUTEX_LOCK_PI(2const): https://man7.org/linux/man-pages/man2/FUTEX_LOCK_PI.2const.html
__SPRT_C_FUNC long __SPRT_ID(futex_lock_pi)(volatile __SPRT_ID(uint32_t) * uaddr,
		__SPRT_ID(uint32_t) flags, __SPRT_TIMESPEC_NAME *timespec) {
	*__sprt___errno_location() = ENOSYS;
	return -1;
}

// FUTEX_TRYLOCK_PI(2const): https://man7.org/linux/man-pages/man2/FUTEX_TRYLOCK_PI.2const.html
__SPRT_C_FUNC long __SPRT_ID(
		futex_trylock_pi)(volatile __SPRT_ID(uint32_t) * uaddr, __SPRT_ID(uint32_t) flags) {
	*__sprt___errno_location() = ENOSYS;
	return -1;
}

// FUTEX_UNLOCK_PI(2const): https://man7.org/linux/man-pages/man2/FUTEX_UNLOCK_PI.2const.html
__SPRT_C_FUNC long __SPRT_ID(
		futex_unlock_pi)(volatile __SPRT_ID(uint32_t) * uaddr, __SPRT_ID(uint32_t) flags) {
	*__sprt___errno_location() = ENOSYS;
	return -1;
}

// Futex2 API

__SPRT_C_FUNC long __SPRT_ID(futex2_wake)(volatile __SPRT_ID(uint32_t) * uaddr,
		__SPRT_ID(uint32_t) bitset, int nr_wake, __SPRT_ID(uint32_t) flags) {
	*__sprt___errno_location() = ENOSYS;
	return -1;
}

__SPRT_C_FUNC long __SPRT_ID(futex2_wait)(volatile __SPRT_ID(uint32_t) * uaddr,
		__SPRT_ID(uint32_t) val, __SPRT_ID(uint32_t) bitset, __SPRT_ID(uint32_t) flags,
		__SPRT_TIMESPEC_NAME *timespec, __SPRT_ID(clockid_t) clockid) {
	*__sprt___errno_location() = ENOSYS;
	return -1;
}

#endif

} // namespace sprt
