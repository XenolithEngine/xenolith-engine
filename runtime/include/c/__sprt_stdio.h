/**
Copyright (c) 2025 Stappler Team <admin@stappler.org>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation __SPRT_ID(FILE)s (the "Software"), to deal
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

#ifndef CORE_RUNTIME_INCLUDE_C___SPRT_STDIO_H_
#define CORE_RUNTIME_INCLUDE_C___SPRT_STDIO_H_

#include <c/bits/__sprt_def.h>
#include <c/bits/__sprt_va_list.h>
#include <c/bits/__sprt_size_t.h>
#include <c/bits/__sprt_ssize_t.h>
#include <c/cross/__sprt_file_ptr.h>
#include <c/bits/seek.h>

#ifndef __SPRT_EOF
#define __SPRT_EOF (-1)
#endif

__SPRT_BEGIN_DECL

SPRT_API __SPRT_ID(size_t) __SPRT_ID(fpath_to_posix)(const char *__SPRT_RESTRICT,
		__SPRT_ID(size_t) pathSize, char *buf, __SPRT_ID(size_t) bufSize);

SPRT_API __SPRT_ID(size_t) __SPRT_ID(fpath_to_native)(const char *__SPRT_RESTRICT,
		__SPRT_ID(size_t) pathSize, char *buf, __SPRT_ID(size_t) bufSize);

#if SPRT_WINDOWS
SPRT_API int __SPRT_ID(fpath_is_native)(const char *__SPRT_RESTRICT, __SPRT_ID(size_t));
SPRT_API int __SPRT_ID(fpath_is_posix)(const char *__SPRT_RESTRICT, __SPRT_ID(size_t));
#else
inline int __SPRT_ID(fpath_is_native)(const char *__SPRT_RESTRICT, __SPRT_ID(size_t)) { return 1; }
inline int __SPRT_ID(fpath_is_posix)(const char *__SPRT_RESTRICT, __SPRT_ID(size_t)) { return 1; }
#endif

SPRT_API __SPRT_ID(FILE) * __SPRT_ID(stdin_impl)();
SPRT_API __SPRT_ID(FILE) * __SPRT_ID(stdout_impl)();
SPRT_API __SPRT_ID(FILE) * __SPRT_ID(stderr_impl)();

SPRT_API __SPRT_ID(FILE)
		* __SPRT_ID(fopen_impl)(const char *__SPRT_RESTRICT, const char *__SPRT_RESTRICT);
#define __sprt_fopen __SPRT_ID(fopen_impl)

SPRT_API __SPRT_ID(FILE)
		* __SPRT_ID(freopen_impl)(const char *__SPRT_RESTRICT, const char *__SPRT_RESTRICT,
				__SPRT_ID(FILE) * __SPRT_RESTRICT);
#define __sprt_freopen __SPRT_ID(freopen_impl)

SPRT_API int __SPRT_ID(fclose_impl)(__SPRT_ID(FILE) *);
#define __sprt_fclose __SPRT_ID(fclose_impl)


SPRT_API int __SPRT_ID(remove_impl)(const char *);
#define __sprt_remove __SPRT_ID(remove_impl)

SPRT_API int __SPRT_ID(rename_impl)(const char *, const char *);
#define __sprt_rename __SPRT_ID(rename_impl)


SPRT_API int __SPRT_ID(feof_impl)(__SPRT_ID(FILE) *);
#define __sprt_feof __SPRT_ID(feof_impl)

SPRT_API int __SPRT_ID(ferror_impl)(__SPRT_ID(FILE) *);
#define __sprt_ferror __SPRT_ID(ferror_impl)

SPRT_API int __SPRT_ID(fflush_impl)(__SPRT_ID(FILE) *);
#define __sprt_fflush __SPRT_ID(fflush_impl)

SPRT_API void __SPRT_ID(clearerr_impl)(__SPRT_ID(FILE) *);
#define __sprt_clearerr __SPRT_ID(clearerr_impl)


SPRT_API int __SPRT_ID(fseek_impl)(__SPRT_ID(FILE) *, long, int);
#define __sprt_fseek __SPRT_ID(fseek_impl)

SPRT_API long __SPRT_ID(ftell_impl)(__SPRT_ID(FILE) *);
#define __sprt_ftell __SPRT_ID(ftell_impl)

SPRT_API void __SPRT_ID(rewind_impl)(__SPRT_ID(FILE) *);
#define __sprt_rewind __SPRT_ID(rewind_impl)


SPRT_API __SPRT_ID(size_t) __SPRT_ID(fread_impl)(void *__SPRT_RESTRICT, __SPRT_ID(size_t),
		__SPRT_ID(size_t), __SPRT_ID(FILE) * __SPRT_RESTRICT);
#define __sprt_fread __SPRT_ID(fread_impl)

SPRT_API __SPRT_ID(size_t) __SPRT_ID(fwrite_impl)(const void *__SPRT_RESTRICT, __SPRT_ID(size_t),
		__SPRT_ID(size_t), __SPRT_ID(FILE) * __SPRT_RESTRICT);
#define __sprt_fwrite __SPRT_ID(fwrite_impl)


SPRT_API int __SPRT_ID(fgetc_impl)(__SPRT_ID(FILE) *);
#define __sprt_fgetc __SPRT_ID(fgetc_impl)

SPRT_API int __SPRT_ID(getc_impl)(__SPRT_ID(FILE) *);
#define __sprt_getc __SPRT_ID(getc_impl)

SPRT_API int __SPRT_ID(getchar_impl)(void);
#define __sprt_getchar __SPRT_ID(getchar_impl)

SPRT_API int __SPRT_ID(ungetc_impl)(int, __SPRT_ID(FILE) *);
#define __sprt_ungetc __SPRT_ID(ungetc_impl)


SPRT_API int __SPRT_ID(fputc_impl)(int, __SPRT_ID(FILE) *);
#define __sprt_fputc __SPRT_ID(fputc_impl)

SPRT_API int __SPRT_ID(putc_impl)(int, __SPRT_ID(FILE) *);
#define __sprt_putc __SPRT_ID(putc_impl)

SPRT_API int __SPRT_ID(putchar_impl)(int);
#define __sprt_putchar __SPRT_ID(putchar_impl)


SPRT_API char *__SPRT_ID(fgets_impl)(char *__SPRT_RESTRICT, int, __SPRT_ID(FILE) * __SPRT_RESTRICT);
#define __sprt_fgets __SPRT_ID(fgets_impl)


SPRT_API int __SPRT_ID(fputs_impl)(const char *__SPRT_RESTRICT, __SPRT_ID(FILE) * __SPRT_RESTRICT);
#define __sprt_fputs __SPRT_ID(fputs_impl)

SPRT_API int __SPRT_ID(puts_impl)(const char *);
#define __sprt_puts __SPRT_ID(puts_impl)


SPRT_API int __SPRT_ID(printf_impl)(const char *__SPRT_RESTRICT, ...);
#define __sprt_printf __SPRT_ID(printf_impl)

SPRT_API int __SPRT_ID(
		fprintf_impl)(__SPRT_ID(FILE) * __SPRT_RESTRICT, const char *__SPRT_RESTRICT, ...);
#define __sprt_fprintf __SPRT_ID(fprintf_impl)

SPRT_API int __SPRT_ID(sprintf_impl)(char *__SPRT_RESTRICT, const char *__SPRT_RESTRICT, ...);
#define __sprt_sprintf __SPRT_ID(sprintf_impl)

SPRT_API int __SPRT_ID(
		snprintf_impl)(char *__SPRT_RESTRICT, __SPRT_ID(size_t), const char *__SPRT_RESTRICT, ...);
#define __sprt_snprintf __SPRT_ID(snprintf_impl)


SPRT_API int __SPRT_ID(vprintf_impl)(const char *__SPRT_RESTRICT, __SPRT_ID(va_list));
#define __sprt_vprintf __SPRT_ID(vprintf_impl)

SPRT_API int __SPRT_ID(vfprintf_impl)(__SPRT_ID(FILE) * __SPRT_RESTRICT,
		const char *__SPRT_RESTRICT, __SPRT_ID(va_list));
#define __sprt_vfprintf __SPRT_ID(vfprintf_impl)

SPRT_API int __SPRT_ID(
		vsprintf_impl)(char *__SPRT_RESTRICT, const char *__SPRT_RESTRICT, __SPRT_ID(va_list));
#define __sprt_vsprintf __SPRT_ID(vsprintf_impl)

SPRT_API int __SPRT_ID(vsnprintf_impl)(char *__SPRT_RESTRICT, __SPRT_ID(size_t),
		const char *__SPRT_RESTRICT, __SPRT_ID(va_list));
#define __sprt_vsnprintf __SPRT_ID(vsnprintf_impl)


SPRT_API int __SPRT_ID(scanf_impl)(const char *__SPRT_RESTRICT, ...);
#define __sprt_scanf __SPRT_ID(scanf_impl)

SPRT_API int __SPRT_ID(
		fscanf_impl)(__SPRT_ID(FILE) * __SPRT_RESTRICT, const char *__SPRT_RESTRICT, ...);
#define __sprt_fscanf __SPRT_ID(fscanf_impl)

SPRT_API int __SPRT_ID(sscanf_impl)(const char *__SPRT_RESTRICT, const char *__SPRT_RESTRICT, ...);
#define __sprt_sscanf __SPRT_ID(sscanf_impl)

SPRT_API int __SPRT_ID(vscanf_impl)(const char *__SPRT_RESTRICT, __SPRT_ID(va_list));
#define __sprt_vscanf __SPRT_ID(vscanf_impl)

SPRT_API int __SPRT_ID(vfscanf_impl)(__SPRT_ID(FILE) * __SPRT_RESTRICT, const char *__SPRT_RESTRICT,
		__SPRT_ID(va_list));
#define __sprt_vfscanf __SPRT_ID(vfscanf_impl)

SPRT_API int __SPRT_ID(
		vsscanf_impl)(const char *__SPRT_RESTRICT, const char *__SPRT_RESTRICT, __SPRT_ID(va_list));
#define __sprt_vsscanf __SPRT_ID(vsscanf_impl)


SPRT_API void __SPRT_ID(perror_impl)(const char *);
#define __sprt_perror __SPRT_ID(perror_impl)


SPRT_API int __SPRT_ID(setvbuf_impl)(__SPRT_ID(FILE) * __SPRT_RESTRICT, char *__SPRT_RESTRICT, int,
		__SPRT_ID(size_t));
#define __sprt_setvbuf __SPRT_ID(setvbuf_impl)

SPRT_API void __SPRT_ID(setbuf_impl)(__SPRT_ID(FILE) * __SPRT_RESTRICT, char *__SPRT_RESTRICT);
#define __sprt_setbuf __SPRT_ID(setbuf_impl)


SPRT_API char *__SPRT_ID(tmpnam_impl)(char *);
#define __sprt_tmpnam __SPRT_ID(tmpnam_impl)

SPRT_API __SPRT_ID(FILE) * __SPRT_ID(tmpfile_impl)(void);
#define __sprt_tmpfile __SPRT_ID(tmpfile_impl)

SPRT_API int __SPRT_ID(asprintf)(char **, const char *, ...);
SPRT_API int __SPRT_ID(vasprintf)(char **, const char *, __SPRT_ID(va_list));

SPRT_API __SPRT_ID(FILE)
		* __SPRT_ID(
				fmemopen)(void *__SPRT_RESTRICT, __SPRT_ID(size_t), const char *__SPRT_RESTRICT);
SPRT_API __SPRT_ID(FILE) * __SPRT_ID(open_memstream)(char **, __SPRT_ID(size_t) *);
SPRT_API __SPRT_ID(FILE) * __SPRT_ID(fdopen)(int, const char *);
SPRT_API __SPRT_ID(FILE) * __SPRT_ID(popen)(const char *, const char *);
SPRT_API int __SPRT_ID(pclose)(__SPRT_ID(FILE) *);
SPRT_API int __SPRT_ID(fileno)(__SPRT_ID(FILE) *);
SPRT_API int __SPRT_ID(fseeko)(__SPRT_ID(FILE) *, __SPRT_ID(off_t), int);
SPRT_API __SPRT_ID(off_t) __SPRT_ID(ftello)(__SPRT_ID(FILE) *);
SPRT_API int __SPRT_ID(dprintf)(int, const char *__SPRT_RESTRICT, ...);
SPRT_API int __SPRT_ID(vdprintf)(int, const char *__SPRT_RESTRICT, __SPRT_ID(va_list));
SPRT_API void __SPRT_ID(flockfile)(__SPRT_ID(FILE) *);
SPRT_API int __SPRT_ID(ftrylockfile)(__SPRT_ID(FILE) *);
SPRT_API void __SPRT_ID(funlockfile)(__SPRT_ID(FILE) *);
SPRT_API int __SPRT_ID(getc_unlocked)(__SPRT_ID(FILE) *);
SPRT_API int __SPRT_ID(getchar_unlocked)(void);
SPRT_API int __SPRT_ID(putc_unlocked)(int, __SPRT_ID(FILE) *);
SPRT_API int __SPRT_ID(putchar_unlocked)(int);
SPRT_API __SPRT_ID(ssize_t) __SPRT_ID(getdelim)(char **__SPRT_RESTRICT,
		__SPRT_ID(size_t) * __SPRT_RESTRICT, int, __SPRT_ID(FILE) * __SPRT_RESTRICT);
SPRT_API __SPRT_ID(ssize_t) __SPRT_ID(getline)(char **__SPRT_RESTRICT,
		__SPRT_ID(size_t) * __SPRT_RESTRICT, __SPRT_ID(FILE) * __SPRT_RESTRICT);
SPRT_API int __SPRT_ID(renameat)(int, const char *, int, const char *);
SPRT_API char *__SPRT_ID(ctermid)(char *);

__SPRT_END_DECL

#endif // CORE_RUNTIME_INCLUDE_C___SPRT_STDIO_H_
