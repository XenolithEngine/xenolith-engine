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

#ifndef CORE_RUNTIME_INCLUDE_C___SPRT_STDLIB_H_
#define CORE_RUNTIME_INCLUDE_C___SPRT_STDLIB_H_

#include <c/bits/__sprt_def.h>
#include <c/bits/__sprt_size_t.h>
#include <c/bits/__sprt_null.h>
#include <c/cross/__sprt_locale.h>

__SPRT_BEGIN_DECL

#define __SPRT_WNOHANG    1
#define __SPRT_WUNTRACED  2

#define __SPRT_WEXITSTATUS(s) (((s) & 0xff00) >> 8)
#define __SPRT_WTERMSIG(s) ((s) & 0x7f)
#define __SPRT_WSTOPSIG(s) __SPRT_WEXITSTATUS(s)
#define __SPRT_WIFEXITED(s) (!__SPRT_WTERMSIG(s))
#define __SPRT_WIFSTOPPED(s) ((short)((((s)&0xffff)*0x1'0001U)>>8) > 0x7f00)
#define __SPRT_WIFSIGNALED(s) (((s)&0xffff)-1U < 0xffu)

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_alloca)
void *__SPRT_ID(alloca)(__SPRT_ID(size_t));
#define __sprt_alloca __builtin_alloca
#endif


#ifndef __clang__
#define __SPRT_MALLOC __SPRT_FALLBACK_ATTR(malloc(__SPRT_ID(free_impl)));
#else
#define __SPRT_MALLOC
#endif


SPRT_API int __SPRT_ID(atoi_impl)(const char *);
#define __sprt_atoi __SPRT_ID(atoi_impl)


SPRT_API long __SPRT_ID(atol_impl)(const char *);
#define __sprt_atol __SPRT_ID(atol_impl)


SPRT_API long long __SPRT_ID(atoll_impl)(const char *);
#define __sprt_atoll __SPRT_ID(atoll_impl)


SPRT_API double __SPRT_ID(atof_impl)(const char *);
#define __sprt_atof __SPRT_ID(atof_impl)


SPRT_API float __SPRT_ID(strtof_impl)(const char *__SPRT_RESTRICT, char **__SPRT_RESTRICT);
#define __sprt_strtof __SPRT_ID(strtof_impl)


SPRT_API double __SPRT_ID(strtod_impl)(const char *__SPRT_RESTRICT, char **__SPRT_RESTRICT);
#define __sprt_strtod __SPRT_ID(strtod_impl)


SPRT_API long double __SPRT_ID(strtold_impl)(const char *__SPRT_RESTRICT, char **__SPRT_RESTRICT);
#define __sprt_strtold __SPRT_ID(strtold_impl)


SPRT_API long __SPRT_ID(strtol_impl)(const char *__SPRT_RESTRICT, char **__SPRT_RESTRICT, int);
#define __sprt_strtol __SPRT_ID(strtol_impl)


SPRT_API unsigned long __SPRT_ID(
		strtoul_impl)(const char *__SPRT_RESTRICT, char **__SPRT_RESTRICT, int);
#define __sprt_strtoul __SPRT_ID(strtoul_impl)


SPRT_API long long __SPRT_ID(
		strtoll_impl)(const char *__SPRT_RESTRICT, char **__SPRT_RESTRICT, int);
#define __sprt_strtoll __SPRT_ID(strtoll_impl)


SPRT_API unsigned long long __SPRT_ID(
		strtoull_impl)(const char *__SPRT_RESTRICT, char **__SPRT_RESTRICT, int);
#define __sprt_strtoull __SPRT_ID(strtoull_impl)


SPRT_API int __SPRT_ID(rand_impl)(void);
#define __sprt_rand __SPRT_ID(rand_impl)


SPRT_API void __SPRT_ID(srand_impl)(unsigned);
#define __sprt_srand __SPRT_ID(srand_impl)

SPRT_API void __SPRT_ID(free_impl)(void *);
#define __sprt_free __SPRT_ID(free_impl)

SPRT_API void *__SPRT_ID(malloc_impl)(__SPRT_ID(size_t)) __SPRT_MALLOC;
#define __sprt_malloc __SPRT_ID(malloc_impl)


SPRT_API void *__SPRT_ID(calloc_impl)(__SPRT_ID(size_t), __SPRT_ID(size_t)) __SPRT_MALLOC;
#define __sprt_calloc __SPRT_ID(calloc_impl)


SPRT_API void *__SPRT_ID(realloc_impl)(void *, __SPRT_ID(size_t)) __SPRT_MALLOC;
#define __sprt_realloc __SPRT_ID(realloc_impl)

#if __SPRT_CONFIG_HAVE_ALLIGNED_ALLOC || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS
SPRT_API void *__SPRT_ID(aligned_alloc_impl)(__SPRT_ID(size_t), __SPRT_ID(size_t)) __SPRT_MALLOC;
#define __sprt_aligned_alloc __SPRT_ID(aligned_alloc_impl)
#endif

SPRT_API __SPRT_NORETURN void __SPRT_ID(abort_impl)(void);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_abort)
inline __SPRT_NORETURN void __SPRT_ID(abort)(void) { __builtin_abort(); }
#else
#define __sprt_abort __SPRT_ID(abort_impl)
#endif


SPRT_API int __SPRT_ID(atexit_impl)(void (*)(void));
#define __sprt_atexit __SPRT_ID(atexit_impl)


