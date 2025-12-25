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

#ifndef CORE_RUNTIME_INCLUDE_LIBC_SYS_STAT_H_
#define CORE_RUNTIME_INCLUDE_LIBC_SYS_STAT_H_

#ifdef __SPRT_BUILD

#include_next <sys/stat.h>

#else

#include <c/sys/__sprt_stat.h>

#ifndef S_IFMT
#define S_IFMT __SPRT_S_IFMT

#define S_IFDIR __SPRT_S_IFDIR
#define S_IFCHR __SPRT_S_IFCHR
#define S_IFBLK __SPRT_S_IFBLK
#define S_IFREG __SPRT_S_IFREG
#define S_IFIFO __SPRT_S_IFIFO
#define S_IFLNK __SPRT_S_IFLNK
#define S_IFSOCK __SPRT_S_IFSOCK

#define S_ISUID __SPRT_S_ISUID
#define S_ISGID __SPRT_S_ISGID
#define S_ISVTX __SPRT_S_ISVTX
#define S_IRUSR __SPRT_S_IRUSR
#define S_IWUSR __SPRT_S_IWUSR
#define S_IXUSR __SPRT_S_IXUSR
#define S_IRWXU __SPRT_S_IRWXU
#define S_IRGRP __SPRT_S_IRGRP
#define S_IWGRP __SPRT_S_IWGRP
#define S_IXGRP __SPRT_S_IXGRP
#define S_IRWXG __SPRT_S_IRWXG
#define S_IROTH __SPRT_S_IROTH
#define S_IWOTH __SPRT_S_IWOTH
#define S_IXOTH __SPRT_S_IXOTH
#define S_IRWXO __SPRT_S_IRWXO

#define S_IREAD __SPRT_S_IREAD
#define S_IWRITE __SPRT_S_IWRITE
#define S_IEXEC __SPRT_S_IEXEC

#define UTIME_NOW __SPRT_UTIME_NOW
#define UTIME_OMIT __SPRT_UTIME_OMIT

#define S_TYPEISMQ(buf) __SPRT_S_TYPEISMQ(buf)
#define S_TYPEISSEM(buf) __SPRT_S_TYPEISSEM(buf)
#define S_TYPEISSHM(buf) __SPRT_S_TYPEISSHM(buf)
#define S_TYPEISTMO(buf) __SPRT_S_TYPEISTMO(buf)

#define S_ISDIR(mode) __SPRT_S_ISDIR(mode)
#define S_ISCHR(mode) __SPRT_S_ISCHR(mode)
#define S_ISBLK(mode) __SPRT_S_ISBLK(mode)
#define S_ISREG(mode) __SPRT_S_ISREG(mode)
#define S_ISFIFO(mode) __SPRT_S_ISFIFO(mode)
#define S_ISLNK(mode) __SPRT_S_ISLNK(mode)
#define S_ISSOCK(mode) __SPRT_S_ISSOCK(mode)
#endif

__SPRT_BEGIN_DECL

typedef __SPRT_ID(mode_t) mode_t;
typedef __SPRT_ID(dev_t) dev_t;

SPRT_FORCEINLINE inline int stat(const char *__SPRT_RESTRICT path,
		struct __SPRT_STAT_NAME *__SPRT_RESTRICT __stat) {
	return __sprt_stat(path, __stat);
}

SPRT_FORCEINLINE inline int fstat(int __fd, struct __SPRT_STAT_NAME *__stat) {
	return __sprt_fstat(__fd, __stat);
}
SPRT_FORCEINLINE inline int lstat(const char *__SPRT_RESTRICT path,
		struct __SPRT_STAT_NAME *__SPRT_RESTRICT __stat) {
	return __sprt_lstat(path, __stat);
}

SPRT_FORCEINLINE inline int fstatat(int __fd, const char *__SPRT_RESTRICT path,
		struct __SPRT_STAT_NAME *__SPRT_RESTRICT __stat, int flags) {
	return __sprt_fstatat(__fd, path, __stat, flags);
}

SPRT_FORCEINLINE inline int chmod(const char *path, mode_t mode) {
	return __sprt_chmod(path, mode);
}

SPRT_FORCEINLINE inline int fchmod(int fd, mode_t mode) { return __sprt_fchmod(fd, mode); }

SPRT_FORCEINLINE inline int fchmodat(int fd, const char *path, mode_t mode, int flags) {
	return __sprt_fchmodat(fd, path, mode, flags);
}

SPRT_FORCEINLINE inline mode_t umask(mode_t mode) { return __sprt_umask(mode); }

SPRT_FORCEINLINE inline int mkdir(const char *path, mode_t mode) {
	return __sprt_mkdir(path, mode);
}

SPRT_FORCEINLINE inline int mkfifo(const char *path, mode_t mode) {
	return __sprt_mkfifo(path, mode);
}

SPRT_FORCEINLINE inline int mkdirat(int fd, const char *path, mode_t mode) {
	return __sprt_mkdirat(fd, path, mode);
}

SPRT_FORCEINLINE inline int mkfifoat(int fd, const char *path, mode_t mode) {
	return __sprt_mkfifoat(fd, path, mode);
}

SPRT_FORCEINLINE inline int mknod(const char *path, mode_t mode, dev_t dev) {
	return __sprt_mknod(path, mode, dev);
}

SPRT_FORCEINLINE inline int mknodat(int fd, const char *path, mode_t mode, dev_t dev) {
	return __sprt_mknodat(fd, path, mode, dev);
}

SPRT_FORCEINLINE inline int futimens(int fd, const struct __SPRT_TIMESPEC_NAME ts[2]) {
	return __sprt_futimens(fd, ts);
}

SPRT_FORCEINLINE inline int utimensat(int fd, const char *path,
		const struct __SPRT_TIMESPEC_NAME ts[2], int flags) {
	return __sprt_utimensat(fd, path, ts, flags);
}

__SPRT_END_DECL

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_SYS_STAT_H_
