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

#ifndef CORE_RUNTIME_INCLUDE_LIBC_WCHAR_H_
#define CORE_RUNTIME_INCLUDE_LIBC_WCHAR_H_

#ifdef __SPRT_BUILD

#include_next <wchar.h>

#else

#include <c/__sprt_wchar.h>
#include <c/__sprt_stdarg.h>

#ifndef __cplusplus
typedef __SPRT_ID(wchar_t) wchar_t;
#endif

__SPRT_BEGIN_DECL

#define WEOF __SPRT_WEOF

#if !__SPRT_MBSTATE_DIRECT
typedef __SPRT_MBSTATE_NAME mbstate_t;
#endif
typedef __SPRT_ID(size_t) size_t;
typedef __SPRT_ID(wint_t) wint_t;
typedef __SPRT_ID(FILE) FILE;
typedef __SPRT_ID(locale_t) locale_t;

SPRT_FORCEINLINE inline wchar_t *wcscpy(wchar_t *__SPRT_RESTRICT a,
		const wchar_t *__SPRT_RESTRICT b) {
	return __sprt_wcscpy(a, b);
}
SPRT_FORCEINLINE inline wchar_t *wcsncpy(wchar_t *__SPRT_RESTRICT a,
		const wchar_t *__SPRT_RESTRICT b, size_t s) {
	return __sprt_wcsncpy(a, b, s);
}

SPRT_FORCEINLINE inline wchar_t *wcscat(wchar_t *__SPRT_RESTRICT a,
		const wchar_t *__SPRT_RESTRICT b) {
	return __sprt_wcscat(a, b);
}
SPRT_FORCEINLINE inline wchar_t *wcsncat(wchar_t *__SPRT_RESTRICT a,
		const wchar_t *__SPRT_RESTRICT b, size_t s) {
	return __sprt_wcsncat(a, b, s);
}

SPRT_FORCEINLINE inline int wcscmp(const wchar_t *a, const wchar_t *b) {
	return __sprt_wcscmp(a, b);
}
SPRT_FORCEINLINE inline int wcsncmp(const wchar_t *a, const wchar_t *b, size_t s) {
	return __sprt_wcsncmp(a, b, s);
}

SPRT_FORCEINLINE inline int wcscoll(const wchar_t *a, const wchar_t *b) {
	return __sprt_wcscoll(a, b);
}
SPRT_FORCEINLINE inline size_t wcsxfrm(wchar_t *__SPRT_RESTRICT a, const wchar_t *__SPRT_RESTRICT b,
		size_t s) {
	return __sprt_wcsxfrm(a, b, s);
}

SPRT_FORCEINLINE inline const wchar_t *wcschr(const wchar_t *a, wchar_t c) {
	return __sprt_wcschr(a, c);
}
SPRT_FORCEINLINE inline const wchar_t *wcsrchr(const wchar_t *a, wchar_t c) {
	return __sprt_wcsrchr(a, c);
}

SPRT_FORCEINLINE inline size_t wcscspn(const wchar_t *a, const wchar_t *b) {
	return __sprt_wcscspn(a, b);
}
SPRT_FORCEINLINE inline size_t wcsspn(const wchar_t *a, const wchar_t *b) {
	return __sprt_wcsspn(a, b);
}
SPRT_FORCEINLINE inline const wchar_t *wcspbrk(const wchar_t *a, const wchar_t *b) {
	return __sprt_wcspbrk(a, b);
}

SPRT_FORCEINLINE inline wchar_t *wcstok(wchar_t *__SPRT_RESTRICT a,
		const wchar_t *__SPRT_RESTRICT b, wchar_t **__SPRT_RESTRICT ret) {
	return __sprt_wcstok(a, b, ret);
}

SPRT_FORCEINLINE inline size_t wcslen(const wchar_t *ret) { return __sprt_wcslen(ret); }

SPRT_FORCEINLINE inline const wchar_t *wcsstr(const wchar_t *__SPRT_RESTRICT a,
		const wchar_t *__SPRT_RESTRICT b) {
	return __sprt_wcsstr(a, b);
}
SPRT_FORCEINLINE inline const wchar_t *wcswcs(const wchar_t *a, const wchar_t *b) {
	return __sprt_wcswcs(a, b);
}

SPRT_FORCEINLINE inline const wchar_t *wmemchr(const wchar_t *a, wchar_t c, size_t s) {
	return __sprt_wmemchr(a, c, s);
}
SPRT_FORCEINLINE inline int wmemcmp(const wchar_t *a, const wchar_t *b, size_t s) {
	return __sprt_wmemcmp(a, b, s);
}
SPRT_FORCEINLINE inline wchar_t *wmemcpy(wchar_t *__SPRT_RESTRICT a,
		const wchar_t *__SPRT_RESTRICT b, size_t s) {
	return __sprt_wmemcpy(a, b, s);
}
SPRT_FORCEINLINE inline wchar_t *wmemmove(wchar_t *a, const wchar_t *b, size_t s) {
	return __sprt_wmemmove(a, b, s);
}
SPRT_FORCEINLINE inline wchar_t *wmemset(wchar_t *a, wchar_t c, size_t s) {
	return __sprt_wmemset(a, c, s);
}