SPRT_API __SPRT_NORETURN void __SPRT_ID(exit_impl)(int);
#define __sprt_exit __SPRT_ID(exit_impl)


SPRT_API __SPRT_NORETURN void __SPRT_ID(_Exit_impl)(int);
#define __sprt__Exit __SPRT_ID(_Exit_impl)


SPRT_API int __SPRT_ID(at_quick_exit_impl)(void (*)(void));
#define __sprt_at_quick_exit __SPRT_ID(at_quick_exit_impl)


SPRT_API __SPRT_NORETURN void __SPRT_ID(quick_exit_impl)(int);
#define __sprt_quick_exit __SPRT_ID(quick_exit_impl)


SPRT_API char *__SPRT_ID(getenv_impl)(const char *);
#define __sprt_getenv __SPRT_ID(getenv_impl)


SPRT_API int __SPRT_ID(system_impl)(const char *);
#define __sprt_system __SPRT_ID(system_impl)


SPRT_API void *__SPRT_ID(bsearch_impl)(const void *, void *, __SPRT_ID(size_t), __SPRT_ID(size_t),
		int (*)(const void *, const void *));
#define __sprt_bsearch __SPRT_ID(bsearch_impl)


SPRT_API void __SPRT_ID(qsort_impl)(void *, __SPRT_ID(size_t), __SPRT_ID(size_t),
		int (*)(const void *, const void *));
#define __sprt_qsort __SPRT_ID(qsort_impl)


SPRT_API int __SPRT_ID(abs_impl)(int);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_abs)
inline int __SPRT_ID(abs)(int v) { return __builtin_abs(v); }
#else
#define __sprt_abs __SPRT_ID(abs_impl)
#endif


SPRT_API long __SPRT_ID(labs_impl)(long);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_labs)
inline long __SPRT_ID(labs)(long v) { return __builtin_labs(v); }
#else
#define __sprt_labs __SPRT_ID(labs_impl)
#endif


SPRT_API long long __SPRT_ID(llabs_impl)(long long);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_llabs)
inline long long __SPRT_ID(llabs)(long long v) { return __builtin_llabs(v); }
#else
#define __sprt_llabs __SPRT_ID(llabs_impl)
#endif


typedef struct {
	int quot, rem;
} __SPRT_ID(div_t);
typedef struct {
	long quot, rem;
} __SPRT_ID(ldiv_t);
typedef struct {
	long long quot, rem;
} __SPRT_ID(lldiv_t);


SPRT_API __SPRT_ID(div_t) __SPRT_ID(div_impl)(int, int);
#define __sprt_div __SPRT_ID(div_impl)


SPRT_API __SPRT_ID(ldiv_t) __SPRT_ID(ldiv_impl)(long, long);
#define __sprt_ldiv __SPRT_ID(ldiv_impl)


SPRT_API __SPRT_ID(lldiv_t) __SPRT_ID(lldiv_impl)(long long, long long);
#define __sprt_lldiv __SPRT_ID(lldiv_impl)

SPRT_API int __SPRT_ID(posix_memalign)(void **, __SPRT_ID(size_t), __SPRT_ID(size_t));
SPRT_API int __SPRT_ID(setenv)(const char *, const char *, int);
SPRT_API int __SPRT_ID(unsetenv)(const char *);
SPRT_API int __SPRT_ID(mkstemp)(char *);
SPRT_API int __SPRT_ID(mkostemp)(char *, int);
SPRT_API char *__SPRT_ID(mkdtemp)(char *);
SPRT_API int __SPRT_ID(getsubopt)(char **, char *const *, char **);
SPRT_API int __SPRT_ID(rand_r)(unsigned *);
SPRT_API char *__SPRT_ID(realpath)(const char *__restrict, char *__restrict);
SPRT_API long int __SPRT_ID(random)(void);
SPRT_API void __SPRT_ID(srandom)(unsigned int);

SPRT_API long __SPRT_ID(
		strtol_l)(const char *__SPRT_RESTRICT, char **__SPRT_RESTRICT, int, __SPRT_ID(locale_t));
SPRT_API long long __SPRT_ID(
		strtoll_l)(const char *__SPRT_RESTRICT, char **__SPRT_RESTRICT, int, __SPRT_ID(locale_t));
SPRT_API unsigned long __SPRT_ID(strtoul_l)(const char *__SPRT_RESTRICT __s, char **__SPRT_RESTRICT,
		int, __SPRT_ID(locale_t));
SPRT_API unsigned long long __SPRT_ID(strtoull_l)(const char *__SPRT_RESTRICT __s,
		char **__SPRT_RESTRICT, int, __SPRT_ID(locale_t));
SPRT_API float __SPRT_ID(
		strtof_l)(const char *__SPRT_RESTRICT, char **__SPRT_RESTRICT, __SPRT_ID(locale_t));
SPRT_API double __SPRT_ID(
		strtod_l)(const char *__SPRT_RESTRICT, char **__SPRT_RESTRICT, __SPRT_ID(locale_t));
SPRT_API long double __SPRT_ID(
		strtold_l)(const char *__SPRT_RESTRICT, char **__SPRT_RESTRICT, __SPRT_ID(locale_t));

__SPRT_END_DECL

#endif // CORE_RUNTIME_INCLUDE_C___SPRT_STDLIB_H_
