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

#include <sprt/c/__sprt_fcntl.h>
#include <sprt/c/__sprt_string.h>
#include <sprt/c/__sprt_stdio.h>
#include <sprt/c/__sprt_errno.h>
#include <sprt/c/__sprt_stdarg.h>

#include <sprt/runtime/log.h>

#include <fcntl.h>

// Bionic forcefully renames *64 versions with linker script, so, it can call non-64 recursively
// To deal with it, we acquire direct pointers for *64 functions from libc
#if SPRT_ANDROID
namespace sprt::platform {

extern int (*_creat64)(const char *_Nonnull __path, mode_t __mode);
extern int (*_open64)(const char *_Nonnull __path, int __flags, ...);
extern int (*_openat64)(int __dir_fd, const char *_Nonnull __path, int __flags, ...);
extern int (*_fallocate64)(int __fd, int __mode, off64_t __offset, off64_t __length);
extern int (*_posix_fadvise64)(int __fd, off64_t __offset, off64_t __length, int __advice);
extern int (*_posix_fallocate64)(int __fd, off64_t __offset, off64_t __length);

} // namespace sprt::platform
#endif

#ifdef SPRT_WINDOWS
#include "platform/windows/fcntl.cc"
#endif

namespace sprt {

__SPRT_C_FUNC int __SPRT_ID(open)(const char *path, int __flags, ...) {
	__SPRT_ID(mode_t) __mode = 0;

	if ((__flags & __SPRT_O_CREAT) || (__flags & __SPRT_O_TMPFILE) == __SPRT_O_TMPFILE) {
		__sprt_va_list ap;
		__sprt_va_start(ap, __flags);
		__mode = __sprt_va_arg(ap, __SPRT_ID(mode_t));
		__sprt_va_end(ap);
	}

#if SPRT_ANDROID
	return platform::_open64(path, __flags, __mode);
#else
	return open64(path, __flags, __mode);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(creat)(const char *path, __SPRT_ID(mode_t) __mode) {
#if SPRT_ANDROID
	return platform::_creat64(path, __mode);
#else
	return creat64(path, __mode);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(openat)(int __dir_fd, const char *path, int __flags, ...) {
	__SPRT_ID(mode_t) __mode = 0;

	if ((__flags & __SPRT_O_CREAT) || (__flags & __SPRT_O_TMPFILE) == __SPRT_O_TMPFILE) {
		__sprt_va_list ap;
		__sprt_va_start(ap, __flags);
		__mode = __sprt_va_arg(ap, __SPRT_ID(mode_t));
		__sprt_va_end(ap);
	}


#if SPRT_ANDROID
	return platform::_openat64(__dir_fd, path, __flags, __mode);
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
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
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
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
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
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
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
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
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
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
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
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
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
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (Android: API not available)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#else
	return ::sync_file_range(__fd, __offset, __length, __flags);
#endif
#else
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
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
