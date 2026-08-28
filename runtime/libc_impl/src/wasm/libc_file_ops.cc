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
#include <sprt/c/__sprt_time.h>
#include <sprt/c/sys/__sprt_mman.h>

extern "C" {
// T1 host imports. `h` is the console fd (0/1/2) for console nodes. `off < 0`
// means "use the wasm-side position" (the host does not track it). Return is
// bytes transferred, or -errno.
__attribute__((import_module("sprt"), import_name("fd_write"))) int __sprt_host_fd_write(int h,
		const void *buf, __SPRT_ID(size_t) len, double off);
__attribute__((import_module("sprt"), import_name("fd_read"))) int __sprt_host_fd_read(int h,
		void *buf, __SPRT_ID(size_t) len, double off);

// Read-only "Bundled" resources the browser pulls (fetch). bundle_size returns the byte
// size of `path` (or -1 if absent); bundle_read copies up to `cap` bytes into `buf` and
// returns the byte count (or -1). The JS side preloads these before _start so both calls
// are synchronous. This backs LocationCategory::Bundled; tmp/persistent use memfs/OPFS.
__attribute__((import_module("sprt"), import_name("bundle_size"))) int __sprt_host_bundle_size(
		const char *path, __SPRT_ID(size_t) pathLen);
__attribute__((import_module("sprt"), import_name("bundle_read"))) int __sprt_host_bundle_read(
		const char *path, __SPRT_ID(size_t) pathLen, void *buf, __SPRT_ID(size_t) cap);

// clock_gettime is defined by wasm/time.cc in this same libc. The __sprt_time.h
// prototype is namespaced when this TU is built without __SPRT_BUILD, so declare
// the plain entry we call for memfs timestamps.
int clock_gettime(__SPRT_ID(clockid_t), struct __SPRT_TIMESPEC_NAME *) __SPRT_NOEXCEPT;
}

