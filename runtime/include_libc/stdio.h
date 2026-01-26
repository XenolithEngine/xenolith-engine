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

#ifdef __SPRT_BUILD

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

#define stdin  __SPRT_ID(stdin_impl)()
#define stdout  __SPRT_ID(stdout_impl)()
#define stderr  __SPRT_ID(stderr_impl)()

typedef __SPRT_ID(FILE) FILE;
typedef __SPRT_ID(size_t) size_t;
typedef __SPRT_ID(off_t) off_t;
typedef __SPRT_ID(ssize_t) ssize_t;

SPRT_FORCEINLINE FILE *fopen(const char *__SPRT_RESTRICT path, const char *__SPRT_RESTRICT mode) {
	return __sprt_fopen(path, mode);
}

SPRT_FORCEINLINE FILE *freopen(const char *__SPRT_RESTRICT path, const char *__SPRT_RESTRICT mode,
		FILE *__SPRT_RESTRICT file) {
	return __sprt_freopen(path, mode, file);
}

SPRT_FORCEINLINE int fclose(FILE *file) { return __sprt_fclose(file); }

SPRT_FORCEINLINE int remove(const char *path) { return __sprt_remove(path); }

SPRT_FORCEINLINE int rename(const char *oldPath, const char *newPath) {
	return __sprt_rename(oldPath, newPath);
}

SPRT_FORCEINLINE int feof(FILE *file) { return __sprt_feof(file); }

SPRT_FORCEINLINE int ferror(FILE *file) { return __sprt_ferror(file); }

SPRT_FORCEINLINE int fflush(FILE *file) { return __sprt_fflush(file); }

SPRT_FORCEINLINE void clearerr(FILE *file) { return __sprt_clearerr(file); }

SPRT_FORCEINLINE int fseek(FILE *file, long pos, int when) { return __sprt_fseek(file, pos, when); }

SPRT_FORCEINLINE long ftell(FILE *file) { return __sprt_ftell(file); }

SPRT_FORCEINLINE void rewind(FILE *file) { return __sprt_rewind(file); }

SPRT_FORCEINLINE size_t fread(void *__SPRT_RESTRICT buf, size_t n, size_t count,
		FILE *__SPRT_RESTRICT file) {
	return __sprt_fread(buf, n, count, file);
}

SPRT_FORCEINLINE size_t fwrite(const void *__SPRT_RESTRICT buf, size_t n, size_t count,
		FILE *__SPRT_RESTRICT file) {
	return __sprt_fwrite(buf, n, count, file);
}

SPRT_FORCEINLINE int fgetc(FILE *file) { return __sprt_fgetc(file); }

SPRT_FORCEINLINE int getc(FILE *file) { return __sprt_getc(file); }

SPRT_FORCEINLINE int getchar(void) { return __sprt_getchar(); }

SPRT_FORCEINLINE int ungetc(int c, FILE *file) { return __sprt_ungetc(c, file); }

SPRT_FORCEINLINE int fputc(int c, FILE *file) { return __sprt_fputc(c, file); }

SPRT_FORCEINLINE int putc(int c, FILE *file) { return __sprt_putc(c, file); }

SPRT_FORCEINLINE int putchar(int c) { return __sprt_putchar(c); }

SPRT_FORCEINLINE char *fgets(char *__SPRT_RESTRICT buf, int n, FILE *__SPRT_RESTRICT file) {
	return __sprt_fgets(buf, n, file);
}

SPRT_FORCEINLINE int fputs(const char *__SPRT_RESTRICT buf, FILE *__SPRT_RESTRICT file) {
	return __sprt_fputs(buf, file);
}

SPRT_FORCEINLINE int puts(const char *str) { return __sprt_puts(str); }

SPRT_FORCEINLINE int printf(const char *__SPRT_RESTRICT fmt, ...) {
	__sprt_va_list list;
	__sprt_va_start(list, fmt);

	auto ret = __sprt_vprintf(fmt, list);

	__sprt_va_end(list);
	return ret;
}

