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

#ifdef __SPRT_BUILD

#include_next <math.h>

#else

#include <c/__sprt_math.h>

#ifndef FP_NAN
#define FP_NAN __SPRT_FP_NAN
#define FP_INFINITE __SPRT_FP_INFINITE
#define FP_ZERO __SPRT_FP_ZERO
#define FP_SUBNORMAL __SPRT_FP_SUBNORMAL
#define FP_NORMAL __SPRT_FP_NORMAL
#endif

#ifndef M_E
#define M_E __SPRT_M_E
#define M_LOG2E __SPRT_M_LOG2E
#define M_LOG10E __SPRT_M_LOG10E
#define M_LN2 __SPRT_M_LN2
#define M_LN10 __SPRT_M_LN10
#define M_PI __SPRT_M_PI
#define M_PI_2 __SPRT_M_PI_2
#define M_PI_4 __SPRT_M_PI_4
#define M_1_PI __SPRT_M_1_PI
#define M_2_PI__SPRT_M_2_PI
#define M_2_SQRTPI __SPRT_M_2_SQRTPI
#define M_SQRT2 __SPRT_M_SQRT2
#define M_SQRT1_2 __SPRT_M_SQRT1_2
#endif

#ifndef NAN
#define NAN __SPRT_NAN
#define INFINITY __SPRT_INFINITY
#define HUGE_VAL __SPRT_HUGE_VAL
#define HUGE_VALF __SPRT_HUGE_VALF
#define HUGE_VALL  __SPRT_HUGE_VALL
#endif

__SPRT_BEGIN_DECL

SPRT_FORCEINLINE inline double acos(double value) { return __sprt_acos(value); }

SPRT_FORCEINLINE inline float acosf(float value) { return __sprt_acosf(value); }

SPRT_FORCEINLINE inline long double acosl(long double value) { return __sprt_acosl(value); }


SPRT_FORCEINLINE inline double acosh(double value) { return __sprt_acosh(value); }

SPRT_FORCEINLINE inline float acoshf(float value) { return __sprt_acoshf(value); }

SPRT_FORCEINLINE inline long double acoshl(long double value) { return __sprt_acoshl(value); }


SPRT_FORCEINLINE inline double asin(double value) { return __sprt_asin(value); }

SPRT_FORCEINLINE inline float asinf(float value) { return __sprt_asinf(value); }

SPRT_FORCEINLINE inline long double asinl(long double value) { return __sprt_asinl(value); }


SPRT_FORCEINLINE inline double asinh(double value) { return __sprt_asinh(value); }

SPRT_FORCEINLINE inline float asinhf(float value) { return __sprt_asinhf(value); }

SPRT_FORCEINLINE inline long double asinhl(long double value) { return __sprt_asinhl(value); }


SPRT_FORCEINLINE inline double atan(double value) { return __sprt_atan(value); }

SPRT_FORCEINLINE inline float atanf(float value) { return __sprt_atanf(value); }

SPRT_FORCEINLINE inline long double atanl(long double value) { return __sprt_atanl(value); }


SPRT_FORCEINLINE inline double atan2(double a, double b) { return __sprt_atan2(a, b); }

SPRT_FORCEINLINE inline float atan2f(float a, float b) { return __sprt_atan2f(a, b); }

SPRT_FORCEINLINE inline long double atan2l(long double a, long double b) {
	return __sprt_atan2l(a, b);
}


SPRT_FORCEINLINE inline double atanh(double value) { return __sprt_atanh(value); }

SPRT_FORCEINLINE inline float atanhf(float value) { return __sprt_atanhf(value); }

SPRT_FORCEINLINE inline long double atanhl(long double value) { return __sprt_atanhl(value); }


SPRT_FORCEINLINE inline double cbrt(double value) { return __sprt_cbrt(value); }

SPRT_FORCEINLINE inline float cbrtf(float value) { return __sprt_cbrtf(value); }

