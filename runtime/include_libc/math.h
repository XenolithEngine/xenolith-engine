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

#if defined(__SPRT_BUILD) && __STDC_HOSTED__ == 1

#include_next <math.h>

#else

#include <sprt/c/__sprt_math.h>

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

#ifndef __cplusplus
#define fpclassify(x) __sprt_fpclassify(x)
#define isinf(x) __sprt_isinf(x)
#define isnan(x) __sprt_isnan(x)
#define isnormal(x) __sprt_isnormal(x)
#define isfinite(x) __sprt_isfinite(x)
#define signbit(x) __sprt_signbit(x)
#endif

SPRT_UMBRELLA_FUNC
double acos(double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_acos(value);
}
#endif

SPRT_UMBRELLA_FUNC
float acosf(float value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_acosf(value);
}
#endif

SPRT_UMBRELLA_FUNC
long double acosl(long double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_acosl(value);
}
#endif


SPRT_UMBRELLA_FUNC
double acosh(double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_acosh(value);
}
#endif

SPRT_UMBRELLA_FUNC
float acoshf(float value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_acoshf(value);
}
#endif

SPRT_UMBRELLA_FUNC
long double acoshl(long double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_acoshl(value);
}
#endif


SPRT_UMBRELLA_FUNC
double asin(double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_asin(value);
}
#endif

SPRT_UMBRELLA_FUNC
float asinf(float value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_asinf(value);
}
#endif

SPRT_UMBRELLA_FUNC
long double asinl(long double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_asinl(value);
}
#endif


SPRT_UMBRELLA_FUNC
double asinh(double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_asinh(value);
}
#endif

SPRT_UMBRELLA_FUNC
float asinhf(float value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_asinhf(value);
}
#endif

SPRT_UMBRELLA_FUNC
long double asinhl(long double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_asinhl(value);
}
#endif


SPRT_UMBRELLA_FUNC
double atan(double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_atan(value);
}
#endif

SPRT_UMBRELLA_FUNC
float atanf(float value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_atanf(value);
}
#endif

SPRT_UMBRELLA_FUNC
long double atanl(long double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_atanl(value);
}
#endif


SPRT_UMBRELLA_FUNC
double atan2(double a, double b) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_atan2(a, b);
}
#endif

SPRT_UMBRELLA_FUNC
float atan2f(float a, float b) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_atan2f(a, b);
}
#endif

SPRT_UMBRELLA_FUNC
long double atan2l(long double a, long double b) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_atan2l(a, b);
}
#endif


SPRT_UMBRELLA_FUNC
double atanh(double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_atanh(value);
}
#endif

SPRT_UMBRELLA_FUNC
float atanhf(float value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_atanhf(value);
}
#endif

SPRT_UMBRELLA_FUNC
long double atanhl(long double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_atanhl(value);
}
#endif


SPRT_UMBRELLA_FUNC
double cbrt(double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_cbrt(value);
}
#endif

SPRT_UMBRELLA_FUNC
float cbrtf(float value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_cbrtf(value);
}
#endif

SPRT_UMBRELLA_FUNC
long double cbrtl(long double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_cbrtl(value);
}
#endif


SPRT_UMBRELLA_FUNC
double ceil(double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_ceil(value);
}
#endif

SPRT_UMBRELLA_FUNC
float ceilf(float value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_ceilf(value);
}
#endif

SPRT_UMBRELLA_FUNC
long double ceill(long double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_ceill(value);
}
#endif


SPRT_UMBRELLA_FUNC
double copysign(double a, double b) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_copysign(a, b);
}
#endif

SPRT_UMBRELLA_FUNC
float copysignf(float a, float b) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_copysignf(a, b);
}
#endif

SPRT_UMBRELLA_FUNC
long double copysignl(long double a, long double b) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_copysignl(a, b);
}
#endif


SPRT_UMBRELLA_FUNC
double cos(double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_cos(value);
}
#endif

