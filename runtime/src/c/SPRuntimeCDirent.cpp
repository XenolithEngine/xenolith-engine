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

#include <c/__sprt_dirent.h>
#include <c/__sprt_string.h>
#include <c/__sprt_stdio.h>
#include <c/__sprt_errno.h>
#include <c/__sprt_stdarg.h>

#include "SPRuntimeLog.h"
#include "private/SPRTFilename.h"
#include "private/SPRTPrivate.h"

#include <dirent.h>

namespace sprt {

__SPRT_C_FUNC __SPRT_ID(DIR) * __SPRT_ID(opendir)(const char *path) {
	return internal::performWithNativePath(path, [&](const char *target) {
		// call with native path
		return ::opendir(target);
	}, (__SPRT_ID(DIR) *)nullptr);
}

__SPRT_C_FUNC __SPRT_ID(DIR) * __SPRT_ID(fdopendir)(int __dir_fd) { return ::fdopendir(__dir_fd); }

__SPRT_C_FUNC struct __SPRT_DIRENT_NAME *__SPRT_ID(readdir)(__SPRT_ID(DIR) * __dir) {
	return (struct __SPRT_DIRENT_NAME *)::readdir64(__dir);
}


#if SPRT_ANROID || SPRT_LINUX
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif

__SPRT_C_FUNC int __SPRT_ID(readdir_r)(__SPRT_ID(DIR) * __dir, struct __SPRT_DIRENT_NAME *__entry,
		struct __SPRT_DIRENT_NAME **__buffer) {
	return ::readdir64_r(__dir, (struct dirent64 *)__entry, (struct dirent64 **)__buffer);
}

#if SPRT_ANROID || SPRT_LINUX
#pragma clang diagnostic pop
#endif

__SPRT_C_FUNC int __SPRT_ID(closedir)(__SPRT_ID(DIR) * __dir) { return ::closedir(__dir); }

__SPRT_C_FUNC void __SPRT_ID(rewinddir)(__SPRT_ID(DIR) * __dir) { return ::rewinddir(__dir); }

__SPRT_C_FUNC void __SPRT_ID(seekdir)(__SPRT_ID(DIR) * __dir, long __location) {
	return ::seekdir(__dir, __location);
}

__SPRT_C_FUNC long __SPRT_ID(telldir)(__SPRT_ID(DIR) * __dir) { return ::telldir(__dir); }

__SPRT_C_FUNC int __SPRT_ID(dirfd)(__SPRT_ID(DIR) * __dir) { return ::dirfd(__dir); }

__SPRT_C_FUNC int __SPRT_ID(alphasort)(const struct __SPRT_DIRENT_NAME **__lhs,
		const struct __SPRT_DIRENT_NAME **__rhs) {
	return ::alphasort64((const struct dirent64 **)__lhs, (const struct dirent64 **)__rhs);
}

__SPRT_C_FUNC int __SPRT_ID(scandir)(const char *path, struct __SPRT_DIRENT_NAME ***__name_list,
		int (*__filter)(const struct __SPRT_DIRENT_NAME *),
		int (*__comparator)(const struct __SPRT_DIRENT_NAME **,
				const struct __SPRT_DIRENT_NAME **)) {
	return internal::performWithNativePath(path, [&](const char *target) {
		// call with native path
		return ::scandir64(target, (struct dirent64 ***)__name_list,
				reinterpret_cast<int (*)(const struct dirent64 *)>(__filter),
				reinterpret_cast<int (*)(const struct dirent64 **, const struct dirent64 **)>(
						__comparator));
	}, -1);
}

__SPRT_C_FUNC int __SPRT_ID(scandirat)(int __dir_fd, const char *path,
		struct __SPRT_DIRENT_NAME ***__name_list,
		int (*__filter)(const struct __SPRT_DIRENT_NAME *),
		int (*__comparator)(const struct __SPRT_DIRENT_NAME **,
				const struct __SPRT_DIRENT_NAME **)) {
	return internal::performWithNativePath(path, [&](const char *target) {
		// call with native path
		return ::scandirat64(__dir_fd, target, (struct dirent64 ***)__name_list,
				reinterpret_cast<int (*)(const struct dirent64 *)>(__filter),
				reinterpret_cast<int (*)(const struct dirent64 **, const struct dirent64 **)>(
						__comparator));
	}, -1);
}

} // namespace sprt
