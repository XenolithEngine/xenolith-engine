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

#include "private/SPRTFilename.h"
#include <c/sys/__sprt_stat.h>
#include <c/__sprt_errno.h>
#include <c/__sprt_string.h>
#include <c/__sprt_stdio.h>

#include <sys/stat.h>

namespace sprt {

static ::mode_t convertModeToNative(__SPRT_ID(mode_t) mode) {
#if SPRT_LINUX || SPRT_ANDROID || SPRT_MACOS
	return mode;
#else
#error TODO
#endif
}

static ::dev_t convertDevToNative(__SPRT_ID(dev_t) dev) {
#if SPRT_LINUX || SPRT_ANDROID || SPRT_MACOS
	return dev;
#else
#error TODO
#endif
}

static ::mode_t convertModeToRuntime(__SPRT_ID(mode_t) mode) {
#if SPRT_LINUX || SPRT_ANDROID || SPRT_MACOS
	return mode;
#else
#error TODO
#endif
}

static void convertStatFromNative(const struct stat64 *native, struct __SPRT_STAT_NAME *rt) {
#if SPRT_LINUX || SPRT_ANDROID || SPRT_MACOS
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
	rt->st_atim.tv_nsec = native->st_atim.tv_nsec;
	rt->st_atim.tv_sec = native->st_atim.tv_sec;
	rt->st_mtim.tv_nsec = native->st_mtim.tv_nsec;
	rt->st_mtim.tv_sec = native->st_mtim.tv_sec;
	rt->st_ctim.tv_nsec = native->st_ctim.tv_nsec;
	rt->st_ctim.tv_sec = native->st_ctim.tv_sec;
#else
#error TODO
#endif
}

__SPRT_C_FUNC int __SPRT_ID(
		stat)(const char *__SPRT_RESTRICT path, struct __SPRT_STAT_NAME *__SPRT_RESTRICT __stat) {
	struct stat64 native;

	return internal::performWithNativePath(path, [&](const char *target) {
		// call with native path
		auto ret = ::stat64(target, &native);
		if (ret == 0) {
			convertStatFromNative(&native, __stat);
		}
		return ret;
	}, -1);
}

__SPRT_C_FUNC int __SPRT_ID(fstat)(int __fd, struct __SPRT_STAT_NAME *__stat) {
	struct stat64 native;
	auto ret = ::fstat64(__fd, &native);
	if (ret == 0) {
		convertStatFromNative(&native, __stat);
	}
	return ret;
}
__SPRT_C_FUNC int __SPRT_ID(
		lstat)(const char *__SPRT_RESTRICT path, struct __SPRT_STAT_NAME *__SPRT_RESTRICT __stat) {
	struct stat64 native;

	return internal::performWithNativePath(path, [&](const char *target) {
		// call with native path
		auto ret = ::lstat64(target, &native);
		if (ret == 0) {
			convertStatFromNative(&native, __stat);
		}
		return ret;
	}, -1);
}

__SPRT_C_FUNC int __SPRT_ID(fstatat)(int __fd, const char *__SPRT_RESTRICT path,
		struct __SPRT_STAT_NAME *__SPRT_RESTRICT __stat, int flags) {
	struct stat64 native;

	return internal::performWithNativePath(path, [&](const char *target) {
		// call with native path
		auto ret = ::fstatat64(__fd, target, &native, flags);
		if (ret == 0) {
			convertStatFromNative(&native, __stat);
		}
		return ret;
	}, -1);
}

__SPRT_C_FUNC int __SPRT_ID(chmod)(const char *path, __SPRT_ID(mode_t) mode) {
	return internal::performWithNativePath(path, [&](const char *target) {
		// call with native path
		return ::chmod(target, convertModeToNative(mode));
	}, -1);
}

__SPRT_C_FUNC int __SPRT_ID(fchmod)(int fd, __SPRT_ID(mode_t) mode) {
	return ::fchmod(fd, convertModeToNative(mode));
}

__SPRT_C_FUNC int __SPRT_ID(fchmodat)(int fd, const char *path, __SPRT_ID(mode_t) mode, int flags) {
	return internal::performWithNativePath(path, [&](const char *target) {
		// call with native path
		return ::fchmodat(fd, target, convertModeToNative(mode), flags);
	}, -1);
}

__SPRT_C_FUNC __SPRT_ID(mode_t) __SPRT_ID(umask)(__SPRT_ID(mode_t) mode) {
	return convertModeToRuntime(::umask(convertModeToNative(mode)));
}

__SPRT_C_FUNC int __SPRT_ID(mkdir)(const char *path, __SPRT_ID(mode_t) mode) {
	return internal::performWithNativePath(path, [&](const char *target) {
		// call with native path
		return ::mkdir(target, convertModeToNative(mode));
	}, -1);
}

__SPRT_C_FUNC int __SPRT_ID(mkfifo)(const char *path, __SPRT_ID(mode_t) mode) {
	return internal::performWithNativePath(path, [&](const char *target) {
		// call with native path
		return ::mkfifo(target, convertModeToNative(mode));
	}, -1);
}

__SPRT_C_FUNC int __SPRT_ID(mkdirat)(int fd, const char *path, __SPRT_ID(mode_t) mode) {
	return internal::performWithNativePath(path, [&](const char *target) {
		// call with native path
		return ::mkdirat(fd, target, convertModeToNative(mode));
	}, -1);
}

__SPRT_C_FUNC int __SPRT_ID(mkfifoat)(int fd, const char *path, __SPRT_ID(mode_t) mode) {
	return internal::performWithNativePath(path, [&](const char *target) {
		// call with native path
		return ::mkfifoat(fd, target, convertModeToNative(mode));
	}, -1);
}

__SPRT_C_FUNC int __SPRT_ID(mknod)(const char *path, __SPRT_ID(mode_t) mode, __SPRT_ID(dev_t) dev) {
	return internal::performWithNativePath(path, [&](const char *target) {
		// call with native path
		return ::mknod(target, convertModeToNative(mode), convertDevToNative(dev));
	}, -1);
}

__SPRT_C_FUNC int __SPRT_ID(
		mknodat)(int fd, const char *path, __SPRT_ID(mode_t) mode, __SPRT_ID(dev_t) dev) {
	return internal::performWithNativePath(path, [&](const char *target) {
		// call with native path
		return ::mknodat(fd, target, convertModeToNative(mode), convertDevToNative(dev));
	}, -1);
}

__SPRT_C_FUNC int __SPRT_ID(futimens)(int fd, const __SPRT_TIMESPEC_NAME ts[2]) {
	struct timespec nativeTs[2];
	nativeTs[0].tv_sec = ts[0].tv_sec;
	nativeTs[0].tv_nsec = ts[0].tv_nsec;
	nativeTs[1].tv_sec = ts[1].tv_sec;
	nativeTs[1].tv_nsec = ts[1].tv_nsec;

	return ::futimens(fd, nativeTs);
}

__SPRT_C_FUNC int __SPRT_ID(
		utimensat)(int fd, const char *path, const __SPRT_TIMESPEC_NAME ts[2], int flags) {
	struct timespec nativeTs[2];
	nativeTs[0].tv_sec = ts[0].tv_sec;
	nativeTs[0].tv_nsec = ts[0].tv_nsec;
	nativeTs[1].tv_sec = ts[1].tv_sec;
	nativeTs[1].tv_nsec = ts[1].tv_nsec;

	return internal::performWithNativePath(path, [&](const char *target) {
		// call with native path
		return ::utimensat(fd, target, nativeTs, flags);
	}, -1);
}

} // namespace sprt
