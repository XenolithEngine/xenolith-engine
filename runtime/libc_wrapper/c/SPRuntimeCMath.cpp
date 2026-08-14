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

#define __SPRT_BUILD 1

#include <sprt/c/__sprt_math.h>
#include <sprt/c/__sprt_string.h>
#include <sprt/c/__sprt_stdio.h>

#if __STDC_HOSTED__ == 0
#include "math.h"
#else

#include <math.h>

#if SPRT_EMBOX
// Embox's math.h is a stub: a handful of libm symbols plus builtins for the
// rest of trig/exp. C99 leftovers (acosh, erf, fma, …) are not declared; route
// them through clang builtins so the wrapper compiles without a second libm.
#ifndef acosh
#define acosh(x) __builtin_acosh(x)
#define acoshf(x) __builtin_acoshf(x)
#define acoshl(x) __builtin_acoshl(x)
#endif
#ifndef asinh
#define asinh(x) __builtin_asinh(x)
#define asinhf(x) __builtin_asinhf(x)
#define asinhl(x) __builtin_asinhl(x)
#endif
#ifndef erf
#define erf(x) __builtin_erf(x)
#define erff(x) __builtin_erff(x)
#define erfl(x) __builtin_erfl(x)
#define erfc(x) __builtin_erfc(x)
#define erfcf(x) __builtin_erfcf(x)
#define erfcl(x) __builtin_erfcl(x)
#endif
#ifndef expm1
#define expm1(x) __builtin_expm1(x)
#define expm1f(x) __builtin_expm1f(x)
#define expm1l(x) __builtin_expm1l(x)
#endif
#ifndef fdim
#define fdim(x, y) __builtin_fdim(x, y)
#define fdimf(x, y) __builtin_fdimf(x, y)
#define fdiml(x, y) __builtin_fdiml(x, y)
#endif
#ifndef fma
#define fma(x, y, z) __builtin_fma(x, y, z)
#define fmaf(x, y, z) __builtin_fmaf(x, y, z)
#define fmal(x, y, z) __builtin_fmal(x, y, z)
#endif
#ifndef fmodf
#define fmodf(x, y) __builtin_fmodf(x, y)
#define fmodl(x, y) __builtin_fmodl(x, y)
#endif
#ifndef ilogb
#define ilogb(x) __builtin_ilogb(x)
#define ilogbf(x) __builtin_ilogbf(x)
#define ilogbl(x) __builtin_ilogbl(x)
#endif
#ifndef lgamma
#define lgamma(x) __builtin_lgamma(x)
#define lgammaf(x) __builtin_lgammaf(x)
#define lgammal(x) __builtin_lgammal(x)
#endif
#ifndef log10f
#define log10f(x) __builtin_log10f(x)
#define log10l(x) __builtin_log10l(x)
#endif
#ifndef log1p
#define log1p(x) __builtin_log1p(x)
#define log1pf(x) __builtin_log1pf(x)
#define log1pl(x) __builtin_log1pl(x)
#endif
#ifndef logb
#define logb(x) __builtin_logb(x)
#define logbf(x) __builtin_logbf(x)
#define logbl(x) __builtin_logbl(x)
#endif
#ifndef modff
#define modff(x, ip) __builtin_modff(x, ip)
#define modfl(x, ip) __builtin_modfl(x, ip)
#endif
#ifndef nan
#define nan(s) __builtin_nan(s)
#define nanf(s) __builtin_nanf(s)
#define nanl(s) __builtin_nanl(s)
#endif
#ifndef nearbyint
#define nearbyint(x) __builtin_nearbyint(x)
#define nearbyintf(x) __builtin_nearbyintf(x)
#define nearbyintl(x) __builtin_nearbyintl(x)
#endif
#ifndef nextafter
#define nextafter(x, y) __builtin_nextafter(x, y)
#define nextafterf(x, y) __builtin_nextafterf(x, y)
#define nextafterl(x, y) __builtin_nextafterl(x, y)
#endif
#ifndef nexttoward
#define nexttoward(x, y) __builtin_nexttoward(x, y)
#define nexttowardf(x, y) __builtin_nexttowardf(x, y)
#define nexttowardl(x, y) __builtin_nexttowardl(x, y)
#endif
#ifndef remainder
#define remainder(x, y) __builtin_remainder(x, y)
#define remainderf(x, y) __builtin_remainderf(x, y)
#define remainderl(x, y) __builtin_remainderl(x, y)
#endif
#ifndef remquo
#define remquo(x, y, q) __builtin_remquo(x, y, q)
#define remquof(x, y, q) __builtin_remquof(x, y, q)
#define remquol(x, y, q) __builtin_remquol(x, y, q)
#endif
#ifndef scalbln
#define scalbln(x, n) __builtin_scalbln(x, n)
#define scalblnf(x, n) __builtin_scalblnf(x, n)
#define scalblnl(x, n) __builtin_scalblnl(x, n)
#endif
#ifndef scalbn
#define scalbn(x, n) __builtin_scalbn(x, n)
#define scalbnf(x, n) __builtin_scalbnf(x, n)
#define scalbnl(x, n) __builtin_scalbnl(x, n)
#endif
#ifndef tgamma
#define tgamma(x) __builtin_tgamma(x)
#define tgammaf(x) __builtin_tgammaf(x)
#define tgammal(x) __builtin_tgammal(x)
#endif
#endif // SPRT_EMBOX

