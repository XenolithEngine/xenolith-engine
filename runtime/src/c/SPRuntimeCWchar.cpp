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

#include <c/__sprt_wchar.h>
#include <c/__sprt_stdarg.h>

#include <wchar.h>
#include <time.h>

#include "private/SPRTTime.h"

namespace sprt {

__SPRT_C_FUNC __SPRT_WCHAR_T *__SPRT_ID(
		wcscpy)(__SPRT_WCHAR_T *__SPRT_RESTRICT a, const __SPRT_WCHAR_T *__SPRT_RESTRICT b) {
	return ::wcscpy(a, b);
}

__SPRT_C_FUNC __SPRT_WCHAR_T *__SPRT_ID(wcsncpy)(__SPRT_WCHAR_T *__SPRT_RESTRICT a,
		const __SPRT_WCHAR_T *__SPRT_RESTRICT b, __SPRT_ID(size_t) s) {
	return ::wcsncpy(a, b, s);
}

__SPRT_C_FUNC __SPRT_WCHAR_T *__SPRT_ID(
		wcscat)(__SPRT_WCHAR_T *__SPRT_RESTRICT a, const __SPRT_WCHAR_T *__SPRT_RESTRICT b) {
	return wcscat(a, b);
}

__SPRT_C_FUNC __SPRT_WCHAR_T *__SPRT_ID(wcsncat)(__SPRT_WCHAR_T *__SPRT_RESTRICT a,
		const __SPRT_WCHAR_T *__SPRT_RESTRICT b, __SPRT_ID(size_t) s) {
	return ::wcsncat(a, b, s);
}

__SPRT_C_FUNC int __SPRT_ID(wcscmp)(const __SPRT_WCHAR_T *a, const __SPRT_WCHAR_T *b) {
	return wcscmp(a, b);
}

__SPRT_C_FUNC int __SPRT_ID(
		wcsncmp)(const __SPRT_WCHAR_T *a, const __SPRT_WCHAR_T *b, __SPRT_ID(size_t) s) {
	return ::wcsncmp(a, b, s);
}

__SPRT_C_FUNC int __SPRT_ID(wcscoll)(const __SPRT_WCHAR_T *a, const __SPRT_WCHAR_T *b) {
	return ::wcscoll(a, b);
}

__SPRT_C_FUNC __SPRT_ID(size_t) __SPRT_ID(wcsxfrm)(__SPRT_WCHAR_T *__SPRT_RESTRICT a,
		const __SPRT_WCHAR_T *__SPRT_RESTRICT b, __SPRT_ID(size_t) s) {
	return ::wcsxfrm(a, b, s);
}

__SPRT_C_FUNC const __SPRT_WCHAR_T *__SPRT_ID(wcschr)(const __SPRT_WCHAR_T *a, __SPRT_WCHAR_T b) {
	return ::wcschr(a, b);
}

__SPRT_C_FUNC const __SPRT_WCHAR_T *__SPRT_ID(wcsrchr)(const __SPRT_WCHAR_T *a, __SPRT_WCHAR_T b) {
	return ::wcsrchr(a, b);
}

__SPRT_C_FUNC __SPRT_ID(size_t)
		__SPRT_ID(wcscspn)(const __SPRT_WCHAR_T *a, const __SPRT_WCHAR_T *b) {
	return ::wcscspn(a, b);
}
__SPRT_C_FUNC __SPRT_ID(size_t)
		__SPRT_ID(wcsspn)(const __SPRT_WCHAR_T *a, const __SPRT_WCHAR_T *b) {
	return ::wcsspn(a, b);
}

__SPRT_C_FUNC const __SPRT_WCHAR_T *__SPRT_ID(
		wcspbrk)(const __SPRT_WCHAR_T *a, const __SPRT_WCHAR_T *b) {
	return ::wcspbrk(a, b);
}

__SPRT_C_FUNC __SPRT_WCHAR_T *__SPRT_ID(wcstok)(__SPRT_WCHAR_T *__SPRT_RESTRICT a,
		const __SPRT_WCHAR_T *__SPRT_RESTRICT b, __SPRT_WCHAR_T **__SPRT_RESTRICT c) {
	return ::wcstok(a, b, c);
}

__SPRT_C_FUNC __SPRT_ID(size_t) __SPRT_ID(wcslen)(const __SPRT_WCHAR_T *v) { return wcslen(v); }

__SPRT_C_FUNC const __SPRT_WCHAR_T *__SPRT_ID(
		wcsstr)(const __SPRT_WCHAR_T *__SPRT_RESTRICT a, const __SPRT_WCHAR_T *__SPRT_RESTRICT b) {
	return ::wcsstr(a, b);
}

__SPRT_C_FUNC const __SPRT_WCHAR_T *__SPRT_ID(
		wcswcs)(const __SPRT_WCHAR_T *a, const __SPRT_WCHAR_T *b) {
	return ::wcswcs(a, b);
}

__SPRT_C_FUNC const __SPRT_WCHAR_T *__SPRT_ID(
		wmemchr)(const __SPRT_WCHAR_T *a, __SPRT_WCHAR_T b, __SPRT_ID(size_t) s) {
	return ::wmemchr(a, b, s);
}

__SPRT_C_FUNC
int __SPRT_ID(wmemcmp)(const __SPRT_WCHAR_T *a, const __SPRT_WCHAR_T *b, __SPRT_ID(size_t) s) {
	return ::wmemcmp(a, b, s);
}

__SPRT_C_FUNC __SPRT_WCHAR_T *__SPRT_ID(wmemcpy)(__SPRT_WCHAR_T *__SPRT_RESTRICT a,
		const __SPRT_WCHAR_T *__SPRT_RESTRICT b, __SPRT_ID(size_t) s) {
	return ::wmemcpy(a, b, s);
}

__SPRT_C_FUNC __SPRT_WCHAR_T *__SPRT_ID(
		wmemmove)(__SPRT_WCHAR_T *a, const __SPRT_WCHAR_T *b, __SPRT_ID(size_t) s) {
	return ::wmemmove(a, b, s);
}

__SPRT_C_FUNC __SPRT_WCHAR_T *__SPRT_ID(
		wmemset)(__SPRT_WCHAR_T *a, __SPRT_WCHAR_T c, __SPRT_ID(size_t) s) {
	return ::wmemset(a, c, s);
}

__SPRT_C_FUNC __SPRT_ID(wint_t) __SPRT_ID(btowc)(int val) { return ::btowc(val); }

__SPRT_C_FUNC int __SPRT_ID(wctob)(__SPRT_ID(wint_t) val) { return ::wctob(val); }

__SPRT_C_FUNC int __SPRT_ID(mbsinit)(const __SPRT_MBSTATE_NAME *val) {
	return ::mbsinit((const ::mbstate_t *)val);
}

__SPRT_C_FUNC __SPRT_ID(size_t)
		__SPRT_ID(mbrtowc)(__SPRT_WCHAR_T *__SPRT_RESTRICT a, const char *__SPRT_RESTRICT b,
				__SPRT_ID(size_t) s, __SPRT_MBSTATE_NAME *__SPRT_RESTRICT state) {
	return ::mbrtowc(a, b, s, (::mbstate_t *)state);
}

__SPRT_C_FUNC __SPRT_ID(size_t) __SPRT_ID(wcrtomb)(char *__SPRT_RESTRICT a, __SPRT_WCHAR_T c,
		__SPRT_MBSTATE_NAME *__SPRT_RESTRICT state) {
	return ::wcrtomb(a, c, (::mbstate_t *)state);
}

__SPRT_C_FUNC __SPRT_ID(size_t) __SPRT_ID(mbrlen)(const char *__SPRT_RESTRICT a,
		__SPRT_ID(size_t) c, __SPRT_MBSTATE_NAME *__SPRT_RESTRICT state) {
	return ::mbrlen(a, c, (::mbstate_t *)state);
}

__SPRT_C_FUNC __SPRT_ID(size_t)
		__SPRT_ID(mbsrtowcs)(__SPRT_WCHAR_T *__SPRT_RESTRICT a, const char **__SPRT_RESTRICT ret,
				__SPRT_ID(size_t) s, __SPRT_MBSTATE_NAME *__SPRT_RESTRICT state) {
	return ::mbsrtowcs(a, ret, s, (::mbstate_t *)state);
}

__SPRT_C_FUNC __SPRT_ID(size_t)
		__SPRT_ID(wcsrtombs)(char *__SPRT_RESTRICT a, const __SPRT_WCHAR_T **__SPRT_RESTRICT ret,
				__SPRT_ID(size_t) s, __SPRT_MBSTATE_NAME *__SPRT_RESTRICT state) {
	return ::wcsrtombs(a, ret, s, (::mbstate_t *)state);
}

__SPRT_C_FUNC float __SPRT_ID(
		wcstof)(const __SPRT_WCHAR_T *__SPRT_RESTRICT a, __SPRT_WCHAR_T **__SPRT_RESTRICT ret) {
	return ::wcstof(a, ret);
}

__SPRT_C_FUNC double __SPRT_ID(
		wcstod)(const __SPRT_WCHAR_T *__SPRT_RESTRICT a, __SPRT_WCHAR_T **__SPRT_RESTRICT ret) {
	return ::wcstod(a, ret);
}

__SPRT_C_FUNC long double __SPRT_ID(
		wcstold)(const __SPRT_WCHAR_T *__SPRT_RESTRICT a, __SPRT_WCHAR_T **__SPRT_RESTRICT ret) {
	return ::wcstold(a, ret);
}

__SPRT_C_FUNC long __SPRT_ID(wcstol)(const __SPRT_WCHAR_T *__SPRT_RESTRICT a,
		__SPRT_WCHAR_T **__SPRT_RESTRICT ret, int base) {
	return ::wcstol(a, ret, base);
}

__SPRT_C_FUNC unsigned long __SPRT_ID(wcstoul)(const __SPRT_WCHAR_T *__SPRT_RESTRICT a,
		__SPRT_WCHAR_T **__SPRT_RESTRICT ret, int base) {
	return ::wcstoul(a, ret, base);
}

__SPRT_C_FUNC long long __SPRT_ID(wcstoll)(const __SPRT_WCHAR_T *__SPRT_RESTRICT a,
		__SPRT_WCHAR_T **__SPRT_RESTRICT ret, int base) {
	return ::wcstoll(a, ret, base);
}

__SPRT_C_FUNC unsigned long long __SPRT_ID(wcstoull)(const __SPRT_WCHAR_T *__SPRT_RESTRICT a,
		__SPRT_WCHAR_T **__SPRT_RESTRICT ret, int base) {
	return ::wcstoull(a, ret, base);
}

__SPRT_C_FUNC int __SPRT_ID(fwide)(__SPRT_ID(FILE) * f, int c) { return ::fwide(f, c); }

__SPRT_C_FUNC int __SPRT_ID(wprintf)(const __SPRT_WCHAR_T *__SPRT_RESTRICT fmt, ...) {
	__sprt_va_list list;
	__sprt_va_start(list, fmt);

	auto ret = ::vwprintf(fmt, list);

	__sprt_va_end(list);
	return ret;
}

__SPRT_C_FUNC int __SPRT_ID(fwprintf)(__SPRT_ID(FILE) * __SPRT_RESTRICT f,
		const __SPRT_WCHAR_T *__SPRT_RESTRICT fmt, ...) {
	__sprt_va_list list;
	__sprt_va_start(list, fmt);

	auto ret = ::vfwprintf(f, fmt, list);

	__sprt_va_end(list);
	return ret;
}

__SPRT_C_FUNC int __SPRT_ID(swprintf)(__SPRT_WCHAR_T *__SPRT_RESTRICT buf, __SPRT_ID(size_t) size,
		const __SPRT_WCHAR_T *__SPRT_RESTRICT fmt, ...) {
	__sprt_va_list list;
	__sprt_va_start(list, fmt);

	auto ret = ::vswprintf(buf, size, fmt, list);

	__sprt_va_end(list);
	return ret;
}

__SPRT_C_FUNC int __SPRT_ID(
		vwprintf)(const __SPRT_WCHAR_T *__SPRT_RESTRICT fmt, __sprt_va_list list) {
	return ::vwprintf(fmt, list);
}

__SPRT_C_FUNC int __SPRT_ID(vfwprintf)(__SPRT_ID(FILE) * __SPRT_RESTRICT f,
		const __SPRT_WCHAR_T *__SPRT_RESTRICT fmt, __sprt_va_list list) {
	return ::vfwprintf(f, fmt, list);
}

__SPRT_C_FUNC int __SPRT_ID(vswprintf)(__SPRT_WCHAR_T *__SPRT_RESTRICT buf, __SPRT_ID(size_t) size,
		const __SPRT_WCHAR_T *__SPRT_RESTRICT fmt, __sprt_va_list list) {
	return ::vswprintf(buf, size, fmt, list);
}

__SPRT_C_FUNC int __SPRT_ID(wscanf)(const __SPRT_WCHAR_T *__SPRT_RESTRICT fmt, ...) {
	__sprt_va_list list;
	__sprt_va_start(list, fmt);

	auto ret = ::vwscanf(fmt, list);

	__sprt_va_end(list);
	return ret;
}

__SPRT_C_FUNC int __SPRT_ID(fwscanf)(__SPRT_ID(FILE) * __SPRT_RESTRICT f,
		const __SPRT_WCHAR_T *__SPRT_RESTRICT fmt, ...) {
	__sprt_va_list list;
	__sprt_va_start(list, fmt);

	auto ret = ::vfwscanf(f, fmt, list);

	__sprt_va_end(list);
	return ret;
}

__SPRT_C_FUNC int __SPRT_ID(swscanf)(const __SPRT_WCHAR_T *__SPRT_RESTRICT buf,
		const __SPRT_WCHAR_T *__SPRT_RESTRICT fmt, ...) {
	__sprt_va_list list;
	__sprt_va_start(list, fmt);

	auto ret = ::vswscanf(buf, fmt, list);

	__sprt_va_end(list);
	return ret;
}

__SPRT_C_FUNC int __SPRT_ID(
		vwscanf)(const __SPRT_WCHAR_T *__SPRT_RESTRICT fmt, __sprt_va_list list) {
	return ::vwscanf(fmt, list);
}

__SPRT_C_FUNC int __SPRT_ID(vfwscanf)(__SPRT_ID(FILE) * __SPRT_RESTRICT f,
		const __SPRT_WCHAR_T *__SPRT_RESTRICT fmt, __sprt_va_list list) {
	return ::vfwscanf(f, fmt, list);
}

__SPRT_C_FUNC int __SPRT_ID(vswscanf)(const __SPRT_WCHAR_T *__SPRT_RESTRICT buf,
		const __SPRT_WCHAR_T *__SPRT_RESTRICT fmt, __sprt_va_list list) {
	return ::vswscanf(buf, fmt, list);
}

__SPRT_C_FUNC __SPRT_ID(wint_t) __SPRT_ID(fgetwc)(__SPRT_ID(FILE) * f) { return ::fgetwc(f); }
__SPRT_C_FUNC __SPRT_ID(wint_t) __SPRT_ID(getwc)(__SPRT_ID(FILE) * f) { return ::getwc(f); }
__SPRT_C_FUNC __SPRT_ID(wint_t) __SPRT_ID(getwchar)(void) { return ::getwchar(); }

__SPRT_C_FUNC __SPRT_ID(wint_t) __SPRT_ID(fputwc)(__SPRT_WCHAR_T c, __SPRT_ID(FILE) * f) {
	return ::fputwc(c, f);
}
__SPRT_C_FUNC __SPRT_ID(wint_t) __SPRT_ID(putwc)(__SPRT_WCHAR_T c, __SPRT_ID(FILE) * f) {
	return ::putwc(c, f);
}
__SPRT_C_FUNC __SPRT_ID(wint_t) __SPRT_ID(putwchar)(__SPRT_WCHAR_T c) { return ::putwchar(c); }

__SPRT_C_FUNC __SPRT_WCHAR_T *__SPRT_ID(
		fgetws)(__SPRT_WCHAR_T *__SPRT_RESTRICT a, int c, __SPRT_ID(FILE) * __SPRT_RESTRICT f) {
	return ::fgetws(a, c, f);
}
__SPRT_C_FUNC int __SPRT_ID(
		fputws)(const __SPRT_WCHAR_T *__SPRT_RESTRICT a, __SPRT_ID(FILE) * __SPRT_RESTRICT f) {
	return ::fputws(a, f);
}

__SPRT_C_FUNC __SPRT_ID(wint_t) __SPRT_ID(ungetwc)(__SPRT_ID(wint_t) c, __SPRT_ID(FILE) * f) {
	return ::ungetwc(c, f);
}

__SPRT_C_FUNC __SPRT_ID(size_t) __SPRT_ID(wcsftime)(__SPRT_WCHAR_T *__SPRT_RESTRICT a,
		__SPRT_ID(size_t) s, const __SPRT_WCHAR_T *__SPRT_RESTRICT b,
		const struct __SPRT_TM_NAME *__SPRT_RESTRICT _tm) {
	auto native = internal::getNativeTm(_tm);

	return ::wcsftime(a, s, b, &native);
}

__SPRT_C_FUNC __SPRT_ID(wint_t) __SPRT_ID(fgetwc_unlocked)(__SPRT_ID(FILE) * f) {
	return ::fgetwc_unlocked(f);
}

__SPRT_C_FUNC __SPRT_ID(wint_t) __SPRT_ID(getwc_unlocked)(__SPRT_ID(FILE) * f) {
	return ::getwc_unlocked(f);
}

__SPRT_C_FUNC __SPRT_ID(wint_t) __SPRT_ID(getwchar_unlocked)(void) { return ::getwchar_unlocked(); }

__SPRT_C_FUNC __SPRT_ID(wint_t) __SPRT_ID(fputwc_unlocked)(__SPRT_WCHAR_T c, __SPRT_ID(FILE) * f) {
	return ::fputwc_unlocked(c, f);
}

__SPRT_C_FUNC __SPRT_ID(wint_t) __SPRT_ID(putwc_unlocked)(__SPRT_WCHAR_T c, __SPRT_ID(FILE) * f) {
	return ::putwc_unlocked(c, f);
}

__SPRT_C_FUNC __SPRT_ID(wint_t) __SPRT_ID(putwchar_unlocked)(__SPRT_WCHAR_T c) {
	return putwchar_unlocked(c);
}

__SPRT_C_FUNC __SPRT_WCHAR_T *__SPRT_ID(fgetws_unlocked)(__SPRT_WCHAR_T *__SPRT_RESTRICT ptr, int c,
		__SPRT_ID(FILE) * __SPRT_RESTRICT f) {
	return ::fgetws_unlocked(ptr, c, f);
}

__SPRT_C_FUNC int __SPRT_ID(fputws_unlocked)(const __SPRT_WCHAR_T *__SPRT_RESTRICT ptr,
		__SPRT_ID(FILE) * __SPRT_RESTRICT f) {
	return ::fputws_unlocked(ptr, f);
}

__SPRT_C_FUNC __SPRT_ID(size_t) __SPRT_ID(wcsftime_l)(__SPRT_WCHAR_T *__SPRT_RESTRICT ptr,
		__SPRT_ID(size_t) size, const __SPRT_WCHAR_T *__SPRT_RESTRICT fmt,
		const struct __SPRT_TM_NAME *__SPRT_RESTRICT _tm, __SPRT_ID(locale_t) loc) {
	auto native = internal::getNativeTm(_tm);
	return ::wcsftime_l(ptr, size, fmt, &native, loc);
}

__SPRT_C_FUNC __SPRT_ID(FILE)
		* __SPRT_ID(open_wmemstream)(__SPRT_WCHAR_T **ptr, __SPRT_ID(size_t) * s) {
	return ::open_wmemstream(ptr, s);
}

__SPRT_C_FUNC __SPRT_ID(size_t) __SPRT_ID(mbsnrtowcs)(__SPRT_WCHAR_T *__SPRT_RESTRICT ptr,
		const char **__SPRT_RESTRICT ret, __SPRT_ID(size_t) a, __SPRT_ID(size_t) b,
		__SPRT_MBSTATE_NAME *__SPRT_RESTRICT state) {
	return ::mbsnrtowcs(ptr, ret, a, b, (mbstate_t *)state);
}

__SPRT_C_FUNC __SPRT_ID(size_t) __SPRT_ID(wcsnrtombs)(char *__SPRT_RESTRICT ptr,
		const __SPRT_WCHAR_T **__SPRT_RESTRICT ret, __SPRT_ID(size_t) a, __SPRT_ID(size_t) b,
		__SPRT_MBSTATE_NAME *__SPRT_RESTRICT state) {
	return ::wcsnrtombs(ptr, ret, a, b, (mbstate_t *)state);
}

__SPRT_C_FUNC __SPRT_WCHAR_T *__SPRT_ID(wcsdup)(const __SPRT_WCHAR_T *ptr) { return ::wcsdup(ptr); }

__SPRT_C_FUNC __SPRT_ID(size_t)
		__SPRT_ID(wcsnlen)(const __SPRT_WCHAR_T *ptr, __SPRT_ID(size_t) len) {
	return ::wcsnlen(ptr, len);
}

__SPRT_C_FUNC __SPRT_WCHAR_T *__SPRT_ID(
		wcpcpy)(__SPRT_WCHAR_T *__SPRT_RESTRICT ptr, const __SPRT_WCHAR_T *__SPRT_RESTRICT buf) {
	return ::wcpcpy(ptr, buf);
}

__SPRT_C_FUNC __SPRT_WCHAR_T *__SPRT_ID(wcpncpy)(__SPRT_WCHAR_T *__SPRT_RESTRICT a,
		const __SPRT_WCHAR_T *__SPRT_RESTRICT b, __SPRT_ID(size_t) size) {
	return ::wcpncpy(a, b, size);
}

__SPRT_C_FUNC int __SPRT_ID(wcscasecmp)(const __SPRT_WCHAR_T *a, const __SPRT_WCHAR_T *b) {
	return ::wcscasecmp(a, b);
}

__SPRT_C_FUNC int __SPRT_ID(
		wcscasecmp_l)(const __SPRT_WCHAR_T *a, const __SPRT_WCHAR_T *b, __SPRT_ID(locale_t) loc) {
	return ::wcscasecmp_l(a, b, loc);
}

__SPRT_C_FUNC int __SPRT_ID(
		wcsncasecmp)(const __SPRT_WCHAR_T *a, const __SPRT_WCHAR_T *b, __SPRT_ID(size_t) s) {
	return ::wcsncasecmp(a, b, s);
}

__SPRT_C_FUNC int __SPRT_ID(wcsncasecmp_l)(const __SPRT_WCHAR_T *a, const __SPRT_WCHAR_T *b,
		__SPRT_ID(size_t) s, __SPRT_ID(locale_t) loc) {
	return ::wcsncasecmp_l(a, b, s, loc);
}

__SPRT_C_FUNC int __SPRT_ID(
		wcscoll_l)(const __SPRT_WCHAR_T *a, const __SPRT_WCHAR_T *b, __SPRT_ID(locale_t) loc) {
	return ::wcscoll_l(a, b, loc);
}

__SPRT_C_FUNC __SPRT_ID(size_t) __SPRT_ID(wcsxfrm_l)(__SPRT_WCHAR_T *__SPRT_RESTRICT a,
		const __SPRT_WCHAR_T *__SPRT_RESTRICT b, __SPRT_ID(size_t) s, __SPRT_ID(locale_t) loc) {
	return ::wcsxfrm_l(a, b, s, loc);
}

__SPRT_C_FUNC int __SPRT_ID(wcwidth)(__SPRT_WCHAR_T c) { return ::wcwidth(c); }

__SPRT_C_FUNC int __SPRT_ID(wcswidth)(const __SPRT_WCHAR_T *ptr, __SPRT_ID(size_t) s) {
	return ::wcswidth(ptr, s);
}

} // namespace sprt
