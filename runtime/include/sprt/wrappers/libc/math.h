#ifndef CORE_RUNTIME_INCLUDE_SPRT_WRAPPERS_LIBC_MATH_H_
#define CORE_RUNTIME_INCLUDE_SPRT_WRAPPERS_LIBC_MATH_H_

#include <sprt/c/__sprt_math.h>

#if __STDC_HOSTED__ == 0 || !defined(__SPRT_BUILD)
#define NAN __SPRT_NAN
#define INFINITY __SPRT_INFINITY
#define HUGE_VAL __SPRT_HUGE_VAL
#define HUGE_VALF __SPRT_HUGE_VALF
#define HUGE_VALL  __SPRT_HUGE_VALL

#ifndef FP_NAN
#define FP_NAN __SPRT_FP_NAN
#define FP_INFINITE __SPRT_FP_INFINITE
#define FP_ZERO __SPRT_FP_ZERO
#define FP_SUBNORMAL __SPRT_FP_SUBNORMAL
#define FP_NORMAL __SPRT_FP_NORMAL
#endif

#define FP_ILOGBNAN __SPRT_FP_ILOGBNAN
#define FP_ILOGB0 __SPRT_FP_ILOGB0

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

#ifndef __cplusplus
typedef __SPRT_ID(float_t) float_t;
typedef __SPRT_ID(double_t) double_t;
#endif

#endif // __STDC_HOSTED__ == 0 || !defined(__SPRT_BUILD)


#ifdef __cplusplus

#include <sprt/runtime/math.h>
#include <sprt/cxx/detail/promote.h>

