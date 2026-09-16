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

#ifndef CORE_RUNTIME_INCLUDE_C___SPRT_COMPLEX_H_
#define CORE_RUNTIME_INCLUDE_C___SPRT_COMPLEX_H_

#include <sprt/c/bits/__sprt_def.h>

// Impl symbols for the C99 <complex.h> functions. On a hosted build the libc
// wrapper provides __sprt_*_impl forwarding to the native libm; on the
// freestanding target the public names are provided by musl (musl-adapters),
// reached through the umbrella's bare prototypes. _Complex is a compiler keyword,
// available in C and (as a clang extension) in C++, so no extra include is needed.

__SPRT_BEGIN_DECL

SPRT_API double __SPRT_ID(cabs_impl)(double _Complex);
#define __sprt_cabs __SPRT_ID(cabs_impl)

SPRT_API float __SPRT_ID(cabsf_impl)(float _Complex);
#define __sprt_cabsf __SPRT_ID(cabsf_impl)

SPRT_API long double __SPRT_ID(cabsl_impl)(long double _Complex);
#define __sprt_cabsl __SPRT_ID(cabsl_impl)

SPRT_API double __SPRT_ID(carg_impl)(double _Complex);
#define __sprt_carg __SPRT_ID(carg_impl)

SPRT_API float __SPRT_ID(cargf_impl)(float _Complex);
#define __sprt_cargf __SPRT_ID(cargf_impl)

SPRT_API long double __SPRT_ID(cargl_impl)(long double _Complex);
#define __sprt_cargl __SPRT_ID(cargl_impl)

SPRT_API double __SPRT_ID(cimag_impl)(double _Complex);
#define __sprt_cimag __SPRT_ID(cimag_impl)

SPRT_API float __SPRT_ID(cimagf_impl)(float _Complex);
#define __sprt_cimagf __SPRT_ID(cimagf_impl)

SPRT_API long double __SPRT_ID(cimagl_impl)(long double _Complex);
#define __sprt_cimagl __SPRT_ID(cimagl_impl)

SPRT_API double __SPRT_ID(creal_impl)(double _Complex);
#define __sprt_creal __SPRT_ID(creal_impl)

SPRT_API float __SPRT_ID(crealf_impl)(float _Complex);
#define __sprt_crealf __SPRT_ID(crealf_impl)

SPRT_API long double __SPRT_ID(creall_impl)(long double _Complex);
#define __sprt_creall __SPRT_ID(creall_impl)

SPRT_API double _Complex __SPRT_ID(cacos_impl)(double _Complex);
#define __sprt_cacos __SPRT_ID(cacos_impl)

SPRT_API float _Complex __SPRT_ID(cacosf_impl)(float _Complex);
#define __sprt_cacosf __SPRT_ID(cacosf_impl)

SPRT_API long double _Complex __SPRT_ID(cacosl_impl)(long double _Complex);
#define __sprt_cacosl __SPRT_ID(cacosl_impl)

SPRT_API double _Complex __SPRT_ID(cacosh_impl)(double _Complex);
#define __sprt_cacosh __SPRT_ID(cacosh_impl)

SPRT_API float _Complex __SPRT_ID(cacoshf_impl)(float _Complex);
#define __sprt_cacoshf __SPRT_ID(cacoshf_impl)

SPRT_API long double _Complex __SPRT_ID(cacoshl_impl)(long double _Complex);
#define __sprt_cacoshl __SPRT_ID(cacoshl_impl)

SPRT_API double _Complex __SPRT_ID(casin_impl)(double _Complex);
#define __sprt_casin __SPRT_ID(casin_impl)

SPRT_API float _Complex __SPRT_ID(casinf_impl)(float _Complex);
#define __sprt_casinf __SPRT_ID(casinf_impl)

SPRT_API long double _Complex __SPRT_ID(casinl_impl)(long double _Complex);
#define __sprt_casinl __SPRT_ID(casinl_impl)

SPRT_API double _Complex __SPRT_ID(casinh_impl)(double _Complex);
#define __sprt_casinh __SPRT_ID(casinh_impl)

SPRT_API float _Complex __SPRT_ID(casinhf_impl)(float _Complex);
#define __sprt_casinhf __SPRT_ID(casinhf_impl)

SPRT_API long double _Complex __SPRT_ID(casinhl_impl)(long double _Complex);
#define __sprt_casinhl __SPRT_ID(casinhl_impl)

SPRT_API double _Complex __SPRT_ID(catan_impl)(double _Complex);
#define __sprt_catan __SPRT_ID(catan_impl)

SPRT_API float _Complex __SPRT_ID(catanf_impl)(float _Complex);
#define __sprt_catanf __SPRT_ID(catanf_impl)

SPRT_API long double _Complex __SPRT_ID(catanl_impl)(long double _Complex);
#define __sprt_catanl __SPRT_ID(catanl_impl)

SPRT_API double _Complex __SPRT_ID(catanh_impl)(double _Complex);
#define __sprt_catanh __SPRT_ID(catanh_impl)

SPRT_API float _Complex __SPRT_ID(catanhf_impl)(float _Complex);
#define __sprt_catanhf __SPRT_ID(catanhf_impl)

SPRT_API long double _Complex __SPRT_ID(catanhl_impl)(long double _Complex);
#define __sprt_catanhl __SPRT_ID(catanhl_impl)

SPRT_API double _Complex __SPRT_ID(ccos_impl)(double _Complex);
#define __sprt_ccos __SPRT_ID(ccos_impl)

SPRT_API float _Complex __SPRT_ID(ccosf_impl)(float _Complex);
#define __sprt_ccosf __SPRT_ID(ccosf_impl)

