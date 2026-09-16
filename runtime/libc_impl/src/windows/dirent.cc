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

#include "dirent.h"
#include "unistd.h"
#include "stdlib.h"
#include "../../include/__impl_libc.h"

#include <sprt/c/__sprt_dirent.h>
#include <sprt/c/__sprt_errno.h>
#include <sprt/c/__sprt_stdlib.h>
#include <sprt/c/__sprt_stdio.h>
#include <sprt/c/__sprt_unistd.h>

#include <sprt/runtime/string.h>
#include <sprt/runtime/enum.h>

#include <sprt/wrappers/windows/basic_api.h>
#include <sprt/wrappers/windows/file_api.h>

#include "specific.h"

#define __SPRT_DIR_ROOT_HANDLE HANDLE(1)

struct __dirstream {
	static constexpr size_t DefaultSize = 4'096;
	static constexpr size_t MaxFilenameSize = 256 + 768;

	struct __SPRT_DIRENT_NAME dent;
	char extraNameBuffer[768];

	size_t size = 0;
	int fd = -1;
	HANDLE handle = nullptr;

	bool noMoreFiles = false;

	FILE_ID_BOTH_DIR_INFO *currentInfo = nullptr;

	// uint64_t for alignment
	sprt::uint64_t extraSpaceSize = 0;
	char *extraSpace[0];
};

struct __rootdirinfo {
	DWORD _driveMask = 0;
	DWORD _nDrives = 0;
	char _drives[28] = {0};
};

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#endif

