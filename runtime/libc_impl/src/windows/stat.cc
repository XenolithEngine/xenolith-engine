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

#include <sprt/c/sys/__sprt_stat.h>
#include <sprt/c/sys/__sprt_statvfs.h>
#include <sprt/c/__sprt_time.h>
#include <sprt/runtime/stringview.h>
#include <sprt/runtime/log.h>

#include <io.h>
#include "specific.h" // IWYU pragma: keep
#include "sys/stat.h" // IWYU pragma: keep
#include "sys/statvfs.h" // IWYU pragma: keep

#include <sprt/wrappers/windows/file_api.h>
#include <sprt/wrappers/windows/security_api.h>
#include <sprt/wrappers/windows/basic_api.h>

namespace sprt::platform {

int hstat(HANDLE h, struct __SPRT_STAT_NAME *__stat) {
	PSID owner_sid = nullptr;
	PSID group_sid = nullptr;

	FILE_STORAGE_INFO storageInfo;
	FILE_STANDARD_INFO standardInfo;
	BY_HANDLE_FILE_INFORMATION data;
	if (!GetFileInformationByHandle(h, &data)) {
		// The HANDLE is owned by the caller (__wstat / __file_stat); do not
		// close it here, or it will be closed twice.
		__sprt_errno = platform::lastErrorToErrno(GetLastError());
		return -1;
	}

	if (!GetFileInformationByHandleEx(h, FileStandardInfo, &standardInfo,
				sizeof(FILE_STANDARD_INFO))) {
		// The HANDLE is owned by the caller (__wstat / __file_stat); do not
		// close it here, or it will be closed twice.
		__sprt_errno = platform::lastErrorToErrno(GetLastError());
		return -1;
	}

	// FileStorageInfo only feeds the advisory st_blksize/st_blocks fields and is not implemented on
	// every backend (notably wine), so its absence must NOT fail stat(): fall back to a 512-byte
	// sector. (Compare GetSecurityInfo below, which is likewise tolerated when unavailable.)
	if (!GetFileInformationByHandleEx(h, FileStorageInfo, &storageInfo,
				sizeof(FILE_STORAGE_INFO))) {
		storageInfo.LogicalBytesPerSector = 512;
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
	__stat->st_blocks = standardInfo.AllocationSize.QuadPart
			/ sprt::max(__stat->st_blksize, (__SPRT_ID(blksize_t))512);

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

int hutimens(HANDLE hFile, const struct __SPRT_TIMESPEC_NAME *times) {
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
		// times[0] = access time, times[1] = modification time (POSIX).
		if (times[0].tv_nsec == __SPRT_UTIME_NOW) {
			fbi.LastAccessTime = __toFiletime(&now);
		} else if (times[0].tv_nsec != __SPRT_UTIME_OMIT) {
			fbi.LastAccessTime = __toFiletime(&times[0]);
		}

		if (times[1].tv_nsec == __SPRT_UTIME_NOW) {
			fbi.LastWriteTime = __toFiletime(&now);
		} else if (times[1].tv_nsec != __SPRT_UTIME_OMIT) {
			fbi.LastWriteTime = __toFiletime(&times[1]);
		}
	}

	if (!SetFileInformationByHandle(hFile, FileBasicInfo, &fbi, sizeof(fbi))) {
		__sprt_errno = platform::lastErrorToErrno(GetLastError());
		return -1;
	}
	return 0;
}

// Fill a POSIX statvfs from the volume that `root` (a "X:\" / mount-point path from
// GetVolumePathNameW) belongs to. Windows exposes allocation units (clusters), so the
// POSIX block == cluster; inodes have no counterpart on NTFS/FAT and read back as 0.
static int __wstatvfs_root(const wchar_t *root, struct __SPRT_STATVFS_NAME *buf) {
	DWORD sectorsPerCluster = 0, bytesPerSector = 0, freeClusters = 0, totalClusters = 0;
	if (!GetDiskFreeSpaceW(root, &sectorsPerCluster, &bytesPerSector, &freeClusters,
				&totalClusters)) {
		__sprt_errno = platform::lastErrorToErrno(GetLastError());
		return -1;
	}

	unsigned long clusterSize = (unsigned long)bytesPerSector * (unsigned long)sectorsPerCluster;
	if (clusterSize == 0) {
		clusterSize = 4'096;
	}

	// GetDiskFreeSpaceEx is the accurate source on volumes > 2 GiB and honours per-user
	// quotas (f_bavail); GetDiskFreeSpace's cluster counts are the fallback if it fails.
	ULARGE_INTEGER availBytes, totalBytes, freeBytes;
	if (GetDiskFreeSpaceExW(root, &availBytes, &totalBytes, &freeBytes)) {
		buf->f_blocks = (__SPRT_ID(fsblkcnt_t))(totalBytes.QuadPart / clusterSize);
		buf->f_bfree = (__SPRT_ID(fsblkcnt_t))(freeBytes.QuadPart / clusterSize);
		buf->f_bavail = (__SPRT_ID(fsblkcnt_t))(availBytes.QuadPart / clusterSize);
	} else {
		buf->f_blocks = totalClusters;
		buf->f_bfree = freeClusters;
		buf->f_bavail = freeClusters;
	}

	buf->f_bsize = clusterSize;
	buf->f_frsize = clusterSize;
	buf->f_files = 0;
	buf->f_ffree = 0;
	buf->f_favail = 0;

	DWORD serial = 0, maxComp = 0, fsFlags = 0;
	if (GetVolumeInformationW(root, nullptr, 0, &serial, &maxComp, &fsFlags, nullptr, 0)) {
		buf->f_fsid = serial;
		buf->f_namemax = maxComp;
		buf->f_flag = (fsFlags & FILE_READ_ONLY_VOLUME) ? ST_RDONLY : 0;
	} else {
		buf->f_fsid = 0;
		buf->f_namemax = 255;
		buf->f_flag = 0;
	}
	return 0;
}

// statvfs backend keyed by an open HANDLE: resolve it to a path, then to its volume
// root. Shared by fstatvfs() (builtin_stat.cpp) on Windows.
int hstatvfs(HANDLE h, struct __SPRT_STATVFS_NAME *buf) {
	auto pathLen = GetFinalPathNameByHandleW(h, nullptr, 0, 0);
	if (pathLen == 0) {
		__sprt_errno = platform::lastErrorToErrno(GetLastError());
		return -1;
	}

	auto pbuf = __sprt_typed_malloca(wchar_t, pathLen + 1);
	auto writtenLen = GetFinalPathNameByHandleW(h, pbuf, pathLen + 1, 0);
	if (writtenLen == 0) {
		__sprt_freea(pbuf);
		__sprt_errno = platform::lastErrorToErrno(GetLastError());
		return -1;
	}

	wchar_t root[MAX_PATH];
	int ret;
	if (GetVolumePathNameW(pbuf, root, MAX_PATH)) {
		ret = __wstatvfs_root(root, buf);
	} else {
		__sprt_errno = platform::lastErrorToErrno(GetLastError());
		ret = -1;
	}
	__sprt_freea(pbuf);
	return ret;
}

} // namespace sprt::platform

namespace sprt {

static int __wstat(const wchar_t *__SPRT_RESTRICT wpath,
		struct __SPRT_STAT_NAME *__SPRT_RESTRICT __stat, bool reparsePoint) {
	HANDLE h = CreateFileW(wpath, GENERIC_READ,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
			FILE_FLAG_BACKUP_SEMANTICS | (reparsePoint ? FILE_FLAG_OPEN_REPARSE_POINT : 0), NULL);
	if (h == INVALID_HANDLE_VALUE) {
		__sprt_errno = platform::lastErrorToErrno(GetLastError());
		return -1;
	}

