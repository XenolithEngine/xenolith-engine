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

#ifndef CORE_RUNTIME_INCLUDE_C_BITS___SPRT_FLOAT16_H_
#define CORE_RUNTIME_INCLUDE_C_BITS___SPRT_FLOAT16_H_

// clang-format off
#ifdef __FLT16_DECIMAL_DIG__
#define __SPRT_FLT16_DECIMAL_DIG __FLT16_DECIMAL_DIG__
#else
#define __SPRT_FLT16_DECIMAL_DIG 5
#endif

#ifdef __FLT16_DENORM_MIN__
#define __SPRT_FLT16_DENORM_MIN __FLT16_DENORM_MIN__
#else
#define __SPRT_FLT16_DENORM_MIN 5.9604644775390625e-8F16
#endif

#ifdef __FLT16_DIG__
#define __SPRT_FLT16_DIG __FLT16_DIG__
#else
#define __SPRT_FLT16_DIG 3
#endif

#ifdef __FLT16_EPSILON__
#define __SPRT_FLT16_EPSILON __FLT16_EPSILON__
#else
#define __SPRT_FLT16_EPSILON 9.765625e-4F16
#endif

#ifdef __FLT16_HAS_DENORM__
#define __SPRT_FLT16_HAS_DENORM __FLT16_HAS_DENORM__
#define __SPRT_FLT16_HAS_SUBNORM __FLT16_HAS_DENORM__
#else
#define __SPRT_FLT16_HAS_DENORM 1
#define __SPRT_FLT16_HAS_SUBNORM 1
#endif

#ifdef __FLT16_HAS_INFINITY__
#define __SPRT_FLT16_HAS_INFINITY __FLT16_HAS_INFINITY__
#else
#define __SPRT_FLT16_HAS_INFINITY 1
#endif

#ifdef __FLT16_HAS_QUIET_NAN__
#define __SPRT_FLT16_HAS_QUIET_NAN __FLT16_HAS_QUIET_NAN__
#else
#define __SPRT_FLT16_HAS_QUIET_NAN 1
#endif

#ifdef __FLT16_MANT_DIG__
#define __SPRT_FLT16_MANT_DIG __FLT16_MANT_DIG__
#else
#define __SPRT_FLT16_MANT_DIG 11
#endif

#ifdef __FLT16_MAX_10_EXP__
#define __SPRT_FLT16_MAX_10_EXP __FLT16_MAX_10_EXP__
#else
#define __SPRT_FLT16_MAX_10_EXP 4
#endif

#ifdef __FLT16_MAX_EXP__
#define __SPRT_FLT16_MAX_EXP __FLT16_MAX_EXP__
#else
#define __SPRT_FLT16_MAX_EXP 16
#endif

#ifdef __FLT16_MAX__
#define __SPRT_FLT16_MAX __FLT16_MAX__
#else
#define __SPRT_FLT16_MAX 6.5504e+4F16
#endif

#ifdef __FLT16_MIN_10_EXP__
#define __SPRT_FLT16_MIN_10_EXP __FLT16_MIN_10_EXP__
#else
#define __SPRT_FLT16_MIN_10_EXP  (-4)
#endif

#ifdef __FLT16_MIN_EXP__
#define __SPRT_FLT16_MIN_EXP __FLT16_MIN_EXP__
#else
#define __SPRT_FLT16_MIN_EXP (-13)
#endif

#ifdef __FLT16_MIN__
#define __SPRT_FLT16_MIN __FLT16_MIN__
#else
#define __SPRT_FLT16_MIN 6.103515625e-5F16
#endif

#ifdef __FLT16_NORM_MAX__
#define __SPRT_FLT16_NORM_MAX __FLT16_NORM_MAX__
#else
#define __SPRT_FLT16_NORM_MAX 6.5504e+4F16
#endif

#define __SPRT_FLT16_TRUE_MIN __SPRT_FLT16_DENORM_MIN
// clang-format on

#endif // CORE_RUNTIME_INCLUDE_C_BITS___SPRT_FLOAT16_H_
