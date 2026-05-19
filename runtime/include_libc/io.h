/**
Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#ifndef CORE_RUNTIME_INCLUDE_LIBC_IO_H_
#define CORE_RUNTIME_INCLUDE_LIBC_IO_H_

/*
	This is minimal implementation of Windows <io.h>
*/

#include <sprt/c/__sprt_stdio.h>
#include <sprt/c/__sprt_fcntl.h>
#include <sprt/c/sys/__sprt_stat.h>
#include <sprt/c/__sprt_wchar.h>
#include <sprt/c/__sprt_unistd.h>
#include <sprt/c/__sprt_errno.h>

#include <unistd.h>

#define _SH_DENYRW      0x10    // deny read/write mode
#define _SH_DENYWR      0x20    // deny write mode
#define _SH_DENYRD      0x30    // deny read mode
#define _SH_DENYNO      0x40    // deny none mode
#define _SH_SECURE      0x80    // secure mode

#define _S_IREAD __SPRT_S_IRUSR
#define _S_IWRITE __SPRT_S_IWUSR

#define _O_BINARY 0
#define O_BINARY _O_BINARY

#define st_atime st_atim.tv_sec
#define st_mtime st_mtim.tv_sec

typedef __SPRT_ID(off_t) off_t;

__SPRT_BEGIN_DECL

__SPRT_C_FUNC int _wopen(const wchar_t *, int, ...) __SPRT_NOEXCEPT;

__SPRT_C_FUNC int setmode(int, int) __SPRT_NOEXCEPT;
__SPRT_C_FUNC int _setmode(int, int) __SPRT_NOEXCEPT;

SPRT_UMBRELLA_FUNC
off_t _lseeki64(int __fd, off_t __offset, int __whence) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_lseek(__fd, __offset, __whence);
}
#endif

SPRT_UMBRELLA_FUNC
int _sopen_s(int *handle, const char *path, int flags, int, int mode) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	if (!handle) {
		return EINVAL;
	}
	*handle = __sprt_open(path, flags, mode);
	if (handle < 0) {
		return __sprt_errno;
	}
	return 0;
}
#endif

SPRT_UMBRELLA_FUNC
__SPRT_ID(FILE)
*_fdopen(int fd, const char *mode) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_fdopen(fd, mode);
}
#endif

SPRT_UMBRELLA_FUNC
off_t _fileno(__SPRT_ID(FILE) * f) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_fileno(f);
}
#endif

SPRT_UMBRELLA_FUNC
int fopen_s(__SPRT_ID(FILE) * *f, const char *path, const char *mode) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	if (!f) {
		return EINVAL;
	}
	*f = __sprt_fopen(path, mode);
	if (!*f) {
		return __sprt_errno;
	}
	return 0;
}
#endif

SPRT_UMBRELLA_FUNC
int _fseeki64(__SPRT_ID(FILE) * file, __SPRT_ID(off_t) pos, int when) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_fseek(file, pos, when);
}
#endif

SPRT_UMBRELLA_FUNC
__SPRT_ID(off_t)
_ftelli64(__SPRT_ID(FILE) * file) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_ftell(file);
}
#endif

SPRT_UMBRELLA_FUNC
int _unlink(const char *__name) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_unlink(__name);
}
#endif

SPRT_UMBRELLA_FUNC
int _isatty(int __fd) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_isatty(__fd);
}
#endif

__SPRT_END_DECL

#endif // CORE_RUNTIME_INCLUDE_LIBC_IO_H_
