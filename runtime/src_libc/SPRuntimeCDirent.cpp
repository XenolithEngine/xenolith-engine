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

namespace sprt {

__SPRT_C_FUNC __SPRT_ID(DIR) * __SPRT_ID(opendir)(const char *path) { return opendir(path); }

__SPRT_C_FUNC __SPRT_ID(DIR) * __SPRT_ID(fdopendir)(int __dir_fd) { return fdopendir(__dir_fd); }

__SPRT_C_FUNC struct __SPRT_DIRENT_NAME *__SPRT_ID(readdir)(__SPRT_ID(DIR) * __dir) {
	return readdir64(__dir);
}

__SPRT_C_FUNC int __SPRT_ID(closedir)(__SPRT_ID(DIR) * __dir) { return closedir(__dir); }

__SPRT_C_FUNC int __SPRT_ID(rewinddir)(__SPRT_ID(DIR) * __dir) {
#ifndef SPRT_WINDOWS
	rewinddir(__dir);
	return 0;
#else
	return rewinddir(__dir);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(seekdir)(__SPRT_ID(DIR) * __dir, long __location) {
#ifndef SPRT_WINDOWS
	::seekdir(__dir, __location);
	return 0;
#else
	return seekdir(__dir, __location);
#endif
}

__SPRT_C_FUNC long __SPRT_ID(telldir)(__SPRT_ID(DIR) * __dir) { return telldir(__dir); }

__SPRT_C_FUNC int __SPRT_ID(dirfd)(__SPRT_ID(DIR) * __dir) { return dirfd(__dir); }

__SPRT_C_FUNC int __SPRT_ID(alphasort)(const struct __SPRT_DIRENT_NAME **__lhs,
		const struct __SPRT_DIRENT_NAME **__rhs) {
#ifndef SPRT_WINDOWS
	return ::alphasort64((const struct dirent64 **)__lhs, (const struct dirent64 **)__rhs);
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
	// call with native path
	return ::scandir64(target, (struct dirent64 ***)__name_list,
			reinterpret_cast<int (*)(const struct dirent64 *)>(__filter),
			reinterpret_cast<int (*)(const struct dirent64 **, const struct dirent64 **)>(
					__comparator));
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
	return ::scandirat64(__dir_fd, target, (struct dirent64 ***)__name_list,
			reinterpret_cast<int (*)(const struct dirent64 *)>(__filter),
			reinterpret_cast<int (*)(const struct dirent64 **, const struct dirent64 **)>(
					__comparator));
#else
	return scandirat(__dir_fd, path, __name_list, __filter, __comparator);
#endif
}

} // namespace sprt
