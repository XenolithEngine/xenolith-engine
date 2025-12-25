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

#ifdef __SPRT_BUILD

#include_next <stdlib.h>

#else

#include <c/__sprt_stdlib.h>

typedef __SPRT_ID(size_t) size_t;
typedef __SPRT_ID(div_t) div_t;
typedef __SPRT_ID(ldiv_t) ldiv_t;
typedef __SPRT_ID(lldiv_t) lldiv_t;

#ifndef alloca
#define alloca __sprt_alloca
#endif

__SPRT_BEGIN_DECL

SPRT_FORCEINLINE inline int atoi(const char *value) { return __sprt_atoi(value); }
SPRT_FORCEINLINE inline long atol(const char *value) { return __sprt_atol(value); }
SPRT_FORCEINLINE inline long long atoll(const char *value) { return __sprt_atoll(value); }
SPRT_FORCEINLINE inline double atof(const char *value) { return __sprt_atof(value); }

SPRT_FORCEINLINE inline float strtof(const char *__SPRT_RESTRICT str,
		char **__SPRT_RESTRICT value) {
	return __sprt_strtof(str, value);
}

SPRT_FORCEINLINE inline double strtod(const char *__SPRT_RESTRICT str,
		char **__SPRT_RESTRICT value) {
	return __sprt_strtod(str, value);
}
SPRT_FORCEINLINE inline long double strtold(const char *__SPRT_RESTRICT str,
		char **__SPRT_RESTRICT value) {
	return __sprt_strtold(str, value);
}

SPRT_FORCEINLINE inline long strtol(const char *__SPRT_RESTRICT str, char **__SPRT_RESTRICT endp,
		int value) {
	return __sprt_strtol(str, endp, value);
}
SPRT_FORCEINLINE inline unsigned long strtoul(const char *__SPRT_RESTRICT str,
		char **__SPRT_RESTRICT endp, int value) {
	return __sprt_strtoul(str, endp, value);
}
SPRT_FORCEINLINE inline long long strtoll(const char *__SPRT_RESTRICT str,
		char **__SPRT_RESTRICT endp, int value) {
	return __sprt_strtoll(str, endp, value);
}
SPRT_FORCEINLINE inline unsigned long long strtoull(const char *__SPRT_RESTRICT str,
		char **__SPRT_RESTRICT endp, int value) {
	return __sprt_strtoull(str, endp, value);
}

SPRT_FORCEINLINE inline int rand(void) { return __sprt_rand(); }
SPRT_FORCEINLINE inline void srand(unsigned value) { return __sprt_srand(value); }

SPRT_FORCEINLINE inline void *malloc(size_t value) { return __sprt_malloc(value); }
SPRT_FORCEINLINE inline void *calloc(size_t a, size_t value) { return __sprt_calloc(a, value); }
SPRT_FORCEINLINE inline void *realloc(void *ptr, size_t value) {
	return __sprt_realloc(ptr, value);
}
SPRT_FORCEINLINE inline void free(void *value) { return __sprt_free(value); }

#if __SPRT_CONFIG_HAVE_ALLIGNED_ALLOC || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS
SPRT_FORCEINLINE inline void *aligned_alloc(size_t a, size_t value) {
	return __sprt_aligned_alloc(a, value);
}
#endif

SPRT_FORCEINLINE inline __SPRT_NORETURN void abort(void) { __sprt_abort(); }

SPRT_FORCEINLINE inline int atexit(void (*value)(void)) { return __sprt_atexit(value); }

SPRT_FORCEINLINE inline __SPRT_NORETURN void exit(int value) { __sprt_exit(value); }
SPRT_FORCEINLINE inline __SPRT_NORETURN void _Exit(int value) { __sprt__Exit(value); }
SPRT_FORCEINLINE inline int at_quick_exit(void (*value)(void)) {
	return __sprt_at_quick_exit(value);
}
SPRT_FORCEINLINE inline __SPRT_NORETURN void quick_exit(int value) { __sprt_quick_exit(value); }

SPRT_FORCEINLINE inline char *getenv(const char *value) { return __sprt_getenv(value); }

SPRT_FORCEINLINE inline int system(const char *value) { return __sprt_system(value); }

