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
#define _FILE_OFFSET_BITS 64

#include <sprt/c/__sprt_fcntl.h>
#include <sprt/c/__sprt_string.h>
#include <sprt/c/__sprt_stdio.h>
#include <sprt/c/__sprt_errno.h>
#include <sprt/c/__sprt_stdarg.h>

#include <sprt/runtime/log.h>

#include <fcntl.h>

static_assert(SEEK_SET == __SPRT_SEEK_SET);
static_assert(SEEK_CUR == __SPRT_SEEK_CUR);
static_assert(SEEK_END == __SPRT_SEEK_END);

static_assert(O_ACCMODE == __SPRT_O_ACCMODE);
static_assert(O_RDONLY == __SPRT_O_RDONLY);
static_assert(O_WRONLY == __SPRT_O_WRONLY);
static_assert(O_RDWR == __SPRT_O_RDWR);

static_assert(O_CREAT == __SPRT_O_CREAT);
static_assert(O_EXCL == __SPRT_O_EXCL);
static_assert(O_NOCTTY == __SPRT_O_NOCTTY);
static_assert(O_TRUNC == __SPRT_O_TRUNC);
static_assert(O_APPEND == __SPRT_O_APPEND);
static_assert(O_NONBLOCK == __SPRT_O_NONBLOCK);
static_assert(O_DSYNC == __SPRT_O_DSYNC);
static_assert(O_SYNC == __SPRT_O_SYNC);
static_assert(O_DIRECTORY == __SPRT_O_DIRECTORY);
static_assert(O_NOFOLLOW == __SPRT_O_NOFOLLOW);
static_assert(O_CLOEXEC == __SPRT_O_CLOEXEC);

#ifdef O_EXEC
static_assert(O_EXEC == __SPRT_O_EXEC);
#endif

#ifdef O_SEARCH
static_assert(O_SEARCH == __SPRT_O_SEARCH);
#endif

static_assert(O_ASYNC == __SPRT_O_ASYNC);
static_assert(O_NDELAY == __SPRT_O_NDELAY);

#ifdef __SPRT_O_RSYNC
static_assert(O_RSYNC == __SPRT_O_RSYNC);
#endif

#ifdef __SPRT_O_DIRECT
static_assert(O_DIRECT == __SPRT_O_DIRECT);
#endif

#ifdef __SPRT_O_LARGEFILE
static_assert(O_LARGEFILE == __SPRT_O_LARGEFILE);
#endif

#ifdef __SPRT_O_NOATIME
static_assert(O_NOATIME == __SPRT_O_NOATIME);
#endif

#ifdef __SPRT_O_PATH
static_assert(O_PATH == __SPRT_O_PATH);
#endif

#ifdef __SPRT_O_TMPFILE
static_assert(O_TMPFILE == __SPRT_O_TMPFILE);
#endif


static_assert(F_DUPFD == __SPRT_F_DUPFD);
static_assert(F_GETFD == __SPRT_F_GETFD);
static_assert(F_SETFD == __SPRT_F_SETFD);
static_assert(F_GETFL == __SPRT_F_GETFL);
static_assert(F_SETFL == __SPRT_F_SETFL);

static_assert(F_SETOWN == __SPRT_F_SETOWN);
static_assert(F_GETOWN == __SPRT_F_GETOWN);

static_assert(F_DUPFD_CLOEXEC == __SPRT_F_DUPFD_CLOEXEC);
static_assert(F_OFD_GETLK == __SPRT_F_OFD_GETLK);
static_assert(F_OFD_SETLK == __SPRT_F_OFD_SETLK);
static_assert(F_OFD_SETLKW == __SPRT_F_OFD_SETLKW);

#ifdef F_SETPIPE_SZ
static_assert(F_SETPIPE_SZ == __SPRT_F_SETPIPE_SZ);
#endif

#ifdef F_GETPIPE_SZ
static_assert(F_GETPIPE_SZ == __SPRT_F_GETPIPE_SZ);
#endif

#ifdef F_ADD_SEALS
static_assert(F_ADD_SEALS == __SPRT_F_ADD_SEALS);
#endif

#ifdef F_GET_SEALS
static_assert(F_GET_SEALS == __SPRT_F_GET_SEALS);
#endif

