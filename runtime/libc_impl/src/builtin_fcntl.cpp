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

#include "fcntl.h"
#include "sys/ioctl.h"
#include "../include/__impl_libc.h"

#if SPRT_WINDOWS
#include "windows/fcntl.cc"
#endif

namespace sprt {

__SPRT_C_FUNC int creat(const char *path, __SPRT_ID(mode_t) __mode) __SPRT_NOEXCEPT {
	return open64(path, __SPRT_O_CREAT | __SPRT_O_WRONLY | __SPRT_O_TRUNC, __mode);
}

static int __fcntl(int __fd, int __cmd, intptr_t arg, __fd_ctl_mode mode) __SPRT_NOEXCEPT {
	auto libc = __libc::get();
	auto fdSlot = libc->get_fd_slot(__fd);
	if (!fdSlot || !fdSlot->handle || !fdSlot->ops->fo_ioctl) {
		__sprt_errno = EBADF;
		return -1;
	}

	return fdSlot->ops->fo_ioctl(fdSlot, __fd, __cmd, arg, mode);
}

__SPRT_C_FUNC int fcntl(int __fd, int __cmd, ...) __SPRT_NOEXCEPT {
	intptr_t arg;
	__sprt_va_list ap;
	__sprt_va_start(ap, __cmd);
	arg = __sprt_va_arg(ap, intptr_t);
	__sprt_va_end(ap);

	return __fcntl(__fd, __cmd, arg, __fd_ctl_mode::fnctl);
}

__SPRT_C_FUNC int ioctl(int __fd, int __cmd, ...) __SPRT_NOEXCEPT {
	intptr_t arg;
	__sprt_va_list ap;
	__sprt_va_start(ap, __cmd);
	arg = __sprt_va_arg(ap, intptr_t);
	__sprt_va_end(ap);

	return __fcntl(__fd, __cmd, arg, __fd_ctl_mode::ioctl);
}

} // namespace sprt
