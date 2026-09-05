
// Embox EL0 path family: everything that names a file rather than holding one.
//
// The kernel's table has exactly two path syscalls -- openat(56) and
// newfstatat(79) -- so this file divides sharply in two:
//
//   * open/fopen/stat/lstat/access/realpath and the *at forms of those are real,
//     built on those two;
//   * every mutation (mkdir, unlink, rename, truncate, chmod, utimes, links) and
//     everything that enumerates a directory is ENOSYS until M2 brings
//     mkdirat(34)/unlinkat(35)/renameat(38)/ftruncate(46)/getdents64(61).
//
// The ENOSYS half is written out rather than left undefined on purpose: a
// missing symbol is a link error in an application that merely MENTIONS remove()
// on an error path it never takes, whereas a call that fails cleanly is
// something a program can handle -- and sprt's own code checks errno.

// The libc's own umbrella headers (include_libc/), not the platform's -- there is
// no platform C library on this target's include path.
#include "dirent.h"
#include "fcntl.h"
#include "stdarg.h"
#include "stdio.h"
#include "string.h"
#include "unistd.h"
#include "sys/stat.h"

#include "../../include/__impl_libc.h"
#include "kstat.h"

#include <sprt/c/__sprt_errno.h>
#include <sprt/c/__sprt_stdlib.h>
#include <sprt/c/cross/__sprt_setjmp.h>

#include "../../../core/include/__el0_syscall.h"

namespace sprt {

void *__el0_handle(int kfd); // libc_file_ops.cc, same TU

// PATH_MAX. The kernel copies a path into a buffer of exactly this size and
// answers ENAMETOOLONG past it (xl_syscall.c), so rejecting here first only
// changes where the same error is produced -- but it does so without a syscall.
static constexpr size_t EL0_PATH_MAX = 4'096;

static bool __el0_path_ok(const char *path) {
	if (!path) {
		__sprt_errno = EFAULT;
		return false;
	}
	if (!*path) {
		__sprt_errno = ENOENT;
		return false;
	}
	if (__builtin_strlen(path) >= EL0_PATH_MAX) {
		__sprt_errno = ENAMETOOLONG;
		return false;
	}
	return true;
}

// Open through the kernel and register the descriptor in a libc slot. The two fd
// numbers are unrelated: the kernel's goes in the slot's handle, the libc's is
// what the caller gets back.
static int __el0_open_slot(int dirfd, const char *path, int flags, mode_t mode) {
	if (!__el0_path_ok(path)) {
		return -1;
	}
	auto kfd = (int)__el0_ret(__el0_openat(dirfd, path, flags, mode));
	if (kfd < 0) {
		return -1;
	}
	auto libc = __libc::get();
	auto fd = libc->create_fd(__el0_handle(kfd), &libc->fdFileOps, (uint32_t)flags,
			(uint32_t)mode);
	if (fd < 0) {
		// The libc ran out of slots; the kernel descriptor would leak otherwise.
		__el0_close(kfd);
		__sprt_errno = EMFILE;
		return -1;
	}
	return fd;
}

static int __el0_stat_path(int dirfd, const char *path, struct __SPRT_STAT_NAME *st, int flags) {
	if (!__el0_path_ok(path)) {
		return -1;
	}
	__el0_kstat ks;
	if (__el0_ret(__el0_newfstatat(dirfd, path, &ks, flags)) < 0) {
		return -1;
	}
	__el0_kstat_to_stat(ks, st);
	return 0;
}

} // namespace sprt

// --- open ------------------------------------------------------------------

__SPRT_C_FUNC int openat(int dirfd, const char *path, int flags, ...) __SPRT_NOEXCEPT {
	__SPRT_ID(mode_t) mode = 0;
	if (flags & __SPRT_O_CREAT) {
		__sprt_va_list ap;
		__sprt_va_start(ap, flags);
		mode = (__SPRT_ID(mode_t))__sprt_va_arg(ap, int);
		__sprt_va_end(ap);
	}
	// dirfd other than AT_FDCWD reaches the kernel and comes back ENOSYS: Embox
	// has no dirfd-relative lookup, and resolving it here would mean
	// reimplementing path resolution in the libc.
	return sprt::__el0_open_slot(dirfd, path, flags, mode);
}

__SPRT_C_FUNC int open(const char *path, int flags, ...) __SPRT_NOEXCEPT {
	__SPRT_ID(mode_t) mode = 0;
	if (flags & __SPRT_O_CREAT) {
		__sprt_va_list ap;
		__sprt_va_start(ap, flags);
		mode = (__SPRT_ID(mode_t))__sprt_va_arg(ap, int);
		__sprt_va_end(ap);
	}
	return sprt::__el0_open_slot(__SPRT_AT_FDCWD, path, flags, mode);
}

