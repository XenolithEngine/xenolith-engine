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

#ifndef CORE_RUNTIME_INCLUDE_C___SPRT_MATH_H_
#define CORE_RUNTIME_INCLUDE_C___SPRT_MATH_H_

#include <sprt/c/bits/__sprt_float.h>
#include <sprt/c/cross/__sprt_math.h>

// clang-format off
#define __SPRT_M_E             2.7182818284590452354   /* e */
#define __SPRT_M_LOG2E         1.4426950408889634074   /* log_2 e */
#define __SPRT_M_LOG10E        0.43429448190325182765  /* log_10 e */
#define __SPRT_M_LN2           0.69314718055994530942  /* log_e 2 */
#define __SPRT_M_LN10          2.30258509299404568402  /* log_e 10 */
#define __SPRT_M_PI            3.14159265358979323846  /* pi */
#define __SPRT_M_PI_2          1.57079632679489661923  /* pi/2 */
#define __SPRT_M_PI_4          0.78539816339744830962  /* pi/4 */
#define __SPRT_M_1_PI          0.31830988618379067154  /* 1/pi */
#define __SPRT_M_2_PI          0.63661977236758134308  /* 2/pi */
#define __SPRT_M_2_SQRTPI      1.12837916709551257390  /* 2/sqrt(pi) */
#define __SPRT_M_SQRT2         1.41421356237309504880  /* sqrt(2) */
#define __SPRT_M_SQRT1_2       0.70710678118654752440  /* 1/sqrt(2) */
// clang-format on

#if __SPRT_HAS_BUILTIN(__builtin_nanf)
#define __SPRT_NAN       __builtin_nanf("")
#else
#define __SPRT_NAN       (float)(0.0f/0.0f)
#endif

#if __SPRT_HAS_BUILTIN(__builtin_inff)
#define __SPRT_INFINITY      __builtin_inff()
#else
#define __SPRT_INFINITY       (float)(1e+300 * 1e+300)
#endif

#if __SPRT_HAS_BUILTIN(__builtin_huge_val)
#define __SPRT_HUGE_VAL  __builtin_huge_val()
#else
#define __SPRT_HUGE_VAL ((double)__SPRT_INFINITY)
#endif

#if __SPRT_HAS_BUILTIN(__builtin_huge_valf)
#define __SPRT_HUGE_VALF  __builtin_huge_valf()
#else
#define __SPRT_HUGE_VALF ((float)__SPRT_INFINITY)
#endif

#if __SPRT_HAS_BUILTIN(__builtin_huge_vall)
#define __SPRT_HUGE_VALL  __builtin_huge_vall()
#else
#define __SPRT_HUGE_VALL ((long double)__SPRT_INFINITY)
#endif

#if defined(__FP_FAST_FMA)
#define FP_FAST_FMA 1
#endif
#if defined(__FP_FAST_FMAF)
#define FP_FAST_FMAF 1
#endif
#if defined(__FP_FAST_FMAL)
#define FP_FAST_FMAL 1
#endif

__SPRT_BEGIN_DECL

#if __SPRT_HAS_BUILTIN(__builtin_fpclassify)
#define __sprt_fpclassify(v) __builtin_fpclassify(__SPRT_FP_NAN, __SPRT_FP_INFINITE, \
	__SPRT_FP_NORMAL, __SPRT_FP_SUBNORMAL, __SPRT_FP_ZERO, v)
#else
SPRT_API int __SPRT_ID(__fpclassify)(double);
SPRT_API int __SPRT_ID(__fpclassifyf)(float);
SPRT_API int __SPRT_ID(__fpclassifyl)(long double);

#define __sprt_fpclassify(x) ( \
	sizeof(x) == sizeof(float) ? __SPRT_ID(__fpclassifyf(x)) : \
	sizeof(x) == sizeof(double) ? __SPRT_ID(__fpclassify)(x) : \
	__SPRT_ID(__fpclassifyl)(x) )
#endif

static inline unsigned __SPRT_FLOAT_BITS(float __f) {
	union {
		float __f;
		unsigned __i;
	} __u;
	__u.__f = __f;
	return __u.__i;
}
static inline unsigned long long __SPRT_DOUBLE_BITS(double __f) {
	union {
		double __f;
		unsigned long long __i;
	} __u;
	__u.__f = __f;
	return __u.__i;
}

