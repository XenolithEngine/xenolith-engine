/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

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

#include "unistd.h"
#include "../include/__impl_libc.h"
#include "dirent.h"
#include "stdlib.h"
#include "string.h"

#if SPRT_WINDOWS
#include "windows/dirent.cc"
#endif

namespace sprt {

__SPRT_C_FUNC int alphasort(const struct __SPRT_DIRENT_NAME **__lhs,
		const struct __SPRT_DIRENT_NAME **__rhs) __SPRT_NOEXCEPT {
	return ::strcoll((*__lhs)->d_name, (*__rhs)->d_name);
}

int __scandir(__SPRT_ID(DIR) * d, struct __SPRT_DIRENT_NAME ***__name_list,
		int (*__filter)(const struct __SPRT_DIRENT_NAME *),
		int (*__comparator)(const struct __SPRT_DIRENT_NAME **,
				const struct __SPRT_DIRENT_NAME **)) {
	// Original code from MUSL (scandir.c)
	struct __SPRT_DIRENT_NAME *de, **names = 0, **tmp;
	size_t cnt = 0, len = 0;
	int old_errno = __sprt_errno;

	if (!d) {
		return -1;
	}

	while ((__sprt_errno = 0), (de = readdir64(d))) {
		if (__filter && !__filter(de)) {
			continue;
		}
		if (cnt >= len) {
			len = 2 * len + 1;
			if (len > Max<size_t> / sizeof *names) {
				break;
			}
			tmp = (struct __SPRT_DIRENT_NAME **)realloc(names, len * sizeof *names);
			if (!tmp) {
				break;
			}
			names = tmp;
		}
		names[cnt] = (struct __SPRT_DIRENT_NAME *)malloc(de->d_reclen);
		if (!names[cnt]) {
			break;
		}
		memcpy(names[cnt++], de, de->d_reclen);
	}

	if (__sprt_errno) {
		if (names) {
			while (cnt-- > 0) { free(names[cnt]); }
		}
		free(names);
		return -1;
	}
	__sprt_errno = old_errno;

	if (__comparator) {
		__sprt_qsort(names, cnt, sizeof *names, (int (*)(const void *, const void *))__comparator);
	}
	*__name_list = names;
	return cnt;
}

__SPRT_C_FUNC int scandir(const char *path, struct __SPRT_DIRENT_NAME ***__name_list,
		int (*__filter)(const struct __SPRT_DIRENT_NAME *),
		int (*__comparator)(const struct __SPRT_DIRENT_NAME **,
				const struct __SPRT_DIRENT_NAME **)) __SPRT_NOEXCEPT {
	auto dir = opendir(path);
	if (!dir) {
		return -1;
	}
	auto result = __scandir(dir, __name_list, __filter, __comparator);
	closedir(dir);
	return result;
}

} // namespace sprt
