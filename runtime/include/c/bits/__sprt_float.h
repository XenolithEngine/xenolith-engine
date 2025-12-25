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

#ifndef CORE_RUNTIME_INCLUDE_C_BITS___SPRT_FLOAT_H_
#define CORE_RUNTIME_INCLUDE_C_BITS___SPRT_FLOAT_H_

#include <c/bits/__sprt_def.h>

// clang-format off
#ifdef __FLT_DECIMAL_DIG__
#define __SPRT_FLT_DECIMAL_DIG __FLT_DECIMAL_DIG__
#else
#define __SPRT_FLT_DECIMAL_DIG 9
#endif

#ifdef __FLT_DENORM_MIN__
#define __SPRT_FLT_DENORM_MIN __FLT_DENORM_MIN__
#else
#define __SPRT_FLT_DENORM_MIN 1.40129846e-45F
#endif

#ifdef __FLT_DIG__
#define __SPRT_FLT_DIG __FLT_DIG__
#else
#define __SPRT_FLT_DIG 6
#endif

#ifdef __FLT_EPSILON__
#define __SPRT_FLT_EPSILON __FLT_EPSILON__
#else
#define __SPRT_FLT_EPSILON 1.19209290e-7F
#endif

#ifdef __FLT_HAS_DENORM__
#define __SPRT_FLT_HAS_DENORM __FLT_HAS_DENORM__
#define __SPRT_FLT_HAS_SUBNORM __FLT_HAS_DENORM__
#else
#define __SPRT_FLT_HAS_DENORM 1
#define __SPRT_FLT_HAS_SUBNORM 1
#endif

#ifdef __FLT_HAS_INFINITY__
#define __SPRT_FLT_HAS_INFINITY __FLT_HAS_INFINITY__
#else
#define __SPRT_FLT_HAS_INFINITY 1
#endif

#ifdef __FLT_HAS_QUIET_NAN__
#define __SPRT_FLT_HAS_QUIET_NAN __FLT_HAS_QUIET_NAN__
#else
#define __SPRT_FLT_HAS_QUIET_NAN 1
#endif

#ifdef __FLT_MANT_DIG__
#define __SPRT_FLT_MANT_DIG __FLT_MANT_DIG__
#else
#define __SPRT_FLT_MANT_DIG 24
#endif

#ifdef __FLT_MAX_10_EXP__
#define __SPRT_FLT_MAX_10_EXP __FLT_MAX_10_EXP__
#else
#define __SPRT_FLT_MAX_10_EXP 38
#endif

#ifdef __FLT_MAX_EXP__
#define __SPRT_FLT_MAX_EXP __FLT_MAX_EXP__
#else
#define __SPRT_FLT_MAX_EXP 128
#endif

#ifdef __FLT_MAX__
#define __SPRT_FLT_MAX __FLT_MAX__
#else
#define __SPRT_FLT_MAX 3.40282347e+38F
#endif

#ifdef __FLT_MIN_10_EXP__
#define __SPRT_FLT_MIN_10_EXP __FLT_MIN_10_EXP__
#else
#define __SPRT_FLT_MIN_10_EXP (-37)
#endif

#ifdef __FLT_MIN_EXP__
#define __SPRT_FLT_MIN_EXP __FLT_MIN_EXP__
#else
#define __SPRT_FLT_MIN_EXP (-125)
#endif

#ifdef __FLT_MIN__
#define __SPRT_FLT_MIN __FLT_MIN__
#else
#define __SPRT_FLT_MIN 1.17549435e-38F
#endif

#ifdef __FLT_NORM_MAX__
#define __SPRT_FLT_NORM_MAX __FLT_NORM_MAX__
#else
#define __SPRT_FLT_NORM_MAX 3.40282347e+38F
#endif

#ifdef __FLT_RADIX__
#define __SPRT_FLT_RADIX __FLT_RADIX__
#else
#define __SPRT_FLT_RADIX 2
#endif

#define __SPRT_FLT_TRUE_MIN __SPRT_FLT_DENORM_MIN
// clang-format on

#if defined(__FLT_EVAL_METHOD__) && __FLT_EVAL_METHOD__ == 2
typedef long double __SPRT_ID(float_t);
typedef long double __SPRT_ID(double_t);
#elif defined(__FLT_EVAL_METHOD__) && __FLT_EVAL_METHOD__ == 1
typedef double __SPRT_ID(float_t);
typedef double __SPRT_ID(double_t);
#else
typedef float __SPRT_ID(float_t);
typedef double __SPRT_ID(double_t);
#endif

#endif // CORE_RUNTIME_INCLUDE_C_BITS___SPRT_FLOAT16_H_
