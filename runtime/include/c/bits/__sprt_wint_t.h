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

#ifndef CORE_RUNTIME_INCLUDE_C_BITS___SPRT_WINT_T_H_
#define CORE_RUNTIME_INCLUDE_C_BITS___SPRT_WINT_T_H_

#include <c/bits/__sprt_def.h>
#include <c/bits/__sprt_int32_t.h>

#if __SPRT_CONFIG_COMPILER_ASSISTED_INTS && defined(__WINT_TYPE__)

typedef __WINT_TYPE__ __SPRT_ID(wint_t);

#ifdef __WINT_MAX__
#define __SPRT_WINT_MAX __WINT_MAX__
#else
#error "Compiler-assisted __WINT_MAX__ is not defined"
#endif

#ifdef __WINT_WIDTH__
#define __SPRT_WINT_WIDTH __WINT_WIDTH__
#else
#error "Compiler-assisted __WINT_WIDTH__ is not defined"
#endif

#if defined(__WINT_UNSIGNED__)
#define __SPRT_WINT_MIN 0
#else
#define __SPRT_WINT_MIN (-1-__SPRT_WINT_MAX)
#endif

#else

#if SPRT_WINDOWS

typedef unsigned short __SPRT_ID(wint_t);
#define __SPRT_WINT_MAX 0xffff
#define __SPRT_WINT_WIDTH 16
#define __SPRT_WINT_MIN 0

#else

typedef unsigned int __SPRT_ID(wint_t);
#define __SPRT_WINT_MAX 0xffff'ffff
#define __SPRT_WINT_WIDTH 32
#define __SPRT_WINT_MIN 0

#endif

#endif

#endif // CORE_RUNTIME_INCLUDE_C_BITS___SPRT_WINT_T_H_
