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

#include <sprt/c/__sprt_fcntl.h>
#include <sprt/c/__sprt_string.h>
#include <sprt/c/__sprt_stdio.h>
#include <sprt/c/__sprt_errno.h>
#include <sprt/c/__sprt_stdarg.h>

#include <sprt/runtime/string.h>
#include <sprt/runtime/enum.h>

#include "private/SPRTFilename.h"

#define WIN32_LEAN_AND_MEAN

#include <Windows.h>
#include <shlwapi.h>
#include <corecrt_io.h>
#include <fcntl.h>

#include "private/SPRTSpecific.h"

#ifdef __clang__
#pragma clang diagnostic ignored "-Wunused-function"
#endif

namespace sprt::platform {

static bool isWinAbsolutePath(const char *path) {
	if (!path) {
		return false;
	}

	if (path[0] == '/') {
		return true;
	}

	return PathIsRelativeA(path) == FALSE;
}

bool openAtPath(int fd, const char *path, const callback<void(const char *, size_t)> &cb) {
	if (isWinAbsolutePath(path)) {
		auto pathlen = strlen(path);
		auto isNative = __sprt_fpath_is_native(path, pathlen);

		auto bufLen = pathlen + 1;

		auto buf = (char *)__sprt_malloca(bufLen + 1);
		auto target = buf;
		auto bufferSize = bufLen;

		if (!isNative) {
			auto ws = __sprt_fpath_to_native(path, pathlen, target, pathlen + 1);
			if (ws == 0) {
				return false;
			}

			target += ws;
			bufferSize -= ws;
		} else {
			target = strappend(target, &bufferSize, path, pathlen);
		}

		cb(buf, target - buf);

		__sprt_freea(buf);
		return true;
	}

	auto extraPathLen = path ? strlen(path) + 1 : 0;

	char *buffer = nullptr;

	DWORD pathLen = 0;
	DWORD writtenLen = 0;
	DWORD bufferLen = 0;

	if (fd == __SPRT_AT_FDCWD) {
		auto pathLen = GetCurrentDirectoryA(0, nullptr);
		if (pathLen == 0) {
			__sprt_errno = EBADF;
			return false;
		}

		bufferLen = pathLen + extraPathLen + 1;
		buffer = (char *)__sprt_malloc(bufferLen);
		writtenLen = GetCurrentDirectoryA(bufferLen, buffer);
	} else {
		auto handle = (HANDLE)_get_osfhandle(fd);
		if (!handle || handle == INVALID_HANDLE_VALUE) {
			__sprt_errno = EBADF;
			return false;
		}

		FILE_STANDARD_INFO standardInfo;
		if (!GetFileInformationByHandleEx(handle, FileStandardInfo, &standardInfo,
					sizeof(FILE_STANDARD_INFO))) {
			__sprt_errno = platform::lastErrorToErrno(GetLastError());
			return false;
		}

		if (!standardInfo.Directory) {
			__sprt_errno = ENOTDIR;
			return false;
		}

		do {
			if (buffer) {
				__sprt_free(buffer);
				buffer = nullptr;
			}

			pathLen = GetFinalPathNameByHandleA(handle, NULL, 0, 0);
			if (pathLen == 0) {
				__sprt_errno = EBADF;
				return false;
			}

			bufferLen = pathLen + extraPathLen + 1;
			buffer = (char *)__sprt_malloc(bufferLen);
			writtenLen = GetFinalPathNameByHandleA(handle, buffer, pathLen, 0);

			// if writtenLen is larger then initial value - try again
			// this may happen if file was moved between two GetFinalPathNameByHandleA calls
		} while (writtenLen > pathLen);
	}

	if (writtenLen == 0) {
		__sprt_errno = EBADF;
		return false;
	}

	auto target = buffer + writtenLen;
	size_t bufferSize = bufferLen - writtenLen;

	if (path) {
		target = strappend(target, &bufferSize, "\\", 1);

		auto pathlen = strlen(path);
		auto isNative = __sprt_fpath_is_native(path, pathlen);

		if (isNative) {
			target = strappend(target, &bufferSize, path, pathLen);
		} else {
			auto ws = __sprt_fpath_to_native(path, pathlen, target, pathlen + 1);
			if (ws == 0) {
				return false;
			}

			target += ws;
			bufferSize -= ws;
		}
	}

	cb(buffer, target - buffer);
	__sprt_free(buffer);

	return true;
}

} // namespace sprt::platform

