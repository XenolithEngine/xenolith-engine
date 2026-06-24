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

#ifndef CORE_RUNTIME_INCLUDE_SPRT_WRAPPERS_LIBC_COMPLEX_H_
#define CORE_RUNTIME_INCLUDE_SPRT_WRAPPERS_LIBC_COMPLEX_H_

// SPRT's own <complex.h>: the C99 complex math surface. Each function comes in
// three precisions (bare = double _Complex, f = float, l = long double). The
// forwarders are plain extern-C functions taking/returning _Complex, used from
// both C and C++ (clang accepts _Complex in C++); there is no C++ namespace
// section because the `complex` macro and std::complex would collide.
//   - hosted: SPRT_UMBRELLA_FUNC is a static-inline forwarding to __sprt_*_impl
//     (the libc wrapper -> native libm).
//   - freestanding: a bare prototype resolved to the musl-provided public name.

#include <sprt/c/__sprt_complex.h>

// ISO C complex macros. `complex`/`imaginary` are keywords-via-macro in C only
// (in C++ they would clash with std::complex). _Complex_I/I and the C11 CMPLX
// family use clang's __builtin_complex so they are valid constant expressions.
#ifndef __cplusplus
#ifndef complex
#define complex _Complex
#endif
#endif

#ifndef _Complex_I
#define _Complex_I (__builtin_complex(0.0f, 1.0f))
#endif
#ifndef I
#define I _Complex_I
#endif

#ifndef CMPLX
#define CMPLX(x, y) __builtin_complex((double)(x), (double)(y))
#define CMPLXF(x, y) __builtin_complex((float)(x), (float)(y))
#define CMPLXL(x, y) __builtin_complex((long double)(x), (long double)(y))
#endif

__SPRT_BEGIN_DECL

SPRT_UMBRELLA_FUNC
double cabs(double _Complex __z) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_cabs(__z);
}
#endif

SPRT_UMBRELLA_FUNC
float cabsf(float _Complex __z) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_cabsf(__z);
}
#endif

SPRT_UMBRELLA_FUNC
long double cabsl(long double _Complex __z) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_cabsl(__z);
}
#endif

SPRT_UMBRELLA_FUNC
double carg(double _Complex __z) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_carg(__z);
}
#endif

SPRT_UMBRELLA_FUNC
float cargf(float _Complex __z) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_cargf(__z);
}
#endif

SPRT_UMBRELLA_FUNC
long double cargl(long double _Complex __z) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_cargl(__z);
}
#endif

SPRT_UMBRELLA_FUNC
double cimag(double _Complex __z) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_cimag(__z);
}
#endif

SPRT_UMBRELLA_FUNC
float cimagf(float _Complex __z) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_cimagf(__z);
}
#endif

SPRT_UMBRELLA_FUNC
long double cimagl(long double _Complex __z) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_cimagl(__z);
}
#endif

SPRT_UMBRELLA_FUNC
double creal(double _Complex __z) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_creal(__z);
}
#endif

SPRT_UMBRELLA_FUNC
float crealf(float _Complex __z) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_crealf(__z);
}
#endif

SPRT_UMBRELLA_FUNC
long double creall(long double _Complex __z) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_creall(__z);
}
#endif

SPRT_UMBRELLA_FUNC
double _Complex cacos(double _Complex __z) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_cacos(__z);
}
#endif

SPRT_UMBRELLA_FUNC
float _Complex cacosf(float _Complex __z) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_cacosf(__z);
}
#endif

SPRT_UMBRELLA_FUNC
long double _Complex cacosl(long double _Complex __z) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_cacosl(__z);
}
#endif

SPRT_UMBRELLA_FUNC
double _Complex cacosh(double _Complex __z) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_cacosh(__z);
}
#endif

SPRT_UMBRELLA_FUNC
float _Complex cacoshf(float _Complex __z) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_cacoshf(__z);
}
#endif

SPRT_UMBRELLA_FUNC
long double _Complex cacoshl(long double _Complex __z) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_cacoshl(__z);
}
#endif

SPRT_UMBRELLA_FUNC
double _Complex casin(double _Complex __z) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_casin(__z);
}
#endif

SPRT_UMBRELLA_FUNC
float _Complex casinf(float _Complex __z) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_casinf(__z);
}
#endif

SPRT_UMBRELLA_FUNC
long double _Complex casinl(long double _Complex __z) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_casinl(__z);
}
#endif

SPRT_UMBRELLA_FUNC
double _Complex casinh(double _Complex __z) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_casinh(__z);
}
#endif

SPRT_UMBRELLA_FUNC
float _Complex casinhf(float _Complex __z) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_casinhf(__z);
}
#endif

SPRT_UMBRELLA_FUNC
long double _Complex casinhl(long double _Complex __z) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_casinhl(__z);
}
#endif

SPRT_UMBRELLA_FUNC
double _Complex catan(double _Complex __z) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_catan(__z);
}
#endif

SPRT_UMBRELLA_FUNC
float _Complex catanf(float _Complex __z) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_catanf(__z);
}
#endif

SPRT_UMBRELLA_FUNC
long double _Complex catanl(long double _Complex __z) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_catanl(__z);
}
#endif

SPRT_UMBRELLA_FUNC
double _Complex catanh(double _Complex __z) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_catanh(__z);
}
#endif

