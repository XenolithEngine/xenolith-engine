#ifndef CORE_RUNTIME_INCLUDE_SPRT_WRAPPERS_UNISTD_IO_H_
#define CORE_RUNTIME_INCLUDE_SPRT_WRAPPERS_UNISTD_IO_H_

/*
	This is minimal implementation of Windows <io.h>
*/

#include <sprt/c/__sprt_stdio.h>
#include <sprt/c/__sprt_fcntl.h>
#include <sprt/c/sys/__sprt_stat.h>
#include <sprt/c/__sprt_wchar.h>
#include <sprt/c/__sprt_stdio.h>
#include <sprt/c/__sprt_stdlib.h>
#include <sprt/c/__sprt_unistd.h>
#include <sprt/c/__sprt_errno.h>
#include <sprt/c/__sprt_time.h>
#include <sprt/c/__sprt_stdarg.h>

struct _stat {
#include <sprt/c/bits/stat_data.h>
};

#define _stati64 _stat
#define _fstati64 _fstat
#define _wstati64 _wstat

#ifndef _O_WRONLY
#define _O_WRONLY __SPRT_O_WRONLY
#endif

#ifndef _O_CREAT
#define _O_CREAT __SPRT_O_CREAT
#endif

#ifndef _O_EXCL
#define _O_EXCL __SPRT_O_EXCL
#endif

#ifndef _O_BINARY
#define _O_BINARY 0

// libc++ posix_compat redefines this unconditionally
#ifndef POSIX_COMPAT_H
#define O_BINARY _O_BINARY
#endif
#endif

#ifndef _O_TEXT
#define _O_TEXT 0
#define O_TEXT _O_TEXT
#endif

// sprt has no text translation modes (see _O_BINARY above); the wide-mode
// constants exist so MSVC-surface code (sqlite shell's console handling)
// compiles, with _setmode() already a no-op-friendly call.
#ifndef _O_WTEXT
#define _O_WTEXT 0
#endif

#ifndef _O_U8TEXT
#define _O_U8TEXT 0
#endif

// ---- MSVC underscore aliases ------------------------------------------------
// MSVC-surface code (sqlite shell, curl) remaps the plain POSIX names to the
// underscore forms under _WIN32. _stat64/_fstat64 and the _S_* bits are pure
// name additions (macros are safe); the underscore FUNCTION names are
// declared and implemented beside their base functions.
#ifndef _stat64
#define _stat64 _stat
#endif
#ifndef _fstat64
#define _fstat64 _fstat
#endif
#ifndef __stat64
#define __stat64 _stat
#endif
#ifndef __fstat64
#define __fstat64 _fstat
#endif
#ifndef _S_IFCHR
#define _S_IFCHR S_IFCHR
#endif
#ifndef _S_IFIFO
#define _S_IFIFO S_IFIFO
#endif
#ifndef _S_IFREG
#define _S_IFREG S_IFREG
#endif

// _mkdir/_rmdir are declared in the windows wrappers' <direct.h>; _unlink and
// _chmod already exist as sprt umbrella functions below.
SPRT_API int _access(const char *path, int mode) __SPRT_NOEXCEPT;
SPRT_API int _pclose(__SPRT_ID(FILE) * stream) __SPRT_NOEXCEPT;

// ---- _wfindfirst / _wfindnext / _findclose (MSVC <io.h> find surface) -------
// File-attribute bits (real SDK values).
#ifndef _A_NORMAL
#define _A_NORMAL 0x00
#endif
#ifndef _A_RDONLY
#define _A_RDONLY 0x01
#endif
#ifndef _A_HIDDEN
#define _A_HIDDEN 0x02
#endif
#ifndef _A_SYSTEM
#define _A_SYSTEM 0x04
#endif
#ifndef _A_SUBDIR
#define _A_SUBDIR 0x10
#endif
#ifndef _A_ARCH
#define _A_ARCH 0x20
#endif

#ifndef _WFINDDATA_T_DEFINED
#define _WFINDDATA_T_DEFINED
struct _wfinddata_t {
	unsigned attrib;
	long long time_create; // time_t-sized; kept integral to avoid type leakage
	long long time_access;
	long long time_write;
	long long size;
	wchar_t name[260];
};
#endif