#ifdef F_SEAL_SEAL
static_assert(F_SEAL_SEAL == __SPRT_F_SEAL_SEAL);
#endif

#ifdef F_SEAL_SHRINK
static_assert(F_SEAL_SHRINK == __SPRT_F_SEAL_SHRINK);
#endif

#ifdef F_SEAL_GROW
static_assert(F_SEAL_GROW == __SPRT_F_SEAL_GROW);
#endif

#ifdef F_SEAL_WRITE
static_assert(F_SEAL_WRITE == __SPRT_F_SEAL_WRITE);
#endif

#ifdef F_SEAL_FUTURE_WRITE
static_assert(F_SEAL_FUTURE_WRITE == __SPRT_F_SEAL_FUTURE_WRITE);
#endif


static_assert(F_RDLCK == __SPRT_F_RDLCK);
static_assert(F_WRLCK == __SPRT_F_WRLCK);
static_assert(F_UNLCK == __SPRT_F_UNLCK);

static_assert(FD_CLOEXEC == __SPRT_FD_CLOEXEC);

#if __SPRT_CONFIG_HAVE_FCNTL_FADVICE
static_assert(POSIX_FADV_NORMAL == __SPRT_POSIX_FADV_NORMAL);
static_assert(POSIX_FADV_RANDOM == __SPRT_POSIX_FADV_RANDOM);
static_assert(POSIX_FADV_SEQUENTIAL == __SPRT_POSIX_FADV_SEQUENTIAL);
static_assert(POSIX_FADV_WILLNEED == __SPRT_POSIX_FADV_WILLNEED);
static_assert(POSIX_FADV_DONTNEED == __SPRT_POSIX_FADV_DONTNEED);
static_assert(POSIX_FADV_NOREUSE == __SPRT_POSIX_FADV_NOREUSE);
#endif

// Bionic forcefully renames *64 versions with linker script, so, it can call non-64 recursively
// To deal with it, we acquire direct pointers for *64 functions from libc
#if SPRT_ANDROID
namespace sprt::platform {

extern int (*_creat64)(const char *__path, mode_t __mode);
extern int (*_open64)(const char *__path, int __flags, ...);
extern int (*_openat64)(int __dir_fd, const char *__path, int __flags, ...);
extern int (*_fallocate64)(int __fd, int __mode, off64_t __offset, off64_t __length);
extern int (*_posix_fadvise64)(int __fd, off64_t __offset, off64_t __length, int __advice);
extern int (*_posix_fallocate64)(int __fd, off64_t __offset, off64_t __length);
extern int (*_sync_file_range)(int __fd, off64_t __offset, off64_t __length, unsigned int __flags);

} // namespace sprt::platform
#endif

#ifdef SPRT_WINDOWS
#include "platform/windows/fcntl.cc"
#endif

