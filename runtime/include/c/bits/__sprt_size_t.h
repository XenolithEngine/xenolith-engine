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

#ifndef CORE_RUNTIME_INCLUDE_C_BITS___SPRT_SIZE_T_H_
#define CORE_RUNTIME_INCLUDE_C_BITS___SPRT_SIZE_T_H_

#include <c/bits/__sprt_def.h>
#include <c/bits/__sprt_int.h>

#if __SPRT_CONFIG_COMPILER_ASSISTED_INTS && defined(__SIZE_TYPE__)

typedef __SIZE_TYPE__ __SPRT_ID(size_t);

#ifdef __SIZE_MAX__
#define __SPRT_SIZE_MAX __SIZE_MAX__
#else
#error "Compiler-assisted __SIZE_MAX__ is not defined"
#endif

#ifdef __SIZE_WIDTH__
#define __SPRT_SIZE_WIDTH __SIZE_WIDTH__
#else
#error "Compiler-assisted __SIZE_WIDTH__ is not defined"
#endif

#ifdef __SIZE_FMTX__
#define __SPRT_SIZE_FMTX __SIZE_FMTX__
#else
#error "Compiler-assisted __SIZE_FMTX__ is not defined"
#endif

#ifdef __SIZE_FMTo__
#define __SPRT_SIZE_FMTo __SIZE_FMTo__
#else
#error "Compiler-assisted __SIZE_FMTo__ is not defined"
#endif

#ifdef __SIZE_FMTu__
#define __SPRT_SIZE_FMTu __SIZE_FMTu__
#else
#error "Compiler-assisted __SIZE_FMTu__ is not defined"
#endif

#ifdef __SIZE_FMTx__
#define __SPRT_SIZE_FMTx __SIZE_FMTx__
#else
#error "Compiler-assisted __SIZE_FMTx__ is not defined"
#endif

#else // __SIZE_TYPE__
// clang-format off
// Use Data models specifications
#if defined(__LLP64__) || defined(_WIN64)

typedef unsigned long long int __SPRT_ID(size_t);
#define __SPRT_SIZE_MAX __SPRT_ULLINT_MAX
#define __SPRT_SIZE_WIDTH __SPRT_ULLINT_WIDTH

#define __SPRT_SIZE_FMTX __SPRT_ULLINT_FMTX
#define __SPRT_SIZE_FMTo __SPRT_ULLINT_FMTo
#define __SPRT_SIZE_FMTu __SPRT_ULLINT_FMTu
#define __SPRT_SIZE_FMTx __SPRT_ULLINT_FMTx

#elif defined(_WIN32) || defined(__ILP32__)

typedef unsigned long int __SPRT_ID(size_t);
#define __SPRT_SIZE_MAX __SPRT_ULINT_MAX
#define __SPRT_SIZE_WIDTH __SPRT_ULINT_WIDTH

#define __SPRT_SIZE_FMTX __SPRT_ULINT_FMTX
#define __SPRT_SIZE_FMTo __SPRT_ULINT_FMTo
#define __SPRT_SIZE_FMTu __SPRT_ULINT_FMTu
#define __SPRT_SIZE_FMTx __SPRT_ULINT_FMTx

#elif defined(__LP64__)

typedef unsigned long int __SPRT_ID(size_t);
#define __SPRT_SIZE_MAX __SPRT_ULINT_MAX
#define __SPRT_SIZE_WIDTH __SPRT_ULINT_WIDTH

#define __SPRT_SIZE_FMTX __SPRT_ULINT_FMTX
#define __SPRT_SIZE_FMTo __SPRT_ULINT_FMTo
#define __SPRT_SIZE_FMTu __SPRT_ULINT_FMTu
#define __SPRT_SIZE_FMTx __SPRT_ULINT_FMTx

#else
#error "Unknown data model (see https://en.cppreference.com/w/cpp/language/types.html#Data_models)"
#endif
// clang-format on
#endif // __SIZE_TYPE__

typedef __SPRT_ID(size_t) __SPRT_ID(rsize_t);

#define __SPRT_SIZE_MIN 0
#define __SPRT_RSIZE_MAX __SPRT_SIZE_MAX
#define __SPRT_RSIZE_MIN __SPRT_SIZE_MIN

#endif // CORE_RUNTIME_INCLUDE_C_BITS___SPRT_SIZE_T_H_