SPRT_FORCEINLINE inline wint_t btowc(int value) { return __sprt_btowc(value); }
SPRT_FORCEINLINE inline int wctob(wint_t value) { return __sprt_wctob(value); }

SPRT_FORCEINLINE inline int mbsinit(const mbstate_t *value) { return __sprt_mbsinit(value); }
SPRT_FORCEINLINE inline size_t mbrtowc(wchar_t *__SPRT_RESTRICT a, const char *__SPRT_RESTRICT b,
		size_t s, mbstate_t *__SPRT_RESTRICT state) {
	return __sprt_mbrtowc(a, b, s, state);
}
SPRT_FORCEINLINE inline size_t wcrtomb(char *__SPRT_RESTRICT a, wchar_t c,
		mbstate_t *__SPRT_RESTRICT state) {
	return __sprt_wcrtomb(a, c, state);
}

SPRT_FORCEINLINE inline size_t mbrlen(const char *__SPRT_RESTRICT a, size_t s,
		mbstate_t *__SPRT_RESTRICT state) {
	return __sprt_mbrlen(a, s, state);
}

SPRT_FORCEINLINE inline size_t mbsrtowcs(wchar_t *__SPRT_RESTRICT a,
		const char **__SPRT_RESTRICT ret, size_t s, mbstate_t *__SPRT_RESTRICT state) {
	return __sprt_mbsrtowcs(a, ret, s, state);
}

SPRT_FORCEINLINE inline size_t wcsrtombs(char *__SPRT_RESTRICT a,
		const wchar_t **__SPRT_RESTRICT ret, size_t s, mbstate_t *__SPRT_RESTRICT state) {
	return __sprt_wcsrtombs(a, ret, s, state);
}

SPRT_FORCEINLINE inline float wcstof(const wchar_t *__SPRT_RESTRICT a,
		wchar_t **__SPRT_RESTRICT ret) {
	return __sprt_wcstof(a, ret);
}
SPRT_FORCEINLINE inline double wcstod(const wchar_t *__SPRT_RESTRICT a,
		wchar_t **__SPRT_RESTRICT ret) {
	return __sprt_wcstod(a, ret);
}
SPRT_FORCEINLINE inline long double wcstold(const wchar_t *__SPRT_RESTRICT a,
		wchar_t **__SPRT_RESTRICT ret) {
	return __sprt_wcstold(a, ret);
}

SPRT_FORCEINLINE inline long wcstol(const wchar_t *__SPRT_RESTRICT a, wchar_t **__SPRT_RESTRICT ret,
		int value) {
	return __sprt_wcstol(a, ret, value);
}
SPRT_FORCEINLINE inline unsigned long wcstoul(const wchar_t *__SPRT_RESTRICT a,
		wchar_t **__SPRT_RESTRICT ret, int value) {
	return __sprt_wcstoul(a, ret, value);
}

SPRT_FORCEINLINE inline long long wcstoll(const wchar_t *__SPRT_RESTRICT a,
		wchar_t **__SPRT_RESTRICT ret, int value) {
	return __sprt_wcstoll(a, ret, value);
}
SPRT_FORCEINLINE inline unsigned long long wcstoull(const wchar_t *__SPRT_RESTRICT a,
		wchar_t **__SPRT_RESTRICT ret, int value) {
	return __sprt_wcstoull(a, ret, value);
}

SPRT_FORCEINLINE inline int fwide(FILE *f, int value) { return __sprt_fwide(f, value); }

SPRT_FORCEINLINE inline int wprintf(const wchar_t *__SPRT_RESTRICT fmt, ...) {
	__sprt_va_list list;
	__sprt_va_start(list, fmt);

	auto ret = ::__sprt_vwprintf(fmt, list);

	__sprt_va_end(list);
	return ret;
}

SPRT_FORCEINLINE inline int fwprintf(__SPRT_ID(FILE) * __SPRT_RESTRICT f,
		const wchar_t *__SPRT_RESTRICT fmt, ...) {
	__sprt_va_list list;
	__sprt_va_start(list, fmt);

	auto ret = ::__sprt_vfwprintf(f, fmt, list);

	__sprt_va_end(list);
	return ret;
}