// clang-format off
#if __SPRT_HAS_BUILTIN(__builtin_isfinite)
#define __sprt_isfinite(v) __builtin_isfinite(v)
#else
#define __sprt_isfinite(x) ( \
	sizeof(x) == sizeof(float) ? (__SPRT_FLOAT_BITS(x) & 0x7fffffff) < 0x7f800000 : \
	sizeof(x) == sizeof(double) ? (__SPRT_DOUBLE_BITS(x) & -1ULL>>1) < 0x7ffULL<<52 : \
	__sprt_fpclassify(x) > __SPRT_FP_INFINITE)
#endif

#if __SPRT_HAS_BUILTIN(__builtin_isnan)
#define __sprt_isnan(v) __builtin_isnan(v)
#else
#define __sprt_isnan(x) ( \
	sizeof(x) == sizeof(float) ? (__SPRT_FLOAT_BITS(x) & 0x7fffffff) > 0x7f800000 : \
	sizeof(x) == sizeof(double) ? (__SPRT_DOUBLE_BITS(x) & -1ULL>>1) > 0x7ffULL<<52 : \
	__sprt_fpclassify(x) == __SPRT_FP_NAN)
#endif

#if __SPRT_HAS_BUILTIN(__builtin_isinf)
#define __sprt_isinf(v) __builtin_isinf(v)
#else
#define __sprt_isinf(x) ( \
	sizeof(x) == sizeof(float) ? (__SPRT_FLOAT_BITS(x) & 0x7fffffff) == 0x7f800000 : \
	sizeof(x) == sizeof(double) ? (__SPRT_DOUBLE_BITS(x) & -1ULL>>1) == 0x7ffULL<<52 : \
	__sprt_fpclassify(x) == __SPRT_FP_INFINITE)
#endif

#if __SPRT_HAS_BUILTIN(__builtin_isnormal)
#define __sprt_isnormal(v) __builtin_isnormal(v)
#else
#define __sprt_isnormal(x) ( \
	sizeof(x) == sizeof(float) ? ((__SPRT_FLOAT_BITS(x)+0x00800000) & 0x7fffffff) >= 0x01000000 : \
	sizeof(x) == sizeof(double) ? ((__SPRT_DOUBLE_BITS(x)+(1ULL<<52)) & -1ULL>>1) >= 1ULL<<53 : \
	__sprt_fpclassify(x) == __SPRT_FP_NORMAL)
#endif

// clang-format off

#if __SPRT_HAS_BUILTIN(__builtin_signbit)
#define __sprt_signbit(v) __builtin_signbit(v)
#else
SPRT_API int __SPRT_ID(__signbit)(double);
SPRT_API int __SPRT_ID(__signbitf)(float);
SPRT_API int __SPRT_ID(__signbitl)(long double);

#define __sprt_signbit(x) ( \
	sizeof(x) == sizeof(float) ? (int)(__SPRT_FLOAT_BITS(x)>>31) : \
	sizeof(x) == sizeof(double) ? (int)(__SPRT_DOUBLE_BITS(x)>>63) : \
	__SPRT_ID(__signbitl)(x) )
#endif

#if __SPRT_HAS_BUILTIN(__builtin_isgreater)
#define __sprt_isgreater(a, b) __builtin_isgreater(a, b)
#endif

#if __SPRT_HAS_BUILTIN(__builtin_isgreaterequal)
#define __sprt_isgreaterequal(a, b) __builtin_isgreaterequal(a, b)
#endif

#if __SPRT_HAS_BUILTIN(__builtin_isless)
#define __sprt_isless(a, b) __builtin_isless(a, b)
#endif

#if __SPRT_HAS_BUILTIN(__builtin_islessequal)
#define __sprt_islessequal(a, b) __builtin_islessequal(a, b)
#endif

#if __SPRT_HAS_BUILTIN(__builtin_islessgreater)
#define __sprt_islessgreater(a, b) __builtin_islessgreater(a, b)
#endif

#if __SPRT_HAS_BUILTIN(__builtin_isunordered)
#define __sprt_isunordered(a, b) __builtin_isunordered(a, b)
#endif

#ifndef __sprt_isunordered
#define __sprt_isunordered(x, y) (__sprt_isnan((x)) ? ((void)(y),1) : __sprt_isnan((y)))
#endif


#define __SPRT_ISREL_DEF(rel, op, type) \
static inline int __sprt_x_is##rel(type __x, type __y) \
{ return !__sprt_isunordered(__x,__y) && __x op __y; }

