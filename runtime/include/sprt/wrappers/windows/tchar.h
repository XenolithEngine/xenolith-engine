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

#ifndef SPRT_WRAPPERS_WINDOWS_TCHAR_H_
#define SPRT_WRAPPERS_WINDOWS_TCHAR_H_

#include <sprt/c/__sprt_string.h>
#include <sprt/c/__sprt_wchar.h>

#if _UNICODE

#define _T(Str) L ## Str
#define TEXT(Str) L ## Str

#define _tcscpy __sprt_wcscpy
#define _tcspbrk __sprt_wcspbrk
#define _tcslen __sprt_wcslen
#define _tcsncmp __sprt_wcsncmp
#define LPCTSTR LPCWSTR

SPRT_FORCEINLINE int _vsntprintf(__SPRT_ID(wchar_t) * __SPRT_RESTRICT buf, size_t n,
		const __SPRT_ID(wchar_t) * __SPRT_RESTRICT fmt, __sprt_va_list arg) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_vswprintf(buf, n, fmt, arg);
}
#endif

#else
#define _T(Str) Str
#define TEXT(Str) Str

#define _tcscpy strcpy
#define LPCTSTR LPCSTR

SPRT_FORCEINLINE int _vsntprintf(char *__SPRT_RESTRICT buf, size_t n,
		const char *__SPRT_RESTRICT fmt, __sprt_va_list arg) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_vsnprintf(buf, n, fmt, arg);
}
#endif

#endif

#endif // SPRT_WRAPPERS_WINDOWS_TCHAR_H_
