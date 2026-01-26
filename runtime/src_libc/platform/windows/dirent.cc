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

#ifndef __SPRT_BUILD
#define __SPRT_BUILD
#endif

#include <sprt/c/__sprt_dirent.h>
#include <sprt/c/__sprt_errno.h>
#include <sprt/c/__sprt_stdlib.h>
#include <sprt/c/__sprt_stdio.h>

#include <sprt/runtime/string.h>

#include <stdlib.h>

#include "private/SPRTFilename.h"

#define WIN32_LEAN_AND_MEAN

#include <Windows.h>

#include "private/SPRTSpecific.h"

struct __dirstream {
	int fd = -1;
	HANDLE handle = nullptr;
	WIN32_FIND_DATAA ffd;
	struct __SPRT_DIRENT_NAME dent;
	bool noMoreFiles = false;
	long currentIndex = 0;
	size_t pathLen = 0;
	const char path[0];
};

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#endif

namespace sprt {

static __dirstream *__opendir(const char *path, size_t pathLen, int fd) {
	auto ds = (__dirstream *)malloc(sizeof(__dirstream) + pathLen + 1);

	ds->handle = FindFirstFileA(path, &ds->ffd);
	if (ds->handle == INVALID_HANDLE_VALUE) {
		free(ds);
		*__sprt___errno_location() = platform::lastErrorToErrno(GetLastError());
		return nullptr;
	}

	ds->fd = platform::openFdHandle(ds->handle, platform::FdHandle::Data{.ptr = ds},
			platform::FdHandleType::Find, fd);

	if (ds->fd < 0) {
		free(ds);
		*__sprt___errno_location() = ENOMEM;
		return nullptr;
	}

	ds->pathLen = pathLen;
	ds->currentIndex = 0;
	memcpy((void *)ds->path, path, pathLen + 1);

	return ds;
}

static int __scandir(__SPRT_ID(DIR) * d, struct __SPRT_DIRENT_NAME ***__name_list,
		int (*__filter)(const struct __SPRT_DIRENT_NAME *),
		int (*__comparator)(const struct __SPRT_DIRENT_NAME **,
				const struct __SPRT_DIRENT_NAME **)) {
	// Original code from MUSL (scandir.c)
	struct __SPRT_DIRENT_NAME *de, **names = 0, **tmp;
	size_t cnt = 0, len = 0;
	int old_errno = errno;

	if (!d) {
		return -1;
	}

	while ((errno = 0), (de = __sprt_readdir(d))) {
		if (__filter && !__filter(de)) {
			continue;
		}
		if (cnt >= len) {
			len = 2 * len + 1;
			if (len > SIZE_MAX / sizeof *names) {
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

	__sprt_closedir(d);

	if (errno) {
		if (names) {
			while (cnt-- > 0) { free(names[cnt]); }
		}
		free(names);
		return -1;
	}
	errno = old_errno;

	if (__comparator) {
		qsort(names, cnt, sizeof *names, (int (*)(const void *, const void *))__comparator);
	}
	*__name_list = names;
	return cnt;
}

static __dirstream *opendir(const char *path) {
	auto pathlen = path ? __sprt_strlen(path) : 0;
	auto buf = (char *)__sprt_malloca(pathlen + 4);

	memcpy(buf, path, pathlen + 1);

	if (!__sprt_fpath_is_native(path, pathlen)) {
		pathlen = __sprt_fpath_to_native(path, pathlen, buf, pathlen + 1);
	}

	if (pathlen <= 0) {
		__sprt_freea(buf);
		*__sprt___errno_location() = EINVAL;
		return nullptr;
	}

	buf[pathlen] = '\\';
	buf[pathlen + 1] = '*';
	buf[pathlen + 2] = 0;

	auto ds = __opendir(buf, pathlen + 2, -1);

	__sprt_freea(buf);

	return ds;
}

static __dirstream *fdopendir(int __dir_fd) {
	auto h = platform::getFdHandle(__dir_fd);
	if (!h) {
		*__sprt___errno_location() = EINVAL;
		return nullptr;
	}

	if (h->type == platform::FdHandleType::Find) {
		// reopen active FindHandle by resetting it, return already active DIR
		__sprt_rewinddir((struct __dirstream *)h->data.ptr);
		return (struct __dirstream *)h->data.ptr;
	} else if (h->type == platform::FdHandleType::File) {
		// open new find handle by directory path if possible, replace dir handle with find handle,
		BY_HANDLE_FILE_INFORMATION info;
		if (GetFileInformationByHandle(h, &info)) {
			if ((info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
				__sprt_errno = ENOTDIR;
				return nullptr;
			}
		}

		__dirstream *ret = nullptr;

		platform::openAtPath(__dir_fd, nullptr, [&](const char *path, size_t pathLen) {
			ret = __opendir(path, pathLen, __dir_fd);
		}, platform::FdHandleType::Find);

		if (ret) {
			return ret;
		}
	}

	*__sprt___errno_location() = EINVAL;
	return nullptr;
}

static struct __SPRT_DIRENT_NAME *readdir64(__dirstream *__dir) {
	if (!__dir || !__dir->handle || __dir->handle == INVALID_HANDLE_VALUE) {
		*__sprt___errno_location() = EINVAL;
		return nullptr;
	}

	if (__dir->noMoreFiles) {
		// signal to end
		*__sprt___errno_location() = 0;
		return nullptr;
	}
	if (__dir->ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
		__dir->dent.d_type = __SPRT_DT_DIR;
	} else {
		auto fileNameLen = strlen(__dir->ffd.cFileName) + 1;
		size_t bufSize = __dir->pathLen + fileNameLen;
		auto filePathBuf = (char *)__sprt_malloca(bufSize);
		auto target = (char *)filePathBuf;

		// strip * from Find path
		target = strappend(target, &bufSize, __dir->path, __dir->pathLen - 1);
		target = strappend(target, &bufSize, __dir->ffd.cFileName, fileNameLen);

		auto tmpFile = CreateFileA(filePathBuf, GENERIC_READ, 0, nullptr, OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL, nullptr);
		if (tmpFile == INVALID_HANDLE_VALUE) {
			__sprt_freea(filePathBuf);
			*__sprt___errno_location() = platform::lastErrorToErrno(GetLastError());
			return nullptr;
		}

		BY_HANDLE_FILE_INFORMATION info;
		if (GetFileInformationByHandle(tmpFile, &info)) {
			__dir->dent.d_ino =
					uint64_t(info.nFileIndexHigh) << 32LLU | uint64_t(info.nFileIndexLow);
			__dir->dent.d_type = __SPRT_DT_DIR;
		}
		__sprt_freea(filePathBuf);
	}

	__dir->dent.d_off = __dir->currentIndex;
	__dir->dent.d_reclen = sizeof(__dirstream);

	memcpy(__dir->dent.d_name, __dir->ffd.cFileName, min(255, strlen(__dir->ffd.cFileName)));

	if (!FindNextFileA(__dir->handle, &__dir->ffd)) {
		auto err = GetLastError();
		if (err != ERROR_NO_MORE_FILES) {
			*__sprt___errno_location() = platform::lastErrorToErrno(GetLastError());
			return nullptr;
		} else {
			// next call will signal to end iteration;
			__dir->noMoreFiles = true;
		}
	} else {
		++__dir->currentIndex;
	}
	return &__dir->dent;
}

static int closedir(__dirstream *__dir) {
	if (!__dir || !__dir->handle || __dir->handle == INVALID_HANDLE_VALUE) {
		*__sprt___errno_location() = EINVAL;
		return -1;
	}

	platform::closeFdHandle(__dir->fd);
	FindClose(__dir->handle);
	free(__dir);
	return 0;
}

static int rewinddir(__dirstream *__dir) {
	if (!__dir || !__dir->handle || __dir->handle == INVALID_HANDLE_VALUE) {
		*__sprt___errno_location() = EINVAL;
		return -1;
	}

	if (!FindClose(__dir->handle)) {
		*__sprt___errno_location() = platform::lastErrorToErrno(GetLastError());
		return -1;
	}
	__dir->handle = FindFirstFileA(__dir->path, &__dir->ffd);
	if (__dir->handle == INVALID_HANDLE_VALUE) {
		*__sprt___errno_location() = platform::lastErrorToErrno(GetLastError());
		return -1;
	}
	__dir->noMoreFiles = false;
	__dir->currentIndex = 0;
	return 0;
}

static int seekdir(__dirstream *__dir, long __location) {
	if (!__dir) {
		*__sprt___errno_location() = EINVAL;
		return -1;
	}
	if (__location >= 0) {
		if (__location == __dir->currentIndex) {
			return 0;
		}

		if (__location < __dir->currentIndex) {
			if (__sprt_rewinddir(__dir) != 0) {
				return -1;
			}
		}

		while (__dir->currentIndex != __location && __dir->handle && !__dir->noMoreFiles) {
			if (!FindNextFileA(__dir->handle, &__dir->ffd)) {
				*__sprt___errno_location() = platform::lastErrorToErrno(GetLastError());
				return -1;
			} else {
				++__dir->currentIndex;
			}
		}
	}
	*__sprt___errno_location() = EINVAL;
	return -1;
}

static long telldir(__dirstream *__dir) {
	if (!__dir) {
		*__sprt___errno_location() = EINVAL;
		return -1;
	}
	return __dir->currentIndex;
}

static int dirfd(__dirstream *__dir) {
	if (!__dir) {
		*__sprt___errno_location() = EINVAL;
		return -1;
	}
	return __dir->fd;
}

static int alphasort(const struct __SPRT_DIRENT_NAME **__lhs,
		const struct __SPRT_DIRENT_NAME **__rhs) {
	return strcoll((*__lhs)->d_name, (*__rhs)->d_name);
}

static int scandir(const char *path, struct __SPRT_DIRENT_NAME ***__name_list,
		int (*__filter)(const struct __SPRT_DIRENT_NAME *),
		int (*__comparator)(const struct __SPRT_DIRENT_NAME **,
				const struct __SPRT_DIRENT_NAME **)) {
	return __scandir(__sprt_opendir(path), __name_list, __filter, __comparator);
}

static int scandirat(int __dir_fd, const char *path, struct __SPRT_DIRENT_NAME ***__name_list,
		int (*__filter)(const struct __SPRT_DIRENT_NAME *),
		int (*__comparator)(const struct __SPRT_DIRENT_NAME **,
				const struct __SPRT_DIRENT_NAME **)) {
	int ret = -1;
	if (!platform::openAtPath(__dir_fd, path, [&](const char *path, size_t pathLen) {
		ret = __scandir(__opendir(path, pathLen, -1), __name_list, __filter, __comparator);
	}, platform::FdHandleType::Find)) {
		__sprt_errno = EINVAL;
		return -1;
	}
	return ret;
}

} // namespace sprt

#ifdef __clang__
#pragma clang diagnostic pop
#endif
