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

#define __SPRT_BUILD 1

#include <sprt/c/__sprt_complex.h>

#if __STDC_HOSTED__ == 0
#include "complex.h"
#else
#include <complex.h>
#endif

// Host: forward each __sprt_*_impl to the native libm complex function. The same
// translation unit serves the freestanding target, where ::cabs/::cexp/... resolve
// (through the umbrella's bare prototypes) to the musl-provided public symbols.

#if SPRT_ANDROID
// Bionic only gained these <complex.h> entries at API 26 (the clog/cpow families
// and every long double variant), but the runtime targets API 24, so they are
// absent from the platform libm and ::clog/::cpow/::cacosl/... do not even exist.
// Supply them from musl — the very sources runtime/libc_impl already uses — pulled
// in right here so they are defined inside this libc_wrapper TU. Each is renamed to
// __sprt_musl_* below; that rename deliberately stays in effect for the rest of the
// file, so the matching *_impl forwarders pick up the musl version instead of the
// missing ::name. The borrowed code's only outside calls are to API<=23
// complex/real-math functions (cabs, cexp, ctan, creall, logl, csqrtl, ...), which
// are left unrenamed and resolve to Bionic's libm at link time.
extern "C" {
#define clog __sprt_musl_clog
#define clogf __sprt_musl_clogf
#define clogl __sprt_musl_clogl
#define cpow __sprt_musl_cpow
#define cpowf __sprt_musl_cpowf
#define cpowl __sprt_musl_cpowl
#define cexpl __sprt_musl_cexpl
#define cacosl __sprt_musl_cacosl
#define cacoshl __sprt_musl_cacoshl
#define casinl __sprt_musl_casinl
#define casinhl __sprt_musl_casinhl
#define catanl __sprt_musl_catanl
#define catanhl __sprt_musl_catanhl
#define ccosl __sprt_musl_ccosl
#define ccoshl __sprt_musl_ccoshl
#define csinl __sprt_musl_csinl
#define csinhl __sprt_musl_csinhl
#define ctanl __sprt_musl_ctanl
#define ctanhl __sprt_musl_ctanhl

// Forward declarations (renamed by the macros above) so the borrowed sources can
// call one another regardless of include order — C++ has no implicit declaration.
double complex clog(double complex);
float complex clogf(float complex);
long double complex clogl(long double complex);
double complex cpow(double complex, double complex);
float complex cpowf(float complex, float complex);
long double complex cpowl(long double complex, long double complex);
long double complex cexpl(long double complex);
long double complex cacosl(long double complex);
long double complex cacoshl(long double complex);
long double complex casinl(long double complex);
long double complex casinhl(long double complex);
long double complex catanl(long double complex);
long double complex catanhl(long double complex);
long double complex ccosl(long double complex);
long double complex ccoshl(long double complex);
long double complex csinl(long double complex);
long double complex csinhl(long double complex);
long double complex ctanl(long double complex);
long double complex ctanhl(long double complex);

#include "../../musl-libc/src/complex/clog.c"
#include "../../musl-libc/src/complex/clogf.c"
#include "../../musl-libc/src/complex/cpow.c"
#include "../../musl-libc/src/complex/cpowf.c"
#include "../../musl-libc/src/complex/cexpl.c"
#include "../../musl-libc/src/complex/clogl.c"
#include "../../musl-libc/src/complex/cpowl.c"
#include "../../musl-libc/src/complex/cacosl.c"
#include "../../musl-libc/src/complex/cacoshl.c"
#include "../../musl-libc/src/complex/casinl.c"
#include "../../musl-libc/src/complex/casinhl.c"
#include "../../musl-libc/src/complex/catanl.c"
#include "../../musl-libc/src/complex/catanhl.c"
#include "../../musl-libc/src/complex/ccosl.c"
#include "../../musl-libc/src/complex/ccoshl.c"
#include "../../musl-libc/src/complex/csinl.c"
#include "../../musl-libc/src/complex/csinhl.c"
#include "../../musl-libc/src/complex/ctanl.c"
#include "../../musl-libc/src/complex/ctanhl.c"
} // extern "C"
#endif // SPRT_ANDROID

