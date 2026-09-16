// Embox EL0 directories: the __fd_ops table for a directory descriptor, and the
// DIR stream over getdents64(61).
//
// WHAT A DIRECTORY DESCRIPTOR IS HERE. Not Embox's -- Embox has none. Its open()
// asserts on O_DIRECTORY rather than refusing it, and all it offers is
// opendir/readdir over a DIR*, which cannot be closed, stat'ed or polled. So the
// kernel invents a descriptor, out of a reserved high range, and serves close,
// fstat, getdents64 and dup for it itself. That number is not the small integer
// Linux hands back; nothing here depends on it being one, and the libc fd a
// caller sees is this layer's own number anyway.
//
// WHAT readdir() CANNOT PROMISE. The kernel's cursor is a count of entries, not
// an offset anything can be resumed from: Embox has no seekdir and its
// rewinddir is a printk stub. So telldir answers with that count, and rewinddir
// and seekdir get back to a position the only way available -- reopen the
// directory and step forward. Both are O(n), and both are correct, which is the
// trade this platform offers.

#include "../../include/__impl_libc.h"
#include "kstat.h"

#include <sprt/c/__sprt_dirent.h>
#include <sprt/c/__sprt_errno.h>
#include <sprt/c/__sprt_fcntl.h>
#include <sprt/c/__sprt_stdlib.h>

#include "../../../core/include/__el0_syscall.h"

