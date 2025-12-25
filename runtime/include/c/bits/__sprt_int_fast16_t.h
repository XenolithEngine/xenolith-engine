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

#ifndef CORE_RUNTIME_INCLUDE_C_BITS___SPRT_INT_FAST16_T_H_
#define CORE_RUNTIME_INCLUDE_C_BITS___SPRT_INT_FAST16_T_H_

#include <c/bits/__sprt_def.h>
#include <c/bits/__sprt_int.h>

#if __SPRT_CONFIG_COMPILER_ASSISTED_INTS && defined(__INT_FAST16_TYPE__)

typedef __INT_FAST16_TYPE__ __SPRT_ID(int_fast16_t);

#ifdef __INT_FAST16_MAX__
#define __SPRT_INT_FAST16_MAX __INT_FAST16_MAX__
#else
#error "Compiler-assisted __INT_FAST16_MAX__ is not defined"
#endif

#define __SPRT_INT_FAST16_WIDTH 16

#ifdef __INT_FAST16_FMTd__
#define __SPRT_INT_FAST16_FMTd __INT_FAST16_FMTd__
#else
#error "Compiler-assisted __INT_FAST16_FMTd__ is not defined"
#endif

#ifdef __INT_FAST16_FMTi__
#define __SPRT_INT_FAST16_FMTo __INT_FAST16_FMTi__
#else
#error "Compiler-assisted __INT_FAST16_FMTi__ is not defined"
#endif

#else // __INT_FAST16_TYPE__
// clang-format off
// Use Data models specifications
#if defined(__LLP64__) || defined(_WIN64) || defined(_WIN32) || defined(__ILP32__) || defined(__LP64__)

typedef int __SPRT_ID(int_fast16_t);
#define __SPRT_INT_FAST16_MAX __SPRT_SHRT_MAX
#define __SPRT_INT_FAST16_WIDTH __SPRT_SHRT_WIDTH
#define __SPRT_INT_FAST16_FMTd __SPRT_SHRT_FMTd
#define __SPRT_INT_FAST16_FMTi __SPRT_SHRT_FMTi

#else
#error "Unknown data model (see https://en.cppreference.com/w/cpp/language/types.html#Data_models)"
#endif
// clang-format on
#endif // __INT_FAST16_TYPE__

#endif
