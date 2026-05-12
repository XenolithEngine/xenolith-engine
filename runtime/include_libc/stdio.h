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

#ifndef CORE_RUNTIME_INCLUDE_C_LIBC_STDIO_H_
#define CORE_RUNTIME_INCLUDE_C_LIBC_STDIO_H_

#if defined(__SPRT_BUILD) && __STDC_HOSTED__ == 1

#include_next <stdio.h>

#else

#include <sprt/c/__sprt_stdio.h>
#include <sprt/c/__sprt_stdarg.h>
#include <sprt/c/bits/__sprt_null.h>

#ifndef EOF
#define EOF __SPRT_EOF
#endif

#ifndef NULL
#define NULL __SPRT_NULL
#endif

#ifndef SEEK_SET
#define SEEK_SET __SPRT_SEEK_SET
#define SEEK_CUR __SPRT_SEEK_CUR
#define SEEK_END __SPRT_SEEK_END
#endif

__SPRT_BEGIN_DECL

#define fopen64 fopen
#define freopen64 freopen

#if __STDC_HOSTED__ == 1
#define stdin  __SPRT_ID(stdin_impl)()
#define stdout  __SPRT_ID(stdout_impl)()
#define stderr  __SPRT_ID(stderr_impl)()
#endif

typedef __SPRT_ID(FILE) FILE;
typedef __SPRT_ID(size_t) size_t;
typedef __SPRT_ID(off_t) off_t;
typedef __SPRT_ID(ssize_t) ssize_t;

SPRT_UMBRELLA_FUNC
FILE *fopen(const char *__SPRT_RESTRICT path, const char *__SPRT_RESTRICT mode) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_fopen(path, mode);
}
#endif

SPRT_UMBRELLA_FUNC
FILE *freopen(const char *__SPRT_RESTRICT path, const char *__SPRT_RESTRICT mode,
		FILE *__SPRT_RESTRICT file) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_freopen(path, mode, file);
}
#endif

SPRT_UMBRELLA_FUNC
int fclose(FILE *file) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_fclose(file);
}
#endif

SPRT_UMBRELLA_FUNC
int remove(const char *path) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_remove(path);
}
#endif

SPRT_UMBRELLA_FUNC
int rename(const char *oldPath, const char *newPath) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_rename(oldPath, newPath);
}
#endif

SPRT_UMBRELLA_FUNC
int feof(FILE *file) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_feof(file);
}
#endif

SPRT_UMBRELLA_FUNC
int ferror(FILE *file) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_ferror(file);
}
#endif

SPRT_UMBRELLA_FUNC
int fflush(FILE *file) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_fflush(file);
}
#endif

SPRT_UMBRELLA_FUNC
void clearerr(FILE *file) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_clearerr(file);
}
#endif

SPRT_UMBRELLA_FUNC
int fseek(FILE *file, long pos, int when) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_fseek(file, pos, when);
}
#endif

SPRT_UMBRELLA_FUNC
long ftell(FILE *file) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_ftell(file);
}
#endif

SPRT_UMBRELLA_FUNC
void rewind(FILE *file) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_rewind(file);
}
#endif

SPRT_UMBRELLA_FUNC
size_t fread(void *__SPRT_RESTRICT buf, size_t n, size_t count,
		FILE *__SPRT_RESTRICT file) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_fread(buf, n, count, file);
}
#endif

SPRT_UMBRELLA_FUNC
size_t fwrite(const void *__SPRT_RESTRICT buf, size_t n, size_t count,
		FILE *__SPRT_RESTRICT file) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_fwrite(buf, n, count, file);
}
#endif

SPRT_UMBRELLA_FUNC
int fgetc(FILE *file) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_fgetc(file);
}
#endif

SPRT_UMBRELLA_FUNC
int getc(FILE *file) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_getc(file);
}
#endif

SPRT_UMBRELLA_FUNC
int getchar(void) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_getchar();
}
#endif

SPRT_UMBRELLA_FUNC
int ungetc(int c, FILE *file) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_ungetc(c, file);
}
#endif

SPRT_UMBRELLA_FUNC
int fputc(int c, FILE *file) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_fputc(c, file);
}
#endif

SPRT_UMBRELLA_FUNC
int putc(int c, FILE *file) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_putc(c, file);
}
#endif

SPRT_UMBRELLA_FUNC
int putchar(int c) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_putchar(c);
}
#endif

SPRT_UMBRELLA_FUNC
char *fgets(char *__SPRT_RESTRICT buf, int n, FILE *__SPRT_RESTRICT file) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_fgets(buf, n, file);
}
#endif

