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

#ifndef CORE_RUNTIME_INCLUDE_C_BITS___SPRT_LONG_DOUBLE_H_
#define CORE_RUNTIME_INCLUDE_C_BITS___SPRT_LONG_DOUBLE_H_

// clang-format off
#ifdef __LDBL_DECIMAL_DIG__
#define __SPRT_LDBL_DECIMAL_DIG __LDBL_DECIMAL_DIG__
#else
#define __SPRT_LDBL_DECIMAL_DIG 21
#endif

#ifdef __LDBL_DENORM_MIN__
#define __SPRT_LDBL_DENORM_MIN __LDBL_DENORM_MIN__
#else
#define __SPRT_LDBL_DENORM_MIN 3.64519953188247460253e-4951L
#endif

#ifdef __LDBL_DIG__
#define __SPRT_LDBL_DIG __LDBL_DIG__
#else
#define __SPRT_LDBL_DIG 18
#endif

#ifdef __LDBL_EPSILON__
#define __SPRT_LDBL_EPSILON __LDBL_EPSILON__
#else
#define __SPRT_LDBL_EPSILON 1.08420217248550443401e-19L
#endif

#ifdef __LDBL_HAS_DENORM__
#define __SPRT_LDBL_HAS_DENORM __LDBL_HAS_DENORM__
#define __SPRT_LDBL_HAS_SUBNORM __LDBL_HAS_DENORM__
#else
#define __SPRT_LDBL_HAS_DENORM 1
#define __SPRT_LDBL_HAS_SUBNORM 1
#endif

#ifdef __LDBL_HAS_INFINITY__
#define __SPRT_LDBL_HAS_INFINITY __LDBL_HAS_INFINITY__
#else
#define __SPRT_LDBL_HAS_INFINITY 1
#endif

#ifdef __LDBL_HAS_QUIET_NAN__
#define __SPRT_LDBL_HAS_QUIET_NAN __LDBL_HAS_QUIET_NAN__
#else
#define __SPRT_LDBL_HAS_QUIET_NAN 1
#endif

#ifdef __LDBL_MANT_DIG__
#define __SPRT_LDBL_MANT_DIG __LDBL_MANT_DIG__
#else
#define __SPRT_LDBL_MANT_DIG 64
#endif

#ifdef __LDBL_MAX_10_EXP__
#define __SPRT_LDBL_MAX_10_EXP __LDBL_MAX_10_EXP__
#else
#define __SPRT_LDBL_MAX_10_EXP 4932
#endif

#ifdef __LDBL_MAX_EXP__
#define __SPRT_LDBL_MAX_EXP __LDBL_MAX_EXP__
#else
#define __SPRT_LDBL_MAX_EXP 16384
#endif

#ifdef __LDBL_MAX__
#define __SPRT_LDBL_MAX __LDBL_MAX__
#else
#define __SPRT_LDBL_MAX 1.18973149535723176502e+4932L
#endif

#ifdef __LDBL_MIN_10_EXP__
#define __SPRT_LDBL_MIN_10_EXP __LDBL_MIN_10_EXP__
#else
#define __SPRT_LDBL_MIN_10_EXP (-4931)
#endif

#ifdef __LDBL_MIN_EXP__
#define __SPRT_LDBL_MIN_EXP __LDBL_MIN_EXP__
#else
#define __SPRT_LDBL_MIN_EXP (-16381)
#endif

#ifdef __LDBL_MIN__
#define __SPRT_LDBL_MIN __LDBL_MIN__
#else
#define __SPRT_LDBL_MIN 3.36210314311209350626e-4932L
#endif

#ifdef __LDBL_NORM_MAX__
#define __SPRT_LDBL_NORM_MAX __LDBL_NORM_MAX__
#else
#define __SPRT_LDBL_NORM_MAX 1.18973149535723176502e+4932L
#endif

#define __SPRT_LDBL_TRUE_MIN __SPRT_LDBL_DENORM_MIN
// clang-format on

#endif // CORE_RUNTIME_INCLUDE_C_BITS___SPRT_LONG_DOUBLE_H_
