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

#ifndef CORE_RUNTIME_INCLUDE_LIBC_FLOAT_H_
#define CORE_RUNTIME_INCLUDE_LIBC_FLOAT_H_

/*
	Dispatch header for <float.h>:
	- hosted SPRT build -> forwards to the system <float.h> (#include_next)
	- otherwise         -> SPRT's own definitions via sprt/wrappers/libc/float.h

	This header declares only macros (no types or functions).

	Floating-point characteristics, defined for each of the three real types with the
	prefix FLT_ (float), DBL_ (double) and LDBL_ (long double):
	  <P>_MANT_DIG     - mantissa digits in the type's radix
	  <P>_DIG          - decimal digits guaranteed to round-trip
	  <P>_DECIMAL_DIG  - decimal digits needed to represent the type exactly
	  <P>_EPSILON      - difference between 1 and the next representable value
	  <P>_MIN / <P>_MAX        - smallest/largest normalized magnitudes
	  <P>_NORM_MAX             - largest finite normalized value
	  <P>_MIN_EXP / <P>_MAX_EXP        - radix-exponent range of normalized values
	  <P>_MIN_10_EXP / <P>_MAX_10_EXP  - decimal-exponent range
	  <P>_DENORM_MIN / <P>_TRUE_MIN    - smallest (subnormal) positive value
	  <P>_HAS_DENORM / <P>_HAS_SUBNORM - whether subnormals are supported
	  <P>_HAS_INFINITY / <P>_HAS_QUIET_NAN - whether inf / quiet NaN exist

	Classification constants (also exposed via <math.h>), defined here if not already:
	  FP_NAN, FP_INFINITE, FP_ZERO, FP_SUBNORMAL, FP_NORMAL

	Note: the type-shared macros FLT_RADIX, FLT_ROUNDS, FLT_EVAL_METHOD and DECIMAL_DIG
	are not defined here.
*/

#if defined(__SPRT_BUILD) && __STDC_HOSTED__ == 1

#include_next <float.h>

#else

#include <sprt/wrappers/libc/float.h>

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_FLOAT_H_
