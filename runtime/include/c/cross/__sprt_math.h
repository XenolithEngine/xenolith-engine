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

#ifndef CORE_RUNTIME_INCLUDE_C_CROSS___SPRT_MATH_H_
#define CORE_RUNTIME_INCLUDE_C_CROSS___SPRT_MATH_H_

#include <c/bits/__sprt_def.h>
#include <c/bits/__sprt_int.h>

#define __SPRT_MATH_ERRNO	1	/* errno set by math functions.  */
#define __SPRT_MATH_ERREXCEPT	2	/* Exceptions raised by math functions.  */

// clang-format off
#if SPRT_LINUX

#define __SPRT_FP_ILOGBNAN (-1-0x7fffffff)
#define __SPRT_FP_ILOGB0 __SPRT_FP_ILOGBNAN

#ifdef __FAST_MATH__
#define __SPRT_math_errhandling	0
#elif defined __NO_MATH_ERRNO__
#define __SPRT_math_errhandling	(__SPRT_MATH_ERREXCEPT)
#else
#define __SPRT_math_errhandling	(__SPRT_MATH_ERRNO | __SPRT_MATH_ERREXCEPT)
#endif

#define __SPRT_FP_NAN       0
#define __SPRT_FP_INFINITE  1
#define __SPRT_FP_ZERO      2
#define __SPRT_FP_SUBNORMAL 3
#define __SPRT_FP_NORMAL    4

#elif SPRT_WINDOWS

#define __SPRT__C2          1  // 0 if not 2's complement
#define __SPRT_FP_ILOGB0   (-0x7fffffff - __SPRT__C2)
#define __SPRT_FP_ILOGBNAN 0x7fffffff

#define __SPRT_math_errhandling  (__SPRT_MATH_ERRNO | __SPRT_MATH_ERREXCEPT)

#define __SPRT_FP_INFINITE  1
#define __SPRT_FP_NAN       2
#define __SPRT_FP_NORMAL    (-1)
#define __SPRT_FP_SUBNORMAL (-2)
#define __SPRT_FP_ZERO      0

#elif SPRT_ANDROID

#define __SPRT_FP_ILOGB0 (-__SPRT_INT_MAX)
#define __SPRT_FP_ILOGBNAN __SPRT_INT_MAX

#define __SPRT_math_errhandling  __SPRT_MATH_ERREXCEPT

#define __SPRT_FP_INFINITE 0x01
#define __SPRT_FP_NAN 0x02
#define __SPRT_FP_NORMAL 0x04
#define __SPRT_FP_SUBNORMAL 0x08
#define __SPRT_FP_ZERO 0x10

#elif SPRT_MACOS

#error "Unknown OS"

#else
#error "Unknown OS"
#endif
// clang-format on

#endif
