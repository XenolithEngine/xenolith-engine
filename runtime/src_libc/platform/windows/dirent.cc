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
#include <fcntl.h>
#include <corecrt_io.h>

#include "private/SPRTFilename.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "private/SPRTSpecific.h"

struct __dirstream {
	static constexpr size_t DefaultSize = 4'096;

	struct __SPRT_DIRENT_NAME dent;
	char extraNameBuffer[512];

	size_t size = 0;
	int fd = -1;
	HANDLE handle = nullptr;

	bool noMoreFiles = false;

	FILE_ID_BOTH_DIR_INFO *currentInfo = nullptr;

	// uint64_t for alignment
	sprt::uint64_t extraSpaceSize = 0;
	char *extraSpace[0];
};

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#endif

namespace sprt {

static bool __isdir(HANDLE handle) {
	// check if handle is directory
	FILE_STANDARD_INFO standardInfo;
	if (!GetFileInformationByHandleEx(handle, FileStandardInfo, &standardInfo,
				sizeof(FILE_STANDARD_INFO))) {
		return false;
	}

	if (!standardInfo.Directory) {
		return false;
	}
	return true;
}

static __dirstream *__wopendir(const char *path) {
	HANDLE h =
			CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
					NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
	if (h == INVALID_HANDLE_VALUE) {
		__sprt_errno = platform::lastErrorToErrno(GetLastError());
		return nullptr;
	}

	if (!__isdir(h)) {
		__sprt_errno = ENOTDIR;
		return nullptr;
	}

	auto ds = new (malloc(__dirstream::DefaultSize), nothrow) __dirstream;
	if (!ds) {
		__sprt_errno = ENOMEM;
		return nullptr;
	}
	ds->size = __dirstream::DefaultSize;
	ds->handle = h;
	ds->extraSpaceSize = ds->size - offsetof(__dirstream, extraSpace);

	return ds;
}

static __dirstream *opendir(const char *path) {
	return internal::performWithNativePath(path, [&](const char *target) {
		return __wopendir(target); //
	}, (__dirstream *)nullptr);
}

static __dirstream *fdopendir(int __dir_fd) {
	auto h = HANDLE(_get_osfhandle(__dir_fd));
	if (!h || h == INVALID_HANDLE_VALUE) {
		*__sprt___errno_location() = EBADF;
		return nullptr;
	}

	if (!__isdir(h)) {
		__sprt_errno = ENOTDIR;
		return nullptr;
	}

	auto ds = new (malloc(__dirstream::DefaultSize), nothrow) __dirstream;
	if (!ds) {
		__sprt_errno = ENOMEM;
		return nullptr;
	}
	ds->size = __dirstream::DefaultSize;
	ds->fd = __dir_fd;
	ds->handle = h;
	ds->dent.d_off = 0;
	ds->extraSpaceSize = ds->size - offsetof(__dirstream, extraSpace);
	return ds;
}

static struct __SPRT_DIRENT_NAME *readdir64(__dirstream *__dir, off_t target = 0) {
	if (!__dir || !__dir->handle || __dir->handle == INVALID_HANDLE_VALUE) {
		*__sprt___errno_location() = EINVAL;
		return nullptr;
	}

	if (__dir->noMoreFiles) {
		// signal to end
		return nullptr;
	}

	if (!__dir->currentInfo) {
		if (GetFileInformationByHandleEx(__dir->handle, FileIdBothDirectoryInfo, __dir->extraSpace,
					__dir->extraSpaceSize)) {
			__dir->currentInfo = (FILE_ID_BOTH_DIR_INFO *)__dir->extraSpace;
		} else {
			__dir->noMoreFiles = true;
			return nullptr;
		}
	}