SPRT_API long double _Complex __SPRT_ID(ccosl_impl)(long double _Complex);
#define __sprt_ccosl __SPRT_ID(ccosl_impl)

SPRT_API double _Complex __SPRT_ID(ccosh_impl)(double _Complex);
#define __sprt_ccosh __SPRT_ID(ccosh_impl)

SPRT_API float _Complex __SPRT_ID(ccoshf_impl)(float _Complex);
#define __sprt_ccoshf __SPRT_ID(ccoshf_impl)

SPRT_API long double _Complex __SPRT_ID(ccoshl_impl)(long double _Complex);
#define __sprt_ccoshl __SPRT_ID(ccoshl_impl)

SPRT_API double _Complex __SPRT_ID(cexp_impl)(double _Complex);
#define __sprt_cexp __SPRT_ID(cexp_impl)

SPRT_API float _Complex __SPRT_ID(cexpf_impl)(float _Complex);
#define __sprt_cexpf __SPRT_ID(cexpf_impl)

SPRT_API long double _Complex __SPRT_ID(cexpl_impl)(long double _Complex);
#define __sprt_cexpl __SPRT_ID(cexpl_impl)

SPRT_API double _Complex __SPRT_ID(clog_impl)(double _Complex);
#define __sprt_clog __SPRT_ID(clog_impl)

SPRT_API float _Complex __SPRT_ID(clogf_impl)(float _Complex);
#define __sprt_clogf __SPRT_ID(clogf_impl)

SPRT_API long double _Complex __SPRT_ID(clogl_impl)(long double _Complex);
#define __sprt_clogl __SPRT_ID(clogl_impl)

SPRT_API double _Complex __SPRT_ID(conj_impl)(double _Complex);
#define __sprt_conj __SPRT_ID(conj_impl)

SPRT_API float _Complex __SPRT_ID(conjf_impl)(float _Complex);
#define __sprt_conjf __SPRT_ID(conjf_impl)

SPRT_API long double _Complex __SPRT_ID(conjl_impl)(long double _Complex);
#define __sprt_conjl __SPRT_ID(conjl_impl)

SPRT_API double _Complex __SPRT_ID(cproj_impl)(double _Complex);
#define __sprt_cproj __SPRT_ID(cproj_impl)

SPRT_API float _Complex __SPRT_ID(cprojf_impl)(float _Complex);
#define __sprt_cprojf __SPRT_ID(cprojf_impl)

SPRT_API long double _Complex __SPRT_ID(cprojl_impl)(long double _Complex);
#define __sprt_cprojl __SPRT_ID(cprojl_impl)

SPRT_API double _Complex __SPRT_ID(csin_impl)(double _Complex);
#define __sprt_csin __SPRT_ID(csin_impl)

SPRT_API float _Complex __SPRT_ID(csinf_impl)(float _Complex);
#define __sprt_csinf __SPRT_ID(csinf_impl)

SPRT_API long double _Complex __SPRT_ID(csinl_impl)(long double _Complex);
#define __sprt_csinl __SPRT_ID(csinl_impl)

SPRT_API double _Complex __SPRT_ID(csinh_impl)(double _Complex);
#define __sprt_csinh __SPRT_ID(csinh_impl)

SPRT_API float _Complex __SPRT_ID(csinhf_impl)(float _Complex);
#define __sprt_csinhf __SPRT_ID(csinhf_impl)

SPRT_API long double _Complex __SPRT_ID(csinhl_impl)(long double _Complex);
#define __sprt_csinhl __SPRT_ID(csinhl_impl)

SPRT_API double _Complex __SPRT_ID(csqrt_impl)(double _Complex);
#define __sprt_csqrt __SPRT_ID(csqrt_impl)

SPRT_API float _Complex __SPRT_ID(csqrtf_impl)(float _Complex);
#define __sprt_csqrtf __SPRT_ID(csqrtf_impl)

SPRT_API long double _Complex __SPRT_ID(csqrtl_impl)(long double _Complex);
#define __sprt_csqrtl __SPRT_ID(csqrtl_impl)

SPRT_API double _Complex __SPRT_ID(ctan_impl)(double _Complex);
#define __sprt_ctan __SPRT_ID(ctan_impl)

SPRT_API float _Complex __SPRT_ID(ctanf_impl)(float _Complex);
#define __sprt_ctanf __SPRT_ID(ctanf_impl)

SPRT_API long double _Complex __SPRT_ID(ctanl_impl)(long double _Complex);
#define __sprt_ctanl __SPRT_ID(ctanl_impl)

SPRT_API double _Complex __SPRT_ID(ctanh_impl)(double _Complex);
#define __sprt_ctanh __SPRT_ID(ctanh_impl)

SPRT_API float _Complex __SPRT_ID(ctanhf_impl)(float _Complex);
#define __sprt_ctanhf __SPRT_ID(ctanhf_impl)

SPRT_API long double _Complex __SPRT_ID(ctanhl_impl)(long double _Complex);
#define __sprt_ctanhl __SPRT_ID(ctanhl_impl)

SPRT_API double _Complex __SPRT_ID(cpow_impl)(double _Complex, double _Complex);
#define __sprt_cpow __SPRT_ID(cpow_impl)

SPRT_API float _Complex __SPRT_ID(cpowf_impl)(float _Complex, float _Complex);
#define __sprt_cpowf __SPRT_ID(cpowf_impl)

SPRT_API long double _Complex __SPRT_ID(cpowl_impl)(long double _Complex, long double _Complex);
#define __sprt_cpowl __SPRT_ID(cpowl_impl)

__SPRT_END_DECL

#endif // CORE_RUNTIME_INCLUDE_C___SPRT_COMPLEX_H_