namespace sprt {

__SPRT_C_FUNC int __SPRT_ID(open)(const char *path, int __flags, ...) {
	__SPRT_ID(mode_t) __mode = 0;

	if ((__flags & __SPRT_O_CREAT)
#if !defined(SPRT_MACOS)
			|| (__flags & __SPRT_O_TMPFILE) == __SPRT_O_TMPFILE
#endif
	) {
		__sprt_va_list ap;
		__sprt_va_start(ap, __flags);
		__mode = __sprt_va_arg(ap, __SPRT_ID(mode_t));
		__sprt_va_end(ap);
	}

#if SPRT_ANDROID
	return platform::_open64(path, __flags, __mode);
#elif SPRT_MACOS
	return open(path, __flags, __mode);
#else
	return open64(path, __flags, __mode);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(creat)(const char *path, __SPRT_ID(mode_t) __mode) {
#if SPRT_ANDROID
	return platform::_creat64(path, __mode);
#elif SPRT_MACOS
	return creat(path, __mode);
#else
	return creat64(path, __mode);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(openat)(int __dir_fd, const char *path, int __flags, ...) {
	__SPRT_ID(mode_t) __mode = 0;

	if ((__flags & __SPRT_O_CREAT)
#if !defined(SPRT_MACOS)
			|| (__flags & __SPRT_O_TMPFILE) == __SPRT_O_TMPFILE
#endif
	) {
		__sprt_va_list ap;
		__sprt_va_start(ap, __flags);
		__mode = __sprt_va_arg(ap, __SPRT_ID(mode_t));
		__sprt_va_end(ap);
	}


#if SPRT_ANDROID
	return platform::_openat64(__dir_fd, path, __flags, __mode);
#elif SPRT_MACOS
	return openat(__dir_fd, path, __flags, __mode);
#else
	return openat64(__dir_fd, path, __flags, __mode);
#endif
}

__SPRT_C_FUNC __SPRT_ID(ssize_t)
		__SPRT_ID(splice)(int __in_fd, __SPRT_ID(off_t) * __in_offset, int __out_fd,
				__SPRT_ID(off_t) * __out_offset, __SPRT_ID(size_t) __length, unsigned int __flags) {
#if __SPRT_CONFIG_HAVE_FCNTL_SPLICE
	return ::splice(__in_fd, __in_offset, __out_fd, __out_offset, __length, __flags);
#else
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_FCNTL_SPLICE)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#endif
}

__SPRT_C_FUNC __SPRT_ID(ssize_t) __SPRT_ID(
		tee)(int __in_fd, int __out_fd, __SPRT_ID(size_t) __length, unsigned int __flags) {
#if __SPRT_CONFIG_HAVE_FCNTL_TEE
	return ::tee(__in_fd, __out_fd, __length, __flags);
#else
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_FCNTL_TEE)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#endif
}

__SPRT_C_FUNC int __SPRT_ID(
		fallocate)(int __fd, int __mode, __SPRT_ID(off_t) __offset, __SPRT_ID(off_t) __length) {
#if __SPRT_CONFIG_HAVE_FCNTL_FALLOCATE
#if SPRT_ANDROID
	return platform::_fallocate64(__fd, __mode, __offset, __length);
#else
	return ::fallocate64(__fd, __mode, __offset, __length);
#endif
#else
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_FCNTL_FALLOCATE)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#endif
}

__SPRT_C_FUNC int __SPRT_ID(posix_fadvise)(int __fd, __SPRT_ID(off_t) __offset,
		__SPRT_ID(off_t) __length, int __advice) {
#if __SPRT_CONFIG_HAVE_FCNTL_FADVICE
#if SPRT_ANDROID
	return platform::_posix_fadvise64(__fd, __offset, __length, __advice);
#else
	return ::posix_fadvise64(__fd, __offset, __length, __advice);
#endif
#else
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_FCNTL_FADVICE)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#endif
}

__SPRT_C_FUNC int __SPRT_ID(
		posix_fallocate)(int __fd, __SPRT_ID(off_t) __offset, __SPRT_ID(off_t) __length) {
#if __SPRT_CONFIG_HAVE_FCNTL_FALLOCATE
#if SPRT_ANDROID
	return platform::_posix_fallocate64(__fd, __offset, __length);
#else
	return ::posix_fallocate64(__fd, __offset, __length);
#endif
#else
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_FCNTL_FALLOCATE)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#endif
}

__SPRT_C_FUNC __SPRT_ID(ssize_t)
		__SPRT_ID(readahead)(int __fd, __SPRT_ID(off_t) __offset, __SPRT_ID(size_t) __length) {
#if __SPRT_CONFIG_HAVE_FCNTL_READAHEAD
	return ::readahead(__fd, __offset, __length);
#else
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_FCNTL_READAHEAD)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#endif
}

__SPRT_C_FUNC int __SPRT_ID(sync_file_range)(int __fd, __SPRT_ID(off_t) __offset,
		__SPRT_ID(off_t) __length, unsigned int __flags) {
#if __SPRT_CONFIG_HAVE_FCNTL_SYNC_FILE_RANGE
#if SPRT_ANDROID
	if (platform::_sync_file_range) {
		return platform::_sync_file_range(__fd, __offset, __length, __flags);
	}
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (Android: API not available)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#else
	return ::sync_file_range(__fd, __offset, __length, __flags);
#endif
#else
	oslog::vprint(oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_FCNTL_SYNC_FILE_RANGE)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#endif
}

__SPRT_C_FUNC int __SPRT_ID(fcntl)(int __fd, int __cmd, ...) {
	unsigned long arg;
	__sprt_va_list ap;
	__sprt_va_start(ap, __cmd);
	arg = __sprt_va_arg(ap, unsigned long);
	__sprt_va_end(ap);

	return fcntl(__fd, __cmd, arg);
}

} // namespace sprt
