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

#ifndef CORE_RUNTIME_INCLUDE_LIBC_STDLIB_H_
#define CORE_RUNTIME_INCLUDE_LIBC_STDLIB_H_

#if defined(__SPRT_BUILD) && __STDC_HOSTED__ == 1

#include_next <stdlib.h>

#else

#include <sprt/c/__sprt_stdlib.h>
#include <sprt/c/__sprt_wchar.h>

#ifndef NULL
#define NULL __SPRT_NULL
#endif

typedef __SPRT_ID(size_t) size_t;
typedef __SPRT_ID(div_t) div_t;
typedef __SPRT_ID(ldiv_t) ldiv_t;
typedef __SPRT_ID(lldiv_t) lldiv_t;

#ifndef __cplusplus
typedef __SPRT_ID(wchar_t) wchar_t;
#endif

#ifdef __sprt_malloca
#define _malloca(Sz) __sprt_malloca(Sz)
#define _freea(Ptr) __sprt_freea(Ptr)
#endif

#define RAND_MAX  __SPRT_RAND_MAX

__SPRT_BEGIN_DECL

#if __STDC_HOSTED__ == 0
size_t ___mb_cur_max_func() __SPRT_NOEXCEPT;
#endif

SPRT_UMBRELLA_FUNC
int atoi(const char *value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_atoi(value);
}
#endif

SPRT_UMBRELLA_FUNC
long atol(const char *value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_atol(value);
}
#endif

SPRT_UMBRELLA_FUNC
long long atoll(const char *value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_atoll(value);
}
#endif

SPRT_UMBRELLA_FUNC
double atof(const char *value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_atof(value);
}
#endif

SPRT_UMBRELLA_FUNC
float strtof(const char *__SPRT_RESTRICT str, char **__SPRT_RESTRICT value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_strtof(str, value);
}
#endif

SPRT_UMBRELLA_FUNC
double strtod(const char *__SPRT_RESTRICT str, char **__SPRT_RESTRICT value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_strtod(str, value);
}
#endif

SPRT_UMBRELLA_FUNC
long double strtold(const char *__SPRT_RESTRICT str, char **__SPRT_RESTRICT value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_strtold(str, value);
}
#endif

SPRT_UMBRELLA_FUNC
long strtol(const char *__SPRT_RESTRICT str, char **__SPRT_RESTRICT endp,
		int value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_strtol(str, endp, value);
}
#endif

SPRT_UMBRELLA_FUNC
unsigned long strtoul(const char *__SPRT_RESTRICT str, char **__SPRT_RESTRICT endp,
		int value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_strtoul(str, endp, value);
}
#endif

SPRT_UMBRELLA_FUNC
long long strtoll(const char *__SPRT_RESTRICT str, char **__SPRT_RESTRICT endp,
		int value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_strtoll(str, endp, value);
}
#endif

SPRT_UMBRELLA_FUNC
unsigned long long strtoull(const char *__SPRT_RESTRICT str, char **__SPRT_RESTRICT endp,
		int value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_strtoull(str, endp, value);
}
#endif

SPRT_UMBRELLA_FUNC
int rand(void) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_rand();
}
#endif

SPRT_UMBRELLA_FUNC
void srand(unsigned value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_srand(value);
}
#endif

SPRT_UMBRELLA_FUNC
void *malloc(size_t value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_malloc(value);
}
#endif

SPRT_UMBRELLA_FUNC
void *calloc(size_t a, size_t value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_calloc(a, value);
}
#endif

SPRT_UMBRELLA_FUNC
void *realloc(void *ptr, size_t value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_realloc(ptr, value);
}
#endif

SPRT_UMBRELLA_FUNC
void free(void *value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_free(value);
}
#endif

SPRT_UMBRELLA_FUNC
void free_sized(void *value, size_t size) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_free_sized(value, size);
}
#endif

SPRT_UMBRELLA_FUNC
void free_aligned_sized(void *value, size_t alignment, size_t size) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_free_aligned_sized(value, alignment, size);
}
#endif

/*
	WARNING: use aligned_free to safely free aligned memory.
	It's MSVC requirement, but safe to follow this rule everywhere
*/
SPRT_UMBRELLA_FUNC
int posix_memalign(void **ptr, size_t size, size_t align) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_posix_memalign(ptr, size, align);
}
#endif

/*
	WARNING: use aligned_free to safely free aligned memory.
	It's MSVC requirement, but safe to follow this rule everywhere
*/
SPRT_UMBRELLA_FUNC
void *aligned_alloc(size_t a, size_t value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_aligned_alloc(a, value);
}
#endif

SPRT_UMBRELLA_FUNC
void aligned_free(void *memblock) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	__sprt_aligned_free(memblock);
}
#endif

SPRT_UMBRELLA_FUNC
__SPRT_NORETURN void abort(void) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	__sprt_abort();
}
#endif

SPRT_UMBRELLA_FUNC
int atexit(void (*value)(void)) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_atexit(value);
}
#endif

SPRT_UMBRELLA_FUNC
__SPRT_NORETURN void exit(int value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	__sprt_exit(value);
}
#endif

SPRT_UMBRELLA_FUNC
__SPRT_NORETURN void _Exit(int value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	__sprt__Exit(value);
}
#endif

SPRT_UMBRELLA_FUNC
int at_quick_exit(void (*value)(void)) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_at_quick_exit(value);
}
#endif

SPRT_UMBRELLA_FUNC
__SPRT_NORETURN void quick_exit(int value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	__sprt_quick_exit(value);
}
#endif

SPRT_UMBRELLA_FUNC
char *getenv(const char *value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_getenv(value);
}
#endif

SPRT_UMBRELLA_FUNC
int system(const char *value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_system(value);
}
#endif

