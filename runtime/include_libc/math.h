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

#ifndef CORE_RUNTIME_INCLUDE_LIBC_MATH_H_
#define CORE_RUNTIME_INCLUDE_LIBC_MATH_H_

/*
	Dispatch header for <math.h>:
	- hosted SPRT build -> forwards to the system <math.h> (#include_next)
	- otherwise         -> SPRT's own definitions via sprt/wrappers/libc/math.h

	Each real function comes in three precisions: the bare name takes/returns double,
	the f-suffixed name float, and the l-suffixed name long double (e.g. cos/cosf/cosl).
	In C++ the functions instead live in namespace sprt::_cmath (pulled into the global
	scope when hosted-and-not-SPRT-build, and into std:: under __SPRT_AS_STD) as
	type-generic overloads plus arithmetic-promoting templates; the classification and
	comparison entries below are real functions in C++ but macros in C.

	Macros:
	  NAN, INFINITY, HUGE_VAL, HUGE_VALF, HUGE_VALL
	  FP_NAN, FP_INFINITE, FP_ZERO, FP_SUBNORMAL, FP_NORMAL  - fpclassify results
	  FP_ILOGB0, FP_ILOGBNAN                                 - ilogb edge results
	  M_E, M_LOG2E, M_LOG10E, M_LN2, M_LN10, M_PI, M_PI_2, M_PI_4, M_1_PI, M_2_PI,
	  M_2_SQRTPI, M_SQRT2, M_SQRT1_2                         - common constants

	Types:
	  float_t, double_t - the most efficient types at least as wide as float/double

	Trigonometric:        acos, asin, atan, atan2, cos, sin, tan
	Hyperbolic:           acosh, asinh, atanh, cosh, sinh, tanh
	Exponential / log:    exp, exp2, expm1, log, log10, log1p, log2, logb, ilogb,
	                      frexp, ldexp, modf, scalbn, scalbln
	Power / roots:        pow, sqrt, cbrt, hypot (2- and, in C++, 3-argument)
	Error / gamma:        erf, erfc, lgamma, tgamma
	Rounding / integer:   ceil, floor, trunc, round, lround, llround, rint, lrint,
	                      llrint, nearbyint
	Remainder / modulo:   fmod, remainder, remquo
	Sign / next / fma:    fabs, copysign, nan, nextafter, nexttoward, fdim, fmax,
	                      fmin, fma
	Classification:       fpclassify, isfinite, isinf, isnan, isnormal, signbit
	Comparison:           isgreater, isgreaterequal, isless, islessequal,
	                      islessgreater, isunordered
	C++-only additions:   abs overloads (integral and floating), lerp
*/

#if defined(__SPRT_BUILD) && __STDC_HOSTED__ == 1

#include_next <math.h>

#else

#include <sprt/wrappers/libc/math.h>

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_MATH_H_