SPRT_UMBRELLA_FUNC
float _Complex catanhf(float _Complex __z) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_catanhf(__z);
}
#endif

SPRT_UMBRELLA_FUNC
long double _Complex catanhl(long double _Complex __z) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_catanhl(__z);
}
#endif

SPRT_UMBRELLA_FUNC
double _Complex ccos(double _Complex __z) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_ccos(__z);
}
#endif

SPRT_UMBRELLA_FUNC
float _Complex ccosf(float _Complex __z) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_ccosf(__z);
}
#endif

SPRT_UMBRELLA_FUNC
long double _Complex ccosl(long double _Complex __z) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_ccosl(__z);
}
#endif

SPRT_UMBRELLA_FUNC
double _Complex ccosh(double _Complex __z) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_ccosh(__z);
}
#endif

SPRT_UMBRELLA_FUNC
float _Complex ccoshf(float _Complex __z) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_ccoshf(__z);
}
#endif

SPRT_UMBRELLA_FUNC
long double _Complex ccoshl(long double _Complex __z) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_ccoshl(__z);
}
#endif

SPRT_UMBRELLA_FUNC
double _Complex cexp(double _Complex __z) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_cexp(__z);
}
#endif

SPRT_UMBRELLA_FUNC
float _Complex cexpf(float _Complex __z) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_cexpf(__z);
}
#endif

SPRT_UMBRELLA_FUNC
long double _Complex cexpl(long double _Complex __z) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_cexpl(__z);
}
#endif

SPRT_UMBRELLA_FUNC
double _Complex clog(double _Complex __z) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_clog(__z);
}
#endif

SPRT_UMBRELLA_FUNC
float _Complex clogf(float _Complex __z) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_clogf(__z);
}
#endif

SPRT_UMBRELLA_FUNC
long double _Complex clogl(long double _Complex __z) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_clogl(__z);
}
#endif

SPRT_UMBRELLA_FUNC
double _Complex conj(double _Complex __z) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_conj(__z);
}
#endif

SPRT_UMBRELLA_FUNC
float _Complex conjf(float _Complex __z) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_conjf(__z);
}
#endif

SPRT_UMBRELLA_FUNC
long double _Complex conjl(long double _Complex __z) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_conjl(__z);
}
#endif

SPRT_UMBRELLA_FUNC
double _Complex cproj(double _Complex __z) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_cproj(__z);
}
#endif

SPRT_UMBRELLA_FUNC
float _Complex cprojf(float _Complex __z) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_cprojf(__z);
}
#endif

SPRT_UMBRELLA_FUNC
long double _Complex cprojl(long double _Complex __z) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_cprojl(__z);
}
#endif

SPRT_UMBRELLA_FUNC
double _Complex csin(double _Complex __z) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_csin(__z);
}
#endif

SPRT_UMBRELLA_FUNC
float _Complex csinf(float _Complex __z) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_csinf(__z);
}
#endif

SPRT_UMBRELLA_FUNC
long double _Complex csinl(long double _Complex __z) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_csinl(__z);
}
#endif

SPRT_UMBRELLA_FUNC
double _Complex csinh(double _Complex __z) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_csinh(__z);
}
#endif

SPRT_UMBRELLA_FUNC
float _Complex csinhf(float _Complex __z) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_csinhf(__z);
}
#endif

SPRT_UMBRELLA_FUNC
long double _Complex csinhl(long double _Complex __z) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_csinhl(__z);
}
#endif

SPRT_UMBRELLA_FUNC
double _Complex csqrt(double _Complex __z) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_csqrt(__z);
}
#endif

SPRT_UMBRELLA_FUNC
float _Complex csqrtf(float _Complex __z) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_csqrtf(__z);
}
#endif

SPRT_UMBRELLA_FUNC
long double _Complex csqrtl(long double _Complex __z) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_csqrtl(__z);
}
#endif

SPRT_UMBRELLA_FUNC
double _Complex ctan(double _Complex __z) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_ctan(__z);
}
#endif

SPRT_UMBRELLA_FUNC
float _Complex ctanf(float _Complex __z) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_ctanf(__z);
}
#endif

SPRT_UMBRELLA_FUNC
long double _Complex ctanl(long double _Complex __z) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_ctanl(__z);
}
#endif

SPRT_UMBRELLA_FUNC
double _Complex ctanh(double _Complex __z) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_ctanh(__z);
}
#endif

SPRT_UMBRELLA_FUNC
float _Complex ctanhf(float _Complex __z) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_ctanhf(__z);
}
#endif

SPRT_UMBRELLA_FUNC
long double _Complex ctanhl(long double _Complex __z) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_ctanhl(__z);
}
#endif

SPRT_UMBRELLA_FUNC
double _Complex cpow(double _Complex __x, double _Complex __y) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_cpow(__x, __y);
}
#endif

SPRT_UMBRELLA_FUNC
float _Complex cpowf(float _Complex __x, float _Complex __y) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_cpowf(__x, __y);
}
#endif

SPRT_UMBRELLA_FUNC
long double _Complex cpowl(long double _Complex __x, long double _Complex __y) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_cpowl(__x, __y);
}
#endif
__SPRT_END_DECL

#endif // CORE_RUNTIME_INCLUDE_SPRT_WRAPPERS_LIBC_COMPLEX_H_