namespace sprt {
inline namespace _cmath {

using float_t = float;
using double_t = double;

#if __SPRT_HAS_BUILTIN(__builtin_huge_val)
static constexpr auto _HUGE_VAL = __builtin_huge_val();
#else
static constexpr auto _HUGE_VAL = __SPRT_HUGE_VAL;
#endif

#if __SPRT_HAS_BUILTIN(__builtin_huge_valf)
static constexpr auto _HUGE_VALF = __builtin_huge_valf();
#else
static constexpr auto _HUGE_VALF = __SPRT_HUGE_VALF;
#endif

#if __SPRT_HAS_BUILTIN(__builtin_huge_vall)
static constexpr auto _HUGE_VALL = __builtin_huge_vall();
#else
static constexpr auto _HUGE_VALL = __SPRT_HUGE_VALL;
#endif


#if __SPRT_HAS_BUILTIN(__builtin_nanf)
static constexpr auto _NAN = __builtin_nanf("");
#else
static constexpr auto _NAN = __SPRT_NAN;
#endif

#if __SPRT_HAS_BUILTIN(__builtin_inff)
static constexpr auto _INFINITY = __builtin_inff();
#else
static constexpr auto _INFINITY = __SPRT_INFINITY;
#endif


static constexpr auto _FP_NAN = __SPRT_FP_NAN;
static constexpr auto _FP_INFINITE = __SPRT_FP_INFINITE;
static constexpr auto _FP_ZERO = __SPRT_FP_ZERO;
static constexpr auto _FP_SUBNORMAL = __SPRT_FP_SUBNORMAL;
static constexpr auto _FP_NORMAL = __SPRT_FP_NORMAL;
static constexpr auto _FP_ILOGB0 = __SPRT_FP_ILOGB0;
static constexpr auto _FP_ILOGBNAN = __SPRT_FP_ILOGBNAN;
static constexpr auto _MATH_ERRNO = __SPRT_MATH_ERRNO;
static constexpr auto _MATH_ERREXCEPT = __SPRT_MATH_ERREXCEPT;

#if SPRT_MACOS
// _math_errhandling is a function in Mac SDK
#define _math_errhandling __SPRT_math_errhandling
#else
static constexpr auto _math_errhandling = __SPRT_math_errhandling;
#endif

SPRT_FORCEINLINE auto acos(float x) { return __builtin_acosf(x); }
SPRT_FORCEINLINE auto acosf(float x) { return __builtin_acosf(x); }
SPRT_FORCEINLINE auto acos(double x) { return __builtin_acos(x); }
SPRT_FORCEINLINE auto acos(long double x) { return __builtin_acosl(x); }
SPRT_FORCEINLINE auto acosl(long double x) { return __builtin_acosl(x); }

SPRT_FORCEINLINE auto asin(float x) { return __builtin_asinf(x); }
SPRT_FORCEINLINE auto asinf(float x) { return __builtin_asinf(x); }
SPRT_FORCEINLINE auto asin(double x) { return __builtin_asin(x); }
SPRT_FORCEINLINE auto asin(long double x) { return __builtin_asinl(x); }
SPRT_FORCEINLINE auto asinl(long double x) { return __builtin_asinl(x); }

SPRT_FORCEINLINE auto atan(float x) { return __builtin_atanf(x); }
SPRT_FORCEINLINE auto atanf(float x) { return __builtin_atanf(x); }
SPRT_FORCEINLINE auto atan(double x) { return __builtin_atan(x); }
SPRT_FORCEINLINE auto atan(long double x) { return __builtin_atanl(x); }
SPRT_FORCEINLINE auto atanl(long double x) { return __builtin_atanl(x); }

SPRT_FORCEINLINE auto atan2(float x, float y) { return __builtin_atan2f(x, y); }
SPRT_FORCEINLINE auto atan2f(float x, float y) { return __builtin_atan2f(x, y); }
SPRT_FORCEINLINE auto atan2(double x, double y) { return __builtin_atan2(x, y); }
SPRT_FORCEINLINE auto atan2(long double x, long double y) { return __builtin_atan2l(x, y); }
SPRT_FORCEINLINE auto atan2l(long double x, long double y) { return __builtin_atan2l(x, y); }

SPRT_FORCEINLINE auto cos(float x) { return __builtin_cosf(x); }
SPRT_FORCEINLINE auto cosf(float x) { return __builtin_cosf(x); }
SPRT_FORCEINLINE auto cos(double x) { return __builtin_cos(x); }
SPRT_FORCEINLINE auto cos(long double x) { return __builtin_cosl(x); }
SPRT_FORCEINLINE auto cosl(long double x) { return __builtin_cosl(x); }

SPRT_FORCEINLINE auto sin(float x) { return __builtin_sinf(x); }
SPRT_FORCEINLINE auto sinf(float x) { return __builtin_sinf(x); }
SPRT_FORCEINLINE auto sin(double x) { return __builtin_sin(x); }
SPRT_FORCEINLINE auto sin(long double x) { return __builtin_sinl(x); }
SPRT_FORCEINLINE auto sinl(long double x) { return __builtin_sinl(x); }

SPRT_FORCEINLINE auto tan(float x) { return __builtin_tanf(x); }
SPRT_FORCEINLINE auto tanf(float x) { return __builtin_tanf(x); }
SPRT_FORCEINLINE auto tan(double x) { return __builtin_tan(x); }
SPRT_FORCEINLINE auto tan(long double x) { return __builtin_tanl(x); }
SPRT_FORCEINLINE auto tanl(long double x) { return __builtin_tanl(x); }

SPRT_FORCEINLINE auto acosh(float x) { return __builtin_acoshf(x); }
SPRT_FORCEINLINE auto acoshf(float x) { return __builtin_acoshf(x); }
SPRT_FORCEINLINE auto acosh(double x) { return __builtin_acosh(x); }
SPRT_FORCEINLINE auto acosh(long double x) { return __builtin_acoshl(x); }
SPRT_FORCEINLINE auto acoshl(long double x) { return __builtin_acoshl(x); }

SPRT_FORCEINLINE auto asinh(float x) { return __builtin_asinhf(x); }
SPRT_FORCEINLINE auto asinhf(float x) { return __builtin_asinhf(x); }
SPRT_FORCEINLINE auto asinh(double x) { return __builtin_asinh(x); }
SPRT_FORCEINLINE auto asinh(long double x) { return __builtin_asinhl(x); }
SPRT_FORCEINLINE auto asinhl(long double x) { return __builtin_asinhl(x); }

SPRT_FORCEINLINE auto atanh(float x) { return __builtin_atanhf(x); }
SPRT_FORCEINLINE auto atanhf(float x) { return __builtin_atanhf(x); }
SPRT_FORCEINLINE auto atanh(double x) { return __builtin_atanh(x); }
SPRT_FORCEINLINE auto atanh(long double x) { return __builtin_atanhl(x); }
SPRT_FORCEINLINE auto atanhl(long double x) { return __builtin_atanhl(x); }

SPRT_FORCEINLINE auto cosh(float x) { return __builtin_coshf(x); }
SPRT_FORCEINLINE auto coshf(float x) { return __builtin_coshf(x); }
SPRT_FORCEINLINE auto cosh(double x) { return __builtin_cosh(x); }
SPRT_FORCEINLINE auto cosh(long double x) { return __builtin_coshl(x); }
SPRT_FORCEINLINE auto coshl(long double x) { return __builtin_coshl(x); }

SPRT_FORCEINLINE auto sinh(float x) { return __builtin_sinhf(x); }
SPRT_FORCEINLINE auto sinhf(float x) { return __builtin_sinhf(x); }
SPRT_FORCEINLINE auto sinh(double x) { return __builtin_sinh(x); }
SPRT_FORCEINLINE auto sinh(long double x) { return __builtin_sinhl(x); }
SPRT_FORCEINLINE auto sinhl(long double x) { return __builtin_sinhl(x); }

SPRT_FORCEINLINE auto tanh(float x) { return __builtin_tanhf(x); }
SPRT_FORCEINLINE auto tanhf(float x) { return __builtin_tanhf(x); }
SPRT_FORCEINLINE auto tanh(double x) { return __builtin_tanh(x); }
SPRT_FORCEINLINE auto tanh(long double x) { return __builtin_tanhl(x); }
SPRT_FORCEINLINE auto tanhl(long double x) { return __builtin_tanhl(x); }

SPRT_FORCEINLINE auto exp(float x) { return __builtin_expf(x); }
SPRT_FORCEINLINE auto expf(float x) { return __builtin_expf(x); }
SPRT_FORCEINLINE auto exp(double x) { return __builtin_exp(x); }
SPRT_FORCEINLINE auto exp(long double x) { return __builtin_expl(x); }
SPRT_FORCEINLINE auto expl(long double x) { return __builtin_expl(x); }

SPRT_FORCEINLINE auto exp2(float x) { return __builtin_exp2f(x); }
SPRT_FORCEINLINE auto exp2f(float x) { return __builtin_exp2f(x); }
SPRT_FORCEINLINE auto exp2(double x) { return __builtin_exp2(x); }
SPRT_FORCEINLINE auto exp2(long double x) { return __builtin_exp2l(x); }
SPRT_FORCEINLINE auto exp2l(long double x) { return __builtin_exp2l(x); }

SPRT_FORCEINLINE auto expm1(float x) { return __builtin_expm1f(x); }
SPRT_FORCEINLINE auto expm1f(float x) { return __builtin_expm1f(x); }
SPRT_FORCEINLINE auto expm1(double x) { return __builtin_expm1(x); }
SPRT_FORCEINLINE auto expm1(long double x) { return __builtin_expm1l(x); }
SPRT_FORCEINLINE auto expm1l(long double x) { return __builtin_expm1l(x); }

SPRT_FORCEINLINE auto frexp(float x, int *exp) { return __builtin_frexpf(x, exp); }
SPRT_FORCEINLINE auto frexpf(float x, int *exp) { return __builtin_frexpf(x, exp); }
SPRT_FORCEINLINE auto frexp(double x, int *exp) { return __builtin_frexp(x, exp); }
SPRT_FORCEINLINE auto frexp(long double x, int *exp) { return __builtin_frexpl(x, exp); }
SPRT_FORCEINLINE auto frexpl(long double x, int *exp) { return __builtin_frexpl(x, exp); }

SPRT_FORCEINLINE auto ilogb(float x) { return __builtin_ilogbf(x); }
SPRT_FORCEINLINE auto ilogbf(float x) { return __builtin_ilogbf(x); }
SPRT_FORCEINLINE auto ilogb(double x) { return __builtin_ilogb(x); }
SPRT_FORCEINLINE auto ilogb(long double x) { return __builtin_ilogbl(x); }
SPRT_FORCEINLINE auto ilogbl(long double x) { return __builtin_ilogbl(x); }

SPRT_FORCEINLINE auto ldexp(float x, int exp) { return __builtin_ldexpf(x, exp); }
SPRT_FORCEINLINE auto ldexpf(float x, int exp) { return __builtin_ldexpf(x, exp); }
SPRT_FORCEINLINE auto ldexp(double x, int exp) { return __builtin_ldexp(x, exp); }
SPRT_FORCEINLINE auto ldexp(long double x, int exp) { return __builtin_ldexpl(x, exp); }
SPRT_FORCEINLINE auto ldexpl(long double x, int exp) { return __builtin_ldexpl(x, exp); }

SPRT_FORCEINLINE auto log(float x) { return __builtin_logf(x); }
SPRT_FORCEINLINE auto logf(float x) { return __builtin_logf(x); }
SPRT_FORCEINLINE auto log(double x) { return __builtin_log(x); }
SPRT_FORCEINLINE auto log(long double x) { return __builtin_logl(x); }
SPRT_FORCEINLINE auto logl(long double x) { return __builtin_logl(x); }

SPRT_FORCEINLINE auto log10(float x) { return __builtin_log10f(x); }
SPRT_FORCEINLINE auto log10f(float x) { return __builtin_log10f(x); }
SPRT_FORCEINLINE auto log10(double x) { return __builtin_log10(x); }
SPRT_FORCEINLINE auto log10(long double x) { return __builtin_log10l(x); }
SPRT_FORCEINLINE auto log10l(long double x) { return __builtin_log10l(x); }

SPRT_FORCEINLINE auto log1p(float x) { return __builtin_log1pf(x); }
SPRT_FORCEINLINE auto log1pf(float x) { return __builtin_log1pf(x); }
SPRT_FORCEINLINE auto log1p(double x) { return __builtin_log1p(x); }
SPRT_FORCEINLINE auto log1p(long double x) { return __builtin_log1pl(x); }
SPRT_FORCEINLINE auto log1pl(long double x) { return __builtin_log1pl(x); }

SPRT_FORCEINLINE auto log2(float x) { return __builtin_log2f(x); }
SPRT_FORCEINLINE auto log2f(float x) { return __builtin_log2f(x); }
SPRT_FORCEINLINE auto log2(double x) { return __builtin_log2(x); }
SPRT_FORCEINLINE auto log2(long double x) { return __builtin_log2l(x); }
SPRT_FORCEINLINE auto log2l(long double x) { return __builtin_log2l(x); }

SPRT_FORCEINLINE auto logb(float x) { return __builtin_logbf(x); }
SPRT_FORCEINLINE auto logbf(float x) { return __builtin_logbf(x); }
SPRT_FORCEINLINE auto logb(double x) { return __builtin_logb(x); }
SPRT_FORCEINLINE auto logb(long double x) { return __builtin_logbl(x); }
SPRT_FORCEINLINE auto logbl(long double x) { return __builtin_logbl(x); }

SPRT_FORCEINLINE auto modf(float x, float *iptr) { return __builtin_modff(x, iptr); }
SPRT_FORCEINLINE auto modff(float x, float *iptr) { return __builtin_modff(x, iptr); }
SPRT_FORCEINLINE auto modf(double x, double *iptr) { return __builtin_modf(x, iptr); }
SPRT_FORCEINLINE auto modf(long double x, long double *iptr) { return __builtin_modfl(x, iptr); }
SPRT_FORCEINLINE auto modfl(long double x, long double *iptr) { return __builtin_modfl(x, iptr); }

SPRT_FORCEINLINE auto scalbn(float x, int n) { return __builtin_scalbnf(x, n); }
SPRT_FORCEINLINE auto scalbnf(float x, int n) { return __builtin_scalbnf(x, n); }
SPRT_FORCEINLINE auto scalbn(double x, int n) { return __builtin_scalbn(x, n); }
SPRT_FORCEINLINE auto scalbn(long double x, int n) { return __builtin_scalbnl(x, n); }
SPRT_FORCEINLINE auto scalbnl(long double x, int n) { return __builtin_scalbnl(x, n); }

SPRT_FORCEINLINE auto scalbln(float x, long int n) { return __builtin_scalblnf(x, n); }
SPRT_FORCEINLINE auto scalblnf(float x, long int n) { return __builtin_scalblnf(x, n); }
SPRT_FORCEINLINE auto scalbln(double x, long int n) { return __builtin_scalbln(x, n); }
SPRT_FORCEINLINE auto scalbln(long double x, long int n) { return __builtin_scalblnl(x, n); }
SPRT_FORCEINLINE auto scalblnl(long double x, long int n) { return __builtin_scalblnl(x, n); }

SPRT_FORCEINLINE auto cbrt(float x) { return __builtin_cbrtf(x); }
SPRT_FORCEINLINE auto cbrtf(float x) { return __builtin_cbrtf(x); }
SPRT_FORCEINLINE auto cbrt(double x) { return __builtin_cbrt(x); }
SPRT_FORCEINLINE auto cbrt(long double x) { return __builtin_cbrtl(x); }
SPRT_FORCEINLINE auto cbrtl(long double x) { return __builtin_cbrtl(x); }

SPRT_FORCEINLINE auto abs(int x) { return __builtin_abs(x); }
SPRT_FORCEINLINE auto abs(long int x) { return __builtin_labs(x); }
SPRT_FORCEINLINE auto abs(long long int x) { return __builtin_llabs(x); }
SPRT_FORCEINLINE auto abs(float x) { return __builtin_fabsf(x); }
SPRT_FORCEINLINE auto absf(float x) { return __builtin_fabsf(x); }
SPRT_FORCEINLINE auto abs(double x) { return __builtin_fabs(x); }
SPRT_FORCEINLINE auto abs(long double x) { return __builtin_fabsl(x); }
SPRT_FORCEINLINE auto absl(long double x) { return __builtin_fabsl(x); }

SPRT_FORCEINLINE auto fabs(int x) { return __builtin_fabs(double(x)); }
SPRT_FORCEINLINE auto fabs(long int x) { return __builtin_fabs(double(x)); }
SPRT_FORCEINLINE auto fabs(long long int x) { return __builtin_fabs(double(x)); }
SPRT_FORCEINLINE auto fabs(float x) { return __builtin_fabsf(x); }
SPRT_FORCEINLINE auto fabsf(float x) { return __builtin_fabsf(x); }
SPRT_FORCEINLINE auto fabs(double x) { return __builtin_fabs(x); }
SPRT_FORCEINLINE auto fabs(long double x) { return __builtin_fabsl(x); }
SPRT_FORCEINLINE auto fabsl(long double x) { return __builtin_fabsl(x); }

SPRT_FORCEINLINE auto hypot(float x, float y) { return __builtin_hypotf(x, y); }
SPRT_FORCEINLINE auto hypotf(float x, float y) { return __builtin_hypotf(x, y); }
SPRT_FORCEINLINE auto hypot(double x, double y) { return __builtin_hypot(x, y); }
SPRT_FORCEINLINE auto hypot(long double x, long double y) { return __builtin_hypotl(x, y); }
SPRT_FORCEINLINE auto hypotl(long double x, long double y) { return __builtin_hypotl(x, y); }

SPRT_FORCEINLINE auto pow(float x, float y) { return __builtin_powf(x, y); }
SPRT_FORCEINLINE auto powf(float x, float y) { return __builtin_powf(x, y); }
SPRT_FORCEINLINE auto pow(double x, double y) { return __builtin_pow(x, y); }
SPRT_FORCEINLINE auto pow(long double x, long double y) { return __builtin_powl(x, y); }
SPRT_FORCEINLINE auto powl(long double x, long double y) { return __builtin_powl(x, y); }

SPRT_FORCEINLINE auto sqrt(float x) { return __builtin_sqrtf(x); }
SPRT_FORCEINLINE auto sqrtf(float x) { return __builtin_sqrtf(x); }
SPRT_FORCEINLINE auto sqrt(double x) { return __builtin_sqrt(x); }
SPRT_FORCEINLINE auto sqrt(long double x) { return __builtin_sqrtl(x); }
SPRT_FORCEINLINE auto sqrtl(long double x) { return __builtin_sqrtl(x); }

SPRT_FORCEINLINE auto erf(float x) { return __builtin_erff(x); }
SPRT_FORCEINLINE auto erff(float x) { return __builtin_erff(x); }
SPRT_FORCEINLINE auto erf(double x) { return __builtin_erf(x); }
SPRT_FORCEINLINE auto erf(long double x) { return __builtin_erfl(x); }
SPRT_FORCEINLINE auto erfl(long double x) { return __builtin_erfl(x); }

SPRT_FORCEINLINE auto erfc(float x) { return __builtin_erfcf(x); }
SPRT_FORCEINLINE auto erfcf(float x) { return __builtin_erfcf(x); }
SPRT_FORCEINLINE auto erfc(double x) { return __builtin_erfc(x); }
SPRT_FORCEINLINE auto erfc(long double x) { return __builtin_erfcl(x); }
SPRT_FORCEINLINE auto erfcl(long double x) { return __builtin_erfcl(x); }

SPRT_FORCEINLINE auto lgamma(float x) { return __builtin_lgammaf(x); }
SPRT_FORCEINLINE auto lgammaf(float x) { return __builtin_lgammaf(x); }
SPRT_FORCEINLINE auto lgamma(double x) { return __builtin_lgamma(x); }
SPRT_FORCEINLINE auto lgamma(long double x) { return __builtin_lgammal(x); }
SPRT_FORCEINLINE auto lgammal(long double x) { return __builtin_lgammal(x); }

SPRT_FORCEINLINE auto tgamma(float x) { return __builtin_tgammaf(x); }
SPRT_FORCEINLINE auto tgammaf(float x) { return __builtin_tgammaf(x); }
SPRT_FORCEINLINE auto tgamma(double x) { return __builtin_tgamma(x); }
SPRT_FORCEINLINE auto tgamma(long double x) { return __builtin_tgammal(x); }
SPRT_FORCEINLINE auto tgammal(long double x) { return __builtin_tgammal(x); }

SPRT_FORCEINLINE auto ceil(float x) { return __builtin_ceilf(x); }
SPRT_FORCEINLINE auto ceilf(float x) { return __builtin_ceilf(x); }
SPRT_FORCEINLINE auto ceil(double x) { return __builtin_ceil(x); }
SPRT_FORCEINLINE auto ceil(long double x) { return __builtin_ceill(x); }
SPRT_FORCEINLINE auto ceill(long double x) { return __builtin_ceill(x); }

SPRT_FORCEINLINE auto floor(float x) { return __builtin_floorf(x); }
SPRT_FORCEINLINE auto floorf(float x) { return __builtin_floorf(x); }
SPRT_FORCEINLINE auto floor(double x) { return __builtin_floor(x); }
SPRT_FORCEINLINE auto floor(long double x) { return __builtin_floorl(x); }
SPRT_FORCEINLINE auto floorl(long double x) { return __builtin_floorl(x); }

SPRT_FORCEINLINE auto nearbyint(float x) { return __builtin_nearbyintf(x); }
SPRT_FORCEINLINE auto nearbyintf(float x) { return __builtin_nearbyintf(x); }
SPRT_FORCEINLINE auto nearbyint(double x) { return __builtin_nearbyint(x); }
SPRT_FORCEINLINE auto nearbyint(long double x) { return __builtin_nearbyintl(x); }
SPRT_FORCEINLINE auto nearbyintl(long double x) { return __builtin_nearbyintl(x); }

SPRT_FORCEINLINE auto rint(float x) { return __builtin_rintf(x); }
SPRT_FORCEINLINE auto rintf(float x) { return __builtin_rintf(x); }
SPRT_FORCEINLINE auto rint(double x) { return __builtin_rint(x); }
SPRT_FORCEINLINE auto rint(long double x) { return __builtin_rintl(x); }
SPRT_FORCEINLINE auto rintl(long double x) { return __builtin_rintl(x); }

SPRT_FORCEINLINE auto lrint(float x) { return __builtin_lrintf(x); }
SPRT_FORCEINLINE auto lrintf(float x) { return __builtin_lrintf(x); }
SPRT_FORCEINLINE auto lrint(double x) { return __builtin_lrint(x); }
SPRT_FORCEINLINE auto lrint(long double x) { return __builtin_lrintl(x); }
SPRT_FORCEINLINE auto lrintl(long double x) { return __builtin_lrintl(x); }

SPRT_FORCEINLINE auto llrint(float x) { return __builtin_llrintf(x); }
SPRT_FORCEINLINE auto llrintf(float x) { return __builtin_llrintf(x); }
SPRT_FORCEINLINE auto llrint(double x) { return __builtin_llrint(x); }
SPRT_FORCEINLINE auto llrint(long double x) { return __builtin_llrintl(x); }
SPRT_FORCEINLINE auto llrintl(long double x) { return __builtin_llrintl(x); }

SPRT_FORCEINLINE auto round(float x) { return __builtin_roundf(x); }
SPRT_FORCEINLINE auto roundf(float x) { return __builtin_roundf(x); }
SPRT_FORCEINLINE auto round(double x) { return __builtin_round(x); }
SPRT_FORCEINLINE auto round(long double x) { return __builtin_roundl(x); }
SPRT_FORCEINLINE auto roundl(long double x) { return __builtin_roundl(x); }

SPRT_FORCEINLINE auto lround(float x) { return __builtin_lroundf(x); }
SPRT_FORCEINLINE auto lroundf(float x) { return __builtin_lroundf(x); }
SPRT_FORCEINLINE auto lround(double x) { return __builtin_lround(x); }
SPRT_FORCEINLINE auto lround(long double x) { return __builtin_lroundl(x); }
SPRT_FORCEINLINE auto lroundl(long double x) { return __builtin_lroundl(x); }

SPRT_FORCEINLINE auto llround(float x) { return __builtin_llroundf(x); }
SPRT_FORCEINLINE auto llroundf(float x) { return __builtin_llroundf(x); }
SPRT_FORCEINLINE auto llround(double x) { return __builtin_llround(x); }
SPRT_FORCEINLINE auto llround(long double x) { return __builtin_llroundl(x); }
SPRT_FORCEINLINE auto llroundl(long double x) { return __builtin_llroundl(x); }

SPRT_FORCEINLINE auto trunc(float x) { return __builtin_truncf(x); }
SPRT_FORCEINLINE auto truncf(float x) { return __builtin_truncf(x); }
SPRT_FORCEINLINE auto trunc(double x) { return __builtin_trunc(x); }
SPRT_FORCEINLINE auto trunc(long double x) { return __builtin_truncl(x); }
SPRT_FORCEINLINE auto truncl(long double x) { return __builtin_truncl(x); }

SPRT_FORCEINLINE auto fmod(float x, float y) { return __builtin_fmodf(x, y); }
SPRT_FORCEINLINE auto fmodf(float x, float y) { return __builtin_fmodf(x, y); }
SPRT_FORCEINLINE auto fmod(double x, double y) { return __builtin_fmod(x, y); }
SPRT_FORCEINLINE auto fmod(long double x, long double y) { return __builtin_fmodl(x, y); }
SPRT_FORCEINLINE auto fmodl(long double x, long double y) { return __builtin_fmodl(x, y); }

SPRT_FORCEINLINE auto remainder(float x, float y) { return __builtin_remainderf(x, y); }
SPRT_FORCEINLINE auto remainderf(float x, float y) { return __builtin_remainderf(x, y); }
SPRT_FORCEINLINE auto remainder(double x, double y) { return __builtin_remainder(x, y); }
SPRT_FORCEINLINE auto remainder(long double x, long double y) { return __builtin_remainderl(x, y); }
SPRT_FORCEINLINE auto remainderl(long double x, long double y) {
	return __builtin_remainderl(x, y);
}

SPRT_FORCEINLINE auto remquo(float x, float y, int *quo) { return __builtin_remquof(x, y, quo); }
SPRT_FORCEINLINE auto remquof(float x, float y, int *quo) { return __builtin_remquof(x, y, quo); }
SPRT_FORCEINLINE auto remquo(double x, double y, int *quo) { return __builtin_remquo(x, y, quo); }
SPRT_FORCEINLINE auto remquo(long double x, long double y, int *quo) {
	return __builtin_remquol(x, y, quo);
}
SPRT_FORCEINLINE auto remquol(long double x, long double y, int *quo) {
	return __builtin_remquol(x, y, quo);
}

SPRT_FORCEINLINE auto copysign(float x, float y) { return __builtin_copysignf(x, y); }
SPRT_FORCEINLINE auto copysignf(float x, float y) { return __builtin_copysignf(x, y); }
SPRT_FORCEINLINE auto copysign(double x, double y) { return __builtin_copysign(x, y); }
SPRT_FORCEINLINE auto copysign(long double x, long double y) { return __builtin_copysignl(x, y); }
SPRT_FORCEINLINE auto copysignl(long double x, long double y) { return __builtin_copysignl(x, y); }

SPRT_FORCEINLINE auto nan(const char *tagp) { return __builtin_nanf(tagp); }
SPRT_FORCEINLINE auto nanf(const char *tagp) { return __builtin_nanf(tagp); }
SPRT_FORCEINLINE auto nanl(const char *tagp) { return __builtin_nanl(tagp); }

SPRT_FORCEINLINE auto nextafter(float x, float y) { return __builtin_nextafterf(x, y); }
SPRT_FORCEINLINE auto nextafterf(float x, float y) { return __builtin_nextafterf(x, y); }
SPRT_FORCEINLINE auto nextafter(double x, double y) { return __builtin_nextafter(x, y); }
SPRT_FORCEINLINE auto nextafter(long double x, long double y) { return __builtin_nextafterl(x, y); }
SPRT_FORCEINLINE auto nextafterl(long double x, long double y) {
	return __builtin_nextafterl(x, y);
}

SPRT_FORCEINLINE auto nexttoward(float x, float y) { return __builtin_nexttowardf(x, y); }
SPRT_FORCEINLINE auto nexttowardf(float x, float y) { return __builtin_nexttowardf(x, y); }
SPRT_FORCEINLINE auto nexttoward(double x, double y) { return __builtin_nexttoward(x, y); }
SPRT_FORCEINLINE auto nexttoward(long double x, long double y) {
	return __builtin_nexttowardl(x, y);
}
SPRT_FORCEINLINE auto nexttowardl(long double x, long double y) {
	return __builtin_nexttowardl(x, y);
}

SPRT_FORCEINLINE auto fdim(float x, float y) { return __builtin_fdimf(x, y); }
SPRT_FORCEINLINE auto fdimf(float x, float y) { return __builtin_fdimf(x, y); }
SPRT_FORCEINLINE auto fdim(double x, double y) { return __builtin_fdim(x, y); }
SPRT_FORCEINLINE auto fdim(long double x, long double y) { return __builtin_fdiml(x, y); }
SPRT_FORCEINLINE auto fdiml(long double x, long double y) { return __builtin_fdiml(x, y); }

SPRT_FORCEINLINE auto fmax(float x, float y) { return __builtin_fmaxf(x, y); }
SPRT_FORCEINLINE auto fmaxf(float x, float y) { return __builtin_fmaxf(x, y); }
SPRT_FORCEINLINE auto fmax(double x, double y) { return __builtin_fmax(x, y); }
SPRT_FORCEINLINE auto fmax(long double x, long double y) { return __builtin_fmaxl(x, y); }
SPRT_FORCEINLINE auto fmaxl(long double x, long double y) { return __builtin_fmaxl(x, y); }

SPRT_FORCEINLINE auto fmin(float x, float y) { return __builtin_fminf(x, y); }
SPRT_FORCEINLINE auto fminf(float x, float y) { return __builtin_fminf(x, y); }
SPRT_FORCEINLINE auto fmin(double x, double y) { return __builtin_fmin(x, y); }
SPRT_FORCEINLINE auto fmin(long double x, long double y) { return __builtin_fminl(x, y); }
SPRT_FORCEINLINE auto fminl(long double x, long double y) { return __builtin_fminl(x, y); }

template <typename Type>
constexpr SPRT_FORCEINLINE Type __lerp(Type a, Type b, Type t) noexcept {
	if ((a <= 0 && b >= 0) || (a >= 0 && b <= 0)) {
		return t * b + (1 - t) * a;
	}
	if (t == 1) {
		return b;
	}
	const Type __x = a + t * (b - a);
	if ((t > 1) == (b > a)) {
		return b < __x ? __x : b;
	} else {
		return __x < b ? __x : b;
	}
}

constexpr SPRT_FORCEINLINE float lerp(float a, float b, float t) noexcept {
	return __lerp(a, b, t);
}

constexpr SPRT_FORCEINLINE double lerp(double a, double b, double t) noexcept {
	return __lerp(a, b, t);
}

constexpr SPRT_FORCEINLINE long double lerp(long double a, long double b, long double t) noexcept {
	return __lerp(a, b, t);
}

template <typename _A1, typename _A2, typename _A3>
requires (is_arithmetic_v<_A1> && is_arithmetic_v<_A2> && is_arithmetic_v<_A3>)
constexpr SPRT_FORCEINLINE sprt::detail::promote_t<_A1, _A2, _A3> lerp(_A1 __a, _A2 __b,
		_A3 __t) noexcept {
	using __result_type = sprt::detail::promote_t<_A1, _A2, _A3>;
	static_assert(!(sprt::is_same_v<_A1, __result_type> && sprt::is_same_v<_A2, __result_type>
			&& sprt::is_same_v<_A3, __result_type>));
	return sprt::_cmath::__lerp((__result_type)__a, (__result_type)__b, (__result_type)__t);
}

SPRT_FORCEINLINE auto fpclassify(float x) {
	return __builtin_fpclassify(_FP_NAN, _FP_INFINITE, _FP_NORMAL, _FP_SUBNORMAL, _FP_ZERO, x);
}

SPRT_FORCEINLINE auto fpclassify(double x) {
	return __builtin_fpclassify(_FP_NAN, _FP_INFINITE, _FP_NORMAL, _FP_SUBNORMAL, _FP_ZERO, x);
}

SPRT_FORCEINLINE auto fpclassify(long double x) {
	return __builtin_fpclassify(_FP_NAN, _FP_INFINITE, _FP_NORMAL, _FP_SUBNORMAL, _FP_ZERO, x);
}

SPRT_FORCEINLINE bool isfinite(float x) { return __builtin_isfinite(x); }
SPRT_FORCEINLINE bool isfinite(double x) { return __builtin_isfinite(x); }
SPRT_FORCEINLINE bool isfinite(long double x) { return __builtin_isfinite(x); }

SPRT_FORCEINLINE bool isinf(float x) { return __builtin_isinf(x); }
SPRT_FORCEINLINE bool isinf(double x) { return __builtin_isinf(x); }
SPRT_FORCEINLINE bool isinf(long double x) { return __builtin_isinf(x); }

SPRT_FORCEINLINE bool isnan(float x) { return __builtin_isnan(x); }
SPRT_FORCEINLINE bool isnan(double x) { return __builtin_isnan(x); }
SPRT_FORCEINLINE bool isnan(long double x) { return __builtin_isnan(x); }

SPRT_FORCEINLINE bool isnormal(float x) { return __builtin_isnormal(x); }
SPRT_FORCEINLINE bool isnormal(double x) { return __builtin_isnormal(x); }
SPRT_FORCEINLINE bool isnormal(long double x) { return __builtin_isnormal(x); }

SPRT_FORCEINLINE bool signbit(float x) { return __builtin_signbit(x); }
SPRT_FORCEINLINE bool signbit(double x) { return __builtin_signbit(x); }
SPRT_FORCEINLINE bool signbit(long double x) { return __builtin_signbit(x); }

SPRT_FORCEINLINE bool isgreater(float x, float y) { return __builtin_isgreater(x, y); }
SPRT_FORCEINLINE bool isgreater(double x, double y) { return __builtin_isgreater(x, y); }
SPRT_FORCEINLINE bool isgreater(long double x, long double y) { return __builtin_isgreater(x, y); }

SPRT_FORCEINLINE bool isgreaterequal(float x, float y) { return __builtin_isgreaterequal(x, y); }
SPRT_FORCEINLINE bool isgreaterequal(double x, double y) { return __builtin_isgreaterequal(x, y); }
SPRT_FORCEINLINE bool isgreaterequal(long double x, long double y) {
	return __builtin_isgreaterequal(x, y);
}

SPRT_FORCEINLINE bool isless(float x, float y) { return __builtin_isless(x, y); }
SPRT_FORCEINLINE bool isless(double x, double y) { return __builtin_isless(x, y); }
SPRT_FORCEINLINE bool isless(long double x, long double y) { return __builtin_isless(x, y); }

SPRT_FORCEINLINE bool islessequal(float x, float y) { return __builtin_islessequal(x, y); }
SPRT_FORCEINLINE bool islessequal(double x, double y) { return __builtin_islessequal(x, y); }
SPRT_FORCEINLINE bool islessequal(long double x, long double y) {
	return __builtin_islessequal(x, y);
}

SPRT_FORCEINLINE bool islessgreater(float x, float y) { return __builtin_islessgreater(x, y); }
SPRT_FORCEINLINE bool islessgreater(double x, double y) { return __builtin_islessgreater(x, y); }
SPRT_FORCEINLINE bool islessgreater(long double x, long double y) {
	return __builtin_islessgreater(x, y);
}

SPRT_FORCEINLINE bool isunordered(float x, float y) { return __builtin_isunordered(x, y); }
SPRT_FORCEINLINE bool isunordered(double x, double y) { return __builtin_isunordered(x, y); }
SPRT_FORCEINLINE bool isunordered(long double x, long double y) {
	return __builtin_isunordered(x, y);
}

// From libc++, comments preserved
//
// Computes the three-dimensional hypotenuse: `std::hypot(x,y,z)`.
// The naive implementation might over-/underflow which is why this implementation is more involved:
//    If the square of an argument might run into issues, we scale the arguments appropriately.
// See https://github.com/llvm/llvm-project/issues/92782 for a detailed discussion and summary.
template <typename _Real>
_Real __hypot(_Real __x, _Real __y, _Real __z) {
	// Factors needed to determine if over-/underflow might happen
	constexpr int __exp = MaxExp<_Real> / 2;
	const _Real __overflow_threshold = ldexp(_Real(1), __exp);
	const _Real __overflow_scale = ldexp(_Real(1), -(__exp + 20));

	// Scale arguments depending on their size
	const _Real __max_abs = fmax(fabs(__x), fmax(fabs(__y), fabs(__z)));
	_Real __scale;
	if (__max_abs > __overflow_threshold) { // x*x + y*y + z*z might overflow
		__scale = __overflow_scale;
	} else if (__max_abs < 1 / __overflow_threshold) { // x*x + y*y + z*z might underflow
		__scale = 1 / __overflow_scale;
	} else {
		__scale = 1;
	}
	__x *= __scale;
	__y *= __scale;
	__z *= __scale;

	// Compute hypot of scaled arguments and undo scaling
	return sqrt(__x * __x + __y * __y + __z * __z) / __scale;
}

SPRT_FORCEINLINE float hypot(float x, float y, float z) { return __hypot(x, y, z); }

SPRT_FORCEINLINE double hypot(double x, double y, double z) { return __hypot(x, y, z); }

SPRT_FORCEINLINE long double hypot(long double x, long double y, long double z) {
	return __hypot(x, y, z);
}

template <typename _A1, typename _A2, typename _A3,
		enable_if_t<is_arithmetic_v<_A1> && is_arithmetic_v<_A2> && is_arithmetic_v<_A3>, int> = 0 >
sprt::detail::promote_t<_A1, _A2, _A3> hypot(_A1 __x, _A2 __y, _A3 __z) noexcept {
	using __result_type = sprt::detail::promote_t<_A1, _A2, _A3>;
	static_assert(!(sprt::is_same_v<_A1, __result_type> && sprt::is_same_v<_A2, __result_type>
			&& sprt::is_same_v<_A3, __result_type>));
	return __hypot(static_cast<__result_type>(__x), static_cast<__result_type>(__y),
			static_cast<__result_type>(__z));
}

} // namespace _cmath
} // namespace sprt

#if __STDC_HOSTED__ == 1 && !defined(__SPRT_BUILD)
using namespace sprt::_cmath;
#endif

#ifdef __SPRT_AS_STD
namespace std {
using namespace sprt::_cmath;
}
#endif
#endif // __cplusplus


#if !defined(__cplusplus) && __STDC_HOSTED__ == 1
#define fpclassify(x) __sprt_fpclassify(x)
#define isinf(x) __sprt_isinf(x)
#define isnan(x) __sprt_isnan(x)
#define isnormal(x) __sprt_isnormal(x)
#define isfinite(x) __sprt_isfinite(x)
#define signbit(x) __sprt_signbit(x)
#endif


#if __STDC_HOSTED__ == 0
__SPRT_BEGIN_DECL
int __fpclassify(double);
int __fpclassifyf(float);
int __fpclassifyl(long double);
int __signbit(double);
int __signbitf(float);
int __signbitl(long double);
__SPRT_END_DECL
#endif


#if __STDC_HOSTED__ == 0 || (!defined(__SPRT_BUILD) && !defined(__cplusplus))
__SPRT_BEGIN_DECL

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
#endif

#endif // CORE_RUNTIME_INCLUDE_SPRT_WRAPPERS_LIBC_MATH_H_