SPRT_UMBRELLA_FUNC
int fputs(const char *__SPRT_RESTRICT buf, FILE *__SPRT_RESTRICT file) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_fputs(buf, file);
}
#endif

SPRT_UMBRELLA_FUNC
int puts(const char *str) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_puts(str);
}
#endif

SPRT_UMBRELLA_FUNC
int printf(const char *__SPRT_RESTRICT fmt, ...) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	__sprt_va_list list;
	__sprt_va_start(list, fmt);

	int ret = __sprt_vprintf(fmt, list);

	__sprt_va_end(list);
	return ret;
}
#endif

SPRT_UMBRELLA_FUNC
int fprintf(FILE *__SPRT_RESTRICT file, const char *__SPRT_RESTRICT fmt, ...) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	__sprt_va_list list;
	__sprt_va_start(list, fmt);

	int ret = __sprt_vfprintf(file, fmt, list);

	__sprt_va_end(list);
	return ret;
}
#endif

SPRT_UMBRELLA_FUNC
int sprintf(char *__SPRT_RESTRICT buf, const char *__SPRT_RESTRICT fmt, ...) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	__sprt_va_list list;
	__sprt_va_start(list, fmt);

	int ret = __sprt_vsprintf(buf, fmt, list);

	__sprt_va_end(list);
	return ret;
}
#endif

SPRT_UMBRELLA_FUNC
int snprintf(char *__SPRT_RESTRICT buf, size_t n, const char *__SPRT_RESTRICT fmt,
		...) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	__sprt_va_list list;
	__sprt_va_start(list, fmt);

	int ret = __sprt_vsnprintf(buf, n, fmt, list);

	__sprt_va_end(list);
	return ret;
}
#endif


SPRT_UMBRELLA_FUNC
int vprintf(const char *__SPRT_RESTRICT fmt, __sprt_va_list arg) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_vprintf(fmt, arg);
}
#endif

SPRT_UMBRELLA_FUNC
int vfprintf(FILE *__SPRT_RESTRICT file, const char *__SPRT_RESTRICT fmt,
		__sprt_va_list arg) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_vfprintf(file, fmt, arg);
}
#endif

SPRT_UMBRELLA_FUNC
int vsprintf(char *__SPRT_RESTRICT buf, const char *__SPRT_RESTRICT fmt,
		__sprt_va_list arg) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_vsprintf(buf, fmt, arg);
}
#endif

SPRT_UMBRELLA_FUNC
int vsnprintf(char *__SPRT_RESTRICT buf, size_t n, const char *__SPRT_RESTRICT fmt,
		__sprt_va_list arg) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_vsnprintf(buf, n, fmt, arg);
}
#endif

SPRT_UMBRELLA_FUNC
int scanf(const char *__SPRT_RESTRICT fmt, ...) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	__sprt_va_list list;
	__sprt_va_start(list, fmt);

	int ret = __sprt_vscanf(fmt, list);

	__sprt_va_end(list);
	return ret;
}
#endif

SPRT_UMBRELLA_FUNC
int fscanf(FILE *__SPRT_RESTRICT file, const char *__SPRT_RESTRICT fmt, ...) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	__sprt_va_list list;
	__sprt_va_start(list, fmt);

	int ret = __sprt_vfscanf(file, fmt, list);

	__sprt_va_end(list);
	return ret;
}
#endif

SPRT_UMBRELLA_FUNC
int sscanf(const char *__SPRT_RESTRICT buf, const char *__SPRT_RESTRICT fmt, ...) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	__sprt_va_list list;
	__sprt_va_start(list, fmt);

	int ret = __sprt_vsscanf(buf, fmt, list);

	__sprt_va_end(list);
	return ret;
}
#endif

SPRT_UMBRELLA_FUNC
int vscanf(const char *__SPRT_RESTRICT fmt, __sprt_va_list arg) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_vscanf(fmt, arg);
}
#endif

SPRT_UMBRELLA_FUNC
int vfscanf(FILE *__SPRT_RESTRICT file, const char *__SPRT_RESTRICT fmt,
		__sprt_va_list arg) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_vfscanf(file, fmt, arg);
}
#endif

SPRT_UMBRELLA_FUNC
int vsscanf(const char *__SPRT_RESTRICT buf, const char *__SPRT_RESTRICT fmt,
		__sprt_va_list arg) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_vsscanf(buf, fmt, arg);
}
#endif

SPRT_UMBRELLA_FUNC
void perror(const char *err) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_perror(err);
}
#endif

SPRT_UMBRELLA_FUNC
int setvbuf(FILE *__SPRT_RESTRICT file, char *__SPRT_RESTRICT buf, int mode,
		size_t size) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_setvbuf(file, buf, mode, size);
}
#endif

