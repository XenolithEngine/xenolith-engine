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

#define __SPRT_BUILD 1
#define _CRT_STDIO_ISO_WIDE_SPECIFIERS 1 // for libc++ compatibility

#include <sprt/c/__sprt_stdlib.h>
#include <sprt/c/__sprt_errno.h>
#include <sprt/c/__sprt_stddef.h>

#include <sprt/runtime/log.h>

#include <stdlib.h>
#include <locale.h>

#include "stdlib/env.cc"
#include "stdlib/getsubopt.cc"
#include "stdlib/byteswap.cc"
#include "stdlib/qsort_s.cc"

#if SPRT_APPLE
#include <xlocale.h>
#include <unistd.h>
#endif

#if SPRT_ANDROID
#include "../src/private/SPRTSpecific.h"
#endif

// musl provides the float strto*_l variants but not the integer ones; integer
// conversion is locale-independent, so fall back to the plain functions there
// (Android is bionic and SPRT_ANDROID, not SPRT_LINUX, and keeps the _l calls).
// NuttX libc has neither the integer nor the float _l variants.
#if (SPRT_LINUX && !defined(__GLIBC__)) || SPRT_NUTTX
#define __SPRT_NO_STRTO_INT_L 1
#else
#define __SPRT_NO_STRTO_INT_L 0
#endif