SPRT_UMBRELLA_FUNC
float cosf(float value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_cosf(value);
}
#endif

SPRT_UMBRELLA_FUNC
long double cosl(long double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_cosl(value);
}
#endif


SPRT_UMBRELLA_FUNC
double cosh(double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_cosh(value);
}
#endif

SPRT_UMBRELLA_FUNC
float coshf(float value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_coshf(value);
}
#endif

SPRT_UMBRELLA_FUNC
long double coshl(long double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_coshl(value);
}
#endif


SPRT_UMBRELLA_FUNC
double erf(double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_erf(value);
}
#endif

SPRT_UMBRELLA_FUNC
float erff(float value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_erff(value);
}
#endif

SPRT_UMBRELLA_FUNC
long double erfl(long double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_erfl(value);
}
#endif


SPRT_UMBRELLA_FUNC
double erfc(double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_erfc(value);
}
#endif

SPRT_UMBRELLA_FUNC
float erfcf(float value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_erfcf(value);
}
#endif

SPRT_UMBRELLA_FUNC
long double erfcl(long double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_erfcl(value);
}
#endif


SPRT_UMBRELLA_FUNC
double exp(double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_exp(value);
}
#endif

SPRT_UMBRELLA_FUNC
float expf(float value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_expf(value);
}
#endif

SPRT_UMBRELLA_FUNC
long double expl(long double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_expl(value);
}
#endif


SPRT_UMBRELLA_FUNC
double exp2(double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_exp2(value);
}
#endif

SPRT_UMBRELLA_FUNC
float exp2f(float value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_exp2f(value);
}
#endif

SPRT_UMBRELLA_FUNC
long double exp2l(long double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_exp2l(value);
}
#endif


SPRT_UMBRELLA_FUNC
double expm1(double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_expm1(value);
}
#endif

SPRT_UMBRELLA_FUNC
float expm1f(float value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_expm1f(value);
}
#endif

SPRT_UMBRELLA_FUNC
long double expm1l(long double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_expm1l(value);
}
#endif


SPRT_UMBRELLA_FUNC
double fabs(double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_fabs(value);
}
#endif

SPRT_UMBRELLA_FUNC
float fabsf(float value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_fabsf(value);
}
#endif

SPRT_UMBRELLA_FUNC
long double fabsl(long double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_fabsl(value);
}
#endif


SPRT_UMBRELLA_FUNC
double fdim(double a, double b) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_fdim(a, b);
}
#endif

SPRT_UMBRELLA_FUNC
float fdimf(float a, float b) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_fdimf(a, b);
}
#endif

SPRT_UMBRELLA_FUNC
long double fdiml(long double a, long double b) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_fdiml(a, b);
}
#endif


SPRT_UMBRELLA_FUNC
double floor(double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_floor(value);
}
#endif

SPRT_UMBRELLA_FUNC
float floorf(float value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_floorf(value);
}
#endif

SPRT_UMBRELLA_FUNC
long double floorl(long double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_floorl(value);
}
#endif


SPRT_UMBRELLA_FUNC
double fma(double a, double b, double c) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_fma(a, b, c);
}
#endif

SPRT_UMBRELLA_FUNC
float fmaf(float a, float b, float c) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_fmaf(a, b, c);
}
#endif

SPRT_UMBRELLA_FUNC
long double fmal(long double a, long double b, long double c) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_fmal(a, b, c);
}
#endif


SPRT_UMBRELLA_FUNC
double fmax(double a, double b) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_fmax(a, b);
}
#endif

SPRT_UMBRELLA_FUNC
float fmaxf(float a, float b) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_fmaxf(a, b);
}
#endif

SPRT_UMBRELLA_FUNC
long double fmaxl(long double a, long double b) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_fmaxl(a, b);
}
#endif


SPRT_UMBRELLA_FUNC
double fmin(double a, double b) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_fmin(a, b);
}
#endif

