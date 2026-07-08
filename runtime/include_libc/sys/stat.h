/**
Copyright (c) 2025 Stappler Team <admin@stappler.org>
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

#ifndef CORE_RUNTIME_INCLUDE_LIBC_SYS_STAT_H_
#define CORE_RUNTIME_INCLUDE_LIBC_SYS_STAT_H_

/*
	Dispatch header for the POSIX <sys/stat.h> (file status and mode bits):
	- hosted SPRT build -> forwards to the system <sys/stat.h> (#include_next)
	- otherwise         -> SPRT's own declarations (defined inline below)

	Public surface provided by the SPRT-own path (internal __sprt_* helpers excluded).
	A function tagged [gate: X] is declared only when __SPRT_CONFIG_HAVE_X is set for
	the target (or when __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS forces all of them).
	struct stat comes in via <sprt/c/sys/__sprt_stat.h>.

	Macros:
	  file-type bits:   S_IFMT, S_IFDIR, S_IFCHR, S_IFBLK, S_IFREG, S_IFIFO, S_IFLNK,
	                    S_IFSOCK
	  permission bits:  S_ISUID, S_ISGID, S_ISVTX, S_IRUSR/S_IWUSR/S_IXUSR/S_IRWXU,
	                    S_IRGRP/S_IWGRP/S_IXGRP/S_IRWXG, S_IROTH/S_IWOTH/S_IXOTH/S_IRWXO
	                    (omitted on Windows-protected builds)
	  legacy aliases:   S_IREAD, S_IWRITE, S_IEXEC
	  mode type-tests:  S_ISDIR, S_ISCHR, S_ISBLK, S_ISREG, S_ISFIFO, S_ISLNK, S_ISSOCK
	  stat-buf tests:   S_TYPEISMQ, S_TYPEISSEM, S_TYPEISSHM, S_TYPEISTMO
	  utimensat values: UTIME_NOW, UTIME_OMIT

	Types:
	  mode_t, dev_t

	Status functions (always available):
	  stat     - file status by path
	  lstat    - file status by path without following a final symlink
	  fstat    - file status by open descriptor
	  fstatat  - file status relative to a directory descriptor

	Mode / creation functions (always available):
	  chmod/fchmod/fchmodat - change permission bits (by path / fd / dir-relative)
	  umask                 - set the file-creation permission mask
	  mkdir/mkdirat         - create a directory (by path / dir-relative)
	  futimens              - set a file's times by descriptor (struct timespec[2])
	  utimensat             - set a file's times relative to a directory descriptor

	Gated creation functions:
	  mkfifo/mkfifoat - create a FIFO (by path / dir-relative)  [gate: STAT_MKFIFO]
	  mknod/mknodat   - create a special or regular file        [gate: STAT_MKNOD]
*/

#if defined(__SPRT_BUILD) && __STDC_HOSTED__ == 1

#include_next <sys/stat.h>

#else

#include <sprt/c/sys/__sprt_stat.h>

// Legacy single-time member names. POSIX keeps the nanosecond-resolution timespec
// members (st_atim/st_mtim/st_ctim) canonical and exposes the historical time_t
// spellings as macros over their tv_sec, which much portable code (e.g. libzip) still
// uses. glibc/musl define these unconditionally in <sys/stat.h>; the hosted path gets
// them from the system header via #include_next above.
#ifndef st_atime
#define st_atime st_atim.tv_sec
#define st_mtime st_mtim.tv_sec
#define st_ctime st_ctim.tv_sec
#endif

#ifndef S_IFMT
#define S_IFMT __SPRT_S_IFMT

#define S_IFDIR __SPRT_S_IFDIR
#define S_IFCHR __SPRT_S_IFCHR
#define S_IFBLK __SPRT_S_IFBLK
#define S_IFREG __SPRT_S_IFREG
#define S_IFIFO __SPRT_S_IFIFO
#define S_IFLNK __SPRT_S_IFLNK
#define S_IFSOCK __SPRT_S_IFSOCK

#ifndef __SPRT_WINDOWS_PROTECTED
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
#endif

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