SPRT_UMBRELLA_FUNC
void setbuf(FILE *__SPRT_RESTRICT file, char *__SPRT_RESTRICT buf) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	__sprt_setbuf(file, buf);
}
#endif

SPRT_UMBRELLA_FUNC
char *tmpnam(char *buf) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_tmpnam(buf);
}
#endif

SPRT_UMBRELLA_FUNC
FILE *tmpfile(void) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_tmpfile();
}
#endif

SPRT_UMBRELLA_FUNC
int asprintf(char **out, const char *fmt, ...) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	__sprt_va_list list;
	__sprt_va_start(list, fmt);

	int ret = __sprt_vasprintf(out, fmt, list);

	__sprt_va_end(list);
	return ret;
}
#endif

SPRT_UMBRELLA_FUNC
int vasprintf(char **out, const char *fmt, __sprt_va_list list) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_vasprintf(out, fmt, list);
}
#endif

SPRT_UMBRELLA_FUNC
FILE *fmemopen(void *__SPRT_RESTRICT ptr, size_t size,
		const char *__SPRT_RESTRICT mode) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_fmemopen(ptr, size, mode);
}
#endif

#if __SPRT_CONFIG_HAVE_STDIO_OPEN_MEMSTREAM || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS
SPRT_UMBRELLA_FUNC
FILE *open_memstream(char **ptr, size_t *sz) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_open_memstream(ptr, sz);
}
#endif
#endif

SPRT_UMBRELLA_FUNC
FILE *fdopen(int fd, const char *mode) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_fdopen(fd, mode);
}
#endif
SPRT_UMBRELLA_FUNC
FILE *popen(const char *str, const char *mode) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_popen(str, mode);
}
#endif
SPRT_UMBRELLA_FUNC
int pclose(FILE *f) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pclose(f);
}
#endif
SPRT_UMBRELLA_FUNC
int fileno(FILE *f) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_fileno(f);
}
#endif
SPRT_UMBRELLA_FUNC
int fseeko(FILE *f, off_t off, int n) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_fseeko(f, off, n);
}
#endif
SPRT_UMBRELLA_FUNC
off_t ftello(FILE *f) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_ftello(f);
}
#endif
SPRT_UMBRELLA_FUNC
int dprintf(int n, const char *__SPRT_RESTRICT fmt, ...) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	__sprt_va_list list;
	__sprt_va_start(list, fmt);

	int ret = __sprt_vdprintf(n, fmt, list);

	__sprt_va_end(list);
	return ret;
}
#endif
SPRT_UMBRELLA_FUNC
int vdprintf(int n, const char *__SPRT_RESTRICT fmt, __sprt_va_list list) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_vdprintf(n, fmt, list);
}
#endif
SPRT_UMBRELLA_FUNC
void flockfile(FILE *f) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_flockfile(f);
}
#endif
SPRT_UMBRELLA_FUNC
int ftrylockfile(FILE *f) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_ftrylockfile(f);
}
#endif
SPRT_UMBRELLA_FUNC
void funlockfile(FILE *f) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_funlockfile(f);
}
#endif
SPRT_UMBRELLA_FUNC
int getc_unlocked(FILE *f) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_getc_unlocked(f);
}
#endif
SPRT_UMBRELLA_FUNC
int getchar_unlocked(void) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_getchar_unlocked();
}
#endif
SPRT_UMBRELLA_FUNC
int putc_unlocked(int c, FILE *f) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_putc_unlocked(c, f);
}
#endif
SPRT_UMBRELLA_FUNC
int putchar_unlocked(int c) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_putchar_unlocked(c);
}
#endif
SPRT_UMBRELLA_FUNC
ssize_t getdelim(char **__SPRT_RESTRICT ret, size_t *__SPRT_RESTRICT sz, int c,
		FILE *__SPRT_RESTRICT f) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_getdelim(ret, sz, c, f);
}
#endif
SPRT_UMBRELLA_FUNC
ssize_t getline(char **__SPRT_RESTRICT ret, size_t *__SPRT_RESTRICT sz,
		FILE *__SPRT_RESTRICT f) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_getline(ret, sz, f);
}
#endif
SPRT_UMBRELLA_FUNC
int renameat(int oldfd, const char *oldPath, int newfd, const char *newPath) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_renameat(oldfd, oldPath, newfd, newPath);
}
#endif
SPRT_UMBRELLA_FUNC
char *ctermid(char *s) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_ctermid(s);
}
#endif