SPRT_UMBRELLA_FUNC
float fminf(float a, float b) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_fminf(a, b);
}
#endif

SPRT_UMBRELLA_FUNC
long double fminl(long double a, long double b) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_fminl(a, b);
}
#endif


SPRT_UMBRELLA_FUNC
double fmod(double a, double b) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_fmod(a, b);
}
#endif

SPRT_UMBRELLA_FUNC
float fmodf(float a, float b) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_fmodf(a, b);
}
#endif

SPRT_UMBRELLA_FUNC
long double fmodl(long double a, long double b) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_fmodl(a, b);
}
#endif


SPRT_UMBRELLA_FUNC
double frexp(double a, int *b) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_frexp(a, b);
}
#endif

SPRT_UMBRELLA_FUNC
float frexpf(float a, int *b) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_frexpf(a, b);
}
#endif

SPRT_UMBRELLA_FUNC
long double frexpl(long double a, int *b) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_frexpl(a, b);
}
#endif


SPRT_UMBRELLA_FUNC
double hypot(double a, double b) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_hypot(a, b);
}
#endif

SPRT_UMBRELLA_FUNC
float hypotf(float a, float b) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_hypotf(a, b);
}
#endif

SPRT_UMBRELLA_FUNC
long double hypotl(long double a, long double b) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_hypotl(a, b);
}
#endif


SPRT_UMBRELLA_FUNC
int ilogb(double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_ilogb(value);
}
#endif

SPRT_UMBRELLA_FUNC
int ilogbf(float value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_ilogbf(value);
}
#endif

SPRT_UMBRELLA_FUNC
int ilogbl(long double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_ilogbl(value);
}
#endif


SPRT_UMBRELLA_FUNC
double ldexp(double a, int b) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_ldexp(a, b);
}
#endif

SPRT_UMBRELLA_FUNC
float ldexpf(float a, int b) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_ldexpf(a, b);
}
#endif

SPRT_UMBRELLA_FUNC
long double ldexpl(long double a, int b) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_ldexpl(a, b);
}
#endif


SPRT_UMBRELLA_FUNC
double lgamma(double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_lgamma(value);
}
#endif

SPRT_UMBRELLA_FUNC
float lgammaf(float value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_lgammaf(value);
}
#endif

SPRT_UMBRELLA_FUNC
long double lgammal(long double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_lgammal(value);
}
#endif


SPRT_UMBRELLA_FUNC
long long llrint(double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_llrint(value);
}
#endif

SPRT_UMBRELLA_FUNC
long long llrintf(float value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_llrintf(value);
}
#endif

SPRT_UMBRELLA_FUNC
long long llrintl(long double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_llrintl(value);
}
#endif


SPRT_UMBRELLA_FUNC
long long llround(double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_llround(value);
}
#endif

SPRT_UMBRELLA_FUNC
long long llroundf(float value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_llroundf(value);
}
#endif

SPRT_UMBRELLA_FUNC
long long llroundl(long double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_llroundl(value);
}
#endif


SPRT_UMBRELLA_FUNC
double log(double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_log(value);
}
#endif

SPRT_UMBRELLA_FUNC
float logf(float value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_logf(value);
}
#endif

SPRT_UMBRELLA_FUNC
long double logl(long double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_logl(value);
}
#endif


SPRT_UMBRELLA_FUNC
double log10(double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_log10(value);
}
#endif

SPRT_UMBRELLA_FUNC
float log10f(float value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_log10f(value);
}
#endif

SPRT_UMBRELLA_FUNC
long double log10l(long double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_log10l(value);
}
#endif


SPRT_UMBRELLA_FUNC
double log1p(double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_log1p(value);
}
#endif

SPRT_UMBRELLA_FUNC
float log1pf(float value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_log1pf(value);
}
#endif

SPRT_UMBRELLA_FUNC
long double log1pl(long double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_log1pl(value);
}
#endif