namespace sprt {

static bool __isdir(HANDLE handle) {
	if (handle == __SPRT_DIR_ROOT_HANDLE) {
		return true;
	}

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

static __dirstream *__wopenroot() {
	auto ds = new (malloc(__dirstream::DefaultSize), nothrow) __dirstream;
	if (!ds) {
		__sprt_errno = ENOMEM;
		return nullptr;
	}

	ds->size = __dirstream::DefaultSize;
	ds->handle = __SPRT_DIR_ROOT_HANDLE;
	ds->dent.d_off = 0;
	ds->extraSpaceSize = ds->size - offsetof(__dirstream, extraSpace);

	auto rdi = new (ds->extraSpace, nothrow) __rootdirinfo;
	rdi->_driveMask = GetLogicalDrives();

	rdi->_nDrives = 0;
	for (auto idx : flags(rdi->_driveMask)) {
		rdi->_drives[rdi->_nDrives++] = 'a' + __builtin_ctz(idx); //
	}

	return ds;
}

static __dirstream *__wopendir(const char *path) {
	auto wpath = __MALLOCA_WSTRING(path);
	HANDLE h =
			CreateFileW(wpath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
					NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
	__sprt_freea(wpath);
	if (h == INVALID_HANDLE_VALUE) {
		__sprt_errno = platform::lastErrorToErrno(GetLastError());
		return nullptr;
	}

	if (!__isdir(h)) {
		CloseHandle(h);
		__sprt_errno = ENOTDIR;
		return nullptr;
	}

	auto ds = new (__sprt_malloc(__dirstream::DefaultSize), nothrow) __dirstream;
	if (!ds) {
		CloseHandle(h);
		__sprt_errno = ENOMEM;
		return nullptr;
	}
	ds->size = __dirstream::DefaultSize;
	ds->handle = h;
	ds->dent.d_off = 0;
	ds->extraSpaceSize = ds->size - offsetof(__dirstream, extraSpace);

	return ds;
}

__SPRT_C_FUNC __dirstream *opendir(const char *path) __SPRT_NOEXCEPT {
	if (!path) {
		__sprt_errno = ENOENT;
		return nullptr;
	}

	if (memcmp(path, "/", 2) == 0) { // note memcmp with nullterm here
		return __wopenroot();
	} else {
		return platform::performWithNativePath(path, [&](const char *target) {
			return __wopendir(target); //
		}, (__dirstream *)nullptr);
	}
}

__SPRT_C_FUNC __dirstream *fdopendir(int __dir_fd) __SPRT_NOEXCEPT {
	if (__dir_fd < 0) {
		__sprt_errno = EBADF;
		return nullptr;
	}

	auto slot = __libc::get()->get_fd_slot(__dir_fd);
	if (!slot || !slot->handle || !hasFlag(slot->ops->mask, __fd_ops_mask::opendir)) {
		__sprt_errno = EBADF;
		return nullptr;
	}

	// there can be dir_fd or file_fd
	if (!__isdir(slot->handle)) {
		__sprt_errno = ENOTDIR;
		return nullptr;
	}

	auto ds = new (__sprt_malloc(__dirstream::DefaultSize), nothrow) __dirstream;
	if (!ds) {
		__sprt_errno = ENOMEM;
		return nullptr;
	}
	ds->size = __dirstream::DefaultSize;
	ds->fd = __dir_fd;
	ds->handle = slot->handle;
	ds->dent.d_off = 0;
	ds->extraSpaceSize = ds->size - offsetof(__dirstream, extraSpace);
	return ds;
}

static struct __SPRT_DIRENT_NAME *__readdir64(__dirstream *__dir, off_t target) {
	if (!__dir || !__dir->handle) {
		*__sprt___errno_location() = EINVAL;
		return nullptr;
	}

	if (__dir->noMoreFiles) {
		// signal to end
		return nullptr;
	}

	if (__dir->handle == __SPRT_DIR_ROOT_HANDLE) {
		auto rdi = (__rootdirinfo *)__dir->extraSpace;
		if (__dir->dent.d_off >= rdi->_nDrives) {
			__dir->noMoreFiles = true;
			return nullptr;
		}

		__dir->dent.d_name[0] = rdi->_drives[__dir->dent.d_off++];
		__dir->dent.d_name[1] = 0;

		return &__dir->dent;
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
		++__dir->dent.d_off;

		if (target == 0 || __dir->dent.d_off == target) {
			size_t len = 0;
			unicode::toUtf8(__dir->dent.d_name, __dirstream::MaxFilenameSize,
					WideStringView((char16_t *)__dir->currentInfo->FileName,
							__dir->currentInfo->FileNameLength / sizeof(wchar_t)),
					&len);
			__dir->dent.d_name[len] = 0;
			// record length: header up to d_name + name bytes + NUL
			__dir->dent.d_reclen = offsetof(struct __SPRT_DIRENT_NAME, d_name) + len + 1;

			if (__dir->currentInfo->FileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
				__dir->dent.d_type = __SPRT_DT_DIR;
			} else if (__dir->currentInfo->FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
				__dir->dent.d_type = __SPRT_DT_LNK;
			} else {
				__dir->dent.d_type = __SPRT_DT_REG;
			}
		}

		if (__dir->currentInfo->NextEntryOffset) {
			auto source = (char *)__dir->currentInfo;
			__dir->currentInfo =
					(FILE_ID_BOTH_DIR_INFO *)(source + __dir->currentInfo->NextEntryOffset);
		} else {
			__dir->currentInfo = nullptr;
		}
	}

	return &__dir->dent;
}

__SPRT_C_FUNC struct __SPRT_DIRENT_NAME *readdir64(__dirstream *__dir) __SPRT_NOEXCEPT {
	return __readdir64(__dir, 0);
}

__SPRT_C_FUNC int closedir(__dirstream *__dir) __SPRT_NOEXCEPT {
	if (!__dir || !__dir->handle || __dir->handle == INVALID_HANDLE_VALUE) {
		*__sprt___errno_location() = EINVAL;
		return -1;
	}

	if (__dir->fd >= 0) {
		close(__dir->fd);
	} else if (__dir->handle) {
		CloseHandle(__dir->handle);
	}

	__dir->fd = -1;
	__dir->handle = nullptr;

	__sprt_free(__dir);
	return 0;
}

__SPRT_C_FUNC int rewinddir(__dirstream *__dir) __SPRT_NOEXCEPT {
	if (!__dir || !__dir->handle) {
		*__sprt___errno_location() = EINVAL;
		return -1;
	}

	// Restart the underlying enumeration unconditionally. Guarding this on
	// `!currentInfo` (only restart once the listing was exhausted) left a rewind
	// issued mid-enumeration — e.g. the backward path of seekdir() — with d_off
	// reset to 0 but `currentInfo` still pointing at a stale entry, so subsequent
	// readdir()s returned the wrong entries.
	if (__dir->handle && __dir->handle != __SPRT_DIR_ROOT_HANDLE) {
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

__SPRT_C_FUNC int seekdir(__dirstream *__dir, long __location) __SPRT_NOEXCEPT {
	if (!__dir) {
		*__sprt___errno_location() = EINVAL;
		return -1;
	}

	if (__location < 0) {
		*__sprt___errno_location() = EINVAL;
		return -1;
	}

	if (__location == __dir->dent.d_off) {
		return 0;
	}

	if (__location < __dir->dent.d_off) {
		if (rewinddir(__dir) != 0) {
			return -1;
		}
	}

	while (__dir->dent.d_off != __location && __dir->handle && !__dir->noMoreFiles) {
		if (!__readdir64(__dir, __location)) {
			*__sprt___errno_location() = EINVAL;
			return -1;
		}
	}
	return 0;
}

__SPRT_C_FUNC long telldir(__dirstream *__dir) __SPRT_NOEXCEPT {
	if (!__dir) {
		*__sprt___errno_location() = EINVAL;
		return -1;
	}
	return __dir->dent.d_off;
}

__SPRT_C_FUNC int dirfd(__dirstream *__dir) __SPRT_NOEXCEPT {
	if (!__dir || !__dir->handle) {
		*__sprt___errno_location() = EINVAL;
		return -1;
	}

	if (__dir->fd < 0) {
		auto libc = __libc::get();

		int newFd = -1;
		if (__dir->handle == __SPRT_DIR_ROOT_HANDLE) {
			newFd = libc->create_fd(__dir->handle, &libc->fdDirOps, __SPRT_O_RDONLY, 0);
			if (newFd < 0) {
				return -1;
			}
		} else {
			newFd = libc->create_fd(__dir->handle, &libc->fdFileOps, __SPRT_O_RDONLY, 0);
			if (newFd < 0) {
				return -1;
			}
		}
		__dir->fd = newFd;
	}

	if (__dir->fd >= 0) {
		return __dir->fd;
	}

	*__sprt___errno_location() = EINVAL;
	return -1;
}

__SPRT_C_FUNC int scandirat(int __dir_fd, const char *path,
		struct __SPRT_DIRENT_NAME ***__name_list,
		int (*__filter)(const struct __SPRT_DIRENT_NAME *),
		int (*__comparator)(const struct __SPRT_DIRENT_NAME **,
				const struct __SPRT_DIRENT_NAME **)) __SPRT_NOEXCEPT {
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

// ---- MSVC <io.h> find surface -----------------------------------------------
//
// Not built on opendir/readdir above: those enumerate a DIRECTORY, and _wfindfirst
// takes a PATTERN ("C:\\src\\*.c"). FindFirstFileW is the API that matches wildcards,
// and it is what the MSVC CRT calls too, so the results agree entry for entry -
// including the "8.3 short name also matches" behaviour a hand-written globber
// would silently differ on.
//
// The search handle is passed back and forth as a long long, which is what the
// MSVC prototype does (intptr_t); every handle _wfindfirst returns must reach
// _findclose.

// FILETIME is 100ns ticks since 1601-01-01, time_t is seconds since 1970-01-01.
static long long __find_filetime(const FILETIME &ft) {
	constexpr long long ticksPerSecond = 10'000'000ll;
	constexpr long long epochDelta = 11'644'473'600ll; // 1601 -> 1970, in seconds
	auto ticks = (static_cast<long long>(ft.dwHighDateTime) << 32)
			| static_cast<long long>(ft.dwLowDateTime);
	if (ticks == 0) {
		return 0; // the filesystem does not keep this stamp
	}
	return ticks / ticksPerSecond - epochDelta;
}

static void __find_fill(const WIN32_FIND_DATAW &src, struct _wfinddata_t *dst) {
	// _A_RDONLY/_A_HIDDEN/_A_SYSTEM/_A_SUBDIR/_A_ARCH have the same values as the
	// FILE_ATTRIBUTE_* bits they come from, so this is a mask, not a translation.
	dst->attrib = unsigned(src.dwFileAttributes)
			& unsigned(_A_RDONLY | _A_HIDDEN | _A_SYSTEM | _A_SUBDIR | _A_ARCH);
	dst->time_create = __find_filetime(src.ftCreationTime);
	dst->time_access = __find_filetime(src.ftLastAccessTime);
	dst->time_write = __find_filetime(src.ftLastWriteTime);
	dst->size = (static_cast<long long>(src.nFileSizeHigh) << 32)
			| static_cast<long long>(src.nFileSizeLow);

	// cFileName is MAX_PATH wide chars and so is the destination, but copy bounded
	// and terminate by hand rather than trusting that to stay true.
	constexpr size_t cap = sizeof(dst->name) / sizeof(dst->name[0]);
	size_t i = 0;
	while (i + 1 < cap && src.cFileName[i] != 0) {
		dst->name[i] = src.cFileName[i];
		++i;
	}
	dst->name[i] = 0;
}

__SPRT_C_FUNC long long _wfindfirst(const wchar_t *__filespec,
		struct _wfinddata_t *__findinfo) __SPRT_NOEXCEPT {
	if (!__filespec || !__findinfo) {
		__sprt_errno = EINVAL;
		return -1;
	}
	WIN32_FIND_DATAW data;
	auto handle = FindFirstFileW(__filespec, &data);
	if (handle == INVALID_HANDLE_VALUE) {
		DWORD err = GetLastError();
		switch (err) {
		// "nothing matched the pattern" is ENOENT, which is how the MSVC CRT
		// reports it and what callers loop on.
		case ERROR_FILE_NOT_FOUND:
		case ERROR_PATH_NOT_FOUND:
		case ERROR_NO_MORE_FILES: __sprt_errno = ENOENT; break;
		case ERROR_INVALID_NAME: __sprt_errno = EINVAL; break;
		default: __sprt_errno = platform::lastErrorToErrno(err);
		}
		return -1;
	}
	__find_fill(data, __findinfo);
	return static_cast<long long>(reinterpret_cast<__SPRT_ID(intptr_t)>(handle));
}

__SPRT_C_FUNC int _wfindnext(long long __handle, struct _wfinddata_t *__findinfo) __SPRT_NOEXCEPT {
	if (__handle == -1 || !__findinfo) {
		__sprt_errno = EINVAL;
		return -1;
	}
	WIN32_FIND_DATAW data;
	if (!FindNextFileW(reinterpret_cast<HANDLE>(static_cast<__SPRT_ID(intptr_t)>(__handle)),
				&data)) {
		DWORD err = GetLastError();
		__sprt_errno = (err == ERROR_NO_MORE_FILES) ? ENOENT : platform::lastErrorToErrno(err);
		return -1;
	}
	__find_fill(data, __findinfo);
	return 0;
}

__SPRT_C_FUNC int _findclose(long long __handle) __SPRT_NOEXCEPT {
	if (__handle == -1) {
		__sprt_errno = EINVAL;
		return -1;
	}
	if (!FindClose(reinterpret_cast<HANDLE>(static_cast<__SPRT_ID(intptr_t)>(__handle)))) {
		__sprt_errno = platform::lastErrorToErrno(GetLastError());
		return -1;
	}
	return 0;
}

} // namespace sprt

#ifdef __clang__
#pragma clang diagnostic pop
#endif