namespace sprt {

// fopen() mode string -> open() flags.
static int __fopen_flags(const char *m) {
	if (!m) {
		return -1;
	}
	bool plus = false;
	for (const char *p = m; *p; ++p) {
		if (*p == '+') {
			plus = true;
		}
	}
	switch (m[0]) {
	case 'r': return plus ? __SPRT_O_RDWR : __SPRT_O_RDONLY;
	case 'w': return (plus ? __SPRT_O_RDWR : __SPRT_O_WRONLY) | __SPRT_O_CREAT | __SPRT_O_TRUNC;
	case 'a': return (plus ? __SPRT_O_RDWR : __SPRT_O_WRONLY) | __SPRT_O_CREAT | __SPRT_O_APPEND;
	default: return -1;
	}
}

} // namespace sprt

__SPRT_C_FUNC FILE *fopen(const char *path, const char *mode) __SPRT_NOEXCEPT {
	int flags = sprt::__fopen_flags(mode);
	if (flags < 0) {
		__sprt_errno = EINVAL;
		return nullptr;
	}
	int fd = sprt::__el0_open_slot(__SPRT_AT_FDCWD, path, flags, 0644);
	if (fd < 0) {
		return nullptr;
	}
	FILE *f = fdopen(fd, mode);
	if (!f) {
		close(fd);
	}
	return f;
}

// --- metadata --------------------------------------------------------------

__SPRT_C_FUNC int stat(const char *__SPRT_RESTRICT path,
		struct __SPRT_STAT_NAME *__SPRT_RESTRICT st) __SPRT_NOEXCEPT {
	return sprt::__el0_stat_path(__SPRT_AT_FDCWD, path, st, 0);
}

__SPRT_C_FUNC int lstat(const char *__SPRT_RESTRICT path,
		struct __SPRT_STAT_NAME *__SPRT_RESTRICT st) __SPRT_NOEXCEPT {
	return sprt::__el0_stat_path(__SPRT_AT_FDCWD, path, st, __SPRT_AT_SYMLINK_NOFOLLOW);
}

__SPRT_C_FUNC int fstatat(int dirfd, const char *__SPRT_RESTRICT path,
		struct __SPRT_STAT_NAME *__SPRT_RESTRICT st, int flags) __SPRT_NOEXCEPT {
	return sprt::__el0_stat_path(dirfd, path, st, flags);
}

// faccessat has no syscall (48 is M2), but the question it asks can be answered
// from newfstatat, and the answer is not an approximation here: Embox has no
// user model, so every task runs with total authority. That is exactly the
// uid-0 case, where POSIX says read and write always succeed on an existing file
// and execute needs some x bit set. So this returns what a real faccessat would.
__SPRT_C_FUNC int faccessat(int dirfd, const char *path, int amode, int flags) __SPRT_NOEXCEPT {
	struct __SPRT_STAT_NAME st;
	if (sprt::__el0_stat_path(dirfd, path, &st, flags & __SPRT_AT_SYMLINK_NOFOLLOW) < 0) {
		return -1;
	}
	if ((amode & __SPRT_X_OK)
			&& !(st.st_mode & (__SPRT_S_IXUSR | __SPRT_S_IXGRP | __SPRT_S_IXOTH))) {
		__sprt_errno = EACCES;
		return -1;
	}
	return 0;
}

__SPRT_C_FUNC int access(const char *path, int amode) __SPRT_NOEXCEPT {
	return faccessat(__SPRT_AT_FDCWD, path, amode, 0);
}

