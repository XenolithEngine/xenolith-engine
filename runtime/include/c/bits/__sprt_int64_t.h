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

#ifndef CORE_RUNTIME_INCLUDE_C_BITS___SPRT_INT64_T_H_
#define CORE_RUNTIME_INCLUDE_C_BITS___SPRT_INT64_T_H_

#include <c/bits/__sprt_def.h>
#include <c/bits/__sprt_int.h>

#if __SPRT_CONFIG_COMPILER_ASSISTED_INTS && defined(__INT64_TYPE__)

typedef __INT64_TYPE__ __SPRT_ID(int64_t);

#ifdef __INT64_MAX__
#define __SPRT_INT64_MAX __INT64_MAX__
#else
#error "Compiler-assisted __INT64_MAX__ is not defined"
#endif

#define __SPRT_INT64_WIDTH 64

#ifdef __INT64_FMTd__
#define __SPRT_INT64_FMTd __INT64_FMTd__
#else
#error "Compiler-assisted __INT64_FMTd__ is not defined"
#endif

#ifdef __INT64_FMTi__
#define __SPRT_INT64_FMTi __INT64_FMTi__
#else
#error "Compiler-assisted __INT64_FMTi__ is not defined"
#endif

#ifdef __INT64_C
#define __SPRT_INT64_C(C) __INT64_C(C)
#else
#error "Compiler-assisted __INT64_FMTx__ is not defined"
#endif

#ifdef __INT64_C_SUFFIX__
#define __INT64_C_SUFFIX __INT64_C_SUFFIX__
#else
#error "Compiler-assisted __INT64_FMTx__ is not defined"
#endif

#else // __INT64_TYPE__
// clang-format off
// Use Data models specifications
#if defined(__LLP64__) || defined(_WIN64) || defined(_WIN32) || defined(__ILP32__)

typedef long long int __SPRT_ID(int64_t);
#define __SPRT_INT64_MAX __SPRT_LLINT_MAX
#define __SPRT_INT64_WIDTH __SPRT_LLINT_WIDTH
#define __SPRT_INT64_FMTd __SPRT_LLINT_FMTd
#define __SPRT_INT64_FMTi __SPRT_LLINT_FMTi
#define __SPRT_INT64_C(C) __SPRT_LLINT_C(C)
#define __SPRT_INT64_C_SUFFIX __SPRT_LLINT_C_SUFFIX

#elif defined(__LP64__)

typedef long int __SPRT_ID(int64_t);
#define __SPRT_INT64_MAX __SPRT_LINT_MAX
#define __SPRT_INT64_WIDTH __SPRT_LINT_WIDTH
#define __SPRT_INT64_FMTd __SPRT_LINT_FMTd
#define __SPRT_INT64_FMTi __SPRT_LINT_FMTi
#define __SPRT_INT64_C(C) __SPRT_LINT_C(C)
#define __SPRT_INT64_C_SUFFIX __SPRT_LLINT_C_SUFFIX

#else
#error "Unknown data model (see https://en.cppreference.com/w/cpp/language/types.html#Data_models)"
#endif
// clang-format on
#endif // __INT64_TYPE__

#endif // CORE_RUNTIME_INCLUDE_C_BITS___SPRT_INT64_T_H_