__SPRT_ISREL_DEF(lessf, <, __SPRT_ID(float_t))
__SPRT_ISREL_DEF(less, <, __SPRT_ID(double_t))
__SPRT_ISREL_DEF(lessl, <, long double)
__SPRT_ISREL_DEF(lessequalf, <=, __SPRT_ID(float_t))
__SPRT_ISREL_DEF(lessequal, <=, __SPRT_ID(double_t))
__SPRT_ISREL_DEF(lessequall, <=, long double)
__SPRT_ISREL_DEF(lessgreaterf, !=, __SPRT_ID(float_t))
__SPRT_ISREL_DEF(lessgreater, !=, __SPRT_ID(double_t))
__SPRT_ISREL_DEF(lessgreaterl, !=, long double)
__SPRT_ISREL_DEF(greaterf, >, __SPRT_ID(float_t))
__SPRT_ISREL_DEF(greater, >, __SPRT_ID(double_t))
__SPRT_ISREL_DEF(greaterl, >, long double)
__SPRT_ISREL_DEF(greaterequalf, >=, __SPRT_ID(float_t))
__SPRT_ISREL_DEF(greaterequal, >=, __SPRT_ID(double_t))
__SPRT_ISREL_DEF(greaterequall, >=, long double)

#define __sprt_x_tg_pred_2(x, y, p) ( \
	sizeof((x)+(y)) == sizeof(float) ? p##f(x, y) : \
	sizeof((x)+(y)) == sizeof(double) ? p(x, y) : \
	p##l(x, y) )

#ifndef __sprt_isless
#define __sprt_isless(x, y)            __sprt_x_tg_pred_2(x, y, __sprt_x_isless)
#endif

#ifndef __sprt_islessequal
#define __sprt_islessequal(x, y)       __sprt_x_tg_pred_2(x, y, __sprt_x_islessequal)
#endif

#ifndef __sprt_islessgreater
#define __sprt_islessgreater(x, y)     __sprt_x_tg_pred_2(x, y, __sprt_x_islessgreater)
#endif

#ifndef __sprt_isgreater
#define __sprt_isgreater(x, y)         __sprt_x_tg_pred_2(x, y, __sprt_x_isgreater)
#endif

#ifndef __sprt_isgreaterequal
#define __sprt_isgreaterequal(x, y)    __sprt_x_tg_pred_2(x, y, __sprt_x_isgreaterequal)
#endif


SPRT_API double __SPRT_ID(acos_impl)(double);
#define __sprt_acos __SPRT_ID(acos_impl)

SPRT_API float __SPRT_ID(acosf_impl)(float);
#define __sprt_acosf __SPRT_ID(acosf_impl)

SPRT_API long double __SPRT_ID(acosl_impl)(long double);
#define __sprt_acosl __SPRT_ID(acosl_impl)

SPRT_API double __SPRT_ID(acosh_impl)(double);
#define __sprt_acosh __SPRT_ID(acosh_impl)

SPRT_API float __SPRT_ID(acoshf_impl)(float);
#define __sprt_acoshf __SPRT_ID(acoshf_impl)

SPRT_API long double __SPRT_ID(acoshl_impl)(long double);
#define __sprt_acoshl __SPRT_ID(acoshl_impl)

SPRT_API double __SPRT_ID(asin_impl)(double);
#define __sprt_asin __SPRT_ID(asin_impl)

SPRT_API float __SPRT_ID(asinf_impl)(float);
#define __sprt_asinf __SPRT_ID(asinf_impl)

SPRT_API long double __SPRT_ID(asinl_impl)(long double);
#define __sprt_asinl __SPRT_ID(asinl_impl)

SPRT_API double __SPRT_ID(asinh_impl)(double);
#define __sprt_asinh __SPRT_ID(asinh_impl)

SPRT_API float __SPRT_ID(asinhf_impl)(float);
#define __sprt_asinhf __SPRT_ID(asinhf_impl)

SPRT_API long double __SPRT_ID(asinhl_impl)(long double);
#define __sprt_asinhl __SPRT_ID(asinhl_impl)

SPRT_API double __SPRT_ID(atan_impl)(double);
#define __sprt_atan __SPRT_ID(atan_impl)

SPRT_API float __SPRT_ID(atanf_impl)(float);
#define __sprt_atanf __SPRT_ID(atanf_impl)

SPRT_API long double __SPRT_ID(atanl_impl)(long double);
#define __sprt_atanl __SPRT_ID(atanl_impl)

SPRT_API double __SPRT_ID(atan2_impl)(double, double);
#define __sprt_atan2 __SPRT_ID(atan2_impl)

