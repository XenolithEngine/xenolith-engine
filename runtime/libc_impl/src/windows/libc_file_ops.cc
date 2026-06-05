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

#include "../../include/__impl_libc.h"
#include "unistd.h"
#include "sys/ioctl.h"
#include "sys/mman.h"
#include "specific.h"

#include <sprt/c/__sprt_fcntl.h>
#include <sprt/c/sys/__sprt_ioctl.h>
#include <sprt/c/sys/__sprt_stat.h>
#include <sprt/c/bits/__sprt_ssize_t.h>
#include <sprt/wrappers/windows/basic_api.h>
#include <sprt/wrappers/windows/file_api.h>
#include <sprt/wrappers/windows/process_api.h>
#include <sprt/wrappers/windows/security_api.h>

namespace sprt {

static ssize_t __file_read(struct __fd_slot *fp, void *buf, size_t nbytes, off64_t *offset,
		uint32_t flags) {
	if (!fp->handle) {
		__sprt_errno = EBADF;
		return -1;
	}

	LARGE_INTEGER orig_pos;
	if (offset) {
		if (!SetFilePointerEx(fp->handle, LARGE_INTEGER{{0, 0}}, &orig_pos, FILE_CURRENT)) {
			__sprt_errno = platform::lastErrorToErrno(GetLastError());
			return -1;
		}

		// Seek to target
		LARGE_INTEGER target = {.QuadPart = *offset};
		if (!SetFilePointerEx(fp->handle, target, NULL, FILE_BEGIN)) {
			__sprt_errno = platform::lastErrorToErrno(GetLastError());
			return -1;
		}
	}

	// ReadFile's count is a DWORD; a size_t request may exceed 4 GiB, so clamp
	// each chunk to a DWORD-sized maximum to avoid silent truncation.
	const DWORD CHUNK_MAX = 0xFFFF0000u; // page-aligned cap below 4 GiB
	DWORD chunk = (nbytes > CHUNK_MAX) ? CHUNK_MAX : (DWORD)nbytes;

	DWORD read;
	if (!ReadFile(fp->handle, buf, chunk, &read, NULL)) {
		__sprt_errno = platform::lastErrorToErrno(GetLastError());
		// pread must not change the file offset, even on error
		if (offset) {
			SetFilePointerEx(fp->handle, orig_pos, NULL, FILE_BEGIN);
		}
		return -1;
	}

	if (offset) {
		SetFilePointerEx(fp->handle, orig_pos, NULL, FILE_BEGIN);
	}

	return (ssize_t)read;
}

static ssize_t __file_write(struct __fd_slot *fp, const void *buf, size_t nbytes, off64_t *offset,
		uint32_t flags) {
	if (!fp->handle) {
		__sprt_errno = EBADF;
		return -1;
	}

	LARGE_INTEGER orig_pos;
	if (offset) {
		if (!SetFilePointerEx(fp->handle, LARGE_INTEGER{{0, 0}}, &orig_pos, FILE_CURRENT)) {
			__sprt_errno = platform::lastErrorToErrno(GetLastError());
			return -1;
		}

		// Seek to target
		LARGE_INTEGER target = {.QuadPart = *offset};
		if (!SetFilePointerEx(fp->handle, target, NULL, FILE_BEGIN)) {
			__sprt_errno = platform::lastErrorToErrno(GetLastError());
			return -1;
		}
	}

	// WriteFile's count is a DWORD; a size_t request may exceed 4 GiB, so clamp
	// each chunk to a DWORD-sized maximum to avoid silent truncation.
	const DWORD CHUNK_MAX = 0xFFFF0000u; // page-aligned cap below 4 GiB
	DWORD chunk = (nbytes > CHUNK_MAX) ? CHUNK_MAX : (DWORD)nbytes;

	DWORD written;
	if (!WriteFile(fp->handle, buf, chunk, &written, NULL)) {
		__sprt_errno = platform::lastErrorToErrno(GetLastError());
		// pwrite must not change the file offset, even on error
		if (offset) {
			SetFilePointerEx(fp->handle, orig_pos, NULL, FILE_BEGIN);
		}
		return -1;
	}

	if (offset) {
		SetFilePointerEx(fp->handle, orig_pos, NULL, FILE_BEGIN);
	}

	return (ssize_t)written;
}

static int __file_close(__fd_slot *fp) {
	if (!fp->handle) {
		__sprt_errno = EBADF;
		return -1;
	}

	if (CloseHandle(fp->handle)) {
		return 0;
	}

	__sprt_errno = platform::lastErrorToErrno(GetLastError());
	return -1;
}

static int __file_dup(__fd_slot *fp, int *target, uint32_t flags) {
	if (!fp->handle) {
		__sprt_errno = EBADF;
		return -1;
	}

	if (target) {
		if (*target < 0 || *target >= MAX_FDS) {
			__sprt_errno = EBADF;
			return -1;
		}
	}

	// FD_CLOEXEC <=> a non-inheritable handle. Duplicate as non-inheritable
	// (close-on-exec) by default — matching freshly-opened fds — and only as an
	// inheritable handle when FD_CLOEXEC is explicitly cleared (dup3 without
	// O_CLOEXEC).
	BOOL inherit = (flags & __SPRT_FD_CLOEXEC) ? FALSE : TRUE;

	HANDLE newHandle = nullptr;
	BOOL result = DuplicateHandle(GetCurrentProcess(), fp->handle, GetCurrentProcess(), &newHandle,
			0L, inherit, DUPLICATE_SAME_ACCESS);
	if (!result) {
		__sprt_errno = platform::lastErrorToErrno(GetLastError());
		return -1;
	}

	auto libc = __libc::get();
	if (!target) {
		auto fd = libc->create_fd(newHandle, &libc->fdFileOps, fp->flags, fp->mode);
		if (fd < 0) {
			CloseHandle(newHandle);
			return -1;
		}

		return fd;
	} else {
		unique_lock lock(libc->fdMutex);
		libc->fdDispatch->bits.set(*target);
		auto fdSlot = libc->get_fd_slot(*target);
		// dup2 semantics: if the target descriptor is already open, close it
		// first (silently), then install the duplicate.
		if (fdSlot->handle && fdSlot->ops && fdSlot->ops->fo_close) {
			fdSlot->ops->fo_close(fdSlot);
		}

		*fdSlot = __fd_slot{newHandle, &libc->fdFileOps, fp->flags, fp->mode};
		return *target;
	}
}

static HANDLE __file_reopen(HANDLE h, int __flags, __SPRT_ID(mode_t) __mode) {
	if ((__flags & __SPRT_O_TMPFILE) || (__flags & __SPRT_O_NONBLOCK)) {
		__sprt_errno = ENOSYS;
		return nullptr;
	}

	if ((__flags & __SPRT_O_PATH) || (__flags & __SPRT_O_DIRECTORY)) {
		__sprt_errno = EINVAL;
		return nullptr;
	}

	platform::OpenInfo info(__flags, __mode);

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

static int __file_ioctl(__fd_slot *fp, int fd, int cmd, intptr_t arg, __fd_ctl_mode mode) {
	if (!fp->handle) {
		__sprt_errno = EBADF;
		return -1;
	}

	if (mode == __fd_ctl_mode::fnctl) {
		switch (cmd) {
		// fds are close-on-exec (non-inheritable) by default on this platform.
		case __SPRT_F_DUPFD: return __file_dup(fp, nullptr, __SPRT_FD_CLOEXEC); break;
		case __SPRT_F_DUPFD_CLOEXEC: return __file_dup(fp, nullptr, __SPRT_FD_CLOEXEC); break;
		case __SPRT_F_GETFD: {
			DWORD flags = 0;
			if (GetHandleInformation(fp->handle, &flags)) {
				int ret = 0;
				// FD_CLOEXEC <=> the handle is NOT inheritable.
				if (!(flags & HANDLE_FLAG_INHERIT)) {
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
			// FD_CLOEXEC <=> non-inheritable; its absence <=> inheritable. The
			// caller clears FD_CLOEXEC here to obtain an inheritable handle.
			DWORD flagsToSet = HANDLE_FLAG_INHERIT;
			if (arg & __SPRT_FD_CLOEXEC) {
				flagsToSet = 0;
				arg &= ~(unsigned long)__SPRT_FD_CLOEXEC;
			}
			if (arg != 0) {
				// unknown flags, fail with EINVAL
				__sprt_errno = EINVAL;
				return -1;
			}
			if (SetHandleInformation(fp->handle, flagsMask, flagsToSet)) {
				return 0;
			} else {
				__sprt_errno = platform::lastErrorToErrno(GetLastError());
				return -1;
			}
			break;
		}
		case __SPRT_F_GETFL: {
			return int(fp->flags);
		}
		case __SPRT_F_SETFL: {
			auto newHandle = __file_reopen(fp->handle, arg, fp->mode);
			if (!newHandle) {
				return -1;
			}

			fp->handle = newHandle;
			fp->flags = arg;
			return 0;
		}
		}
	} else if (mode == __fd_ctl_mode::ioctl) {
		switch (cmd) {
		case __SPRT_TIOCGWINSZ: {
			HANDLE hConsole = nullptr;
			CONSOLE_SCREEN_BUFFER_INFO csbi;

			switch (fd) {
			case 0: hConsole = GetStdHandle(STD_INPUT_HANDLE); break;
			case 1: hConsole = GetStdHandle(STD_OUTPUT_HANDLE); break;
			case 2: hConsole = GetStdHandle(STD_ERROR_HANDLE); break;
			default: __sprt_errno = EINVAL; return -1;
			}
			// 1. Get handle to current console
			if (!hConsole || hConsole == INVALID_HANDLE_VALUE) {
				__sprt_errno = EINVAL;
				return -1;
			}

			// 2. Get console screen buffer information
			if (!GetConsoleScreenBufferInfo(hConsole, &csbi)) {
				return -1;
			}

			// 3. Fill in the winsize structure
			auto ws = reinterpret_cast<struct winsize *>(arg);
			ws->ws_row = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
			ws->ws_col = csbi.srWindow.Right - csbi.srWindow.Left + 1;
			return 0;
			break;
		}

		case __SPRT_TIOCSWINSZ: break;
		}
	}
	__sprt_errno = EINVAL;
	return -1;
}

static ssize_t __file_readv(__fd_slot *fp, const struct iovec *iov, int iovcnt) {
	if (!fp->handle) {
		__sprt_errno = EBADF;
		return -1;
	}

	if (iov == nullptr || iovcnt <= 0) {
		if (iov == nullptr && iovcnt == 0) {
			return 0;
		}
		__sprt_errno = EINVAL;
		return -1;
	}

	// ReadFile's count is a DWORD; an iov_len may exceed 4 GiB, so drain each
	// segment in DWORD-sized chunks to avoid silent truncation.
	const DWORD CHUNK_MAX = 0xFFFF0000u; // page-aligned cap below 4 GiB
	ssize_t totalBytes = 0;
	for (int i = 0; i < iovcnt; i++) {
		size_t remaining = iov[i].iov_len;
		char *base = (char *)iov[i].iov_base;
		while (remaining > 0) {
			DWORD chunk = (remaining > CHUNK_MAX) ? CHUNK_MAX : (DWORD)remaining;
			DWORD readBytes = 0;
			BOOL result = ReadFile(fp->handle, base, chunk, &readBytes, nullptr);
			totalBytes += readBytes;
			if (!result) {
				__sprt_errno = platform::lastErrorToErrno(GetLastError());
				return -1;
			}
			remaining -= readBytes;
			base += readBytes;
			if (readBytes < chunk) {
				// short read (EOF): stop reading further
				return totalBytes;
			}
		}
	}

	return totalBytes;
}

static ssize_t __file_writev(__fd_slot *fp, const struct iovec *iov, int iovcnt) {
	if (!fp->handle) {
		__sprt_errno = EBADF;
		return -1;
	}

	if (iov == nullptr || iovcnt <= 0) {
		if (iov == nullptr && iovcnt == 0) {
			return 0;
		}
		__sprt_errno = EINVAL;
		return -1;
	}

	// WriteFile's count is a DWORD; an iov_len may exceed 4 GiB, so drain each
	// segment in DWORD-sized chunks to avoid silent truncation.
	const DWORD CHUNK_MAX = 0xFFFF0000u; // page-aligned cap below 4 GiB
	ssize_t totalWritten = 0;
	for (int i = 0; i < iovcnt; i++) {
		size_t remaining = iov[i].iov_len;
		const char *base = (const char *)iov[i].iov_base;
		while (remaining > 0) {
			DWORD chunk = (remaining > CHUNK_MAX) ? CHUNK_MAX : (DWORD)remaining;
			DWORD written = 0;
			BOOL result = WriteFile(fp->handle, base, chunk, &written, nullptr);
			totalWritten += written;
			if (!result) {
				__sprt_errno = platform::lastErrorToErrno(GetLastError());
				return -1;
			}
			remaining -= written;
			base += written;
			if (written < chunk) {
				// short write: stop writing further
				return totalWritten;
			}
		}
	}

	return totalWritten;
}

static off_t __file_seek(__fd_slot *fp, off_t off, int whence) {
	if (!fp->handle) {
		__sprt_errno = EBADF;
		return -1;
	}

	LARGE_INTEGER seek_pos = {.QuadPart = off};

	int winWhence = 0;
	switch (whence) {
	case __SPRT_SEEK_SET: winWhence = FILE_BEGIN; break;
	case __SPRT_SEEK_CUR: winWhence = FILE_CURRENT; break;
	case __SPRT_SEEK_END: winWhence = FILE_END; break;
	}

	LARGE_INTEGER new_pos = {.QuadPart = 0};
	if (!SetFilePointerEx(fp->handle, seek_pos, &new_pos, winWhence)) {
		__sprt_errno = platform::lastErrorToErrno(GetLastError());
		return -1;
	}

	return new_pos.QuadPart;
}

static int __file_stat(__fd_slot *fp, struct __SPRT_STAT_NAME *__stat) {
	if (!fp->handle) {
		__sprt_errno = EBADF;
		return -1;
	}

	return platform::hstat(fp->handle, __stat);
}

static int __file_chmod(__fd_slot *fp, mode_t __mode) {
	if (!fp->handle) {
		__sprt_errno = EBADF;
		return -1;
	}

	if (fp->mode == __mode) {
		return 0;
	}

	FILE_BASIC_INFO fbi;
	if (!GetFileInformationByHandleEx(fp->handle, FileBasicInfo, &fbi, sizeof(FILE_BASIC_INFO))) {
		return -1;
	}

	if (__mode & __SPRT_S_IWUSR) {
		fbi.FileAttributes &= ~FILE_ATTRIBUTE_READONLY;
	} else {
		fbi.FileAttributes |= FILE_ATTRIBUTE_READONLY;
	}

	if (!SetFileInformationByHandle(fp->handle, FileBasicInfo, &fbi, sizeof(FILE_BASIC_INFO))) {
		__sprt_errno = platform::lastErrorToErrno(GetLastError());
		return -1;
	}

	fp->mode = __mode;

	return 0;
}

static int __flie_utimens(__fd_slot *fp, const struct __SPRT_TIMESPEC_NAME *times) {
	if (!fp->handle) {
		__sprt_errno = EBADF;
		return -1;
	}

	return platform::hutimens(fp->handle, times);
}

static void *__file_mmap(__fd_slot *fp, void *addr, size_t length, int prot, int flags,
		off_t offset) {
	auto flProtect = platform::__getProtectFlags(prot);

	if (flProtect == PAGE_NOACCESS) {
		__sprt_errno = EINVAL;
		return __SPRT_MAP_FAILED;
	}

	if (flags & __SPRT_MAP_HUGETLB) {
		flProtect |= SEC_LARGE_PAGES;
	}

	LARGE_INTEGER liOffset = {
		.QuadPart = offset,
	};

	HANDLE hMap = CreateFileMappingW(fp->handle, NULL, flProtect, liOffset.HighPart,
			liOffset.LowPart, NULL);
	if (!hMap) {
		__sprt_errno = platform::lastErrorToErrno(GetLastError());
		return __SPRT_MAP_FAILED;
	}

	DWORD dwDesiredAccess = FILE_MAP_READ;
	if (prot & __SPRT_PROT_WRITE) {
		dwDesiredAccess |= FILE_MAP_WRITE;
	}
	if (prot & __SPRT_PROT_EXEC) {
		dwDesiredAccess |= FILE_MAP_EXECUTE;
	}

	if (flags & __SPRT_MAP_PRIVATE) {
		dwDesiredAccess |= FILE_MAP_COPY;
	}

	if (flags & __SPRT_MAP_HUGETLB) {
		dwDesiredAccess |= FILE_MAP_LARGE_PAGES;
	}

	void *pMap = MapViewOfFile(hMap, dwDesiredAccess, liOffset.HighPart, liOffset.LowPart, length);

	CloseHandle(hMap); // already closed here; the view keeps its own ref

	if (!pMap) {
		__sprt_errno = platform::lastErrorToErrno(GetLastError());
		return __SPRT_MAP_FAILED;
	}

	return pMap;
}

static int __file_munmap(__fd_slot *fp, void *addr, size_t length) {
	BOOL ok = UnmapViewOfFile(addr);
	if (!ok) {
		__sprt_errno = platform::lastErrorToErrno(GetLastError());
		return -1;
	}
	return 0;
}

static int __file_msync(__fd_slot *fp, void *addr, size_t length, int flags) {
	if (!addr || length == 0) {
		__sprt_errno = EINVAL;
		return -1;
	}

	// MS_ASYNC: Windows schedules automatically (noop)
	if (flags == __SPRT_MS_ASYNC) {
		return 0;
	}

	// MS_SYNC or MS_INVALIDATE: flush dirty pages
	if (flags != __SPRT_MS_SYNC && flags != __SPRT_MS_INVALIDATE) {
		__sprt_errno = EINVAL;
		return -1;
	}

	BOOL ok = FlushViewOfFile(addr, length);
	if (!ok) {
		__sprt_errno = platform::lastErrorToErrno(GetLastError());
		return -1;
	}

	if (fp->handle != INVALID_HANDLE_VALUE) {
		FlushFileBuffers(fp->handle);
		return 0;
	}

	__sprt_errno = EBADF;
	return -1;
}

void *__file_mmap_anon(void *addr, size_t length, int prot, int flags, off_t offset) {
	auto flProtect = platform::__getProtectFlags(prot);

	auto retAddr = VirtualAlloc(addr, length, MEM_COMMIT | MEM_RESERVE, flProtect);
	if (retAddr) {
		return retAddr;
	} else {
		__sprt_errno = ENOMEM;
		return __SPRT_MAP_FAILED;
	}
}

int __file_munmap_anon(void *addr, size_t length) {
	BOOL ok = VirtualFree(addr, 0, MEM_RELEASE);
	if (!ok) {
		__sprt_errno = platform::lastErrorToErrno(GetLastError());
		return -1;
	}
	return 0;
}

void __libc::load_file_fd_ops(__fd_ops *ops) {
	ops->mask = __fd_ops_mask::opendir;
	ops->fo_read = &__file_read;
	ops->fo_write = &__file_write;
	ops->fo_close = &__file_close;
	ops->fo_dup = &__file_dup;
	ops->fo_ioctl = &__file_ioctl;
	ops->fo_readv = &__file_readv;
	ops->fo_writev = &__file_writev;
	ops->fo_seek = &__file_seek;
	ops->fo_stat = &__file_stat;
	ops->fo_chmod = &__file_chmod;
	ops->fo_utimens = &__flie_utimens;
	ops->fo_mmap = &__file_mmap;
	ops->fo_munmap = &__file_munmap;
	ops->fo_msync = &__file_msync;
}

} // namespace sprt
