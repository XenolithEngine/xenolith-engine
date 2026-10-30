// Directories on Tau: entries of tau.fs through libtau (sprt_tau.h), read a buffer at a time
// and handed out one struct dirent each; the position is the cookie of the last entry read.

#include "tau_fs.h"

#include <sprt/c/__sprt_dirent.h>
#include <sprt/c/__sprt_errno.h>
#include <sprt/c/__sprt_fcntl.h>
#include <sprt/c/__sprt_stdlib.h>

namespace sprt {

static ssize_t __dir_read(__fd_slot *, void *, size_t, off64_t *, uint32_t) {
	__sprt_errno = EISDIR;
	return -1;
}

static ssize_t __dir_write(__fd_slot *, const void *, size_t, off64_t *, uint32_t) {
	__sprt_errno = EBADF;
	return -1;
}

static int __dir_close(__fd_slot *fp) {
	auto f = static_cast<__tau_fd *>(fp->handle);
	int r = (int)__tau_ret(__sprt_tau_fs_close(f->file));
	__sprt_free(f);
	fp->handle = nullptr;
	return r < 0 ? -1 : 0;
}

static int __dir_stat(__fd_slot *fp, struct __SPRT_STAT_NAME *st) {
	__sprt_tau_fs_stat s;
	if (__tau_ret(__sprt_tau_fs_fstat(static_cast<__tau_fd *>(fp->handle)->file, &s)) < 0) {
		return -1;
	}
	__tau_to_stat(s, st);
	return 0;
}

static int __dir_dup(__fd_slot *, int *, uint32_t) {
	__sprt_errno = EBADF;
	return -1;
}

static int __dir_ioctl(__fd_slot *fp, int, int cmd, intptr_t, __fd_ctl_mode mode) {
	if (mode == __fd_ctl_mode::fnctl) {
		switch (cmd) {
		case __SPRT_F_GETFL: return (int)fp->flags;
		case __SPRT_F_GETFD:
		case __SPRT_F_SETFD: return 0;
		default: __sprt_errno = EINVAL; return -1;
		}
	}
	__sprt_errno = ENOTTY;
	return -1;
}

void __libc::load_dir_fd_ops(__fd_ops *ops) {
	ops->mask = __fd_ops_mask::opendir;
	ops->fo_read = &__dir_read;
	ops->fo_write = &__dir_write;
	ops->fo_close = &__dir_close;
	ops->fo_dup = &__dir_dup;
	ops->fo_ioctl = &__dir_ioctl;
	ops->fo_stat = &__dir_stat;
}

static constexpr size_t TAU_DIR_BUF = 2'048;

static constexpr unsigned char dtypes[] = {__SPRT_DT_UNKNOWN, __SPRT_DT_REG, __SPRT_DT_DIR, __SPRT_DT_LNK, __SPRT_DT_CHR,
	__SPRT_DT_BLK, __SPRT_DT_FIFO, __SPRT_DT_SOCK};

} // namespace sprt

struct __dirstream {
	int fd;
	bool owned; // opendir made the descriptor
	bool atEnd; // the buffer holds the last entries
	unsigned pos;
	unsigned end;
	unsigned long long cookie; // where the next read starts
	unsigned long long tell; // after the last entry handed out
	struct __SPRT_DIRENT_NAME entry;
	char buf[sprt::TAU_DIR_BUF];
};

namespace sprt {

static __SPRT_ID(DIR) * __tau_dir(int fd, bool owned) {
	auto d = (__SPRT_ID(DIR) *)__sprt_malloc(sizeof(__SPRT_ID(DIR)));
	if (!d) {
		__sprt_errno = ENOMEM;
		return nullptr;
	}
	d->fd = fd;
	d->owned = owned;
	d->atEnd = false;
	d->pos = d->end = 0;
	d->cookie = d->tell = 0;
	return d;
}

static __tau_fd *__tau_dir_fd(__SPRT_ID(DIR) * d) {
	auto slot = __libc::get()->get_fd_slot(d->fd);
	return slot && slot->handle ? static_cast<__tau_fd *>(slot->handle) : nullptr;
}

} // namespace sprt

__SPRT_C_FUNC __SPRT_ID(DIR) * opendir(const char *path) __SPRT_NOEXCEPT {
	using namespace sprt;
	auto libc = __libc::get();
	int fd = __tau_open_fd(__SPRT_AT_FDCWD, path, __SPRT_O_RDONLY | __SPRT_O_DIRECTORY, 0, &libc->fdDirOps);
	if (fd < 0) {
		return nullptr;
	}
	auto d = __tau_dir(fd, true);
	if (!d) {
		close(fd);
	}
	return d;
}

