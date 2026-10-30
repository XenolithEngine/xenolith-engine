// File descriptors of a Tau module: streams of its services, and files of its namespace
// (sprt_tau.h), each with its own position; anonymous mappings come from libtau's map region.

#include "tau_fs.h"

#include <sprt/c/__sprt_errno.h>
#include <sprt/c/sys/__sprt_ioctl.h>
#include <sprt/c/sys/__sprt_mman.h>
#include <sys/ioctl.h>
#include <sprt/c/__sprt_fcntl.h>
#include <sprt/c/__sprt_stdlib.h>
#include <sprt/c/__sprt_string.h>
#include <sprt/c/cross/tau_sprt/sprt_tau.h>

namespace sprt {

static __sprt_tau_stream *__tau_stream(const __fd_slot *fp) { return static_cast<__sprt_tau_stream *>(fp->handle); }

static ssize_t __tau_result(long r) {
	if (r < 0) {
		__sprt_errno = (int)-r;
		return -1;
	}
	return (ssize_t)r;
}

static unsigned __tau_io_flags(const __fd_slot *fp) {
	return (fp->flags & __SPRT_O_NONBLOCK) ? __SPRT_TAU_IO_NONBLOCK : 0;
}

static ssize_t __file_read(__fd_slot *fp, void *buf, size_t nbytes, off64_t *, uint32_t) {
	return __tau_result(__sprt_tau_stream_read(__tau_stream(fp), buf, nbytes, __tau_io_flags(fp)));
}

static ssize_t __file_write(__fd_slot *fp, const void *buf, size_t nbytes, off64_t *, uint32_t) {
	return __tau_result(__sprt_tau_stream_write(__tau_stream(fp), buf, nbytes, __tau_io_flags(fp)));
}

static ssize_t __file_readv(__fd_slot *fp, const __SPRT_IOVEC_NAME *iov, int iovcnt) {
	ssize_t total = 0;
	for (int i = 0; i < iovcnt; ++i) {
		auto n = __file_read(fp, iov[i].iov_base, iov[i].iov_len, nullptr, 0);
		if (n < 0) {
			return total ? total : n;
		}
		total += n;
		if ((size_t)n < iov[i].iov_len) {
			break;
		}
	}
	return total;
}

// One write for what fits a line buffer: stdout flushes its buffer and the new line in two
// pieces, which the console must see together.
static ssize_t __file_writev(__fd_slot *fp, const __SPRT_IOVEC_NAME *iov, int iovcnt) {
	char joined[1'024];
	size_t length = 0;
	for (int i = 0; i < iovcnt && length <= sizeof(joined); ++i) {
		length += iov[i].iov_len;
	}
	if (length <= sizeof(joined)) {
		size_t at = 0;
		for (int i = 0; i < iovcnt; ++i) {
			__builtin_memcpy(joined + at, iov[i].iov_base, iov[i].iov_len);
			at += iov[i].iov_len;
		}
		return __file_write(fp, joined, length, nullptr, 0);
	}
	ssize_t total = 0;
	for (int i = 0; i < iovcnt; ++i) {
		auto n = __file_write(fp, iov[i].iov_base, iov[i].iov_len, nullptr, 0);
		if (n < 0) {
			return total ? total : n;
		}
		total += n;
	}
	return total;
}

static off_t __file_seek(__fd_slot *, off_t, int) {
	__sprt_errno = ESPIPE;
	return -1;
}

static int __file_stat(__fd_slot *, struct __SPRT_STAT_NAME *st) {
	__builtin_memset(st, 0, sizeof(*st));
	st->st_mode = __SPRT_S_IFCHR | 0'666;
	return 0;
}

static int __file_close(__fd_slot *fp) { return (int)__tau_result(__sprt_tau_stream_close(__tau_stream(fp))); }

static int __file_dup(__fd_slot *, int *, uint32_t) {
	__sprt_errno = ENOSYS;
	return -1;
}

// The streams are the module's console, a terminal: stdout then buffers by line, and lines
// of different modules do not break up mid-way.
static int __file_ioctl(__fd_slot *, int, int cmd, intptr_t arg, __fd_ctl_mode) {
	if (cmd == __SPRT_TIOCGWINSZ && arg) {
		auto ws = reinterpret_cast<struct winsize *>(arg);
		*ws = winsize{};
		ws->ws_row = 24;
		ws->ws_col = 80;
		return 0;
	}
	__sprt_errno = ENOTTY;
	return -1;
}

static int __file_chmod(__fd_slot *, mode_t) {
	__sprt_errno = ENOSYS;
	return -1;
}

static int __file_utimens(__fd_slot *, const struct __SPRT_TIMESPEC_NAME *) {
	__sprt_errno = ENOSYS;
	return -1;
}

static void *__file_mmap(__fd_slot *, void *, size_t, int, int, off_t) {
	__sprt_errno = ENODEV;
	return __SPRT_MAP_FAILED;
}

static int __file_munmap(__fd_slot *, void *, size_t) {
	__sprt_errno = EINVAL;
	return -1;
}

static int __file_msync(__fd_slot *, void *, size_t, int) { return 0; }


static __tau_fd *__tau_file(const __fd_slot *fp) { return static_cast<__tau_fd *>(fp->handle); }

long __tau_ret(long r) {
	if (r < 0) {
		__sprt_errno = (int)-r;
		return -1;
	}
	return r;
}

void __tau_to_stat(const __sprt_tau_fs_stat &in, struct __SPRT_STAT_NAME *st) {
	static const mode_t types[] = {0, __SPRT_S_IFREG, __SPRT_S_IFDIR, __SPRT_S_IFLNK, __SPRT_S_IFCHR, __SPRT_S_IFBLK,
		__SPRT_S_IFIFO, __SPRT_S_IFSOCK};
	__builtin_memset(st, 0, sizeof(*st));
	st->st_dev = in.mount;
	st->st_ino = in.ino;
	st->st_mode = (in.type < 8 ? types[in.type] : 0) | (in.mode & 07'777);
	st->st_nlink = in.nlink;
	st->st_uid = in.uid;
	st->st_gid = in.gid;
	st->st_size = (off_t)in.size;
	st->st_blksize = 4'096;
	st->st_blocks = (blkcnt_t)in.blocks;
	st->st_atim.tv_sec = in.atime_ns / 1'000'000'000;
	st->st_atim.tv_nsec = in.atime_ns % 1'000'000'000;
	st->st_mtim.tv_sec = in.mtime_ns / 1'000'000'000;
	st->st_mtim.tv_nsec = in.mtime_ns % 1'000'000'000;
	st->st_ctim.tv_sec = in.ctime_ns / 1'000'000'000;
	st->st_ctim.tv_nsec = in.ctime_ns % 1'000'000'000;
}

static ssize_t __fs_read(__fd_slot *fp, void *buf, size_t nbytes, off64_t *offset, uint32_t) {
	__tau_fd *f = __tau_file(fp);
	off64_t at = offset ? *offset : f->pos;
	auto n = __tau_ret(__sprt_tau_fs_pread(f->file, buf, nbytes, (unsigned long long)at));
	if (n > 0 && !offset) {
		f->pos += n;
	}
	return n;
}

static ssize_t __fs_write(__fd_slot *fp, const void *buf, size_t nbytes, off64_t *offset, uint32_t) {
	__tau_fd *f = __tau_file(fp);
	off64_t at = offset ? *offset : f->pos;
	auto n = __tau_ret(__sprt_tau_fs_pwrite(f->file, buf, nbytes, (unsigned long long)at));
	if (n > 0 && !offset) {
		if (f->flags & __SPRT_TAU_FS_APPEND) {
			__sprt_tau_fs_stat st;
			f->pos = __sprt_tau_fs_fstat(f->file, &st) == 0 ? (off64_t)st.size : f->pos + n;
		} else {
			f->pos += n;
		}
	}
	return n;
}

static ssize_t __fs_readv(__fd_slot *fp, const __SPRT_IOVEC_NAME *iov, int iovcnt) {
	ssize_t total = 0;
	for (int i = 0; i < iovcnt; ++i) {
		auto n = __fs_read(fp, iov[i].iov_base, iov[i].iov_len, nullptr, 0);
		if (n < 0) {
			return total ? total : n;
		}
		total += n;
		if ((size_t)n < iov[i].iov_len) {
			break;
		}
	}
	return total;
}

static ssize_t __fs_writev(__fd_slot *fp, const __SPRT_IOVEC_NAME *iov, int iovcnt) {
	ssize_t total = 0;
	for (int i = 0; i < iovcnt; ++i) {
		auto n = __fs_write(fp, iov[i].iov_base, iov[i].iov_len, nullptr, 0);
		if (n < 0) {
			return total ? total : n;
		}
		total += n;
		if ((size_t)n < iov[i].iov_len) {
			break;
		}
	}
	return total;
}

static off_t __fs_seek(__fd_slot *fp, off_t offset, int whence) {
	__tau_fd *f = __tau_file(fp);
	off64_t base = 0;
	if (whence == __SPRT_SEEK_CUR) {
		base = f->pos;
	} else if (whence == __SPRT_SEEK_END) {
		__sprt_tau_fs_stat st;
		if (__tau_ret(__sprt_tau_fs_fstat(f->file, &st)) < 0) {
			return -1;
		}
		base = (off64_t)st.size;
	} else if (whence != __SPRT_SEEK_SET) {
		__sprt_errno = EINVAL;
		return -1;
	}
	if (base + offset < 0) {
		__sprt_errno = EINVAL;
		return -1;
	}
	f->pos = base + offset;
	return (off_t)f->pos;
}

static int __fs_stat(__fd_slot *fp, struct __SPRT_STAT_NAME *st) {
	__sprt_tau_fs_stat s;
	if (__tau_ret(__sprt_tau_fs_fstat(__tau_file(fp)->file, &s)) < 0) {
		return -1;
	}
	__tau_to_stat(s, st);
	return 0;
}

static int __fs_close(__fd_slot *fp) {
	__tau_fd *f = __tau_file(fp);
	int r = (int)__tau_ret(__sprt_tau_fs_close(f->file));
	__sprt_free(f);
	fp->handle = nullptr;
	return r < 0 ? -1 : 0;
}

static int __fs_dup(__fd_slot *, int *, uint32_t) {
	__sprt_errno = ENOSYS;
	return -1;
}

static int __fs_ioctl(__fd_slot *fp, int, int cmd, intptr_t, __fd_ctl_mode mode) {
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

static int __fs_chmod(__fd_slot *fp, mode_t mode) {
	return (int)__tau_ret(__sprt_tau_fs_fchmod(__tau_file(fp)->file, mode));
}

// There is no wall clock: UTIME_NOW leaves the time as it is, as UTIME_OMIT does.
static long long __tau_time(const struct __SPRT_TIMESPEC_NAME *t, int which) {
	if (!t || t[which].tv_nsec == __SPRT_UTIME_OMIT || t[which].tv_nsec == __SPRT_UTIME_NOW) {
		return -1;
	}
	return (long long)t[which].tv_sec * 1'000'000'000ll + t[which].tv_nsec;
}

static int __fs_utimens(__fd_slot *fp, const struct __SPRT_TIMESPEC_NAME *times) {
	return (int)__tau_ret(__sprt_tau_fs_futimens(__tau_file(fp)->file, __tau_time(times, 0), __tau_time(times, 1)));
}

const __fd_ops *__tau_fs_ops() {
	static __fd_ops ops = [] {
		__fd_ops o;
		o.fo_read = &__fs_read;
		o.fo_write = &__fs_write;
		o.fo_ioctl = &__fs_ioctl;
		o.fo_dup = &__fs_dup;
		o.fo_close = &__fs_close;
		o.fo_readv = &__fs_readv;
		o.fo_writev = &__fs_writev;
		o.fo_seek = &__fs_seek;
		o.fo_stat = &__fs_stat;
		o.fo_chmod = &__fs_chmod;
		o.fo_utimens = &__fs_utimens;
		o.fo_mmap = &__file_mmap;
		o.fo_munmap = &__file_munmap;
		o.fo_msync = &__file_msync;
		return o;
	}();
	return &ops;
}

unsigned __tau_open_flags(int oflags) {
	unsigned f = 0;
	switch (oflags & __SPRT_O_ACCMODE) {
	case __SPRT_O_RDONLY: f = __SPRT_TAU_FS_READ; break;
	case __SPRT_O_WRONLY: f = __SPRT_TAU_FS_WRITE; break;
	default: f = __SPRT_TAU_FS_READ | __SPRT_TAU_FS_WRITE; break;
	}
	f |= (oflags & __SPRT_O_CREAT) ? __SPRT_TAU_FS_CREATE : 0;
	f |= (oflags & __SPRT_O_EXCL) ? __SPRT_TAU_FS_EXCL : 0;
	f |= (oflags & __SPRT_O_TRUNC) ? __SPRT_TAU_FS_TRUNC : 0;
	f |= (oflags & __SPRT_O_APPEND) ? __SPRT_TAU_FS_APPEND : 0;
	f |= (oflags & __SPRT_O_DIRECTORY) ? __SPRT_TAU_FS_DIRECTORY : 0;
	f |= (oflags & __SPRT_O_NOFOLLOW) ? __SPRT_TAU_FS_NOFOLLOW : 0;
	return f;
}

int __tau_open_fd(int dirfd, const char *path, int oflags, mode_t mode, const __fd_ops *ops) {
	char buf[TAU_PATH_MAX];
	__sprt_tau_file *dir = nullptr;
	if (!__tau_path_ok(path) || !__tau_at(dirfd, path, buf, &dir, &path)) {
		return -1;
	}
	auto f = static_cast<__tau_fd *>(__sprt_malloc(sizeof(__tau_fd)));
	if (!f) {
		__sprt_errno = ENOMEM;
		return -1;
	}
	f->pos = 0;
	f->flags = __tau_open_flags(oflags);
	if (__tau_ret(__sprt_tau_fs_open(dir, path, f->flags, (unsigned)mode, &f->file)) < 0) {
		__sprt_free(f);
		return -1;
	}
	auto libc = __libc::get();
	int fd = libc->create_fd(f, ops, (uint32_t)oflags, (uint32_t)mode);
	if (fd < 0) {
		__sprt_tau_fs_close(f->file);
		__sprt_free(f);
		__sprt_errno = EMFILE;
		return -1;
	}
	return fd;
}

void __libc::load_file_fd_ops(__fd_ops *ops) {
	ops->mask = __fd_ops_mask::none;
	ops->fo_read = &__file_read;
	ops->fo_write = &__file_write;
	ops->fo_ioctl = &__file_ioctl;
	ops->fo_dup = &__file_dup;
	ops->fo_close = &__file_close;
	ops->fo_readv = &__file_readv;
	ops->fo_writev = &__file_writev;
	ops->fo_seek = &__file_seek;
	ops->fo_stat = &__file_stat;
	ops->fo_chmod = &__file_chmod;
	ops->fo_utimens = &__file_utimens;
	ops->fo_mmap = &__file_mmap;
	ops->fo_munmap = &__file_munmap;
	ops->fo_msync = &__file_msync;
}

// Zeroed pages anywhere in the map region; there is no fixed placement. Only MAP_JIT memory
// may be executable, its rights set after it is mapped.
void *__file_mmap_anon(void *addr, size_t length, int prot, int flags, off_t) {
	if ((flags & __SPRT_MAP_FIXED) && addr) {
		__sprt_errno = ENOTSUP;
		return __SPRT_MAP_FAILED;
	}
	bool jit = flags & __SPRT_MAP_JIT;
	if ((prot & __SPRT_PROT_EXEC) && !jit) {
		__sprt_errno = EACCES;
		return __SPRT_MAP_FAILED;
	}
	void *p = nullptr;
	int st = __sprt_tau_mem_map(length, 4'096, jit ? __SPRT_TAU_MEM_JIT : 0, &p);
	if (st == 0 && (prot & __SPRT_PROT_EXEC)) {
		int rights = __SPRT_TAU_PROT_EXEC | ((prot & __SPRT_PROT_READ) ? __SPRT_TAU_PROT_READ : 0)
				| ((prot & __SPRT_PROT_WRITE) ? __SPRT_TAU_PROT_WRITE : 0);
		st = __sprt_tau_mem_protect(p, length, rights);
		if (st != 0) {
			__sprt_tau_mem_unmap(p, length);
		}
	}
	if (st != 0) {
		__sprt_errno = -st;
		return __SPRT_MAP_FAILED;
	}
	return p;
}

int __file_munmap_anon(void *addr, size_t length) { return __sprt_tau_mem_unmap(addr, length); }

} // namespace sprt