// NuttX <math.h> does not define MATH_ERRNO/MATH_ERREXCEPT/math_errhandling and
// uses different FP_/M_ numeric values than the glibc layout sprt pins against.
// Skip the canonical-equality pin block on NuttX; the wrapper re-exports the
// symbols under __sprt_-prefixed names regardless.
#if !SPRT_HOSTED_RTOS
static_assert(MATH_ERRNO == __SPRT_MATH_ERRNO);
static_assert(MATH_ERREXCEPT == __SPRT_MATH_ERREXCEPT);

#if !defined(SPRT_APPLE)
static_assert(math_errhandling == __SPRT_math_errhandling);
#endif

static_assert(FP_NAN == __SPRT_FP_NAN);
static_assert(FP_INFINITE == __SPRT_FP_INFINITE);
static_assert(FP_ZERO == __SPRT_FP_ZERO);
static_assert(FP_SUBNORMAL == __SPRT_FP_SUBNORMAL);
static_assert(FP_NORMAL == __SPRT_FP_NORMAL);

static_assert(M_E == __SPRT_M_E);
static_assert(M_LOG2E == __SPRT_M_LOG2E);
static_assert(M_LOG10E == __SPRT_M_LOG10E);
static_assert(M_LN2 == __SPRT_M_LN2);
static_assert(M_LN10 == __SPRT_M_LN10);
static_assert(M_PI == __SPRT_M_PI);
static_assert(M_PI_2 == __SPRT_M_PI_2);
static_assert(M_PI_4 == __SPRT_M_PI_4);
static_assert(M_1_PI == __SPRT_M_1_PI);
static_assert(M_2_PI == __SPRT_M_2_PI);
static_assert(M_2_SQRTPI == __SPRT_M_2_SQRTPI);
static_assert(M_SQRT2 == __SPRT_M_SQRT2);
static_assert(M_SQRT1_2 == __SPRT_M_SQRT1_2);

//static_assert(NAN == __SPRT_NAN);
static_assert(INFINITY == __SPRT_INFINITY);
static_assert(HUGE_VAL == __SPRT_HUGE_VAL);
static_assert(HUGE_VALF == __SPRT_HUGE_VALF);
static_assert(HUGE_VALL == __SPRT_HUGE_VALL);

#endif // !SPRT_HOSTED_RTOS

#endif

