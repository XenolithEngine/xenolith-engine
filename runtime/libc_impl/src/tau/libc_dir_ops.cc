// Directories on Tau: no file system service yet, every call fails with ENOSYS.

#include "../../include/__impl_libc.h"

#include <sprt/c/__sprt_dirent.h>
#include <sprt/c/__sprt_errno.h>

namespace sprt {

static int __dir_close(__fd_slot *) { return 0; }

void __libc::load_dir_fd_ops(__fd_ops *ops) {
	ops->mask = __fd_ops_mask::opendir;
	ops->fo_close = &__dir_close;
}

} // namespace sprt

__SPRT_C_FUNC __SPRT_ID(DIR) * opendir(const char *) __SPRT_NOEXCEPT {
	__sprt_errno = ENOSYS;
	return nullptr;
}

__SPRT_C_FUNC __SPRT_ID(DIR) * fdopendir(int) __SPRT_NOEXCEPT {
	__sprt_errno = ENOSYS;
	return nullptr;
}

__SPRT_C_FUNC int closedir(__SPRT_ID(DIR) *) __SPRT_NOEXCEPT {
	__sprt_errno = EBADF;
	return -1;
}

__SPRT_C_FUNC struct __SPRT_DIRENT_NAME *readdir(__SPRT_ID(DIR) *) __SPRT_NOEXCEPT {
	__sprt_errno = EBADF;
	return nullptr;
}

__SPRT_C_FUNC long telldir(__SPRT_ID(DIR) *) __SPRT_NOEXCEPT {
	__sprt_errno = EBADF;
	return -1;
}

__SPRT_C_FUNC int dirfd(__SPRT_ID(DIR) *) __SPRT_NOEXCEPT {
	__sprt_errno = EBADF;
	return -1;
}

__SPRT_C_FUNC int rewinddir(__SPRT_ID(DIR) *) __SPRT_NOEXCEPT { return 0; }

__SPRT_C_FUNC int seekdir(__SPRT_ID(DIR) *, long) __SPRT_NOEXCEPT { return 0; }
