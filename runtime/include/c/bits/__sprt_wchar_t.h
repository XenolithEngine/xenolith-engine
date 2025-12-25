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

#ifndef CORE_RUNTIME_INCLUDE_C_BITS___SPRT_WCHAR_T_H_
#define CORE_RUNTIME_INCLUDE_C_BITS___SPRT_WCHAR_T_H_

#include <c/bits/__sprt_def.h>

#if __SPRT_CONFIG_COMPILER_ASSISTED_INTS && defined(__WCHAR_TYPE__)

#ifndef __cplusplus
typedef __WCHAR_TYPE__ __SPRT_ID(wchar_t);
#endif

#ifdef __WCHAR_MAX__
#define __SPRT_WCHAR_MAX __WCHAR_MAX__
#else
#error "Compiler-assisted __WCHAR_MAX__ is not defined"
#endif

#ifdef __WCHAR_WIDTH__
#define __SPRT_WCHAR_WIDTH __WCHAR_WIDTH__
#else
#error "Compiler-assisted __WCHAR_WIDTH__ is not defined"
#endif

#if L'\0' - 1 > 0
#define __SPRT_WCHAR_MIN (0+L'\0')
#else
#define __SPRT_WCHAR_MIN (-1-__SPRT_WCHAR_MAX+L'\0')
#endif

#else

#if SPRT_WINDOWS

#ifndef __cplusplus
typedef unsigned short __SPRT_ID(wchar_t);
#endif

#define __SPRT_WCHAR_MAX (0xffff)
#define __SPRT_WCHAR_MIN (0)
#define __SPRT_WCHAR_WIDTH 16

#else

#ifndef __cplusplus
typedef int __SPRT_ID(wchar_t);
#endif

#define __SPRT_WCHAR_MAX 0x7fff'ffff
#define __SPRT_WCHAR_MIN (-1-0x7fff'ffff)
#define __SPRT_WCHAR_WIDTH 32

#endif

#endif

#endif // CORE_RUNTIME_INCLUDE_C_BITS___SPRT_WCHAR_T_H_
