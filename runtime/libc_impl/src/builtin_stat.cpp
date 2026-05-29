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

#include <sprt/c/sys/__sprt_stat.h>

#if SPRT_WINDOWS
#include "windows/stat.cc"
#endif

#include "../include/__impl_libc.h"
#include "sys/stat.h" // IWYU pragma: keep

namespace sprt {

__SPRT_C_FUNC int fstat(int __fd, struct __SPRT_STAT_NAME *__SPRT_RESTRICT __stat) __SPRT_NOEXCEPT {
	auto libc = __libc::get();
	auto fdSlot = libc->get_fd_slot(__fd);
	if (!fdSlot || !fdSlot->handle || !fdSlot->ops->fo_stat) {
		__sprt_errno = EBADF;
		return -1;
	}

	return fdSlot->ops->fo_stat(fdSlot, __stat);
}

__SPRT_C_FUNC int fchmod(int __fd, __SPRT_ID(mode_t) mode) __SPRT_NOEXCEPT {
	auto libc = __libc::get();
	auto fdSlot = libc->get_fd_slot(__fd);
	if (!fdSlot || !fdSlot->handle || !fdSlot->ops->fo_chmod) {
		__sprt_errno = EBADF;
		return -1;
	}

	return fdSlot->ops->fo_chmod(fdSlot, mode);
}

__SPRT_C_FUNC int futimens(int __fd, const struct __SPRT_TIMESPEC_NAME *times) __SPRT_NOEXCEPT {
	auto libc = __libc::get();
	auto fdSlot = libc->get_fd_slot(__fd);
	if (!fdSlot || !fdSlot->handle || !fdSlot->ops->fo_utimens) {
		__sprt_errno = EBADF;
		return -1;
	}

	return fdSlot->ops->fo_utimens(fdSlot, times);
}

__SPRT_C_FUNC mode_t umask(mode_t mask) __SPRT_NOEXCEPT {
	mask &= 0777;
	return __libc::get()->umask.exchange(mask);
}

} // namespace sprt
