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
#include "io.h"
#include "stdlib.h"
#include "../../include/__impl_libc.h"

#include "specific.h"

#include <sprt/c/__sprt_fcntl.h>
#include <sprt/c/__sprt_unistd.h>

#include <sprt/wrappers/windows/file_api.h>

namespace sprt {

static int __wopen(const wchar_t *wpath, int __flags, __SPRT_ID(mode_t) __mode) {
	if ((__flags & __SPRT_O_TMPFILE) == __SPRT_O_TMPFILE
			|| (__flags & __SPRT_O_NONBLOCK) == __SPRT_O_NONBLOCK) {
		__sprt_errno = ENOSYS;
		return -1;
	}

	platform::OpenInfo info(__flags, __mode);

	HANDLE h = CreateFileW(wpath, // file path
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
	auto newFd = libc->create_fd(h, &libc->fdFileOps, __flags, __mode);
	if (newFd < 0) {
		CloseHandle(h);
	}
	return newFd;
}

static int __open(const char *path, int __flags, __SPRT_ID(mode_t) __mode) {
	auto wpath = __MALLOCA_WSTRING(path);
	auto ret = __wopen(wpath, __flags, __mode);
	__sprt_freea(wpath);
	return ret;
}

static int __open64(const char *path, int __flags, __SPRT_ID(mode_t) __mode) __SPRT_NOEXCEPT {
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

static int __shareModeToFlags(int __sh) {
	int __flags = 0;
	switch (__sh) {
	case _SH_DENYRW: __flags |= __SPRT_O_SH_DENYRW; break;
	case _SH_DENYWR: __flags |= __SPRT_O_SH_DENYWR; break;
	case _SH_DENYRD: __flags |= __SPRT_O_SH_DENYRD; break;
	case _SH_DENYNO: __flags |= __SPRT_O_SH_DENYNO; break;
	}
	return __flags;
}

__SPRT_C_FUNC
int _sopen_s(int *pfh, const char *__path, int __flags, int __sh, ...) __SPRT_NOEXCEPT {
	if (!pfh || !__path) {
		if (*pfh) {
			*pfh = -1;
		}
		__sprt_errno = EINVAL;
		return EINVAL;
	}

	mode_t __mode = 0;

	if ((__flags & __SPRT_O_CREAT) || (__flags & __SPRT_O_TMPFILE) == __SPRT_O_TMPFILE) {
		__sprt_va_list ap;
		__sprt_va_start(ap, __sh);
		__mode = __sprt_va_arg(ap, mode_t);
		__sprt_va_end(ap);
	}

	__flags |= __shareModeToFlags(__sh);

	*pfh = __open64(__path, __flags, __mode);
	if (*pfh >= 0) {
		return 0;
	}
	return __sprt_errno;
}

__SPRT_C_FUNC
int _sopen(const char *__path, int __flags, int __sh, ...) __SPRT_NOEXCEPT {
	mode_t __mode = 0;

	if ((__flags & __SPRT_O_CREAT) || (__flags & __SPRT_O_TMPFILE) == __SPRT_O_TMPFILE) {
		__sprt_va_list ap;
		__sprt_va_start(ap, __sh);
		__mode = __sprt_va_arg(ap, mode_t);
		__sprt_va_end(ap);
	}

	__flags |= __shareModeToFlags(__sh);

	return __open64(__path, __flags, __mode);
}

__SPRT_C_FUNC
int _wsopen_s(int *pfh, const wchar_t *__path, int __flags, int __sh, ...) __SPRT_NOEXCEPT {
	if (!pfh || !__path) {
		if (*pfh) {
			*pfh = -1;
		}
		__sprt_errno = EINVAL;
		return EINVAL;
	}

	mode_t __mode = 0;

	if ((__flags & __SPRT_O_CREAT) || (__flags & __SPRT_O_TMPFILE) == __SPRT_O_TMPFILE) {
		__sprt_va_list ap;
		__sprt_va_start(ap, __sh);
		__mode = __sprt_va_arg(ap, mode_t);
		__sprt_va_end(ap);
	}

	__flags |= __shareModeToFlags(__sh);

	*pfh = platform::performWithNativePath(__path, [&](const wchar_t *target) {
		return __wopen(target, __flags, __mode); //
	}, -1);

	if (*pfh >= 0) {
		return 0;
	}
	return __sprt_errno;
}

__SPRT_C_FUNC
int _wsopen(const wchar_t *wpath, int __flags, int __sh, ...) __SPRT_NOEXCEPT {
	if (!wpath) {
		__sprt_errno = EINVAL;
		return -1;
	}

	mode_t __mode = 0;
	if ((__flags & __SPRT_O_CREAT) || (__flags & __SPRT_O_TMPFILE) == __SPRT_O_TMPFILE) {
		__sprt_va_list ap;
		__sprt_va_start(ap, __sh);
		__mode = __sprt_va_arg(ap, mode_t);
		__sprt_va_end(ap);
	}

	__flags |= __shareModeToFlags(__sh);

	return platform::performWithNativePath(wpath, [&](const wchar_t *target) {
		return __wopen(target, __flags, __mode); //
	}, -1);
}

__SPRT_C_FUNC int _wopen(const wchar_t *wpath, int __flags, ...) __SPRT_NOEXCEPT {
	if (!wpath) {
		__sprt_errno = EINVAL;
		return -1;
	}

	mode_t __mode = 0;
	if ((__flags & __SPRT_O_CREAT) || (__flags & __SPRT_O_TMPFILE) == __SPRT_O_TMPFILE) {
		__sprt_va_list ap;
		__sprt_va_start(ap, __flags);
		__mode = __sprt_va_arg(ap, mode_t);
		__sprt_va_end(ap);
	}

	return platform::performWithNativePath(wpath, [&](const wchar_t *target) {
		return __wopen(target, __flags, __mode); //
	}, -1);
}

} // namespace sprt