SPRT_FORCEINLINE int fprintf(FILE *__SPRT_RESTRICT file, const char *__SPRT_RESTRICT fmt, ...) {
	__sprt_va_list list;
	__sprt_va_start(list, fmt);

	auto ret = __sprt_vfprintf(file, fmt, list);

	__sprt_va_end(list);
	return ret;
}

SPRT_FORCEINLINE int sprintf(char *__SPRT_RESTRICT buf, const char *__SPRT_RESTRICT fmt, ...) {
	__sprt_va_list list;
	__sprt_va_start(list, fmt);

	auto ret = __sprt_vsprintf(buf, fmt, list);

	__sprt_va_end(list);
	return ret;
}

SPRT_FORCEINLINE int snprintf(char *__SPRT_RESTRICT buf, size_t n, const char *__SPRT_RESTRICT fmt,
		...) {
	__sprt_va_list list;
	__sprt_va_start(list, fmt);

	auto ret = __sprt_vsnprintf(buf, n, fmt, list);

	__sprt_va_end(list);
	return ret;
}


SPRT_FORCEINLINE int vprintf(const char *__SPRT_RESTRICT fmt, __sprt_va_list arg) {
	return __sprt_vprintf(fmt, arg);
}

SPRT_FORCEINLINE int vfprintf(FILE *__SPRT_RESTRICT file, const char *__SPRT_RESTRICT fmt,
		__sprt_va_list arg) {
	return __sprt_vfprintf(file, fmt, arg);
}

SPRT_FORCEINLINE int vsprintf(char *__SPRT_RESTRICT buf, const char *__SPRT_RESTRICT fmt,
		__sprt_va_list arg) {
	return __sprt_vsprintf(buf, fmt, arg);
}

SPRT_FORCEINLINE int vsnprintf(char *__SPRT_RESTRICT buf, size_t n, const char *__SPRT_RESTRICT fmt,
		__sprt_va_list arg) {
	return __sprt_vsnprintf(buf, n, fmt, arg);
}

SPRT_FORCEINLINE int scanf(const char *__SPRT_RESTRICT fmt, ...) {
	__sprt_va_list list;
	__sprt_va_start(list, fmt);

	auto ret = __sprt_vscanf(fmt, list);

	__sprt_va_end(list);
	return ret;
}

SPRT_FORCEINLINE int fscanf(FILE *__SPRT_RESTRICT file, const char *__SPRT_RESTRICT fmt, ...) {
	__sprt_va_list list;
	__sprt_va_start(list, fmt);

	auto ret = __sprt_vfscanf(file, fmt, list);

	__sprt_va_end(list);
	return ret;
}

SPRT_FORCEINLINE int sscanf(const char *__SPRT_RESTRICT buf, const char *__SPRT_RESTRICT fmt, ...) {
	__sprt_va_list list;
	__sprt_va_start(list, fmt);

	auto ret = __sprt_vsscanf(buf, fmt, list);

	__sprt_va_end(list);
	return ret;
}

SPRT_FORCEINLINE int vscanf(const char *__SPRT_RESTRICT fmt, __sprt_va_list arg) {
	return __sprt_vscanf(fmt, arg);
}

SPRT_FORCEINLINE int vfscanf(FILE *__SPRT_RESTRICT file, const char *__SPRT_RESTRICT fmt,
		__sprt_va_list arg) {
	return __sprt_vfscanf(file, fmt, arg);
}

SPRT_FORCEINLINE int vsscanf(const char *__SPRT_RESTRICT buf, const char *__SPRT_RESTRICT fmt,
		__sprt_va_list arg) {
	return __sprt_vsscanf(buf, fmt, arg);
}

SPRT_FORCEINLINE void perror(const char *err) { return __sprt_perror(err); }

SPRT_FORCEINLINE int setvbuf(FILE *__SPRT_RESTRICT file, char *__SPRT_RESTRICT buf, int mode,
		size_t size) {
	return __sprt_setvbuf(file, buf, mode, size);
}

SPRT_FORCEINLINE void setbuf(FILE *__SPRT_RESTRICT file, char *__SPRT_RESTRICT buf) {
	__sprt_setbuf(file, buf);
}

