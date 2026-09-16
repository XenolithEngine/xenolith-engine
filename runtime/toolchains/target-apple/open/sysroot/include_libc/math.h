/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

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

/*
	Pseudo-system <math.h> for the Xcode-SDK-free macOS target (*-apple-macosx+open).

	macOS ships no open-source <math.h> (Apple's Libm has been frozen since 2011),
	so this header reconstructs the C99 surface: the value macros / types / builtin
	classification macros are defined here (matching the SDK's ABI), while the
	function prototypes come from the umbrella subunit "math_impl.h", which the
	sysroot install (open-sysroot.mk) copies in beside this header. That keeps the
	sysroot self-contained: <math.h> resolves with no dependency on the sprt/
	include namespace. (Single source of truth: the runtime's own
	sprt/wrappers/libc/math_impl.h, also used by the SPRT wrapper.) The symbols
	resolve from libSystem's libm at link time (the same way the SDK's own <math.h>
	does); the generated .tbd stubs carry them.

	Consumed only by external code compiled against this sysroot; the runtime's own
	sources see the SPRT wrapper via include_libc instead.
*/

#ifndef _MATH_H_
#define _MATH_H_

/* --- special values (compiler builtins; ABI-identical to libm) --- */
#define HUGE_VAL   __builtin_huge_val()
#define HUGE_VALF  __builtin_huge_valf()
#define HUGE_VALL  __builtin_huge_vall()
#define INFINITY   __builtin_inff()
#define NAN        __builtin_nanf("")

/* --- fpclassify() result codes (macOS values) --- */
#define FP_NAN       1
#define FP_INFINITE  2
#define FP_ZERO      3
#define FP_NORMAL    4
#define FP_SUBNORMAL 5
#define FP_ILOGB0    (-2'147'483'647 - 1)
#define FP_ILOGBNAN  (-2'147'483'647 - 1)

/* --- error handling --- */
#define MATH_ERRNO       1
#define MATH_ERREXCEPT   2
#define math_errhandling (MATH_ERRNO | MATH_ERREXCEPT)

/* --- common constants --- */
#define M_E        2.71828182845904523536028747135266250
#define M_LOG2E    1.44269504088896340735992468100189214
#define M_LOG10E   0.434294481903251827651128918916605082
#define M_LN2      0.693147180559945309417232121458176568
#define M_LN10     2.30258509299404568401799145468436421
#define M_PI       3.14159265358979323846264338327950288
#define M_PI_2     1.57079632679489661923132169163975144
#define M_PI_4     0.785398163397448309615660845819875721
#define M_1_PI     0.318309886183790671537767526745028724
#define M_2_PI     0.636619772367581343075535053490057448
#define M_2_SQRTPI 1.12837916709551257389615890312154517
#define M_SQRT2    1.41421356237309504880168872420969808
#define M_SQRT1_2  0.707106781186547524400844362104849039

/* --- the most efficient types at least as wide as float / double --- */
typedef float float_t;
typedef double double_t;

/* --- C99 function surface, sourced from the shared umbrella list --- */
#ifdef __cplusplus
extern "C" {
#endif

#define SPRT_FUNC_BEGIN extern
#define SPRT_FUNC_END ;
#define SPRT_FUNC_BODY 0
#include "math_impl.h"
#undef SPRT_FUNC_BEGIN
#undef SPRT_FUNC_END
#undef SPRT_FUNC_BODY

#ifdef __cplusplus
}
#endif

/* --- classification & comparison (compiler builtins) --- */
#define fpclassify(x) \
	__builtin_fpclassify(FP_NAN, FP_INFINITE, FP_NORMAL, FP_SUBNORMAL, FP_ZERO, (x))
#define isfinite(x)   __builtin_isfinite(x)
#define isinf(x)      __builtin_isinf(x)
#define isnan(x)      __builtin_isnan(x)
#define isnormal(x)   __builtin_isnormal(x)
#define signbit(x)    __builtin_signbit(x)

#define isgreater(x, y)      __builtin_isgreater(x, y)
#define isgreaterequal(x, y) __builtin_isgreaterequal(x, y)
#define isless(x, y)         __builtin_isless(x, y)
#define islessequal(x, y)    __builtin_islessequal(x, y)
#define islessgreater(x, y)  __builtin_islessgreater(x, y)
#define isunordered(x, y)    __builtin_isunordered(x, y)

/* macOS SDK-internal classification helpers referenced directly by the runtime's
   own Apple libc wrapper (SPRuntimeCMath.cpp, SPRT_APPLE branch). Defined via
   builtins so no libSystem symbol is needed. */
static __inline__ __attribute__((__unused__)) int __fpclassifyf(float __x) {
	return __builtin_fpclassify(FP_NAN, FP_INFINITE, FP_NORMAL, FP_SUBNORMAL, FP_ZERO, __x);
}
static __inline__ __attribute__((__unused__)) int __fpclassifyd(double __x) {
	return __builtin_fpclassify(FP_NAN, FP_INFINITE, FP_NORMAL, FP_SUBNORMAL, FP_ZERO, __x);
}
static __inline__ __attribute__((__unused__)) int __fpclassifyl(long double __x) {
	return __builtin_fpclassify(FP_NAN, FP_INFINITE, FP_NORMAL, FP_SUBNORMAL, FP_ZERO, __x);
}
static __inline__ __attribute__((__unused__)) int __inline_signbitf(float __x) {
	return __builtin_signbitf(__x);
}
static __inline__ __attribute__((__unused__)) int __inline_signbitd(double __x) {
	return __builtin_signbit(__x);
}
static __inline__ __attribute__((__unused__)) int __inline_signbitl(long double __x) {
	return __builtin_signbitl(__x);
}

#endif /* _MATH_H_ */
