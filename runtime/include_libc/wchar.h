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

#if defined(__SPRT_BUILD) && __STDC_HOSTED__ == 1

#include_next <wchar.h>

#else

#include <sprt/c/__sprt_wchar.h>
#include <sprt/c/__sprt_wctype.h>
#include <sprt/c/__sprt_stdarg.h>

#include <sprt/wrappers/windows/__sprt_config.h>

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
typedef __SPRT_ID(wctype_t) wctype_t;
typedef __SPRT_ID(FILE) FILE;
typedef __SPRT_ID(locale_t) locale_t;


#if !__SPRT_WIN_USE_IMPORT_STRING_LIB
SPRT_UMBRELLA_FUNC
wchar_t *wcscpy(wchar_t *__SPRT_RESTRICT a, const wchar_t *__SPRT_RESTRICT b) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_wcscpy(a, b);
}
#endif

SPRT_UMBRELLA_FUNC
wchar_t *wcsncpy(wchar_t *__SPRT_RESTRICT a, const wchar_t *__SPRT_RESTRICT b,
		size_t s) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_wcsncpy(a, b, s);
}
#endif

SPRT_UMBRELLA_FUNC
size_t wcslen(const wchar_t *ret) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_wcslen(ret);
}
#endif

SPRT_UMBRELLA_FUNC
const wchar_t *wcsstr(const wchar_t *__SPRT_RESTRICT a,
		const wchar_t *__SPRT_RESTRICT b) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_wcsstr(a, b);
}
#endif

SPRT_UMBRELLA_FUNC
size_t wcsnlen(const wchar_t *ptr, size_t value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_wcsnlen(ptr, value);
}
#endif
#endif

SPRT_UMBRELLA_FUNC
wchar_t *wcscat(wchar_t *__SPRT_RESTRICT a, const wchar_t *__SPRT_RESTRICT b) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_wcscat(a, b);
}
#endif

SPRT_UMBRELLA_FUNC
wchar_t *wcsncat(wchar_t *__SPRT_RESTRICT a, const wchar_t *__SPRT_RESTRICT b,
		size_t s) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_wcsncat(a, b, s);
}
#endif

SPRT_UMBRELLA_FUNC
int wcscmp(const wchar_t *a, const wchar_t *b) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_wcscmp(a, b);
}
#endif

SPRT_UMBRELLA_FUNC
int wcsncmp(const wchar_t *a, const wchar_t *b, size_t s) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_wcsncmp(a, b, s);
}
#endif

SPRT_UMBRELLA_FUNC
int wcscoll(const wchar_t *a, const wchar_t *b) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_wcscoll(a, b);
}
#endif

SPRT_UMBRELLA_FUNC
size_t wcsxfrm(wchar_t *__SPRT_RESTRICT a, const wchar_t *__SPRT_RESTRICT b,
		size_t s) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_wcsxfrm(a, b, s);
}
#endif

SPRT_UMBRELLA_FUNC
const wchar_t *wcschr(const wchar_t *a, wchar_t c) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_wcschr(a, c);
}
#endif

SPRT_UMBRELLA_FUNC
const wchar_t *wcsrchr(const wchar_t *a, wchar_t c) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_wcsrchr(a, c);
}
#endif

SPRT_UMBRELLA_FUNC
size_t wcscspn(const wchar_t *a, const wchar_t *b) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_wcscspn(a, b);
}
#endif

SPRT_UMBRELLA_FUNC
size_t wcsspn(const wchar_t *a, const wchar_t *b) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_wcsspn(a, b);
}
#endif

SPRT_UMBRELLA_FUNC
const wchar_t *wcspbrk(const wchar_t *a, const wchar_t *b) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_wcspbrk(a, b);
}
#endif

SPRT_UMBRELLA_FUNC
wchar_t *wcstok(wchar_t *__SPRT_RESTRICT a, const wchar_t *__SPRT_RESTRICT b,
		wchar_t **__SPRT_RESTRICT ret) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_wcstok(a, b, ret);
}
#endif

SPRT_UMBRELLA_FUNC
const wchar_t *wcswcs(const wchar_t *a, const wchar_t *b) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_wcswcs(a, b);
}
#endif

