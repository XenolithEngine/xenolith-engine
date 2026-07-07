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

// WebAssembly fd backend: the __fd_ops table (wasm-port-draft.adoc §3.3).
//
// Two node kinds share one table:
//   * console (fd 0/1/2) — reads/writes route to the fd_read/fd_write host
//     imports (T1 SYNC); JS sends stdout/stderr to the DevTools console;
//   * memfs — an in-linear-memory tmpfs node holding a growable byte buffer.
//     This is Level 0 of the VFS: fully LOCAL, no host round-trip, ideal for
//     /tmp and tests. Persistent OPFS-backed files (Level 1, PROXY open) land
//     later. Nothing yet creates memfs fds because the path family (open) is a
//     later milestone; the ops are complete so they light up the moment it does.
//
// __file_mmap_anon / __file_munmap_anon back MAP_ANONYMOUS with plain allocator
// memory (wasm memory only grows; a munmap just frees to the allocator).

#include "../../include/__impl_libc.h"

#include <sprt/c/__sprt_fcntl.h>
#include <sprt/c/__sprt_stdlib.h>
#include <sprt/c/sys/__sprt_mman.h>

extern "C" {
// T1 host imports. `h` is the console fd (0/1/2) for console nodes. `off < 0`
// means "use the wasm-side position" (the host does not track it). Return is
// bytes transferred, or -errno.
__attribute__((import_module("sprt"), import_name("fd_write"))) int __sprt_host_fd_write(int h,
		const void *buf, __SPRT_ID(size_t) len, double off);
__attribute__((import_module("sprt"), import_name("fd_read"))) int __sprt_host_fd_read(int h,
		void *buf, __SPRT_ID(size_t) len, double off);
}

