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

#include <c/bits/__sprt_float.h>
#include <c/cross/__sprt_math.h>

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

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_nanf)
#define __SPRT_NAN       __builtin_nanf("")
#else
#define __SPRT_NAN       (float)(0.0f/0.0f)
#endif

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_inff)
#define __SPRT_INFINITY      __builtin_inff()
#else
#define __SPRT_INFINITY       (float)(1e+300 * 1e+300)
#endif

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_huge_val)
#define __SPRT_HUGE_VAL  __builtin_huge_val()
#else
#define __SPRT_HUGE_VAL ((double)__SPRT_INFINITY)
#endif

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_huge_valf)
#define __SPRT_HUGE_VALF  __builtin_huge_valf()
#else
#define __SPRT_HUGE_VALF ((float)__SPRT_INFINITY)
#endif

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_huge_vall)
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

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_fpclassify)
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

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_isfinite)
#define __sprt_isfinite(v) __builtin_isfinite(v)
#else
#define __sprt_isfinite(x) ( \
	sizeof(x) == sizeof(float) ? (__SPRT_FLOAT_BITS(x) & 0x7fff'ffff) < 0x7f80'0000 : \
	sizeof(x) == sizeof(double) ? (__SPRT_DOUBLE_BITS(x) & -1ULL>>1) < 0x7ffULL<<52 : \
	__sprt_fpclassify(x) > __SPRT_FP_INFINITE)
#endif

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_isnan)
#define __sprt_isnan(v) __builtin_isnan(v)
#else
#define __sprt_isnan(x) ( \
	sizeof(x) == sizeof(float) ? (__SPRT_FLOAT_BITS(x) & 0x7fff'ffff) > 0x7f80'0000 : \
	sizeof(x) == sizeof(double) ? (__SPRT_DOUBLE_BITS(x) & -1ULL>>1) > 0x7ffULL<<52 : \
	__sprt_fpclassify(x) == __SPRT_FP_NAN)
#endif

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_isinf)
#define __sprt_isinf(v) __builtin_isinf(v)
#else
#define __sprt_isinf(x) ( \
	sizeof(x) == sizeof(float) ? (__SPRT_FLOAT_BITS(x) & 0x7fff'ffff) == 0x7f80'0000 : \
	sizeof(x) == sizeof(double) ? __SPRT_DOUBLE_BITS(x) & -1ULL>>1) == 0x7ffULL<<52 : \
	__sprt_fpclassify(x) == __SPRT_FP_INFINITE)
#endif

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_isnormal)
#define __sprt_isnormal(v) __builtin_isnormal(v)
#else
#define __sprt_isnormal(x) ( \
	sizeof(x) == sizeof(float) ? ((__SPRT_FLOAT_BITS(x)+0x0080'0000) & 0x7fff'ffff) >= 0x0100'0000 : \
	sizeof(x) == sizeof(double) ? ((__SPRT_DOUBLE_BITS(x)+(1ULL<<52)) & -1ULL>>1) >= 1ULL<<53 : \
	__sprt_fpclassify(x) == __SPRT_FP_NORMAL)
#endif


#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_signbit)
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

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_isgreater)
#define __sprt_isgreater(a, b) __builtin_isgreater(a, b)
#endif

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_isgreaterequal)
#define __sprt_isgreaterequal(a, b) __builtin_isgreaterequal(a, b)
#endif

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_isless)
#define __sprt_isless(a, b) __builtin_isless(a, b)
#endif

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_islessequal)
#define __sprt_islessequal(a, b) __builtin_islessequal(a, b)
#endif

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_islessgreater)
#define __sprt_islessgreater(a, b) __builtin_islessgreater(a, b)
#endif

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_isunordered)
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

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_acos)
inline double __SPRT_ID(acos)(double value) { return __builtin_acos(value); }
#else
#define __sprt_acos __SPRT_ID(acos_impl)
#endif


SPRT_API float __SPRT_ID(acosf_impl)(float);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_acosf)
inline float __SPRT_ID(acosf)(float value) { return __builtin_acosf(value); }
#else
#define __sprt_acosf __SPRT_ID(acosf_impl)
#endif


SPRT_API long double __SPRT_ID(acosl_impl)(long double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_acosl)
inline long double __SPRT_ID(acosl)(long double value) { return __builtin_acosl(value); }
#else
#define __sprt_acosl __SPRT_ID(acosl_impl)
#endif


SPRT_API double __SPRT_ID(acosh_impl)(double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_acosh)
inline double __SPRT_ID(acosh)(double value) { return __builtin_acosh(value); }
#else
#define __sprt_acosh __SPRT_ID(acosh_impl)
#endif


SPRT_API float __SPRT_ID(acoshf_impl)(float);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_acoshf)
inline float __SPRT_ID(acoshf)(float value) { return __builtin_acoshf(value); }
#else
#define __sprt_acoshf __SPRT_ID(acoshf_impl)
#endif


SPRT_API long double __SPRT_ID(acoshl_impl)(long double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_acoshl)
inline long double __SPRT_ID(acoshl)(long double value) { return __builtin_acoshl(value); }
#else
#define __sprt_acoshl __SPRT_ID(acoshl_impl)
#endif


SPRT_API double __SPRT_ID(asin_impl)(double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_asin)
inline double __SPRT_ID(asin)(double value) { return __builtin_asin(value); }
#else
#define __sprt_asin __SPRT_ID(asin_impl)
#endif


SPRT_API float __SPRT_ID(asinf_impl)(float);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_asinf)
inline float __SPRT_ID(asinf)(float value) { return __builtin_asinf(value); }
#else
#define __sprt_asinf __SPRT_ID(asinf_impl)
#endif


SPRT_API long double __SPRT_ID(asinl_impl)(long double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_asinl)
inline long double __SPRT_ID(asinl)(long double value) { return __builtin_asinl(value); }
#else
#define __sprt_asinl __SPRT_ID(asinl_impl)
#endif


SPRT_API double __SPRT_ID(asinh_impl)(double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_asinh)
inline double __SPRT_ID(asinh)(double value) { return __builtin_asinh(value); }
#else
#define __sprt_asinh __SPRT_ID(asinh_impl)
#endif