SPRT_FORCEINLINE inline int swprintf(wchar_t *__SPRT_RESTRICT buf, __SPRT_ID(size_t) size,
		const wchar_t *__SPRT_RESTRICT fmt, ...) {
	__sprt_va_list list;
	__sprt_va_start(list, fmt);

	auto ret = ::__sprt_vswprintf(buf, size, fmt, list);

	__sprt_va_end(list);
	return ret;
}

SPRT_FORCEINLINE inline int vwprintf(const wchar_t *__SPRT_RESTRICT fmt, __sprt_va_list list) {
	return ::__sprt_vwprintf(fmt, list);
}

SPRT_FORCEINLINE inline int vfwprintf(__SPRT_ID(FILE) * __SPRT_RESTRICT f,
		const wchar_t *__SPRT_RESTRICT fmt, __sprt_va_list list) {
	return ::__sprt_vfwprintf(f, fmt, list);
}

SPRT_FORCEINLINE inline int vswprintf(wchar_t *__SPRT_RESTRICT buf, __SPRT_ID(size_t) size,
		const wchar_t *__SPRT_RESTRICT fmt, __sprt_va_list list) {
	return ::__sprt_vswprintf(buf, size, fmt, list);
}

SPRT_FORCEINLINE inline int wscanf(const wchar_t *__SPRT_RESTRICT fmt, ...) {
	__sprt_va_list list;
	__sprt_va_start(list, fmt);

	auto ret = ::__sprt_vwscanf(fmt, list);

	__sprt_va_end(list);
	return ret;
}

SPRT_FORCEINLINE inline int fwscanf(__SPRT_ID(FILE) * __SPRT_RESTRICT f,
		const wchar_t *__SPRT_RESTRICT fmt, ...) {
	__sprt_va_list list;
	__sprt_va_start(list, fmt);

	auto ret = ::__sprt_vfwscanf(f, fmt, list);

	__sprt_va_end(list);
	return ret;
}

SPRT_FORCEINLINE inline int swscanf(const wchar_t *__SPRT_RESTRICT buf,
		const wchar_t *__SPRT_RESTRICT fmt, ...) {
	__sprt_va_list list;
	__sprt_va_start(list, fmt);

	auto ret = ::__sprt_vswscanf(buf, fmt, list);

	__sprt_va_end(list);
	return ret;
}

SPRT_FORCEINLINE inline int vwscanf(const wchar_t *__SPRT_RESTRICT fmt, __sprt_va_list list) {
	return ::__sprt_vwscanf(fmt, list);
}

SPRT_FORCEINLINE inline int vfwscanf(__SPRT_ID(FILE) * __SPRT_RESTRICT f,
		const wchar_t *__SPRT_RESTRICT fmt, __sprt_va_list list) {
	return ::__sprt_vfwscanf(f, fmt, list);
}

SPRT_FORCEINLINE inline int vswscanf(const wchar_t *__SPRT_RESTRICT buf,
		const wchar_t *__SPRT_RESTRICT fmt, __sprt_va_list list) {
	return ::__sprt_vswscanf(buf, fmt, list);
}


SPRT_FORCEINLINE inline wint_t fgetwc(FILE *value) { return __sprt_fgetwc(value); }
SPRT_FORCEINLINE inline wint_t getwc(FILE *value) { return __sprt_getwc(value); }
SPRT_FORCEINLINE inline wint_t getwchar() { return __sprt_getwchar(); }

SPRT_FORCEINLINE inline wint_t fputwc(wchar_t c, FILE *value) { return __sprt_fputwc(c, value); }
SPRT_FORCEINLINE inline wint_t putwc(wchar_t c, FILE *value) { return __sprt_putwc(c, value); }
SPRT_FORCEINLINE inline wint_t putwchar(wchar_t value) { return __sprt_putwchar(value); }

SPRT_FORCEINLINE inline wchar_t *fgetws(wchar_t *__SPRT_RESTRICT ptr, int c,
		FILE *__SPRT_RESTRICT value) {
	return __sprt_fgetws(ptr, c, value);
}
SPRT_FORCEINLINE inline int fputws(const wchar_t *__SPRT_RESTRICT ptr,
		FILE *__SPRT_RESTRICT value) {
	return __sprt_fputws(ptr, value);
}

SPRT_FORCEINLINE inline wint_t ungetwc(wint_t c, FILE *value) { return __sprt_ungetwc(c, value); }

SPRT_FORCEINLINE inline size_t wcsftime(wchar_t *__SPRT_RESTRICT ptr, size_t s,
		const wchar_t *__SPRT_RESTRICT fmt, const struct tm *__SPRT_RESTRICT value) {
	return __sprt_wcsftime(ptr, s, fmt, value);
}

SPRT_FORCEINLINE inline wint_t fgetwc_unlocked(FILE *value) {
	return __sprt_fgetwc_unlocked(value);
}