SPRT_FORCEINLINE inline void *bsearch(const void *a, void *b, size_t c, size_t d,
		int (*value)(const void *, const void *)) {
	return __sprt_bsearch(a, b, c, d, value);
}
SPRT_FORCEINLINE inline void qsort(void *a, size_t b, size_t c,
		int (*value)(const void *, const void *)) {
	return __sprt_qsort(a, b, c, value);
}

SPRT_FORCEINLINE inline int abs(int value) { return __sprt_abs(value); }
SPRT_FORCEINLINE inline long labs(long value) { return __sprt_labs(value); }
SPRT_FORCEINLINE inline long long llabs(long long value) { return __sprt_llabs(value); }

SPRT_FORCEINLINE inline div_t div(int a, int value) { return __sprt_div(a, value); }
SPRT_FORCEINLINE inline ldiv_t ldiv(long a, long value) { return __sprt_ldiv(a, value); }
SPRT_FORCEINLINE inline lldiv_t lldiv(long long a, long long value) {
	return __sprt_lldiv(a, value);
}


SPRT_FORCEINLINE inline int posix_memalign(void **ptr, size_t size, size_t align) {
	return __sprt_posix_memalign(ptr, size, align);
}
SPRT_FORCEINLINE inline int setenv(const char *n, const char *v, int r) {
	return __sprt_setenv(n, v, r);
}
SPRT_FORCEINLINE inline int unsetenv(const char *n) { return __sprt_unsetenv(n); }
SPRT_FORCEINLINE inline int mkstemp(char *tpl) { return __sprt_mkstemp(tpl); }
SPRT_FORCEINLINE inline int mkostemp(char *tpl, int n) { return __sprt_mkostemp(tpl, n); }
SPRT_FORCEINLINE inline char *mkdtemp(char *tpl) { return __sprt_mkdtemp(tpl); }
SPRT_FORCEINLINE inline int getsubopt(char **opts, char *const *toks, char **vals) {
	return __sprt_getsubopt(opts, toks, vals);
}
SPRT_FORCEINLINE inline int rand_r(unsigned *v) { return __sprt_rand_r(v); }
SPRT_FORCEINLINE inline char *realpath(const char *__SPRT_RESTRICT path,
		char *__SPRT_RESTRICT out) {
	return __sprt_realpath(path, out);
}
SPRT_FORCEINLINE inline long int random(void) { return __sprt_random(); }
SPRT_FORCEINLINE inline void srandom(unsigned int seed) { __sprt_srandom(seed); }

SPRT_FORCEINLINE inline long strtol_l(const char *__SPRT_RESTRICT str, char **__SPRT_RESTRICT endp,
		int base, __SPRT_ID(locale_t) loc) {
	return __sprt_strtol_l(str, endp, base, loc);
}
SPRT_FORCEINLINE inline long long strtoll_l(const char *__SPRT_RESTRICT str,
		char **__SPRT_RESTRICT endp, int base, __SPRT_ID(locale_t) loc) {
	return __sprt_strtoll_l(str, endp, base, loc);
}
SPRT_FORCEINLINE inline unsigned long strtoul_l(const char *__SPRT_RESTRICT str,
		char **__SPRT_RESTRICT endp, int base, __SPRT_ID(locale_t) loc) {
	return __sprt_strtoul_l(str, endp, base, loc);
}
SPRT_FORCEINLINE inline unsigned long long strtoull_l(const char *__SPRT_RESTRICT str,
		char **__SPRT_RESTRICT endp, int base, __SPRT_ID(locale_t) loc) {
	return __sprt_strtoull_l(str, endp, base, loc);
}
SPRT_FORCEINLINE inline float strtof_l(const char *__SPRT_RESTRICT str, char **__SPRT_RESTRICT endp,
		__SPRT_ID(locale_t) loc) {
	return __sprt_strtof_l(str, endp, loc);
}
SPRT_FORCEINLINE inline double strtod_l(const char *__SPRT_RESTRICT str,
		char **__SPRT_RESTRICT endp, __SPRT_ID(locale_t) loc) {
	return __sprt_strtod_l(str, endp, loc);
}
SPRT_FORCEINLINE inline long double strtold_l(const char *__SPRT_RESTRICT str,
		char **__SPRT_RESTRICT endp, __SPRT_ID(locale_t) loc) {
	return __sprt_strtold_l(str, endp, loc);
}

__SPRT_END_DECL

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_STDLIB_H_