	if (__dir->currentInfo) {
		__dir->dent.d_ino = __dir->currentInfo->FileId.QuadPart;
		__dir->dent.d_off = __dir->currentInfo->FileIndex;

		if (target == 0 || __dir->dent.d_off == target) {
			size_t len = 0;
			unicode::toUtf8(__dir->dent.d_name, 256 + 512,
					WideStringView((char16_t *)__dir->currentInfo->FileName,
							__dir->currentInfo->FileNameLength),
					&len);
			__dir->dent.d_name[len] = 0;
			__dir->dent.d_reclen = sizeof(struct __SPRT_DIRENT_NAME) + len - 256;

			if (__dir->currentInfo->FileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
				__dir->dent.d_type = __SPRT_DT_DIR;
			} else if (__dir->currentInfo->FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
				__dir->dent.d_type = __SPRT_DT_LNK;
			} else {
				__dir->dent.d_type = __SPRT_DT_REG;
			}
		}

		if (__dir->currentInfo->NextEntryOffset) {
			__dir->currentInfo =
					(FILE_ID_BOTH_DIR_INFO *)__dir->extraSpace[__dir->currentInfo->NextEntryOffset];
		} else {
			__dir->currentInfo = nullptr;
		}
	}

	return &__dir->dent;
}

static int closedir(__dirstream *__dir) {
	if (!__dir || !__dir->handle || __dir->handle == INVALID_HANDLE_VALUE) {
		*__sprt___errno_location() = EINVAL;
		return -1;
	}

	if (__dir->fd >= 0) {
		_close(__dir->fd);
	} else if (__dir->handle) {
		CloseHandle(__dir->handle);
	}

	__dir->fd = -1;
	__dir->handle = nullptr;

	free(__dir);
	return 0;
}

static int rewinddir(__dirstream *__dir) {
	if (!__dir || !__dir->handle || __dir->handle == INVALID_HANDLE_VALUE) {
		*__sprt___errno_location() = EINVAL;
		return -1;
	}

	if (!__dir->currentInfo) {
		if (GetFileInformationByHandleEx(__dir->handle, FileIdBothDirectoryRestartInfo,
					__dir->extraSpace, __dir->extraSpaceSize)) {
			__dir->currentInfo = (FILE_ID_BOTH_DIR_INFO *)__dir->extraSpace;
		} else {
			__dir->noMoreFiles = true;
			return -1;
		}
	}

	__dir->dent.d_off = 0;
	__dir->noMoreFiles = false;
	return 0;
}

static int seekdir(__dirstream *__dir, long __location) {
	if (!__dir) {
		*__sprt___errno_location() = EINVAL;
		return -1;
	}

	if (__location >= 0) {
		if (__location == __dir->dent.d_off) {
			return 0;
		}

		if (__location < __dir->dent.d_off) {
			if (__sprt_rewinddir(__dir) != 0) {
				return -1;
			}
		}

		while (__dir->dent.d_off != __location && __dir->handle && !__dir->noMoreFiles) {
			if (!readdir64(__dir, __location)) {
				*__sprt___errno_location() = EINVAL;
				return -1;
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
	return __dir->dent.d_off;
}

static int dirfd(__dirstream *__dir) {
	if (!__dir || !__dir->handle || __dir->handle == INVALID_HANDLE_VALUE) {
		*__sprt___errno_location() = EINVAL;
		return -1;
	}

	if (__dir->fd < 0) {
		__dir->fd = _open_osfhandle(intptr_t(__dir->handle), _O_RDONLY);
	}

	if (__dir->fd >= 0) {
		return __dir->fd;
	}

	*__sprt___errno_location() = EINVAL;
	return -1;
}

static int alphasort(const struct __SPRT_DIRENT_NAME **__lhs,
		const struct __SPRT_DIRENT_NAME **__rhs) {
	return strcoll((*__lhs)->d_name, (*__rhs)->d_name);
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
		auto dir = __wopendir(path);
		if (dir) {
			ret = __scandir(dir, __name_list, __filter, __comparator);
			closedir(dir);
		}
	})) {
		__sprt_errno = EINVAL;
		return -1;
	}
	return ret;
}

} // namespace sprt

#ifdef __clang__
#pragma clang diagnostic pop
#endif
