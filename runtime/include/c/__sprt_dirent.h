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

#ifndef CORE_RUNTIME_INCLUDE_C___SPRT_DIRENT_H_
#define CORE_RUNTIME_INCLUDE_C___SPRT_DIRENT_H_

#include <c/bits/__sprt_ssize_t.h>
#include <c/cross/__sprt_fstypes.h>
#include <c/cross/__sprt_dir_ptr.h>

#define __SPRT_DT_UNKNOWN 0
#define __SPRT_DT_FIFO 1
#define __SPRT_DT_CHR 2
#define __SPRT_DT_DIR 4
#define __SPRT_DT_BLK 6
#define __SPRT_DT_REG 8
#define __SPRT_DT_LNK 10
#define __SPRT_DT_SOCK 12
#define __SPRT_DT_WHT 14

#ifndef d_fileno
#define d_fileno d_ino
#endif

__SPRT_BEGIN_DECL

#ifdef __SPRT_BUILD
#define __SPRT_DIRENT_NAME __SPRT_ID(dirent)
#else
#define __SPRT_DIRENT_NAME dirent
#endif

struct __SPRT_DIRENT_NAME;


// should match dirent64 for target systems
struct __SPRT_DIRENT_NAME {
	__SPRT_ID(ino_t) d_ino;
	__SPRT_ID(off_t) d_off;
	unsigned short d_reclen;
	unsigned char d_type;
	char d_name[256];
};

SPRT_API __SPRT_ID(DIR) * __SPRT_ID(opendir)(const char *__path);
SPRT_API __SPRT_ID(DIR) * __SPRT_ID(fdopendir)(int __dir_fd);

SPRT_API struct __SPRT_DIRENT_NAME *__SPRT_ID(readdir)(__SPRT_ID(DIR) * __dir);
SPRT_API struct __SPRT_DIRENT_NAME *__SPRT_ID(readdir64)(__SPRT_ID(DIR) * __dir);

SPRT_API int __SPRT_ID(readdir_r)(__SPRT_ID(DIR) * __dir, struct __SPRT_DIRENT_NAME *__entry,
		struct __SPRT_DIRENT_NAME **__buffer);
SPRT_API int __SPRT_ID(readdir64_r)(__SPRT_ID(DIR) * __dir, struct __SPRT_DIRENT_NAME *__entry,
		struct __SPRT_DIRENT_NAME **__buffer);

SPRT_API int __SPRT_ID(closedir)(__SPRT_ID(DIR) * __dir);
SPRT_API void __SPRT_ID(rewinddir)(__SPRT_ID(DIR) * __dir);

SPRT_API void __SPRT_ID(seekdir)(__SPRT_ID(DIR) * __dir, long __location);
SPRT_API long __SPRT_ID(telldir)(__SPRT_ID(DIR) * __dir);

SPRT_API int __SPRT_ID(dirfd)(__SPRT_ID(DIR) * __dir);
SPRT_API int __SPRT_ID(alphasort)(const struct __SPRT_DIRENT_NAME **__lhs,
		const struct __SPRT_DIRENT_NAME **__rhs);

SPRT_API int __SPRT_ID(scandir)(const char *__path, struct __SPRT_DIRENT_NAME ***__name_list,
		int (*__filter)(const struct __SPRT_DIRENT_NAME *),
		int (*__comparator)(const struct __SPRT_DIRENT_NAME **,
				const struct __SPRT_DIRENT_NAME **));

SPRT_API int __SPRT_ID(scandirat)(int __dir_fd, const char *__path,
		struct __SPRT_DIRENT_NAME ***__name_list,
		int (*__filter)(const struct __SPRT_DIRENT_NAME *),
		int (*__comparator)(const struct __SPRT_DIRENT_NAME **,
				const struct __SPRT_DIRENT_NAME **));

__SPRT_END_DECL

#endif // CORE_RUNTIME_INCLUDE_C___SPRT_DIRENT_H_