SPRT_API float __SPRT_ID(asinhf_impl)(float);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_asinhf)
inline float __SPRT_ID(asinhf)(float value) { return __builtin_asinhf(value); }
#else
#define __sprt_asinhf __SPRT_ID(asinhf_impl)
#endif


SPRT_API long double __SPRT_ID(asinhl_impl)(long double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_asinhl)
inline long double __SPRT_ID(asinhl)(long double value) { return __builtin_asinhl(value); }
#else
#define __sprt_asinhl __SPRT_ID(asinhl_impl)
#endif


SPRT_API double __SPRT_ID(atan_impl)(double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_atan)
inline double __SPRT_ID(atan)(double value) { return __builtin_atan(value); }
#else
#define __sprt_atan __SPRT_ID(atan_impl)
#endif


SPRT_API float __SPRT_ID(atanf_impl)(float);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_atanf)
inline float __SPRT_ID(atanf)(float value) { return __builtin_atanf(value); }
#else
#define __sprt_atanf __SPRT_ID(atanf_impl)
#endif


SPRT_API long double __SPRT_ID(atanl_impl)(long double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_atanl)
inline long double __SPRT_ID(atanl)(long double value) { return __builtin_atanl(value); }
#else
#define __sprt_atanl __SPRT_ID(atanl_impl)
#endif


SPRT_API double __SPRT_ID(atan2_impl)(double, double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_atan2)
inline double __SPRT_ID(atan2)(double a, double b) { return __builtin_atan2(a, b); }
#else
#define __sprt_atan2 __SPRT_ID(atan2_impl)
#endif


SPRT_API float __SPRT_ID(atan2f_impl)(float, float);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_atan2f)
inline float __SPRT_ID(atan2f)(float a, float b) { return __builtin_atan2f(a, b); }
#else
#define __sprt_atan2f __SPRT_ID(atan2f_impl)
#endif


SPRT_API long double __SPRT_ID(atan2l_impl)(long double, long double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_atan2l)
inline long double __SPRT_ID(atan2l)(long double a, long double b) {
	return __builtin_atan2l(a, b);
}
#else
#define __sprt_atan2l __SPRT_ID(atan2l_impl)
#endif


SPRT_API double __SPRT_ID(atanh_impl)(double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_atanh)
inline double __SPRT_ID(atanh)(double value) { return __builtin_atanh(value); }
#else
#define __sprt_atanh __SPRT_ID(atanh_impl)
#endif


SPRT_API float __SPRT_ID(atanhf_impl)(float);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_atanhf)
inline float __SPRT_ID(atanhf)(float value) { return __builtin_atanhf(value); }
#else
#define __sprt_atanhf __SPRT_ID(atanhf_impl)
#endif


SPRT_API long double __SPRT_ID(atanhl_impl)(long double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_atanhl)
inline long double __SPRT_ID(atanhl)(long double value) { return __builtin_atanhl(value); }
#else
#define __sprt_atanhl __SPRT_ID(atanhl_impl)
#endif


SPRT_API double __SPRT_ID(cbrt_impl)(double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_cbrt)
inline double __SPRT_ID(cbrt)(double value) { return __builtin_cbrt(value); }
#else
#define __sprt_cbrt __SPRT_ID(cbrt_impl)
#endif


SPRT_API float __SPRT_ID(cbrtf_impl)(float);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_cbrtf)
inline float __SPRT_ID(cbrtf)(float value) { return __builtin_cbrtf(value); }
#else
#define __sprt_cbrtf __SPRT_ID(cbrtf_impl)
#endif


SPRT_API long double __SPRT_ID(cbrtl_impl)(long double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_cbrtl)
inline long double __SPRT_ID(cbrtl)(long double value) { return __builtin_cbrtl(value); }
#else
#define __sprt_cbrtl __SPRT_ID(cbrtl_impl)
#endif


SPRT_API double __SPRT_ID(ceil_impl)(double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_ceil)
inline double __SPRT_ID(ceil)(double value) { return __builtin_ceil(value); }
#else
#define __sprt_ceil __SPRT_ID(ceil_impl)
#endif


SPRT_API float __SPRT_ID(ceilf_impl)(float);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_ceilf)
inline float __SPRT_ID(ceilf)(float value) { return __builtin_ceilf(value); }
#else
#define __sprt_ceilf __SPRT_ID(ceilf_impl)
#endif


SPRT_API long double __SPRT_ID(ceill_impl)(long double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_ceill)
inline long double __SPRT_ID(ceill)(long double value) { return __builtin_ceill(value); }
#else
#define __sprt_ceill __SPRT_ID(ceill_impl)
#endif


SPRT_API double __SPRT_ID(copysign_impl)(double, double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_copysign)
inline double __SPRT_ID(copysign)(double a, double b) { return __builtin_copysign(a, b); }
#else
#define __sprt_copysign __SPRT_ID(copysign_impl)
#endif


SPRT_API float __SPRT_ID(copysignf_impl)(float, float);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_copysignf)
inline float __SPRT_ID(copysignf)(float a, float b) { return __builtin_copysignf(a, b); }
#else
#define __sprt_copysignf __SPRT_ID(copysignf_impl)
#endif


SPRT_API long double __SPRT_ID(copysignl_impl)(long double, long double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_copysignl)
inline long double __SPRT_ID(copysignl)(long double a, long double b) {
	return __builtin_copysignl(a, b);
}
#else
#define __sprt_copysignl __SPRT_ID(copysignl_impl)
#endif


SPRT_API double __SPRT_ID(cos_impl)(double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_cos)
inline double __SPRT_ID(cos)(double value) { return __builtin_cos(value); }
#else
#define __sprt_cos __SPRT_ID(cos_impl)
#endif


SPRT_API float __SPRT_ID(cosf_impl)(float);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_cosf)
inline float __SPRT_ID(cosf)(float value) { return __builtin_cosf(value); }
#else
#define __sprt_cosf __SPRT_ID(cosf_impl)
#endif


SPRT_API long double __SPRT_ID(cosl_impl)(long double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_cosl)
inline long double __SPRT_ID(cosl)(long double value) { return __builtin_cosl(value); }
#else
#define __sprt_cosl __SPRT_ID(cosl_impl)
#endif


SPRT_API double __SPRT_ID(cosh_impl)(double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_cosh)
inline double __SPRT_ID(cosh)(double value) { return __builtin_cosh(value); }
#else
#define __sprt_cosh __SPRT_ID(cosh_impl)
#endif


