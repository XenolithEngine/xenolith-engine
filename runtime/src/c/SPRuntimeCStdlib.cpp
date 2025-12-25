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

#include "private/SPRTFilename.h"
#define __SPRT_BUILD 1

#include <c/__sprt_stdlib.h>
#include <c/__sprt_errno.h>

#include "SPRuntimeLog.h"

#include <stdlib.h>

#if __SPRT_CONFIG_HAVE_ALLIGNED_ALLOC
__SPRT_C_FUNC void *aligned_alloc(size_t, size_t);
#endif

namespace sprt {

__SPRT_C_FUNC int __SPRT_ID(atoi_impl)(const char *str) { return ::atoi(str); }

__SPRT_C_FUNC long __SPRT_ID(atol_impl)(const char *str) { return ::atol(str); }

__SPRT_C_FUNC long long __SPRT_ID(atoll_impl)(const char *str) { return ::atoll(str); }

__SPRT_C_FUNC double __SPRT_ID(atof_impl)(const char *str) { return ::atof(str); }


__SPRT_C_FUNC float __SPRT_ID(
		strtof_impl)(const char *__SPRT_RESTRICT buf, char **__SPRT_RESTRICT out) {
	return ::strtof(buf, out);
}

__SPRT_C_FUNC double __SPRT_ID(
		strtod_impl)(const char *__SPRT_RESTRICT buf, char **__SPRT_RESTRICT out) {
	return ::strtod(buf, out);
}

__SPRT_C_FUNC long double __SPRT_ID(
		strtold_impl)(const char *__SPRT_RESTRICT buf, char **__SPRT_RESTRICT out) {
	return ::strtold(buf, out);
}


__SPRT_C_FUNC long __SPRT_ID(
		strtol_impl)(const char *__SPRT_RESTRICT buf, char **__SPRT_RESTRICT out, int base) {
	return ::strtol(buf, out, base);
}

__SPRT_C_FUNC unsigned long __SPRT_ID(
		strtoul_impl)(const char *__SPRT_RESTRICT buf, char **__SPRT_RESTRICT out, int base) {
	return ::strtoul(buf, out, base);
}

__SPRT_C_FUNC long long __SPRT_ID(
		strtoll_impl)(const char *__SPRT_RESTRICT buf, char **__SPRT_RESTRICT out, int base) {
	return ::strtoll(buf, out, base);
}

__SPRT_C_FUNC unsigned long long __SPRT_ID(
		strtoull_impl)(const char *__SPRT_RESTRICT buf, char **__SPRT_RESTRICT out, int base) {
	return ::strtoull(buf, out, base);
}


__SPRT_C_FUNC int __SPRT_ID(rand_impl)(void) { return ::rand(); }

__SPRT_C_FUNC void __SPRT_ID(srand_impl)(unsigned base) { return ::srand(base); }


__SPRT_C_FUNC void *__SPRT_ID(malloc_impl)(size_t size) { return ::malloc(size); }

__SPRT_C_FUNC void *__SPRT_ID(calloc_impl)(size_t n, size_t size) { return ::calloc(n, size); }

__SPRT_C_FUNC void *__SPRT_ID(realloc_impl)(void *ptr, size_t size) { return ::realloc(ptr, size); }

__SPRT_C_FUNC void __SPRT_ID(free_impl)(void *ptr) { return ::free(ptr); }

__SPRT_C_FUNC void *__SPRT_ID(aligned_alloc_impl)(size_t align, size_t size) {
#if !__SPRT_CONFIG_HAVE_ALLIGNED_ALLOC
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_ALLIGNED_ALLOC)");
	*__sprt___errno_location() = ENOSYS;
	return nullptr;
#else
	return ::aligned_alloc(align, size);
#endif
}

__SPRT_C_FUNC __SPRT_NORETURN void __SPRT_ID(abort_impl)(void) { ::abort(); }

__SPRT_C_FUNC int __SPRT_ID(atexit_impl)(void (*cb)(void)) { return ::atexit(cb); }

__SPRT_C_FUNC __SPRT_NORETURN void __SPRT_ID(exit_impl)(int ret) { ::exit(ret); }

__SPRT_NORETURN void _Exit(int);
__SPRT_C_FUNC int __SPRT_ID(at_quick_exit_impl)(void (*cb)(void)) { return ::at_quick_exit(cb); }

__SPRT_C_FUNC __SPRT_NORETURN void __SPRT_ID(quick_exit_impl)(int ret) { ::quick_exit(ret); }

__SPRT_C_FUNC char *__SPRT_ID(getenv_impl)(const char *name) { return ::getenv(name); }

__SPRT_C_FUNC int __SPRT_ID(system_impl)(const char *cmd) { return ::system(cmd); }


__SPRT_C_FUNC void *__SPRT_ID(bsearch_impl)(const void *key, void *base, size_t nmemb, size_t size,
		int (*compar)(const void *, const void *)) {
	return ::bsearch(key, base, nmemb, size, compar);
}