namespace sprt {

__SPRT_C_FUNC int __SPRT_ID(__fpclassify)(double v) {
#if SPRT_APPLE
	return ::__fpclassifyd(v);
#elif SPRT_WINDOWS || SPRT_WASM
	return ::__fpclassify(v);
#elif defined(fpclassify)
	return fpclassify(v);
#else
	return ::fpclassify(v);
#endif
}
__SPRT_C_FUNC int __SPRT_ID(__fpclassifyf)(float v) {
#if SPRT_APPLE || SPRT_WINDOWS || SPRT_WASM
	return ::__fpclassifyf(v);
#elif defined(fpclassify)
	return fpclassify(v);
#else
	return ::fpclassify(v);
#endif
}
__SPRT_C_FUNC int __SPRT_ID(__fpclassifyl)(long double v) {
#if SPRT_APPLE || SPRT_WINDOWS || SPRT_WASM
	return ::__fpclassifyl(v);
#elif defined(fpclassify)
	return fpclassify(v);
#else
	return ::fpclassify(v);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(__signbit)(double v) {
#if SPRT_APPLE
	return ::__inline_signbitd(v);
#elif SPRT_WINDOWS
	return ::__signbit(v);
#elif SPRT_WASM
	return __builtin_signbit(v);
#elif defined(signbit)
	return signbit(v);
#else
	return ::signbit(v);
#endif
}
__SPRT_C_FUNC int __SPRT_ID(__signbitf)(float v) {
#if SPRT_APPLE
	return ::__inline_signbitf(v);
#elif SPRT_WINDOWS
	return ::__signbitf(v);
#elif SPRT_WASM
	return __builtin_signbit(v);
#elif defined(signbit)
	return signbit(v);
#else
	return ::signbit(v);
#endif
}
__SPRT_C_FUNC int __SPRT_ID(__signbitl)(long double v) {
#if SPRT_APPLE
	return ::__inline_signbitl(v);
#elif SPRT_WINDOWS
	return ::__signbitl(v);
#elif SPRT_WASM
	return __builtin_signbit(v);
#elif defined(signbit)
	return signbit(v);
#else
	return ::signbit(v);
#endif
}

__SPRT_C_FUNC double __SPRT_ID(acos_impl)(double value) { return ::acos(value); }

__SPRT_C_FUNC float __SPRT_ID(acosf_impl)(float value) { return ::acosf(value); }

__SPRT_C_FUNC long double __SPRT_ID(acosl_impl)(long double value) { return ::acosl(value); }


__SPRT_C_FUNC double __SPRT_ID(acosh_impl)(double value) { return ::acosh(value); }

__SPRT_C_FUNC float __SPRT_ID(acoshf_impl)(float value) { return ::acoshf(value); }

__SPRT_C_FUNC long double __SPRT_ID(acoshl_impl)(long double value) { return ::acoshl(value); }


__SPRT_C_FUNC double __SPRT_ID(asin_impl)(double value) { return ::asin(value); }

__SPRT_C_FUNC float __SPRT_ID(asinf_impl)(float value) { return ::asinf(value); }

__SPRT_C_FUNC long double __SPRT_ID(asinl_impl)(long double value) { return ::asinl(value); }

__SPRT_C_FUNC double __SPRT_ID(asinh_impl)(double value) { return ::asinh(value); }

__SPRT_C_FUNC float __SPRT_ID(asinhf_impl)(float value) { return ::asinhf(value); }

__SPRT_C_FUNC long double __SPRT_ID(asinhl_impl)(long double value) { return ::asinhl(value); }


__SPRT_C_FUNC double __SPRT_ID(atan_impl)(double value) { return ::atan(value); }

__SPRT_C_FUNC float __SPRT_ID(atanf_impl)(float value) { return ::atanf(value); }

__SPRT_C_FUNC long double __SPRT_ID(atanl_impl)(long double value) { return ::atanl(value); }


__SPRT_C_FUNC double __SPRT_ID(atan2_impl)(double a, double b) { return ::atan2(a, b); }

__SPRT_C_FUNC float __SPRT_ID(atan2f_impl)(float a, float b) { return ::atan2f(a, b); }

__SPRT_C_FUNC long double __SPRT_ID(atan2l_impl)(long double a, long double b) {
	return ::atan2l(a, b);
}


__SPRT_C_FUNC double __SPRT_ID(atanh_impl)(double value) { return ::atanh(value); }

__SPRT_C_FUNC float __SPRT_ID(atanhf_impl)(float value) { return ::atanhf(value); }

__SPRT_C_FUNC long double __SPRT_ID(atanhl_impl)(long double value) { return ::atanhl(value); }


__SPRT_C_FUNC double __SPRT_ID(cbrt_impl)(double value) { return ::cbrt(value); }

__SPRT_C_FUNC float __SPRT_ID(cbrtf_impl)(float value) { return ::cbrtf(value); }

__SPRT_C_FUNC long double __SPRT_ID(cbrtl_impl)(long double value) { return ::cbrtl(value); }


__SPRT_C_FUNC double __SPRT_ID(ceil_impl)(double value) { return ::ceil(value); }

__SPRT_C_FUNC float __SPRT_ID(ceilf_impl)(float value) { return ::ceilf(value); }

__SPRT_C_FUNC long double __SPRT_ID(ceill_impl)(long double value) { return ::ceill(value); }


__SPRT_C_FUNC double __SPRT_ID(copysign_impl)(double a, double b) { return ::copysign(a, b); }

__SPRT_C_FUNC float __SPRT_ID(copysignf_impl)(float a, float b) { return ::copysignf(a, b); }

__SPRT_C_FUNC long double __SPRT_ID(copysignl_impl)(long double a, long double b) {
	return ::copysignl(a, b);
}


__SPRT_C_FUNC double __SPRT_ID(cos_impl)(double value) { return ::cos(value); }

__SPRT_C_FUNC float __SPRT_ID(cosf_impl)(float value) { return ::cosf(value); }

__SPRT_C_FUNC long double __SPRT_ID(cosl_impl)(long double value) { return ::cosl(value); }


__SPRT_C_FUNC double __SPRT_ID(cosh_impl)(double value) { return ::cosh(value); }

__SPRT_C_FUNC float __SPRT_ID(coshf_impl)(float value) { return ::coshf(value); }

__SPRT_C_FUNC long double __SPRT_ID(coshl_impl)(long double value) { return ::coshl(value); }


__SPRT_C_FUNC double __SPRT_ID(erf_impl)(double value) { return ::erf(value); }

__SPRT_C_FUNC float __SPRT_ID(erff_impl)(float value) { return ::erff(value); }

__SPRT_C_FUNC long double __SPRT_ID(erfl_impl)(long double value) { return ::erfl(value); }


__SPRT_C_FUNC double __SPRT_ID(erfc_impl)(double value) { return ::erfc(value); }

__SPRT_C_FUNC float __SPRT_ID(erfcf_impl)(float value) { return ::erfcf(value); }

__SPRT_C_FUNC long double __SPRT_ID(erfcl_impl)(long double value) { return ::erfcl(value); }


__SPRT_C_FUNC double __SPRT_ID(exp_impl)(double value) { return ::exp(value); }

__SPRT_C_FUNC float __SPRT_ID(expf_impl)(float value) { return ::expf(value); }

__SPRT_C_FUNC long double __SPRT_ID(expl_impl)(long double value) { return ::expl(value); }


__SPRT_C_FUNC double __SPRT_ID(exp2_impl)(double value) { return ::exp2(value); }

__SPRT_C_FUNC float __SPRT_ID(exp2f_impl)(float value) { return ::exp2f(value); }

__SPRT_C_FUNC long double __SPRT_ID(exp2l_impl)(long double value) { return ::exp2l(value); }


__SPRT_C_FUNC double __SPRT_ID(expm1_impl)(double value) { return ::expm1(value); }

__SPRT_C_FUNC float __SPRT_ID(expm1f_impl)(float value) { return ::expm1f(value); }

__SPRT_C_FUNC long double __SPRT_ID(expm1l_impl)(long double value) { return ::expm1l(value); }


__SPRT_C_FUNC double __SPRT_ID(fabs_impl)(double value) { return ::fabs(value); }

__SPRT_C_FUNC float __SPRT_ID(fabsf_impl)(float value) { return ::fabsf(value); }

__SPRT_C_FUNC long double __SPRT_ID(fabsl_impl)(long double value) { return ::fabsl(value); }


__SPRT_C_FUNC double __SPRT_ID(fdim_impl)(double a, double b) { return ::fdim(a, b); }

__SPRT_C_FUNC float __SPRT_ID(fdimf_impl)(float a, float b) { return ::fdimf(a, b); }

__SPRT_C_FUNC long double __SPRT_ID(fdiml_impl)(long double a, long double b) {
	return ::fdiml(a, b);
}


__SPRT_C_FUNC double __SPRT_ID(floor_impl)(double value) { return ::floor(value); }

__SPRT_C_FUNC float __SPRT_ID(floorf_impl)(float value) { return ::floorf(value); }

__SPRT_C_FUNC long double __SPRT_ID(floorl_impl)(long double value) { return ::floorl(value); }


__SPRT_C_FUNC double __SPRT_ID(fma_impl)(double a, double b, double c) { return ::fma(a, b, c); }

__SPRT_C_FUNC float __SPRT_ID(fmaf_impl)(float a, float b, float c) { return ::fmaf(a, b, c); }

__SPRT_C_FUNC long double __SPRT_ID(fmal_impl)(long double a, long double b, long double c) {
	return ::fmal(a, b, c);
}


__SPRT_C_FUNC double __SPRT_ID(fmax_impl)(double a, double b) { return ::fmax(a, b); }

__SPRT_C_FUNC float __SPRT_ID(fmaxf_impl)(float a, float b) { return ::fmaxf(a, b); }

__SPRT_C_FUNC long double __SPRT_ID(fmaxl_impl)(long double a, long double b) {
	return ::fmaxl(a, b);
}


__SPRT_C_FUNC double __SPRT_ID(fmin_impl)(double a, double b) { return ::fmin(a, b); }

__SPRT_C_FUNC float __SPRT_ID(fminf_impl)(float a, float b) { return ::fminf(a, b); }

__SPRT_C_FUNC long double __SPRT_ID(fminl_impl)(long double a, long double b) {
	return ::fminl(a, b);
}


__SPRT_C_FUNC double __SPRT_ID(fmod_impl)(double a, double b) { return ::fmod(a, b); }

__SPRT_C_FUNC float __SPRT_ID(fmodf_impl)(float a, float b) { return ::fmodf(a, b); }

__SPRT_C_FUNC long double __SPRT_ID(fmodl_impl)(long double a, long double b) {
	return ::fmodl(a, b);
}


__SPRT_C_FUNC double __SPRT_ID(frexp_impl)(double a, int *b) { return ::frexp(a, b); }

__SPRT_C_FUNC float __SPRT_ID(frexpf_impl)(float a, int *b) { return ::frexpf(a, b); }

__SPRT_C_FUNC long double __SPRT_ID(frexpl_impl)(long double a, int *b) { return ::frexpl(a, b); }


__SPRT_C_FUNC double __SPRT_ID(hypot_impl)(double a, double b) { return ::hypot(a, b); }

__SPRT_C_FUNC float __SPRT_ID(hypotf_impl)(float a, float b) { return ::hypotf(a, b); }

__SPRT_C_FUNC long double __SPRT_ID(hypotl_impl)(long double a, long double b) {
	return ::hypotl(a, b);
}


__SPRT_C_FUNC int __SPRT_ID(ilogb_impl)(double value) { return ::ilogb(value); }

__SPRT_C_FUNC int __SPRT_ID(ilogbf_impl)(float value) { return ::ilogbf(value); }

__SPRT_C_FUNC int __SPRT_ID(ilogbl_impl)(long double value) { return ::ilogbl(value); }


__SPRT_C_FUNC double __SPRT_ID(ldexp_impl)(double a, int b) { return ::ldexp(a, b); }

__SPRT_C_FUNC float __SPRT_ID(ldexpf_impl)(float a, int b) { return ::ldexpf(a, b); }

__SPRT_C_FUNC long double __SPRT_ID(ldexpl_impl)(long double a, int b) { return ::ldexpl(a, b); }


__SPRT_C_FUNC double __SPRT_ID(lgamma_impl)(double value) { return ::lgamma(value); }

__SPRT_C_FUNC float __SPRT_ID(lgammaf_impl)(float value) { return ::lgammaf(value); }

__SPRT_C_FUNC long double __SPRT_ID(lgammal_impl)(long double value) { return ::lgammal(value); }


__SPRT_C_FUNC long long __SPRT_ID(llrint_impl)(double value) { return ::llrint(value); }

__SPRT_C_FUNC long long __SPRT_ID(llrintf_impl)(float value) { return ::llrintf(value); }

__SPRT_C_FUNC long long __SPRT_ID(llrintl_impl)(long double value) { return ::llrintl(value); }


__SPRT_C_FUNC long long __SPRT_ID(llround_impl)(double value) { return ::llround(value); }

__SPRT_C_FUNC long long __SPRT_ID(llroundf_impl)(float value) { return ::llroundf(value); }

__SPRT_C_FUNC long long __SPRT_ID(llroundl_impl)(long double value) { return ::llroundl(value); }


__SPRT_C_FUNC double __SPRT_ID(log_impl)(double value) { return ::log(value); }

__SPRT_C_FUNC float __SPRT_ID(logf_impl)(float value) { return ::logf(value); }

__SPRT_C_FUNC long double __SPRT_ID(logl_impl)(long double value) { return ::logl(value); }


__SPRT_C_FUNC double __SPRT_ID(log10_impl)(double value) { return ::log10(value); }

__SPRT_C_FUNC float __SPRT_ID(log10f_impl)(float value) { return ::log10f(value); }

__SPRT_C_FUNC long double __SPRT_ID(log10l_impl)(long double value) { return ::log10l(value); }


__SPRT_C_FUNC double __SPRT_ID(log1p_impl)(double value) { return ::log1p(value); }

__SPRT_C_FUNC float __SPRT_ID(log1pf_impl)(float value) { return ::log1pf(value); }

__SPRT_C_FUNC long double __SPRT_ID(log1pl_impl)(long double value) { return ::log1pl(value); }


__SPRT_C_FUNC double __SPRT_ID(logb_impl)(double value) { return ::logb(value); }

__SPRT_C_FUNC float __SPRT_ID(logbf_impl)(float value) { return ::logbf(value); }

__SPRT_C_FUNC long double __SPRT_ID(logbl_impl)(long double value) { return ::logbl(value); }


__SPRT_C_FUNC long __SPRT_ID(lrint_impl)(double value) { return ::lrint(value); }

__SPRT_C_FUNC long __SPRT_ID(lrintf_impl)(float value) { return ::lrintf(value); }

__SPRT_C_FUNC long __SPRT_ID(lrintl_impl)(long double value) { return ::lrintl(value); }


__SPRT_C_FUNC long __SPRT_ID(lround_impl)(double value) { return ::lround(value); }

__SPRT_C_FUNC long __SPRT_ID(lroundf_impl)(float value) { return ::lroundf(value); }

__SPRT_C_FUNC long __SPRT_ID(lroundl_impl)(long double value) { return ::lroundl(value); }


__SPRT_C_FUNC double __SPRT_ID(modf_impl)(double a, double *b) { return ::modf(a, b); }

__SPRT_C_FUNC float __SPRT_ID(modff_impl)(float a, float *b) { return ::modff(a, b); }

__SPRT_C_FUNC long double __SPRT_ID(modfl_impl)(long double a, long double *b) {
	return ::modfl(a, b);
}


__SPRT_C_FUNC double __SPRT_ID(nan_impl)(const char *value) { return ::nan(value); }

__SPRT_C_FUNC float __SPRT_ID(nanf_impl)(const char *value) { return ::nanf(value); }

__SPRT_C_FUNC long double __SPRT_ID(nanl_impl)(const char *value) { return ::nanl(value); }


__SPRT_C_FUNC double __SPRT_ID(nearbyint_impl)(double value) { return ::nearbyint(value); }

__SPRT_C_FUNC float __SPRT_ID(nearbyintf_impl)(float value) { return ::nearbyintf(value); }

__SPRT_C_FUNC long double __SPRT_ID(nearbyintl_impl)(long double value) {
	return ::nearbyintl(value);
}


__SPRT_C_FUNC double __SPRT_ID(nextafter_impl)(double a, double b) { return ::nextafter(a, b); }

__SPRT_C_FUNC float __SPRT_ID(nextafterf_impl)(float a, float b) { return ::nextafterf(a, b); }

__SPRT_C_FUNC long double __SPRT_ID(nextafterl_impl)(long double a, long double b) {
	return ::nextafterl(a, b);
}


__SPRT_C_FUNC double __SPRT_ID(nexttoward_impl)(double a, long double b) {
	return ::nexttoward(a, b);
}

__SPRT_C_FUNC float __SPRT_ID(nexttowardf_impl)(float a, long double b) {
	return ::nexttowardf(a, b);
}

__SPRT_C_FUNC long double __SPRT_ID(nexttowardl_impl)(long double a, long double b) {
	return ::nexttowardl(a, b);
}


__SPRT_C_FUNC double __SPRT_ID(pow_impl)(double a, double b) { return ::pow(a, b); }

__SPRT_C_FUNC float __SPRT_ID(powf_impl)(float a, float b) { return ::powf(a, b); }

__SPRT_C_FUNC long double __SPRT_ID(powl_impl)(long double a, long double b) {
	return ::powl(a, b);
}


__SPRT_C_FUNC double __SPRT_ID(remainder_impl)(double a, double b) { return ::remainder(a, b); }

__SPRT_C_FUNC float __SPRT_ID(remainderf_impl)(float a, float b) { return ::remainderf(a, b); }

__SPRT_C_FUNC long double __SPRT_ID(remainderl_impl)(long double a, long double b) {
	return ::remainderl(a, b);
}


__SPRT_C_FUNC double __SPRT_ID(remquo_impl)(double a, double b, int *c) {
	return ::remquo(a, b, c);
}

__SPRT_C_FUNC float __SPRT_ID(remquof_impl)(float a, float b, int *c) { return ::remquof(a, b, c); }

__SPRT_C_FUNC long double __SPRT_ID(remquol_impl)(long double a, long double b, int *c) {
	return ::remquol(a, b, c);
}


__SPRT_C_FUNC double __SPRT_ID(rint_impl)(double value) { return ::rint(value); }

__SPRT_C_FUNC float __SPRT_ID(rintf_impl)(float value) { return ::rintf(value); }

__SPRT_C_FUNC long double __SPRT_ID(rintl_impl)(long double value) { return ::rintl(value); }


__SPRT_C_FUNC double __SPRT_ID(round_impl)(double value) { return ::round(value); }

__SPRT_C_FUNC float __SPRT_ID(roundf_impl)(float value) { return ::roundf(value); }

__SPRT_C_FUNC long double __SPRT_ID(roundl_impl)(long double value) { return ::roundl(value); }


__SPRT_C_FUNC double __SPRT_ID(scalbln_impl)(double a, long b) { return ::scalbln(a, b); }

__SPRT_C_FUNC float __SPRT_ID(scalblnf_impl)(float a, long b) { return ::scalblnf(a, b); }

__SPRT_C_FUNC long double __SPRT_ID(scalblnl_impl)(long double a, long b) {
	return ::scalblnl(a, b);
}


__SPRT_C_FUNC double __SPRT_ID(scalbn_impl)(double a, int b) { return ::scalbn(a, b); }

__SPRT_C_FUNC float __SPRT_ID(scalbnf_impl)(float a, int b) { return ::scalbnf(a, b); }

__SPRT_C_FUNC long double __SPRT_ID(scalbnl_impl)(long double a, int b) { return ::scalbnl(a, b); }


__SPRT_C_FUNC double __SPRT_ID(sin_impl)(double value) { return ::sin(value); }

__SPRT_C_FUNC float __SPRT_ID(sinf_impl)(float value) { return ::sinf(value); }

__SPRT_C_FUNC long double __SPRT_ID(sinl_impl)(long double value) { return ::sinl(value); }


__SPRT_C_FUNC double __SPRT_ID(sinh_impl)(double value) { return ::sinh(value); }

__SPRT_C_FUNC float __SPRT_ID(sinhf_impl)(float value) { return ::sinhf(value); }

__SPRT_C_FUNC long double __SPRT_ID(sinhl_impl)(long double value) { return ::sinhl(value); }


__SPRT_C_FUNC double __SPRT_ID(sqrt_impl)(double value) { return ::sqrt(value); }

__SPRT_C_FUNC float __SPRT_ID(sqrtf_impl)(float value) { return ::sqrtf(value); }

__SPRT_C_FUNC long double __SPRT_ID(sqrtl_impl)(long double value) { return ::sqrtl(value); }


__SPRT_C_FUNC double __SPRT_ID(tan_impl)(double value) { return ::tan(value); }

__SPRT_C_FUNC float __SPRT_ID(tanf_impl)(float value) { return ::tanf(value); }

__SPRT_C_FUNC long double __SPRT_ID(tanl_impl)(long double value) { return ::tanl(value); }


__SPRT_C_FUNC double __SPRT_ID(tanh_impl)(double value) { return ::tanh(value); }

__SPRT_C_FUNC float __SPRT_ID(tanhf_impl)(float value) { return ::tanhf(value); }

__SPRT_C_FUNC long double __SPRT_ID(tanhl_impl)(long double value) { return ::tanhl(value); }


__SPRT_C_FUNC double __SPRT_ID(tgamma_impl)(double value) { return ::tgamma(value); }

__SPRT_C_FUNC float __SPRT_ID(tgammaf_impl)(float value) { return ::tgammaf(value); }

__SPRT_C_FUNC long double __SPRT_ID(tgammal_impl)(long double value) { return ::tgammal(value); }


__SPRT_C_FUNC double __SPRT_ID(trunc_impl)(double value) { return ::trunc(value); }

__SPRT_C_FUNC float __SPRT_ID(truncf_impl)(float value) { return ::truncf(value); }

__SPRT_C_FUNC long double __SPRT_ID(truncl_impl)(long double value) { return ::truncl(value); }

} // namespace sprt