SPRT_API long long _wfindfirst(const wchar_t *filespec, struct _wfinddata_t *findinfo) __SPRT_NOEXCEPT;
SPRT_API int _wfindnext(long long findhandle, struct _wfinddata_t *findinfo) __SPRT_NOEXCEPT;
SPRT_API int _findclose(long long findhandle) __SPRT_NOEXCEPT;

// ---- wide file operations ----------------------------------------------------
SPRT_API int _wchmod(const wchar_t *filename, int pmode) __SPRT_NOEXCEPT;
SPRT_API int _wmkdir(const wchar_t *dirname) __SPRT_NOEXCEPT;
SPRT_API int _wunlink(const wchar_t *filename) __SPRT_NOEXCEPT;

#ifndef S_IFREG
#define S_IFREG __SPRT_S_IFREG
#endif

#define _S_IFMT __SPRT_S_IFMT
#define _S_IFDIR __SPRT_S_IFDIR

#ifdef _WIN32
#ifndef _MAX_PATH
#define _MAX_PATH   260 // max. length of full pathname
#define _MAX_DRIVE  3   // max. length of drive component
#define _MAX_DIR    256 // max. length of path component
#define _MAX_FNAME  256 // max. length of file name component
#define _MAX_EXT    256 // max. length of extension component
#endif
#endif

#ifndef _SH_DENYRW
#define _SH_DENYRW      0x10    // deny read/write mode
#define _SH_DENYWR      0x20    // deny write mode
#define _SH_DENYRD      0x30    // deny read mode
#define _SH_DENYNO      0x40    // deny none mode
#define _SH_SECURE      0x80    // secure mode
#endif

#ifndef _S_IREAD
#define _S_IREAD __SPRT_S_IRUSR
#define _S_IWRITE __SPRT_S_IWUSR
#endif

#ifndef st_atime
#define st_atime st_atim.tv_sec
#define st_mtime st_mtim.tv_sec
#define st_ctime st_ctim.tv_sec
#endif

typedef __SPRT_ID(off_t) off_t;
typedef __SPRT_ID(size_t) size_t;

#ifndef __cplusplus
typedef __SPRT_ID(wchar_t) wchar_t;
#endif

__SPRT_BEGIN_DECL

SPRT_API __SPRT_ID(intptr_t) _get_osfhandle(int) __SPRT_NOEXCEPT;

SPRT_API int _mktemp_s(char *_TemplateName, __SPRT_ID(size_t) _SizeInChars) __SPRT_NOEXCEPT;

SPRT_API int _open_osfhandle(__SPRT_ID(intptr_t) osfhandle, int flags) __SPRT_NOEXCEPT;

SPRT_API int _wsopen_s(int *pfh, const wchar_t *filename, int oflag, int shflag,
		...) __SPRT_NOEXCEPT;
SPRT_API int _wsopen(const wchar_t *, int, int, ...) __SPRT_NOEXCEPT;
SPRT_API int _wopen(const wchar_t *, int, ...) __SPRT_NOEXCEPT;

SPRT_API int _sopen_s(int *pfh, const char *filename, int oflag, int shflag, ...) __SPRT_NOEXCEPT;
SPRT_API int _sopen(const char *, int, int, ...) __SPRT_NOEXCEPT;


SPRT_API char *_fullpath(char *absPath, const char *relPath, size_t maxLength) __SPRT_NOEXCEPT;

SPRT_API wchar_t *_wfullpath(wchar_t *absPath, const wchar_t *relPath,
		size_t maxLength) __SPRT_NOEXCEPT;


SPRT_API __SPRT_ID(FILE) * _wfopen(const wchar_t *, const wchar_t *) __SPRT_NOEXCEPT;

// Wide popen: the MSVC CRT surface sqlite's shell and friends expect. The
// command and mode are converted UTF-16 -> UTF-8 and delegated to popen().
SPRT_API __SPRT_ID(FILE) * _wpopen(const wchar_t *, const wchar_t *) __SPRT_NOEXCEPT;


SPRT_API __SPRT_ID(FILE) * _fsopen(const char *filename, const char *mode, int sh) __SPRT_NOEXCEPT;

SPRT_API __SPRT_ID(FILE)
		* _wfsopen(const wchar_t *filename, const wchar_t *mode, int shflag) __SPRT_NOEXCEPT;


SPRT_API int freopen_s(__SPRT_ID(FILE) * *stream, const char *fileName, const char *mode,
		__SPRT_ID(FILE) * oldStream) __SPRT_NOEXCEPT;

