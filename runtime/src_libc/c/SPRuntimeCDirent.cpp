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

#include <sprt/c/__sprt_dirent.h>
#include <sprt/c/__sprt_string.h>
#include <sprt/c/__sprt_stdio.h>
#include <sprt/c/__sprt_errno.h>
#include <sprt/c/__sprt_stdarg.h>

#include <sprt/runtime/log.h>
#include "private/SPRTFilename.h"
#include "private/SPRTPrivate.h"

#ifndef SPRT_WINDOWS

#include <dirent.h>

#else

#include "platform/windows/dirent.cc"

#endif

#if SPRT_MACOS
#include <sys/syslimits.h>
#include <unistd.h>
#include <fcntl.h>
#endif

static_assert(sizeof(struct dirent) == sizeof(struct __SPRT_DIRENT_NAME));

namespace sprt {

__SPRT_C_FUNC __SPRT_ID(DIR) * __SPRT_ID(opendir)(const char *path) {
	return (__SPRT_ID(DIR) *)opendir(path);
}

__SPRT_C_FUNC __SPRT_ID(DIR) * __SPRT_ID(fdopendir)(int __dir_fd) {
	return (__SPRT_ID(DIR) *)fdopendir(__dir_fd);
}

__SPRT_C_FUNC struct __SPRT_DIRENT_NAME *__SPRT_ID(readdir)(__SPRT_ID(DIR) * __dir) {
#if SPRT_MACOS
	return (struct __SPRT_DIRENT_NAME *)readdir((DIR *)__dir);
#else
	return (struct __SPRT_DIRENT_NAME *)readdir64(__dir);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(closedir)(__SPRT_ID(DIR) * __dir) { return closedir((DIR *)__dir); }

__SPRT_C_FUNC int __SPRT_ID(rewinddir)(__SPRT_ID(DIR) * __dir) {
#ifndef SPRT_WINDOWS
	rewinddir((DIR *)__dir);
	return 0;
#else
	return rewinddir(__dir);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(seekdir)(__SPRT_ID(DIR) * __dir, long __location) {
#ifndef SPRT_WINDOWS
	::seekdir((DIR *)__dir, __location);
	return 0;
#else
	return seekdir(__dir, __location);
#endif
}

__SPRT_C_FUNC long __SPRT_ID(telldir)(__SPRT_ID(DIR) * __dir) { return telldir((DIR *)__dir); }

__SPRT_C_FUNC int __SPRT_ID(dirfd)(__SPRT_ID(DIR) * __dir) { return dirfd((DIR *)__dir); }

__SPRT_C_FUNC int __SPRT_ID(alphasort)(const struct __SPRT_DIRENT_NAME **__lhs,
		const struct __SPRT_DIRENT_NAME **__rhs) {
#ifndef SPRT_WINDOWS
#if SPRT_MACOS
	return ::alphasort((const struct dirent **)__lhs, (const struct dirent **)__rhs);
#else
	return ::alphasort64((const struct dirent64 **)__lhs, (const struct dirent64 **)__rhs);
#endif
#else
	return alphasort(__lhs, __rhs);
#endif
}

SPRT_API int __SPRT_ID(versionsort)(const struct __SPRT_DIRENT_NAME **__lhs,
		const struct __SPRT_DIRENT_NAME **__rhs) {
	return __sprt_strverscmp((*__lhs)->d_name, (*__rhs)->d_name);
}

__SPRT_C_FUNC int __SPRT_ID(scandir)(const char *path, struct __SPRT_DIRENT_NAME ***__name_list,
		int (*__filter)(const struct __SPRT_DIRENT_NAME *),
		int (*__comparator)(const struct __SPRT_DIRENT_NAME **,
				const struct __SPRT_DIRENT_NAME **)) {
#ifndef SPRT_WINDOWS
#if SPRT_MACOS
	return ::scandir(path, (struct dirent ***)__name_list,
			reinterpret_cast<int (*)(const struct dirent *)>(__filter),
			reinterpret_cast<int (*)(const struct dirent **, const struct dirent **)>(
					__comparator));
#else
	// call with native path
	return ::scandir64(path, (struct dirent64 ***)__name_list,
			reinterpret_cast<int (*)(const struct dirent64 *)>(__filter),
			reinterpret_cast<int (*)(const struct dirent64 **, const struct dirent64 **)>(
					__comparator));
#endif
#else
	return scandir(path, __name_list, __filter, __comparator);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(scandirat)(int __dir_fd, const char *path,
		struct __SPRT_DIRENT_NAME ***__name_list,
		int (*__filter)(const struct __SPRT_DIRENT_NAME *),
		int (*__comparator)(const struct __SPRT_DIRENT_NAME **,
				const struct __SPRT_DIRENT_NAME **)) {
#ifndef SPRT_WINDOWS
#if SPRT_MACOS
	if (path[0] == '/') {
		return ::scandir(path, (struct dirent ***)__name_list,
				reinterpret_cast<int (*)(const struct dirent *)>(__filter),
				reinterpret_cast<int (*)(const struct dirent **, const struct dirent **)>(
						__comparator));
	}

	char buffer[PATH_MAX] = {0};
	if (__dir_fd == AT_FDCWD) {
		if (getcwd(buffer, PATH_MAX) != 0) {
			*__sprt___errno_location() = EBADF;
			return -1;
		}
	} else {
		if (fcntl(__dir_fd, F_GETPATH, buffer) != 0 || buffer[0] == 0) {
			*__sprt___errno_location() = EBADF;
			return -1;
		}
	}

	auto len = __sprt_strlen(buffer);
	auto target = &buffer[len];
	auto remains = PATH_MAX - len;

	target = strappend(target, &remains, "/", 1);
	target = strappend(target, &remains, path, __sprt_strlen(path));

	if (remains > 0) {
		return ::scandir(buffer, (struct dirent ***)__name_list,
				reinterpret_cast<int (*)(const struct dirent *)>(__filter),
				reinterpret_cast<int (*)(const struct dirent **, const struct dirent **)>(
						__comparator));
	}

	*__sprt___errno_location() = EFAULT;
	return -1;
#else
	return ::scandirat64(__dir_fd, path, (struct dirent64 ***)__name_list,
			reinterpret_cast<int (*)(const struct dirent64 *)>(__filter),
			reinterpret_cast<int (*)(const struct dirent64 **, const struct dirent64 **)>(
					__comparator));
#endif
#else
	return scandirat(__dir_fd, path, __name_list, __filter, __comparator);
#endif
}

} // namespace sprt