SPRT_UMBRELLA_FUNC
void *bsearch(const void *a, const void *b, size_t c, size_t d,
		int (*value)(const void *, const void *)) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_bsearch(a, b, c, d, value);
}
#endif

SPRT_UMBRELLA_FUNC
void qsort(void *a, size_t b, size_t c, int (*value)(const void *, const void *)) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_qsort(a, b, c, value);
}
#endif

SPRT_UMBRELLA_FUNC
int abs(int value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_abs(value);
}
#endif

SPRT_UMBRELLA_FUNC
long labs(long value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_labs(value);
}
#endif

SPRT_UMBRELLA_FUNC
long long llabs(long long value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_llabs(value);
}
#endif

SPRT_UMBRELLA_FUNC
div_t div(int a, int value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_div(a, value);
}
#endif

SPRT_UMBRELLA_FUNC
ldiv_t ldiv(long a, long value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_ldiv(a, value);
}
#endif

SPRT_UMBRELLA_FUNC
lldiv_t lldiv(long long a, long long value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_lldiv(a, value);
}
#endif

SPRT_UMBRELLA_FUNC
int setenv(const char *n, const char *v, int r) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_setenv(n, v, r);
}
#endif

SPRT_UMBRELLA_FUNC
int unsetenv(const char *n) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_unsetenv(n);
}
#endif

SPRT_UMBRELLA_FUNC
int mkstemp(char *tpl) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_mkstemp(tpl);
}
#endif

SPRT_UMBRELLA_FUNC
int mkostemp(char *tpl, int n) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_mkostemp(tpl, n);
}
#endif

SPRT_UMBRELLA_FUNC
char *mkdtemp(char *tpl) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_mkdtemp(tpl);
}
#endif

SPRT_UMBRELLA_FUNC
int getsubopt(char **opts, char *const *toks, char **vals) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_getsubopt(opts, toks, vals);
}
#endif

SPRT_UMBRELLA_FUNC
int rand_r(unsigned *v) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_rand_r(v);
}
#endif

SPRT_UMBRELLA_FUNC
char *realpath(const char *__SPRT_RESTRICT path, char *__SPRT_RESTRICT out) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_realpath(path, out);
}
#endif

SPRT_UMBRELLA_FUNC
long int random(void) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_random();
}
#endif

SPRT_UMBRELLA_FUNC
void srandom(unsigned int seed) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	__sprt_srandom(seed);
}
#endif

SPRT_UMBRELLA_FUNC
long strtol_l(const char *__SPRT_RESTRICT str, char **__SPRT_RESTRICT endp, int base,
		__SPRT_ID(locale_t) loc) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_strtol_l(str, endp, base, loc);
}
#endif

SPRT_UMBRELLA_FUNC
long long strtoll_l(const char *__SPRT_RESTRICT str, char **__SPRT_RESTRICT endp, int base,
		__SPRT_ID(locale_t) loc) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_strtoll_l(str, endp, base, loc);
}
#endif

SPRT_UMBRELLA_FUNC
unsigned long strtoul_l(const char *__SPRT_RESTRICT str, char **__SPRT_RESTRICT endp, int base,
		__SPRT_ID(locale_t) loc) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_strtoul_l(str, endp, base, loc);
}
#endif

SPRT_UMBRELLA_FUNC
unsigned long long strtoull_l(const char *__SPRT_RESTRICT str, char **__SPRT_RESTRICT endp,
		int base, __SPRT_ID(locale_t) loc) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_strtoull_l(str, endp, base, loc);
}
#endif

SPRT_UMBRELLA_FUNC
float strtof_l(const char *__SPRT_RESTRICT str, char **__SPRT_RESTRICT endp,
		__SPRT_ID(locale_t) loc) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_strtof_l(str, endp, loc);
}
#endif

SPRT_UMBRELLA_FUNC
double strtod_l(const char *__SPRT_RESTRICT str, char **__SPRT_RESTRICT endp,
		__SPRT_ID(locale_t) loc) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_strtod_l(str, endp, loc);
}
#endif

SPRT_UMBRELLA_FUNC
long double strtold_l(const char *__SPRT_RESTRICT str, char **__SPRT_RESTRICT endp,
		__SPRT_ID(locale_t) loc) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_strtold_l(str, endp, loc);
}
#endif

SPRT_UMBRELLA_FUNC
size_t mbstowcs(wchar_t *__dst, const char *__src, size_t __n) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __SPRT_ID(mbstowcs)(__dst, __src, __n);
}
#endif

SPRT_UMBRELLA_FUNC
int mbtowc(wchar_t *__wc_ptr, const char *__s, size_t __n) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __SPRT_ID(mbtowc)(__wc_ptr, __s, __n);
}
#endif

SPRT_UMBRELLA_FUNC
int wctomb(char *__dst, wchar_t __wc) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __SPRT_ID(wctomb)(__dst, __wc);
}
#endif

SPRT_UMBRELLA_FUNC
size_t wcstombs(char *__dst, const wchar_t *__src, size_t __n) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __SPRT_ID(wcstombs)(__dst, __src, __n);
}
#endif

#define MB_CUR_MAX __SPRT_ID(__ctype_get_mb_cur_max)()

__SPRT_END_DECL

#ifdef _LIBCPP_MSVCRT
#define _strtol_l strtol_l
#define _strtoll_l strtoll_l
#define _strtoul_l strtoul_l
#define _strtoull_l strtoull_l
#define _strtof_l strtof_l
#define _strtod_l strtod_l
#define _strtold_l strtold_l

#define _strtoi64_l strtoll_l
#define _strtoui64_l strtoull_l
#endif

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_STDLIB_H_