namespace sprt {

// A persistent memfs file (inode): content survives open/close. Nodes live in a global
// singly-linked registry keyed by absolute path (see the path family in libc_path.cc).
struct __memfs_inode {
	char *path; // absolute, normalized
	unsigned char *data; // payload (heap)
	__SPRT_ID(size_t) size; // valid bytes
	__SPRT_ID(size_t) cap; // allocated capacity
	__SPRT_ID(mode_t) mode; // permission bits
	// Stable identity for st_ino: assigned once at creation and preserved across
	// rename (rename swaps the node's path, not the node), so a rename-over is
	// observable as an inode change at the target name (dispatch watchFile).
	__SPRT_ID(ino_t) inum;
	bool isDir;
	// Symbolic link: `data`/`size` hold the target path instead of file content, and
	// the node is transparent to the path family unless the caller asked not to
	// follow it (O_NOFOLLOW / lstat / readlink / unlink).
	bool isLink;
	bool opfs; // backed by the persistent /opfs (OPFS) mount
	bool dirty; // in-memory content differs from OPFS — write back on close/fsync
	bool readonly; // read-only overlay (JS bundle) — reject writes
	// POSIX timestamps. The wall clock comes from the host (JS Date, via
	// clock_gettime REALTIME); set at creation and advanced on write/truncate/
	// chmod, and settable via utimensat/futimens/utimes.
	struct __SPRT_TIMESPEC_NAME atim;
	struct __SPRT_TIMESPEC_NAME mtim;
	struct __SPRT_TIMESPEC_NAME ctim;
	__memfs_inode *next;
};

// OPFS backend (defined in libc_opfs.cc, same TU, included after this unit).
static __memfs_inode *__opfs_resolve_inode(const char *abs, int flags, __SPRT_ID(mode_t) mode);
static int __opfs_store(const char *abs, const unsigned char *data, __SPRT_ID(size_t) size);
static bool __vfs_is_opfs(const char *abs);

// Current wall-clock time from the host (JS side: Date.now via the clock_now
// import). Falls back to the epoch if the host clock is somehow unavailable.
static void __memfs_now(struct __SPRT_TIMESPEC_NAME *ts) {
	if (clock_gettime(__SPRT_CLOCK_REALTIME, ts) != 0) {
		ts->tv_sec = 0;
		ts->tv_nsec = 0;
	}
}

// An open file / stream description. For console nodes `ino` is null and I/O routes to
// the host fd_read/fd_write; for memfs nodes `ino` points at the persistent inode and
// `pos` is the per-open stream position.
//
// `refs` counts the descriptors bound to this one description: dup/dup2/dup3 and
// F_DUPFD share it (POSIX: duplicated descriptors share the file offset and status
// flags), so the description is freed only when the last of them is closed. Console
// descriptions are static and never freed, so their count is unused.
struct __wasm_fnode {
	bool isConsole;
	int consoleFd; // 0/1/2 when isConsole
	__memfs_inode *ino; // memfs inode, null for console
	__SPRT_ID(off_t) pos; // current stream position
	int refs; // descriptors sharing this description
};

// The three console descriptions, wired by __init_default_fds() via the accessor.
static __wasm_fnode s_console[3] = {
	{true, 0, nullptr, 0, 1},
	{true, 1, nullptr, 0, 1},
	{true, 2, nullptr, 0, 1},
};

void *__wasm_console_handle(int fd) {
	if (fd < 0 || fd > 2) {
		return nullptr;
	}
	return &s_console[fd];
}

// Grow a memfs inode so it can hold at least `need` bytes.
static bool __memfs_reserve(__memfs_inode *n, __SPRT_ID(size_t) need) {
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
	if (at < 0 || (__SPRT_ID(size_t))at >= n->ino->size) {
		return 0; // at/after EOF
	}
	__SPRT_ID(size_t) avail = n->ino->size - (__SPRT_ID(size_t))at;
	__SPRT_ID(size_t) cnt = nbytes < avail ? nbytes : avail;
	__builtin_memcpy(buf, n->ino->data + at, cnt);
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
		at = (__SPRT_ID(off_t))n->ino->size;
	}
	if (at < 0) {
		__sprt_errno = EINVAL;
		return -1;
	}
	__SPRT_ID(size_t) end = (__SPRT_ID(size_t))at + nbytes;
	if (!__memfs_reserve(n->ino, end)) {
		return -1;
	}
	// Zero any gap created by a seek past EOF.
	if ((__SPRT_ID(size_t))at > n->ino->size) {
		__builtin_memset(n->ino->data + n->ino->size, 0, (__SPRT_ID(size_t))at - n->ino->size);
	}
	__builtin_memcpy(n->ino->data + at, buf, nbytes);
	if (end > n->ino->size) {
		n->ino->size = end;
	}
	if (nbytes > 0) {
		__memfs_now(&n->ino->mtim);
		n->ino->ctim = n->ino->mtim;
		n->ino->dirty = true; // needs write-back if OPFS-backed
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
	case __SPRT_SEEK_END: base = (__SPRT_ID(off_t))n->ino->size; break;
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
	if (!n->isConsole && --n->refs <= 0) {
		// Last descriptor on this description: persist any pending changes to OPFS
		// (the durability point of the load-on-open / write-back-on-close model).
		if (n->ino && n->ino->opfs && n->ino->dirty && !n->ino->isDir) {
			if (__opfs_store(n->ino->path, n->ino->data, n->ino->size) == 0) {
				n->ino->dirty = false;
			}
		}
		// Free only the open description; the inode (content) persists in the registry.
		__sprt_free(n);
	}
	fp->handle = nullptr;
	return 0;
}

// dup/dup2/dup3/F_DUPFD: bind a second descriptor to the SAME open description, so
// the two share the file offset and status flags (POSIX). There is no OS handle to
// duplicate here, so the sharing is a refcount bump on the __wasm_fnode.
// `target` null => allocate the lowest free fd; otherwise install at *target
// (closing whatever was there first, dup2 semantics).
static int __file_dup(__fd_slot *fp, int *target, uint32_t flags) {
	auto n = (__wasm_fnode *)fp->handle;
	if (!n) {
		__sprt_errno = EBADF;
		return -1;
	}
	if (target && (*target < 0 || *target >= MAX_FDS)) {
		__sprt_errno = EBADF;
		return -1;
	}

	// wasm never execs, so FD_CLOEXEC is pure bookkeeping: keep it in the slot's
	// O_CLOEXEC bit, which is where __file_ioctl's F_GETFD/F_SETFD read it from.
	uint32_t newFlags = (flags & __SPRT_FD_CLOEXEC) ? (fp->flags | __SPRT_O_CLOEXEC)
													: (fp->flags & ~(uint32_t)__SPRT_O_CLOEXEC);

	auto libc = __libc::get();
	if (!n->isConsole) {
		++n->refs;
	}

	if (!target) {
		int fd = libc->create_fd(n, &libc->fdFileOps, newFlags, fp->mode);
		if (fd < 0) {
			if (!n->isConsole) {
				--n->refs;
			}
			return -1;
		}
		return fd;
	}

	unique_lock lock(libc->fdMutex);
	libc->fdDispatch->bits.set(*target);
	auto fdSlot = libc->get_fd_slot(*target);
	if (fdSlot->handle && fdSlot->ops && fdSlot->ops->fo_close) {
		fdSlot->ops->fo_close(fdSlot);
	}
	*fdSlot = __fd_slot{.handle = n, .ops = &libc->fdFileOps, .flags = newFlags, .mode = fp->mode};
	return *target;
}

static int __file_lock(__fd_slot *fp, int cmd, struct flock *fl);

static int __file_ioctl(__fd_slot *fp, int fd, int cmd, intptr_t arg, __fd_ctl_mode mode) {
	if (!fp->handle) {
		__sprt_errno = EBADF;
		return -1;
	}

	if (mode == __fd_ctl_mode::fnctl) {
		switch (cmd) {
		case __SPRT_F_DUPFD: return __file_dup(fp, nullptr, 0);
		case __SPRT_F_DUPFD_CLOEXEC: return __file_dup(fp, nullptr, __SPRT_FD_CLOEXEC);

		// There is no exec on wasm, so FD_CLOEXEC has no observable effect; it is
		// still tracked (in the slot's O_CLOEXEC bit) so it round-trips as POSIX
		// programs expect.
		case __SPRT_F_GETFD: return (fp->flags & __SPRT_O_CLOEXEC) ? __SPRT_FD_CLOEXEC : 0;
		case __SPRT_F_SETFD: {
			if (arg & ~(intptr_t)__SPRT_FD_CLOEXEC) {
				__sprt_errno = EINVAL;
				return -1;
			}
			if (arg & __SPRT_FD_CLOEXEC) {
				fp->flags |= __SPRT_O_CLOEXEC;
			} else {
				fp->flags &= ~(uint32_t)__SPRT_O_CLOEXEC;
			}
			return 0;
		}

		case __SPRT_F_GETFL: return int(fp->flags);
		case __SPRT_F_SETFL: {
			// Only the settable status bits; the access mode and creation flags are
			// fixed at open (POSIX ignores them here).
			constexpr uint32_t settable = __SPRT_O_APPEND | __SPRT_O_NONBLOCK;
			fp->flags = (fp->flags & ~settable) | ((uint32_t)arg & settable);
			return 0;
		}

		case __SPRT_F_GETLK:
		case __SPRT_F_SETLK:
		case __SPRT_F_SETLKW:
			return __file_lock(fp, cmd, reinterpret_cast<struct flock *>(arg));

		// Signal-driven I/O has no wasm equivalent (there are no signals to
		// deliver): report "not implemented" rather than a bogus success.
		case __SPRT_F_GETOWN:
		case __SPRT_F_SETOWN:
		case __SPRT_F_GETOWN_EX:
		case __SPRT_F_SETOWN_EX:
		case __SPRT_F_GETSIG:
		case __SPRT_F_SETSIG:
		case __SPRT_F_GETOWNER_UIDS: __sprt_errno = ENOSYS; return -1;
		}
	} else if (mode == __fd_ctl_mode::ioctl) {
		// No terminal ioctls are modelled: a JS console is not a terminal, and the
		// window-size query is answered by isatty/tty_info instead.
		__sprt_errno = ENOTTY;
		return -1;
	}
	__sprt_errno = EINVAL;
	return -1;
}

// POSIX record locks (F_SETLK/F_SETLKW/F_GETLK).
//
// POSIX locks are owned by the *process*, and a wasm module is a single process
// with no other process able to open the same memfs file — so a lock request can
// never conflict with anyone but its own owner, which POSIX defines as always
// succeeding (an existing lock is simply replaced). The bookkeeping is therefore
// unobservable and the operations reduce to argument validation:
//   F_SETLK/F_SETLKW -> succeed
//   F_GETLK          -> report F_UNLCK ("no conflicting lock"), as on Linux here
// This matches what the same test sees on Linux, where a's lock is invisible to b
// because both descriptors belong to one process.
static int __file_lock(__fd_slot *fp, int cmd, struct flock *fl) {
	auto n = (__wasm_fnode *)fp->handle;
	if (!fl || !n) {
		__sprt_errno = EBADF;
		return -1;
	}
	if (n->isConsole) {
		__sprt_errno = EBADF; // not a regular file
		return -1;
	}
	switch (fl->l_whence) {
	case __SPRT_SEEK_SET:
	case __SPRT_SEEK_CUR:
	case __SPRT_SEEK_END: break;
	default: __sprt_errno = EINVAL; return -1;
	}
	switch (fl->l_type) {
	case __SPRT_F_RDLCK:
		if ((fp->flags & __SPRT_O_ACCMODE) == __SPRT_O_WRONLY) {
			__sprt_errno = EBADF;
			return -1;
		}
		break;
	case __SPRT_F_WRLCK:
		if ((fp->flags & __SPRT_O_ACCMODE) == __SPRT_O_RDONLY) {
			__sprt_errno = EBADF;
			return -1;
		}
		break;
	case __SPRT_F_UNLCK: break;
	default: __sprt_errno = EINVAL; return -1;
	}
	if (cmd == __SPRT_F_GETLK) {
		fl->l_type = __SPRT_F_UNLCK;
	}
	return 0;
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
	} else if (n->ino->isDir) {
		// A directory fd (open with O_DIRECTORY, the dirfd of the *at() family) must
		// report S_IFDIR, or callers reject it as "not a directory".
		st->st_mode = __SPRT_S_IFDIR | (n->ino->mode ? (n->ino->mode & 0777) : 0755);
		st->st_ino = n->ino->inum;
		st->st_atim = n->ino->atim;
		st->st_mtim = n->ino->mtim;
		st->st_ctim = n->ino->ctim;
	} else {
		st->st_mode = __SPRT_S_IFREG | (n->ino->mode ? (n->ino->mode & 0777) : 0644);
		st->st_size = (__SPRT_ID(off_t))n->ino->size;
		st->st_blocks = (__SPRT_ID(blkcnt_t))((n->ino->size + 511) / 512);
		st->st_ino = n->ino->inum;
		st->st_atim = n->ino->atim;
		st->st_mtim = n->ino->mtim;
		st->st_ctim = n->ino->ctim;
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
		n->ino->mode = mode;
		__memfs_now(&n->ino->ctim);
	}
	return 0;
}

