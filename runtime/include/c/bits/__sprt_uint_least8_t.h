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

#ifndef CORE_RUNTIME_INCLUDE_C_BITS___SPRT_UINT_LEAST8_T_H_
#define CORE_RUNTIME_INCLUDE_C_BITS___SPRT_UINT_LEAST8_T_H_

#include <c/bits/__sprt_def.h>
#include <c/bits/__sprt_int.h>

#if __SPRT_CONFIG_COMPILER_ASSISTED_INTS && defined(__UINT_LEAST8_TYPE__)

typedef __UINT_LEAST8_TYPE__ __SPRT_ID(uint_least8_t);

#ifdef __UINT_LEAST8_MAX__
#define __SPRT_UINT_LEAST8_MAX __UINT_LEAST8_MAX__
#else
#error "Compiler-assisted __UINT_LEAST8_MAX__ is not defined"
#endif

#define __SPRT_UINT_LEAST8_WIDTH 8

#ifdef __UINT_LEAST8_FMTX__
#define __SPRT_UINT_LEAST8_FMTX __UINT_LEAST8_FMTX__
#else
#error "Compiler-assisted __UINT_LEAST8_FMTX__ is not defined"
#endif

#ifdef __UINT_LEAST8_FMTo__
#define __SPRT_UINT_LEAST8_FMTo __UINT_LEAST8_FMTo__
#else
#error "Compiler-assisted __UINT_LEAST8_FMTo__ is not defined"
#endif

#ifdef __UINT_LEAST8_FMTu__
#define __SPRT_UINT_LEAST8_FMTu __UINT_LEAST8_FMTu__
#else
#error "Compiler-assisted __UINT_LEAST8_FMTu__ is not defined"
#endif

#ifdef __UINT_LEAST8_FMTx__
#define __SPRT_UINT_LEAST8_FMTx __UINT_LEAST8_FMTx__
#else
#error "Compiler-assisted __UINT_LEAST8_FMTx__ is not defined"
#endif

#else // __UINT_LEAST8_TYPE__
// clang-format off
// Use Data models specifications
#if defined(__LLP64__) || defined(_WIN64) || defined(_WIN32) || defined(__ILP32__) || defined(__LP64__)

typedef unsigned char __SPRT_ID(uint_least8_t);
#define __SPRT_UINT_LEAST8_MAX __SPRT_USSHRT_MAX
#define __SPRT_UINT_LEAST8_WIDTH __SPRT_USSHRT_WIDTH
#define __SPRT_UINT_LEAST8_FMTX __SPRT_USSHRT_FMTX
#define __SPRT_UINT_LEAST8_FMTo __SPRT_USSHRT_FMTo
#define __SPRT_UINT_LEAST8_FMTu __SPRT_USSHRT_FMTu
#define __SPRT_UINT_LEAST8_FMTx __SPRT_USSHRT_FMTx

#else
#error "Unknown data model (see https://en.cppreference.com/w/cpp/language/types.html#Data_models)"
#endif
// clang-format on
#endif // __UINT_LEAST8_TYPE__

#endif