SPRT_API float __SPRT_ID(coshf_impl)(float);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_coshf)
inline float __SPRT_ID(coshf)(float value) { return __builtin_coshf(value); }
#else
#define __sprt_coshf __SPRT_ID(coshf_impl)
#endif


SPRT_API long double __SPRT_ID(coshl_impl)(long double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_coshl)
inline long double __SPRT_ID(coshl)(long double value) { return __builtin_coshl(value); }
#else
#define __sprt_coshl __SPRT_ID(coshl_impl)
#endif


SPRT_API double __SPRT_ID(erf_impl)(double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_erf)
inline double __SPRT_ID(erf)(double value) { return __builtin_erf(value); }
#else
#define __sprt_erf __SPRT_ID(erf_impl)
#endif


SPRT_API float __SPRT_ID(erff_impl)(float);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_erff)
inline float __SPRT_ID(erff)(float value) { return __builtin_erff(value); }
#else
#define __sprt_erff __SPRT_ID(erff_impl)
#endif


SPRT_API long double __SPRT_ID(erfl_impl)(long double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_erfl)
inline long double __SPRT_ID(erfl)(long double value) { return __builtin_erfl(value); }
#else
#define __sprt_erfl __SPRT_ID(erfl_impl)
#endif


SPRT_API double __SPRT_ID(erfc_impl)(double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_erfc)
inline double __SPRT_ID(erfc)(double value) { return __builtin_erfc(value); }
#else
#define __sprt_erfc __SPRT_ID(erfc_impl)
#endif


SPRT_API float __SPRT_ID(erfcf_impl)(float);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_erfcf)
inline float __SPRT_ID(erfcf)(float value) { return __builtin_erfcf(value); }
#else
#define __sprt_erfcf __SPRT_ID(erfcf_impl)
#endif


SPRT_API long double __SPRT_ID(erfcl_impl)(long double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_erfcl)
inline long double __SPRT_ID(erfcl)(long double value) { return __builtin_erfcl(value); }
#else
#define __sprt_erfcl __SPRT_ID(erfcl_impl)
#endif


SPRT_API double __SPRT_ID(exp_impl)(double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_exp)
inline double __SPRT_ID(exp)(double value) { return __builtin_exp(value); }
#else
#define __sprt_exp __SPRT_ID(exp_impl)
#endif


SPRT_API float __SPRT_ID(expf_impl)(float);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_expf)
inline float __SPRT_ID(expf)(float value) { return __builtin_expf(value); }
#else
#define __sprt_expf __SPRT_ID(expf_impl)
#endif


SPRT_API long double __SPRT_ID(expl_impl)(long double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_expl)
inline long double __SPRT_ID(expl)(long double value) { return __builtin_expl(value); }
#else
#define __sprt_expl __SPRT_ID(expl_impl)
#endif


SPRT_API double __SPRT_ID(exp2_impl)(double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_exp2)
inline double __SPRT_ID(exp2)(double value) { return __builtin_exp2(value); }
#else
#define __sprt_exp2 __SPRT_ID(exp2_impl)
#endif


SPRT_API float __SPRT_ID(exp2f_impl)(float);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_exp2f)
inline float __SPRT_ID(exp2f)(float value) { return __builtin_exp2f(value); }
#else
#define __sprt_exp2f __SPRT_ID(exp2f_impl)
#endif


SPRT_API long double __SPRT_ID(exp2l_impl)(long double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_exp2l)
inline long double __SPRT_ID(exp2l)(long double value) { return __builtin_exp2l(value); }
#else
#define __sprt_exp2l __SPRT_ID(exp2l_impl)
#endif


SPRT_API double __SPRT_ID(expm1_impl)(double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_expm1)
inline double __SPRT_ID(expm1)(double value) { return __builtin_expm1(value); }
#else
#define __sprt_expm1 __SPRT_ID(expm1_impl)
#endif


SPRT_API float __SPRT_ID(expm1f_impl)(float);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_expm1f)
inline float __SPRT_ID(expm1f)(float value) { return __builtin_expm1f(value); }
#else
#define __sprt_expm1f __SPRT_ID(expm1f_impl)
#endif


SPRT_API long double __SPRT_ID(expm1l_impl)(long double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_expm1l)
inline long double __SPRT_ID(expm1l)(long double value) { return __builtin_expm1l(value); }
#else
#define __sprt_expm1l __SPRT_ID(expm1l_impl)
#endif


SPRT_API double __SPRT_ID(fabs_impl)(double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_fabs)
inline double __SPRT_ID(fabs)(double value) { return __builtin_fabs(value); }
#else
#define __sprt_fabs __SPRT_ID(fabs_impl)
#endif


SPRT_API float __SPRT_ID(fabsf_impl)(float);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_fabsf)
inline float __SPRT_ID(fabsf)(float value) { return __builtin_fabsf(value); }
#else
#define __sprt_fabsf __SPRT_ID(fabsf_impl)
#endif


SPRT_API long double __SPRT_ID(fabsl_impl)(long double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_fabsl)
inline long double __SPRT_ID(fabsl)(long double value) { return __builtin_fabsl(value); }
#else
#define __sprt_fabsl __SPRT_ID(fabsl_impl)
#endif


SPRT_API double __SPRT_ID(fdim_impl)(double, double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_fdim)
inline double __SPRT_ID(fdim)(double a, double b) { return __builtin_fdim(a, b); }
#else
#define __sprt_fdim __SPRT_ID(fdim_impl)
#endif


SPRT_API float __SPRT_ID(fdimf_impl)(float, float);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_fdimf)
inline float __SPRT_ID(fdimf)(float a, float b) { return __builtin_fdimf(a, b); }
#else
#define __sprt_fdimf __SPRT_ID(fdimf_impl)
#endif


SPRT_API long double __SPRT_ID(fdiml_impl)(long double, long double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_fdiml)
inline long double __SPRT_ID(fdiml)(long double a, long double b) { return __builtin_fdiml(a, b); }
#else
#define __sprt_fdiml __SPRT_ID(fdiml_impl)
#endif


SPRT_API double __SPRT_ID(floor_impl)(double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_floor)
inline double __SPRT_ID(floor)(double value) { return __builtin_floor(value); }
#else
#define __sprt_floor __SPRT_ID(floor_impl)
#endif