namespace sprt {

struct __OpenInfo {
	DWORD dwDesiredAccess = 0;
	DWORD dwShareMode = 0;
	DWORD dwFlagsAndAttributes = 0;
	DWORD dwCreationDisposition = 0;

	// @AI-generated
	__OpenInfo(int __flags, __SPRT_ID(mode_t) __mode) {
		if (__flags & __SPRT_O_PATH) {
			dwDesiredAccess = 0;
			dwCreationDisposition = OPEN_EXISTING;
			dwFlagsAndAttributes = FILE_FLAG_BACKUP_SEMANTICS;
		} else {
			int accessFlags = __flags & (__SPRT_O_RDONLY | __SPRT_O_WRONLY | __SPRT_O_RDWR);
			if (accessFlags == __SPRT_O_RDONLY) {
				dwDesiredAccess = GENERIC_READ; // 0x80000000
			} else if (accessFlags == __SPRT_O_WRONLY) {
				dwDesiredAccess = GENERIC_WRITE; // 0x40000000
			} else if (accessFlags == __SPRT_O_RDWR) {
				dwDesiredAccess = GENERIC_READ | GENERIC_WRITE; // 0xC0000000
			}

			if (__flags & __SPRT_O_CREAT) {
				dwDesiredAccess |= GENERIC_WRITE;
			}

			if (__flags & __SPRT_O_APPEND) {
				if ((__flags & __SPRT_O_TRUNC) == 0) {
					dwDesiredAccess &= ~DWORD(GENERIC_WRITE); // Remove GENERIC_WRITE
				}
				// Add append-specific rights
				dwDesiredAccess |= FILE_APPEND_DATA // 0x00000004
						| FILE_WRITE_ATTRIBUTES // 0x00000100
						| STANDARD_RIGHTS_WRITE // 0x00120000
						| SYNCHRONIZE;
			}

			if ((__flags & (__SPRT_O_CREAT | __SPRT_O_EXCL)) == (__SPRT_O_CREAT | __SPRT_O_EXCL)) {
				dwCreationDisposition = CREATE_NEW; // 1 - atomic create, fails if exists
			} else if ((__flags & __SPRT_O_CREAT) == __SPRT_O_CREAT) {
				dwCreationDisposition = OPEN_ALWAYS; // 4 - create if needed, open if exists
			} else if ((__flags & __SPRT_O_TRUNC) == __SPRT_O_TRUNC) {
				dwCreationDisposition = TRUNCATE_EXISTING; // 5 - truncate if exists
				dwDesiredAccess |= GENERIC_WRITE;
			} else {
				dwCreationDisposition = OPEN_EXISTING;
			}

			dwFlagsAndAttributes = FILE_ATTRIBUTE_NORMAL; // 0x80
			if ((__mode & __SPRT_S_IWRITE) == 0) {
				dwFlagsAndAttributes = FILE_ATTRIBUTE_READONLY; // 0x01 (overrides NORMAL)
			}

			// Allow opening directories when reading (not writing)
			if (accessFlags == __SPRT_O_RDONLY) { // or not (O_WRONLY | O_RDWR)
				dwFlagsAndAttributes |= FILE_FLAG_BACKUP_SEMANTICS;
			}

			// Synchronous writes when O_SYNC is set
			if ((__flags & __SPRT_O_SYNC) != 0) {
				dwFlagsAndAttributes |= FILE_FLAG_WRITE_THROUGH;
			}

			// Don't follow symlinks when atomically creating files
			if ((__flags & (__SPRT_O_CREAT | __SPRT_O_EXCL)) == (__SPRT_O_CREAT | __SPRT_O_EXCL)) {
				dwFlagsAndAttributes |= FILE_FLAG_OPEN_REPARSE_POINT;
			}
		}

		if ((__flags & __SPRT_O_DIRECTORY) == __SPRT_O_DIRECTORY) {
			dwDesiredAccess = FILE_LIST_DIRECTORY;
			dwFlagsAndAttributes |= FILE_FLAG_BACKUP_SEMANTICS;
		}

		if ((__flags & __SPRT_O_NOFOLLOW) == __SPRT_O_NOFOLLOW) {
			dwFlagsAndAttributes |= FILE_FLAG_OPEN_REPARSE_POINT;
		}

		dwShareMode = FILE_SHARE_READ | FILE_SHARE_WRITE;
	}
};

static int __open(const char *path, int __flags, __SPRT_ID(mode_t) __mode) {
	if ((__flags & __SPRT_O_TMPFILE) || (__flags & __SPRT_O_NONBLOCK)) {
		__sprt_errno = ENOSYS;
		return -1;
	}

	__OpenInfo info(__flags, __mode);

	HANDLE h = CreateFileA(path, // file path
			info.dwDesiredAccess, // ← maps to O_RDONLY/O_WRONLY/O_RDWR
			info.dwShareMode, // ← no POSIX equivalent; Windows-specific
			nullptr, // security descriptor
			info.dwCreationDisposition, // ← maps to O_CREAT/O_EXCL/O_TRUNC
			info.dwFlagsAndAttributes, // ← maps to mode + O_APPEND/O_SYNC/FILE_FLAG_*
			nullptr);
	if (!h || h == INVALID_HANDLE_VALUE) {
		__sprt_errno = platform::lastErrorToErrno(GetLastError());
		return -1;
	}

	if ((__flags & __SPRT_O_PATH) == 0) {
		// Handle O_TRUNC explicitly (avoids FILE_ATTRIBUTE_READONLY issues)
		if ((__flags & __SPRT_O_SYNC) != 0 && info.dwCreationDisposition == OPEN_ALWAYS
				&& GetLastError() == ERROR_ALREADY_EXISTS) {
			SetFilePointer(h, 0, NULL, FILE_BEGIN);
			SetEndOfFile(h);
		}
	}

	if ((__flags & __SPRT_O_DIRECTORY) == 0) {
		BY_HANDLE_FILE_INFORMATION info;
		if (GetFileInformationByHandle(h, &info)) {
			if ((info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
				CloseHandle(h);
				__sprt_errno = ENOTDIR;
				return -1;
			}
		}
		CloseHandle(h);
		__sprt_errno = EINVAL;
		return -1;
	}

	int accessFlags = __flags & (__SPRT_O_RDONLY | __SPRT_O_WRONLY | __SPRT_O_RDWR);
	int openHandleFlags = _O_BINARY;
	if (__flags & __SPRT_O_APPEND) {
		openHandleFlags |= _O_APPEND;
	}
	if (accessFlags == __SPRT_O_RDONLY) {
		openHandleFlags |= _O_RDONLY;
	}
	return _open_osfhandle(intptr_t(h), openHandleFlags);
}

static HANDLE __reopen(HANDLE h, int __flags, __SPRT_ID(mode_t) __mode) {
	if ((__flags & __SPRT_O_TMPFILE) || (__flags & __SPRT_O_NONBLOCK)) {
		__sprt_errno = ENOSYS;
		return nullptr;
	}

	if ((__flags & __SPRT_O_PATH) || (__flags & __SPRT_O_DIRECTORY)) {
		__sprt_errno = EINVAL;
		return nullptr;
	}

	__OpenInfo info(__flags, __mode);

	auto newH = ReOpenFile(h, info.dwDesiredAccess, info.dwShareMode, info.dwFlagsAndAttributes);
	if (!newH || newH == INVALID_HANDLE_VALUE) {
		__sprt_errno = platform::lastErrorToErrno(GetLastError());
		return nullptr;
	}

	// Handle O_TRUNC explicitly (avoids FILE_ATTRIBUTE_READONLY issues)
	if ((__flags & __SPRT_O_SYNC) != 0 && info.dwCreationDisposition == OPEN_ALWAYS
			&& GetLastError() == ERROR_ALREADY_EXISTS) {
		SetFilePointer(h, 0, NULL, FILE_BEGIN);
		SetEndOfFile(h);
	}

	CloseHandle(h);

	return newH;
}

static int open64(const char *path, int __flags, __SPRT_ID(mode_t) mode) {
	__SPRT_ID(mode_t) __mode = 0;

	return internal::performWithNativePath(path, [&](const char *target) {
		return __open(target, __flags, __mode); //
	}, -1);
}

static int creat64(const char *path, __SPRT_ID(mode_t) __mode) {
	return open64(path, __SPRT_O_CREAT | __SPRT_O_WRONLY | __SPRT_O_TRUNC, __mode);
}

static int openat64(int __dir_fd, const char *path, int __flags, __SPRT_ID(mode_t) __mode) {
	int ret = 0;
	platform::openAtPath(__dir_fd, path,
			[&](const char *path, size_t pathLen) { ret = __sprt_open(path, __flags, __mode); });
	return ret;
}

static int fcntl(int __fd, int __cmd, unsigned long arg) {
	auto handle = (HANDLE)_get_osfhandle(__fd);
	if (!handle || handle == INVALID_HANDLE_VALUE) {
		__sprt_errno = EBADF;
		return -1;
	}

	switch (__cmd) {
	case __SPRT_F_DUPFD: return _dup(__fd); break;
	case __SPRT_F_DUPFD_CLOEXEC: {
		auto nfd = _dup(__fd);
		auto nhandle = (HANDLE)_get_osfhandle(nfd);

		if (!handle || handle == INVALID_HANDLE_VALUE) {
			_close(nfd);
			__sprt_errno = EBADF;
			return -1;
		}

		DWORD flagsMask = HANDLE_FLAG_INHERIT;
		DWORD flagsToSet = 0;

		if (SetHandleInformation(nhandle, flagsMask, flagsToSet)) {
			return nfd;
		} else {
			_close(nfd);
			__sprt_errno = platform::lastErrorToErrno(GetLastError());
			return -1;
		}
		break;
	}
	case __SPRT_F_GETFD: {
		DWORD flags = 0;
		if (GetHandleInformation(handle, &flags)) {
			int ret = 0;
			if (flags & HANDLE_FLAG_INHERIT) {
				ret |= __SPRT_FD_CLOEXEC;
			}
			return ret;
		} else {
			__sprt_errno = platform::lastErrorToErrno(GetLastError());
			return -1;
		}
		break;
	}
	case __SPRT_F_SETFD: {
		DWORD flagsMask = HANDLE_FLAG_INHERIT;
		DWORD flagsToSet = 0;
		if (arg & __SPRT_FD_CLOEXEC) {
			flagsToSet |= HANDLE_FLAG_INHERIT;
			arg &= ~(unsigned long)__SPRT_FD_CLOEXEC;
		}
		if (arg != 0) {
			// unknown flags, fail with EINVAL
			__sprt_errno = EINVAL;
			return -1;
		}
		if (SetHandleInformation(handle, flagsMask, flagsToSet)) {
			return 0;
		} else {
			__sprt_errno = platform::lastErrorToErrno(GetLastError());
			return -1;
		}
		break;
	}
	}
	__sprt_errno = EINVAL;
	return -1;
}

} // namespace sprt
