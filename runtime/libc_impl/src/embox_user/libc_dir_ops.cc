
// Embox EL0 directory fd ops.
//
// getdents64(61) is M2, so there is no way to read a directory from EL0 yet and
// no directory descriptor is ever created (opendir in libc_path.cc fails with
// ENOSYS). This table exists because __libc's constructor installs it
// unconditionally; every entry is the EBADF a stream operation on a directory
// would give anyway.
//
// When getdents64 lands, this is where the batch reader goes -- and opendir
// starts producing handles that reach it.

#include "../../include/__impl_libc.h"

namespace sprt {

static ssize_t __dir_read(__fd_slot *fp, void *buf, size_t nbytes, off64_t *offset,
		uint32_t flags) {
	(void)fp;
	(void)buf;
	(void)nbytes;
	(void)offset;
	(void)flags;
	__sprt_errno = EBADF;
	return -1;
}

static ssize_t __dir_write(__fd_slot *fp, const void *buf, size_t nbytes, off64_t *offset,
		uint32_t flags) {
	(void)fp;
	(void)buf;
	(void)nbytes;
	(void)offset;
	(void)flags;
	__sprt_errno = EBADF;
	return -1;
}

static int __dir_close(__fd_slot *fp) {
	(void)fp;
	__sprt_errno = EBADF;
	return -1;
}

static int __dir_dup(__fd_slot *fp, int *target, uint32_t flags) {
	(void)fp;
	(void)target;
	(void)flags;
	__sprt_errno = EBADF;
	return -1;
}

static int __dir_ioctl(__fd_slot *fp, int fd, int cmd, intptr_t arg, __fd_ctl_mode mode) {
	(void)fp;
	(void)fd;
	(void)cmd;
	(void)arg;
	(void)mode;
	__sprt_errno = EBADF;
	return -1;
}

void __libc::load_dir_fd_ops(__fd_ops *ops) {
	ops->mask = __fd_ops_mask::opendir;
	ops->fo_read = &__dir_read;
	ops->fo_write = &__dir_write;
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