SPRT_UMBRELLA_FUNC
double log2(double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_log2(value);
}
#endif

SPRT_UMBRELLA_FUNC
float log2f(float value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_log2f(value);
}
#endif

SPRT_UMBRELLA_FUNC
long double log2l(long double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_log2l(value);
}
#endif


SPRT_UMBRELLA_FUNC
double logb(double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_logb(value);
}
#endif

SPRT_UMBRELLA_FUNC
float logbf(float value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_logbf(value);
}
#endif

SPRT_UMBRELLA_FUNC
long double logbl(long double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_logbl(value);
}
#endif


SPRT_UMBRELLA_FUNC
long lrint(double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_lrint(value);
}
#endif

SPRT_UMBRELLA_FUNC
long lrintf(float value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_lrintf(value);
}
#endif

SPRT_UMBRELLA_FUNC
long lrintl(long double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_lrintl(value);
}
#endif


SPRT_UMBRELLA_FUNC
long lround(double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_lround(value);
}
#endif

SPRT_UMBRELLA_FUNC
long lroundf(float value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_lroundf(value);
}
#endif

SPRT_UMBRELLA_FUNC
long lroundl(long double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_lroundl(value);
}
#endif


SPRT_UMBRELLA_FUNC
double modf(double a, double *b) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_modf(a, b);
}
#endif

SPRT_UMBRELLA_FUNC
float modff(float a, float *b) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_modff(a, b);
}
#endif

SPRT_UMBRELLA_FUNC
long double modfl(long double a, long double *b) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_modfl(a, b);
}
#endif


SPRT_UMBRELLA_FUNC
double nan(const char *value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_nan(value);
}
#endif

SPRT_UMBRELLA_FUNC
float nanf(const char *value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_nanf(value);
}
#endif

SPRT_UMBRELLA_FUNC
long double nanl(const char *value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_nanl(value);
}
#endif


SPRT_UMBRELLA_FUNC
double nearbyint(double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_nearbyint(value);
}
#endif

SPRT_UMBRELLA_FUNC
float nearbyintf(float value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_nearbyintf(value);
}
#endif

SPRT_UMBRELLA_FUNC
long double nearbyintl(long double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_nearbyintl(value);
}
#endif


SPRT_UMBRELLA_FUNC
double nextafter(double a, double b) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_nextafter(a, b);
}
#endif

SPRT_UMBRELLA_FUNC
float nextafterf(float a, float b) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_nextafterf(a, b);
}
#endif

SPRT_UMBRELLA_FUNC
long double nextafterl(long double a, long double b) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_nextafterl(a, b);
}
#endif


SPRT_UMBRELLA_FUNC
double nexttoward(double a, long double b) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_nexttoward(a, b);
}
#endif

SPRT_UMBRELLA_FUNC
float nexttowardf(float a, long double b) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_nexttowardf(a, b);
}
#endif

SPRT_UMBRELLA_FUNC
long double nexttowardl(long double a, long double b) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_nexttowardl(a, b);
}
#endif


SPRT_UMBRELLA_FUNC
double pow(double a, double b) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pow(a, b);
}
#endif

SPRT_UMBRELLA_FUNC
float powf(float a, float b) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_powf(a, b);
}
#endif

SPRT_UMBRELLA_FUNC
long double powl(long double a, long double b) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_powl(a, b);
}
#endif


SPRT_UMBRELLA_FUNC
double remainder(double a, double b) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_remainder(a, b);
}
#endif

SPRT_UMBRELLA_FUNC
float remainderf(float a, float b) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_remainderf(a, b);
}
#endif

SPRT_UMBRELLA_FUNC
long double remainderl(long double a, long double b) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_remainderl(a, b);
}
#endif


SPRT_UMBRELLA_FUNC
double remquo(double a, double b, int *c) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_remquo(a, b, c);
}
#endif