SPRT_API float __SPRT_ID(floorf_impl)(float);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_floorf)
inline float __SPRT_ID(floorf)(float value) { return __builtin_floorf(value); }
#else
#define __sprt_floorf __SPRT_ID(floorf_impl)
#endif


SPRT_API long double __SPRT_ID(floorl_impl)(long double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_floorl)
inline long double __SPRT_ID(floorl)(long double value) { return __builtin_floorl(value); }
#else
#define __sprt_floorl __SPRT_ID(floorl_impl)
#endif


SPRT_API double __SPRT_ID(fma_impl)(double, double, double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_fma)
inline double __SPRT_ID(fma)(double a, double b, double c) { return __builtin_fma(a, b, c); }
#else
#define __sprt_fma __SPRT_ID(fma_impl)
#endif


SPRT_API float __SPRT_ID(fmaf_impl)(float, float, float);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_fmaf)
inline float __SPRT_ID(fmaf)(float a, float b, float c) { return __builtin_fmaf(a, b, c); }
#else
#define __sprt_fmaf __SPRT_ID(fmaf_impl)
#endif


SPRT_API long double __SPRT_ID(fmal_impl)(long double, long double, long double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_fmal)
inline long double __SPRT_ID(fmal)(long double a, long double b, long double c) {
	return __builtin_fmal(a, b, c);
}
#else
#define __sprt_fmal __SPRT_ID(fmal_impl)
#endif


SPRT_API double __SPRT_ID(fmax_impl)(double, double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_fmax)
inline double __SPRT_ID(fmax)(double a, double b) { return __builtin_fmax(a, b); }
#else
#define __sprt_fmax __SPRT_ID(fmax_impl)
#endif


SPRT_API float __SPRT_ID(fmaxf_impl)(float, float);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_fmaxf)
inline float __SPRT_ID(fmaxf)(float a, float b) { return __builtin_fmaxf(a, b); }
#else
#define __sprt_fmaxf __SPRT_ID(fmaxf_impl)
#endif


SPRT_API long double __SPRT_ID(fmaxl_impl)(long double, long double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_fmaxl)
inline long double __SPRT_ID(fmaxl)(long double a, long double b) { return __builtin_fmaxl(a, b); }
#else
#define __sprt_fmaxl __SPRT_ID(fmaxl_impl)
#endif


SPRT_API double __SPRT_ID(fmin_impl)(double, double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_fmin)
inline double __SPRT_ID(fmin)(double a, double b) { return __builtin_fmin(a, b); }
#else
#define __sprt_fmin __SPRT_ID(fmin_impl)
#endif


SPRT_API float __SPRT_ID(fminf_impl)(float, float);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_fminf)
inline float __SPRT_ID(fminf)(float a, float b) { return __builtin_fminf(a, b); }
#else
#define __sprt_fminf __SPRT_ID(fminf_impl)
#endif


SPRT_API long double __SPRT_ID(fminl_impl)(long double, long double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_fminl)
inline long double __SPRT_ID(fminl)(long double a, long double b) { return __builtin_fminl(a, b); }
#else
#define __sprt_fminl __SPRT_ID(fminl_impl)
#endif


SPRT_API double __SPRT_ID(fmod_impl)(double, double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_fmod)
inline double __SPRT_ID(fmod)(double a, double b) { return __builtin_fmod(a, b); }
#else
#define __sprt_fmod __SPRT_ID(fmod_impl)
#endif


SPRT_API float __SPRT_ID(fmodf_impl)(float, float);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_fmodf)
inline float __SPRT_ID(fmodf)(float a, float b) { return __builtin_fmodf(a, b); }
#else
#define __sprt_fmodf __SPRT_ID(fmodf_impl)
#endif


SPRT_API long double __SPRT_ID(fmodl_impl)(long double, long double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_fmodl)
inline long double __SPRT_ID(fmodl)(long double a, long double b) { return __builtin_fmodl(a, b); }
#else
#define __sprt_fmodl __SPRT_ID(fmodl_impl)
#endif


SPRT_API double __SPRT_ID(frexp_impl)(double, int *);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_frexp)
inline double __SPRT_ID(frexp)(double a, int *b) { return __builtin_frexp(a, b); }
#else
#define __sprt_frexp __SPRT_ID(frexp_impl)
#endif


SPRT_API float __SPRT_ID(frexpf_impl)(float, int *);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_frexpf)
inline float __SPRT_ID(frexpf)(float a, int *b) { return __builtin_frexpf(a, b); }
#else
#define __sprt_frexpf __SPRT_ID(frexpf_impl)
#endif


SPRT_API long double __SPRT_ID(frexpl_impl)(long double, int *);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_frexpl)
inline long double __SPRT_ID(frexpl)(long double a, int *b) { return __builtin_frexpl(a, b); }
#else
#define __sprt_frexpl __SPRT_ID(frexpl_impl)
#endif


SPRT_API double __SPRT_ID(hypot_impl)(double, double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_hypot)
inline double __SPRT_ID(hypot)(double a, double b) { return __builtin_hypot(a, b); }
#else
#define __sprt_hypot __SPRT_ID(hypot_impl)
#endif


SPRT_API float __SPRT_ID(hypotf_impl)(float, float);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_hypotf)
inline float __SPRT_ID(hypotf)(float a, float b) { return __builtin_hypotf(a, b); }
#else
#define __sprt_hypotf __SPRT_ID(hypotf_impl)
#endif


SPRT_API long double __SPRT_ID(hypotl_impl)(long double, long double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_hypotl)
inline long double __SPRT_ID(hypotl)(long double a, long double b) {
	return __builtin_hypotl(a, b);
}
#else
#define __sprt_hypotl __SPRT_ID(hypotl_impl)
#endif


SPRT_API int __SPRT_ID(ilogb_impl)(double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_ilogb)
inline int __SPRT_ID(ilogb)(double value) { return __builtin_ilogb(value); }
#else
#define __sprt_ilogb __SPRT_ID(ilogb_impl)
#endif


SPRT_API int __SPRT_ID(ilogbf_impl)(float);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_ilogbf)
inline int __SPRT_ID(ilogbf)(float value) { return __builtin_ilogbf(value); }
#else
#define __sprt_ilogbf __SPRT_ID(ilogbf_impl)
#endif


