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

#include <sprt/c/sys/__sprt_stat.h>
#include <sprt/c/__sprt_time.h>
#include <sprt/runtime/stringview.h>
#include <sprt/runtime/log.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <aclapi.h>
#include <shlwapi.h>
#include <corecrt_io.h>
#include <sys/stat.h>

#include "private/SPRTSpecific.h" // IWYU pragma: keep
#include "private/SPRTFilename.h" // IWYU pragma: keep

#ifdef __clang__
#pragma clang diagnostic ignored "-Wunused-function"
#endif

namespace sprt {

static struct __SPRT_TIMESPEC_NAME __toTimespec(const FILETIME &ft) {
	constexpr unsigned __int64 TICKS_PER_SEC = 10'000'000ULL;
	constexpr unsigned __int64 EPOCH_DIFFERENCE = 11'644'473'600ULL;

	ULARGE_INTEGER ull;
	ull.LowPart = ft.dwLowDateTime;
	ull.HighPart = ft.dwHighDateTime;

	struct __SPRT_TIMESPEC_NAME ts;
	ts.tv_sec = (time_t)((ull.QuadPart / TICKS_PER_SEC) - EPOCH_DIFFERENCE);
	ts.tv_nsec = (long)((ull.QuadPart % TICKS_PER_SEC) * 100);
	return ts;
}


LARGE_INTEGER __toFiletime(const struct __SPRT_TIMESPEC_NAME *ts) {
	constexpr unsigned __int64 EPOCH_DIFF_NS100 = (11'644'473'600ULL * 10'000'000ULL);

	LARGE_INTEGER ret;
	unsigned __int64 total_ns100 = (unsigned __int64)ts->tv_sec * 10'000'000ULL + ts->tv_nsec;
	total_ns100 += EPOCH_DIFF_NS100;
	ret.QuadPart = total_ns100;
	return ret;
}

static int __hstat(HANDLE h, struct __SPRT_STAT_NAME *__stat) {
	PSID owner_sid = nullptr;
	PSID group_sid = nullptr;

	FILE_STORAGE_INFO storageInfo;
	FILE_STANDARD_INFO standardInfo;
	BY_HANDLE_FILE_INFORMATION data;
	if (!GetFileInformationByHandle(h, &data)) {
		CloseHandle(h);
		__sprt_errno = platform::lastErrorToErrno(GetLastError());
		return -1;
	}

	if (!GetFileInformationByHandleEx(h, FileStandardInfo, &standardInfo,
				sizeof(FILE_STANDARD_INFO))) {
		CloseHandle(h);
		__sprt_errno = platform::lastErrorToErrno(GetLastError());
		return -1;
	}

	if (!GetFileInformationByHandleEx(h, FileStorageInfo, &storageInfo,
				sizeof(FILE_STORAGE_INFO))) {
		CloseHandle(h);
		__sprt_errno = platform::lastErrorToErrno(GetLastError());
		return -1;
	}

	if (GetSecurityInfo(h, SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION,
				&owner_sid, &group_sid, NULL, NULL, NULL)
			== ERROR_SUCCESS) {
		__stat->st_uid = platform::getRid(owner_sid);
		__stat->st_gid = platform::getRid(group_sid);
	} else {
		__stat->st_uid = 0;
		__stat->st_gid = 0;
	}

	__stat->st_dev = data.dwVolumeSerialNumber;
	__stat->st_ino = ULARGE_INTEGER{{data.nFileIndexLow, data.nFileIndexHigh}}.QuadPart;
	__stat->st_nlink = data.nNumberOfLinks;
	__stat->st_rdev = 0;
	__stat->st_blksize = storageInfo.LogicalBytesPerSector;
	__stat->st_blocks = standardInfo.AllocationSize.QuadPart / max(__stat->st_blksize, 512);

	if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
		__stat->st_mode = __SPRT_S_IFDIR | 0555; // Dirs often executable/searchable
	} else if (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
		__stat->st_mode = __SPRT_S_IFLNK | 0555; // Reparse often symlinks
	} else {
		__stat->st_mode = __SPRT_S_IFREG | 0444; // Default regular file
	}

	if ((data.dwFileAttributes & FILE_ATTRIBUTE_READONLY) == 0) {
		__stat->st_mode |= 0222;
	}

	__stat->st_size = ULARGE_INTEGER{{data.nFileSizeLow, data.nFileSizeHigh}}.QuadPart;

	__stat->st_ctim = __toTimespec(data.ftCreationTime);
	__stat->st_atim = __toTimespec(data.ftLastAccessTime);
	__stat->st_mtim = __toTimespec(data.ftLastWriteTime);

	return 0;
}

static int __wstat(const char *__SPRT_RESTRICT path,
		struct __SPRT_STAT_NAME *__SPRT_RESTRICT __stat, bool reparsePoint) {
	auto wpath = __MALLOCA_WSTRING(path);
	HANDLE h = CreateFileW(wpath, GENERIC_READ,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
			FILE_FLAG_BACKUP_SEMANTICS | (reparsePoint ? FILE_FLAG_OPEN_REPARSE_POINT : 0), NULL);
	__sprt_freea(wpath);
	if (h == INVALID_HANDLE_VALUE) {
		__sprt_errno = platform::lastErrorToErrno(GetLastError());
		return -1;
	}

