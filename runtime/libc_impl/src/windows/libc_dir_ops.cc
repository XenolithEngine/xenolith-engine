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

#include "../../include/__impl_libc.h"
#include "specific.h"

namespace sprt {

static ssize_t __dir_read(struct __fd_slot *fp, void *buf, size_t nbytes, off64_t *offset,
		uint32_t flags) {
	__sprt_errno = EBADF;
	return -1;
}

static ssize_t __dir_write(struct __fd_slot *fp, const void *buf, size_t nbytes, off64_t *offset,
		uint32_t flags) {
	__sprt_errno = EBADF;
	return -1;
}

static int __dir_close(__fd_slot *fp) {
	__sprt_errno = EBADF;
	return -1;
}

static int __dir_dup(__fd_slot *fp, int *target, uint32_t flags) {
	__sprt_errno = EBADF;
	return -1;
}

static int __dir_ioctl(__fd_slot *fp, int fd, int cmd, intptr_t arg, __fd_ctl_mode mode) {
	__sprt_errno = EBADF;
	return -1;
}

void __libc::load_dir_fd_ops(__fd_ops *ops) {
	ops->mask = __fd_ops_mask::opendir;
	ops->fo_read = __dir_read;
	ops->fo_write = __dir_write;
	ops->fo_close = &__dir_close;
	ops->fo_dup = &__dir_dup;
	ops->fo_ioctl = &__dir_ioctl;
	ops->fo_readv = nullptr;
	ops->fo_writev = nullptr;
	ops->fo_seek = nullptr;
	ops->fo_stat = nullptr;
	ops->fo_chmod = nullptr;
}

} // namespace sprt