__SPRT_C_FUNC void __SPRT_ID(qsort_impl)(void *array, size_t n, size_t size,
		int (*comparator)(const void *lhs, const void *rhs)) {
	::qsort(array, n, size, comparator);
}


__SPRT_C_FUNC int __SPRT_ID(abs_impl)(int v) { return ::abs(v); }

__SPRT_C_FUNC long __SPRT_ID(labs_impl)(long v) { return ::labs(v); }

__SPRT_C_FUNC long long __SPRT_ID(llabs_impl)(long long v) { return ::llabs(v); }


__SPRT_C_FUNC __SPRT_ID(div_t) __SPRT_ID(div_impl)(int a, int b) {
	auto ret = ::div(a, b);
	return __SPRT_ID(div_t){ret.quot, ret.rem};
}

__SPRT_C_FUNC __SPRT_ID(ldiv_t) __SPRT_ID(ldiv_impl)(long a, long b) {
	auto ret = ::ldiv(a, b);
	return __SPRT_ID(ldiv_t){ret.quot, ret.rem};
}

__SPRT_C_FUNC __SPRT_ID(lldiv_t) __SPRT_ID(lldiv_impl)(long long a, long long b) {
	auto ret = ::lldiv(a, b);
	return __SPRT_ID(lldiv_t){ret.quot, ret.rem};
}


__SPRT_C_FUNC int __SPRT_ID(
		posix_memalign)(void **ptr, __SPRT_ID(size_t) size, __SPRT_ID(size_t) align) {
	return ::posix_memalign(ptr, size, align);
}
__SPRT_C_FUNC int __SPRT_ID(setenv)(const char *n, const char *v, int r) {
	return ::setenv(n, v, r);
}
__SPRT_C_FUNC int __SPRT_ID(unsetenv)(const char *n) { return ::unsetenv(n); }
__SPRT_C_FUNC int __SPRT_ID(mkstemp)(char *tpl) { return ::mkstemp(tpl); }
__SPRT_C_FUNC int __SPRT_ID(mkostemp)(char *tpl, int n) { return ::mkostemp(tpl, n); }
__SPRT_C_FUNC char *__SPRT_ID(mkdtemp)(char *tpl) { return ::mkdtemp(tpl); }
__SPRT_C_FUNC int __SPRT_ID(getsubopt)(char **opts, char *const *toks, char **vals) {
	return ::getsubopt(opts, toks, vals);
}
__SPRT_C_FUNC int __SPRT_ID(rand_r)(unsigned *v) { return ::rand_r(v); }

__SPRT_C_FUNC char *__SPRT_ID(
		realpath)(const char *__SPRT_RESTRICT path, char *__SPRT_RESTRICT out) {
	return internal::performWithNativePath(path, [&](const char *target) {
		// call with native path
		return internal::performWithPosixePath(::realpath(target, out),
				[](const char *ptr) { return (char *)ptr; }, (char *)nullptr);
	}, (char *)nullptr);
}
__SPRT_C_FUNC long int __SPRT_ID(random)(void) { return ::random(); }
__SPRT_C_FUNC void __SPRT_ID(srandom)(unsigned int seed) { ::srandom(seed); }

__SPRT_C_FUNC long __SPRT_ID(strtol_l)(const char *__SPRT_RESTRICT str, char **__SPRT_RESTRICT endp,
		int base, __SPRT_ID(locale_t) loc) {
	return ::strtol_l(str, endp, base, loc);
}
__SPRT_C_FUNC long long __SPRT_ID(strtoll_l)(const char *__SPRT_RESTRICT str,
		char **__SPRT_RESTRICT endp, int base, __SPRT_ID(locale_t) loc) {
	return ::strtoll_l(str, endp, base, loc);
}
__SPRT_C_FUNC unsigned long __SPRT_ID(strtoul_l)(const char *__SPRT_RESTRICT str,
		char **__SPRT_RESTRICT endp, int base, __SPRT_ID(locale_t) loc) {
	return ::strtoul_l(str, endp, base, loc);
}
__SPRT_C_FUNC unsigned long long __SPRT_ID(strtoull_l)(const char *__SPRT_RESTRICT str,
		char **__SPRT_RESTRICT endp, int base, __SPRT_ID(locale_t) loc) {
	return ::strtoull_l(str, endp, base, loc);
}
__SPRT_C_FUNC float __SPRT_ID(strtof_l)(const char *__SPRT_RESTRICT str,
		char **__SPRT_RESTRICT endp, __SPRT_ID(locale_t) loc) {
	return ::strtof_l(str, endp, loc);
}
__SPRT_C_FUNC double __SPRT_ID(strtod_l)(const char *__SPRT_RESTRICT str,
		char **__SPRT_RESTRICT endp, __SPRT_ID(locale_t) loc) {
	return ::strtod_l(str, endp, loc);
}
__SPRT_C_FUNC long double __SPRT_ID(strtold_l)(const char *__SPRT_RESTRICT str,
		char **__SPRT_RESTRICT endp, __SPRT_ID(locale_t) loc) {
	return ::strtold_l(str, endp, loc);
}

} // namespace sprt