SPRT_API int __SPRT_ID(ilogbl_impl)(long double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_ilogbl)
inline int __SPRT_ID(ilogbl)(long double value) { return __builtin_ilogbl(value); }
#else
#define __sprt_ilogbl __SPRT_ID(ilogbl_impl)
#endif


SPRT_API double __SPRT_ID(ldexp_impl)(double, int);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_ldexp)
inline double __SPRT_ID(ldexp)(double a, int b) { return __builtin_ldexp(a, b); }
#else
#define __sprt_ldexp __SPRT_ID(ldexp_impl)
#endif


SPRT_API float __SPRT_ID(ldexpf_impl)(float, int);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_ldexpf)
inline float __SPRT_ID(ldexpf)(float a, int b) { return __builtin_ldexpf(a, b); }
#else
#define __sprt_ldexpf __SPRT_ID(ldexpf_impl)
#endif


SPRT_API long double __SPRT_ID(ldexpl_impl)(long double, int);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_ldexpl)
inline long double __SPRT_ID(ldexpl)(long double a, int b) { return __builtin_ldexpl(a, b); }
#else
#define __sprt_ldexpl __SPRT_ID(ldexpl_impl)
#endif


SPRT_API double __SPRT_ID(lgamma_impl)(double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_lgamma)
inline double __SPRT_ID(lgamma)(double value) { return __builtin_lgamma(value); }
#else
#define __sprt_lgamma __SPRT_ID(lgamma_impl)
#endif


SPRT_API float __SPRT_ID(lgammaf_impl)(float);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_lgammaf)
inline float __SPRT_ID(lgammaf)(float value) { return __builtin_lgammaf(value); }
#else
#define __sprt_lgammaf __SPRT_ID(lgammaf_impl)
#endif


SPRT_API long double __SPRT_ID(lgammal_impl)(long double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_lgammal)
inline long double __SPRT_ID(lgammal)(long double value) { return __builtin_lgammal(value); }
#else
#define __sprt_lgammal __SPRT_ID(lgammal_impl)
#endif


SPRT_API long long __SPRT_ID(llrint_impl)(double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_llrint)
inline long long __SPRT_ID(llrint)(double value) { return __builtin_llrint(value); }
#else
#define __sprt_llrint __SPRT_ID(llrint_impl)
#endif


SPRT_API long long __SPRT_ID(llrintf_impl)(float);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_llrintf)
inline long long __SPRT_ID(llrintf)(float value) { return __builtin_llrintf(value); }
#else
#define __sprt_llrintf __SPRT_ID(llrintf_impl)
#endif


SPRT_API long long __SPRT_ID(llrintl_impl)(long double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_llrintl)
inline long long __SPRT_ID(llrintl)(long double value) { return __builtin_llrintl(value); }
#else
#define __sprt_llrintl __SPRT_ID(llrintl_impl)
#endif


SPRT_API long long __SPRT_ID(llround_impl)(double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_llround)
inline long long __SPRT_ID(llround)(double value) { return __builtin_llround(value); }
#else
#define __sprt_llround __SPRT_ID(llround_impl)
#endif


SPRT_API long long __SPRT_ID(llroundf_impl)(float);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_llroundf)
inline long long __SPRT_ID(llroundf)(float value) { return __builtin_llroundf(value); }
#else
#define __sprt_llroundf __SPRT_ID(llroundf_impl)
#endif


SPRT_API long long __SPRT_ID(llroundl_impl)(long double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_llroundl)
inline long long __SPRT_ID(llroundl)(long double value) { return __builtin_llroundl(value); }
#else
#define __sprt_llroundl __SPRT_ID(llroundl_impl)
#endif


SPRT_API double __SPRT_ID(log_impl)(double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_log)
inline double __SPRT_ID(log)(double value) { return __builtin_log(value); }
#else
#define __sprt_log __SPRT_ID(log_impl)
#endif


SPRT_API float __SPRT_ID(logf_impl)(float);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_logf)
inline float __SPRT_ID(logf)(float value) { return __builtin_logf(value); }
#else
#define __sprt_logf __SPRT_ID(logf_impl)
#endif


SPRT_API long double __SPRT_ID(logl_impl)(long double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_logl)
inline long double __SPRT_ID(logl)(long double value) { return __builtin_logl(value); }
#else
#define __sprt_logl __SPRT_ID(logl_impl)
#endif


SPRT_API double __SPRT_ID(log10_impl)(double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_log10)
inline double __SPRT_ID(log10)(double value) { return __builtin_log10(value); }
#else
#define __sprt_log10 __SPRT_ID(log10_impl)
#endif


SPRT_API float __SPRT_ID(log10f_impl)(float);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_log10f)
inline float __SPRT_ID(log10f)(float value) { return __builtin_log10f(value); }
#else
#define __sprt_log10f __SPRT_ID(log10f_impl)
#endif


SPRT_API long double __SPRT_ID(log10l_impl)(long double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_log10l)
inline long double __SPRT_ID(log10l)(long double value) { return __builtin_log10l(value); }
#else
#define __sprt_log10l __SPRT_ID(log10l_impl)
#endif


SPRT_API double __SPRT_ID(log1p_impl)(double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_log1p)
inline double __SPRT_ID(log1p)(double value) { return __builtin_log1p(value); }
#else
#define __sprt_log1p __SPRT_ID(log1p_impl)
#endif


SPRT_API float __SPRT_ID(log1pf_impl)(float);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_log1pf)
inline float __SPRT_ID(log1pf)(float value) { return __builtin_log1pf(value); }
#else
#define __sprt_log1pf __SPRT_ID(log1pf_impl)
#endif


SPRT_API long double __SPRT_ID(log1pl_impl)(long double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_log1pl)
inline long double __SPRT_ID(log1pl)(long double value) { return __builtin_log1pl(value); }
#else
#define __sprt_log1pl __SPRT_ID(log1pl_impl)
#endif


SPRT_API double __SPRT_ID(log2_impl)(double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_log2)
inline double __SPRT_ID(log2)(double value) { return __builtin_log2(value); }
#else
#define __sprt_log2 __SPRT_ID(log2_impl)
#endif