SPRT_FORCEINLINE inline long double cbrtl(long double value) { return __sprt_cbrtl(value); }


SPRT_FORCEINLINE inline double ceil(double value) { return __sprt_ceil(value); }

SPRT_FORCEINLINE inline float ceilf(float value) { return __sprt_ceilf(value); }

SPRT_FORCEINLINE inline long double ceill(long double value) { return __sprt_ceill(value); }


SPRT_FORCEINLINE inline double copysign(double a, double b) { return __sprt_copysign(a, b); }

SPRT_FORCEINLINE inline float copysignf(float a, float b) { return __sprt_copysignf(a, b); }

SPRT_FORCEINLINE inline long double copysignl(long double a, long double b) {
	return __sprt_copysignl(a, b);
}


SPRT_FORCEINLINE inline double cos(double value) { return __sprt_cos(value); }

SPRT_FORCEINLINE inline float cosf(float value) { return __sprt_cosf(value); }

SPRT_FORCEINLINE inline long double cosl(long double value) { return __sprt_cosl(value); }


SPRT_FORCEINLINE inline double cosh(double value) { return __sprt_cosh(value); }

SPRT_FORCEINLINE inline float coshf(float value) { return __sprt_coshf(value); }

SPRT_FORCEINLINE inline long double coshl(long double value) { return __sprt_coshl(value); }


SPRT_FORCEINLINE inline double erf(double value) { return __sprt_erf(value); }

SPRT_FORCEINLINE inline float erff(float value) { return __sprt_erff(value); }

SPRT_FORCEINLINE inline long double erfl(long double value) { return __sprt_erfl(value); }


SPRT_FORCEINLINE inline double erfc(double value) { return __sprt_erfc(value); }

SPRT_FORCEINLINE inline float erfcf(float value) { return __sprt_erfcf(value); }

SPRT_FORCEINLINE inline long double erfcl(long double value) { return __sprt_erfcl(value); }


SPRT_FORCEINLINE inline double exp(double value) { return __sprt_exp(value); }

SPRT_FORCEINLINE inline float expf(float value) { return __sprt_expf(value); }

SPRT_FORCEINLINE inline long double expl(long double value) { return __sprt_expl(value); }


SPRT_FORCEINLINE inline double exp2(double value) { return __sprt_exp2(value); }

SPRT_FORCEINLINE inline float exp2f(float value) { return __sprt_exp2f(value); }

SPRT_FORCEINLINE inline long double exp2l(long double value) { return __sprt_exp2l(value); }


SPRT_FORCEINLINE inline double expm1(double value) { return __sprt_expm1(value); }

SPRT_FORCEINLINE inline float expm1f(float value) { return __sprt_expm1f(value); }

SPRT_FORCEINLINE inline long double expm1l(long double value) { return __sprt_expm1l(value); }


SPRT_FORCEINLINE inline double fabs(double value) { return __sprt_fabs(value); }

SPRT_FORCEINLINE inline float fabsf(float value) { return __sprt_fabsf(value); }

SPRT_FORCEINLINE inline long double fabsl(long double value) { return __sprt_fabsl(value); }


SPRT_FORCEINLINE inline double fdim(double a, double b) { return __sprt_fdim(a, b); }

SPRT_FORCEINLINE inline float fdimf(float a, float b) { return __sprt_fdimf(a, b); }

SPRT_FORCEINLINE inline long double fdiml(long double a, long double b) {
	return __sprt_fdiml(a, b);
}


SPRT_FORCEINLINE inline double floor(double value) { return __sprt_floor(value); }

SPRT_FORCEINLINE inline float floorf(float value) { return __sprt_floorf(value); }

SPRT_FORCEINLINE inline long double floorl(long double value) { return __sprt_floorl(value); }


SPRT_FORCEINLINE inline double fma(double a, double b, double c) { return __sprt_fma(a, b, c); }

SPRT_FORCEINLINE inline float fmaf(float a, float b, float c) { return __sprt_fmaf(a, b, c); }