__SPRT_C_FUNC __SPRT_ID(DIR) * fdopendir(int fd) __SPRT_NOEXCEPT {
	using namespace sprt;
	auto libc = __libc::get();
	auto slot = libc->get_fd_slot(fd);
	if (!slot || !slot->handle) {
		__sprt_errno = EBADF;
		return nullptr;
	}
	if (slot->ops != &libc->fdDirOps && slot->ops != __tau_fs_ops()) {
		__sprt_errno = ENOTDIR;
		return nullptr;
	}
	return __tau_dir(fd, false);
}

// The descriptor goes with the stream, also one fdopendir took over.
__SPRT_C_FUNC int closedir(__SPRT_ID(DIR) * d) __SPRT_NOEXCEPT {
	if (!d) {
		__sprt_errno = EBADF;
		return -1;
	}
	int r = close(d->fd);
	__sprt_free(d);
	return r;
}

__SPRT_C_FUNC struct __SPRT_DIRENT_NAME *readdir(__SPRT_ID(DIR) * d) __SPRT_NOEXCEPT {
	using namespace sprt;
	if (!d) {
		__sprt_errno = EBADF;
		return nullptr;
	}
	if (d->pos >= d->end) {
		if (d->atEnd) {
			return nullptr;
		}
		__tau_fd *f = __tau_dir_fd(d);
		if (!f) {
			__sprt_errno = EBADF;
			return nullptr;
		}
		unsigned long long cookie = d->cookie;
		long n = __tau_ret(__sprt_tau_fs_readdir(f->file, &cookie, d->buf, sizeof(d->buf)));
		if (n < 0) {
			return nullptr;
		}
		d->cookie = cookie;
		d->atEnd = cookie == 0;
		d->pos = 0;
		d->end = (unsigned)n;
		if (!n) {
			d->atEnd = true;
			return nullptr;
		}
	}
	__sprt_tau_fs_entry e;
	__builtin_memcpy(&e, d->buf + d->pos, sizeof(e));
	if (e.reclen < sizeof(e) + e.namelen || d->pos + e.reclen > d->end) {
		d->atEnd = true;
		d->pos = d->end;
		__sprt_errno = EIO;
		return nullptr;
	}
	d->entry.d_ino = e.ino;
	d->entry.d_off = (off_t)e.cookie;
	d->entry.d_reclen = sizeof(d->entry);
	d->entry.d_type = e.type < sizeof(dtypes) ? dtypes[e.type] : __SPRT_DT_UNKNOWN;
	__builtin_memcpy(d->entry.d_name, d->buf + d->pos + sizeof(e), e.namelen);
	d->entry.d_name[e.namelen] = 0;
	d->pos += e.reclen;
	d->tell = e.cookie;
	return &d->entry;
}

__SPRT_C_FUNC long telldir(__SPRT_ID(DIR) * d) __SPRT_NOEXCEPT {
	if (!d) {
		__sprt_errno = EBADF;
		return -1;
	}
	return (long)d->tell;
}

__SPRT_C_FUNC int dirfd(__SPRT_ID(DIR) * d) __SPRT_NOEXCEPT {
	if (!d) {
		__sprt_errno = EBADF;
		return -1;
	}
	return d->fd;
}

__SPRT_C_FUNC int seekdir(__SPRT_ID(DIR) * d, long loc) __SPRT_NOEXCEPT {
	if (!d || loc < 0) {
		__sprt_errno = d ? EINVAL : EBADF;
		return -1;
	}
	d->cookie = d->tell = (unsigned long long)loc;
	d->pos = d->end = 0;
	d->atEnd = false;
	return 0;
}

__SPRT_C_FUNC int rewinddir(__SPRT_ID(DIR) * d) __SPRT_NOEXCEPT { return seekdir(d, 0); }

// Relative to the working directory or absolute only: tau.fs has no names of open directories.
__SPRT_C_FUNC int scandirat(int dirfd, const char *path, struct __SPRT_DIRENT_NAME ***list,
		int (*filter)(const struct __SPRT_DIRENT_NAME *),
		int (*compar)(const struct __SPRT_DIRENT_NAME **, const struct __SPRT_DIRENT_NAME **)) __SPRT_NOEXCEPT {
	if (dirfd != __SPRT_AT_FDCWD && path[0] != '/') {
		__sprt_errno = ENOTSUP;
		return -1;
	}
	return __SPRT_ID(scandir)(path, list, filter, compar);
}