namespace sprt {

// An open file / stream description.
struct __wasm_fnode {
	bool isConsole;
	int consoleFd; // 0/1/2 when isConsole
	unsigned char *data; // memfs payload (heap), null for console
	__SPRT_ID(size_t) size; // valid bytes
	__SPRT_ID(size_t) cap; // allocated capacity
	__SPRT_ID(off_t) pos; // current stream position
	__SPRT_ID(mode_t) mode; // file mode bits
};

// The three console descriptions, wired by __init_default_fds() via the accessor.
static __wasm_fnode s_console[3] = {
	{true, 0, nullptr, 0, 0, 0, 0},
	{true, 1, nullptr, 0, 0, 0, 0},
	{true, 2, nullptr, 0, 0, 0, 0},
};

void *__wasm_console_handle(int fd) {
	if (fd < 0 || fd > 2) {
		return nullptr;
	}
	return &s_console[fd];
}

// Grow a memfs node so it can hold at least `need` bytes.
static bool __memfs_reserve(__wasm_fnode *n, __SPRT_ID(size_t) need) {
	if (need <= n->cap) {
		return true;
	}
	__SPRT_ID(size_t) newCap = n->cap ? n->cap * 2 : 64;
	if (newCap < need) {
		newCap = need;
	}
	auto p = (unsigned char *)__sprt_realloc(n->data, newCap);
	if (!p) {
		__sprt_errno = ENOMEM;
		return false;
	}
	n->data = p;
	n->cap = newCap;
	return true;
}

static ssize_t __file_read(struct __fd_slot *fp, void *buf, size_t nbytes, off64_t *offset,
		uint32_t flags) {
	auto n = (__wasm_fnode *)fp->handle;
	if (!n) {
		__sprt_errno = EBADF;
		return -1;
	}
	if ((fp->flags & __SPRT_O_ACCMODE) == __SPRT_O_WRONLY) {
		__sprt_errno = EBADF;
		return -1;
	}
	if (n->isConsole) {
		int r = __sprt_host_fd_read(n->consoleFd, buf, nbytes,
				offset ? (double)*offset : -1.0);
		if (r < 0) {
			__sprt_errno = -r;
			return -1;
		}
		return r;
	}
	// memfs
	__SPRT_ID(off_t) at = offset ? *offset : n->pos;
	if (at < 0 || (__SPRT_ID(size_t))at >= n->size) {
		return 0; // at/after EOF
	}
	__SPRT_ID(size_t) avail = n->size - (__SPRT_ID(size_t))at;
	__SPRT_ID(size_t) cnt = nbytes < avail ? nbytes : avail;
	__builtin_memcpy(buf, n->data + at, cnt);
	if (!offset) {
		n->pos = at + (__SPRT_ID(off_t))cnt;
	}
	return (ssize_t)cnt;
}

static ssize_t __file_write(struct __fd_slot *fp, const void *buf, size_t nbytes, off64_t *offset,
		uint32_t flags) {
	auto n = (__wasm_fnode *)fp->handle;
	if (!n) {
		__sprt_errno = EBADF;
		return -1;
	}
	if ((fp->flags & __SPRT_O_ACCMODE) == __SPRT_O_RDONLY) {
		__sprt_errno = EBADF;
		return -1;
	}
	if (n->isConsole) {
		int r = __sprt_host_fd_write(n->consoleFd, buf, nbytes,
				offset ? (double)*offset : -1.0);
		if (r < 0) {
			__sprt_errno = -r;
			return -1;
		}
		return r;
	}
	// memfs
	__SPRT_ID(off_t) at = offset ? *offset : n->pos;
	if (fp->flags & __SPRT_O_APPEND) {
		at = (__SPRT_ID(off_t))n->size;
	}
	if (at < 0) {
		__sprt_errno = EINVAL;
		return -1;
	}
	__SPRT_ID(size_t) end = (__SPRT_ID(size_t))at + nbytes;
	if (!__memfs_reserve(n, end)) {
		return -1;
	}
	// Zero any gap created by a seek past EOF.
	if ((__SPRT_ID(size_t))at > n->size) {
		__builtin_memset(n->data + n->size, 0, (__SPRT_ID(size_t))at - n->size);
	}
	__builtin_memcpy(n->data + at, buf, nbytes);
	if (end > n->size) {
		n->size = end;
	}
	if (!offset) {
		n->pos = (__SPRT_ID(off_t))end;
	}
	return (ssize_t)nbytes;
}

static ssize_t __file_readv(__fd_slot *fp, const __SPRT_IOVEC_NAME *iov, int iovcnt) {
	ssize_t total = 0;
	for (int i = 0; i < iovcnt; ++i) {
		if (iov[i].iov_len == 0) {
			continue;
		}
		ssize_t r = __file_read(fp, iov[i].iov_base, iov[i].iov_len, nullptr, 0);
		if (r < 0) {
			return total ? total : -1;
		}
		total += r;
		if ((size_t)r < iov[i].iov_len) {
			break; // short read
		}
	}
	return total;
}

static ssize_t __file_writev(__fd_slot *fp, const __SPRT_IOVEC_NAME *iov, int iovcnt) {
	ssize_t total = 0;
	for (int i = 0; i < iovcnt; ++i) {
		if (iov[i].iov_len == 0) {
			continue;
		}
		ssize_t r = __file_write(fp, iov[i].iov_base, iov[i].iov_len, nullptr, 0);
		if (r < 0) {
			return total ? total : -1;
		}
		total += r;
		if ((size_t)r < iov[i].iov_len) {
			break; // short write
		}
	}
	return total;
}

static off_t __file_seek(__fd_slot *fp, off_t off, int whence) {
	auto n = (__wasm_fnode *)fp->handle;
	if (!n || n->isConsole) {
		__sprt_errno = ESPIPE;
		return -1;
	}
	__SPRT_ID(off_t) base = 0;
	switch (whence) {
	case __SPRT_SEEK_SET: base = 0; break;
	case __SPRT_SEEK_CUR: base = n->pos; break;
	case __SPRT_SEEK_END: base = (__SPRT_ID(off_t))n->size; break;
	default: __sprt_errno = EINVAL; return -1;
	}
	__SPRT_ID(off_t) np = base + off;
	if (np < 0) {
		__sprt_errno = EINVAL;
		return -1;
	}
	n->pos = np;
	return np;
}

static int __file_close(__fd_slot *fp) {
	auto n = (__wasm_fnode *)fp->handle;
	if (!n) {
		__sprt_errno = EBADF;
		return -1;
	}
	if (!n->isConsole) {
		if (n->data) {
			__sprt_free(n->data);
		}
		__sprt_free(n);
	}
	fp->handle = nullptr;
	return 0;
}

static int __file_dup(__fd_slot *fp, int *target, uint32_t flags) {
	// Sharing an open description across fds needs refcounting; not wired yet.
	__sprt_errno = ENOSYS;
	return -1;
}

static int __file_ioctl(__fd_slot *fp, int fd, int cmd, intptr_t arg, __fd_ctl_mode mode) {
	// No terminal ioctls modelled yet (TIOCGWINSZ handled by isatty/tty_info).
	__sprt_errno = ENOTTY;
	return -1;
}

static int __file_stat(__fd_slot *fp, struct __SPRT_STAT_NAME *st) {
	auto n = (__wasm_fnode *)fp->handle;
	if (!n) {
		__sprt_errno = EBADF;
		return -1;
	}
	__builtin_memset(st, 0, sizeof(*st));
	st->st_nlink = 1;
	st->st_blksize = 65536;
	if (n->isConsole) {
		st->st_mode = __SPRT_S_IFCHR | 0620;
		st->st_size = 0;
	} else {
		st->st_mode = __SPRT_S_IFREG | (n->mode ? (n->mode & 0777) : 0644);
		st->st_size = (__SPRT_ID(off_t))n->size;
		st->st_blocks = (__SPRT_ID(blkcnt_t))((n->size + 511) / 512);
	}
	return 0;
}

static int __file_chmod(__fd_slot *fp, mode_t mode) {
	auto n = (__wasm_fnode *)fp->handle;
	if (!n) {
		__sprt_errno = EBADF;
		return -1;
	}
	if (!n->isConsole) {
		n->mode = mode;
	}
	return 0;
}

static int __file_utimens(__fd_slot *fp, const struct __SPRT_TIMESPEC_NAME *times) {
	// memfs keeps no timestamps yet: accept silently.
	(void)fp;
	(void)times;
	return 0;
}

static void *__file_mmap(__fd_slot *fp, void *addr, size_t length, int prot, int flags,
		off_t offset) {
	// File-backed mappings are a later milestone.
	(void)fp;
	(void)addr;
	(void)length;
	(void)prot;
	(void)flags;
	(void)offset;
	__sprt_errno = ENODEV;
	return __SPRT_MAP_FAILED;
}

static int __file_munmap(__fd_slot *fp, void *addr, size_t length) {
	(void)fp;
	(void)addr;
	(void)length;
	return 0;
}

static int __file_msync(__fd_slot *fp, void *addr, size_t length, int flags) {
	(void)fp;
	(void)addr;
	(void)length;
	(void)flags;
	return 0;
}

// Anonymous mapping over allocator memory (zeroed, page-aligned).
void *__file_mmap_anon(void *addr, size_t length, int prot, int flags, off_t offset) {
	(void)addr;
	(void)prot;
	(void)flags;
	(void)offset;
	if (length == 0) {
		__sprt_errno = EINVAL;
		return __SPRT_MAP_FAILED;
	}
	// 64 KiB alignment matches the wasm page granularity reported by getpagesize.
	void *p = __sprt_aligned_alloc(65536, length);
	if (!p) {
		__sprt_errno = ENOMEM;
		return __SPRT_MAP_FAILED;
	}
	__builtin_memset(p, 0, length);
	return p;
}

int __file_munmap_anon(void *addr, size_t length) {
	(void)length;
	__sprt_free(addr);
	return 0;
}

void __libc::load_file_fd_ops(__fd_ops *ops) {
	ops->mask = __fd_ops_mask::none;
	ops->fo_read = &__file_read;
	ops->fo_write = &__file_write;
	ops->fo_close = &__file_close;
	ops->fo_dup = &__file_dup;
	ops->fo_ioctl = &__file_ioctl;
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

} // namespace sprt
