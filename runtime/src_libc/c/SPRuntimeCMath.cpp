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

extern "C" {

double acos(double);
float acosf(float);
long double acosl(long double);

double acosh(double);
float acoshf(float);
long double acoshl(long double);

double asin(double);
float asinf(float);
long double asinl(long double);

double asinh(double);
float asinhf(float);
long double asinhl(long double);

double atan(double);
float atanf(float);
long double atanl(long double);

double atan2(double, double);
float atan2f(float, float);
long double atan2l(long double, long double);

double atanh(double);
float atanhf(float);
long double atanhl(long double);

double cbrt(double);
float cbrtf(float);
long double cbrtl(long double);

double ceil(double);
float ceilf(float);
long double ceill(long double);

double copysign(double, double);
float copysignf(float, float);
long double copysignl(long double, long double);

double cos(double);
float cosf(float);
long double cosl(long double);

double cosh(double);
float coshf(float);
long double coshl(long double);

double erf(double);
float erff(float);
long double erfl(long double);

double erfc(double);
float erfcf(float);
long double erfcl(long double);

double exp(double);
float expf(float);
long double expl(long double);

double exp2(double);
float exp2f(float);
long double exp2l(long double);

double expm1(double);
float expm1f(float);
long double expm1l(long double);

double fabs(double);
float fabsf(float);
long double fabsl(long double);

double fdim(double, double);
float fdimf(float, float);
long double fdiml(long double, long double);

double floor(double);
float floorf(float);
long double floorl(long double);

double fma(double, double, double);
float fmaf(float, float, float);
long double fmal(long double, long double, long double);

double fmax(double, double);
float fmaxf(float, float);
long double fmaxl(long double, long double);

double fmin(double, double);
float fminf(float, float);
long double fminl(long double, long double);

double fmod(double, double);
float fmodf(float, float);
long double fmodl(long double, long double);

double frexp(double, int *);
float frexpf(float, int *);
long double frexpl(long double, int *);

double hypot(double, double);
float hypotf(float, float);
long double hypotl(long double, long double);

int ilogb(double);
int ilogbf(float);
int ilogbl(long double);

double ldexp(double, int);
float ldexpf(float, int);
long double ldexpl(long double, int);

double lgamma(double);
float lgammaf(float);
long double lgammal(long double);

long long llrint(double);
long long llrintf(float);
long long llrintl(long double);

long long llround(double);
long long llroundf(float);
long long llroundl(long double);

double log(double);
float logf(float);
long double logl(long double);

double log10(double);
float log10f(float);
long double log10l(long double);

double log1p(double);
float log1pf(float);
long double log1pl(long double);

double log2(double);
float log2f(float);
long double log2l(long double);

double logb(double);
float logbf(float);
long double logbl(long double);

long lrint(double);
long lrintf(float);
long lrintl(long double);

long lround(double);
long lroundf(float);
long lroundl(long double);

double modf(double, double *);
float modff(float, float *);
long double modfl(long double, long double *);

double nan(const char *);
float nanf(const char *);
long double nanl(const char *);

double nearbyint(double);
float nearbyintf(float);
long double nearbyintl(long double);

double nextafter(double, double);
float nextafterf(float, float);
long double nextafterl(long double, long double);

double nexttoward(double, long double);
float nexttowardf(float, long double);
long double nexttowardl(long double, long double);

double pow(double, double);
float powf(float, float);
long double powl(long double, long double);

double remainder(double, double);
float remainderf(float, float);
long double remainderl(long double, long double);

double remquo(double, double, int *);
float remquof(float, float, int *);
long double remquol(long double, long double, int *);

double rint(double);
float rintf(float);
long double rintl(long double);

double round(double);
float roundf(float);
long double roundl(long double);

double scalbln(double, long);
float scalblnf(float, long);
long double scalblnl(long double, long);

double scalbn(double, int);
float scalbnf(float, int);
long double scalbnl(long double, int);

double sin(double);
float sinf(float);
long double sinl(long double);

double sinh(double);
float sinhf(float);
long double sinhl(long double);

double sqrt(double);
float sqrtf(float);
long double sqrtl(long double);

double tan(double);
float tanf(float);
long double tanl(long double);

double tanh(double);
float tanhf(float);
long double tanhl(long double);

double tgamma(double);
float tgammaf(float);
long double tgammal(long double);

double trunc(double);
float truncf(float);
long double truncl(long double);

int __fpclassify(double);
int __fpclassifyf(float);
int __fpclassifyl(long double);

int __signbit(double);
int __signbitf(float);
int __signbitl(long double);
}

#else

#include <math.h>

static_assert(MATH_ERRNO == __SPRT_MATH_ERRNO);
static_assert(MATH_ERREXCEPT == __SPRT_MATH_ERREXCEPT);

#if !defined(SPRT_MACOS)
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

#endif

namespace sprt {

__SPRT_C_FUNC int __SPRT_ID(__fpclassify)(double v) {
#if SPRT_MACOS
	return ::__fpclassifyd(v);
#elif SPRT_WINDOWS
	return ::__fpclassify(v);
#else
	return ::fpclassify(v);
#endif
}
__SPRT_C_FUNC int __SPRT_ID(__fpclassifyf)(float v) {
#if SPRT_MACOS || SPRT_WINDOWS
	return ::__fpclassifyf(v);
#else
	return ::fpclassify(v);
#endif
}
__SPRT_C_FUNC int __SPRT_ID(__fpclassifyl)(long double v) {
#if SPRT_MACOS || SPRT_WINDOWS
	return ::__fpclassifyl(v);
#else
	return ::fpclassify(v);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(__signbit)(double v) {
#if SPRT_MACOS
	return ::__inline_signbitd(v);
#elif SPRT_WINDOWS
	return ::__signbit(v);
#else
	return ::signbit(v);
#endif
}
__SPRT_C_FUNC int __SPRT_ID(__signbitf)(float v) {
#if SPRT_MACOS
	return ::__inline_signbitf(v);
#elif SPRT_WINDOWS
	return ::__signbitf(v);
#else
	return ::signbit(v);
#endif
}
__SPRT_C_FUNC int __SPRT_ID(__signbitl)(long double v) {
#if SPRT_MACOS
	return ::__inline_signbitl(v);
#elif SPRT_WINDOWS
	return ::__signbitl(v);
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


__SPRT_C_FUNC double __SPRT_ID(log2_impl)(double value) { return ::log2(value); }

__SPRT_C_FUNC float __SPRT_ID(log2f_impl)(float value) { return ::log2f(value); }

__SPRT_C_FUNC long double __SPRT_ID(log2l_impl)(long double value) { return ::log2l(value); }


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
