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
#include "fcntl.h"
#include "../../include/__impl_libc.h"

#include "specific.h"

#include <sprt/c/__sprt_fcntl.h>
#include <sprt/c/__sprt_unistd.h>
#include <sprt/wrappers/windows/file_api.h>
#include <sprt/wrappers/windows/basic_api.h>
#include <sprt/wrappers/windows/security_api.h>

#include "../../../src/private/SPRTFilename.h" // IWYU pragma: keep

namespace sprt::platform {

// WSA error code constants (from winsock2.h)
// Note: WSAGetLastError() is used for socket errors, but we map them here for completeness
#ifndef WSAECONNRESET
#define WSAECONNRESET 10'054
#define WSAENETDOWN 10'050
#define WSAENOTCONN 10'057
#define WSAEISCONN 10'056
#define WSAEAFNOSUPPORT 10'047
#define WSAEMSGSIZE 10'040
#define WSAEDESTADDRREQ 10'039
#define WSAENOPROTOOPT 10'042
#define WSAESOCKTNOSUPPORT 10'044
#define WSAEPFNOSUPPORT 10'046
#define WSAEHOSTDOWN 10'064
#define WSAETOOMANYREFS 10'059
#endif

OpenInfo::OpenInfo(int __flags, int __mode) {
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

static bool isWinAbsolutePath(const char *path) {
	if (!path) {
		return false;
	}

	if (path[0] == '/') {
		return true;
	}

	auto wpath = __MALLOCA_WSTRING(path);

	auto ret = PathIsRelativeW(wpath) == FALSE;

	__sprt_freea(wpath);
	return ret;
}

static WideStringView allocateNativePath(int fd) {
	size_t pathLen = 0;
	wchar_t *pathBuffer = 0;
	if (fd == __SPRT_AT_FDCWD) {
		pathLen = GetCurrentDirectoryW(0, nullptr);
		if (pathLen == 0) {
			__sprt_errno = EBADF;
			return WideStringView();
		}

		pathBuffer = (wchar_t *)__sprt_malloc((pathLen + 1) * sizeof(wchar_t));
		pathLen = GetCurrentDirectoryW(pathLen + 1, pathBuffer);
	} else {
		if (fd < 0) {
			__sprt_errno = EBADF;
			return WideStringView();
		}

		auto slot = __libc::get()->get_fd_slot(fd);
		if (!slot || slot->handle) {
			__sprt_errno = EBADF;
			return WideStringView();
		}

		FILE_STANDARD_INFO standardInfo;
		if (!GetFileInformationByHandleEx(slot->handle, FileStandardInfo, &standardInfo,
					sizeof(FILE_STANDARD_INFO))) {
			__sprt_errno = platform::lastErrorToErrno(GetLastError());
			return WideStringView();
		}

		if (!standardInfo.Directory) {
			__sprt_errno = ENOTDIR;
			return WideStringView();
		}

		size_t bufferLen = 0;
		do {
			if (pathBuffer) {
				__sprt_free(pathBuffer);
				pathBuffer = nullptr;
			}

			pathLen = GetFinalPathNameByHandleW(slot->handle, NULL, 0, 0);
			if (pathLen == 0) {
				__sprt_errno = EBADF;
				return WideStringView();
			}

			bufferLen = pathLen + 1;
			pathBuffer = (wchar_t *)__sprt_malloc(bufferLen * sizeof(wchar_t));
			pathLen = GetFinalPathNameByHandleW(slot->handle, pathBuffer, bufferLen, 0);

			// if writtenLen is larger then initial value - try again
			// this may happen if file was moved between two GetFinalPathNameByHandleA calls
		} while (pathLen > bufferLen);
	}
	return WideStringView((char16_t *)pathBuffer, pathLen);
}

bool openAtPath(int fd, const char *path, const callback<void(const char *, size_t)> &cb) {
	if (isWinAbsolutePath(path)) {
		auto pathlen = strlen(path);
		auto isNative = __sprt_fpath_is_native(path, pathlen);

		auto bufLen = sprt::max(pathlen + 1, size_t(8));

		auto buf = __sprt_typed_malloca(char, bufLen + 1);
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

	auto extraPathLen = path ? sprt::max(strlen(path) + 2, size_t(8)) : 0;

	auto nativePath = allocateNativePath(fd);
	if (nativePath.empty()) {
		return false;
	}

	auto resultBufferLen = extraPathLen + unicode::getUtf8Length(nativePath);
	auto buffer = __sprt_typed_malloca(char, resultBufferLen);

	size_t offset = 0;
	unicode::toUtf8(buffer, resultBufferLen, nativePath, &offset);

	auto target = buffer + offset;
	size_t bufferSize = resultBufferLen - offset;

	if (path) {
		target = strappend(target, &bufferSize, "\\", 1);

		auto pathlen = strlen(path);
		auto isNative = __sprt_fpath_is_native(path, pathlen);

		if (isNative) {
			target = strappend(target, &bufferSize, path, pathlen);
		} else {
			auto ws = __sprt_fpath_to_native(path, pathlen, target, bufferSize);
			if (ws == 0) {
				return false;
			}

			target += ws;
			bufferSize -= ws;
		}
	}

	StringView out(buffer, target - buffer);
	if (out.starts_with("\\\\?\\")) {
		out += 4;
	}

	cb(out.data(), out.size());
	__sprt_freea(buffer);
	__sprt_free((void *)nativePath.data());

	return true;
}

bool isAppContainer() { return __libc::get()->isAppContainer; }

uint32_t getRid(void *sid) {
	if (!GetSidSubAuthorityCount((LPSID)sid)) {
		__sprt_errno = EINVAL;
		return uint32_t(-1);
	}

	DWORD sub_count = *GetSidSubAuthorityCount((LPSID)sid);

	return (uint32_t)*GetSidSubAuthority((LPSID)sid, sub_count - 1);
}

int lastErrorToErrno(unsigned long winerr) {
	switch (winerr) {
	// Success
	case ERROR_SUCCESS: return 0;

	// File and path errors
	case ERROR_FILE_NOT_FOUND:
	case ERROR_PATH_NOT_FOUND:
	case ERROR_BAD_PATHNAME:
	case ERROR_INVALID_DRIVE:
	case ERROR_BAD_NETPATH:
	case ERROR_BAD_NET_NAME:
	case ERROR_FILENAME_EXCED_RANGE:
		return ENOENT; // No such file or directory

	// Permission and access errors
	case ERROR_ACCESS_DENIED:
	case ERROR_LOCK_VIOLATION:
	case ERROR_CANNOT_MAKE:
	case ERROR_NETWORK_ACCESS_DENIED:
		return EACCES; // Permission denied

	// Sharing violation (can be access denied or text file busy)
	case ERROR_SHARING_VIOLATION:
		return ETXTBSY; // Text file busy (sharing violation often indicates file is locked/in use)

	// Invalid handle/file descriptor
	case ERROR_INVALID_HANDLE:
	case ERROR_INVALID_ACCESS:
		return EBADF; // Bad file descriptor

	// Memory errors
	case ERROR_NOT_ENOUGH_MEMORY:
	case ERROR_OUTOFMEMORY:
	case ERROR_ARENA_TRASHED:
		return ENOMEM; // Out of memory

	// Invalid parameter/argument
	case ERROR_INVALID_PARAMETER:
	case ERROR_INVALID_DATA:
	case ERROR_BAD_LENGTH:
	case ERROR_INVALID_NAME:
	case ERROR_BAD_FORMAT:
		return EINVAL; // Invalid argument

	// File already exists
	case ERROR_ALREADY_EXISTS:
	case ERROR_FILE_EXISTS:
		return EEXIST; // File exists

	// Too many files
	case ERROR_TOO_MANY_OPEN_FILES: return EMFILE; // Too many open files
	case ERROR_NO_PROC_SLOTS:
		return EAGAIN; // Resource temporarily unavailable

	// I/O errors
	case ERROR_WRITE_FAULT:
	case ERROR_READ_FAULT:
	case ERROR_SEEK:
	case ERROR_BAD_COMMAND:
	case ERROR_CRC:
	case ERROR_DISK_CHANGE:
	case ERROR_GEN_FAILURE:
	case ERROR_IO_DEVICE:
	case ERROR_IO_INCOMPLETE:
	case ERROR_BAD_NET_RESP:
	case ERROR_NET_WRITE_FAULT:
		return EIO; // I/O error

	// Not ready (could be I/O error or no medium)
	case ERROR_NOT_READY:
		return ENOMEDIUM; // No medium found (or device not ready)

	// No space
	case ERROR_HANDLE_DISK_FULL:
	case ERROR_DISK_FULL:
		return ENOSPC; // No space left on device

	// Broken pipe
	case ERROR_BROKEN_PIPE:
		return EPIPE; // Broken pipe

	// Device errors
	case ERROR_BAD_UNIT:
	case ERROR_NOT_CONNECTED:
		return ENODEV; // No such device

	// Network errors
	case ERROR_NETWORK_BUSY: return EBUSY; // Device or resource busy
	case ERROR_NETNAME_DELETED: return ENOENT; // No such file or directory
	case ERROR_NETWORK_UNREACHABLE: return ENETUNREACH; // Network is unreachable
	case ERROR_HOST_UNREACHABLE: return EHOSTUNREACH; // Host is unreachable
	case ERROR_CONNECTION_REFUSED: return ECONNREFUSED; // Connection refused
	case ERROR_CONNECTION_ABORTED: return ECONNABORTED; // Connection aborted
	case WSAECONNRESET: return ECONNRESET; // Connection reset by peer
	case WSAENETDOWN:
		return ENETDOWN; // Network is down

	// Timeout
	case ERROR_SEM_TIMEOUT:
	case WAIT_TIMEOUT:
		return ETIMEDOUT; // Connection timed out

	// Socket errors
	case WSAENOTCONN: return ENOTCONN; // Transport endpoint is not connected
	case WSAEISCONN: return EISCONN; // Transport endpoint is already connected
	case ERROR_TOO_MANY_NAMES:
		return EMFILE; // Too many open files

	// Directory errors
	case ERROR_DIRECTORY:
	case ERROR_CURRENT_DIRECTORY: return ENOTDIR; // Not a directory
	case ERROR_DIR_NOT_EMPTY: return ENOTEMPTY; // Directory not empty
	case ERROR_DIRECTORY_NOT_SUPPORTED:
		return ENOSYS; // Function not implemented

	// Is a directory
	case ERROR_CANTOPEN:
		return EISDIR; // Is a directory

	// Operation in progress
	case ERROR_OPERATION_IN_PROGRESS:
		return EINPROGRESS; // Operation now in progress

	// Already in progress
	case ERROR_ALREADY_ASSIGNED:
		return EALREADY; // Operation already in progress

	// Interrupted
	case ERROR_INVALID_AT_INTERRUPT_TIME:
		return EINTR; // Interrupted system call

	// Canceled
	case ERROR_OPERATION_ABORTED:
	case ERROR_CANCELLED:
		return ECANCELED; // Operation canceled

	// Device or resource busy
	case ERROR_BUSY:
	case ERROR_BUSY_DRIVE:
	case ERROR_DEVICE_IN_USE:
		return EBUSY; // Device or resource busy

	// Function not implemented
	case ERROR_CALL_NOT_IMPLEMENTED:
		return ENOSYS; // Function not implemented

	// Address family not supported
	case WSAEAFNOSUPPORT:
		return EAFNOSUPPORT; // Address family not supported by protocol

	// Operation not supported
	case ERROR_NOT_SUPPORTED:
		return EOPNOTSUPP; // Operation not supported on transport endpoint

	// Address already in use
	case ERROR_ADDRESS_ALREADY_ASSOCIATED:
		return EADDRINUSE; // Address already in use

	// Address not available
	case ERROR_ADDRESS_NOT_ASSOCIATED:
		return EADDRNOTAVAIL; // Cannot assign requested address

	// Message too long
	case WSAEMSGSIZE:
		return EMSGSIZE; // Message too long

	// Protocol errors
	case ERROR_PROTOCOL_UNREACHABLE:
		return ENOLINK; // Link has been severed (protocol unreachable)

	// Would block / pending operations
	case ERROR_IO_PENDING:
		return EWOULDBLOCK; // Operation would block

	// Buffer overflow
	case ERROR_INSUFFICIENT_BUFFER: return ENAMETOOLONG; // File name too long
	case ERROR_BUFFER_OVERFLOW:
		return ENAMETOOLONG; // File name too long

	// Semaphore errors
	case ERROR_TOO_MANY_SEMAPHORES: return ENFILE; // Too many open files in system
	case ERROR_EXCL_SEM_ALREADY_OWNED: return EAGAIN; // Resource temporarily unavailable
	case ERROR_SEM_IS_SET: return EAGAIN; // Resource temporarily unavailable
	case ERROR_TOO_MANY_SEM_REQUESTS: return EAGAIN; // Resource temporarily unavailable
	case ERROR_SEM_OWNER_DIED:
		return EOWNERDEAD; // Owner died

	// Mutex errors (using same mapping as semaphore)

	// Not same device
	case ERROR_NOT_SAME_DEVICE:
		return EXDEV; // Cross-device link

	// Read-only file system
	case ERROR_WRITE_PROTECT:
		return EROFS; // Read-only file system (write protected)

	// Range error
	case ERROR_INVALID_ADDRESS:
		return ERANGE; // Numerical result out of range

	// Domain error
	case ERROR_INVALID_DOMAIN_ROLE:
	case ERROR_INVALID_DOMAIN_STATE:
		return EDOM; // Numerical argument out of domain

	// No message available
	case ERROR_NO_DATA:
		return ENOMSG; // No message of desired type

	// Identifier removed
	case ERROR_NO_TOKEN:
		return EIDRM; // Identifier removed

	// Invalid handle state / link number out of range
	case ERROR_INVALID_HANDLE_STATE:
		return ELNRNG; // Link number out of range

	// Medium type wrong - ERROR_BAD_MEDIA_TYPE may not be defined in all Windows versions
	// Using ERROR_BAD_UNIT as alternative for media-related errors

	// Socket operation on non-socket
	case ERROR_INVALID_LEVEL:
		return ENOTSOCK; // Socket operation on non-socket

	// Destination address required
	case WSAEDESTADDRREQ:
		return EDESTADDRREQ; // Destination address required

	// Protocol not available
	case WSAENOPROTOOPT:
		return ENOPROTOOPT; // Protocol not available

	// Socket type not supported
	case WSAESOCKTNOSUPPORT:
		return ESOCKTNOSUPPORT; // Socket type not supported

	// Protocol family not supported
	case WSAEPFNOSUPPORT:
		return EPFNOSUPPORT; // Protocol family not supported

	// Oplock not granted
	case ERROR_OPLOCK_NOT_GRANTED:
		return EACCES; // Permission denied

	// Connection timed out
	case ERROR_TIMEOUT:
		return ETIMEDOUT; // Connection timed out

	// Connection refused
	case ERROR_CONNECTION_UNAVAIL:
		return ECONNREFUSED; // Connection refused

	// Host is down
	case WSAEHOSTDOWN:
		return EHOSTDOWN; // Host is down

	// Too many references
	case WSAETOOMANYREFS:
		return ETOOMANYREFS; // Too many references: cannot splice

	// Stale file handle
	case ERROR_FILE_CORRUPT:
	case ERROR_DISK_CORRUPT:
		return ESTALE; // Stale file handle

	// Default: return generic error
	default: return EINVAL; // Invalid argument (as a generic fallback)
	}
	return EINVAL;
}

} // namespace sprt::platform

namespace sprt {

static int __open(const char *path, int __flags, __SPRT_ID(mode_t) __mode) {
	if ((__flags & __SPRT_O_TMPFILE) == __SPRT_O_TMPFILE
			|| (__flags & __SPRT_O_NONBLOCK) == __SPRT_O_NONBLOCK) {
		__sprt_errno = ENOSYS;
		return -1;
	}

	platform::OpenInfo info(__flags, __mode);

	auto wpath = __MALLOCA_WSTRING(path);
	HANDLE h = CreateFileW(wpath, // file path
			info.dwDesiredAccess, // ← maps to O_RDONLY/O_WRONLY/O_RDWR
			info.dwShareMode, // ← no POSIX equivalent; Windows-specific
			nullptr, // security descriptor
			info.dwCreationDisposition, // ← maps to O_CREAT/O_EXCL/O_TRUNC
			info.dwFlagsAndAttributes, // ← maps to mode + O_APPEND/O_SYNC/FILE_FLAG_*
			nullptr);
	__sprt_freea(wpath);
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

	if ((__flags & __SPRT_O_DIRECTORY) != 0 || (__flags & __SPRT_O_NOFOLLOW) != 0) {
		BY_HANDLE_FILE_INFORMATION info;
		if (GetFileInformationByHandle(h, &info)) {
			if ((__flags & __SPRT_O_DIRECTORY) != 0
					&& (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
				CloseHandle(h);
				__sprt_errno = ENOTDIR;
				return -1;
			}
			if ((__flags & __SPRT_O_NOFOLLOW) != 0
					&& (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
				CloseHandle(h);
				__sprt_errno = ELOOP;
				return -1;
			}
		}
	}

	auto libc = __libc::get();
	auto newFd = libc->allocate_fd();
	if (newFd < 0) {
		CloseHandle(h);
		return newFd;
	}

	auto slot = libc->get_fd_slot(newFd);
	slot->handle = h;
	slot->flags = __flags;
	slot->mode = __mode;
	return newFd;
}

static int __open64(const char *path, int __flags, __SPRT_ID(mode_t) mode) __SPRT_NOEXCEPT {
	__SPRT_ID(mode_t) __mode = 0;

	return platform::performWithNativePath(path, [&](const char *target) {
		return __open(target, __flags, __mode); //
	}, -1);
}

static int __openat64(int __dir_fd, const char *path, int __flags,
		__SPRT_ID(mode_t) __mode) __SPRT_NOEXCEPT {
	int ret = 0;
	platform::openAtPath(__dir_fd, path, [&](const char *path, size_t pathLen) {
		ret = __open(path, __flags, __mode); //
	});
	return ret;
}

__SPRT_C_FUNC
int open(const char *path, int __flags, ...) __SPRT_NOEXCEPT {
	mode_t __mode = 0;

	if ((__flags & __SPRT_O_CREAT) || (__flags & __SPRT_O_TMPFILE) == __SPRT_O_TMPFILE) {
		__sprt_va_list ap;
		__sprt_va_start(ap, __flags);
		__mode = __sprt_va_arg(ap, mode_t);
		__sprt_va_end(ap);
	}

	return __open64(path, __flags, __mode);
}

__SPRT_C_FUNC
int openat(int __dir_fd, const char *path, int __flags, ...) __SPRT_NOEXCEPT {
	mode_t __mode = 0;

	if ((__flags & __SPRT_O_CREAT) || (__flags & __SPRT_O_TMPFILE) == __SPRT_O_TMPFILE) {
		__sprt_va_list ap;
		__sprt_va_start(ap, __flags);
		__mode = __sprt_va_arg(ap, mode_t);
		__sprt_va_end(ap);
	}

	return __openat64(__dir_fd, path, __flags, __mode);
}

} // namespace sprt