SPRT_UMBRELLA_FUNC
const wchar_t *wmemchr(const wchar_t *a, wchar_t c, size_t s) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_wmemchr(a, c, s);
}
#endif

SPRT_UMBRELLA_FUNC
int wmemcmp(const wchar_t *a, const wchar_t *b, size_t s) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_wmemcmp(a, b, s);
}
#endif

SPRT_UMBRELLA_FUNC
wchar_t *wmemcpy(wchar_t *__SPRT_RESTRICT a, const wchar_t *__SPRT_RESTRICT b,
		size_t s) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_wmemcpy(a, b, s);
}
#endif

SPRT_UMBRELLA_FUNC
wchar_t *wmemmove(wchar_t *a, const wchar_t *b, size_t s) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_wmemmove(a, b, s);
}
#endif

SPRT_UMBRELLA_FUNC
wchar_t *wmemset(wchar_t *a, wchar_t c, size_t s) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_wmemset(a, c, s);
}
#endif

SPRT_UMBRELLA_FUNC
wint_t btowc(int value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_btowc(value);
}
#endif

SPRT_UMBRELLA_FUNC
int wctob(wint_t value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_wctob(value);
}
#endif

SPRT_UMBRELLA_FUNC
int mbsinit(const mbstate_t *value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_mbsinit(value);
}
#endif

SPRT_UMBRELLA_FUNC
size_t mbrtowc(wchar_t *__SPRT_RESTRICT a, const char *__SPRT_RESTRICT b, size_t s,
		mbstate_t *__SPRT_RESTRICT state) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_mbrtowc(a, b, s, state);
}
#endif

SPRT_UMBRELLA_FUNC
size_t wcrtomb(char *__SPRT_RESTRICT a, wchar_t c,
		mbstate_t *__SPRT_RESTRICT state) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_wcrtomb(a, c, state);
}
#endif

SPRT_UMBRELLA_FUNC
size_t mbrlen(const char *__SPRT_RESTRICT a, size_t s,
		mbstate_t *__SPRT_RESTRICT state) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_mbrlen(a, s, state);
}
#endif

SPRT_UMBRELLA_FUNC
size_t mbsrtowcs(wchar_t *__SPRT_RESTRICT a, const char **__SPRT_RESTRICT ret, size_t s,
		mbstate_t *__SPRT_RESTRICT state) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_mbsrtowcs(a, ret, s, state);
}
#endif

SPRT_UMBRELLA_FUNC
size_t wcsrtombs(char *__SPRT_RESTRICT a, const wchar_t **__SPRT_RESTRICT ret, size_t s,
		mbstate_t *__SPRT_RESTRICT state) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_wcsrtombs(a, ret, s, state);
}
#endif

SPRT_UMBRELLA_FUNC
float wcstof(const wchar_t *__SPRT_RESTRICT a, wchar_t **__SPRT_RESTRICT ret) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_wcstof(a, ret);
}
#endif

SPRT_UMBRELLA_FUNC
double wcstod(const wchar_t *__SPRT_RESTRICT a, wchar_t **__SPRT_RESTRICT ret) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_wcstod(a, ret);
}
#endif

SPRT_UMBRELLA_FUNC
long double wcstold(const wchar_t *__SPRT_RESTRICT a,
		wchar_t **__SPRT_RESTRICT ret) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_wcstold(a, ret);
}
#endif

SPRT_UMBRELLA_FUNC
long wcstol(const wchar_t *__SPRT_RESTRICT a, wchar_t **__SPRT_RESTRICT ret,
		int value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_wcstol(a, ret, value);
}
#endif

SPRT_UMBRELLA_FUNC
unsigned long wcstoul(const wchar_t *__SPRT_RESTRICT a, wchar_t **__SPRT_RESTRICT ret,
		int value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_wcstoul(a, ret, value);
}
#endif

SPRT_UMBRELLA_FUNC
long long wcstoll(const wchar_t *__SPRT_RESTRICT a, wchar_t **__SPRT_RESTRICT ret,
		int value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_wcstoll(a, ret, value);
}
#endif

