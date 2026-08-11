/**
Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#define __SPRT_BUILD 1
// musl only exposes the *64 file API when _LARGEFILE64_SOURCE is set (its plain
// symbols are already 64-bit); on glibc this is already implied by _GNU_SOURCE.
#define _LARGEFILE64_SOURCE 1

#include <sprt/c/sys/__sprt_stat.h>
#include <sprt/c/sys/__sprt_sendfile.h>
#include <sprt/c/sys/__sprt_statvfs.h>
#include <sprt/c/__sprt_errno.h>
#include <sprt/c/__sprt_string.h>
#include <sprt/c/__sprt_stdio.h>
#include <sprt/runtime/log.h>

#include "unistd.h"
#include <sys/stat.h>
#include <sys/statvfs.h>

#if SPRT_LINUX && SPRT_ANDROID
#include <sys/sendfile.h>
#endif

namespace sprt {

#if __STDC_HOSTED__ == 1

#if __DARWIN_64_BIT_INO_T
#define stat64 stat
#define fstat64 fstat
#define lstat64 lstat
#define fstatat64 fstatat
#endif

#if SPRT_NUTTX
// NuttX has no LFS stat64/fstat64/lstat64 — they are plain `stat`. Redefine
// so the convertStatFromNative + the wrappers below keep compiling.
#define stat64 stat
#define fstat64 fstat
#define lstat64 lstat
#define fstatat64 fstatat
#endif

static ::dev_t convertDevToNative(__SPRT_ID(dev_t) dev) { return dev; }

static ::mode_t convertModeToRuntime(__SPRT_ID(mode_t) mode) { return mode; }

static auto convertModeToNative(__SPRT_ID(mode_t) mode) { return mode; }

static void convertStatFromNative(const struct stat64 *native, struct __SPRT_STAT_NAME *rt) {
	rt->st_dev = native->st_dev;
	rt->st_ino = native->st_ino;
	rt->st_nlink = native->st_nlink;
	rt->st_mode = native->st_mode;
	rt->st_uid = native->st_uid;
	rt->st_gid = native->st_gid;
	rt->st_rdev = native->st_rdev;
	rt->st_size = native->st_size;
	rt->st_blksize = native->st_blksize;
	rt->st_blocks = native->st_blocks;
#if SPRT_APPLE
	rt->st_atim.tv_nsec = native->st_atimespec.tv_nsec;
	rt->st_atim.tv_sec = native->st_atimespec.tv_sec;
	rt->st_mtim.tv_nsec = native->st_mtimespec.tv_nsec;
	rt->st_mtim.tv_sec = native->st_mtimespec.tv_sec;
	rt->st_ctim.tv_nsec = native->st_ctimespec.tv_nsec;
	rt->st_ctim.tv_sec = native->st_ctimespec.tv_sec;
#else
	rt->st_atim.tv_nsec = native->st_atim.tv_nsec;
	rt->st_atim.tv_sec = native->st_atim.tv_sec;
	rt->st_mtim.tv_nsec = native->st_mtim.tv_nsec;
	rt->st_mtim.tv_sec = native->st_mtim.tv_sec;
	rt->st_ctim.tv_nsec = native->st_ctim.tv_nsec;
	rt->st_ctim.tv_sec = native->st_ctim.tv_sec;
#endif
}
#else

static auto convertModeToNative(__SPRT_ID(mode_t) mode) { return mode; }

#endif

__SPRT_C_FUNC int __SPRT_ID(
		stat)(const char *__SPRT_RESTRICT path, struct __SPRT_STAT_NAME *__SPRT_RESTRICT __stat) {
#if __STDC_HOSTED__ == 0
	return stat(path, __stat);
#else
	struct stat64 native;
	auto ret = ::stat64(path, &native);
	if (ret == 0) {
		convertStatFromNative(&native, __stat);
	}
	return ret;
#endif
}

__SPRT_C_FUNC int __SPRT_ID(fstat)(int __fd, struct __SPRT_STAT_NAME *__stat) {
#if __STDC_HOSTED__ == 0
	return fstat(__fd, __stat);
#else
	struct stat64 native;
	auto ret = ::fstat64(__fd, &native);
	if (ret == 0) {
		convertStatFromNative(&native, __stat);
	}
	return ret;
#endif
}

__SPRT_C_FUNC int __SPRT_ID(
		lstat)(const char *__SPRT_RESTRICT path, struct __SPRT_STAT_NAME *__SPRT_RESTRICT __stat) {
#if __STDC_HOSTED__ == 0
	return lstat(path, __stat);
#else
	struct stat64 native;
	auto ret = ::lstat64(path, &native);
	if (ret == 0) {
		convertStatFromNative(&native, __stat);
	}
	return ret;
#endif
}

__SPRT_C_FUNC int __SPRT_ID(fstatat)(int __fd, const char *__SPRT_RESTRICT path,
		struct __SPRT_STAT_NAME *__SPRT_RESTRICT __stat, int flags) {
#if __STDC_HOSTED__ == 0
	return fstatat(__fd, path, __stat, flags);
#else
	struct stat64 native;
	auto ret = ::fstatat64(__fd, path, &native, flags);
	if (ret == 0) {
		convertStatFromNative(&native, __stat);
	}
	return ret;
#endif
}

__SPRT_C_FUNC int __SPRT_ID(chmod)(const char *path, __SPRT_ID(mode_t) mode) {
	return chmod(path, convertModeToNative(mode));
}

__SPRT_C_FUNC int __SPRT_ID(fchmod)(int fd, __SPRT_ID(mode_t) mode) {
	return fchmod(fd, convertModeToNative(mode));
}

__SPRT_C_FUNC int __SPRT_ID(fchmodat)(int fd, const char *path, __SPRT_ID(mode_t) mode, int flags) {
	return fchmodat(fd, path, convertModeToNative(mode), flags);
}

__SPRT_C_FUNC __SPRT_ID(mode_t) __SPRT_ID(umask)(__SPRT_ID(mode_t) mode) {
#if __STDC_HOSTED__ == 0
	return umask(mode);
#else
	return convertModeToRuntime(::umask(convertModeToNative(mode)));
#endif
}

__SPRT_C_FUNC int __SPRT_ID(mkdir)(const char *path, __SPRT_ID(mode_t) mode) {
	return mkdir(path, convertModeToNative(mode));
}

__SPRT_C_FUNC int __SPRT_ID(mkdirat)(int fd, const char *path, __SPRT_ID(mode_t) mode) {
	return mkdirat(fd, path, convertModeToNative(mode));
}

__SPRT_C_FUNC int __SPRT_ID(mkfifo)(const char *path, __SPRT_ID(mode_t) mode) {
#if __SPRT_CONFIG_HAVE_STAT_MKFIFO
	return ::mkfifo(path, convertModeToNative(mode));
#else
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_STAT_MKFIFO)");
	__sprt_errno = ENOSYS;
	return -1;
#endif
}

__SPRT_C_FUNC int __SPRT_ID(mkfifoat)(int fd, const char *path, __SPRT_ID(mode_t) mode) {
#if __SPRT_CONFIG_HAVE_STAT_MKFIFO
	return ::mkfifoat(fd, path, convertModeToNative(mode));
#else
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_STAT_MKFIFO)");
	__sprt_errno = ENOSYS;
	return -1;
#endif
}

__SPRT_C_FUNC int __SPRT_ID(mknod)(const char *path, __SPRT_ID(mode_t) mode, __SPRT_ID(dev_t) dev) {
#if __SPRT_CONFIG_HAVE_STAT_MKNOD
	return ::mknod(path, convertModeToNative(mode), convertDevToNative(dev));
#else
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_STAT_MKNOD)");
	__sprt_errno = ENOSYS;
	return -1;
#endif
}

__SPRT_C_FUNC int __SPRT_ID(
		mknodat)(int fd, const char *path, __SPRT_ID(mode_t) mode, __SPRT_ID(dev_t) dev) {
#if __SPRT_CONFIG_HAVE_STAT_MKNOD
	return ::mknodat(fd, path, convertModeToNative(mode), convertDevToNative(dev));
#else
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_STAT_MKNOD)");
	__sprt_errno = ENOSYS;
	return -1;
#endif
}

__SPRT_C_FUNC int __SPRT_ID(futimens)(int fd, const __SPRT_TIMESPEC_NAME ts[2]) {
#if __STDC_HOSTED__ == 0
	return futimens(fd, ts);
#else
	struct timespec nativeTs[2];
	if (ts) {
		nativeTs[0].tv_sec = ts[0].tv_sec;
		nativeTs[0].tv_nsec = ts[0].tv_nsec;
		nativeTs[1].tv_sec = ts[1].tv_sec;
		nativeTs[1].tv_nsec = ts[1].tv_nsec;
	}

	return ::futimens(fd, ts ? nativeTs : nullptr);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(
		utimensat)(int fd, const char *path, const __SPRT_TIMESPEC_NAME ts[2], int flags) {
#if __STDC_HOSTED__ == 0
	return utimensat(fd, path, ts, flags);
#else
	struct timespec nativeTs[2];
	if (ts) {
		nativeTs[0].tv_sec = ts[0].tv_sec;
		nativeTs[0].tv_nsec = ts[0].tv_nsec;
		nativeTs[1].tv_sec = ts[1].tv_sec;
		nativeTs[1].tv_nsec = ts[1].tv_nsec;
	}

	return ::utimensat(fd, path, ts ? nativeTs : nullptr, flags);
#endif
}

#if __STDC_HOSTED__ == 1
static void convertStatvfsFromNative(const struct ::statvfs *native,
		struct __SPRT_STATVFS_NAME *rt) {
	rt->f_bsize = native->f_bsize;
	rt->f_frsize = native->f_frsize;
	rt->f_blocks = native->f_blocks;
	rt->f_bfree = native->f_bfree;
	rt->f_bavail = native->f_bavail;
	rt->f_files = native->f_files;
	rt->f_ffree = native->f_ffree;
	rt->f_favail = native->f_favail;
	rt->f_fsid = native->f_fsid;
	rt->f_flag = native->f_flag;
	rt->f_namemax = native->f_namemax;
}
#endif

__SPRT_C_FUNC int __SPRT_ID(statvfs)(const char *__SPRT_RESTRICT path,
		struct __SPRT_STATVFS_NAME *__SPRT_RESTRICT buf) {
#if __STDC_HOSTED__ == 0
	// Freestanding backend: WinAPI on windows, a zeroed no-op on wasm (libc_impl/builtin_stat).
	return statvfs(path, buf);
#else
	struct ::statvfs native;
	auto ret = ::statvfs(path, &native);
	if (ret == 0) {
		convertStatvfsFromNative(&native, buf);
	}
	return ret;
#endif
}

__SPRT_C_FUNC int __SPRT_ID(fstatvfs)(int fd, struct __SPRT_STATVFS_NAME *buf) {
#if __STDC_HOSTED__ == 0
	// Freestanding backend: WinAPI on windows, a zeroed no-op on wasm (libc_impl/builtin_stat).
	return fstatvfs(fd, buf);
#else
	struct ::statvfs native;
	auto ret = ::fstatvfs(fd, &native);
	if (ret == 0) {
		convertStatvfsFromNative(&native, buf);
	}
	return ret;
#endif
}

__SPRT_C_FUNC __SPRT_ID(ssize_t) __SPRT_ID(
		sendfile)(int out_fd, int in_fd, __SPRT_ID(off_t) * offs, __SPRT_ID(size_t) noffs) {
#if __STDC_HOSTED__ == 0
	__sprt_errno = EINVAL;
	return -1;
#else
#if SPRT_LINUX && SPRT_ANDROID
	return ::sendfile64(out_fd, in_fd, offs, noffs);
#else
	__sprt_errno = EINVAL;
	return -1;
#endif
#endif
}

} // namespace sprt
