// Paths on Tau: the namespace of the module (tau.fs@1 through libtau, sprt_tau.h). The
// working directory is a path kept here; relative paths from AT_FDCWD are made absolute
// from it, so no directory handle is shared between threads.

#include "dirent.h"
#include "fcntl.h"
#include "stdarg.h"
#include "stdio.h"
#include "string.h"
#include "unistd.h"
#include "sys/stat.h"
#include "tau_fs.h"

#include <sprt/c/__sprt_errno.h>
#include <sprt/c/__sprt_stdlib.h>
#include <sprt/c/cross/__sprt_setjmp.h>

namespace sprt {

static mutex cwdMutex;
static char cwdPath[TAU_PATH_MAX] = "/";

bool __tau_path_ok(const char *path) {
	if (!path) {
		__sprt_errno = EFAULT;
		return false;
	}
	if (!*path) {
		__sprt_errno = ENOENT;
		return false;
	}
	if (__builtin_strlen(path) >= TAU_PATH_MAX) {
		__sprt_errno = ENAMETOOLONG;
		return false;
	}
	return true;
}

bool __tau_at(int dirfd, const char *path, char *buf, __sprt_tau_file **dir, const char **out) {
	*dir = nullptr;
	*out = path;
	if (path[0] == '/') {
		return true;
	}
	if (dirfd != __SPRT_AT_FDCWD) {
		auto libc = __libc::get();
		auto slot = libc->get_fd_slot(dirfd);
		if (!slot || !slot->handle) {
			__sprt_errno = EBADF;
			return false;
		}
		if (slot->ops != __tau_fs_ops() && slot->ops != &libc->fdDirOps) {
			__sprt_errno = ENOTDIR;
			return false;
		}
		*dir = static_cast<__tau_fd *>(slot->handle)->file;
		return true;
	}
	unique_lock lock(cwdMutex);
	size_t clen = __builtin_strlen(cwdPath), plen = __builtin_strlen(path);
	if (clen + 1 + plen >= TAU_PATH_MAX) {
		__sprt_errno = ENAMETOOLONG;
		return false;
	}
	__builtin_memcpy(buf, cwdPath, clen);
	if (clen > 1) {
		buf[clen++] = '/';
	}
	__builtin_memcpy(buf + clen, path, plen + 1);
	*out = buf;
	return true;
}

// `path` made absolute and without ".", ".." and repeated slashes, into out.
static bool __tau_normalize(const char *path, char *out) {
	char buf[TAU_PATH_MAX];
	__sprt_tau_file *dir = nullptr;
	if (!__tau_at(__SPRT_AT_FDCWD, path, buf, &dir, &path)) {
		return false;
	}
	size_t len = 0;
	out[len++] = '/';
	for (const char *p = path; *p;) {
		while (*p == '/') {
			++p;
		}
		const char *seg = p;
		while (*p && *p != '/') {
			++p;
		}
		size_t n = (size_t)(p - seg);
		if (!n || (n == 1 && seg[0] == '.')) {
			continue;
		}
		if (n == 2 && seg[0] == '.' && seg[1] == '.') {
			while (len > 1 && out[len - 1] != '/') {
				--len;
			}
			len -= len > 1 ? 1 : 0;
			continue;
		}
		if (len + 1 + n >= TAU_PATH_MAX) {
			__sprt_errno = ENAMETOOLONG;
			return false;
		}
		if (len > 1) {
			out[len++] = '/';
		}
		__builtin_memcpy(out + len, seg, n);
		len += n;
	}
	out[len] = 0;
	return true;
}

static int __tau_stat_at(int dirfd, const char *path, struct __SPRT_STAT_NAME *st, int flags) {
	char buf[TAU_PATH_MAX];
	__sprt_tau_file *dir = nullptr;
	if ((flags & __SPRT_AT_EMPTY_PATH) && path && !*path && dirfd != __SPRT_AT_FDCWD) {
		return fstat(dirfd, st);
	}
	if (!__tau_path_ok(path) || !__tau_at(dirfd, path, buf, &dir, &path)) {
		return -1;
	}
	__sprt_tau_fs_stat s;
	if (__tau_ret(__sprt_tau_fs_statat(dir, path, (flags & __SPRT_AT_SYMLINK_NOFOLLOW) != 0, &s)) < 0) {
		return -1;
	}
	__tau_to_stat(s, st);
	return 0;
}

// The calls that take a path and give a status: f(dir, path).
template <typename F>
static int __tau_path_call(int dirfd, const char *path, F &&f) {
	char buf[TAU_PATH_MAX];
	__sprt_tau_file *dir = nullptr;
	if (!__tau_path_ok(path) || !__tau_at(dirfd, path, buf, &dir, &path)) {
		return -1;
	}
	return (int)__tau_ret(f(dir, path));
}

template <typename F>
static int __tau_pair_call(int dirfd, const char *a, int dirfd2, const char *b, F &&f) {
	char buf[TAU_PATH_MAX], buf2[TAU_PATH_MAX];
	__sprt_tau_file *dir = nullptr, *dir2 = nullptr;
	if (!__tau_path_ok(a) || !__tau_path_ok(b) || !__tau_at(dirfd, a, buf, &dir, &a) || !__tau_at(dirfd2, b, buf2, &dir2, &b)) {
		return -1;
	}
	return (int)__tau_ret(f(dir, a, dir2, b));
}

static int __fopen_flags(const char *m) {
	if (!m) {
		return -1;
	}
	bool plus = false;
	for (const char *p = m; *p; ++p) {
		plus = plus || *p == '+';
	}
	switch (m[0]) {
	case 'r': return plus ? __SPRT_O_RDWR : __SPRT_O_RDONLY;
	case 'w': return (plus ? __SPRT_O_RDWR : __SPRT_O_WRONLY) | __SPRT_O_CREAT | __SPRT_O_TRUNC;
	case 'a': return (plus ? __SPRT_O_RDWR : __SPRT_O_WRONLY) | __SPRT_O_CREAT | __SPRT_O_APPEND;
	default: return -1;
	}
}

} // namespace sprt

