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

// Makes Embox's <math.h> usable as the declaration surface for the musl math
// port in c/math/embox_math_{flt,dbl,ldbl}.c. Include it after <math.h> and
// before any musl source. Embox-only; nothing else in the runtime includes it,
// apart from SPRuntimeCMath.cpp (see the extern "C" note below).
//
// Embox does not declare the C99 math surface. It declares two dozen entries
// (exp/log/pow/sqrt/floor/ceil/round/fmod/modf/fabs and their f/l siblings) and
// #defines every other name in src/compat/libc/math/math_builtins.h as a
// function-like macro onto the matching clang builtin - and a builtin with no
// library behind it lowers straight back to a call to the libm symbol nobody
// defines. So the macros are not an implementation, they are the hole. (The
// entries it does declare are implemented by its math_simple module, which the
// board template no longer selects - see embox_math_dbl.c.)
//
// Two things follow, and both are done here:
//
//   1. The macros must go before a musl source can DEFINE any of those names -
//      a function-like macro would otherwise eat the definition's parameter
//      list. #undef leaves the names undeclared, hence
//   2. the prototypes below, taken verbatim from musl's own <math.h> so the
//      definitions that follow cannot drift from what callers see.
//
// Also fixed here: Embox spells clang's finite/normal predicates
// `__builtin___isfinite` / `__builtin___isnormal` (three underscores). Those are
// not clang builtins - the real names carry one underscore - so `isfinite(x)`
// and `isnormal(x)` do not compile at all under clang, which several musl
// sources below use. Upstream bug, worked around rather than patched.

#ifndef CORE_RUNTIME_LIBC_WRAPPER_C_MATH_EMBOX_MATH_SHIM_H_
#define CORE_RUNTIME_LIBC_WRAPPER_C_MATH_EMBOX_MATH_SHIM_H_

#if !SPRT_EMBOX
#error "embox_math_shim.h is for the Embox target only"
#endif

// clang-format off
#undef acos
#undef acosf
#undef acosl
#undef asin
#undef asinf
#undef asinl
#undef atan
#undef atanf
#undef atanl
#undef atan2
#undef atan2f
#undef atan2l
#undef atanh
#undef atanhf
#undef atanhl
#undef cbrt
#undef cbrtf
#undef cbrtl
#undef copysign
#undef copysignf
#undef copysignl
#undef cos
#undef cosf
#undef cosl
#undef cosh
#undef coshf
#undef coshl
#undef exp2
#undef exp2f
#undef exp2l
#undef frexp
#undef frexpf
#undef frexpl
#undef hypot
#undef hypotf
#undef hypotl
#undef ldexp
#undef ldexpf
#undef ldexpl
#undef llrint
#undef llrintf
#undef llrintl
#undef llround
#undef llroundf
#undef llroundl
#undef log2
#undef log2f
#undef log2l
#undef lrint
#undef lrintf
#undef lrintl
#undef lround
#undef lroundf
#undef lroundl
#undef fmax
#undef fmaxf
#undef fmaxl
#undef fmin
#undef fminf
#undef fminl
#undef rint
#undef rintf
#undef rintl
#undef sin
#undef sinf
#undef sinl
#undef sinh
#undef sinhf
#undef sinhl
#undef tan
#undef tanf
#undef tanl
#undef tanh
#undef tanhf
#undef tanhl
#undef trunc
#undef truncf
#undef truncl
// Embox's own isinf/isnan/fpclassify are fine; these two are not (see above).
#undef isfinite
#undef isnormal
#define isfinite(x) __builtin_isfinite(x)
#define isnormal(x) __builtin_isnormal(x)

// Prototypes for everything the port defines, plus the handful of Embox-declared
// names that were macros and are now gone. Copied from musl's <math.h>.
//
// extern "C" because SPRuntimeCMath.cpp includes this too: the wrapper forwards
// __sprt_acosh/... to the plain names, which on Embox are neither declared nor
// macro-defined, and it must see the same declarations the definitions get.
#ifdef __cplusplus
extern "C" {
#endif
double acos(double);
float acosf(float);
double acosh(double);
float acoshf(float);
long double acoshl(long double);
long double acosl(long double);
double asin(double);
float asinf(float);
double asinh(double);
float asinhf(float);
long double asinhl(long double);
long double asinl(long double);
double atan(double);
double atan2(double, double);
float atan2f(float, float);
long double atan2l(long double, long double);
float atanf(float);
double atanh(double);
float atanhf(float);
long double atanhl(long double);
long double atanl(long double);
double cbrt(double);
float cbrtf(float);
long double cbrtl(long double);
double copysign(double, double);
float copysignf(float, float);
long double copysignl(long double, long double);
double cos(double);
float cosf(float);
double cosh(double);
float coshf(float);
long double coshl(long double);
long double cosl(long double);
double erf(double);
double erfc(double);
float erfcf(float);
long double erfcl(long double);
float erff(float);
long double erfl(long double);
double exp2(double);
float exp2f(float);
long double exp2l(long double);
double expm1(double);
float expm1f(float);
long double expm1l(long double);
double fdim(double, double);
float fdimf(float, float);
long double fdiml(long double, long double);
double fma(double, double, double);
float fmaf(float, float, float);
long double fmal(long double, long double, long double);
double fmax(double, double);
float fmaxf(float, float);
long double fmaxl(long double, long double);
double fmin(double, double);
float fminf(float, float);
long double fminl(long double, long double);
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
float j0f(float);
float j1f(float);
float jnf(int, float);
double ldexp(double, int);
float ldexpf(float, int);
long double ldexpl(long double, int);
double lgamma(double);
double lgamma_r(double, int*);
float lgammaf(float);
long double lgammal(long double);
long long llrint(double);
long long llrintf(float);
long long llrintl(long double);
long long llround(double);
long long llroundf(float);
long long llroundl(long double);
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
double pow10(double);
float pow10f(float);
long double pow10l(long double);
double remainder(double, double);
float remainderf(float, float);
long double remainderl(long double, long double);
double remquo(double, double, int *);
float remquof(float, float, int *);
long double remquol(long double, long double, int *);
double rint(double);
float rintf(float);
long double rintl(long double);
double scalbln(double, long);
float scalblnf(float, long);
long double scalblnl(long double, long);
double scalbn(double, int);
float scalbnf(float, int);
long double scalbnl(long double, int);
double sin(double);
float sinf(float);
double sinh(double);
float sinhf(float);
long double sinhl(long double);
long double sinl(long double);
double tan(double);
float tanf(float);
double tanh(double);
float tanhf(float);
long double tanhl(long double);
long double tanl(long double);
double tgamma(double);
float tgammaf(float);
long double tgammal(long double);
double trunc(double);
float truncf(float);
long double truncl(long double);
float y0f(float);
float y1f(float);
float ynf(int, float);
#ifdef __cplusplus
}
#endif
// clang-format on

#endif // CORE_RUNTIME_LIBC_WRAPPER_C_MATH_EMBOX_MATH_SHIM_H_