SPRT_API float __SPRT_ID(atan2f_impl)(float, float);
#define __sprt_atan2f __SPRT_ID(atan2f_impl)

SPRT_API long double __SPRT_ID(atan2l_impl)(long double, long double);
#define __sprt_atan2l __SPRT_ID(atan2l_impl)

SPRT_API double __SPRT_ID(atanh_impl)(double);
#define __sprt_atanh __SPRT_ID(atanh_impl)

SPRT_API float __SPRT_ID(atanhf_impl)(float);
#define __sprt_atanhf __SPRT_ID(atanhf_impl)

SPRT_API long double __SPRT_ID(atanhl_impl)(long double);
#define __sprt_atanhl __SPRT_ID(atanhl_impl)

SPRT_API double __SPRT_ID(cbrt_impl)(double);
#define __sprt_cbrt __SPRT_ID(cbrt_impl)

SPRT_API float __SPRT_ID(cbrtf_impl)(float);
#define __sprt_cbrtf __SPRT_ID(cbrtf_impl)

SPRT_API long double __SPRT_ID(cbrtl_impl)(long double);
#define __sprt_cbrtl __SPRT_ID(cbrtl_impl)

SPRT_API double __SPRT_ID(ceil_impl)(double);
#define __sprt_ceil __SPRT_ID(ceil_impl)

SPRT_API float __SPRT_ID(ceilf_impl)(float);
#define __sprt_ceilf __SPRT_ID(ceilf_impl)

SPRT_API long double __SPRT_ID(ceill_impl)(long double);
#define __sprt_ceill __SPRT_ID(ceill_impl)

SPRT_API double __SPRT_ID(copysign_impl)(double, double);
#define __sprt_copysign __SPRT_ID(copysign_impl)

SPRT_API float __SPRT_ID(copysignf_impl)(float, float);
#define __sprt_copysignf __SPRT_ID(copysignf_impl)

SPRT_API long double __SPRT_ID(copysignl_impl)(long double, long double);
#define __sprt_copysignl __SPRT_ID(copysignl_impl)

SPRT_API double __SPRT_ID(cos_impl)(double);
#define __sprt_cos __SPRT_ID(cos_impl)

SPRT_API float __SPRT_ID(cosf_impl)(float);
#define __sprt_cosf __SPRT_ID(cosf_impl)

SPRT_API long double __SPRT_ID(cosl_impl)(long double);
#define __sprt_cosl __SPRT_ID(cosl_impl)

SPRT_API double __SPRT_ID(cosh_impl)(double);
#define __sprt_cosh __SPRT_ID(cosh_impl)

SPRT_API float __SPRT_ID(coshf_impl)(float);
#define __sprt_coshf __SPRT_ID(coshf_impl)

SPRT_API long double __SPRT_ID(coshl_impl)(long double);
#define __sprt_coshl __SPRT_ID(coshl_impl)

SPRT_API double __SPRT_ID(erf_impl)(double);
#define __sprt_erf __SPRT_ID(erf_impl)

SPRT_API float __SPRT_ID(erff_impl)(float);
#define __sprt_erff __SPRT_ID(erff_impl)

SPRT_API long double __SPRT_ID(erfl_impl)(long double);
#define __sprt_erfl __SPRT_ID(erfl_impl)

SPRT_API double __SPRT_ID(erfc_impl)(double);
#define __sprt_erfc __SPRT_ID(erfc_impl)

SPRT_API float __SPRT_ID(erfcf_impl)(float);
#define __sprt_erfcf __SPRT_ID(erfcf_impl)

SPRT_API long double __SPRT_ID(erfcl_impl)(long double);
#define __sprt_erfcl __SPRT_ID(erfcl_impl)

SPRT_API double __SPRT_ID(exp_impl)(double);
#define __sprt_exp __SPRT_ID(exp_impl)

SPRT_API float __SPRT_ID(expf_impl)(float);
#define __sprt_expf __SPRT_ID(expf_impl)

SPRT_API long double __SPRT_ID(expl_impl)(long double);
#define __sprt_expl __SPRT_ID(expl_impl)

SPRT_API double __SPRT_ID(exp2_impl)(double);
#define __sprt_exp2 __SPRT_ID(exp2_impl)

SPRT_API float __SPRT_ID(exp2f_impl)(float);
#define __sprt_exp2f __SPRT_ID(exp2f_impl)

SPRT_API long double __SPRT_ID(exp2l_impl)(long double);
#define __sprt_exp2l __SPRT_ID(exp2l_impl)