namespace sprt {

__SPRT_C_FUNC double __SPRT_ID(cabs_impl)(double _Complex __z) { return ::cabs(__z); }
__SPRT_C_FUNC float __SPRT_ID(cabsf_impl)(float _Complex __z) { return ::cabsf(__z); }
__SPRT_C_FUNC long double __SPRT_ID(cabsl_impl)(long double _Complex __z) { return ::cabsl(__z); }
__SPRT_C_FUNC double __SPRT_ID(carg_impl)(double _Complex __z) { return ::carg(__z); }
__SPRT_C_FUNC float __SPRT_ID(cargf_impl)(float _Complex __z) { return ::cargf(__z); }
__SPRT_C_FUNC long double __SPRT_ID(cargl_impl)(long double _Complex __z) { return ::cargl(__z); }
__SPRT_C_FUNC double __SPRT_ID(cimag_impl)(double _Complex __z) { return ::cimag(__z); }
__SPRT_C_FUNC float __SPRT_ID(cimagf_impl)(float _Complex __z) { return ::cimagf(__z); }
__SPRT_C_FUNC long double __SPRT_ID(cimagl_impl)(long double _Complex __z) { return ::cimagl(__z); }
__SPRT_C_FUNC double __SPRT_ID(creal_impl)(double _Complex __z) { return ::creal(__z); }
__SPRT_C_FUNC float __SPRT_ID(crealf_impl)(float _Complex __z) { return ::crealf(__z); }
__SPRT_C_FUNC long double __SPRT_ID(creall_impl)(long double _Complex __z) { return ::creall(__z); }
__SPRT_C_FUNC double _Complex __SPRT_ID(cacos_impl)(double _Complex __z) { return ::cacos(__z); }
__SPRT_C_FUNC float _Complex __SPRT_ID(cacosf_impl)(float _Complex __z) { return ::cacosf(__z); }
__SPRT_C_FUNC long double _Complex __SPRT_ID(cacosl_impl)(long double _Complex __z) { return ::cacosl(__z); }
__SPRT_C_FUNC double _Complex __SPRT_ID(cacosh_impl)(double _Complex __z) { return ::cacosh(__z); }
__SPRT_C_FUNC float _Complex __SPRT_ID(cacoshf_impl)(float _Complex __z) { return ::cacoshf(__z); }
__SPRT_C_FUNC long double _Complex __SPRT_ID(cacoshl_impl)(long double _Complex __z) { return ::cacoshl(__z); }
__SPRT_C_FUNC double _Complex __SPRT_ID(casin_impl)(double _Complex __z) { return ::casin(__z); }
__SPRT_C_FUNC float _Complex __SPRT_ID(casinf_impl)(float _Complex __z) { return ::casinf(__z); }
__SPRT_C_FUNC long double _Complex __SPRT_ID(casinl_impl)(long double _Complex __z) { return ::casinl(__z); }
__SPRT_C_FUNC double _Complex __SPRT_ID(casinh_impl)(double _Complex __z) { return ::casinh(__z); }
__SPRT_C_FUNC float _Complex __SPRT_ID(casinhf_impl)(float _Complex __z) { return ::casinhf(__z); }
__SPRT_C_FUNC long double _Complex __SPRT_ID(casinhl_impl)(long double _Complex __z) { return ::casinhl(__z); }
__SPRT_C_FUNC double _Complex __SPRT_ID(catan_impl)(double _Complex __z) { return ::catan(__z); }
__SPRT_C_FUNC float _Complex __SPRT_ID(catanf_impl)(float _Complex __z) { return ::catanf(__z); }
__SPRT_C_FUNC long double _Complex __SPRT_ID(catanl_impl)(long double _Complex __z) { return ::catanl(__z); }
__SPRT_C_FUNC double _Complex __SPRT_ID(catanh_impl)(double _Complex __z) { return ::catanh(__z); }
__SPRT_C_FUNC float _Complex __SPRT_ID(catanhf_impl)(float _Complex __z) { return ::catanhf(__z); }
__SPRT_C_FUNC long double _Complex __SPRT_ID(catanhl_impl)(long double _Complex __z) { return ::catanhl(__z); }
__SPRT_C_FUNC double _Complex __SPRT_ID(ccos_impl)(double _Complex __z) { return ::ccos(__z); }
__SPRT_C_FUNC float _Complex __SPRT_ID(ccosf_impl)(float _Complex __z) { return ::ccosf(__z); }
__SPRT_C_FUNC long double _Complex __SPRT_ID(ccosl_impl)(long double _Complex __z) { return ::ccosl(__z); }
__SPRT_C_FUNC double _Complex __SPRT_ID(ccosh_impl)(double _Complex __z) { return ::ccosh(__z); }
__SPRT_C_FUNC float _Complex __SPRT_ID(ccoshf_impl)(float _Complex __z) { return ::ccoshf(__z); }
__SPRT_C_FUNC long double _Complex __SPRT_ID(ccoshl_impl)(long double _Complex __z) { return ::ccoshl(__z); }
__SPRT_C_FUNC double _Complex __SPRT_ID(cexp_impl)(double _Complex __z) { return ::cexp(__z); }
__SPRT_C_FUNC float _Complex __SPRT_ID(cexpf_impl)(float _Complex __z) { return ::cexpf(__z); }
__SPRT_C_FUNC long double _Complex __SPRT_ID(cexpl_impl)(long double _Complex __z) { return ::cexpl(__z); }
__SPRT_C_FUNC double _Complex __SPRT_ID(clog_impl)(double _Complex __z) { return ::clog(__z); }
__SPRT_C_FUNC float _Complex __SPRT_ID(clogf_impl)(float _Complex __z) { return ::clogf(__z); }
__SPRT_C_FUNC long double _Complex __SPRT_ID(clogl_impl)(long double _Complex __z) { return ::clogl(__z); }
__SPRT_C_FUNC double _Complex __SPRT_ID(conj_impl)(double _Complex __z) { return ::conj(__z); }
__SPRT_C_FUNC float _Complex __SPRT_ID(conjf_impl)(float _Complex __z) { return ::conjf(__z); }
__SPRT_C_FUNC long double _Complex __SPRT_ID(conjl_impl)(long double _Complex __z) { return ::conjl(__z); }
__SPRT_C_FUNC double _Complex __SPRT_ID(cproj_impl)(double _Complex __z) { return ::cproj(__z); }
__SPRT_C_FUNC float _Complex __SPRT_ID(cprojf_impl)(float _Complex __z) { return ::cprojf(__z); }
__SPRT_C_FUNC long double _Complex __SPRT_ID(cprojl_impl)(long double _Complex __z) { return ::cprojl(__z); }
__SPRT_C_FUNC double _Complex __SPRT_ID(csin_impl)(double _Complex __z) { return ::csin(__z); }
__SPRT_C_FUNC float _Complex __SPRT_ID(csinf_impl)(float _Complex __z) { return ::csinf(__z); }
__SPRT_C_FUNC long double _Complex __SPRT_ID(csinl_impl)(long double _Complex __z) { return ::csinl(__z); }
__SPRT_C_FUNC double _Complex __SPRT_ID(csinh_impl)(double _Complex __z) { return ::csinh(__z); }
__SPRT_C_FUNC float _Complex __SPRT_ID(csinhf_impl)(float _Complex __z) { return ::csinhf(__z); }
__SPRT_C_FUNC long double _Complex __SPRT_ID(csinhl_impl)(long double _Complex __z) { return ::csinhl(__z); }
__SPRT_C_FUNC double _Complex __SPRT_ID(csqrt_impl)(double _Complex __z) { return ::csqrt(__z); }
__SPRT_C_FUNC float _Complex __SPRT_ID(csqrtf_impl)(float _Complex __z) { return ::csqrtf(__z); }
__SPRT_C_FUNC long double _Complex __SPRT_ID(csqrtl_impl)(long double _Complex __z) { return ::csqrtl(__z); }
__SPRT_C_FUNC double _Complex __SPRT_ID(ctan_impl)(double _Complex __z) { return ::ctan(__z); }
__SPRT_C_FUNC float _Complex __SPRT_ID(ctanf_impl)(float _Complex __z) { return ::ctanf(__z); }
__SPRT_C_FUNC long double _Complex __SPRT_ID(ctanl_impl)(long double _Complex __z) { return ::ctanl(__z); }
__SPRT_C_FUNC double _Complex __SPRT_ID(ctanh_impl)(double _Complex __z) { return ::ctanh(__z); }
__SPRT_C_FUNC float _Complex __SPRT_ID(ctanhf_impl)(float _Complex __z) { return ::ctanhf(__z); }
__SPRT_C_FUNC long double _Complex __SPRT_ID(ctanhl_impl)(long double _Complex __z) { return ::ctanhl(__z); }
__SPRT_C_FUNC double _Complex __SPRT_ID(cpow_impl)(double _Complex __x, double _Complex __y) { return ::cpow(__x, __y); }
__SPRT_C_FUNC float _Complex __SPRT_ID(cpowf_impl)(float _Complex __x, float _Complex __y) { return ::cpowf(__x, __y); }
__SPRT_C_FUNC long double _Complex __SPRT_ID(cpowl_impl)(long double _Complex __x, long double _Complex __y) { return ::cpowl(__x, __y); }

} // namespace sprt