// realpath on a filesystem with no symbolic links is lexical normalisation plus
// an existence check -- there is nothing left for it to resolve. Embox's initfs,
// ramfs and FAT have no links, and no syscall could follow one if they did. A
// relative path cannot be handled: making it absolute needs the working
// directory, and getcwd(17) is not implemented.
__SPRT_C_FUNC char *realpath(const char *path, char *resolved) __SPRT_NOEXCEPT {
	if (!sprt::__el0_path_ok(path)) {
		return nullptr;
	}
	if (path[0] != '/') {
		__sprt_errno = ENOSYS; // needs getcwd(17), which is M2
		return nullptr;
	}

	char stack[sprt::EL0_PATH_MAX];
	char *out = resolved ? resolved : stack;
	size_t len = 0;
	out[len++] = '/';

	const char *p = path;
	while (*p) {
		while (*p == '/') {
			++p;
		}
		if (!*p) {
			break;
		}
		const char *seg = p;
		while (*p && *p != '/') {
			++p;
		}
		size_t n = (size_t)(p - seg);
		if (n == 1 && seg[0] == '.') {
			continue;
		}
		if (n == 2 && seg[0] == '.' && seg[1] == '.') {
			while (len > 1 && out[len - 1] != '/') {
				--len;
			}
			if (len > 1) {
				--len; // drop the separator too
			}
			continue;
		}
		if (len > 1) {
			out[len++] = '/';
		}
		if (len + n >= sprt::EL0_PATH_MAX) {
			__sprt_errno = ENAMETOOLONG;
			return nullptr;
		}
		__builtin_memcpy(out + len, seg, n);
		len += n;
	}
	out[len] = 0;

	struct __SPRT_STAT_NAME st;
	if (sprt::__el0_stat_path(__SPRT_AT_FDCWD, out, &st, 0) < 0) {
		return nullptr;
	}
	if (resolved) {
		return resolved;
	}
	auto copy = (char *)__sprt_malloc(len + 1);
	if (!copy) {
		__sprt_errno = ENOMEM;
		return nullptr;
	}
	__builtin_memcpy(copy, out, len + 1);
	return copy;
}

// --- not in the table yet ---------------------------------------------------
//
// M2 brings mkdirat(34), unlinkat(35), renameat(38), ftruncate(46),
// readlinkat(78), fsync(82), getcwd(17) and getdents64(61). Until then each of
// these is a call the kernel would not recognise, and saying so is the whole
// contribution: the alternative -- a stub that reports success -- would have
// callers believe a file was written, removed or renamed.

#define __EL0_ENOSYS_RET(Type, Value) \
	{ \
		__sprt_errno = ENOSYS; \
		return (Type)(Value); \
	}

__SPRT_C_FUNC int mkdir(const char *, __SPRT_ID(mode_t)) __SPRT_NOEXCEPT
		__EL0_ENOSYS_RET(int, -1)
__SPRT_C_FUNC int mkdirat(int, const char *, __SPRT_ID(mode_t)) __SPRT_NOEXCEPT
		__EL0_ENOSYS_RET(int, -1)
__SPRT_C_FUNC int rmdir(const char *) __SPRT_NOEXCEPT __EL0_ENOSYS_RET(int, -1)
__SPRT_C_FUNC int unlink(const char *) __SPRT_NOEXCEPT __EL0_ENOSYS_RET(int, -1)
__SPRT_C_FUNC int unlinkat(int, const char *, int) __SPRT_NOEXCEPT __EL0_ENOSYS_RET(int, -1)
__SPRT_C_FUNC int remove(const char *) __SPRT_NOEXCEPT __EL0_ENOSYS_RET(int, -1)
__SPRT_C_FUNC int rename(const char *, const char *) __SPRT_NOEXCEPT __EL0_ENOSYS_RET(int, -1)
__SPRT_C_FUNC int renameat(int, const char *, int, const char *) __SPRT_NOEXCEPT
		__EL0_ENOSYS_RET(int, -1)
__SPRT_C_FUNC int ftruncate(int, __SPRT_ID(off_t)) __SPRT_NOEXCEPT __EL0_ENOSYS_RET(int, -1)
__SPRT_C_FUNC int truncate(const char *, __SPRT_ID(off_t)) __SPRT_NOEXCEPT
		__EL0_ENOSYS_RET(int, -1)
__SPRT_C_FUNC int link(const char *, const char *) __SPRT_NOEXCEPT __EL0_ENOSYS_RET(int, -1)
__SPRT_C_FUNC int linkat(int, const char *, int, const char *, int) __SPRT_NOEXCEPT
		__EL0_ENOSYS_RET(int, -1)
__SPRT_C_FUNC int symlink(const char *, const char *) __SPRT_NOEXCEPT __EL0_ENOSYS_RET(int, -1)
__SPRT_C_FUNC int symlinkat(const char *, int, const char *) __SPRT_NOEXCEPT
		__EL0_ENOSYS_RET(int, -1)
__SPRT_C_FUNC __SPRT_ID(ssize_t)
		readlink(const char *, char *, __SPRT_ID(size_t)) __SPRT_NOEXCEPT
		__EL0_ENOSYS_RET(__SPRT_ID(ssize_t), -1)
__SPRT_C_FUNC __SPRT_ID(ssize_t)
		readlinkat(int, const char *, char *, __SPRT_ID(size_t)) __SPRT_NOEXCEPT
		__EL0_ENOSYS_RET(__SPRT_ID(ssize_t), -1)
__SPRT_C_FUNC int utimensat(int, const char *, const struct __SPRT_TIMESPEC_NAME *,
		int) __SPRT_NOEXCEPT __EL0_ENOSYS_RET(int, -1)
