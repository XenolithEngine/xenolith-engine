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

#ifndef CORE_RUNTIME_INCLUDE_C___SPRT_FLOAT_H_
#define CORE_RUNTIME_INCLUDE_C___SPRT_FLOAT_H_

#include <c/bits/__sprt_def.h>
#include <c/bits/__sprt_float16.h>
#include <c/bits/__sprt_float.h>
#include <c/bits/__sprt_double.h>
#include <c/bits/__sprt_long_double.h>

// Stappler runtime is designed to work in conjunction with a compiler in the toolchain,
// therefore, if the compiler has preferences regarding limits, we use them first

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_flt_rounds)
#define __SPRT_FLT_ROUNDS (__builtin_flt_rounds())
#else
int __SPRT_ID(__flt_rounds)(void);
#define __SPRT_FLT_ROUNDS (__SPRT_ID(__flt_rounds)())
#endif

#ifdef __FLT_EVAL_METHOD__
#define __SPRT_FLT_EVAL_METHOD __FLT_EVAL_METHOD__
#else
#define __SPRT_FLT_EVAL_METHOD 0
#endif

#ifdef __DECIMAL_DIG__
#define __SPRT_DECIMAL_DIG __DECIMAL_DIG__
#else
#define __SPRT_DECIMAL_DIG __SPRT_LDBL_DECIMAL_DIG
#endif

#endif // CORE_RUNTIME_INCLUDE_C___SPRT_FLOAT_H_