namespace sprt {

__SPRT_C_FUNC int __SPRT_ID(atoi_impl)(const char *str) { return ::atoi(str); }

__SPRT_C_FUNC long __SPRT_ID(atol_impl)(const char *str) { return ::atol(str); }

__SPRT_C_FUNC long long __SPRT_ID(atoll_impl)(const char *str) { return ::atoll(str); }

__SPRT_C_FUNC double __SPRT_ID(atof_impl)(const char *str) { return ::atof(str); }

#if __STDC_HOSTED__ == 1

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
#endif

__SPRT_C_FUNC void *__SPRT_ID(aligned_alloc)(size_t align, size_t size) __SPRT_NOEXCEPT {
#if SPRT_ANDROID
	if (align <= _Alignof(__SPRT_ID(max_align_t))) {
		return ::malloc(size);
	}
	if (sprt::platform::_aligned_alloc) {
		return sprt::platform::_aligned_alloc(align, size);
	}

	void *__result = nullptr;
	(void)::posix_memalign(&__result, align, size);
	if (__result) {
		return __result;
	}

	sprt::oslog::vprint(sprt::oslog::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (Android: API not available)");
	*__sprt___errno_location() = ENOSYS;
	return nullptr;
#else
	return ::aligned_alloc(align, size);
#endif
}

__SPRT_C_FUNC void __SPRT_ID(aligned_free)(void *memblock) {
#if SPRT_WINDOWS
	// Windows UCRT requires special handling, but we do not support in any more
	::aligned_free(memblock);
#else
	::free(memblock);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(system_impl)(const char *cmd) {
#if SPRT_IOS
	// system() is marked unavailable on iOS (no shell / process spawning); report
	// it as unsupported rather than failing to compile.
	(void)cmd;
	__sprt_errno = ENOSYS;
	return -1;
#else
	return ::system(cmd);
#endif
}

#if __STDC_HOSTED__ == 1
__SPRT_C_FUNC void *__SPRT_ID(bsearch_impl)(const void *key, const void *base, size_t nmemb,
		size_t size, int (*compar)(const void *, const void *)) {
	return ::bsearch(key, base, nmemb, size, compar);
}
#endif

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
	return posix_memalign(ptr, align, size);
}
__SPRT_C_FUNC int __SPRT_ID(mkstemp)(char *tpl) { return mkstemp(tpl); }
__SPRT_C_FUNC int __SPRT_ID(mkostemp)(char *tpl, int n) {
#if SPRT_NUTTX
	// NuttX libc has no mkostemp; fall back to mkstemp (the flags argument is
	// silently dropped — NuttX does not honour O_CLOEXEC on tempfile creation
	// anyway, callers must fcntl FD_CLOEXEC afterwards).
	(void)n;
	return mkstemp(tpl);
#else
	return mkostemp(tpl, n);
#endif
}
__SPRT_C_FUNC char *__SPRT_ID(mkdtemp)(char *tpl) { return mkdtemp(tpl); }

__SPRT_C_FUNC char *__SPRT_ID(
		realpath)(const char *__SPRT_RESTRICT path, char *__SPRT_RESTRICT out) {
	return realpath(path, out);
}

__SPRT_C_FUNC long __SPRT_ID(strtol_l)(const char *__SPRT_RESTRICT str, char **__SPRT_RESTRICT endp,
		int base, __SPRT_ID(locale_t) loc) {
#if __SPRT_NO_STRTO_INT_L
	(void)loc;
	return ::strtol(str, endp, base);
#else
	return ::strtol_l(str, endp, base, loc);
#endif
}
__SPRT_C_FUNC long long __SPRT_ID(strtoll_l)(const char *__SPRT_RESTRICT str,
		char **__SPRT_RESTRICT endp, int base, __SPRT_ID(locale_t) loc) {
#if __SPRT_NO_STRTO_INT_L
	(void)loc;
	return ::strtoll(str, endp, base);
#else
	return ::strtoll_l(str, endp, base, loc);
#endif
}
__SPRT_C_FUNC unsigned long __SPRT_ID(strtoul_l)(const char *__SPRT_RESTRICT str,
		char **__SPRT_RESTRICT endp, int base, __SPRT_ID(locale_t) loc) {
#if __SPRT_NO_STRTO_INT_L
	(void)loc;
	return ::strtoul(str, endp, base);
#else
	return ::strtoul_l(str, endp, base, loc);
#endif
}
__SPRT_C_FUNC unsigned long long __SPRT_ID(strtoull_l)(const char *__SPRT_RESTRICT str,
		char **__SPRT_RESTRICT endp, int base, __SPRT_ID(locale_t) loc) {
#if __SPRT_NO_STRTO_INT_L
	(void)loc;
	return ::strtoull(str, endp, base);
#else
	return ::strtoull_l(str, endp, base, loc);
#endif
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


__SPRT_C_FUNC __SPRT_ID(size_t)
		__SPRT_ID(mbstowcs)(wchar_t *__dst, const char *__src, __SPRT_ID(size_t) __n) {
	return ::mbstowcs(__dst, __src, __n);
}

__SPRT_C_FUNC int __SPRT_ID(mblen)(const char *__s, __SPRT_ID(size_t) __n) {
#if SPRT_ANDROID
	// Bionic only added mblen at API 26, but the runtime targets 24. mblen is
	// defined as mbtowc(NULL, ...) against a private state, and since the encoding
	// is stateless UTF-8 there is nothing to carry; mirror that (as libc_impl does)
	// through Bionic's mbtowc, which is available.
	return ::mbtowc(nullptr, __s, __n);
#else
	return ::mblen(__s, __n);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(mbtowc)(wchar_t *__wc_ptr, const char *__s, __SPRT_ID(size_t) __n) {
	return ::mbtowc(__wc_ptr, __s, __n);
}

__SPRT_C_FUNC int __SPRT_ID(wctomb)(char *__dst, wchar_t __wc) { return ::wctomb(__dst, __wc); }

__SPRT_C_FUNC __SPRT_ID(size_t)
		__SPRT_ID(wcstombs)(char *__dst, const wchar_t *__src, __SPRT_ID(size_t) __n) {
	return ::wcstombs(__dst, __src, __n);
}

__SPRT_C_FUNC __SPRT_ID(size_t) __SPRT_ID(__ctype_get_mb_cur_max)(void) {
#if SPRT_APPLE
	return ___mb_cur_max();
#elif SPRT_NUTTX
	// NuttX has no __ctype_get_mb_cur_max; MB_CUR_MAX is a compile-time macro.
	return MB_CUR_MAX;
#else
	return ::__ctype_get_mb_cur_max();
#endif
}

} // namespace sprt
