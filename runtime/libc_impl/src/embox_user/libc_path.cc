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

static int __el0_at_dirfd(int dirfd, const char *path) {
	if (dirfd == __SPRT_AT_FDCWD || (path && path[0] == '/')) {
		return __SPRT_AT_FDCWD;
	}
	auto libc = __libc::get();
	auto slot = libc->get_fd_slot(dirfd);
	if (!slot) {
		__sprt_errno = EBADF;
		return -1;
	}
	if (slot->ops != &libc->fdFileOps) {
		__sprt_errno = ENOTDIR;
		return -1;
	}
	return __el0_kfd(slot);
}

// Open through the kernel and register the descriptor in a libc slot. The two fd
// numbers are unrelated: the kernel's goes in the slot's handle, the libc's is
// what the caller gets back.
static int __el0_open_slot(int dirfd, const char *path, int flags, mode_t mode) {
	if (!__el0_path_ok(path)) {
		return -1;
	}
	auto kdir = __el0_at_dirfd(dirfd, path);
	if (kdir == -1) {
		return -1;
	}
	auto kfd = (int)__el0_ret(__el0_openat(kdir, path, flags, mode));
	if (kfd < 0) {
		return -1;
	}
	auto libc = sprt::__libc::get();
	auto fd = libc->create_fd(__el0_handle(kfd), &libc->fdFileOps, (uint32_t)flags, (uint32_t)mode);
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
	auto kdir = __el0_at_dirfd(dirfd, path);
	if (kdir == -1) {
		return -1;
	}
	__el0_kstat ks;
	if (__el0_ret(__el0_newfstatat(kdir, path, &ks, flags)) < 0) {
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
	// dirfd is translated to the kernel's descriptor (__el0_at_dirfd), and the
	// kernel resolves the relative path against the directory it names.
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
// Since M2 the kernel performs exactly this emulation, from the same stat and
// for the same reason, so the call goes there rather than being answered twice
// in two places that could drift apart.
__SPRT_C_FUNC int faccessat(int dirfd, const char *path, int amode, int flags) __SPRT_NOEXCEPT {
	if (!sprt::__el0_path_ok(path)) {
		return -1;
	}
	auto kdir = sprt::__el0_at_dirfd(dirfd, path);
	if (kdir == -1) {
		return -1;
	}
	return (int)__el0_ret(__el0_faccessat(kdir, path, amode, flags));
}

__SPRT_C_FUNC int access(const char *path, int amode) __SPRT_NOEXCEPT {
	return faccessat(__SPRT_AT_FDCWD, path, amode, 0);
}

// realpath on a filesystem with no symbolic links is lexical normalisation plus
// an existence check -- there is nothing left for it to resolve. Embox's initfs,
// ramfs and FAT have no links, and no syscall could follow one if they did. A
// relative path is made absolute against the working directory, which getcwd(17)
// can answer for since M2 -- before that this was the one input realpath had to
// refuse.
__SPRT_C_FUNC char *realpath(const char *path, char *resolved) __SPRT_NOEXCEPT {
	if (!sprt::__el0_path_ok(path)) {
		return nullptr;
	}

	char joined[sprt::EL0_PATH_MAX];
	if (path[0] != '/') {
		char cwd[sprt::EL0_PATH_MAX];
		if (__el0_ret(__el0_getcwd(cwd, sizeof(cwd))) < 0) {
			return nullptr;
		}
		size_t clen = __builtin_strlen(cwd);
		size_t plen = __builtin_strlen(path);
		bool sep = clen > 0 && cwd[clen - 1] != '/';
		if (clen + (sep ? 1u : 0u) + plen + 1 > sizeof(joined)) {
			__sprt_errno = ENAMETOOLONG;
			return nullptr;
		}
		__builtin_memcpy(joined, cwd, clen);
		if (sep) {
			joined[clen++] = '/';
		}
		__builtin_memcpy(joined + clen, path, plen + 1);
		path = joined;
	}

	char stack[sprt::EL0_PATH_MAX];
	char *out = resolved ? resolved : stack;
	size_t len = 0;
	out[len++] = '/';

	const char *p = path;
	while (*p) {
		while (*p == '/') { ++p; }
		if (!*p) {
			break;
		}
		const char *seg = p;
		while (*p && *p != '/') { ++p; }
		size_t n = (size_t)(p - seg);
		if (n == 1 && seg[0] == '.') {
			continue;
		}
		if (n == 2 && seg[0] == '.' && seg[1] == '.') {
			while (len > 1 && out[len - 1] != '/') { --len; }
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

// --- paths, over the M2 numbers ---------------------------------------------
//
// mkdirat(34), unlinkat(35), renameat(38), ftruncate(46), faccessat(48),
// readlinkat(78), fsync(82), getcwd(17) and chdir(49) are all implemented now.
// Three of them do not behave quite as a Linux caller expects, and nothing on
// this side can detect the difference, so it is stated rather than hidden:
//
//   rename     is copy-then-delete in the kernel below, so it is NOT atomic;
//              a crash between the halves leaves both names
//   readlink   is always EINVAL on a path that exists -- no filesystem in this
//              image has symbolic links, and nothing can create one
//   access     has no user model to consult, so R_OK and W_OK are answered by
//              the path existing (what a caller running as root would get) and
//              only X_OK can fail
//
// What is still missing is still refused rather than faked: link, symlink,
// chmod and utimensat have no numbers, and a stub that reported success would
// have callers believe a file was linked, made executable or touched.

#define __EL0_ENOSYS_RET(Type, Value) \
	{ \
		__sprt_errno = ENOSYS; \
		return (Type)(Value); \
	}

__SPRT_C_FUNC int mkdirat(int dirfd, const char *path, __SPRT_ID(mode_t) mode) __SPRT_NOEXCEPT {
	if (!sprt::__el0_path_ok(path)) {
		return -1;
	}
	auto kdir = sprt::__el0_at_dirfd(dirfd, path);
	if (kdir == -1) {
		return -1;
	}
	return (int)__el0_ret(__el0_mkdirat(kdir, path, mode));
}

__SPRT_C_FUNC int mkdir(const char *path, __SPRT_ID(mode_t) mode) __SPRT_NOEXCEPT {
	return mkdirat(__SPRT_AT_FDCWD, path, mode);
}

__SPRT_C_FUNC int unlinkat(int dirfd, const char *path, int flags) __SPRT_NOEXCEPT {
	if (!sprt::__el0_path_ok(path)) {
		return -1;
	}
	auto kdir = sprt::__el0_at_dirfd(dirfd, path);
	if (kdir == -1) {
		return -1;
	}
	return (int)__el0_ret(__el0_unlinkat(kdir, path, flags));
}

__SPRT_C_FUNC int unlink(const char *path) __SPRT_NOEXCEPT {
	return unlinkat(__SPRT_AT_FDCWD, path, 0);
}

__SPRT_C_FUNC int rmdir(const char *path) __SPRT_NOEXCEPT {
	return unlinkat(__SPRT_AT_FDCWD, path, __SPRT_AT_REMOVEDIR);
}

// remove() is rmdir for a directory and unlink for everything else, and the
// only way to tell which is to look. A caller passing a directory to unlink
// would otherwise get EISDIR from a function documented to handle both.
__SPRT_C_FUNC int remove(const char *path) __SPRT_NOEXCEPT {
	struct __SPRT_STAT_NAME st;
	if (sprt::__el0_stat_path(__SPRT_AT_FDCWD, path, &st, 0) == 0 && __SPRT_S_ISDIR(st.st_mode)) {
		return rmdir(path);
	}
	return unlink(path);
}

__SPRT_C_FUNC int renameat(int olddirfd, const char *old_path, int newdirfd,
		const char *new_path) __SPRT_NOEXCEPT {
	if (!sprt::__el0_path_ok(old_path) || !sprt::__el0_path_ok(new_path)) {
		return -1;
	}
	auto kold = sprt::__el0_at_dirfd(olddirfd, old_path);
	if (kold == -1) {
		return -1;
	}
	auto knew = sprt::__el0_at_dirfd(newdirfd, new_path);
	if (knew == -1) {
		return -1;
	}
	return (int)__el0_ret(__el0_renameat(kold, old_path, knew, new_path));
}

__SPRT_C_FUNC int rename(const char *old_path, const char *new_path) __SPRT_NOEXCEPT {
	return renameat(__SPRT_AT_FDCWD, old_path, __SPRT_AT_FDCWD, new_path);
}

__SPRT_C_FUNC int ftruncate(int fd, __SPRT_ID(off_t) length) __SPRT_NOEXCEPT {
	auto libc = sprt::__libc::get();
	auto slot = libc->get_fd_slot(fd);
	if (!slot || !slot->handle) {
		__sprt_errno = EBADF;
		return -1;
	}
	return (int)__el0_ret(__el0_ftruncate(sprt::__el0_kfd(slot), (long)length));
}

// truncate(path) has no number of its own; open, ftruncate, close is the same
// operation and the only difference a caller could see is that this one needs a
// free descriptor.
__SPRT_C_FUNC int truncate(const char *path, __SPRT_ID(off_t) length) __SPRT_NOEXCEPT {
	if (!sprt::__el0_path_ok(path)) {
		return -1;
	}
	auto kfd = (int)__el0_ret(__el0_openat(__SPRT_AT_FDCWD, path, __SPRT_O_WRONLY, 0));
	if (kfd < 0) {
		return -1;
	}
	auto ret = (int)__el0_ret(__el0_ftruncate(kfd, (long)length));
	__el0_close(kfd);
	return ret;
}

__SPRT_C_FUNC __SPRT_ID(ssize_t)
		readlinkat(int dirfd, const char *path, char *buf, __SPRT_ID(size_t) n) __SPRT_NOEXCEPT {
	if (!sprt::__el0_path_ok(path)) {
		return -1;
	}
	auto kdir = sprt::__el0_at_dirfd(dirfd, path);
	if (kdir == -1) {
		return -1;
	}
	return (__SPRT_ID(ssize_t))__el0_ret(__el0_readlinkat(kdir, path, buf, n));
}

__SPRT_C_FUNC __SPRT_ID(ssize_t)
		readlink(const char *path, char *buf, __SPRT_ID(size_t) n) __SPRT_NOEXCEPT {
	return readlinkat(__SPRT_AT_FDCWD, path, buf, n);
}

// Succeeding without doing anything is the truth here rather than a promise
// broken quietly: no filesystem in this image has a write-back cache, so a
// write that returned is already where fsync would put it. The descriptor is
// still checked, which is what makes fsync(-1) EBADF.
__SPRT_C_FUNC int fsync(int fd) __SPRT_NOEXCEPT {
	auto libc = sprt::__libc::get();
	auto slot = libc->get_fd_slot(fd);
	if (!slot || !slot->handle) {
		__sprt_errno = EBADF;
		return -1;
	}
	return (int)__el0_ret(__el0_fsync(sprt::__el0_kfd(slot)));
}

__SPRT_C_FUNC int fdatasync(int fd) __SPRT_NOEXCEPT { return fsync(fd); }

__SPRT_C_FUNC char *getcwd(char *buf, __SPRT_ID(size_t) size) __SPRT_NOEXCEPT {
	// getcwd(nullptr, 0) is the GNU extension that allocates. The syscall
	// cannot do it, so the path is fetched into a local first -- the kernel's
	// own ceiling is well under this.
	char local[sprt::EL0_PATH_MAX];
	if (!buf) {
		if (__el0_ret(__el0_getcwd(local, sizeof(local))) < 0) {
			return nullptr;
		}
		size_t n = __builtin_strlen(local) + 1;
		auto out = (char *)__sprt_malloc(size > n ? size : n);
		if (!out) {
			__sprt_errno = ENOMEM;
			return nullptr;
		}
		__builtin_memcpy(out, local, n);
		return out;
	}
	if (size == 0) {
		__sprt_errno = EINVAL;
		return nullptr;
	}
	// The raw syscall answers with the length, this function with the buffer.
	if (__el0_ret(__el0_getcwd(buf, size)) < 0) {
		return nullptr;
	}
	return buf;
}

__SPRT_C_FUNC int chdir(const char *path) __SPRT_NOEXCEPT {
	if (!sprt::__el0_path_ok(path)) {
		return -1;
	}
	return (int)__el0_ret(__el0_chdir(path));
}

// fchdir needs a path to hand chdir, and a descriptor here does not carry one:
// the kernel remembers a directory's path but does not publish it. Refusing is
// better than guessing.
__SPRT_C_FUNC int fchdir(int) __SPRT_NOEXCEPT __EL0_ENOSYS_RET(int, -1)

				__SPRT_C_FUNC int link(const char *, const char *) __SPRT_NOEXCEPT
		__EL0_ENOSYS_RET(int, -1) __SPRT_C_FUNC
		int linkat(int, const char *, int, const char *, int) __SPRT_NOEXCEPT
		__EL0_ENOSYS_RET(int, -1) __SPRT_C_FUNC
		int symlink(const char *, const char *) __SPRT_NOEXCEPT
		__EL0_ENOSYS_RET(int, -1) __SPRT_C_FUNC
		int symlinkat(const char *, int, const char *) __SPRT_NOEXCEPT
		__EL0_ENOSYS_RET(int, -1) __SPRT_C_FUNC
		int utimensat(int, const char *, const struct __SPRT_TIMESPEC_NAME *, int) __SPRT_NOEXCEPT
		__EL0_ENOSYS_RET(int, -1) __SPRT_C_FUNC
		int chmod(const char *, __SPRT_ID(mode_t)) __SPRT_NOEXCEPT
		__EL0_ENOSYS_RET(int, -1) __SPRT_C_FUNC
		int fchmodat(int, const char *, __SPRT_ID(mode_t), int) __SPRT_NOEXCEPT
		__EL0_ENOSYS_RET(int, -1)

				__SPRT_C_FUNC __SPRT_ID(FILE)
		* tmpfile(void) __SPRT_NOEXCEPT __EL0_ENOSYS_RET(__SPRT_ID(FILE) *, nullptr)

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
