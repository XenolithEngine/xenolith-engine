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

// MSVC's <float.h> also declares the floating-point control/status CRT helpers
// (_clearfp/_statusfp/_controlfp). UCRT normally provides them, but the SPRT
// runtime has no UCRT — so we synthesise them on top of the POSIX <fenv.h> API
// that the runtime does implement. LLVM (e.g. llvm-exegesis) uses _clearfp() to
// reset the x87/SSE status word; other code may probe/adjust rounding via
// _controlfp(). Everything below is header-only (static inline) so it needs no
// CRT object to link against.
#if defined(_WIN32) || defined(__SPRT_WINDOWS)

#include <fenv.h>

// MSVC status-word (_SW_*) and control-word (_EM_*/_RC_*/_MCW_*) bit layout.
#ifndef _SW_INEXACT
#define _SW_INEXACT    0x00000001u // inexact (precision)
#define _SW_UNDERFLOW  0x00000002u // underflow
#define _SW_OVERFLOW   0x00000004u // overflow
#define _SW_ZERODIVIDE 0x00000008u // zero divide
#define _SW_INVALID    0x00000010u // invalid
#define _SW_DENORMAL   0x00080000u // denormal status bit
#endif

#ifndef _MCW_EM
#define _EM_INEXACT    0x00000001u
#define _EM_UNDERFLOW  0x00000002u
#define _EM_OVERFLOW   0x00000004u
#define _EM_ZERODIVIDE 0x00000008u
#define _EM_INVALID    0x00000010u
#define _EM_DENORMAL   0x00080000u
#define _MCW_EM        0x0008001fu // interrupt exception masks

#define _RC_NEAR       0x00000000u
#define _RC_DOWN       0x00000100u
#define _RC_UP         0x00000200u
#define _RC_CHOP       0x00000300u
#define _MCW_RC        0x00000300u // rounding control
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Map hardware fenv exception bits (FE_*) -> MSVC status-word bits (_SW_*).
// FE_DENORMAL is x86-only and not guaranteed by <fenv.h>, so guard it.
static inline unsigned int __sprt_fe_to_sw(int __fe) {
	unsigned int __sw = 0u;
#ifdef FE_INEXACT
	if (__fe & FE_INEXACT)   __sw |= _SW_INEXACT;
#endif
#ifdef FE_UNDERFLOW
	if (__fe & FE_UNDERFLOW) __sw |= _SW_UNDERFLOW;
#endif
#ifdef FE_OVERFLOW
	if (__fe & FE_OVERFLOW)  __sw |= _SW_OVERFLOW;
#endif
#ifdef FE_DIVBYZERO
	if (__fe & FE_DIVBYZERO) __sw |= _SW_ZERODIVIDE;
#endif
#ifdef FE_INVALID
	if (__fe & FE_INVALID)   __sw |= _SW_INVALID;
#endif
#ifdef FE_DENORMAL
	if (__fe & FE_DENORMAL)  __sw |= _SW_DENORMAL;
#endif
	return __sw;
}

// _statusfp(): report the currently-raised FP exceptions as an MSVC status word.
static inline unsigned int _statusfp(void) {
	return __sprt_fe_to_sw(fetestexcept(FE_ALL_EXCEPT));
}

// _clearfp(): clear all FP exception flags, returning the prior status word.
static inline unsigned int _clearfp(void) {
	unsigned int __sw = __sprt_fe_to_sw(fetestexcept(FE_ALL_EXCEPT));
	feclearexcept(FE_ALL_EXCEPT);
	return __sw;
}

// _controlfp(new, mask): read/adjust the FP control word. POSIX <fenv.h> only
// exposes the rounding direction portably, so we honour _MCW_RC via
// fe{get,set}round() and report the exception masks as "all masked" (the CRT
// default). Returns the resulting control word.
static inline unsigned int _controlfp(unsigned int __new, unsigned int __mask) {
	unsigned int __cw = _MCW_EM; // default: every exception masked

	if (__mask & _MCW_RC) {
		unsigned int __rc = __new & _MCW_RC;
		int __round = FE_TONEAREST;
		if (__rc == _RC_DOWN)      __round = FE_DOWNWARD;
		else if (__rc == _RC_UP)   __round = FE_UPWARD;
		else if (__rc == _RC_CHOP) __round = FE_TOWARDZERO;
		fesetround(__round);
	}

	switch (fegetround()) {
	case FE_DOWNWARD:   __cw |= _RC_DOWN; break;
	case FE_UPWARD:     __cw |= _RC_UP;   break;
	case FE_TOWARDZERO: __cw |= _RC_CHOP; break;
	default:            __cw |= _RC_NEAR; break;
	}
	return __cw;
}

#ifdef __cplusplus
}
#endif
#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_FLOAT_H_
