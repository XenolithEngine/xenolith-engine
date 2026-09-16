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

// Embox: the single-precision third of the musl libm port. See embox_math_dbl.c
// for what the port is and why it exists; the three units mirror musl's own
// per-precision split, which is what keeps its per-file static coefficients
// (Lg1..Lg7, pS0..pS5, S1..S4, ...) from colliding.

#define __SPRT_BUILD 1

#include <sprt/c/bits/__sprt_def.h>

#if SPRT_EMBOX

#include "rtos_math_prologue.h"

// aarch64 takes musl's generic C sources; the x86_64 assembly variants the
// wasm/windows adapter picks between do not apply here.
#include "../../../musl-libc/src/math/fabsf.c"
#include "../../../musl-libc/src/math/fmaf.c"
#include "../../../musl-libc/src/math/llrintf.c"
#include "../../../musl-libc/src/math/lrintf.c"
#include "../../../musl-libc/src/math/sqrtf.c"

#include "../../../musl-libc/src/math/acoshf.c"

#include "../../../musl-libc/src/math/asinhf.c"
#include "../../../musl-libc/src/math/atanf.c"
#include "../../../musl-libc/src/math/atanhf.c"
#include "../../../musl-libc/src/math/cbrtf.c"
#include "../../../musl-libc/src/math/ceilf.c"
#include "../../../musl-libc/src/math/copysignf.c"
#include "../../../musl-libc/src/math/cosf.c"
#include "../../../musl-libc/src/math/coshf.c"
#include "../../../musl-libc/src/math/erff.c"
#include "../../../musl-libc/src/math/exp2f_data.c"
#include "../../../musl-libc/src/math/__expo2f.c"
#include "../../../musl-libc/src/math/fdimf.c"
#include "../../../musl-libc/src/math/fmaxf.c"
#include "../../../musl-libc/src/math/fminf.c"
#include "../../../musl-libc/src/math/fmodf.c"
#include "../../../musl-libc/src/math/frexpf.c"
#include "../../../musl-libc/src/math/hypotf.c"
#include "../../../musl-libc/src/math/ilogbf.c"
#include "../../../musl-libc/src/math/ldexpf.c"
#include "../../../musl-libc/src/math/llroundf.c"
#include "../../../musl-libc/src/math/logbf.c"
#include "../../../musl-libc/src/math/lroundf.c"

#define pio2_hi asin_pio2_hi
#define pio2_lo asin_pio2_lo
#define pS0 asin_pS0
#define pS1 asin_pS1
#define pS2 asin_pS2
#define qS1 asin_qS1
#define R asin_R

#include "../../../musl-libc/src/math/asinf.c"

#undef pio2_hi
#undef pio2_lo
#undef pS0
#undef pS1
#undef pS2
#undef qS1
#undef R

#define pio2_hi acos_pio2_hi
#define pio2_lo acos_pio2_lo
#define pS0 acos_pS0
#define pS1 acos_pS1
#define pS2 acos_pS2
#define qS1 acos_qS1
#define R acos_R

#include "../../../musl-libc/src/math/acosf.c"

#undef pio2_hi
#undef pio2_lo
#undef pS0
#undef pS1
#undef pS2
#undef qS1
#undef R

#define top12 exp2f_top12
#include "../../../musl-libc/src/math/exp2f.c"
#undef C
#undef SHIFT
#undef top12

#define top12 expf_top12
#include "../../../musl-libc/src/math/expf.c"
#undef C
#undef N
#undef T
#undef InvLn2N
#undef SHIFT

#include "../../../musl-libc/src/math/floorf.c"
#include "../../../musl-libc/src/math/lgammaf.c"

#define pi atan2f_pi
#define ln2_hi atan2f_ln2_hi
#include "../../../musl-libc/src/math/atan2f.c"
#undef pi
#undef ln2_hi

