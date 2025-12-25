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

#ifndef CORE_RUNTIME_INCLUDE_LIBC_FCNTL_H_
#define CORE_RUNTIME_INCLUDE_LIBC_FCNTL_H_

#ifdef __SPRT_BUILD

#include_next <fcntl.h>

#else

#include <c/__sprt_fcntl.h>
#include <c/__sprt_stdarg.h>

#ifndef SEEK_SET
#define SEEK_SET __SPRT_SEEK_SET
#define SEEK_CUR __SPRT_SEEK_CUR
#define SEEK_END __SPRT_SEEK_END
#endif

#define O_CREAT __SPRT_O_CREAT
#define O_EXCL __SPRT_O_EXCL
#define O_NOCTTY __SPRT_O_NOCTTY
#define O_TRUNC __SPRT_O_TRUNC
#define O_APPEND __SPRT_O_APPEND
#define O_NONBLOCK __SPRT_O_NONBLOCK
#define O_DSYNC __SPRT_O_DSYNC
#define O_SYNC __SPRT_O_SYNC
#define O_RSYNC __SPRT_O_RSYNC
#define O_DIRECTORY __SPRT_O_DIRECTORY
#define O_NOFOLLOW __SPRT_O_NOFOLLOW
#define O_CLOEXEC __SPRT_O_CLOEXEC

#define O_ASYNC __SPRT_O_ASYNC
#define O_DIRECT __SPRT_O_DIRECT
#define O_LARGEFILE __SPRT_O_LARGEFILE
#define O_NOATIME __SPRT_O_NOATIME
#define O_PATH __SPRT_O_PATH
#define O_TMPFILE __SPRT_O_TMPFILE
#define O_NDELAY __SPRT_O_NDELAY

#define F_DUPFD __SPRT_F_DUPFD
#define F_GETFD __SPRT_F_GETFD
#define F_SETFD __SPRT_F_SETFD
#define F_GETFL __SPRT_F_GETFL
#define F_SETFL __SPRT_F_SETFL

#define F_SETOWN __SPRT_F_SETOWN
#define F_GETOWN __SPRT_F_GETOWN
#define F_SETSIG __SPRT_F_SETSIG
#define F_GETSIG __SPRT_F_GETSIG

#define F_GETLK __SPRT_F_GETLK
#define F_SETLK __SPRT_F_SETLK
#define F_SETLKW __SPRT_F_SETLKW

#define F_SETOWN_EX __SPRT_F_SETOWN_EX
#define F_GETOWN_EX __SPRT_F_GETOWN_EX

#define F_GETOWNER_UIDS __SPRT_F_GETOWNER_UIDS

#define O_SEARCH __SPRT_O_SEARCH
#define O_EXEC __SPRT_O_EXEC
#define O_TTY_INIT __SPRT_O_TTY_INIT

#define O_ACCMODE __SPRT_O_ACCMODE
#define O_RDONLY __SPRT_O_RDONLY
#define O_WRONLY __SPRT_O_WRONLY
#define O_RDWR __SPRT_O_RDWR

#define F_OFD_GETLK __SPRT_F_OFD_GETLK
#define F_OFD_SETLK __SPRT_F_OFD_SETLK
#define F_OFD_SETLKW __SPRT_F_OFD_SETLKW

#define F_DUPFD_CLOEXEC __SPRT_F_DUPFD_CLOEXEC

#define F_RDLCK __SPRT_F_RDLCK
#define F_WRLCK __SPRT_F_WRLCK
#define F_UNLCK __SPRT_F_UNLCK

#define FD_CLOEXEC __SPRT_FD_CLOEXEC

#define POSIX_FADV_NORMAL __SPRT_POSIX_FADV_NORMAL
#define POSIX_FADV_RANDOM __SPRT_POSIX_FADV_RANDOM
#define POSIX_FADV_SEQUENTIAL __SPRT_POSIX_FADV_SEQUENTIAL
#define POSIX_FADV_WILLNEED __SPRT_POSIX_FADV_WILLNEED
#define POSIX_FADV_DONTNEED __SPRT_POSIX_FADV_DONTNEED
#define POSIX_FADV_NOREUSE __SPRT_POSIX_FADV_NOREUSE

__SPRT_BEGIN_DECL

typedef __SPRT_ID(size_t) size_t;
typedef __SPRT_ID(ssize_t) ssize_t;
typedef __SPRT_ID(off_t) off_t;
typedef __SPRT_ID(mode_t) mode_t;

SPRT_FORCEINLINE inline int fcntl(int __fd, int __cmd, ...) {
	unsigned long arg;
	__sprt_va_list ap;
	__sprt_va_start(ap, __cmd);
	arg = __sprt_va_arg(ap, unsigned long);
	__sprt_va_end(ap);

	return __sprt_fcntl(__fd, __cmd, arg);
}

SPRT_FORCEINLINE inline int creat(const char *path, mode_t __mode) {
	return __sprt_creat(path, __mode);
}

SPRT_FORCEINLINE inline int open(const char *path, int __flags, ...) {
	mode_t __mode = 0;

	if ((__flags & __SPRT_O_CREAT) || (__flags & __SPRT_O_TMPFILE) == __SPRT_O_TMPFILE) {
		__sprt_va_list ap;
		__sprt_va_start(ap, __flags);
		__mode = __sprt_va_arg(ap, mode_t);
		__sprt_va_end(ap);
	}

	return __sprt_open(path, __mode);
}

SPRT_FORCEINLINE inline int openat(int __dir_fd, const char *path, int __flags, ...) {
	mode_t __mode = 0;

	if ((__flags & __SPRT_O_CREAT) || (__flags & __SPRT_O_TMPFILE) == __SPRT_O_TMPFILE) {
		__sprt_va_list ap;
		__sprt_va_start(ap, __flags);
		__mode = __sprt_va_arg(ap, mode_t);
		__sprt_va_end(ap);
	}

	return __sprt_openat(__dir_fd, path, __mode);
}

SPRT_FORCEINLINE inline ssize_t splice(int __in_fd, off_t *__in_offset, int __out_fd,
		off_t *__out_offset, size_t __length, unsigned int __flags) {
	return __sprt_splice(__in_fd, __in_offset, __out_fd, __out_offset, __length, __flags);
}

SPRT_FORCEINLINE inline ssize_t tee(int __in_fd, int __out_fd, size_t __length,
		unsigned int __flags) {
	return __sprt_tee(__in_fd, __out_fd, __length, __flags);
}

SPRT_FORCEINLINE inline int fallocate(int __fd, int __mode, off_t __offset, off_t __length) {
	return __sprt_fallocate(__fd, __mode, __offset, __length);
}

SPRT_FORCEINLINE inline int posix_fadvise(int __fd, off_t __offset, off_t __length, int __advice) {
	return __sprt_posix_fadvise(__fd, __offset, __length, __advice);
}

SPRT_FORCEINLINE inline int posix_fallocate(int __fd, off_t __offset, off_t __length) {
	return __sprt_posix_fallocate(__fd, __offset, __length);
}

SPRT_FORCEINLINE inline ssize_t readahead(int __fd, off_t __offset, size_t __length) {
	return __sprt_readahead(__fd, __offset, __length);
}

SPRT_FORCEINLINE inline int sync_file_range(int __fd, off_t __offset, off_t __length,
		unsigned int __flags) {
	return __sprt_sync_file_range(__fd, __offset, __length, __flags);
}

__SPRT_END_DECL

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_FCNTL_H_
