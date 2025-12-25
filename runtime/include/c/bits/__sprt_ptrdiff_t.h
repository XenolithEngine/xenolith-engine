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

#ifndef CORE_RUNTIME_INCLUDE_C_BITS___SPRT_PTRDIFF_T_H_
#define CORE_RUNTIME_INCLUDE_C_BITS___SPRT_PTRDIFF_T_H_

#include <c/bits/__sprt_def.h>
#include <c/bits/__sprt_int.h>

#if __SPRT_CONFIG_COMPILER_ASSISTED_INTS && defined(__PTRDIFF_TYPE__)

typedef __PTRDIFF_TYPE__ __SPRT_ID(ptrdiff_t);

#ifdef __PTRDIFF_MAX__
#define __SPRT_PTRDIFF_MAX __PTRDIFF_MAX__
#else
#error "Compiler-assisted __PTRDIFF_MAX__ is not defined"
#endif

#ifdef __PTRDIFF_WIDTH__
#define __SPRT_PTRDIFF_WIDTH __PTRDIFF_WIDTH__
#else
#error "Compiler-assisted __PTRDIFF_WIDTH__ is not defined"
#endif

#ifdef __PTRDIFF_FMTd__
#define __SPRT_PTRDIFF_FMTd __PTRDIFF_FMTd__
#else
#error "Compiler-assisted __PTRDIFF_FMTd__ is not defined"
#endif

#ifdef __PTRDIFF_FMTi__
#define __SPRT_PTRDIFF_FMTi __PTRDIFF_FMTi__
#else
#error "Compiler-assisted __PTRDIFF_FMTi__ is not defined"
#endif

#else

#if defined(__LLP64__) || defined(_WIN64)

typedef long long int __SPRT_ID(ptrdiff_t);
#define __SPRT_PTRDIFF_MAX __SPRT_LLINT_MAX
#define __SPRT_PTRDIFF_WIDTH __SPRT_LLINT_WIDTH
#define __SPRT_PTRDIFF_FMTd __SPRT_ULLINT_FMTd
#define __SPRT_PTRDIFF_FMTi __SPRT_ULLINT_FMTi

#elif defined(_WIN32) || defined(__ILP32__)

typedef int __SPRT_ID(ptrdiff_t);
#define __SPRT_PTRDIFF_MAX __SPRT_INT_MAX
#define __SPRT_PTRDIFF_WIDTH __SPRT_INT_WIDTH
#define __SPRT_PTRDIFF_FMTd __SPRT_INT_FMTd
#define __SPRT_PTRDIFF_FMTi __SPRT_INT_FMTi

#elif defined(__LP64__)

typedef long int __SPRT_ID(ptrdiff_t);
#define __SPRT_PTRDIFF_MAX __SPRT_LINT_MAX
#define __SPRT_PTRDIFF_WIDTH __SPRT_LINT_WIDTH
#define __SPRT_PTRDIFF_FMTd __SPRT_ULINT_FMTd
#define __SPRT_PTRDIFF_FMTi __SPRT_ULINT_FMTi

#else
#error "Unknown data model (see https://en.cppreference.com/w/cpp/language/types.html#Data_models)"
#endif

#endif

#define __SPRT_PTRDIFF_MIN (-1-__SPRT_PTRDIFF_MAX)

#endif
