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

#ifndef CORE_RUNTIME_INCLUDE_LIBC_DIRENT_H_
#define CORE_RUNTIME_INCLUDE_LIBC_DIRENT_H_

#if defined(__SPRT_BUILD) && __STDC_HOSTED__ == 1

#include_next <dirent.h>

#else

#include <sprt/c/__sprt_dirent.h>

#define DT_UNKNOWN __SPRT_DT_UNKNOWN
#define DT_FIFO __SPRT_DT_FIFO
#define DT_CHR __SPRT_DT_CHR
#define DT_DIR __SPRT_DT_DIR
#define DT_BLK __SPRT_DT_BLK
#define DT_REG __SPRT_DT_REG
#define DT_LNK __SPRT_DT_LNK
#define DT_SOCK __SPRT_DT_SOCK
#define DT_WHT __SPRT_DT_WHT

#define dirent64 dirent
#define readdir64 readdir
#define alphasort64 alphasort
#define scandir64 scandir
#define scandirat64 scandirat

typedef __SPRT_ID(ssize_t) ssize_t;
typedef __SPRT_ID(off_t) off_t;
typedef __SPRT_ID(DIR) DIR;

__SPRT_BEGIN_DECL

SPRT_UMBRELLA_FUNC
DIR *opendir(const char *path) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_opendir(path);
}
#endif

SPRT_UMBRELLA_FUNC
DIR *fdopendir(int __dir_fd) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_fdopendir(__dir_fd);
}
#endif

SPRT_UMBRELLA_FUNC
struct dirent *readdir(DIR *__dir) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_readdir(__dir);
}
#endif

SPRT_UMBRELLA_FUNC
int closedir(DIR *__dir) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_closedir(__dir);
}
#endif

SPRT_UMBRELLA_FUNC
int rewinddir(DIR *__dir) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_rewinddir(__dir);
}
#endif

SPRT_UMBRELLA_FUNC
int seekdir(DIR *__dir, long __location) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_seekdir(__dir, __location);
}
#endif

SPRT_UMBRELLA_FUNC
long telldir(DIR *__dir) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_telldir(__dir);
}
#endif

SPRT_UMBRELLA_FUNC
int dirfd(DIR *__dir) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_dirfd(__dir);
}
#endif

SPRT_UMBRELLA_FUNC
int alphasort(const struct dirent **__lhs, const struct dirent **__rhs) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_alphasort(__lhs, __rhs);
}
#endif

SPRT_UMBRELLA_FUNC
int scandir(const char *path, struct dirent ***__name_list, int (*__filter)(const struct dirent *),
		int (*__comparator)(const struct dirent **, const struct dirent **)) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_scandir(path, __name_list, __filter, __comparator);
}
#endif

SPRT_UMBRELLA_FUNC
int scandirat(int __dir_fd, const char *path, struct dirent ***__name_list,
		int (*__filter)(const struct dirent *),
		int (*__comparator)(const struct dirent **, const struct dirent **)) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_scandirat(__dir_fd, path, __name_list, __filter, __comparator);
}
#endif

__SPRT_END_DECL

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_DIRENT_H_
