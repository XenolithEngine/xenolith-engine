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

#define __SPRT_BUILD

#include "private/SPRTSpecific.h"

#include <sprt/c/__sprt_errno.h>
#include <windows.h>

/*
	AI-Generated
*/

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

namespace sprt::platform {

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