__SPRT_C_FUNC int openat(int dirfd, const char *path, int flags, ...) __SPRT_NOEXCEPT {
	__SPRT_ID(mode_t) mode = 0;
	if (flags & __SPRT_O_CREAT) {
		__sprt_va_list ap;
		__sprt_va_start(ap, flags);
		mode = (__SPRT_ID(mode_t))__sprt_va_arg(ap, int);
		__sprt_va_end(ap);
	}
	return sprt::__tau_open_fd(dirfd, path, flags, mode, sprt::__tau_fs_ops());
}

__SPRT_C_FUNC int open(const char *path, int flags, ...) __SPRT_NOEXCEPT {
	__SPRT_ID(mode_t) mode = 0;
	if (flags & __SPRT_O_CREAT) {
		__sprt_va_list ap;
		__sprt_va_start(ap, flags);
		mode = (__SPRT_ID(mode_t))__sprt_va_arg(ap, int);
		__sprt_va_end(ap);
	}
	return sprt::__tau_open_fd(__SPRT_AT_FDCWD, path, flags, mode, sprt::__tau_fs_ops());
}

__SPRT_C_FUNC FILE *fopen(const char *path, const char *mode) __SPRT_NOEXCEPT {
	int flags = sprt::__fopen_flags(mode);
	if (flags < 0) {
		__sprt_errno = EINVAL;
		return nullptr;
	}
	int fd = sprt::__tau_open_fd(__SPRT_AT_FDCWD, path, flags, 0'666, sprt::__tau_fs_ops());
	if (fd < 0) {
		return nullptr;
	}
	FILE *f = fdopen(fd, mode);
	if (!f) {
		close(fd);
	}
	return f;
}

__SPRT_C_FUNC int stat(const char *__SPRT_RESTRICT path, struct __SPRT_STAT_NAME *__SPRT_RESTRICT st) __SPRT_NOEXCEPT {
	return sprt::__tau_stat_at(__SPRT_AT_FDCWD, path, st, 0);
}

__SPRT_C_FUNC int lstat(const char *__SPRT_RESTRICT path, struct __SPRT_STAT_NAME *__SPRT_RESTRICT st) __SPRT_NOEXCEPT {
	return sprt::__tau_stat_at(__SPRT_AT_FDCWD, path, st, __SPRT_AT_SYMLINK_NOFOLLOW);
}

__SPRT_C_FUNC int fstatat(int dirfd, const char *__SPRT_RESTRICT path, struct __SPRT_STAT_NAME *__SPRT_RESTRICT st,
		int flags) __SPRT_NOEXCEPT {
	return sprt::__tau_stat_at(dirfd, path, st, flags);
}

// No users: an existing name may be read and written, and executed when an x bit is set.
__SPRT_C_FUNC int faccessat(int dirfd, const char *path, int amode, int flags) __SPRT_NOEXCEPT {
	struct __SPRT_STAT_NAME st;
	if (sprt::__tau_stat_at(dirfd, path, &st, flags & __SPRT_AT_SYMLINK_NOFOLLOW) < 0) {
		return -1;
	}
	if ((amode & __SPRT_X_OK) && !(st.st_mode & 0'111)) {
		__sprt_errno = EACCES;
		return -1;
	}
	return 0;
}

__SPRT_C_FUNC int access(const char *path, int amode) __SPRT_NOEXCEPT { return faccessat(__SPRT_AT_FDCWD, path, amode, 0); }

__SPRT_C_FUNC int utimensat(int dirfd, const char *path, const struct __SPRT_TIMESPEC_NAME *times, int flags) __SPRT_NOEXCEPT {
	int fd = sprt::__tau_open_fd(dirfd, path, __SPRT_O_RDONLY | ((flags & __SPRT_AT_SYMLINK_NOFOLLOW) ? __SPRT_O_NOFOLLOW : 0),
			0, sprt::__tau_fs_ops());
	if (fd < 0) {
		return -1;
	}
	int r = futimens(fd, times);
	close(fd);
	return r;
}

// Lexical, then checked: a path through a symbolic link resolves as the link's directory.
__SPRT_C_FUNC char *realpath(const char *path, char *resolved) __SPRT_NOEXCEPT {
	char out[sprt::TAU_PATH_MAX];
	if (!sprt::__tau_path_ok(path) || !sprt::__tau_normalize(path, out)) {
		return nullptr;
	}
	struct __SPRT_STAT_NAME st;
	if (sprt::__tau_stat_at(__SPRT_AT_FDCWD, out, &st, 0) < 0) {
		return nullptr;
	}
	size_t len = __builtin_strlen(out);
	char *dst = resolved ? resolved : (char *)__sprt_malloc(len + 1);
	if (!dst) {
		__sprt_errno = ENOMEM;
		return nullptr;
	}
	__builtin_memcpy(dst, out, len + 1);
	return dst;
}

__SPRT_C_FUNC int mkdirat(int dirfd, const char *path, __SPRT_ID(mode_t) mode) __SPRT_NOEXCEPT {
	return sprt::__tau_path_call(dirfd, path, [&](__sprt_tau_file *d, const char *p) { return __sprt_tau_fs_mkdir(d, p, mode); });
}

__SPRT_C_FUNC int mkdir(const char *path, __SPRT_ID(mode_t) mode) __SPRT_NOEXCEPT { return mkdirat(__SPRT_AT_FDCWD, path, mode); }

__SPRT_C_FUNC int unlinkat(int dirfd, const char *path, int flags) __SPRT_NOEXCEPT {
	return sprt::__tau_path_call(dirfd, path, [&](__sprt_tau_file *d, const char *p) {
		return (flags & __SPRT_AT_REMOVEDIR) ? __sprt_tau_fs_rmdir(d, p) : __sprt_tau_fs_unlink(d, p);
	});
}

__SPRT_C_FUNC int unlink(const char *path) __SPRT_NOEXCEPT { return unlinkat(__SPRT_AT_FDCWD, path, 0); }

__SPRT_C_FUNC int rmdir(const char *path) __SPRT_NOEXCEPT { return unlinkat(__SPRT_AT_FDCWD, path, __SPRT_AT_REMOVEDIR); }

__SPRT_C_FUNC int remove(const char *path) __SPRT_NOEXCEPT {
	struct __SPRT_STAT_NAME st;
	if (sprt::__tau_stat_at(__SPRT_AT_FDCWD, path, &st, __SPRT_AT_SYMLINK_NOFOLLOW) == 0 && __SPRT_S_ISDIR(st.st_mode)) {
		return rmdir(path);
	}
	return unlink(path);
}

__SPRT_C_FUNC int renameat(int olddirfd, const char *from, int newdirfd, const char *to) __SPRT_NOEXCEPT {
	return sprt::__tau_pair_call(olddirfd, from, newdirfd, to, &__sprt_tau_fs_rename);
}

__SPRT_C_FUNC int rename(const char *from, const char *to) __SPRT_NOEXCEPT {
	return renameat(__SPRT_AT_FDCWD, from, __SPRT_AT_FDCWD, to);
}

__SPRT_C_FUNC int linkat(int olddirfd, const char *from, int newdirfd, const char *to, int) __SPRT_NOEXCEPT {
	return sprt::__tau_pair_call(olddirfd, from, newdirfd, to, &__sprt_tau_fs_link);
}

__SPRT_C_FUNC int link(const char *from, const char *to) __SPRT_NOEXCEPT {
	return linkat(__SPRT_AT_FDCWD, from, __SPRT_AT_FDCWD, to, 0);
}

__SPRT_C_FUNC int symlinkat(const char *target, int dirfd, const char *path) __SPRT_NOEXCEPT {
	if (!target) {
		__sprt_errno = EFAULT;
		return -1;
	}
	return sprt::__tau_path_call(dirfd, path, [&](__sprt_tau_file *d, const char *p) { return __sprt_tau_fs_symlink(target, d, p); });
}

__SPRT_C_FUNC int symlink(const char *target, const char *path) __SPRT_NOEXCEPT {
	return symlinkat(target, __SPRT_AT_FDCWD, path);
}

__SPRT_C_FUNC __SPRT_ID(ssize_t) readlinkat(int dirfd, const char *path, char *buf, __SPRT_ID(size_t) n) __SPRT_NOEXCEPT {
	char tmp[sprt::TAU_PATH_MAX];
	__sprt_tau_file *dir = nullptr;
	if (!sprt::__tau_path_ok(path) || !sprt::__tau_at(dirfd, path, tmp, &dir, &path)) {
		return -1;
	}
	return (__SPRT_ID(ssize_t))sprt::__tau_ret(__sprt_tau_fs_readlink(dir, path, buf, n));
}

__SPRT_C_FUNC __SPRT_ID(ssize_t) readlink(const char *path, char *buf, __SPRT_ID(size_t) n) __SPRT_NOEXCEPT {
	return readlinkat(__SPRT_AT_FDCWD, path, buf, n);
}

static sprt::__tau_fd *__tau_fd_of(int fd) {
	auto libc = sprt::__libc::get();
	auto slot = libc->get_fd_slot(fd);
	if (!slot || !slot->handle || (slot->ops != sprt::__tau_fs_ops() && slot->ops != &libc->fdDirOps)) {
		__sprt_errno = slot && slot->handle ? EINVAL : EBADF;
		return nullptr;
	}
	return static_cast<sprt::__tau_fd *>(slot->handle);
}

__SPRT_C_FUNC int ftruncate(int fd, __SPRT_ID(off_t) length) __SPRT_NOEXCEPT {
	auto f = __tau_fd_of(fd);
	if (!f || length < 0) {
		__sprt_errno = f ? EINVAL : __sprt_errno;
		return -1;
	}
	return (int)sprt::__tau_ret(__sprt_tau_fs_ftruncate(f->file, (unsigned long long)length));
}

__SPRT_C_FUNC int truncate(const char *path, __SPRT_ID(off_t) length) __SPRT_NOEXCEPT {
	int fd = sprt::__tau_open_fd(__SPRT_AT_FDCWD, path, __SPRT_O_WRONLY, 0, sprt::__tau_fs_ops());
	if (fd < 0) {
		return -1;
	}
	int r = ftruncate(fd, length);
	close(fd);
	return r;
}

// The console's streams have nothing to write back.
__SPRT_C_FUNC int fsync(int fd) __SPRT_NOEXCEPT {
	auto libc = sprt::__libc::get();
	auto slot = libc->get_fd_slot(fd);
	if (!slot || !slot->handle) {
		__sprt_errno = EBADF;
		return -1;
	}
	if (slot->ops != sprt::__tau_fs_ops()) {
		return 0;
	}
	return (int)sprt::__tau_ret(__sprt_tau_fs_fsync(static_cast<sprt::__tau_fd *>(slot->handle)->file));
}

__SPRT_C_FUNC int fdatasync(int fd) __SPRT_NOEXCEPT { return fsync(fd); }

__SPRT_C_FUNC int chmod(const char *path, __SPRT_ID(mode_t) mode) __SPRT_NOEXCEPT {
	return fchmodat(__SPRT_AT_FDCWD, path, mode, 0);
}

__SPRT_C_FUNC int fchmodat(int dirfd, const char *path, __SPRT_ID(mode_t) mode, int flags) __SPRT_NOEXCEPT {
	int fd = sprt::__tau_open_fd(dirfd, path, __SPRT_O_RDONLY | ((flags & __SPRT_AT_SYMLINK_NOFOLLOW) ? __SPRT_O_NOFOLLOW : 0),
			0, sprt::__tau_fs_ops());
	if (fd < 0) {
		return -1;
	}
	int r = fchmod(fd, mode);
	close(fd);
	return r;
}

__SPRT_C_FUNC char *getcwd(char *buf, __SPRT_ID(size_t) size) __SPRT_NOEXCEPT {
	sprt::unique_lock lock(sprt::cwdMutex);
	size_t n = __builtin_strlen(sprt::cwdPath) + 1;
	if (!buf) {
		buf = (char *)__sprt_malloc(size > n ? size : n);
		if (!buf) {
			__sprt_errno = ENOMEM;
			return nullptr;
		}
	} else if (size < n) {
		__sprt_errno = size ? ERANGE : EINVAL;
		return nullptr;
	}
	__builtin_memcpy(buf, sprt::cwdPath, n);
	return buf;
}

__SPRT_C_FUNC int chdir(const char *path) __SPRT_NOEXCEPT {
	char out[sprt::TAU_PATH_MAX];
	struct __SPRT_STAT_NAME st;
	if (!sprt::__tau_path_ok(path) || !sprt::__tau_normalize(path, out) || sprt::__tau_stat_at(__SPRT_AT_FDCWD, out, &st, 0) < 0) {
		return -1;
	}
	if (!__SPRT_S_ISDIR(st.st_mode)) {
		__sprt_errno = ENOTDIR;
		return -1;
	}
	sprt::unique_lock lock(sprt::cwdMutex);
	__builtin_memcpy(sprt::cwdPath, out, __builtin_strlen(out) + 1);
	return 0;
}

// A descriptor does not carry its path.
__SPRT_C_FUNC int fchdir(int) __SPRT_NOEXCEPT {
	__sprt_errno = ENOSYS;
	return -1;
}

__SPRT_C_FUNC __SPRT_ID(FILE) * tmpfile(void) __SPRT_NOEXCEPT {
	__sprt_errno = ENOSYS;
	return nullptr;
}

__SPRT_C_FUNC char *tmpnam(char *) __SPRT_NOEXCEPT {
	__sprt_errno = ENOSYS;
	return nullptr;
}

// sigsetjmp is setjmp here (runtime_core_setjmp.cpp): no signal mask to restore.
extern "C" __SPRT_NORETURN void longjmp(__SPRT_ID(native_jmp_buf), int) __SPRT_NOEXCEPT;

extern "C" __SPRT_NORETURN void siglongjmp(__SPRT_ID(native_sigjmp_buf) buf, int ret) __SPRT_NOEXCEPT {
	longjmp(buf, ret);
}
