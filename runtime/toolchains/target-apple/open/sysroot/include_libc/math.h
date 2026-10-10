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
#include <__sprt_abi/math.h>

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