namespace sprt {

void *__el0_handle(int kfd);        // libc_file_ops.cc
int __el0_kfd(const __fd_slot *fp); // libc_file_ops.cc

// --- the descriptor ---------------------------------------------------------

static ssize_t __dir_read(__fd_slot *fp, void *buf, size_t nbytes, off64_t *offset,
		uint32_t flags) {
	(void)fp;
	(void)buf;
	(void)nbytes;
	(void)offset;
	(void)flags;
	// What Linux answers for read() of a directory, and what the kernel answers
	// if a call gets that far.
	__sprt_errno = EISDIR;
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
	auto ret = (int)__el0_ret(__el0_close(__el0_kfd(fp)));
	// Released whatever the kernel said: a close that reports an error has
	// still consumed the descriptor.
	fp->handle = nullptr;
	return ret;
}

static int __dir_stat(__fd_slot *fp, struct __SPRT_STAT_NAME *st) {
	__el0_kstat ks;
	if (__el0_ret(__el0_fstat(__el0_kfd(fp), &ks)) < 0) {
		return -1;
	}
	__el0_kstat_to_stat(ks, st);
	return 0;
}

static int __dir_dup(__fd_slot *fp, int *target, uint32_t flags) {
	(void)fp;
	(void)target;
	(void)flags;
	// A duplicated directory descriptor would be two streams sharing one
	// kernel cursor, and the cursor is the only state a directory descriptor
	// has. Refusing beats handing out two halves of one enumeration.
	__sprt_errno = EBADF;
	return -1;
}

static int __dir_ioctl(__fd_slot *fp, int fd, int cmd, intptr_t arg, __fd_ctl_mode mode) {
	(void)fd;
	(void)arg;
	if (mode == __fd_ctl_mode::fnctl) {
		switch (cmd) {
		case __SPRT_F_GETFL: return (int)fp->flags;
		case __SPRT_F_GETFD: return 0; // there is no exec at EL0 (decision D5)
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
	ops->fo_readv = nullptr;
	ops->fo_writev = nullptr;
	ops->fo_seek = nullptr;
	ops->fo_stat = &__dir_stat;
	ops->fo_chmod = nullptr;
}

// --- the stream -------------------------------------------------------------

// One getdents64 per refill. Embox caps a name at 32 characters, so 2 KiB holds
// around forty entries and a directory of any size this image can hold is a
// handful of syscalls.
static constexpr size_t EL0_DIR_BUF = 2'048;

// The path is kept so rewinddir and seekdir have something to reopen, and it is
// bounded by the kernel's own PATH_MAX rather than by the libc's: a longer path
// could not have been opened in the first place.
static constexpr size_t EL0_DIR_PATH = 128;

} // namespace sprt

// At global scope, because the public typedef is: __SPRT_ID(DIR) names
// ::__dirstream, and a definition inside namespace sprt would be a different
// type that nothing else could see.
struct __dirstream {
	int fd;    // the libc descriptor, which is what dirfd() reports
	int pos;   // read cursor within buf
	int end;   // valid bytes in buf
	long tell; // entries returned so far -- what telldir answers
	bool at_end;
	char path[sprt::EL0_DIR_PATH];
	char buf[sprt::EL0_DIR_BUF];
};

namespace sprt {

static __SPRT_ID(DIR) * __el0_dir_new(int fd, const char *path) {
	auto d = (__SPRT_ID(DIR) *)__sprt_malloc(sizeof(__SPRT_ID(DIR)));
	if (!d) {
		__sprt_errno = ENOMEM;
		return nullptr;
	}
	d->fd = fd;
	d->pos = 0;
	d->end = 0;
	d->tell = 0;
	d->at_end = false;
	d->path[0] = '\0';
	if (path) {
		size_t n = __builtin_strlen(path);
		if (n < sizeof(d->path)) {
			__builtin_memcpy(d->path, path, n + 1);
		}
	}
	return d;
}

// The kernel descriptor behind a stream. Every syscall goes through this rather
// than through the libc number, which the kernel has never heard of.
static int __el0_dir_kfd(__SPRT_ID(DIR) * d) {
	auto libc = __libc::get();
	auto slot = libc->get_fd_slot(d->fd);
	if (!slot || !slot->handle) {
		return -1;
	}
	return __el0_kfd(slot);
}

// Start the directory over. There is no rewinddir behind a directory descriptor
// here and no lseek either -- the kernel's cursor only moves forward -- so the
// stream is reopened. Without a remembered path (fdopendir adopts a descriptor
// somebody else opened) there is nothing to reopen, and the caller is told so
// rather than left believing the stream was rewound.
static int __el0_dir_restart(__SPRT_ID(DIR) * d) {
	if (!d->path[0]) {
		__sprt_errno = ENOSYS;
		return -1;
	}

	auto kfd = (int)__el0_ret(
			__el0_openat(__SPRT_AT_FDCWD, d->path, __SPRT_O_RDONLY | __SPRT_O_DIRECTORY, 0));
	if (kfd < 0) {
		return -1;
	}

	auto libc = __libc::get();
	auto slot = libc->get_fd_slot(d->fd);
	if (!slot) {
		__el0_close(kfd);
		__sprt_errno = EBADF;
		return -1;
	}
	if (slot->handle && slot->ops && slot->ops->fo_close) {
		slot->ops->fo_close(slot);
	}
	slot->handle = __el0_handle(kfd);
	slot->ops = &libc->fdDirOps;
	slot->flags = __SPRT_O_RDONLY;
	slot->mode = 0;

	d->pos = 0;
	d->end = 0;
	d->tell = 0;
	d->at_end = false;
	return 0;
}

} // namespace sprt

__SPRT_C_FUNC __SPRT_ID(DIR) * opendir(const char *path) __SPRT_NOEXCEPT {
	using namespace sprt;

	if (!path || !*path) {
		__sprt_errno = path ? ENOENT : EFAULT;
		return nullptr;
	}

	auto kfd = (int)__el0_ret(
			__el0_openat(__SPRT_AT_FDCWD, path, __SPRT_O_RDONLY | __SPRT_O_DIRECTORY, 0));
	if (kfd < 0) {
		return nullptr;
	}

	auto libc = __libc::get();
	int fd = libc->create_fd(__el0_handle(kfd), &libc->fdDirOps, __SPRT_O_RDONLY, 0);
	if (fd < 0) {
		__el0_close(kfd);
		__sprt_errno = EMFILE;
		return nullptr;
	}

	auto d = __el0_dir_new(fd, path);
	if (!d) {
		auto slot = libc->get_fd_slot(fd);
		slot->ops->fo_close(slot);
		libc->release_fd(fd);
		return nullptr;
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
	return __el0_dir_new(fd, nullptr);
}

__SPRT_C_FUNC int closedir(__SPRT_ID(DIR) * d) __SPRT_NOEXCEPT {
	using namespace sprt;

	if (!d) {
		__sprt_errno = EBADF;
		return -1;
	}

	auto libc = __libc::get();
	auto slot = libc->get_fd_slot(d->fd);
	int ret = 0;
	if (slot && slot->handle && slot->ops && slot->ops->fo_close) {
		ret = slot->ops->fo_close(slot);
		libc->release_fd(d->fd);
	}
	__sprt_free(d);
	return ret;
}

__SPRT_C_FUNC struct __SPRT_DIRENT_NAME *readdir(__SPRT_ID(DIR) * d) __SPRT_NOEXCEPT {
	using namespace sprt;

	if (!d) {
		__sprt_errno = EBADF;
		return nullptr;
	}

	if (d->pos >= d->end) {
		if (d->at_end) {
			return nullptr; // the end of a directory is not an error
		}
		int kfd = __el0_dir_kfd(d);
		if (kfd < 0) {
			__sprt_errno = EBADF;
			return nullptr;
		}
		auto n = __el0_ret(__el0_getdents64(kfd, d->buf, sizeof(d->buf)));
		if (n < 0) {
			return nullptr;
		}
		if (n == 0) {
			d->at_end = true;
			return nullptr;
		}
		d->pos = 0;
		d->end = (int)n;
	}

	// The wire record and struct dirent agree field for field; only d_name
	// differs, being as long as the name rather than a fixed 256. Handing back
	// a pointer into the buffer is what a libc does here -- and it is why the
	// pointer is valid only until the next readdir on this stream.
	auto e = (struct __SPRT_DIRENT_NAME *)(d->buf + d->pos);
	if (e->d_reclen < 20 || (d->pos + (int)e->d_reclen) > d->end) {
		// A record that cannot hold its own header, or that claims to run past
		// what the kernel wrote, would make the walk step nowhere or out of
		// the buffer. Stopping is the only safe answer.
		d->at_end = true;
		__sprt_errno = EIO;
		return nullptr;
	}
	d->pos += e->d_reclen;
	d->tell++;
	return e;
}

__SPRT_C_FUNC long telldir(__SPRT_ID(DIR) * d) __SPRT_NOEXCEPT {
	if (!d) {
		__sprt_errno = EBADF;
		return -1;
	}
	return d->tell;
}

__SPRT_C_FUNC int dirfd(__SPRT_ID(DIR) * d) __SPRT_NOEXCEPT {
	if (!d) {
		__sprt_errno = EBADF;
		return -1;
	}
	return d->fd;
}

__SPRT_C_FUNC int rewinddir(__SPRT_ID(DIR) * d) __SPRT_NOEXCEPT {
	if (!d) {
		__sprt_errno = EBADF;
		return -1;
	}
	return sprt::__el0_dir_restart(d);
}

// seekdir only ever receives a value telldir produced, which here is a count of
// entries. Getting back to it therefore means starting over and stepping
// forward -- O(n), and the only thing a forward-only cursor allows.
__SPRT_C_FUNC int seekdir(__SPRT_ID(DIR) * d, long loc) __SPRT_NOEXCEPT {
	if (!d || loc < 0) {
		__sprt_errno = d ? EINVAL : EBADF;
		return -1;
	}
	if (sprt::__el0_dir_restart(d) != 0) {
		return -1;
	}
	while (d->tell < loc) {
		if (!readdir(d)) {
			break; // past the end: the stream stops there, as it would anyway
		}
	}
	return 0;
}