	auto ret = __hstat(h, __stat);
	CloseHandle(h);
	return ret;
}

static int stat(const char *__SPRT_RESTRICT path, struct __SPRT_STAT_NAME *__SPRT_RESTRICT __stat) {
	return internal::performWithNativePath(path, [&](const char *target) {
		return __wstat(target, __stat, false); //
	}, -1);
}

static int lstat(const char *__SPRT_RESTRICT path,
		struct __SPRT_STAT_NAME *__SPRT_RESTRICT __stat) {
	return internal::performWithNativePath(path, [&](const char *target) {
		return __wstat(target, __stat, true); //
	}, -1);
}

static int fstat(int fd, struct __SPRT_STAT_NAME *__SPRT_RESTRICT __stat) {
	if (fd < 0) {
		__sprt_errno = EBADF;
		return -1;
	}

	HANDLE h = (HANDLE)_get_osfhandle(fd);
	if (h && h != INVALID_HANDLE_VALUE) {
		return __hstat(h, __stat);
	}
	__sprt_errno = EBADF;
	return -1;
}

static int fstatat(int fd, const char *__path, struct __SPRT_STAT_NAME *__SPRT_RESTRICT __stat,
		int __flags) {
	int ret = -1;
	platform::openAtPath(fd, __path, [&](const char *path, size_t) {
		ret = __wstat(path, __stat, (__flags & __SPRT_AT_SYMLINK_NOFOLLOW) ? true : false);
	});
	return ret;
}

static int __wchmod(const wchar_t *__path, __SPRT_ID(mode_t) mode) {
	auto origAttr = GetFileAttributesW(__path);
	auto attr = origAttr;
	if (attr == INVALID_FILE_ATTRIBUTES) {
		__sprt_errno = platform::lastErrorToErrno(GetLastError());
		return -1;
	}
	if (mode & __SPRT_S_IWUSR) {
		// Make file writable -> clear READONLY
		attr &= ~FILE_ATTRIBUTE_READONLY;
	} else {
		// No write bit -> read-only
		attr |= FILE_ATTRIBUTE_READONLY;
	}
	if (attr != origAttr) {
		if (!SetFileAttributesW(__path, attr)) {
			__sprt_errno = platform::lastErrorToErrno(GetLastError());
			return -1;
		}
	}
	return 0;
}

static int chmod(const char *__path, __SPRT_ID(mode_t) mode) {
	return internal::performWithNativePath(__path, [&](const char *target) {
		auto wpath = __MALLOCA_WSTRING(target);
		auto ret = __wchmod(wpath, mode); //
		__sprt_freea(wpath);
		return ret;
	}, -1);
}

static int fchmod(int __fd, __SPRT_ID(mode_t) mode) {
	if (__fd < 0) {
		__sprt_errno = EBADF;
		return -1;
	}

	HANDLE hFile = (HANDLE)_get_osfhandle(__fd);
	if (!hFile || hFile == INVALID_HANDLE_VALUE) {
		__sprt_errno = EBADF;
		return -1;
	}

	FILE_BASIC_INFO fbi;
	if (!GetFileInformationByHandleEx(hFile, FileBasicInfo, &fbi, sizeof(FILE_BASIC_INFO))) {
		return -1;
	}

	if (mode & __SPRT_S_IWUSR) {
		fbi.FileAttributes &= ~FILE_ATTRIBUTE_READONLY;
	} else {
		fbi.FileAttributes |= FILE_ATTRIBUTE_READONLY;
	}

	if (!SetFileInformationByHandle(hFile, FileBasicInfo, &fbi, sizeof(FILE_BASIC_INFO))) {
		__sprt_errno = platform::lastErrorToErrno(GetLastError());
		return -1;
	}
	return 0;
}

static int fchmodat(int __fd, const char *__path, __SPRT_ID(mode_t) mode, int flags) {
	int ret = -1;
	platform::openAtPath(__fd, __path, [&](const char *path, size_t) {
		auto wpath = __MALLOCA_WSTRING(path);
		ret = __wchmod(wpath, mode); //
		__sprt_freea(wpath);
	});
	return ret;
}

static __SPRT_ID(mode_t) umask(__SPRT_ID(mode_t) mode) {
	int nMode = 0;
	int oMode = 0;

	if (mode & __SPRT_S_IWUSR) {
		nMode |= _S_IWRITE;
	}
	if (mode & __SPRT_S_IRUSR) {
		nMode |= _S_IREAD;
	}

	auto err = _umask_s(nMode, &oMode);
	if (err == 0) {
		mode = 0111;
		if (oMode & _S_IWRITE) {
			mode |= 0222;
		}
		if (oMode & _S_IREAD) {
			mode |= 0444;
		}
	}
	return mode;
}

static int __wmkdir(const char *__path, __SPRT_ID(mode_t) mode) {
	auto wpath = __MALLOCA_WSTRING(__path);
	if (!CreateDirectoryW(wpath, nullptr)) {
		__sprt_freea(wpath);
		__sprt_errno = platform::lastErrorToErrno(GetLastError());
		return -1;
	}
	__wchmod(wpath, mode);
	__sprt_freea(wpath);
	return 0;
}

static int mkdir(const char *__path, __SPRT_ID(mode_t) mode) {
	return internal::performWithNativePath(__path, [&](const char *target) {
		return __wmkdir(target, mode); //
	}, -1);
}

static int mkdirat(int __fd, const char *__path, __SPRT_ID(mode_t) mode) {
	int ret = -1;
	platform::openAtPath(__fd, __path, [&](const char *path, size_t) {
		ret = __wmkdir(path, mode); //
	});
	return ret;
}

static int __wfutimens(HANDLE hFile, const struct __SPRT_TIMESPEC_NAME *times) {
	FILE_BASIC_INFO fbi;

	if (!GetFileInformationByHandleEx(hFile, FileBasicInfo, &fbi, sizeof(FILE_BASIC_INFO))) {
		__sprt_errno = EBADF;
		return -1;
	}

	struct __SPRT_TIMESPEC_NAME now;
	__sprt_clock_gettime(__SPRT_CLOCK_REALTIME, &now);

	if (times == nullptr) {
		fbi.LastWriteTime = fbi.LastAccessTime = __toFiletime(&now);
	} else {
		if (times[0].tv_nsec == __SPRT_UTIME_NOW) {
			fbi.LastAccessTime = __toFiletime(&now);
		} else if (times[0].tv_nsec != __SPRT_UTIME_OMIT) {
			fbi.LastAccessTime = __toFiletime(&times[0]);
		}

		if (times[0].tv_nsec == __SPRT_UTIME_NOW) {
			fbi.LastWriteTime = __toFiletime(&now);
		} else if (times[0].tv_nsec != __SPRT_UTIME_OMIT) {
			fbi.LastWriteTime = __toFiletime(&times[1]);
		}
	}

	if (!SetFileInformationByHandle(hFile, FileBasicInfo, &fbi, sizeof(fbi))) {
		__sprt_errno = platform::lastErrorToErrno(GetLastError());
		return -1;
	}
	return 0;
}

static int futimens(int __fd, const struct __SPRT_TIMESPEC_NAME *times) {
	if (__fd < 0) {
		__sprt_errno = EBADF;
		return -1;
	}

	HANDLE hFile = (HANDLE)_get_osfhandle(__fd);
	if (!hFile || hFile == INVALID_HANDLE_VALUE) {
		__sprt_errno = EBADF;
		return -1;
	}

	return __wfutimens(hFile, times);
}

static int utimensat(int __fd, const char *__path, const __SPRT_TIMESPEC_NAME *times, int flags) {
	int ret = -1;
	platform::openAtPath(__fd, __path, [&](const char *path, size_t) {
		auto wpath = __MALLOCA_WSTRING(path);
		HANDLE hFile = CreateFileW(wpath, FILE_WRITE_ATTRIBUTES,
				FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
				FILE_FLAG_BACKUP_SEMANTICS
						| ((flags & __SPRT_AT_SYMLINK_NOFOLLOW) ? FILE_FLAG_OPEN_REPARSE_POINT : 0),
				NULL);
		__sprt_freea(wpath);
		if (!hFile || hFile == INVALID_HANDLE_VALUE) {
			__sprt_errno = platform::lastErrorToErrno(GetLastError());
			ret = -1;
		} else {
			ret = __wfutimens(hFile, times);
		}
	});
	return ret;
}

} // namespace sprt