	auto ret = platform::hstat(h, __stat);
	CloseHandle(h);
	return ret;
}

static int __wstat(const char *__SPRT_RESTRICT path,
		struct __SPRT_STAT_NAME *__SPRT_RESTRICT __stat, bool reparsePoint) {
	auto wpath = __MALLOCA_WSTRING(path);
	auto ret = __wstat(wpath, __stat, reparsePoint);
	__sprt_freea(wpath);
	return ret;
}

__SPRT_C_FUNC int stat(const char *__SPRT_RESTRICT path,
		struct __SPRT_STAT_NAME *__SPRT_RESTRICT __stat) __SPRT_NOEXCEPT {
	return platform::performWithNativePath(path, [&](const char *target) {
		return __wstat(target, __stat, false); //
	}, -1);
}

__SPRT_C_FUNC int _wstat(const wchar_t *path, struct _stati64 *__stat) __SPRT_NOEXCEPT {
	return platform::performWithNativePath(path, [&](const wchar_t *target) {
		return __wstat(target, (struct __SPRT_STAT_NAME *)__stat, false); //
	}, -1);
}

__SPRT_C_FUNC int lstat(const char *__SPRT_RESTRICT path,
		struct __SPRT_STAT_NAME *__SPRT_RESTRICT __stat) __SPRT_NOEXCEPT {
	return platform::performWithNativePath(path, [&](const char *target) {
		return __wstat(target, __stat, true); //
	}, -1);
}

__SPRT_C_FUNC int fstatat(int fd, const char *__path,
		struct __SPRT_STAT_NAME *__SPRT_RESTRICT __stat, int __flags) __SPRT_NOEXCEPT {
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

__SPRT_C_FUNC int chmod(const char *__path, __SPRT_ID(mode_t) mode) __SPRT_NOEXCEPT {
	return platform::performWithNativePath(__path, [&](const char *target) {
		auto wpath = __MALLOCA_WSTRING(target);
		auto ret = __wchmod(wpath, mode); //
		__sprt_freea(wpath);
		return ret;
	}, -1);
}


__SPRT_C_FUNC int fchmodat(int __fd, const char *__path, __SPRT_ID(mode_t) mode,
		int flags) __SPRT_NOEXCEPT {
	int ret = -1;
	platform::openAtPath(__fd, __path, [&](const char *path, size_t) {
		auto wpath = __MALLOCA_WSTRING(path);
		ret = __wchmod(wpath, mode); //
		__sprt_freea(wpath);
	});
	return ret;
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

__SPRT_C_FUNC int mkdir(const char *__path, __SPRT_ID(mode_t) mode) __SPRT_NOEXCEPT {
	return platform::performWithNativePath(__path, [&](const char *target) {
		return __wmkdir(target, mode); //
	}, -1);
}

__SPRT_C_FUNC int mkdirat(int __fd, const char *__path, __SPRT_ID(mode_t) mode) __SPRT_NOEXCEPT {
	int ret = -1;
	platform::openAtPath(__fd, __path, [&](const char *path, size_t) {
		ret = __wmkdir(path, mode); //
	});
	return ret;
}

__SPRT_C_FUNC int utimensat(int __fd, const char *__path, const __SPRT_TIMESPEC_NAME *times,
		int flags) __SPRT_NOEXCEPT {
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
			ret = platform::hutimens(hFile, times);
			CloseHandle(hFile);
		}
	});
	return ret;
}

__SPRT_C_FUNC int statvfs(const char *__SPRT_RESTRICT path,
		struct __SPRT_STATVFS_NAME *__SPRT_RESTRICT buf) __SPRT_NOEXCEPT {
	return platform::performWithNativePath(path, [&](const char *target) {
		auto wpath = __MALLOCA_WSTRING(target);
		wchar_t root[MAX_PATH];
		int ret;
		if (GetVolumePathNameW(wpath, root, MAX_PATH)) {
			ret = platform::__wstatvfs_root(root, buf);
		} else {
			__sprt_errno = platform::lastErrorToErrno(GetLastError());
			ret = -1;
		}
		__sprt_freea(wpath);
		return ret;
	}, -1);
}

} // namespace sprt
