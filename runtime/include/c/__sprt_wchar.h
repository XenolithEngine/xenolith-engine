/**
Copyright (c) 2025 Stappler Team <admin@stappler.org>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation __SPRT_ID(FILE)s (the "Software"), to deal
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

#ifndef CORE_RUNTIME_INCLUDE_C___SPRT_WCHAR_H_
#define CORE_RUNTIME_INCLUDE_C___SPRT_WCHAR_H_

#include <c/bits/__sprt_wchar_t.h>
#include <c/bits/__sprt_time_t.h>
#include <c/bits/__sprt_wint_t.h>
#include <c/bits/__sprt_size_t.h>
#include <c/bits/__sprt_va_list.h>
#include <c/cross/__sprt_mbstate.h>
#include <c/cross/__sprt_file_ptr.h>
#include <c/cross/__sprt_locale.h>

__SPRT_BEGIN_DECL

#ifdef __cplusplus
#define __SPRT_WCHAR_T wchar_t
#else
#define __SPRT_WCHAR_T __SPRT_WCHAR_T
#endif

SPRT_API __SPRT_WCHAR_T *__SPRT_ID(
		wcscpy)(__SPRT_WCHAR_T *__SPRT_RESTRICT, const __SPRT_WCHAR_T *__SPRT_RESTRICT);
SPRT_API __SPRT_WCHAR_T *__SPRT_ID(wcsncpy)(__SPRT_WCHAR_T *__SPRT_RESTRICT,
		const __SPRT_WCHAR_T *__SPRT_RESTRICT, __SPRT_ID(size_t));

SPRT_API __SPRT_WCHAR_T *__SPRT_ID(
		wcscat)(__SPRT_WCHAR_T *__SPRT_RESTRICT, const __SPRT_WCHAR_T *__SPRT_RESTRICT);
SPRT_API __SPRT_WCHAR_T *__SPRT_ID(wcsncat)(__SPRT_WCHAR_T *__SPRT_RESTRICT,
		const __SPRT_WCHAR_T *__SPRT_RESTRICT, __SPRT_ID(size_t));

SPRT_API int __SPRT_ID(wcscmp)(const __SPRT_WCHAR_T *, const __SPRT_WCHAR_T *);
SPRT_API int __SPRT_ID(wcsncmp)(const __SPRT_WCHAR_T *, const __SPRT_WCHAR_T *, __SPRT_ID(size_t));

SPRT_API int __SPRT_ID(wcscoll)(const __SPRT_WCHAR_T *, const __SPRT_WCHAR_T *);
SPRT_API __SPRT_ID(size_t) __SPRT_ID(wcsxfrm)(__SPRT_WCHAR_T *__SPRT_RESTRICT,
		const __SPRT_WCHAR_T *__SPRT_RESTRICT, __SPRT_ID(size_t));

SPRT_API const __SPRT_WCHAR_T *__SPRT_ID(wcschr)(const __SPRT_WCHAR_T *, __SPRT_WCHAR_T);
SPRT_API const __SPRT_WCHAR_T *__SPRT_ID(wcsrchr)(const __SPRT_WCHAR_T *, __SPRT_WCHAR_T);

SPRT_API __SPRT_ID(size_t) __SPRT_ID(wcscspn)(const __SPRT_WCHAR_T *, const __SPRT_WCHAR_T *);
SPRT_API __SPRT_ID(size_t) __SPRT_ID(wcsspn)(const __SPRT_WCHAR_T *, const __SPRT_WCHAR_T *);
SPRT_API const __SPRT_WCHAR_T *__SPRT_ID(wcspbrk)(const __SPRT_WCHAR_T *, const __SPRT_WCHAR_T *);

SPRT_API __SPRT_WCHAR_T *__SPRT_ID(wcstok)(__SPRT_WCHAR_T *__SPRT_RESTRICT,
		const __SPRT_WCHAR_T *__SPRT_RESTRICT, __SPRT_WCHAR_T **__SPRT_RESTRICT);

SPRT_API __SPRT_ID(size_t) __SPRT_ID(wcslen)(const __SPRT_WCHAR_T *);

SPRT_API const __SPRT_WCHAR_T *__SPRT_ID(
		wcsstr)(const __SPRT_WCHAR_T *__SPRT_RESTRICT, const __SPRT_WCHAR_T *__SPRT_RESTRICT);
SPRT_API const __SPRT_WCHAR_T *__SPRT_ID(wcswcs)(const __SPRT_WCHAR_T *, const __SPRT_WCHAR_T *);

SPRT_API const __SPRT_WCHAR_T *__SPRT_ID(
		wmemchr)(const __SPRT_WCHAR_T *, __SPRT_WCHAR_T, __SPRT_ID(size_t));
SPRT_API int __SPRT_ID(wmemcmp)(const __SPRT_WCHAR_T *, const __SPRT_WCHAR_T *, __SPRT_ID(size_t));
SPRT_API __SPRT_WCHAR_T *__SPRT_ID(wmemcpy)(__SPRT_WCHAR_T *__SPRT_RESTRICT,
		const __SPRT_WCHAR_T *__SPRT_RESTRICT, __SPRT_ID(size_t));
SPRT_API __SPRT_WCHAR_T *__SPRT_ID(
		wmemmove)(__SPRT_WCHAR_T *, const __SPRT_WCHAR_T *, __SPRT_ID(size_t));
SPRT_API __SPRT_WCHAR_T *__SPRT_ID(wmemset)(__SPRT_WCHAR_T *, __SPRT_WCHAR_T, __SPRT_ID(size_t));

SPRT_API __SPRT_ID(wint_t) __SPRT_ID(btowc)(int);
SPRT_API int __SPRT_ID(wctob)(__SPRT_ID(wint_t));

SPRT_API int __SPRT_ID(mbsinit)(const __SPRT_MBSTATE_NAME *);
SPRT_API __SPRT_ID(size_t) __SPRT_ID(mbrtowc)(__SPRT_WCHAR_T *__SPRT_RESTRICT,
		const char *__SPRT_RESTRICT, __SPRT_ID(size_t), __SPRT_MBSTATE_NAME *__SPRT_RESTRICT);
SPRT_API __SPRT_ID(size_t) __SPRT_ID(
		wcrtomb)(char *__SPRT_RESTRICT, __SPRT_WCHAR_T, __SPRT_MBSTATE_NAME *__SPRT_RESTRICT);

SPRT_API __SPRT_ID(size_t) __SPRT_ID(mbrlen)(const char *__SPRT_RESTRICT, __SPRT_ID(size_t),
		__SPRT_MBSTATE_NAME *__SPRT_RESTRICT);

SPRT_API __SPRT_ID(size_t) __SPRT_ID(mbsrtowcs)(__SPRT_WCHAR_T *__SPRT_RESTRICT,
		const char **__SPRT_RESTRICT, __SPRT_ID(size_t), __SPRT_MBSTATE_NAME *__SPRT_RESTRICT);
SPRT_API __SPRT_ID(size_t)
		__SPRT_ID(wcsrtombs)(char *__SPRT_RESTRICT, const __SPRT_WCHAR_T **__SPRT_RESTRICT,
				__SPRT_ID(size_t), __SPRT_MBSTATE_NAME *__SPRT_RESTRICT);

SPRT_API float __SPRT_ID(
		wcstof)(const __SPRT_WCHAR_T *__SPRT_RESTRICT, __SPRT_WCHAR_T **__SPRT_RESTRICT);
SPRT_API double __SPRT_ID(
		wcstod)(const __SPRT_WCHAR_T *__SPRT_RESTRICT, __SPRT_WCHAR_T **__SPRT_RESTRICT);
SPRT_API long double __SPRT_ID(
		wcstold)(const __SPRT_WCHAR_T *__SPRT_RESTRICT, __SPRT_WCHAR_T **__SPRT_RESTRICT);

SPRT_API long __SPRT_ID(
		wcstol)(const __SPRT_WCHAR_T *__SPRT_RESTRICT, __SPRT_WCHAR_T **__SPRT_RESTRICT, int);
SPRT_API unsigned long __SPRT_ID(
		wcstoul)(const __SPRT_WCHAR_T *__SPRT_RESTRICT, __SPRT_WCHAR_T **__SPRT_RESTRICT, int);

SPRT_API long long __SPRT_ID(
		wcstoll)(const __SPRT_WCHAR_T *__SPRT_RESTRICT, __SPRT_WCHAR_T **__SPRT_RESTRICT, int);
SPRT_API unsigned long long __SPRT_ID(
		wcstoull)(const __SPRT_WCHAR_T *__SPRT_RESTRICT, __SPRT_WCHAR_T **__SPRT_RESTRICT, int);

SPRT_API int __SPRT_ID(fwide)(__SPRT_ID(FILE) *, int);

SPRT_API int __SPRT_ID(wprintf)(const __SPRT_WCHAR_T *__SPRT_RESTRICT, ...);
SPRT_API int __SPRT_ID(
		fwprintf)(__SPRT_ID(FILE) * __SPRT_RESTRICT, const __SPRT_WCHAR_T *__SPRT_RESTRICT, ...);
SPRT_API int __SPRT_ID(swprintf)(__SPRT_WCHAR_T *__SPRT_RESTRICT, __SPRT_ID(size_t),
		const __SPRT_WCHAR_T *__SPRT_RESTRICT, ...);

SPRT_API int __SPRT_ID(vwprintf)(const __SPRT_WCHAR_T *__SPRT_RESTRICT, __sprt_va_list);
SPRT_API int __SPRT_ID(vfwprintf)(__SPRT_ID(FILE) * __SPRT_RESTRICT,
		const __SPRT_WCHAR_T *__SPRT_RESTRICT, __sprt_va_list);
SPRT_API int __SPRT_ID(vswprintf)(__SPRT_WCHAR_T *__SPRT_RESTRICT, __SPRT_ID(size_t),
		const __SPRT_WCHAR_T *__SPRT_RESTRICT, __sprt_va_list);

SPRT_API int __SPRT_ID(wscanf)(const __SPRT_WCHAR_T *__SPRT_RESTRICT, ...);
SPRT_API int __SPRT_ID(
		fwscanf)(__SPRT_ID(FILE) * __SPRT_RESTRICT, const __SPRT_WCHAR_T *__SPRT_RESTRICT, ...);
SPRT_API int __SPRT_ID(
		swscanf)(const __SPRT_WCHAR_T *__SPRT_RESTRICT, const __SPRT_WCHAR_T *__SPRT_RESTRICT, ...);

SPRT_API int __SPRT_ID(vwscanf)(const __SPRT_WCHAR_T *__SPRT_RESTRICT, __sprt_va_list);
SPRT_API int __SPRT_ID(vfwscanf)(__SPRT_ID(FILE) * __SPRT_RESTRICT,
		const __SPRT_WCHAR_T *__SPRT_RESTRICT, __sprt_va_list);
SPRT_API int __SPRT_ID(vswscanf)(const __SPRT_WCHAR_T *__SPRT_RESTRICT,
		const __SPRT_WCHAR_T *__SPRT_RESTRICT, __sprt_va_list);

SPRT_API __SPRT_ID(wint_t) __SPRT_ID(fgetwc)(__SPRT_ID(FILE) *);
SPRT_API __SPRT_ID(wint_t) __SPRT_ID(getwc)(__SPRT_ID(FILE) *);
SPRT_API __SPRT_ID(wint_t) __SPRT_ID(getwchar)(void);

SPRT_API __SPRT_ID(wint_t) __SPRT_ID(fputwc)(__SPRT_WCHAR_T, __SPRT_ID(FILE) *);
SPRT_API __SPRT_ID(wint_t) __SPRT_ID(putwc)(__SPRT_WCHAR_T, __SPRT_ID(FILE) *);
SPRT_API __SPRT_ID(wint_t) __SPRT_ID(putwchar)(__SPRT_WCHAR_T);

SPRT_API __SPRT_WCHAR_T *__SPRT_ID(
		fgetws)(__SPRT_WCHAR_T *__SPRT_RESTRICT, int, __SPRT_ID(FILE) * __SPRT_RESTRICT);
SPRT_API int __SPRT_ID(
		fputws)(const __SPRT_WCHAR_T *__SPRT_RESTRICT, __SPRT_ID(FILE) * __SPRT_RESTRICT);

SPRT_API __SPRT_ID(wint_t) __SPRT_ID(ungetwc)(__SPRT_ID(wint_t), __SPRT_ID(FILE) *);

SPRT_API __SPRT_ID(size_t) __SPRT_ID(wcsftime)(__SPRT_WCHAR_T *__SPRT_RESTRICT, __SPRT_ID(size_t),
		const __SPRT_WCHAR_T *__SPRT_RESTRICT, const struct __SPRT_TM_NAME *__SPRT_RESTRICT);


SPRT_API __SPRT_ID(wint_t) __SPRT_ID(fgetwc_unlocked)(__SPRT_ID(FILE) *);
SPRT_API __SPRT_ID(wint_t) __SPRT_ID(getwc_unlocked)(__SPRT_ID(FILE) *);
SPRT_API __SPRT_ID(wint_t) __SPRT_ID(getwchar_unlocked)(void);
SPRT_API __SPRT_ID(wint_t) __SPRT_ID(fputwc_unlocked)(__SPRT_WCHAR_T, __SPRT_ID(FILE) *);
SPRT_API __SPRT_ID(wint_t) __SPRT_ID(putwc_unlocked)(__SPRT_WCHAR_T, __SPRT_ID(FILE) *);
SPRT_API __SPRT_ID(wint_t) __SPRT_ID(putwchar_unlocked)(__SPRT_WCHAR_T);
SPRT_API __SPRT_WCHAR_T *__SPRT_ID(
		fgetws_unlocked)(__SPRT_WCHAR_T *__SPRT_RESTRICT, int, __SPRT_ID(FILE) * __SPRT_RESTRICT);
SPRT_API int __SPRT_ID(
		fputws_unlocked)(const __SPRT_WCHAR_T *__SPRT_RESTRICT, __SPRT_ID(FILE) * __SPRT_RESTRICT);

SPRT_API __SPRT_ID(size_t) __SPRT_ID(wcsftime_l)(__SPRT_WCHAR_T *__SPRT_RESTRICT, __SPRT_ID(size_t),
		const __SPRT_WCHAR_T *__SPRT_RESTRICT, const struct __SPRT_TM_NAME *__SPRT_RESTRICT,
		__SPRT_ID(locale_t));

SPRT_API __SPRT_ID(FILE) * __SPRT_ID(open_wmemstream)(__SPRT_WCHAR_T **, __SPRT_ID(size_t) *);
SPRT_API __SPRT_ID(size_t)
		__SPRT_ID(mbsnrtowcs)(__SPRT_WCHAR_T *__SPRT_RESTRICT, const char **__SPRT_RESTRICT,
				__SPRT_ID(size_t), __SPRT_ID(size_t), __SPRT_MBSTATE_NAME *__SPRT_RESTRICT);
SPRT_API __SPRT_ID(size_t)
		__SPRT_ID(wcsnrtombs)(char *__SPRT_RESTRICT, const __SPRT_WCHAR_T **__SPRT_RESTRICT,
				__SPRT_ID(size_t), __SPRT_ID(size_t), __SPRT_MBSTATE_NAME *__SPRT_RESTRICT);
SPRT_API __SPRT_WCHAR_T *__SPRT_ID(wcsdup)(const __SPRT_WCHAR_T *);
SPRT_API __SPRT_ID(size_t) __SPRT_ID(wcsnlen)(const __SPRT_WCHAR_T *, __SPRT_ID(size_t));
SPRT_API __SPRT_WCHAR_T *__SPRT_ID(
		wcpcpy)(__SPRT_WCHAR_T *__SPRT_RESTRICT, const __SPRT_WCHAR_T *__SPRT_RESTRICT);
SPRT_API __SPRT_WCHAR_T *__SPRT_ID(wcpncpy)(__SPRT_WCHAR_T *__SPRT_RESTRICT,
		const __SPRT_WCHAR_T *__SPRT_RESTRICT, __SPRT_ID(size_t));
SPRT_API int __SPRT_ID(wcscasecmp)(const __SPRT_WCHAR_T *, const __SPRT_WCHAR_T *);
SPRT_API int __SPRT_ID(
		wcscasecmp_l)(const __SPRT_WCHAR_T *, const __SPRT_WCHAR_T *, __SPRT_ID(locale_t));
SPRT_API int __SPRT_ID(
		wcsncasecmp)(const __SPRT_WCHAR_T *, const __SPRT_WCHAR_T *, __SPRT_ID(size_t));
SPRT_API int __SPRT_ID(wcsncasecmp_l)(const __SPRT_WCHAR_T *, const __SPRT_WCHAR_T *,
		__SPRT_ID(size_t), __SPRT_ID(locale_t));
SPRT_API int __SPRT_ID(
		wcscoll_l)(const __SPRT_WCHAR_T *, const __SPRT_WCHAR_T *, __SPRT_ID(locale_t));
SPRT_API __SPRT_ID(size_t) __SPRT_ID(wcsxfrm_l)(__SPRT_WCHAR_T *__SPRT_RESTRICT,
		const __SPRT_WCHAR_T *__SPRT_RESTRICT, __SPRT_ID(size_t), __SPRT_ID(locale_t));

SPRT_API int __SPRT_ID(wcwidth)(__SPRT_WCHAR_T);
SPRT_API int __SPRT_ID(wcswidth)(const __SPRT_WCHAR_T *, __SPRT_ID(size_t));

__SPRT_END_DECL

#endif
