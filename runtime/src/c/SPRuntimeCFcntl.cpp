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

#include <c/__sprt_fcntl.h>
#include <c/__sprt_string.h>
#include <c/__sprt_stdio.h>
#include <c/__sprt_errno.h>
#include <c/__sprt_stdarg.h>

#include "SPRuntimeLog.h"
#include "private/SPRTFilename.h"
#include "private/SPRTPrivate.h"

#include <fcntl.h>

namespace sprt {

__SPRT_C_FUNC int __SPRT_ID(fcntl)(int __fd, int __cmd, ...) {
	unsigned long arg;
	__sprt_va_list ap;
	__sprt_va_start(ap, __cmd);
	arg = __sprt_va_arg(ap, unsigned long);
	__sprt_va_end(ap);

	return ::fcntl(__fd, __cmd, arg);
}

__SPRT_C_FUNC int __SPRT_ID(creat)(const char *path, __SPRT_ID(mode_t) __mode) {
	return internal::performWithNativePath(path, [&](const char *target) {
		// call with native path
		return ::creat64(target, __mode);
	}, -1);
}

__SPRT_C_FUNC int __SPRT_ID(open)(const char *path, int __flags, ...) {
	__SPRT_ID(mode_t) __mode = 0;

	if ((__flags & __SPRT_O_CREAT) || (__flags & __SPRT_O_TMPFILE) == __SPRT_O_TMPFILE) {
		__sprt_va_list ap;
		__sprt_va_start(ap, __flags);
		__mode = __sprt_va_arg(ap, __SPRT_ID(mode_t));
		__sprt_va_end(ap);
	}

	return internal::performWithNativePath(path, [&](const char *target) {
		// call with native path
		return ::open64(target, __mode);
	}, -1);
}

__SPRT_C_FUNC int __SPRT_ID(openat)(int __dir_fd, const char *path, int __flags, ...) {
	__SPRT_ID(mode_t) __mode = 0;

	if ((__flags & __SPRT_O_CREAT) || (__flags & __SPRT_O_TMPFILE) == __SPRT_O_TMPFILE) {
		__sprt_va_list ap;
		__sprt_va_start(ap, __flags);
		__mode = __sprt_va_arg(ap, __SPRT_ID(mode_t));
		__sprt_va_end(ap);
	}

	return internal::performWithNativePath(path, [&](const char *target) {
		// call with native path
		return ::openat64(__dir_fd, target, __mode);
	}, -1);
}

__SPRT_C_FUNC __SPRT_ID(ssize_t)
		__SPRT_ID(splice)(int __in_fd, __SPRT_ID(off_t) * __in_offset, int __out_fd,
				__SPRT_ID(off_t) * __out_offset, __SPRT_ID(size_t) __length, unsigned int __flags) {
	return ::splice(__in_fd, __in_offset, __out_fd, __out_offset, __length, __flags);
}

__SPRT_C_FUNC __SPRT_ID(ssize_t) __SPRT_ID(
		tee)(int __in_fd, int __out_fd, __SPRT_ID(size_t) __length, unsigned int __flags) {
	return ::tee(__in_fd, __out_fd, __length, __flags);
}

__SPRT_C_FUNC int __SPRT_ID(
		fallocate)(int __fd, int __mode, __SPRT_ID(off_t) __offset, __SPRT_ID(off_t) __length) {
	return ::fallocate64(__fd, __mode, __offset, __length);
}

__SPRT_C_FUNC int __SPRT_ID(posix_fadvise)(int __fd, __SPRT_ID(off_t) __offset,
		__SPRT_ID(off_t) __length, int __advice) {
	return ::posix_fadvise64(__fd, __offset, __length, __advice);
}

__SPRT_C_FUNC int __SPRT_ID(
		posix_fallocate)(int __fd, __SPRT_ID(off_t) __offset, __SPRT_ID(off_t) __length) {
	return ::posix_fallocate64(__fd, __offset, __length);
}

__SPRT_C_FUNC __SPRT_ID(ssize_t)
		__SPRT_ID(readahead)(int __fd, __SPRT_ID(off_t) __offset, __SPRT_ID(size_t) __length) {
	return ::readahead(__fd, __offset, __length);
}

__SPRT_C_FUNC int __SPRT_ID(sync_file_range)(int __fd, __SPRT_ID(off_t) __offset,
		__SPRT_ID(off_t) __length, unsigned int __flags) {
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
}

} // namespace sprt