SPRT_UMBRELLA_FUNC
unsigned long long wcstoull(const wchar_t *__SPRT_RESTRICT a, wchar_t **__SPRT_RESTRICT ret,
		int value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_wcstoull(a, ret, value);
}
#endif

SPRT_UMBRELLA_FUNC
int fwide(FILE *f, int value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_fwide(f, value);
}
#endif

SPRT_UMBRELLA_FUNC
int wprintf(const wchar_t *__SPRT_RESTRICT fmt, ...) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	__sprt_va_list list;
	__sprt_va_start(list, fmt);

	auto ret = __sprt_vwprintf(fmt, list);

	__sprt_va_end(list);
	return ret;
}
#endif

SPRT_UMBRELLA_FUNC
int fwprintf(__SPRT_ID(FILE) * __SPRT_RESTRICT f, const wchar_t *__SPRT_RESTRICT fmt,
		...) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	__sprt_va_list list;
	__sprt_va_start(list, fmt);

	auto ret = __sprt_vfwprintf(f, fmt, list);

	__sprt_va_end(list);
	return ret;
}
#endif

SPRT_UMBRELLA_FUNC
int swprintf(wchar_t *__SPRT_RESTRICT buf, __SPRT_ID(size_t) size,
		const wchar_t *__SPRT_RESTRICT fmt, ...) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	__sprt_va_list list;
	__sprt_va_start(list, fmt);

	auto ret = __sprt_vswprintf(buf, size, fmt, list);

	__sprt_va_end(list);
	return ret;
}
#endif

SPRT_UMBRELLA_FUNC
int snwprintf(wchar_t *__SPRT_RESTRICT buf, __SPRT_ID(size_t) size,
		const wchar_t *__SPRT_RESTRICT fmt, ...) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	__sprt_va_list list;
	__sprt_va_start(list, fmt);

	auto ret = __sprt_vswprintf(buf, size, fmt, list);

	__sprt_va_end(list);
	return ret;
}
#endif

SPRT_UMBRELLA_FUNC
int _snwprintf(wchar_t *__SPRT_RESTRICT buf, __SPRT_ID(size_t) size,
		const wchar_t *__SPRT_RESTRICT fmt, ...) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	__sprt_va_list list;
	__sprt_va_start(list, fmt);

	auto ret = __sprt_vswprintf(buf, size, fmt, list);

	__sprt_va_end(list);
	return ret;
}
#endif

SPRT_UMBRELLA_FUNC
int vwprintf(const wchar_t *__SPRT_RESTRICT fmt, __sprt_va_list list) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_vwprintf(fmt, list);
}
#endif

SPRT_UMBRELLA_FUNC
int vfwprintf(__SPRT_ID(FILE) * __SPRT_RESTRICT f, const wchar_t *__SPRT_RESTRICT fmt,
		__sprt_va_list list) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_vfwprintf(f, fmt, list);
}
#endif

SPRT_UMBRELLA_FUNC
int vswprintf(wchar_t *__SPRT_RESTRICT buf, __SPRT_ID(size_t) size,
		const wchar_t *__SPRT_RESTRICT fmt, __sprt_va_list list) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_vswprintf(buf, size, fmt, list);
}
#endif

SPRT_UMBRELLA_FUNC
int wscanf(const wchar_t *__SPRT_RESTRICT fmt, ...) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	__sprt_va_list list;
	__sprt_va_start(list, fmt);

	auto ret = __sprt_vwscanf(fmt, list);

	__sprt_va_end(list);
	return ret;
}
#endif

SPRT_UMBRELLA_FUNC
int fwscanf(__SPRT_ID(FILE) * __SPRT_RESTRICT f, const wchar_t *__SPRT_RESTRICT fmt,
		...) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	__sprt_va_list list;
	__sprt_va_start(list, fmt);

	auto ret = __sprt_vfwscanf(f, fmt, list);

	__sprt_va_end(list);
	return ret;
}
#endif

SPRT_UMBRELLA_FUNC
int swscanf(const wchar_t *__SPRT_RESTRICT buf, const wchar_t *__SPRT_RESTRICT fmt,
		...) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	__sprt_va_list list;
	__sprt_va_start(list, fmt);

	auto ret = __sprt_vswscanf(buf, fmt, list);

	__sprt_va_end(list);
	return ret;
}
#endif