// POSIX requires <sys/stat.h> to make these types visible (identical redefinitions
// of the ones in <sys/types.h>, which C/C++ permit). Several consumers (e.g. libzip's
// compat.h) rely on off_t appearing after <sys/stat.h>.
typedef __SPRT_ID(mode_t) mode_t;
typedef __SPRT_ID(dev_t) dev_t;
typedef __SPRT_ID(off_t) off_t;
typedef __SPRT_ID(ino_t) ino_t;
typedef __SPRT_ID(nlink_t) nlink_t;
typedef __SPRT_ID(blksize_t) blksize_t;
typedef __SPRT_ID(blkcnt_t) blkcnt_t;
typedef __SPRT_ID(uid_t) uid_t;
typedef __SPRT_ID(gid_t) gid_t;

SPRT_UMBRELLA_FUNC
int stat(const char *__SPRT_RESTRICT path,
		struct __SPRT_STAT_NAME *__SPRT_RESTRICT __stat) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_stat(path, __stat);
}
#endif

SPRT_UMBRELLA_FUNC
int fstat(int __fd, struct __SPRT_STAT_NAME *__stat) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_fstat(__fd, __stat);
}
#endif

SPRT_UMBRELLA_FUNC
int lstat(const char *__SPRT_RESTRICT path,
		struct __SPRT_STAT_NAME *__SPRT_RESTRICT __stat) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_lstat(path, __stat);
}
#endif

SPRT_UMBRELLA_FUNC
int fstatat(int __fd, const char *__SPRT_RESTRICT path,
		struct __SPRT_STAT_NAME *__SPRT_RESTRICT __stat, int flags) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_fstatat(__fd, path, __stat, flags);
}
#endif

SPRT_UMBRELLA_FUNC
int chmod(const char *path, mode_t mode) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_chmod(path, mode);
}
#endif

SPRT_UMBRELLA_FUNC
int fchmod(int fd, mode_t mode) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_fchmod(fd, mode);
}
#endif

SPRT_UMBRELLA_FUNC
int fchmodat(int fd, const char *path, mode_t mode, int flags) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_fchmodat(fd, path, mode, flags);
}
#endif

SPRT_UMBRELLA_FUNC
mode_t umask(mode_t mode) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_umask(mode);
}
#endif

SPRT_UMBRELLA_FUNC
int mkdir(const char *path, mode_t mode) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_mkdir(path, mode);
}
#endif

SPRT_UMBRELLA_FUNC
int mkdirat(int fd, const char *path, mode_t mode) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_mkdirat(fd, path, mode);
}
#endif

#if __SPRT_CONFIG_HAVE_STAT_MKFIFO || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS
SPRT_UMBRELLA_FUNC
int mkfifo(const char *path, mode_t mode) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_mkfifo(path, mode);
}
#endif

SPRT_UMBRELLA_FUNC
int mkfifoat(int fd, const char *path, mode_t mode) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_mkfifoat(fd, path, mode);
}
#endif
#endif // __SPRT_CONFIG_HAVE_STAT_MKFIFO

#if __SPRT_CONFIG_HAVE_STAT_MKNOD || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS
SPRT_UMBRELLA_FUNC
int mknod(const char *path, mode_t mode, dev_t dev) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_mknod(path, mode, dev);
}
#endif

SPRT_UMBRELLA_FUNC
int mknodat(int fd, const char *path, mode_t mode, dev_t dev) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_mknodat(fd, path, mode, dev);
}
#endif
#endif // __SPRT_CONFIG_HAVE_STAT_MKNOD

SPRT_UMBRELLA_FUNC
int futimens(int fd, const struct __SPRT_TIMESPEC_NAME ts[2]) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_futimens(fd, ts);
}
#endif

SPRT_UMBRELLA_FUNC
int utimensat(int fd, const char *path, const struct __SPRT_TIMESPEC_NAME ts[2],
		int flags) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_utimensat(fd, path, ts, flags);
}
#endif

__SPRT_END_DECL

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_SYS_STAT_H_