SPRT_API float __SPRT_ID(log2f_impl)(float);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_log2f)
inline float __SPRT_ID(log2f)(float value) { return __builtin_log2f(value); }
#else
#define __sprt_log2f __SPRT_ID(log2f_impl)
#endif


SPRT_API long double __SPRT_ID(log2l_impl)(long double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_log2l)
inline long double __SPRT_ID(log2l)(long double value) { return __builtin_log2l(value); }
#else
#define __sprt_log2l __SPRT_ID(log2l_impl)
#endif


SPRT_API double __SPRT_ID(logb_impl)(double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_logb)
inline double __SPRT_ID(logb)(double value) { return __builtin_logb(value); }
#else
#define __sprt_logb __SPRT_ID(logb_impl)
#endif


SPRT_API float __SPRT_ID(logbf_impl)(float);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_logbf)
inline float __SPRT_ID(logbf)(float value) { return __builtin_logbf(value); }
#else
#define __sprt_logbf __SPRT_ID(logbf_impl)
#endif


SPRT_API long double __SPRT_ID(logbl_impl)(long double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_logbl)
inline long double __SPRT_ID(logbl)(long double value) { return __builtin_logbl(value); }
#else
#define __sprt_logbl __SPRT_ID(logbl_impl)
#endif


SPRT_API long __SPRT_ID(lrint_impl)(double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_lrint)
inline long __SPRT_ID(lrint)(double value) { return __builtin_lrint(value); }
#else
#define __sprt_lrint __SPRT_ID(lrint_impl)
#endif


SPRT_API long __SPRT_ID(lrintf_impl)(float);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_lrintf)
inline long __SPRT_ID(lrintf)(float value) { return __builtin_lrintf(value); }
#else
#define __sprt_lrintf __SPRT_ID(lrintf_impl)
#endif


SPRT_API long __SPRT_ID(lrintl_impl)(long double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_lrintl)
inline long __SPRT_ID(lrintl)(long double value) { return __builtin_lrintl(value); }
#else
#define __sprt_lrintl __SPRT_ID(lrintl_impl)
#endif


SPRT_API long __SPRT_ID(lround_impl)(double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_lround)
inline long __SPRT_ID(lround)(double value) { return __builtin_lround(value); }
#else
#define __sprt_lround __SPRT_ID(lround_impl)
#endif


SPRT_API long __SPRT_ID(lroundf_impl)(float);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_lroundf)
inline long __SPRT_ID(lroundf)(float value) { return __builtin_lroundf(value); }
#else
#define __sprt_lroundf __SPRT_ID(lroundf_impl)
#endif


SPRT_API long __SPRT_ID(lroundl_impl)(long double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_lroundl)
inline long __SPRT_ID(lroundl)(long double value) { return __builtin_lroundl(value); }
#else
#define __sprt_lroundl __SPRT_ID(lroundl_impl)
#endif


SPRT_API double __SPRT_ID(modf_impl)(double, double *);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_modf)
inline double __SPRT_ID(modf)(double a, double *b) { return __builtin_modf(a, b); }
#else
#define __sprt_modf __SPRT_ID(modf_impl)
#endif


SPRT_API float __SPRT_ID(modff_impl)(float, float *);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_modff)
inline float __SPRT_ID(modff)(float a, float *b) { return __builtin_modff(a, b); }
#else
#define __sprt_modff __SPRT_ID(modff_impl)
#endif


SPRT_API long double __SPRT_ID(modfl_impl)(long double, long double *);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_modfl)
inline long double __SPRT_ID(modfl)(long double a, long double *b) { return __builtin_modfl(a, b); }
#else
#define __sprt_modfl __SPRT_ID(modfl_impl)
#endif


SPRT_API double __SPRT_ID(nan_impl)(const char *);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_nan)
inline double __SPRT_ID(nan)(const char *value) { return __builtin_nan(value); }
#else
#define __sprt_nan __SPRT_ID(nan_impl)
#endif


SPRT_API float __SPRT_ID(nanf_impl)(const char *);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_nanf)
inline float __SPRT_ID(nanf)(const char *value) { return __builtin_nanf(value); }
#else
#define __sprt_nanf __SPRT_ID(nanf_impl)
#endif


SPRT_API long double __SPRT_ID(nanl_impl)(const char *);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_nanl)
inline long double __SPRT_ID(nanl)(const char *value) { return __builtin_nanl(value); }
#else
#define __sprt_nanl __SPRT_ID(nanl_impl)
#endif


SPRT_API double __SPRT_ID(nearbyint_impl)(double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_nearbyint)
inline double __SPRT_ID(nearbyint)(double value) { return __builtin_nearbyint(value); }
#else
#define __sprt_nearbyint __SPRT_ID(nearbyint_impl)
#endif


SPRT_API float __SPRT_ID(nearbyintf_impl)(float);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_nearbyintf)
inline float __SPRT_ID(nearbyintf)(float value) { return __builtin_nearbyintf(value); }
#else
#define __sprt_nearbyintf __SPRT_ID(nearbyintf_impl)
#endif


SPRT_API long double __SPRT_ID(nearbyintl_impl)(long double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_nearbyintl)
inline long double __SPRT_ID(nearbyintl)(long double value) { return __builtin_nearbyintl(value); }
#else
#define __sprt_nearbyintl __SPRT_ID(nearbyintl_impl)
#endif


SPRT_API double __SPRT_ID(nextafter_impl)(double, double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_nextafter)
inline double __SPRT_ID(nextafter)(double a, double b) { return __builtin_nextafter(a, b); }
#else
#define __sprt_nextafter __SPRT_ID(nextafter_impl)
#endif


SPRT_API float __SPRT_ID(nextafterf_impl)(float, float);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_nextafterf)
inline float __SPRT_ID(nextafterf)(float a, float b) { return __builtin_nextafterf(a, b); }
#else
#define __sprt_nextafterf __SPRT_ID(nextafterf_impl)
#endif


SPRT_API long double __SPRT_ID(nextafterl_impl)(long double, long double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_nextafterl)
inline long double __SPRT_ID(nextafterl)(long double a, long double b) {
	return __builtin_nextafterl(a, b);
}
#else
#define __sprt_nextafterl __SPRT_ID(nextafterl_impl)
#endif