SPRT_UMBRELLA_FUNC
int scanf_l(__SPRT_ID(locale_t) loc, const char *__SPRT_RESTRICT fmt, ...) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	__sprt_va_list list;
	__sprt_va_start(list, fmt);

	int ret = __sprt_vscanf_l(loc, fmt, list);

	__sprt_va_end(list);
	return ret;
}
#endif


SPRT_UMBRELLA_FUNC
int fscanf_l(FILE *__SPRT_RESTRICT stream, __SPRT_ID(locale_t) loc, const char *__SPRT_RESTRICT fmt,
		...) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	__sprt_va_list list;
	__sprt_va_start(list, fmt);

	int ret = __sprt_fscanf_l(stream, loc, fmt, list);

	__sprt_va_end(list);
	return ret;
}
#endif

SPRT_UMBRELLA_FUNC
int sscanf_l(const char *__SPRT_RESTRICT buf, __SPRT_ID(locale_t) loc,
		const char *__SPRT_RESTRICT fmt, ...) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	__sprt_va_list list;
	__sprt_va_start(list, fmt);

	int ret = __sprt_vsscanf_l(buf, loc, fmt, list);

	__sprt_va_end(list);
	return ret;
}
#endif

SPRT_UMBRELLA_FUNC
int vscanf_l(__SPRT_ID(locale_t) loc, const char *__SPRT_RESTRICT format,
		__SPRT_ID(va_list) ap) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_vscanf_l(loc, format, ap);
}
#endif

SPRT_UMBRELLA_FUNC
int vfscanf_l(FILE *__SPRT_RESTRICT stream, __SPRT_ID(locale_t) loc,
		const char *__SPRT_RESTRICT format, __SPRT_ID(va_list) ap) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_vfscanf_l(stream, loc, format, ap);
}
#endif

SPRT_UMBRELLA_FUNC
int vsscanf_l(const char *__SPRT_RESTRICT str, __SPRT_ID(locale_t) loc,
		const char *__SPRT_RESTRICT format, __SPRT_ID(va_list) ap) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_vsscanf_l(str, loc, format, ap);
}
#endif

SPRT_UMBRELLA_FUNC
int snprintf_l(char *__SPRT_RESTRICT buf, __SPRT_ID(size_t) len,
		__SPRT_ID(locale_t) __SPRT_RESTRICT loc, const char *__SPRT_RESTRICT fmt,
		...) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	__sprt_va_list list;
	__sprt_va_start(list, fmt);

	int ret = __sprt_vsnprintf_l(buf, len, loc, fmt, list);

	__sprt_va_end(list);
	return ret;
}
#endif

SPRT_UMBRELLA_FUNC
int vsnprintf_l(char *__SPRT_RESTRICT buf, __SPRT_ID(size_t) len,
		__SPRT_ID(locale_t) __SPRT_RESTRICT loc, const char *__SPRT_RESTRICT fmt,
		__SPRT_ID(va_list) list) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_vsnprintf_l(buf, len, loc, fmt, list);
}
#endif

SPRT_UMBRELLA_FUNC
int asprintf_l(char **__SPRT_RESTRICT target, __SPRT_ID(locale_t) __SPRT_RESTRICT loc,
		const char *__SPRT_RESTRICT fmt, ...) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	__sprt_va_list list;
	__sprt_va_start(list, fmt);

	int ret = __sprt_vasprintf_l(target, loc, fmt, list);

	__sprt_va_end(list);
	return ret;
}
#endif

SPRT_UMBRELLA_FUNC
int vasprintf_l(char **__SPRT_RESTRICT target, __SPRT_ID(locale_t) __SPRT_RESTRICT loc,
		const char *__SPRT_RESTRICT fmt, __SPRT_ID(va_list) list) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_vasprintf_l(target, loc, fmt, list);
}
#endif

__SPRT_END_DECL

#ifdef _LIBCPP_MSVCRT
#define _scanf_l(fmt, loc, ...) scanf_l(loc, fmt, __VA_ARGS__)
#define _fscanf_l(stream, fmt, loc, ...) fscanf_l(stream, loc, fmt, __VA_ARGS__)
#define _sscanf_l(str, fmt, loc, ...) sscanf_l(str, loc, fmt, __VA_ARGS__)
#define _vscanf_l(fmt, loc, ...) vscanf_l(loc, fmt, __VA_ARGS__)
#define _vfscanf_l(stream, fmt, loc, ...) vfscanf_l(stream, loc, fmt, __VA_ARGS__)
#define _vsscanf_l(str, fmt, loc, ...) vsscanf_l(str, loc, fmt, __VA_ARGS__)
#endif

#endif

#endif // CORE_RUNTIME_INCLUDE_C_LIBC_STDIO_H_