// Apply a utimensat/futimens times[2] vector ([0]=atime, [1]=mtime) to an inode.
// A null `times` sets both to now; per-element UTIME_NOW -> now, UTIME_OMIT -> keep.
// ctime always advances to now (a metadata change).
static void __memfs_apply_times(__memfs_inode *ino, const struct __SPRT_TIMESPEC_NAME *times) {
	struct __SPRT_TIMESPEC_NAME now;
	__memfs_now(&now);
	if (!times) {
		ino->atim = now;
		ino->mtim = now;
	} else {
		if (times[0].tv_nsec == __SPRT_UTIME_NOW) {
			ino->atim = now;
		} else if (times[0].tv_nsec != __SPRT_UTIME_OMIT) {
			ino->atim = times[0];
		}
		if (times[1].tv_nsec == __SPRT_UTIME_NOW) {
			ino->mtim = now;
		} else if (times[1].tv_nsec != __SPRT_UTIME_OMIT) {
			ino->mtim = times[1];
		}
	}
	ino->ctim = now;
}

static int __file_utimens(__fd_slot *fp, const struct __SPRT_TIMESPEC_NAME *times) {
	auto n = (__wasm_fnode *)fp->handle;
	if (!n) {
		__sprt_errno = EBADF;
		return -1;
	}
	if (!n->isConsole) {
		__memfs_apply_times(n->ino, times);
	}
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

// ============================================================================
// memfs registry + path helpers (used by the path family in libc_path.cc, same TU)
// ============================================================================

static __memfs_inode *s_memfs = nullptr; // singly-linked list of files/dirs

// Normalize `path` into an absolute, "/"-rooted form in `out`, collapsing "//", "." and
// resolving "..". cwd is "/" (see getcwd), so a relative path is rooted at "/".
static bool __memfs_normpath(const char *path, char *out, __SPRT_ID(size_t) cap) {
	__SPRT_ID(size_t) len = 0;
	if (cap < 2) {
		return false;
	}
	out[len++] = '/';
	const char *p = path;
	while (*p) {
		while (*p == '/') {
			++p;
		}
		if (!*p) {
			break;
		}
		const char *start = p;
		while (*p && *p != '/') {
			++p;
		}
		__SPRT_ID(size_t) clen = (__SPRT_ID(size_t))(p - start);
		if (clen == 1 && start[0] == '.') {
			continue;
		}
		if (clen == 2 && start[0] == '.' && start[1] == '.') {
			if (len > 1) {
				while (len > 1 && out[len - 1] != '/') {
					--len;
				}
				if (len > 1) {
					--len; // drop the separating '/'
				}
			}
			continue;
		}
		if (len > 1) {
			if (len + 1 >= cap) {
				return false;
			}
			out[len++] = '/';
		}
		if (len + clen >= cap) {
			return false;
		}
		__builtin_memcpy(out + len, start, clen);
		len += clen;
	}
	out[len] = '\0';
	return true;
}

static __memfs_inode *__memfs_find(const char *abspath) {
	for (auto n = s_memfs; n; n = n->next) {
		if (__builtin_strcmp(n->path, abspath) == 0) {
			return n;
		}
	}
	return nullptr;
}

// Cap on symlink chains, as POSIX requires a bound (-> ELOOP).
static constexpr int __MEMFS_LINK_MAX = 16;

// Follow `abs` through any symlink nodes and write the final path into `out`.
// Returns false (with errno set) only on a cycle/too-long chain or an overlong
// path; a chain ending at a name that does not exist is NOT an error here — the
// caller decides what a missing target means (open with O_CREAT creates it,
// stat reports ENOENT).
//
// Only the LAST component is followed: memfs is a flat namespace, so a symlink
// used as an intermediate directory component does not redirect its children.
static bool __memfs_resolve_link(const char *abs, char *out, __SPRT_ID(size_t) cap) {
	if (out != abs) {
		__SPRT_ID(size_t) l = __builtin_strlen(abs);
		if (l + 1 > cap) {
			__sprt_errno = ENAMETOOLONG;
			return false;
		}
		__builtin_memcpy(out, abs, l + 1);
	}
	for (int i = 0; i < __MEMFS_LINK_MAX; ++i) {
		auto n = __memfs_find(out);
		if (!n || !n->isLink) {
			return true;
		}
		char next[512];
		// A relative target resolves against the directory holding the link.
		if (n->size && n->data[0] == '/') {
			if (n->size + 1 > sizeof(next)) {
				__sprt_errno = ENAMETOOLONG;
				return false;
			}
			__builtin_memcpy(next, n->data, n->size);
			next[n->size] = '\0';
		} else {
			__SPRT_ID(size_t) dl = __builtin_strlen(out);
			while (dl > 1 && out[dl - 1] != '/') {
				--dl;
			}
			if (dl + n->size + 1 > sizeof(next)) {
				__sprt_errno = ENAMETOOLONG;
				return false;
			}
			__builtin_memcpy(next, out, dl);
			__builtin_memcpy(next + dl, n->data, n->size);
			next[dl + n->size] = '\0';
		}
		if (!__memfs_normpath(next, out, cap)) {
			__sprt_errno = ENAMETOOLONG;
			return false;
		}
	}
	__sprt_errno = ELOOP;
	return false;
}

// Re-key every node living under `from` so it lives under `to` instead. Called
// after a directory inode itself has been renamed (rename() below): the registry is
// flat, so the subtree does not move with its parent on its own.
static void __memfs_reparent(const char *from, const char *to) {
	const __SPRT_ID(size_t) flen = __builtin_strlen(from);
	const __SPRT_ID(size_t) tlen = __builtin_strlen(to);
	for (auto n = s_memfs; n; n = n->next) {
		if (__builtin_strncmp(n->path, from, flen) != 0 || n->path[flen] != '/') {
			continue;
		}
		const __SPRT_ID(size_t) rest = __builtin_strlen(n->path + flen);
		auto np = (char *)__sprt_malloc(tlen + rest + 1);
		if (!np) {
			continue; // out of memory: leave this child where it is
		}
		__builtin_memcpy(np, to, tlen);
		__builtin_memcpy(np + tlen, n->path + flen, rest + 1);
		__sprt_free(n->path);
		n->path = np;
	}
}

// True when `abs` names a directory the VFS should treat as existing. That is any
// explicitly-created directory inode, the root, or — because memfs is flat and a
// file may be created at any path without its parents — any prefix that some
// existing node lives under.
static bool __memfs_is_dir_path(const char *abs) {
	if (abs[0] == '/' && abs[1] == '\0') {
		return true; // the root always exists
	}
	auto n = __memfs_find(abs);
	if (n) {
		return n->isDir;
	}
	__SPRT_ID(size_t) len = __builtin_strlen(abs);
	for (auto m = s_memfs; m; m = m->next) {
		if (__builtin_strncmp(m->path, abs, len) == 0 && m->path[len] == '/') {
			return true;
		}
	}
	return false;
}

static __memfs_inode *__memfs_create(const char *abspath, bool isDir, __SPRT_ID(mode_t) mode) {
	auto n = (__memfs_inode *)__sprt_malloc(sizeof(__memfs_inode));
	if (!n) {
		__sprt_errno = ENOMEM;
		return nullptr;
	}
	__SPRT_ID(size_t) plen = __builtin_strlen(abspath);
	n->path = (char *)__sprt_malloc(plen + 1);
	if (!n->path) {
		__sprt_free(n);
		__sprt_errno = ENOMEM;
		return nullptr;
	}
	__builtin_memcpy(n->path, abspath, plen + 1);
	n->data = nullptr;
	n->size = 0;
	n->cap = 0;
	n->mode = mode;
	n->isDir = isDir;
	n->isLink = false;
	n->opfs = false;
	n->dirty = false;
	n->readonly = false;
	static __SPRT_ID(ino_t) s_nextInum = 1;
	n->inum = s_nextInum++;
	__memfs_now(&n->mtim);
	n->atim = n->mtim;
	n->ctim = n->mtim;
	n->next = s_memfs;
	s_memfs = n;
	return n;
}

// Try to satisfy a missing path from the read-only Bundled overlay (browser fetch).
static __memfs_inode *__memfs_load_bundle(const char *abspath) {
	__SPRT_ID(size_t) plen = __builtin_strlen(abspath);
	int sz = __sprt_host_bundle_size(abspath, plen);
	if (sz < 0) {
		return nullptr; // not a bundled resource
	}
	auto ino = __memfs_create(abspath, false, 0444);
	if (!ino) {
		return nullptr;
	}
	ino->readonly = true; // Bundled overlay is read-only (JS-served, immutable)
	if (sz > 0) {
		if (!__memfs_reserve(ino, (__SPRT_ID(size_t))sz)) {
			return nullptr;
		}
		int rd = __sprt_host_bundle_read(abspath, plen, ino->data, (__SPRT_ID(size_t))sz);
		ino->size = (rd > 0) ? (__SPRT_ID(size_t))rd : 0;
	}
	return ino;
}

// Remove a file (dir=false) or empty directory (dir=true) from the registry.
static bool __memfs_unlink_node(const char *abspath, bool dir) {
	__memfs_inode **pp = &s_memfs;
	for (auto n = s_memfs; n; pp = &n->next, n = n->next) {
		if (__builtin_strcmp(n->path, abspath) != 0) {
			continue;
		}
		if (n->isDir != dir) {
			__sprt_errno = dir ? ENOTDIR : EISDIR;
			return false;
		}
		if (dir) {
			// reject non-empty directory
			__SPRT_ID(size_t) plen = __builtin_strlen(abspath);
			for (auto m = s_memfs; m; m = m->next) {
				if (m != n && __builtin_strncmp(m->path, abspath, plen) == 0
						&& m->path[plen] == '/') {
					__sprt_errno = ENOTEMPTY;
					return false;
				}
			}
		}
		*pp = n->next;
		if (n->data) {
			__sprt_free(n->data);
		}
		__sprt_free(n->path);
		__sprt_free(n);
		return true;
	}
	__sprt_errno = ENOENT;
	return false;
}

// open() core: resolve, apply O_CREAT/O_TRUNC/O_EXCL, bind an fd to the inode.
static int __memfs_openfd(const char *path, int flags, __SPRT_ID(mode_t) mode) {
	char abs[512];
	if (!__memfs_normpath(path, abs, sizeof(abs))) {
		__sprt_errno = ENAMETOOLONG;
		return -1;
	}
	{
		// O_NOFOLLOW: fail on a symlink instead of following it (POSIX: ELOOP).
		// Otherwise open acts on the link's target, as every other path call does.
		auto link = __memfs_find(abs);
		if (link && link->isLink) {
			if (flags & __SPRT_O_NOFOLLOW) {
				__sprt_errno = ELOOP;
				return -1;
			}
			if (!__memfs_resolve_link(abs, abs, sizeof(abs))) {
				return -1;
			}
		}
	}
	// A directory that exists only implicitly (memfs is flat, so "/app" may have no
	// inode of its own while "/app/f" does) still has to open as one — that is what
	// an O_PATH|O_DIRECTORY fd for *at() resolution is.
	if ((flags & __SPRT_O_DIRECTORY) && !__memfs_find(abs) && !__vfs_is_opfs(abs)
			&& __memfs_is_dir_path(abs)) {
		if (!__memfs_create(abs, true, 0755)) {
			return -1;
		}
	}
	__memfs_inode *ino = __memfs_find(abs);
	if (!ino && __vfs_is_opfs(abs)) {
		// Persistent mount: hydrate from OPFS (or create there). Handles O_CREAT/
		// O_EXCL/O_TRUNC + ENOENT internally and returns a ready cache inode.
		ino = __opfs_resolve_inode(abs, flags, mode);
		if (!ino) {
			return -1; // errno set by the resolver
		}
	} else {
		if (!ino) {
			ino = __memfs_load_bundle(abs); // read-only Bundled (browser fetch)
		}
		if (!ino) {
			if (!(flags & __SPRT_O_CREAT)) {
				__sprt_errno = ENOENT;
				return -1;
			}
			ino = __memfs_create(abs, false, mode & 0777);
			if (!ino) {
				return -1;
			}
		} else {
			if ((flags & __SPRT_O_CREAT) && (flags & __SPRT_O_EXCL)) {
				__sprt_errno = EEXIST;
				return -1;
			}
			if (ino->isDir && ((flags & __SPRT_O_ACCMODE) != __SPRT_O_RDONLY)) {
				__sprt_errno = EISDIR;
				return -1;
			}
			if (flags & __SPRT_O_TRUNC) {
				ino->size = 0;
			}
		}
	}
	if ((flags & __SPRT_O_DIRECTORY) && !ino->isDir) {
		__sprt_errno = ENOTDIR;
		return -1;
	}
	// Read-only overlay (JS bundle): reject any write access.
	if (ino->readonly && ((flags & __SPRT_O_ACCMODE) != __SPRT_O_RDONLY)) {
		__sprt_errno = EROFS;
		return -1;
	}
	auto libc = __libc::get();
	auto fn = (__wasm_fnode *)__sprt_malloc(sizeof(__wasm_fnode));
	if (!fn) {
		__sprt_errno = ENOMEM;
		return -1;
	}
	fn->isConsole = false;
	fn->consoleFd = -1;
	fn->ino = ino;
	fn->pos = (flags & __SPRT_O_APPEND) ? (__SPRT_ID(off_t))ino->size : 0;
	fn->refs = 1;
	int fd = libc->create_fd(fn, &libc->fdFileOps, (uint32_t)flags, (uint32_t)mode);
	if (fd < 0) {
		__sprt_free(fn);
		return -1;
	}
	return fd;
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