SPRT_FORCEINLINE inline wint_t getwc_unlocked(FILE *value) { return __sprt_getwc_unlocked(value); }

SPRT_FORCEINLINE inline wint_t getwchar_unlocked(void) { return __sprt_getwchar_unlocked(); }
SPRT_FORCEINLINE inline wint_t fputwc_unlocked(wchar_t c, FILE *value) {
	return __sprt_fputwc_unlocked(c, value);
}
SPRT_FORCEINLINE inline wint_t putwc_unlocked(wchar_t c, FILE *value) {
	return __sprt_putwc_unlocked(c, value);
}
SPRT_FORCEINLINE inline wint_t putwchar_unlocked(wchar_t value) {
	return __sprt_putwchar_unlocked(value);
}
SPRT_FORCEINLINE inline wchar_t *fgetws_unlocked(wchar_t *__SPRT_RESTRICT ptr, int c,
		FILE *__SPRT_RESTRICT value) {
	return __sprt_fgetws_unlocked(ptr, c, value);
}
SPRT_FORCEINLINE inline int fputws_unlocked(const wchar_t *__SPRT_RESTRICT ptr,
		FILE *__SPRT_RESTRICT value) {
	return __sprt_fputws_unlocked(ptr, value);
}

SPRT_FORCEINLINE inline size_t wcsftime_l(wchar_t *__SPRT_RESTRICT ptr, size_t s,
		const wchar_t *__SPRT_RESTRICT fmt, const struct tm *__SPRT_RESTRICT tm, locale_t value) {
	return __sprt_wcsftime_l(ptr, s, fmt, tm, value);
}

SPRT_FORCEINLINE inline FILE *open_wmemstream(wchar_t **ptr, size_t *value) {
	return __sprt_open_wmemstream(ptr, value);
}
SPRT_FORCEINLINE inline size_t mbsnrtowcs(wchar_t *__SPRT_RESTRICT ptr,
		const char **__SPRT_RESTRICT ret, size_t a, size_t b, mbstate_t *__SPRT_RESTRICT value) {
	return __sprt_mbsnrtowcs(ptr, ret, a, b, value);
}
SPRT_FORCEINLINE inline size_t wcsnrtombs(char *__SPRT_RESTRICT ptr,
		const wchar_t **__SPRT_RESTRICT ret, size_t a, size_t b, mbstate_t *__SPRT_RESTRICT value) {
	return __sprt_wcsnrtombs(ptr, ret, a, b, value);
}
SPRT_FORCEINLINE inline wchar_t *wcsdup(const wchar_t *value) { return __sprt_wcsdup(value); }

SPRT_FORCEINLINE inline size_t wcsnlen(const wchar_t *ptr, size_t value) {
	return __sprt_wcsnlen(ptr, value);
}

SPRT_FORCEINLINE inline wchar_t *wcpcpy(wchar_t *__SPRT_RESTRICT ptr,
		const wchar_t *__SPRT_RESTRICT value) {
	return __sprt_wcpcpy(ptr, value);
}
SPRT_FORCEINLINE inline wchar_t *wcpncpy(wchar_t *__SPRT_RESTRICT a,
		const wchar_t *__SPRT_RESTRICT b, size_t value) {
	return __sprt_wcpncpy(a, b, value);
}
SPRT_FORCEINLINE inline int wcscasecmp(const wchar_t *a, const wchar_t *b) {
	return __sprt_wcscasecmp(a, b);
}
SPRT_FORCEINLINE inline int wcscasecmp_l(const wchar_t *a, const wchar_t *b, locale_t value) {
	return __sprt_wcscasecmp_l(a, b, value);
}
SPRT_FORCEINLINE inline int wcsncasecmp(const wchar_t *a, const wchar_t *b, size_t value) {
	return __sprt_wcsncasecmp(a, b, value);
}
SPRT_FORCEINLINE inline int wcsncasecmp_l(const wchar_t *a, const wchar_t *b, size_t s,
		locale_t value) {
	return __sprt_wcsncasecmp_l(a, b, s, value);
}
SPRT_FORCEINLINE inline int wcscoll_l(const wchar_t *a, const wchar_t *b, locale_t value) {
	return __sprt_wcscoll_l(a, b, value);
}
SPRT_FORCEINLINE inline size_t wcsxfrm_l(wchar_t *__SPRT_RESTRICT a,
		const wchar_t *__SPRT_RESTRICT b, size_t s, locale_t value) {
	return __sprt_wcsxfrm_l(a, b, s, value);
}

SPRT_FORCEINLINE inline int wcwidth(wchar_t value) { return __sprt_wcwidth(value); }

SPRT_FORCEINLINE inline int wcswidth(const wchar_t *ptr, size_t value) {
	return __sprt_wcswidth(ptr, value);
}

__SPRT_END_DECL

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_WCHAR_H_