SPRT_API double __SPRT_ID(expm1_impl)(double);
#define __sprt_expm1 __SPRT_ID(expm1_impl)

SPRT_API float __SPRT_ID(expm1f_impl)(float);
#define __sprt_expm1f __SPRT_ID(expm1f_impl)

SPRT_API long double __SPRT_ID(expm1l_impl)(long double);
#define __sprt_expm1l __SPRT_ID(expm1l_impl)

SPRT_API double __SPRT_ID(fabs_impl)(double);
#define __sprt_fabs __SPRT_ID(fabs_impl)

SPRT_API float __SPRT_ID(fabsf_impl)(float);
#define __sprt_fabsf __SPRT_ID(fabsf_impl)

SPRT_API long double __SPRT_ID(fabsl_impl)(long double);
#define __sprt_fabsl __SPRT_ID(fabsl_impl)

SPRT_API double __SPRT_ID(fdim_impl)(double, double);
#define __sprt_fdim __SPRT_ID(fdim_impl)

SPRT_API float __SPRT_ID(fdimf_impl)(float, float);
#define __sprt_fdimf __SPRT_ID(fdimf_impl)

SPRT_API long double __SPRT_ID(fdiml_impl)(long double, long double);
#define __sprt_fdiml __SPRT_ID(fdiml_impl)

SPRT_API double __SPRT_ID(floor_impl)(double);
#define __sprt_floor __SPRT_ID(floor_impl)

SPRT_API float __SPRT_ID(floorf_impl)(float);
#define __sprt_floorf __SPRT_ID(floorf_impl)

SPRT_API long double __SPRT_ID(floorl_impl)(long double);
#define __sprt_floorl __SPRT_ID(floorl_impl)

SPRT_API double __SPRT_ID(fma_impl)(double, double, double);
#define __sprt_fma __SPRT_ID(fma_impl)

SPRT_API float __SPRT_ID(fmaf_impl)(float, float, float);
#define __sprt_fmaf __SPRT_ID(fmaf_impl)

SPRT_API long double __SPRT_ID(fmal_impl)(long double, long double, long double);
#define __sprt_fmal __SPRT_ID(fmal_impl)

SPRT_API double __SPRT_ID(fmax_impl)(double, double);
#define __sprt_fmax __SPRT_ID(fmax_impl)

SPRT_API float __SPRT_ID(fmaxf_impl)(float, float);
#define __sprt_fmaxf __SPRT_ID(fmaxf_impl)

SPRT_API long double __SPRT_ID(fmaxl_impl)(long double, long double);
#define __sprt_fmaxl __SPRT_ID(fmaxl_impl)

SPRT_API double __SPRT_ID(fmin_impl)(double, double);
#define __sprt_fmin __SPRT_ID(fmin_impl)

SPRT_API float __SPRT_ID(fminf_impl)(float, float);
#define __sprt_fminf __SPRT_ID(fminf_impl)

SPRT_API long double __SPRT_ID(fminl_impl)(long double, long double);
#define __sprt_fminl __SPRT_ID(fminl_impl)

SPRT_API double __SPRT_ID(fmod_impl)(double, double);
#define __sprt_fmod __SPRT_ID(fmod_impl)

SPRT_API float __SPRT_ID(fmodf_impl)(float, float);
#define __sprt_fmodf __SPRT_ID(fmodf_impl)

SPRT_API long double __SPRT_ID(fmodl_impl)(long double, long double);
#define __sprt_fmodl __SPRT_ID(fmodl_impl)

SPRT_API double __SPRT_ID(frexp_impl)(double, int *);
#define __sprt_frexp __SPRT_ID(frexp_impl)

SPRT_API float __SPRT_ID(frexpf_impl)(float, int *);
#define __sprt_frexpf __SPRT_ID(frexpf_impl)

SPRT_API long double __SPRT_ID(frexpl_impl)(long double, int *);
#define __sprt_frexpl __SPRT_ID(frexpl_impl)

SPRT_API double __SPRT_ID(hypot_impl)(double, double);
#define __sprt_hypot __SPRT_ID(hypot_impl)

SPRT_API float __SPRT_ID(hypotf_impl)(float, float);
#define __sprt_hypotf __SPRT_ID(hypotf_impl)

SPRT_API long double __SPRT_ID(hypotl_impl)(long double, long double);
#define __sprt_hypotl __SPRT_ID(hypotl_impl)