SPRT_API int _wfreopen_s(__SPRT_ID(FILE) * *stream, const wchar_t *fileName, const wchar_t *mode,
		__SPRT_ID(FILE) * oldStream) __SPRT_NOEXCEPT;


SPRT_API int _wstat(const wchar_t *path, struct _stati64 *buffer) __SPRT_NOEXCEPT;

SPRT_API int setmode(int, int) __SPRT_NOEXCEPT;
SPRT_API int _setmode(int, int) __SPRT_NOEXCEPT;

SPRT_UMBRELLA_FUNC
off_t _lseeki64(int __fd, off_t __offset, int __whence) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_lseek(__fd, __offset, __whence);
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
__SPRT_ID(FILE)
*_fdopen(int fd, const char *mode) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_fdopen(fd, mode);
}
#endif

SPRT_UMBRELLA_FUNC
int _fileno(__SPRT_ID(FILE) * f) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_fileno(f);
}
#endif

SPRT_UMBRELLA_FUNC
int _chsize(int fd, long size) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_ftruncate(fd, size);
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
int _wfopen_s(__SPRT_ID(FILE) * *f, const wchar_t *path, const wchar_t *mode) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	if (!f) {
		return EINVAL;
	}
	*f = _wfopen(path, mode);
	if (!*f) {
		return __sprt_errno;
	}
	return 0;
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
int _fstat(int __fd, struct _stat *__stat) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_fstat(__fd, (struct __SPRT_STAT_NAME *)__stat);
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

SPRT_UMBRELLA_FUNC
__SPRT_ID(ssize_t)
_write(int __fd, const void *__buf, size_t __n) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_write(__fd, __buf, __n);
}
#endif

SPRT_UMBRELLA_FUNC
__SPRT_ID(ssize_t)
_read(int __fd, void *__buf, size_t __n) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_read(__fd, __buf, __n);
}
#endif

SPRT_UMBRELLA_FUNC
off_t _lseek(int __fd, off_t __offset, int __whence) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_lseek(__fd, __offset, __whence);
}
#endif

SPRT_UMBRELLA_FUNC
int _close(int __fd) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_close(__fd);
}
#endif

SPRT_UMBRELLA_FUNC
int _open(const char *__path, int __flags, ...) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	__SPRT_ID(mode_t) __mode = 0;
	if (__flags & __SPRT_O_CREAT) {
		__sprt_va_list ap;
		__sprt_va_start(ap, __flags);
		__mode = __SPRT_VA_ARG_MODE_T(ap);
		__sprt_va_end(ap);
	}
	return __sprt_open(__path, __flags, __mode);
}
#endif

SPRT_UMBRELLA_FUNC
int _dup(int __fd) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_dup(__fd);
}
#endif

SPRT_UMBRELLA_FUNC
int _stat(const char *__SPRT_RESTRICT path, struct _stat *__SPRT_RESTRICT __stat) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_stat(path, (struct __SPRT_STAT_NAME *)__stat);
}
#endif

SPRT_UMBRELLA_FUNC
__SPRT_NORETURN void _exit(int value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	__sprt_exit(value);
}
#endif

typedef int (*onexit_t)();

SPRT_UMBRELLA_FUNC
onexit_t _onexit(onexit_t value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	__sprt_atexit((void (*)())value);
	return value;
}
#endif

SPRT_UMBRELLA_FUNC
int _vsnprintf(char *__SPRT_RESTRICT buf, size_t n, const char *__SPRT_RESTRICT fmt,
		__sprt_va_list arg) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_vsnprintf(buf, n, fmt, arg);
}
#endif

SPRT_UMBRELLA_FUNC
int _chmod(const char *path, __SPRT_ID(mode_t) mode) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_chmod(path, mode);
}
#endif

// MSVC _pipe(fds, size, textmode): the buffer-size hint and text/binary mode are
// advisory; forward to the runtime's POSIX pipe primitive.
SPRT_UMBRELLA_FUNC
int _pipe(int *__pfds, unsigned int __psize, int __textmode) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	(void)__psize;
	(void)__textmode;
	return __SPRT_ID(pipe)(__pfds);
}
#endif

__SPRT_END_DECL

#endif // CORE_RUNTIME_INCLUDE_SPRT_WRAPPERS_UNISTD_IO_H_
