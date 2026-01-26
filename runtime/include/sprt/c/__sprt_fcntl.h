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

#ifndef CORE_RUNTIME_INCLUDE_C___SPRT_FCNTL_H_
#define CORE_RUNTIME_INCLUDE_C___SPRT_FCNTL_H_

#include <sprt/c/cross/__sprt_fcntl.h>
#include <sprt/c/bits/atfile.h>
#include <sprt/c/bits/seek.h>
#include <sprt/c/bits/access.h>
#include <sprt/c/bits/__sprt_size_t.h>
#include <sprt/c/bits/__sprt_ssize_t.h>
#include <sprt/c/cross/__sprt_fstypes.h>

// clang-format off
#define __SPRT_O_SEARCH   __SPRT_O_PATH
#define __SPRT_O_EXEC     __SPRT_O_PATH
#define __SPRT_O_TTY_INIT 0

#define __SPRT_O_ACCMODE (03|__SPRT_O_SEARCH)
#define __SPRT_O_RDONLY  00
#define __SPRT_O_WRONLY  01
#define __SPRT_O_RDWR    02

#define __SPRT_F_OFD_GETLK 36
#define __SPRT_F_OFD_SETLK 37
#define __SPRT_F_OFD_SETLKW 38

#define __SPRT_F_DUPFD_CLOEXEC 1030

#define __SPRT_F_RDLCK 0
#define __SPRT_F_WRLCK 1
#define __SPRT_F_UNLCK 2
#define __SPRT_F_SETPIPE_SZ	1031
#define __SPRT_F_GETPIPE_SZ	1032
#define __SPRT_F_ADD_SEALS 1033
#define __SPRT_F_GET_SEALS 1034

#define __SPRT_F_SEAL_SEAL 0x0001
#define __SPRT_F_SEAL_SHRINK 0x0002
#define __SPRT_F_SEAL_GROW 0x0004
#define __SPRT_F_SEAL_WRITE 0x0008
#define __SPRT_F_SEAL_FUTURE_WRITE 0x0010

#define __SPRT_FD_CLOEXEC 1

#define __SPRT_POSIX_FADV_NORMAL     0
#define __SPRT_POSIX_FADV_RANDOM     1
#define __SPRT_POSIX_FADV_SEQUENTIAL 2
#define __SPRT_POSIX_FADV_WILLNEED   3
#define __SPRT_POSIX_FADV_DONTNEED   4
#define __SPRT_POSIX_FADV_NOREUSE    5
// clang-format on

__SPRT_BEGIN_DECL

SPRT_API int __SPRT_ID(fcntl)(int __fd, int __cmd, ...);

SPRT_API int __SPRT_ID(creat)(const char *__path, __SPRT_ID(mode_t) __mode);

SPRT_API int __SPRT_ID(open)(const char *__path, int __flags, ...);

SPRT_API int __SPRT_ID(openat)(int __dir_fd, const char *__path, int __flags, ...);

#if __SPRT_CONFIG_HAVE_FCNTL_SPLICE || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS
SPRT_API __SPRT_ID(ssize_t)
		__SPRT_ID(splice)(int __in_fd, __SPRT_ID(off_t) * __in_offset, int __out_fd,
				__SPRT_ID(off_t) * __out_offset, __SPRT_ID(size_t) __length, unsigned int __flags);
#endif

#if __SPRT_CONFIG_HAVE_FCNTL_TEE || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS
SPRT_API __SPRT_ID(ssize_t)
		__SPRT_ID(tee)(int __in_fd, int __out_fd, __SPRT_ID(size_t) __length, unsigned int __flags);
#endif

#if __SPRT_CONFIG_HAVE_FCNTL_FALLOCATE || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS
SPRT_API int __SPRT_ID(
		fallocate)(int __fd, int __mode, __SPRT_ID(off_t) __offset, __SPRT_ID(off_t) __length);

SPRT_API int __SPRT_ID(
		posix_fallocate)(int __fd, __SPRT_ID(off_t) __offset, __SPRT_ID(off_t) __length);
#endif

#if __SPRT_CONFIG_HAVE_FCNTL_FADVICE || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS
SPRT_API int __SPRT_ID(posix_fadvise)(int __fd, __SPRT_ID(off_t) __offset,
		__SPRT_ID(off_t) __length, int __advice);
#endif

#if __SPRT_CONFIG_HAVE_FCNTL_READAHEAD || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS
SPRT_API __SPRT_ID(ssize_t)
		__SPRT_ID(readahead)(int __fd, __SPRT_ID(off_t) __offset, __SPRT_ID(size_t) __length);
#endif

#if __SPRT_CONFIG_HAVE_FCNTL_SYNC_FILE_RANGE || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS
SPRT_API int __SPRT_ID(sync_file_range)(int __fd, __SPRT_ID(off_t) __offset,
		__SPRT_ID(off_t) __length, unsigned int __flags);
#endif

__SPRT_END_DECL

#endif // CORE_RUNTIME_INCLUDE_C___SPRT_FCNTL_H_