SPRT_API int __SPRT_ID(ilogb_impl)(double);
#define __sprt_ilogb __SPRT_ID(ilogb_impl)

SPRT_API int __SPRT_ID(ilogbf_impl)(float);
#define __sprt_ilogbf __SPRT_ID(ilogbf_impl)

SPRT_API int __SPRT_ID(ilogbl_impl)(long double);
#define __sprt_ilogbl __SPRT_ID(ilogbl_impl)

SPRT_API double __SPRT_ID(ldexp_impl)(double, int);
#define __sprt_ldexp __SPRT_ID(ldexp_impl)

SPRT_API float __SPRT_ID(ldexpf_impl)(float, int);
#define __sprt_ldexpf __SPRT_ID(ldexpf_impl)

SPRT_API long double __SPRT_ID(ldexpl_impl)(long double, int);
#define __sprt_ldexpl __SPRT_ID(ldexpl_impl)

SPRT_API double __SPRT_ID(lgamma_impl)(double);
#define __sprt_lgamma __SPRT_ID(lgamma_impl)

SPRT_API float __SPRT_ID(lgammaf_impl)(float);
#define __sprt_lgammaf __SPRT_ID(lgammaf_impl)

SPRT_API long double __SPRT_ID(lgammal_impl)(long double);
#define __sprt_lgammal __SPRT_ID(lgammal_impl)

SPRT_API long long __SPRT_ID(llrint_impl)(double);
#define __sprt_llrint __SPRT_ID(llrint_impl)

SPRT_API long long __SPRT_ID(llrintf_impl)(float);
#define __sprt_llrintf __SPRT_ID(llrintf_impl)

SPRT_API long long __SPRT_ID(llrintl_impl)(long double);
#define __sprt_llrintl __SPRT_ID(llrintl_impl)

SPRT_API long long __SPRT_ID(llround_impl)(double);
#define __sprt_llround __SPRT_ID(llround_impl)

SPRT_API long long __SPRT_ID(llroundf_impl)(float);
#define __sprt_llroundf __SPRT_ID(llroundf_impl)

SPRT_API long long __SPRT_ID(llroundl_impl)(long double);
#define __sprt_llroundl __SPRT_ID(llroundl_impl)

SPRT_API double __SPRT_ID(log_impl)(double);
#define __sprt_log __SPRT_ID(log_impl)

SPRT_API float __SPRT_ID(logf_impl)(float);
#define __sprt_logf __SPRT_ID(logf_impl)

SPRT_API long double __SPRT_ID(logl_impl)(long double);
#define __sprt_logl __SPRT_ID(logl_impl)

SPRT_API double __SPRT_ID(log10_impl)(double);
#define __sprt_log10 __SPRT_ID(log10_impl)

SPRT_API float __SPRT_ID(log10f_impl)(float);
#define __sprt_log10f __SPRT_ID(log10f_impl)

SPRT_API long double __SPRT_ID(log10l_impl)(long double);
#define __sprt_log10l __SPRT_ID(log10l_impl)

SPRT_API double __SPRT_ID(log1p_impl)(double);
#define __sprt_log1p __SPRT_ID(log1p_impl)

SPRT_API float __SPRT_ID(log1pf_impl)(float);
#define __sprt_log1pf __SPRT_ID(log1pf_impl)

SPRT_API long double __SPRT_ID(log1pl_impl)(long double);
#define __sprt_log1pl __SPRT_ID(log1pl_impl)

SPRT_API double __SPRT_ID(log2_impl)(double);
#define __sprt_log2 __SPRT_ID(log2_impl)

SPRT_API float __SPRT_ID(log2f_impl)(float);
#define __sprt_log2f __SPRT_ID(log2f_impl)

SPRT_API long double __SPRT_ID(log2l_impl)(long double);
#define __sprt_log2l __SPRT_ID(log2l_impl)

SPRT_API double __SPRT_ID(logb_impl)(double);
#define __sprt_logb __SPRT_ID(logb_impl)

SPRT_API float __SPRT_ID(logbf_impl)(float);
#define __sprt_logbf __SPRT_ID(logbf_impl)

SPRT_API long double __SPRT_ID(logbl_impl)(long double);
#define __sprt_logbl __SPRT_ID(logbl_impl)

SPRT_API long __SPRT_ID(lrint_impl)(double);
#define __sprt_lrint __SPRT_ID(lrint_impl)

SPRT_API long __SPRT_ID(lrintf_impl)(float);
#define __sprt_lrintf __SPRT_ID(lrintf_impl)