SPRT_API double __SPRT_ID(nexttoward_impl)(double, long double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_nexttoward)
inline double __SPRT_ID(nexttoward)(double a, long double b) { return __builtin_nexttoward(a, b); }
#else
#define __sprt_nexttoward __SPRT_ID(nexttoward_impl)
#endif


SPRT_API float __SPRT_ID(nexttowardf_impl)(float, long double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_nexttowardf)
inline float __SPRT_ID(nexttowardf)(float a, long double b) { return __builtin_nexttowardf(a, b); }
#else
#define __sprt_nexttowardf __SPRT_ID(nexttowardf_impl)
#endif


SPRT_API long double __SPRT_ID(nexttowardl_impl)(long double, long double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_nexttowardl)
inline long double __SPRT_ID(nexttowardl)(long double a, long double b) {
	return __builtin_nexttowardl(a, b);
}
#else
#define __sprt_nexttowardl __SPRT_ID(nexttowardl_impl)
#endif


SPRT_API double __SPRT_ID(pow_impl)(double, double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_pow)
inline double __SPRT_ID(pow)(double a, double b) { return __builtin_pow(a, b); }
#else
#define __sprt_pow __SPRT_ID(pow_impl)
#endif


SPRT_API float __SPRT_ID(powf_impl)(float, float);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_powf)
inline float __SPRT_ID(powf)(float a, float b) { return __builtin_powf(a, b); }
#else
#define __sprt_powf __SPRT_ID(powf_impl)
#endif


SPRT_API long double __SPRT_ID(powl_impl)(long double, long double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_powl)
inline long double __SPRT_ID(powl)(long double a, long double b) { return __builtin_powl(a, b); }
#else
#define __sprt_powl __SPRT_ID(powl_impl)
#endif


SPRT_API double __SPRT_ID(remainder_impl)(double, double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_remainder)
inline double __SPRT_ID(remainder)(double a, double b) { return __builtin_remainder(a, b); }
#else
#define __sprt_remainder __SPRT_ID(remainder_impl)
#endif


SPRT_API float __SPRT_ID(remainderf_impl)(float, float);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_remainderf)
inline float __SPRT_ID(remainderf)(float a, float b) { return __builtin_remainderf(a, b); }
#else
#define __sprt_remainderf __SPRT_ID(remainderf_impl)
#endif


SPRT_API long double __SPRT_ID(remainderl_impl)(long double, long double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_remainderl)
inline long double __SPRT_ID(remainderl)(long double a, long double b) {
	return __builtin_remainderl(a, b);
}
#else
#define __sprt_remainderl __SPRT_ID(remainderl_impl)
#endif


SPRT_API double __SPRT_ID(remquo_impl)(double, double, int *);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_remquo)
inline double __SPRT_ID(remquo)(double a, double b, int *c) { return __builtin_remquo(a, b, c); }
#else
#define __sprt_remquo __SPRT_ID(remquo_impl)
#endif


SPRT_API float __SPRT_ID(remquof_impl)(float, float, int *);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_remquof)
inline float __SPRT_ID(remquof)(float a, float b, int *c) { return __builtin_remquof(a, b, c); }
#else
#define __sprt_remquof __SPRT_ID(remquof_impl)
#endif


SPRT_API long double __SPRT_ID(remquol_impl)(long double, long double, int *);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_remquol)
inline long double __SPRT_ID(remquol)(long double a, long double b, int *c) {
	return __builtin_remquol(a, b, c);
}
#else
#define __sprt_remquol __SPRT_ID(remquol_impl)
#endif


SPRT_API double __SPRT_ID(rint_impl)(double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_rint)
inline double __SPRT_ID(rint)(double value) { return __builtin_rint(value); }
#else
#define __sprt_rint __SPRT_ID(rint_impl)
#endif


SPRT_API float __SPRT_ID(rintf_impl)(float);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_rintf)
inline float __SPRT_ID(rintf)(float value) { return __builtin_rintf(value); }
#else
#define __sprt_rintf __SPRT_ID(rintf_impl)
#endif


SPRT_API long double __SPRT_ID(rintl_impl)(long double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_rintl)
inline long double __SPRT_ID(rintl)(long double value) { return __builtin_rintl(value); }
#else
#define __sprt_rintl __SPRT_ID(rintl_impl)
#endif


SPRT_API double __SPRT_ID(round_impl)(double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_round)
inline double __SPRT_ID(round)(double value) { return __builtin_round(value); }
#else
#define __sprt_round __SPRT_ID(round_impl)
#endif


SPRT_API float __SPRT_ID(roundf_impl)(float);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_roundf)
inline float __SPRT_ID(roundf)(float value) { return __builtin_roundf(value); }
#else
#define __sprt_roundf __SPRT_ID(roundf_impl)
#endif


SPRT_API long double __SPRT_ID(roundl_impl)(long double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_roundl)
inline long double __SPRT_ID(roundl)(long double value) { return __builtin_roundl(value); }
#else
#define __sprt_roundl __SPRT_ID(roundl_impl)
#endif


SPRT_API double __SPRT_ID(scalbln_impl)(double, long);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_scalbln)
inline double __SPRT_ID(scalbln)(double a, long b) { return __builtin_scalbln(a, b); }
#else
#define __sprt_scalbln __SPRT_ID(scalbln_impl)
#endif


SPRT_API float __SPRT_ID(scalblnf_impl)(float, long);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_scalblnf)
inline float __SPRT_ID(scalblnf)(float a, long b) { return __builtin_scalblnf(a, b); }
#else
#define __sprt_scalblnf __SPRT_ID(scalblnf_impl)
#endif


SPRT_API long double __SPRT_ID(scalblnl_impl)(long double, long);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_scalblnl)
inline long double __SPRT_ID(scalblnl)(long double a, long b) { return __builtin_scalblnl(a, b); }
#else
#define __sprt_scalblnl __SPRT_ID(scalblnl_impl)
#endif


SPRT_API double __SPRT_ID(scalbn_impl)(double, int);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_scalbn)
inline double __SPRT_ID(scalbn)(double a, int b) { return __builtin_scalbn(a, b); }
#else
#define __sprt_scalbn __SPRT_ID(scalbn_impl)
#endif


