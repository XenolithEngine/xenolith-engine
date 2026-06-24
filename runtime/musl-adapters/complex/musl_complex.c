#define __SPRT_BUILD

#include "../include/defs.h"

// musl's complex math, compiled into the freestanding libc_impl. Several source
// files declare file-static helpers with colliding names (`huge`, `k`, `kln2`)
// which would clash once everything is pulled into this single translation unit;
// rename them per-include. The __cexp/__cexpf reduction helpers are included
// first because cexp/csin/ccos/... reference them.

#define k __cexp_k
#define kln2 __cexp_kln2
#include "../../musl-libc/src/complex/__cexp.c"
#undef k
#undef kln2

#define k __cexpf_k
#define kln2 __cexpf_kln2
#include "../../musl-libc/src/complex/__cexpf.c"
#undef k
#undef kln2

// --- magnitude / argument / parts (real results, no extra statics) ---
#include "../../musl-libc/src/complex/cabs.c"
#include "../../musl-libc/src/complex/cabsf.c"
#include "../../musl-libc/src/complex/cabsl.c"
#include "../../musl-libc/src/complex/carg.c"
#include "../../musl-libc/src/complex/cargf.c"
#include "../../musl-libc/src/complex/cargl.c"
#include "../../musl-libc/src/complex/cimag.c"
#include "../../musl-libc/src/complex/cimagf.c"
#include "../../musl-libc/src/complex/cimagl.c"
#include "../../musl-libc/src/complex/creal.c"
#include "../../musl-libc/src/complex/crealf.c"
#include "../../musl-libc/src/complex/creall.c"
#include "../../musl-libc/src/complex/conj.c"
#include "../../musl-libc/src/complex/conjf.c"
#include "../../musl-libc/src/complex/conjl.c"
#include "../../musl-libc/src/complex/cproj.c"
#include "../../musl-libc/src/complex/cprojf.c"
#include "../../musl-libc/src/complex/cprojl.c"

// --- exp / log / pow / sqrt ---
#define exp_ovfl cexp_exp_ovfl
#define cexp_ovfl cexp_cexp_ovfl
#include "../../musl-libc/src/complex/cexp.c"
#undef exp_ovfl
#undef cexp_ovfl
#define exp_ovfl cexpf_exp_ovfl
#define cexp_ovfl cexpf_cexp_ovfl
#include "../../musl-libc/src/complex/cexpf.c"
#undef exp_ovfl
#undef cexp_ovfl
#include "../../musl-libc/src/complex/cexpl.c"
#include "../../musl-libc/src/complex/clog.c"
#include "../../musl-libc/src/complex/clogf.c"
#include "../../musl-libc/src/complex/clogl.c"
#include "../../musl-libc/src/complex/cpow.c"
#include "../../musl-libc/src/complex/cpowf.c"
#include "../../musl-libc/src/complex/cpowl.c"
#include "../../musl-libc/src/complex/csqrt.c"
#include "../../musl-libc/src/complex/csqrtf.c"
#include "../../musl-libc/src/complex/csqrtl.c"

// --- trig ---
#define float_pi_2 cacosf_float_pi_2
#include "../../musl-libc/src/complex/cacosf.c"
#undef float_pi_2
#include "../../musl-libc/src/complex/cacos.c"
#include "../../musl-libc/src/complex/cacosl.c"
#include "../../musl-libc/src/complex/casin.c"
#include "../../musl-libc/src/complex/casinf.c"
#include "../../musl-libc/src/complex/casinl.c"
#include "../../musl-libc/src/complex/catan.c"
#include "../../musl-libc/src/complex/catanf.c"
#include "../../musl-libc/src/complex/catanl.c"
#include "../../musl-libc/src/complex/ccos.c"
#include "../../musl-libc/src/complex/ccosf.c"
#include "../../musl-libc/src/complex/ccosl.c"
#include "../../musl-libc/src/complex/csin.c"
#include "../../musl-libc/src/complex/csinf.c"
#include "../../musl-libc/src/complex/csinl.c"
#include "../../musl-libc/src/complex/ctan.c"
#include "../../musl-libc/src/complex/ctanf.c"
#include "../../musl-libc/src/complex/ctanl.c"

// --- hyperbolic (ccosh/csinh + f variants carry a `huge` static) ---
#include "../../musl-libc/src/complex/cacosh.c"
#include "../../musl-libc/src/complex/cacoshf.c"
#include "../../musl-libc/src/complex/cacoshl.c"
#include "../../musl-libc/src/complex/casinh.c"
#include "../../musl-libc/src/complex/casinhf.c"
#include "../../musl-libc/src/complex/casinhl.c"
#include "../../musl-libc/src/complex/catanh.c"
#include "../../musl-libc/src/complex/catanhf.c"
#include "../../musl-libc/src/complex/catanhl.c"
#define huge ccosh_huge
#include "../../musl-libc/src/complex/ccosh.c"
#undef huge
#define huge ccoshf_huge
#include "../../musl-libc/src/complex/ccoshf.c"
#undef huge
#include "../../musl-libc/src/complex/ccoshl.c"
#define huge csinh_huge
#include "../../musl-libc/src/complex/csinh.c"
#undef huge
#define huge csinhf_huge
#include "../../musl-libc/src/complex/csinhf.c"
#undef huge
#include "../../musl-libc/src/complex/csinhl.c"
#include "../../musl-libc/src/complex/ctanh.c"
#include "../../musl-libc/src/complex/ctanhf.c"
#include "../../musl-libc/src/complex/ctanhl.c"