SPRT_API long __SPRT_ID(lrintl_impl)(long double);
#define __sprt_lrintl __SPRT_ID(lrintl_impl)

SPRT_API long __SPRT_ID(lround_impl)(double);
#define __sprt_lround __SPRT_ID(lround_impl)

SPRT_API long __SPRT_ID(lroundf_impl)(float);
#define __sprt_lroundf __SPRT_ID(lroundf_impl)

SPRT_API long __SPRT_ID(lroundl_impl)(long double);
#define __sprt_lroundl __SPRT_ID(lroundl_impl)

SPRT_API double __SPRT_ID(modf_impl)(double, double *);
#define __sprt_modf __SPRT_ID(modf_impl)

SPRT_API float __SPRT_ID(modff_impl)(float, float *);
#define __sprt_modff __SPRT_ID(modff_impl)

SPRT_API long double __SPRT_ID(modfl_impl)(long double, long double *);
#define __sprt_modfl __SPRT_ID(modfl_impl)

SPRT_API double __SPRT_ID(nan_impl)(const char *);
#define __sprt_nan __SPRT_ID(nan_impl)

SPRT_API float __SPRT_ID(nanf_impl)(const char *);
#define __sprt_nanf __SPRT_ID(nanf_impl)

SPRT_API long double __SPRT_ID(nanl_impl)(const char *);
#define __sprt_nanl __SPRT_ID(nanl_impl)

SPRT_API double __SPRT_ID(nearbyint_impl)(double);
#define __sprt_nearbyint __SPRT_ID(nearbyint_impl)

SPRT_API float __SPRT_ID(nearbyintf_impl)(float);
#define __sprt_nearbyintf __SPRT_ID(nearbyintf_impl)

SPRT_API long double __SPRT_ID(nearbyintl_impl)(long double);
#define __sprt_nearbyintl __SPRT_ID(nearbyintl_impl)

SPRT_API double __SPRT_ID(nextafter_impl)(double, double);
#define __sprt_nextafter __SPRT_ID(nextafter_impl)

SPRT_API float __SPRT_ID(nextafterf_impl)(float, float);
#define __sprt_nextafterf __SPRT_ID(nextafterf_impl)

SPRT_API long double __SPRT_ID(nextafterl_impl)(long double, long double);
#define __sprt_nextafterl __SPRT_ID(nextafterl_impl)

SPRT_API double __SPRT_ID(nexttoward_impl)(double, long double);
#define __sprt_nexttoward __SPRT_ID(nexttoward_impl)

SPRT_API float __SPRT_ID(nexttowardf_impl)(float, long double);
#define __sprt_nexttowardf __SPRT_ID(nexttowardf_impl)

SPRT_API long double __SPRT_ID(nexttowardl_impl)(long double, long double);
#define __sprt_nexttowardl __SPRT_ID(nexttowardl_impl)

SPRT_API double __SPRT_ID(pow_impl)(double, double);
#define __sprt_pow __SPRT_ID(pow_impl)

SPRT_API float __SPRT_ID(powf_impl)(float, float);
#define __sprt_powf __SPRT_ID(powf_impl)

SPRT_API long double __SPRT_ID(powl_impl)(long double, long double);
#define __sprt_powl __SPRT_ID(powl_impl)

SPRT_API double __SPRT_ID(remainder_impl)(double, double);
#define __sprt_remainder __SPRT_ID(remainder_impl)

SPRT_API float __SPRT_ID(remainderf_impl)(float, float);
#define __sprt_remainderf __SPRT_ID(remainderf_impl)

SPRT_API long double __SPRT_ID(remainderl_impl)(long double, long double);
#define __sprt_remainderl __SPRT_ID(remainderl_impl)

SPRT_API double __SPRT_ID(remquo_impl)(double, double, int *);
#define __sprt_remquo __SPRT_ID(remquo_impl)

SPRT_API float __SPRT_ID(remquof_impl)(float, float, int *);
#define __sprt_remquof __SPRT_ID(remquof_impl)

SPRT_API long double __SPRT_ID(remquol_impl)(long double, long double, int *);
#define __sprt_remquol __SPRT_ID(remquol_impl)

SPRT_API double __SPRT_ID(rint_impl)(double);
#define __sprt_rint __SPRT_ID(rint_impl)

SPRT_API float __SPRT_ID(rintf_impl)(float);
#define __sprt_rintf __SPRT_ID(rintf_impl)

SPRT_API long double __SPRT_ID(rintl_impl)(long double);
#define __sprt_rintl __SPRT_ID(rintl_impl)