SPRT_UMBRELLA_FUNC
float remquof(float a, float b, int *c) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_remquof(a, b, c);
}
#endif

SPRT_UMBRELLA_FUNC
long double remquol(long double a, long double b, int *c) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_remquol(a, b, c);
}
#endif

SPRT_UMBRELLA_FUNC
double rint(double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_rint(value);
}
#endif

SPRT_UMBRELLA_FUNC
float rintf(float value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_rintf(value);
}
#endif

SPRT_UMBRELLA_FUNC
long double rintl(long double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_rintl(value);
}
#endif


SPRT_UMBRELLA_FUNC
double round(double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_round(value);
}
#endif

SPRT_UMBRELLA_FUNC
float roundf(float value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_roundf(value);
}
#endif

SPRT_UMBRELLA_FUNC
long double roundl(long double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_roundl(value);
}
#endif


SPRT_UMBRELLA_FUNC
double scalbln(double a, long b) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_scalbln(a, b);
}
#endif

SPRT_UMBRELLA_FUNC
float scalblnf(float a, long b) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_scalblnf(a, b);
}
#endif

SPRT_UMBRELLA_FUNC
long double scalblnl(long double a, long b) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_scalblnl(a, b);
}
#endif


SPRT_UMBRELLA_FUNC
double scalbn(double a, int b) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_scalbn(a, b);
}
#endif

SPRT_UMBRELLA_FUNC
float scalbnf(float a, int b) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_scalbnf(a, b);
}
#endif

SPRT_UMBRELLA_FUNC
long double scalbnl(long double a, int b) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_scalbnl(a, b);
}
#endif


SPRT_UMBRELLA_FUNC
double sin(double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_sin(value);
}
#endif

SPRT_UMBRELLA_FUNC
float sinf(float value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_sinf(value);
}
#endif

SPRT_UMBRELLA_FUNC
long double sinl(long double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_sinl(value);
}
#endif


SPRT_UMBRELLA_FUNC
double sinh(double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_sinh(value);
}
#endif

SPRT_UMBRELLA_FUNC
float sinhf(float value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_sinhf(value);
}
#endif

SPRT_UMBRELLA_FUNC
long double sinhl(long double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_sinhl(value);
}
#endif


SPRT_UMBRELLA_FUNC
double sqrt(double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_sqrt(value);
}
#endif

SPRT_UMBRELLA_FUNC
float sqrtf(float value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_sqrtf(value);
}
#endif

SPRT_UMBRELLA_FUNC
long double sqrtl(long double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_sqrtl(value);
}
#endif


SPRT_UMBRELLA_FUNC
double tan(double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_tan(value);
}
#endif

SPRT_UMBRELLA_FUNC
float tanf(float value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_tanf(value);
}
#endif

SPRT_UMBRELLA_FUNC
long double tanl(long double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_tanl(value);
}
#endif


SPRT_UMBRELLA_FUNC
double tanh(double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_tanh(value);
}
#endif

SPRT_UMBRELLA_FUNC
float tanhf(float value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_tanhf(value);
}
#endif

SPRT_UMBRELLA_FUNC
long double tanhl(long double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_tanhl(value);
}
#endif


SPRT_UMBRELLA_FUNC
double tgamma(double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_tgamma(value);
}
#endif

SPRT_UMBRELLA_FUNC
float tgammaf(float value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_tgammaf(value);
}
#endif

SPRT_UMBRELLA_FUNC
long double tgammal(long double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_tgammal(value);
}
#endif


SPRT_UMBRELLA_FUNC
double trunc(double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_trunc(value);
}
#endif

SPRT_UMBRELLA_FUNC
float truncf(float value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_truncf(value);
}
#endif

SPRT_UMBRELLA_FUNC
long double truncl(long double value) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_truncl(value);
}
#endif

__SPRT_END_DECL

#ifdef _LIBCPP_MSVCRT
#include <vcruntime_math.h>
#endif

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_MATH_H_
