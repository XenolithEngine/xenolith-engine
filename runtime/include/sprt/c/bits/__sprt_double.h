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

#ifndef CORE_RUNTIME_INCLUDE_C_BITS___SPRT_DOUBLE_H_
#define CORE_RUNTIME_INCLUDE_C_BITS___SPRT_DOUBLE_H_

// clang-format off
#ifdef __DBL_DECIMAL_DIG__
#define __SPRT_DBL_DECIMAL_DIG __DBL_DECIMAL_DIG__
#else
#define __SPRT_DBL_DECIMAL_DIG 17
#endif

#ifdef __DBL_DENORM_MIN__
#define __SPRT_DBL_DENORM_MIN __DBL_DENORM_MIN__
#else
#define __SPRT_DBL_DENORM_MIN 4.9406564584124654e-324
#endif

#ifdef __DBL_DIG__
#define __SPRT_DBL_DIG __DBL_DIG__
#else
#define __SPRT_DBL_DIG 15
#endif

#ifdef __DBL_EPSILON__
#define __SPRT_DBL_EPSILON __DBL_EPSILON__
#else
#define __SPRT_DBL_EPSILON 2.2204460492503131e-16
#endif

#ifdef __DBL_HAS_DENORM__
#define __SPRT_DBL_HAS_DENORM __DBL_HAS_DENORM__
#define __SPRT_DBL_HAS_SUBNORM __DBL_HAS_DENORM__
#else
#define __SPRT_DBL_HAS_DENORM 1
#define __SPRT_DBL_HAS_SUBNORM 1
#endif

#ifdef __DBL_HAS_INFINITY__
#define __SPRT_DBL_HAS_INFINITY __DBL_HAS_INFINITY__
#else
#define __SPRT_DBL_HAS_INFINITY 1
#endif

#ifdef __DBL_HAS_QUIET_NAN__
#define __SPRT_DBL_HAS_QUIET_NAN __DBL_HAS_QUIET_NAN__
#else
#define __SPRT_DBL_HAS_QUIET_NAN 1
#endif

#ifdef __DBL_MANT_DIG__
#define __SPRT_DBL_MANT_DIG __DBL_MANT_DIG__
#else
#define __SPRT_DBL_MANT_DIG 53
#endif

#ifdef __DBL_MAX_10_EXP__
#define __SPRT_DBL_MAX_10_EXP __DBL_MAX_10_EXP__
#else
#define __SPRT_DBL_MAX_10_EXP 308
#endif

#ifdef __DBL_MAX_EXP__
#define __SPRT_DBL_MAX_EXP __DBL_MAX_EXP__
#else
#define __SPRT_DBL_MAX_EXP 1024
#endif

#ifdef __DBL_MAX__
#define __SPRT_DBL_MAX __DBL_MAX__
#else
#define __SPRT_DBL_MAX 1.79769313486231570815e+308
#endif

#ifdef __DBL_MIN_10_EXP__
#define __SPRT_DBL_MIN_10_EXP __DBL_MIN_10_EXP__
#else
#define __SPRT_DBL_MIN_10_EXP (-307)
#endif

#ifdef __DBL_MIN_EXP__
#define __SPRT_DBL_MIN_EXP __DBL_MIN_EXP__
#else
#define __SPRT_DBL_MIN_EXP (-1021)
#endif

#ifdef __DBL_MIN__
#define __SPRT_DBL_MIN __DBL_MIN__
#else
#define __SPRT_DBL_MIN 2.22507385850720138309e-308
#endif

#ifdef __DBL_NORM_MAX__
#define __SPRT_DBL_NORM_MAX __DBL_NORM_MAX__
#else
#define __SPRT_DBL_NORM_MAX 1.7976931348623157e+308
#endif

#define __SPRT_DBL_TRUE_MIN __SPRT_DBL_DENORM_MIN
// clang-format on

#endif // CORE_RUNTIME_INCLUDE_C_BITS___SPRT_FLOAT16_H_