SPRT_UMBRELLA_FUNC
int vwscanf(const wchar_t *__SPRT_RESTRICT fmt, __sprt_va_list list) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_vwscanf(fmt, list);
}
#endif

SPRT_UMBRELLA_FUNC
int vfwscanf(__SPRT_ID(FILE) * __SPRT_RESTRICT f, const wchar_t *__SPRT_RESTRICT fmt,
		__sprt_va_list list) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_vfwscanf(f, fmt, list);
}
#endif

SPRT_UMBRELLA_FUNC
int vswscanf(const wchar_t *__SPRT_RESTRICT buf, const wchar_t *__SPRT_RESTRICT fmt,
		__sprt_va_list list) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_vswscanf(buf, fmt, list);
}
#endif


SPRT_UMBRELLA_FUNC
wint_t fgetwc(FILE *value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_fgetwc(value);
}
#endif

SPRT_UMBRELLA_FUNC
wint_t getwc(FILE *value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_getwc(value);
}
#endif

SPRT_UMBRELLA_FUNC
wint_t getwchar() SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_getwchar();
}
#endif

SPRT_UMBRELLA_FUNC
wint_t fputwc(wchar_t c, FILE *value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_fputwc(c, value);
}
#endif

SPRT_UMBRELLA_FUNC
wint_t putwc(wchar_t c, FILE *value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_putwc(c, value);
}
#endif

SPRT_UMBRELLA_FUNC
wint_t putwchar(wchar_t value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_putwchar(value);
}
#endif

SPRT_UMBRELLA_FUNC
wchar_t *fgetws(wchar_t *__SPRT_RESTRICT ptr, int c, FILE *__SPRT_RESTRICT value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_fgetws(ptr, c, value);
}
#endif

SPRT_UMBRELLA_FUNC
int fputws(const wchar_t *__SPRT_RESTRICT ptr, FILE *__SPRT_RESTRICT value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_fputws(ptr, value);
}
#endif

SPRT_UMBRELLA_FUNC
wint_t ungetwc(wint_t c, FILE *value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_ungetwc(c, value);
}
#endif

SPRT_UMBRELLA_FUNC
size_t wcsftime(wchar_t *__SPRT_RESTRICT ptr, size_t s, const wchar_t *__SPRT_RESTRICT fmt,
		const struct tm *__SPRT_RESTRICT value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_wcsftime(ptr, s, fmt, value);
}
#endif

SPRT_UMBRELLA_FUNC
wint_t fgetwc_unlocked(FILE *value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_fgetwc_unlocked(value);
}
#endif

SPRT_UMBRELLA_FUNC
wint_t getwc_unlocked(FILE *value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_getwc_unlocked(value);
}
#endif

SPRT_UMBRELLA_FUNC
wint_t getwchar_unlocked(void) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_getwchar_unlocked();
}
#endif

SPRT_UMBRELLA_FUNC
wint_t fputwc_unlocked(wchar_t c, FILE *value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_fputwc_unlocked(c, value);
}
#endif

SPRT_UMBRELLA_FUNC
wint_t putwc_unlocked(wchar_t c, FILE *value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_putwc_unlocked(c, value);
}
#endif

SPRT_UMBRELLA_FUNC
wint_t putwchar_unlocked(wchar_t value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_putwchar_unlocked(value);
}
#endif

SPRT_UMBRELLA_FUNC
wchar_t *fgetws_unlocked(wchar_t *__SPRT_RESTRICT ptr, int c,
		FILE *__SPRT_RESTRICT value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_fgetws_unlocked(ptr, c, value);
}
#endif

SPRT_UMBRELLA_FUNC
int fputws_unlocked(const wchar_t *__SPRT_RESTRICT ptr,
		FILE *__SPRT_RESTRICT value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_fputws_unlocked(ptr, value);
}
#endif