__SPRT_C_FUNC int chmod(const char *, __SPRT_ID(mode_t)) __SPRT_NOEXCEPT __EL0_ENOSYS_RET(int, -1)
__SPRT_C_FUNC int fchmodat(int, const char *, __SPRT_ID(mode_t), int) __SPRT_NOEXCEPT
		__EL0_ENOSYS_RET(int, -1)

// A successful fsync is a promise that the data has reached the device. Nothing
// in this build can make that promise -- fsync(82) is M2 -- so it must not
// return 0. Callers that treat ENOSYS as fatal are rare; callers that lose data
// because a flush silently did nothing are not.
__SPRT_C_FUNC int fsync(int) __SPRT_NOEXCEPT __EL0_ENOSYS_RET(int, -1)
__SPRT_C_FUNC int fdatasync(int) __SPRT_NOEXCEPT __EL0_ENOSYS_RET(int, -1)

// getcwd(17) has a number reserved in the kernel header but no dispatcher case,
// so it answers ENOSYS there too -- this just saves the trap.
__SPRT_C_FUNC char *getcwd(char *, __SPRT_ID(size_t)) __SPRT_NOEXCEPT
		__EL0_ENOSYS_RET(char *, nullptr)
__SPRT_C_FUNC int chdir(const char *) __SPRT_NOEXCEPT __EL0_ENOSYS_RET(int, -1)
__SPRT_C_FUNC int fchdir(int) __SPRT_NOEXCEPT __EL0_ENOSYS_RET(int, -1)

// Directory enumeration needs getdents64(61). opendir failing means no directory
// descriptor is ever created, which is why libc_dir_ops.cc has nothing to do.
__SPRT_C_FUNC __SPRT_ID(DIR) * opendir(const char *) __SPRT_NOEXCEPT
		__EL0_ENOSYS_RET(__SPRT_ID(DIR) *, nullptr)
__SPRT_C_FUNC __SPRT_ID(DIR) * fdopendir(int) __SPRT_NOEXCEPT
		__EL0_ENOSYS_RET(__SPRT_ID(DIR) *, nullptr)
__SPRT_C_FUNC struct __SPRT_DIRENT_NAME *readdir(__SPRT_ID(DIR) *) __SPRT_NOEXCEPT
		__EL0_ENOSYS_RET(struct __SPRT_DIRENT_NAME *, nullptr)
__SPRT_C_FUNC int closedir(__SPRT_ID(DIR) *) __SPRT_NOEXCEPT __EL0_ENOSYS_RET(int, -1)
__SPRT_C_FUNC int rewinddir(__SPRT_ID(DIR) *) __SPRT_NOEXCEPT __EL0_ENOSYS_RET(int, -1)
__SPRT_C_FUNC int seekdir(__SPRT_ID(DIR) *, long) __SPRT_NOEXCEPT __EL0_ENOSYS_RET(int, -1)
__SPRT_C_FUNC long telldir(__SPRT_ID(DIR) *) __SPRT_NOEXCEPT __EL0_ENOSYS_RET(long, -1)
__SPRT_C_FUNC int dirfd(__SPRT_ID(DIR) *) __SPRT_NOEXCEPT __EL0_ENOSYS_RET(int, -1)

__SPRT_C_FUNC __SPRT_ID(FILE) * tmpfile(void) __SPRT_NOEXCEPT
		__EL0_ENOSYS_RET(__SPRT_ID(FILE) *, nullptr)

#undef __EL0_ENOSYS_RET

// tmpnam and tmpfile need a writable directory to create a file in AND a way to
// remove it afterwards. openat(56) provides the first; unlink is M2, so a
// temporary file created now would be permanent. Returning a name that cannot be
// cleaned up is worse than refusing: the caller would leave litter on a
// read-only-ish initfs with no way to notice.
__SPRT_C_FUNC char *tmpnam(char *s) __SPRT_NOEXCEPT {
	(void)s;
	__sprt_errno = ENOSYS;
	return nullptr;
}

// sigsetjmp on this target IS setjmp (runtime_core_setjmp.cpp: there is no
// signal mask to save until K8), so siglongjmp is longjmp. Keeping the two
// statements in agreement is why this lives here rather than being borrowed from
// musl, whose siglongjmp restores a mask that was never saved.
extern "C" __SPRT_NORETURN void longjmp(__SPRT_ID(native_jmp_buf), int) __SPRT_NOEXCEPT;

extern "C" __SPRT_NORETURN void siglongjmp(__SPRT_ID(native_sigjmp_buf) buf,
		int ret) __SPRT_NOEXCEPT {
	longjmp(buf, ret);
}