SPRT_API float __SPRT_ID(scalbnf_impl)(float, int);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_scalbnf)
inline float __SPRT_ID(scalbnf)(float a, int b) { return __builtin_scalbnf(a, b); }
#else
#define __sprt_scalbnf __SPRT_ID(scalbnf_impl)
#endif


SPRT_API long double __SPRT_ID(scalbnl_impl)(long double, int);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_scalbnl)
inline long double __SPRT_ID(scalbnl)(long double a, int b) { return __builtin_scalbnl(a, b); }
#else
#define __sprt_scalbnl __SPRT_ID(scalbnl_impl)
#endif


SPRT_API double __SPRT_ID(sin_impl)(double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_sin)
inline double __SPRT_ID(sin)(double value) { return __builtin_sin(value); }
#else
#define __sprt_sin __SPRT_ID(sin_impl)
#endif


SPRT_API float __SPRT_ID(sinf_impl)(float);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_sinf)
inline float __SPRT_ID(sinf)(float value) { return __builtin_sinf(value); }
#else
#define __sprt_sinf __SPRT_ID(sinf_impl)
#endif


SPRT_API long double __SPRT_ID(sinl_impl)(long double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_sinl)
inline long double __SPRT_ID(sinl)(long double value) { return __builtin_sinl(value); }
#else
#define __sprt_sinl __SPRT_ID(sinl_impl)
#endif


SPRT_API double __SPRT_ID(sinh_impl)(double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_sinh)
inline double __SPRT_ID(sinh)(double value) { return __builtin_sinh(value); }
#else
#define __sprt_sinh __SPRT_ID(sinh_impl)
#endif


SPRT_API float __SPRT_ID(sinhf_impl)(float);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_sinhf)
inline float __SPRT_ID(sinhf)(float value) { return __builtin_sinhf(value); }
#else
#define __sprt_sinhf __SPRT_ID(sinhf_impl)
#endif


SPRT_API long double __SPRT_ID(sinhl_impl)(long double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_sinhl)
inline long double __SPRT_ID(sinhl)(long double value) { return __builtin_sinhl(value); }
#else
#define __sprt_sinhl __SPRT_ID(sinhl_impl)
#endif


SPRT_API double __SPRT_ID(sqrt_impl)(double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_sqrt)
inline double __SPRT_ID(sqrt)(double value) { return __builtin_sqrt(value); }
#else
#define __sprt_sqrt __SPRT_ID(sqrt_impl)
#endif


SPRT_API float __SPRT_ID(sqrtf_impl)(float);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_sqrtf)
inline float __SPRT_ID(sqrtf)(float value) { return __builtin_sqrtf(value); }
#else
#define __sprt_sqrtf __SPRT_ID(sqrtf_impl)
#endif


SPRT_API long double __SPRT_ID(sqrtl_impl)(long double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_sqrtl)
inline long double __SPRT_ID(sqrtl)(long double value) { return __builtin_sqrtl(value); }
#else
#define __sprt_sqrtl __SPRT_ID(sqrtl_impl)
#endif


SPRT_API double __SPRT_ID(tan_impl)(double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_tan)
inline double __SPRT_ID(tan)(double value) { return __builtin_tan(value); }
#else
#define __sprt_tan __SPRT_ID(tan_impl)
#endif


SPRT_API float __SPRT_ID(tanf_impl)(float);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_tanf)
inline float __SPRT_ID(tanf)(float value) { return __builtin_tanf(value); }
#else
#define __sprt_tanf __SPRT_ID(tanf_impl)
#endif


SPRT_API long double __SPRT_ID(tanl_impl)(long double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_tanl)
inline long double __SPRT_ID(tanl)(long double value) { return __builtin_tanl(value); }
#else
#define __sprt_tanl __SPRT_ID(tanl_impl)
#endif


SPRT_API double __SPRT_ID(tanh_impl)(double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_tanh)
inline double __SPRT_ID(tanh)(double value) { return __builtin_tanh(value); }
#else
#define __sprt_tanh __SPRT_ID(tanh_impl)
#endif


SPRT_API float __SPRT_ID(tanhf_impl)(float);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_tanhf)
inline float __SPRT_ID(tanhf)(float value) { return __builtin_tanhf(value); }
#else
#define __sprt_tanhf __SPRT_ID(tanhf_impl)
#endif


SPRT_API long double __SPRT_ID(tanhl_impl)(long double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_tanhl)
inline long double __SPRT_ID(tanhl)(long double value) { return __builtin_tanhl(value); }
#else
#define __sprt_tanhl __SPRT_ID(tanhl_impl)
#endif


SPRT_API double __SPRT_ID(tgamma_impl)(double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_tgamma)
inline double __SPRT_ID(tgamma)(double value) { return __builtin_tgamma(value); }
#else
#define __sprt_tgamma __SPRT_ID(tgamma_impl)
#endif


SPRT_API float __SPRT_ID(tgammaf_impl)(float);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_tgammaf)
inline float __SPRT_ID(tgammaf)(float value) { return __builtin_tgammaf(value); }
#else
#define __sprt_tgammaf __SPRT_ID(tgammaf_impl)
#endif


SPRT_API long double __SPRT_ID(tgammal_impl)(long double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_tgammal)
inline long double __SPRT_ID(tgammal)(long double value) { return __builtin_tgammal(value); }
#else
#define __sprt_tgammal __SPRT_ID(tgammal_impl)
#endif


SPRT_API double __SPRT_ID(trunc_impl)(double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_trunc)
inline double __SPRT_ID(trunc)(double value) { return __builtin_trunc(value); }
#else
#define __sprt_trunc __SPRT_ID(trunc_impl)
#endif


SPRT_API float __SPRT_ID(truncf_impl)(float);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_truncf)
inline float __SPRT_ID(truncf)(float value) { return __builtin_truncf(value); }
#else
#define __sprt_truncf __SPRT_ID(truncf_impl)
#endif


SPRT_API long double __SPRT_ID(truncl_impl)(long double);

#if __SPRT_CONFIG_BUILTIN_INLINES && __SPRT_HAS_BUILTIN(__builtin_truncl)
inline long double __SPRT_ID(truncl)(long double value) { return __builtin_truncl(value); }
#else
#define __sprt_truncl __SPRT_ID(truncl_impl)
#endif


__SPRT_END_DECL

#endif // CORE_RUNTIME_INCLUDE_C___SPRT_MATH_H_