SPRT_UMBRELLA_FUNC
size_t wcsftime_l(wchar_t *__SPRT_RESTRICT ptr, size_t s, const wchar_t *__SPRT_RESTRICT fmt,
		const struct tm *__SPRT_RESTRICT tm, locale_t value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_wcsftime_l(ptr, s, fmt, tm, value);
}
#endif

#if __SPRT_CONFIG_HAVE_STDIO_OPEN_MEMSTREAM || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS
SPRT_UMBRELLA_FUNC
FILE *open_wmemstream(wchar_t **ptr, size_t *value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_open_wmemstream(ptr, value);
}
#endif
#endif

SPRT_UMBRELLA_FUNC
size_t mbsnrtowcs(wchar_t *__SPRT_RESTRICT ptr, const char **__SPRT_RESTRICT ret, size_t a,
		size_t b, mbstate_t *__SPRT_RESTRICT value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_mbsnrtowcs(ptr, ret, a, b, value);
}
#endif

SPRT_UMBRELLA_FUNC
size_t wcsnrtombs(char *__SPRT_RESTRICT dst, const wchar_t **__SPRT_RESTRICT src, size_t ssize,
		size_t dsize, mbstate_t *__SPRT_RESTRICT value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_wcsnrtombs(dst, src, ssize, dsize, value);
}
#endif

SPRT_UMBRELLA_FUNC
wchar_t *wcsdup(const wchar_t *value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_wcsdup(value);
}
#endif

SPRT_UMBRELLA_FUNC
wchar_t *_wcsdup(const wchar_t *value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_wcsdup(value);
}
#endif

SPRT_UMBRELLA_FUNC
wchar_t *wcpcpy(wchar_t *__SPRT_RESTRICT ptr,
		const wchar_t *__SPRT_RESTRICT value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_wcpcpy(ptr, value);
}
#endif

SPRT_UMBRELLA_FUNC
wchar_t *wcpncpy(wchar_t *__SPRT_RESTRICT a, const wchar_t *__SPRT_RESTRICT b,
		size_t value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_wcpncpy(a, b, value);
}
#endif

SPRT_UMBRELLA_FUNC
int wcscasecmp(const wchar_t *a, const wchar_t *b) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_wcscasecmp(a, b);
}
#endif

SPRT_UMBRELLA_FUNC
int wcscasecmp_l(const wchar_t *a, const wchar_t *b, locale_t value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_wcscasecmp_l(a, b, value);
}
#endif

SPRT_UMBRELLA_FUNC
int wcsncasecmp(const wchar_t *a, const wchar_t *b, size_t value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_wcsncasecmp(a, b, value);
}
#endif

SPRT_UMBRELLA_FUNC
int wcsncasecmp_l(const wchar_t *a, const wchar_t *b, size_t s, locale_t value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_wcsncasecmp_l(a, b, s, value);
}
#endif

SPRT_UMBRELLA_FUNC
int wcscoll_l(const wchar_t *a, const wchar_t *b, locale_t value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_wcscoll_l(a, b, value);
}
#endif

SPRT_UMBRELLA_FUNC
size_t wcsxfrm_l(wchar_t *__SPRT_RESTRICT a, const wchar_t *__SPRT_RESTRICT b, size_t s,
		locale_t value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_wcsxfrm_l(a, b, s, value);
}
#endif

SPRT_UMBRELLA_FUNC
int wcwidth(wchar_t value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_wcwidth(value);
}
#endif

SPRT_UMBRELLA_FUNC
int wcswidth(const wchar_t *ptr, size_t value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_wcswidth(ptr, value);
}
#endif

SPRT_UMBRELLA_FUNC
wint_t towlower(wint_t c) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_towlower(c);
}
#endif

SPRT_UMBRELLA_FUNC
wint_t towupper(wint_t c) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_towupper(c);
}
#endif

SPRT_API int wcscpy_s(wchar_t *dest, size_t dest_size, const wchar_t *src);

SPRT_API int wcsncpy_s(wchar_t *strDest, size_t numberOfElements, const wchar_t *strSource,
		size_t count);

SPRT_API int wcstombs_s(size_t *pReturnValue, char *mbstr, size_t sizeInBytes, const wchar_t *wcstr,
		size_t count);

__SPRT_END_DECL

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_WCHAR_H_
