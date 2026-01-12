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

#include <sprt/c/sys/__sprt_uring.h>
#include <sprt/c/cross/__sprt_signal.h>
#include <sprt/c/__sprt_errno.h>

namespace sprt {

#if __SPRT_CONFIG_HAVE_URING

__SPRT_C_FUNC int __SPRT_ID(io_uring_setup)(unsigned entries, struct io_uring_params *p) {
	return (int)syscall(SYS_IO_URING_SETUP, entries, p);
}

__SPRT_C_FUNC int __SPRT_ID(io_uring_enter)(int ring_fd, unsigned int to_submit,
		unsigned int min_complete, unsigned int flags, const __SPRT_ID(sigset_t) * sig) {
	return (int)syscall(SYS_IO_URING_ENTER, ring_fd, to_submit, min_complete, flags, sig,
			__SPRT__NSIG / 8);
}

__SPRT_C_FUNC int __SPRT_ID(io_uring_enter2)(int ring_fd, unsigned int to_submit,
		unsigned int min_complete, unsigned int flags, void *arg, __SPRT_ID(size_t) argsize) {
	return (int)syscall(SYS_IO_URING_ENTER, ring_fd, to_submit, min_complete, flags, arg, argsize);
}

__SPRT_C_FUNC int __SPRT_ID(io_uring_register)(unsigned int fd, unsigned int opcode,
		const void *arg, unsigned int nr_args) {
	return (int)syscall(SYS_IO_URING_REGISTER, fd, opcode, arg, nr_args);
}

#else

__SPRT_C_FUNC int __SPRT_ID(io_uring_setup)(unsigned entries, struct io_uring_params *p) {
	*__sprt___errno_location() = ENOSYS;
	return -1;
}

__SPRT_C_FUNC int __SPRT_ID(io_uring_enter)(int ring_fd, unsigned int to_submit,
		unsigned int min_complete, unsigned int flags, const __SPRT_ID(sigset_t) * sig) {
	*__sprt___errno_location() = ENOSYS;
	return -1;
}

__SPRT_C_FUNC int __SPRT_ID(io_uring_enter2)(int ring_fd, unsigned int to_submit,
		unsigned int min_complete, unsigned int flags, void *arg, __SPRT_ID(size_t) argsize) {
	*__sprt___errno_location() = ENOSYS;
	return -1;
}

__SPRT_C_FUNC int __SPRT_ID(io_uring_register)(unsigned int fd, unsigned int opcode,
		const void *arg, unsigned int nr_args) {
	*__sprt___errno_location() = ENOSYS;
	return -1;
}

#endif

} // namespace sprt
