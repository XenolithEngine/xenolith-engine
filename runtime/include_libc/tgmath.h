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

#ifndef CORE_RUNTIME_INCLUDE_LIBC_TGMATH_H_
#define CORE_RUNTIME_INCLUDE_LIBC_TGMATH_H_

/*
	Dispatch header for <tgmath.h> (C99/C11 type-generic math):
	- hosted SPRT build -> forwards to the system <tgmath.h> (#include_next)
	- otherwise         -> pulls in <math.h> and <complex.h> and, in C, defines the
	                       type-generic macros with C11 _Generic, selecting the
	                       float/double/long-double (and, where it exists, complex)
	                       variant from the argument types. Integer and plain double
	                       arguments fall through `default:` to the double variant.

	In C++ the math overloads in <math.h>/<complex.h> already provide type-generic
	behaviour, so no macros are defined there — including the two headers suffices.
*/

#if defined(__SPRT_BUILD) && __STDC_HOSTED__ == 1

#include_next <tgmath.h>

#else

#include <math.h>
#include <complex.h>

#ifndef __cplusplus

// fn / fnf / fnl for reals plus cfn / cfnf / cfnl for complex (real & integer ->
// the bare double variant). _Generic's controlling expression is unevaluated, so
// the argument is evaluated exactly once, in the trailing call.
#define __sprt_tg_rc(fn, x) \
	_Generic((x), float: fn##f, long double: fn##l, double _Complex: c##fn, \
			float _Complex: c##fn##f, long double _Complex: c##fn##l, default: fn)
#define __sprt_tg_r(fn, x) _Generic((x), float: fn##f, long double: fn##l, default: fn)

// Functions defined for both real and complex arguments.
#define acos(x) __sprt_tg_rc(acos, (x))(x)
#define asin(x) __sprt_tg_rc(asin, (x))(x)
#define atan(x) __sprt_tg_rc(atan, (x))(x)
#define acosh(x) __sprt_tg_rc(acosh, (x))(x)
#define asinh(x) __sprt_tg_rc(asinh, (x))(x)
#define atanh(x) __sprt_tg_rc(atanh, (x))(x)
#define cos(x) __sprt_tg_rc(cos, (x))(x)
#define sin(x) __sprt_tg_rc(sin, (x))(x)
#define tan(x) __sprt_tg_rc(tan, (x))(x)
#define cosh(x) __sprt_tg_rc(cosh, (x))(x)
#define sinh(x) __sprt_tg_rc(sinh, (x))(x)
#define tanh(x) __sprt_tg_rc(tanh, (x))(x)
#define exp(x) __sprt_tg_rc(exp, (x))(x)
#define log(x) __sprt_tg_rc(log, (x))(x)
#define sqrt(x) __sprt_tg_rc(sqrt, (x))(x)

// fabs is real+complex but its complex variant is spelled cabs (not cfabs).
#define fabs(x) \
	_Generic((x), float: fabsf, long double: fabsl, double _Complex: cabs, \
			float _Complex: cabsf, long double _Complex: cabsl, default: fabs)(x)

// pow is real+complex with two generic arguments.
#define pow(x, y) \
	_Generic((x) + (y), float: powf, long double: powl, double _Complex: cpow, \
			float _Complex: cpowf, long double _Complex: cpowl, default: pow)((x), (y))

// Real-only, single argument.
#define cbrt(x) __sprt_tg_r(cbrt, (x))(x)
#define ceil(x) __sprt_tg_r(ceil, (x))(x)
#define erf(x) __sprt_tg_r(erf, (x))(x)
#define erfc(x) __sprt_tg_r(erfc, (x))(x)
#define exp2(x) __sprt_tg_r(exp2, (x))(x)
#define expm1(x) __sprt_tg_r(expm1, (x))(x)
#define floor(x) __sprt_tg_r(floor, (x))(x)
#define ilogb(x) __sprt_tg_r(ilogb, (x))(x)
#define lgamma(x) __sprt_tg_r(lgamma, (x))(x)
#define llrint(x) __sprt_tg_r(llrint, (x))(x)
#define llround(x) __sprt_tg_r(llround, (x))(x)
#define log10(x) __sprt_tg_r(log10, (x))(x)
#define log1p(x) __sprt_tg_r(log1p, (x))(x)
#define log2(x) __sprt_tg_r(log2, (x))(x)
#define logb(x) __sprt_tg_r(logb, (x))(x)
#define lrint(x) __sprt_tg_r(lrint, (x))(x)
#define lround(x) __sprt_tg_r(lround, (x))(x)
#define nearbyint(x) __sprt_tg_r(nearbyint, (x))(x)
#define rint(x) __sprt_tg_r(rint, (x))(x)
#define round(x) __sprt_tg_r(round, (x))(x)
#define tgamma(x) __sprt_tg_r(tgamma, (x))(x)
#define trunc(x) __sprt_tg_r(trunc, (x))(x)

// Real-only, two generic arguments (type from their usual arithmetic conversion).
#define atan2(x, y) _Generic((x) + (y), float: atan2f, long double: atan2l, default: atan2)((x), (y))
#define copysign(x, y) \
	_Generic((x) + (y), float: copysignf, long double: copysignl, default: copysign)((x), (y))
#define fdim(x, y) _Generic((x) + (y), float: fdimf, long double: fdiml, default: fdim)((x), (y))
#define fmax(x, y) _Generic((x) + (y), float: fmaxf, long double: fmaxl, default: fmax)((x), (y))
#define fmin(x, y) _Generic((x) + (y), float: fminf, long double: fminl, default: fmin)((x), (y))
#define fmod(x, y) _Generic((x) + (y), float: fmodf, long double: fmodl, default: fmod)((x), (y))
#define hypot(x, y) _Generic((x) + (y), float: hypotf, long double: hypotl, default: hypot)((x), (y))
#define nextafter(x, y) \
	_Generic((x) + (y), float: nextafterf, long double: nextafterl, default: nextafter)((x), (y))
#define remainder(x, y) \
	_Generic((x) + (y), float: remainderf, long double: remainderl, default: remainder)((x), (y))

// Real-only with a fixed (non-generic) trailing argument; dispatch on the first.
#define frexp(x, y) _Generic((x), float: frexpf, long double: frexpl, default: frexp)((x), (y))
#define ldexp(x, y) _Generic((x), float: ldexpf, long double: ldexpl, default: ldexp)((x), (y))
#define scalbn(x, y) _Generic((x), float: scalbnf, long double: scalbnl, default: scalbn)((x), (y))
#define scalbln(x, y) \
	_Generic((x), float: scalblnf, long double: scalblnl, default: scalbln)((x), (y))
#define nexttoward(x, y) \
	_Generic((x), float: nexttowardf, long double: nexttowardl, default: nexttoward)((x), (y))

// Real-only, three arguments.
#define fma(x, y, z) \
	_Generic((x) + (y) + (z), float: fmaf, long double: fmal, default: fma)((x), (y), (z))
#define remquo(x, y, z) \
	_Generic((x) + (y), float: remquof, long double: remquol, default: remquo)((x), (y), (z))

// Complex-only (real arguments convert to complex through `default:`).
#define carg(x) \
	_Generic((x), float _Complex: cargf, long double _Complex: cargl, default: carg)(x)
#define cimag(x) \
	_Generic((x), float _Complex: cimagf, long double _Complex: cimagl, default: cimag)(x)
#define conj(x) \
	_Generic((x), float _Complex: conjf, long double _Complex: conjl, default: conj)(x)
#define cproj(x) \
	_Generic((x), float _Complex: cprojf, long double _Complex: cprojl, default: cproj)(x)
#define creal(x) \
	_Generic((x), float _Complex: crealf, long double _Complex: creall, default: creal)(x)

#endif // !__cplusplus

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_TGMATH_H_