SPRT_FORCEINLINE inline long double fmal(long double a, long double b, long double c) {
	return __sprt_fmal(a, b, c);
}


SPRT_FORCEINLINE inline double fmax(double a, double b) { return __sprt_fmax(a, b); }

SPRT_FORCEINLINE inline float fmaxf(float a, float b) { return __sprt_fmaxf(a, b); }

SPRT_FORCEINLINE inline long double fmaxl(long double a, long double b) {
	return __sprt_fmaxl(a, b);
}


SPRT_FORCEINLINE inline double fmin(double a, double b) { return __sprt_fmin(a, b); }

SPRT_FORCEINLINE inline float fminf(float a, float b) { return __sprt_fminf(a, b); }

SPRT_FORCEINLINE inline long double fminl(long double a, long double b) {
	return __sprt_fminl(a, b);
}


SPRT_FORCEINLINE inline double fmod(double a, double b) { return __sprt_fmod(a, b); }

SPRT_FORCEINLINE inline float fmodf(float a, float b) { return __sprt_fmodf(a, b); }

SPRT_FORCEINLINE inline long double fmodl(long double a, long double b) {
	return __sprt_fmodl(a, b);
}


SPRT_FORCEINLINE inline double frexp(double a, int *b) { return __sprt_frexp(a, b); }

SPRT_FORCEINLINE inline float frexpf(float a, int *b) { return __sprt_frexpf(a, b); }

SPRT_FORCEINLINE inline long double frexpl(long double a, int *b) { return __sprt_frexpl(a, b); }


SPRT_FORCEINLINE inline double hypot(double a, double b) { return __sprt_hypot(a, b); }

SPRT_FORCEINLINE inline float hypotf(float a, float b) { return __sprt_hypotf(a, b); }

SPRT_FORCEINLINE inline long double hypotl(long double a, long double b) {
	return __sprt_hypotl(a, b);
}


SPRT_FORCEINLINE inline int ilogb(double value) { return __sprt_ilogb(value); }

SPRT_FORCEINLINE inline int ilogbf(float value) { return __sprt_ilogbf(value); }

SPRT_FORCEINLINE inline int ilogbl(long double value) { return __sprt_ilogbl(value); }


SPRT_FORCEINLINE inline double ldexp(double a, int b) { return __sprt_ldexp(a, b); }

SPRT_FORCEINLINE inline float ldexpf(float a, int b) { return __sprt_ldexpf(a, b); }

SPRT_FORCEINLINE inline long double ldexpl(long double a, int b) { return __sprt_ldexpl(a, b); }


SPRT_FORCEINLINE inline double lgamma(double value) { return __sprt_lgamma(value); }

SPRT_FORCEINLINE inline float lgammaf(float value) { return __sprt_lgammaf(value); }

SPRT_FORCEINLINE inline long double lgammal(long double value) { return __sprt_lgammal(value); }


SPRT_FORCEINLINE inline long long llrint(double value) { return __sprt_llrint(value); }

SPRT_FORCEINLINE inline long long llrintf(float value) { return __sprt_llrintf(value); }

SPRT_FORCEINLINE inline long long llrintl(long double value) { return __sprt_llrintl(value); }


SPRT_FORCEINLINE inline long long llround(double value) { return __sprt_llround(value); }

SPRT_FORCEINLINE inline long long llroundf(float value) { return __sprt_llroundf(value); }

SPRT_FORCEINLINE inline long long llroundl(long double value) { return __sprt_llroundl(value); }


SPRT_FORCEINLINE inline double log(double value) { return __sprt_log(value); }

SPRT_FORCEINLINE inline float logf(float value) { return __sprt_logf(value); }

SPRT_FORCEINLINE inline long double logl(long double value) { return __sprt_logl(value); }


SPRT_FORCEINLINE inline double log10(double value) { return __sprt_log10(value); }

SPRT_FORCEINLINE inline float log10f(float value) { return __sprt_log10f(value); }