SPRT_API double __SPRT_ID(round_impl)(double);
#define __sprt_round __SPRT_ID(round_impl)

SPRT_API float __SPRT_ID(roundf_impl)(float);
#define __sprt_roundf __SPRT_ID(roundf_impl)

SPRT_API long double __SPRT_ID(roundl_impl)(long double);
#define __sprt_roundl __SPRT_ID(roundl_impl)

SPRT_API double __SPRT_ID(scalbln_impl)(double, long);
#define __sprt_scalbln __SPRT_ID(scalbln_impl)

SPRT_API float __SPRT_ID(scalblnf_impl)(float, long);
#define __sprt_scalblnf __SPRT_ID(scalblnf_impl)

SPRT_API long double __SPRT_ID(scalblnl_impl)(long double, long);
#define __sprt_scalblnl __SPRT_ID(scalblnl_impl)

SPRT_API double __SPRT_ID(scalbn_impl)(double, int);
#define __sprt_scalbn __SPRT_ID(scalbn_impl)

SPRT_API float __SPRT_ID(scalbnf_impl)(float, int);
#define __sprt_scalbnf __SPRT_ID(scalbnf_impl)

SPRT_API long double __SPRT_ID(scalbnl_impl)(long double, int);
#define __sprt_scalbnl __SPRT_ID(scalbnl_impl)

SPRT_API double __SPRT_ID(sin_impl)(double);
#define __sprt_sin __SPRT_ID(sin_impl)

SPRT_API float __SPRT_ID(sinf_impl)(float);
#define __sprt_sinf __SPRT_ID(sinf_impl)

SPRT_API long double __SPRT_ID(sinl_impl)(long double);
#define __sprt_sinl __SPRT_ID(sinl_impl)

SPRT_API double __SPRT_ID(sinh_impl)(double);
#define __sprt_sinh __SPRT_ID(sinh_impl)

SPRT_API float __SPRT_ID(sinhf_impl)(float);
#define __sprt_sinhf __SPRT_ID(sinhf_impl)

SPRT_API long double __SPRT_ID(sinhl_impl)(long double);
#define __sprt_sinhl __SPRT_ID(sinhl_impl)

SPRT_API double __SPRT_ID(sqrt_impl)(double);
#define __sprt_sqrt __SPRT_ID(sqrt_impl)

SPRT_API float __SPRT_ID(sqrtf_impl)(float);
#define __sprt_sqrtf __SPRT_ID(sqrtf_impl)

SPRT_API long double __SPRT_ID(sqrtl_impl)(long double);
#define __sprt_sqrtl __SPRT_ID(sqrtl_impl)

SPRT_API double __SPRT_ID(tan_impl)(double);
#define __sprt_tan __SPRT_ID(tan_impl)

SPRT_API float __SPRT_ID(tanf_impl)(float);
#define __sprt_tanf __SPRT_ID(tanf_impl)

SPRT_API long double __SPRT_ID(tanl_impl)(long double);
#define __sprt_tanl __SPRT_ID(tanl_impl)

SPRT_API double __SPRT_ID(tanh_impl)(double);
#define __sprt_tanh __SPRT_ID(tanh_impl)

SPRT_API float __SPRT_ID(tanhf_impl)(float);
#define __sprt_tanhf __SPRT_ID(tanhf_impl)

SPRT_API long double __SPRT_ID(tanhl_impl)(long double);
#define __sprt_tanhl __SPRT_ID(tanhl_impl)

SPRT_API double __SPRT_ID(tgamma_impl)(double);
#define __sprt_tgamma __SPRT_ID(tgamma_impl)

SPRT_API float __SPRT_ID(tgammaf_impl)(float);
#define __sprt_tgammaf __SPRT_ID(tgammaf_impl)

SPRT_API long double __SPRT_ID(tgammal_impl)(long double);
#define __sprt_tgammal __SPRT_ID(tgammal_impl)

SPRT_API double __SPRT_ID(trunc_impl)(double);
#define __sprt_trunc __SPRT_ID(trunc_impl)

SPRT_API float __SPRT_ID(truncf_impl)(float);
#define __sprt_truncf __SPRT_ID(truncf_impl)

SPRT_API long double __SPRT_ID(truncl_impl)(long double);
#define __sprt_truncl __SPRT_ID(truncl_impl)

__SPRT_END_DECL

#endif // CORE_RUNTIME_INCLUDE_C___SPRT_MATH_H_
