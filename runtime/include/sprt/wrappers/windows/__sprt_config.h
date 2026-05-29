/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

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

#ifndef SPRT_WRAPPERS_WINDOWS___SPRT_CONFIG_H_
#define SPRT_WRAPPERS_WINDOWS___SPRT_CONFIG_H_

#include <sprt/c/bits/__sprt_def.h>
#include <sprt/c/bits/__sprt_wchar_t.h>

#if _WIN32

#ifndef __SPRT_WIN_USE_IMPORT_LIB
#define __SPRT_WIN_USE_IMPORT_LIB 1
#endif

#ifndef __SPRT_WIN_USE_IMPORT_STRING_LIB
#define __SPRT_WIN_USE_IMPORT_STRING_LIB 1
#endif

#if __SPRT_WIN_USE_IMPORT_LIB
#define __SPRT_WIN_IMPORT __SPRT_DLLIMPORT
#else
#define __SPRT_WIN_IMPORT
#endif

#if __SPRT_WIN_USE_IMPORT_STRING_LIB
#define __SPRT_WIN_STRING_IMPORT __SPRT_DLLIMPORT
#else
#define __SPRT_WIN_STRING_IMPORT
#endif

#if __SPRT_WIN_USE_IMPORT_STRING_LIB
__SPRT_BEGIN_DECL

__SPRT_WIN_STRING_IMPORT int memcmp(const void *s1, const void *s2, size_t len) __SPRT_NOEXCEPT;

__SPRT_WIN_STRING_IMPORT void *memcpy(void *__SPRT_RESTRICT dest,
		const void *__SPRT_RESTRICT source, size_t size) __SPRT_NOEXCEPT;

__SPRT_WIN_STRING_IMPORT void *memmove(void *dst, const void *src, size_t len) __SPRT_NOEXCEPT;

__SPRT_WIN_STRING_IMPORT void *memset(void *dst, int c, size_t len) __SPRT_NOEXCEPT;

__SPRT_WIN_STRING_IMPORT char *strcpy(char *__SPRT_RESTRICT dest,
		const char *__SPRT_RESTRICT src) __SPRT_NOEXCEPT;

__SPRT_WIN_STRING_IMPORT size_t strlen(const char *s) __SPRT_NOEXCEPT;

__SPRT_WIN_STRING_IMPORT char *strncpy(char *__SPRT_RESTRICT dest, const char *__SPRT_RESTRICT src,
		size_t size) __SPRT_NOEXCEPT;

__SPRT_WIN_STRING_IMPORT size_t strnlen(const char *s, size_t size) __SPRT_NOEXCEPT;

__SPRT_WIN_STRING_IMPORT char *strstr(const char *str, const char *nstr) __SPRT_NOEXCEPT;

__SPRT_WIN_STRING_IMPORT char *strchr(const char *s, int c) __SPRT_NOEXCEPT;

__SPRT_WIN_STRING_IMPORT int strcmp(const char *s1, const char *s2) __SPRT_NOEXCEPT;

__SPRT_WIN_STRING_IMPORT int strncmp(const char *s1, const char *s2, size_t len) __SPRT_NOEXCEPT;

__SPRT_WIN_STRING_IMPORT __SPRT_ID(wchar_t)
		* wcscpy(__SPRT_ID(wchar_t) * __SPRT_RESTRICT dest,
				const __SPRT_ID(wchar_t) * __SPRT_RESTRICT src) __SPRT_NOEXCEPT;

__SPRT_WIN_STRING_IMPORT size_t wcslen(const __SPRT_ID(wchar_t) * s) __SPRT_NOEXCEPT;

__SPRT_WIN_STRING_IMPORT __SPRT_ID(wchar_t)
		* wcsncpy(__SPRT_ID(wchar_t) * __SPRT_RESTRICT dest,
				const __SPRT_ID(wchar_t) * __SPRT_RESTRICT src, size_t size) __SPRT_NOEXCEPT;

__SPRT_WIN_STRING_IMPORT size_t wcsnlen(const __SPRT_ID(wchar_t) * s, size_t size) __SPRT_NOEXCEPT;

__SPRT_WIN_STRING_IMPORT const __SPRT_ID(wchar_t)
		* wcsstr(const __SPRT_ID(wchar_t) * str, const __SPRT_ID(wchar_t) * nstr) __SPRT_NOEXCEPT;

__SPRT_END_DECL
#endif

#endif

#endif // SPRT_WRAPPERS_WINDOWS___SPRT_CONFIG_H_