SPRT_FORCEINLINE inline long double log10l(long double value) { return __sprt_log10l(value); }


SPRT_FORCEINLINE inline double log1p(double value) { return __sprt_log1p(value); }

SPRT_FORCEINLINE inline float log1pf(float value) { return __sprt_log1pf(value); }

SPRT_FORCEINLINE inline long double log1pl(long double value) { return __sprt_log1pl(value); }


SPRT_FORCEINLINE inline double log2(double value) { return __sprt_log2(value); }

SPRT_FORCEINLINE inline float log2f(float value) { return __sprt_log2f(value); }

SPRT_FORCEINLINE inline long double log2l(long double value) { return __sprt_log2l(value); }


SPRT_FORCEINLINE inline double logb(double value) { return __sprt_logb(value); }

SPRT_FORCEINLINE inline float logbf(float value) { return __sprt_logbf(value); }

SPRT_FORCEINLINE inline long double logbl(long double value) { return __sprt_logbl(value); }


SPRT_FORCEINLINE inline long lrint(double value) { return __sprt_lrint(value); }

SPRT_FORCEINLINE inline long lrintf(float value) { return __sprt_lrintf(value); }

SPRT_FORCEINLINE inline long lrintl(long double value) { return __sprt_lrintl(value); }


SPRT_FORCEINLINE inline long lround(double value) { return __sprt_lround(value); }

SPRT_FORCEINLINE inline long lroundf(float value) { return __sprt_lroundf(value); }

SPRT_FORCEINLINE inline long lroundl(long double value) { return __sprt_lroundl(value); }


SPRT_FORCEINLINE inline double modf(double a, double *b) { return __sprt_modf(a, b); }

SPRT_FORCEINLINE inline float modff(float a, float *b) { return __sprt_modff(a, b); }

SPRT_FORCEINLINE inline long double modfl(long double a, long double *b) {
	return __sprt_modfl(a, b);
}


SPRT_FORCEINLINE inline double nan(const char *value) { return __sprt_nan(value); }

SPRT_FORCEINLINE inline float nanf(const char *value) { return __sprt_nanf(value); }

SPRT_FORCEINLINE inline long double nanl(const char *value) { return __sprt_nanl(value); }


SPRT_FORCEINLINE inline double nearbyint(double value) { return __sprt_nearbyint(value); }

SPRT_FORCEINLINE inline float nearbyintf(float value) { return __sprt_nearbyintf(value); }

SPRT_FORCEINLINE inline long double nearbyintl(long double value) {
	return __sprt_nearbyintl(value);
}


SPRT_FORCEINLINE inline double nextafter(double a, double b) { return __sprt_nextafter(a, b); }

SPRT_FORCEINLINE inline float nextafterf(float a, float b) { return __sprt_nextafterf(a, b); }

SPRT_FORCEINLINE inline long double nextafterl(long double a, long double b) {
	return __sprt_nextafterl(a, b);
}


SPRT_FORCEINLINE inline double nexttoward(double a, long double b) {
	return __sprt_nexttoward(a, b);
}

SPRT_FORCEINLINE inline float nexttowardf(float a, long double b) {
	return __sprt_nexttowardf(a, b);
}

SPRT_FORCEINLINE inline long double nexttowardl(long double a, long double b) {
	return __sprt_nexttowardl(a, b);
}


SPRT_FORCEINLINE inline double pow(double a, double b) { return __sprt_pow(a, b); }

SPRT_FORCEINLINE inline float powf(float a, float b) { return __sprt_powf(a, b); }

SPRT_FORCEINLINE inline long double powl(long double a, long double b) { return __sprt_powl(a, b); }


SPRT_FORCEINLINE inline double remainder(double a, double b) { return __sprt_remainder(a, b); }

SPRT_FORCEINLINE inline float remainderf(float a, float b) { return __sprt_remainderf(a, b); }