#define pi lgammaf_r_pi
#define ln2_hi lgammaf_r_ln2_hi
#include "../../../musl-libc/src/math/lgammaf_r.c"
#undef pi
#undef ln2_hi

#define ln2_hi expm1f_ln2_hi
#define ln2_lo expm1f_ln2_lo
#define invln2 expm1f_invln2
#define Q1 expm1f_Q1
#define Q2 expm1f_Q2

#include "../../../musl-libc/src/math/expm1f.c"

#undef ln2_hi
#undef ln2_lo
#undef invln2
#undef Q1
#undef Q2

#define ivln10hi log10f_ivln10hi
#define ivln10lo log10f_ivln10lo
#define log10_2hi log10f_log10_2hi
#define log10_2lo log10f_log10_2lo
#define Lg1 log10f_Lg1
#define Lg2 log10f_Lg2
#define Lg3 log10f_Lg3
#define Lg4 log10f_Lg4

#include "../../../musl-libc/src/math/log10f.c"

#undef ivln10hi
#undef ivln10lo
#undef log10_2hi
#undef log10_2lo
#undef Lg1
#undef Lg2
#undef Lg3
#undef Lg4

#define log10_2hi log1pf_log10_2hi
#define log10_2lo log1pf_log10_2lo
#define Lg1 log1pf_Lg1
#define Lg2 log1pf_Lg2
#define Lg3 log1pf_Lg3
#define Lg4 log1pf_Lg4

#include "../../../musl-libc/src/math/log1pf.c"

#undef log10_2hi
#undef log10_2lo
#undef Lg1
#undef Lg2
#undef Lg3
#undef Lg4

#include "../../../musl-libc/src/math/log2f.c"

#undef N
#undef T
#undef A
#undef OFF

#include "../../../musl-libc/src/math/log2f_data.c"

#include "../../../musl-libc/src/math/logf.c"

#undef T
#undef A
#undef Ln2
#undef N
#undef OFF

#include "../../../musl-libc/src/math/logf_data.c"
#include "../../../musl-libc/src/math/__math_divzerof.c"
#include "../../../musl-libc/src/math/__math_invalidf.c"
#include "../../../musl-libc/src/math/modff.c"
#include "../../../musl-libc/src/math/nanf.c"
#include "../../../musl-libc/src/math/nearbyintf.c"
#include "../../../musl-libc/src/math/nextafterf.c"
#include "../../../musl-libc/src/math/nexttowardf.c"
#include "../../../musl-libc/src/math/remainderf.c"

#define toint __rem_pio2f_toint
#include "../../../musl-libc/src/math/__rem_pio2f.c"
#undef EPS
#undef toint

#include "../../../musl-libc/src/math/remquof.c"

#include "../../../musl-libc/src/math/powf.c"

#undef N
#undef T
#undef A
#undef OFF
#undef SHIFT

#define toint roundf_toint
#include "../../../musl-libc/src/math/roundf.c"
#undef EPS
#undef toint

#define s1pio2 sinf_s1pio2
#define s2pio2 sinf_s2pio2
#define s3pio2 sinf_s3pio2
#define s4pio2 sinf_s4pio2
#include "../../../musl-libc/src/math/sinf.c"
#undef s1pio2
#undef s2pio2
#undef s3pio2
#undef s4pio2

#include "../../../musl-libc/src/math/powf_data.c"
#include "../../../musl-libc/src/math/rintf.c"
#include "../../../musl-libc/src/math/scalblnf.c"
#include "../../../musl-libc/src/math/scalbnf.c"
#include "../../../musl-libc/src/math/sinhf.c"
#include "../../../musl-libc/src/math/__tandf.c"
#include "../../../musl-libc/src/math/tanf.c"
#include "../../../musl-libc/src/math/tanhf.c"
#include "../../../musl-libc/src/math/tgammaf.c"
#include "../../../musl-libc/src/math/truncf.c"

#endif // SPRT_EMBOX