SPRT_FORCEINLINE char *tmpnam(char *buf) { return __sprt_tmpnam(buf); }

SPRT_FORCEINLINE FILE *tmpfile(void) { return __sprt_tmpfile(); }

SPRT_FORCEINLINE int asprintf(char **out, const char *fmt, ...) {
	__sprt_va_list list;
	__sprt_va_start(list, fmt);

	auto ret = __sprt_vasprintf(out, fmt, list);

	__sprt_va_end(list);
	return ret;
}

SPRT_FORCEINLINE int vasprintf(char **out, const char *fmt, __sprt_va_list list) {
	return __sprt_vasprintf(out, fmt, list);
}

SPRT_FORCEINLINE FILE *fmemopen(void *__SPRT_RESTRICT ptr, size_t size,
		const char *__SPRT_RESTRICT mode) {
	return __sprt_fmemopen(ptr, size, mode);
}

#if __SPRT_CONFIG_HAVE_STDIO_OPEN_MEMSTREAM || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS
SPRT_FORCEINLINE FILE *open_memstream(char **ptr, size_t *sz) {
	return __sprt_open_memstream(ptr, sz);
}
#endif

SPRT_FORCEINLINE FILE *fdopen(int fd, const char *mode) { return __sprt_fdopen(fd, mode); }
SPRT_FORCEINLINE FILE *popen(const char *str, const char *mode) { return __sprt_popen(str, mode); }
SPRT_FORCEINLINE int pclose(FILE *f) { return __sprt_pclose(f); }
SPRT_FORCEINLINE int fileno(FILE *f) { return __sprt_fileno(f); }
SPRT_FORCEINLINE int fseeko(FILE *f, off_t off, int n) { return __sprt_fseeko(f, off, n); }
SPRT_FORCEINLINE off_t ftello(FILE *f) { return __sprt_ftello(f); }
SPRT_FORCEINLINE int dprintf(int n, const char *__SPRT_RESTRICT fmt, ...) {
	__sprt_va_list list;
	__sprt_va_start(list, fmt);

	auto ret = __sprt_vdprintf(n, fmt, list);

	__sprt_va_end(list);
	return ret;
}
SPRT_FORCEINLINE int vdprintf(int n, const char *__SPRT_RESTRICT fmt, __sprt_va_list list) {
	return __sprt_vdprintf(n, fmt, list);
}
SPRT_FORCEINLINE void flockfile(FILE *f) { return __sprt_flockfile(f); }
SPRT_FORCEINLINE int ftrylockfile(FILE *f) { return __sprt_ftrylockfile(f); }
SPRT_FORCEINLINE void funlockfile(FILE *f) { return __sprt_funlockfile(f); }
SPRT_FORCEINLINE int getc_unlocked(FILE *f) { return __sprt_getc_unlocked(f); }
SPRT_FORCEINLINE int getchar_unlocked(void) { return __sprt_getchar_unlocked(); }
SPRT_FORCEINLINE int putc_unlocked(int c, FILE *f) { return __sprt_putc_unlocked(c, f); }
SPRT_FORCEINLINE int putchar_unlocked(int c) { return __sprt_putchar_unlocked(c); }
SPRT_FORCEINLINE ssize_t getdelim(char **__SPRT_RESTRICT ret, size_t *__SPRT_RESTRICT sz, int c,
		FILE *__SPRT_RESTRICT f) {
	return __sprt_getdelim(ret, sz, c, f);
}
SPRT_FORCEINLINE ssize_t getline(char **__SPRT_RESTRICT ret, size_t *__SPRT_RESTRICT sz,
		FILE *__SPRT_RESTRICT f) {
	return __sprt_getline(ret, sz, f);
}
SPRT_FORCEINLINE int renameat(int oldfd, const char *oldPath, int newfd, const char *newPath) {
	return __sprt_renameat(oldfd, oldPath, newfd, newPath);
}
SPRT_FORCEINLINE char *ctermid(char *s) { return __sprt_ctermid(s); }


__SPRT_END_DECL

#endif

#endif // CORE_RUNTIME_INCLUDE_C_LIBC_STDIO_H_