SPRT_FORCEINLINE inline long double remainderl(long double a, long double b) {
	return __sprt_remainderl(a, b);
}


SPRT_FORCEINLINE inline double remquo(double a, double b, int *c) { return __sprt_remquo(a, b, c); }

SPRT_FORCEINLINE inline float remquof(float a, float b, int *c) { return __sprt_remquof(a, b, c); }

SPRT_FORCEINLINE inline long double remquol(long double a, long double b, int *c) {
	return __sprt_remquol(a, b, c);
}


SPRT_FORCEINLINE inline double rint(double value) { return __sprt_rint(value); }

SPRT_FORCEINLINE inline float rintf(float value) { return __sprt_rintf(value); }

SPRT_FORCEINLINE inline long double rintl(long double value) { return __sprt_rintl(value); }


SPRT_FORCEINLINE inline double round(double value) { return __sprt_round(value); }

SPRT_FORCEINLINE inline float roundf(float value) { return __sprt_roundf(value); }

SPRT_FORCEINLINE inline long double roundl(long double value) { return __sprt_roundl(value); }


SPRT_FORCEINLINE inline double scalbln(double a, long b) { return __sprt_scalbln(a, b); }

SPRT_FORCEINLINE inline float scalblnf(float a, long b) { return __sprt_scalblnf(a, b); }

SPRT_FORCEINLINE inline long double scalblnl(long double a, long b) {
	return __sprt_scalblnl(a, b);
}


SPRT_FORCEINLINE inline double scalbn(double a, int b) { return __sprt_scalbn(a, b); }

SPRT_FORCEINLINE inline float scalbnf(float a, int b) { return __sprt_scalbnf(a, b); }

SPRT_FORCEINLINE inline long double scalbnl(long double a, int b) { return __sprt_scalbnl(a, b); }


SPRT_FORCEINLINE inline double sin(double value) { return __sprt_sin(value); }

SPRT_FORCEINLINE inline float sinf(float value) { return __sprt_sinf(value); }

SPRT_FORCEINLINE inline long double sinl(long double value) { return __sprt_sinl(value); }


SPRT_FORCEINLINE inline double sinh(double value) { return __sprt_sinh(value); }

SPRT_FORCEINLINE inline float sinhf(float value) { return __sprt_sinhf(value); }

SPRT_FORCEINLINE inline long double sinhl(long double value) { return __sprt_sinhl(value); }


SPRT_FORCEINLINE inline double sqrt(double value) { return __sprt_sqrt(value); }

SPRT_FORCEINLINE inline float sqrtf(float value) { return __sprt_sqrtf(value); }

SPRT_FORCEINLINE inline long double sqrtl(long double value) { return __sprt_sqrtl(value); }


SPRT_FORCEINLINE inline double tan(double value) { return __sprt_tan(value); }

SPRT_FORCEINLINE inline float tanf(float value) { return __sprt_tanf(value); }

SPRT_FORCEINLINE inline long double tanl(long double value) { return __sprt_tanl(value); }


SPRT_FORCEINLINE inline double tanh(double value) { return __sprt_tanh(value); }

SPRT_FORCEINLINE inline float tanhf(float value) { return __sprt_tanhf(value); }

SPRT_FORCEINLINE inline long double tanhl(long double value) { return __sprt_tanhl(value); }


SPRT_FORCEINLINE inline double tgamma(double value) { return __sprt_tgamma(value); }

SPRT_FORCEINLINE inline float tgammaf(float value) { return __sprt_tgammaf(value); }

SPRT_FORCEINLINE inline long double tgammal(long double value) { return __sprt_tgammal(value); }


SPRT_FORCEINLINE inline double trunc(double value) { return __sprt_trunc(value); }

SPRT_FORCEINLINE inline float truncf(float value) { return __sprt_truncf(value); }

SPRT_FORCEINLINE inline long double truncl(long double value) { return __sprt_truncl(value); }

__SPRT_END_DECL

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_MATH_H_
