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

// Embox: the binary128 (long double) third of the musl libm port. See
// embox_math_dbl.c for what the port is and why it exists.
//
// This is the half that matters most for correctness: Embox's own long double
// support was expl/logl/powl computed in double and widened, and sqrtl was
// literally `(long double)sqrt((double)x)`. Everything on this page is musl's
// real 113-bit code.

#define __SPRT_BUILD 1

#include <sprt/c/bits/__sprt_def.h>

#if SPRT_EMBOX

#include "rtos_math_prologue.h"

double exp10(double x);

// aarch64 takes musl's generic C sources; the x86_64 assembly variants the
// wasm/windows adapter picks between do not apply here.
#include "../../../musl-libc/src/math/acosl.c"
#include "../../../musl-libc/src/math/asinl.c"
#include "../../../musl-libc/src/math/atan2l.c"
#include "../../../musl-libc/src/math/atanl.c"
#include "../../../musl-libc/src/math/exp2l.c"
#define P __sprt_ldbl_expl_P
#define Q __sprt_ldbl_expl_Q
#include "../../../musl-libc/src/math/expl.c"
#undef P
#undef Q
#define toint __sprt_ldbl_ceill_toint
#include "../../../musl-libc/src/math/ceill.c"
#undef toint
#include "../../../musl-libc/src/math/fabsl.c"
#include "../../../musl-libc/src/math/fmodl.c"
#include "../../../musl-libc/src/math/llrintl.c"
#include "../../../musl-libc/src/math/lrintl.c"
#include "../../../musl-libc/src/math/expm1l.c"
#include "../../../musl-libc/src/math/log10l.c"
#define toint __sprt_ldbl_floorl_toint
#include "../../../musl-libc/src/math/floorl.c"
#undef toint
#include "../../../musl-libc/src/math/log1pl.c"
#include "../../../musl-libc/src/math/log2l.c"
#include "../../../musl-libc/src/math/logl.c"
#include "../../../musl-libc/src/math/remainderl.c"
#include "../../../musl-libc/src/math/remquol.c"
#define toint __sprt_ldbl_rintl_toint
#include "../../../musl-libc/src/math/rintl.c"
#undef toint
#include "../../../musl-libc/src/math/sqrtl.c"
#define toint __sprt_ldbl_truncl_toint
#include "../../../musl-libc/src/math/truncl.c"
#undef toint

#include "../../../musl-libc/src/math/acoshl.c"
#include "../../../musl-libc/src/math/asinhl.c"
#include "../../../musl-libc/src/math/atanhl.c"
#include "../../../musl-libc/src/math/cbrtl.c"
#include "../../../musl-libc/src/math/copysignl.c"
#include "../../../musl-libc/src/math/coshl.c"
#include "../../../musl-libc/src/math/__cosl.c"
#undef POLY

#include "../../../musl-libc/src/math/cosl.c"
#include "../../../musl-libc/src/math/erfl.c"
#include "../../../musl-libc/src/math/fdiml.c"

#include "../../../musl-libc/src/math/fmal.c"
#undef SPLIT

#include "../../../musl-libc/src/math/fmaxl.c"
#include "../../../musl-libc/src/math/fminl.c"
#include "../../../musl-libc/src/math/frexpl.c"
#include "../../../musl-libc/src/math/ilogbl.c"
#include "../../../musl-libc/src/math/__invtrigl.c"
#include "../../../musl-libc/src/math/ldexpl.c"
#include "../../../musl-libc/src/math/lgammal.c"
#include "../../../musl-libc/src/math/llroundl.c"
#include "../../../musl-libc/src/math/logbl.c"
#include "../../../musl-libc/src/math/lroundl.c"

#include "../../../musl-libc/src/math/hypotl.c"
#undef SPLIT

#define toint __sprt_ldbl_roundl_toint
#include "../../../musl-libc/src/math/roundl.c"
#undef toint
#define toint __sprt_ldbl_rem_pio2l_toint
#define pio4 __sprt_ldbl_rem_pio2l_pio4
#define pio4lo __sprt_ldbl_rem_pio2l_pio4lo
#include "../../../musl-libc/src/math/__rem_pio2l.c"
#undef toint
#undef pio4
#undef pio4lo
#include "../../../musl-libc/src/math/__math_invalidl.c"
#define toint __sprt_ldbl_modfl_toint
#include "../../../musl-libc/src/math/modfl.c"
#undef toint
#include "../../../musl-libc/src/math/nanl.c"
#define toint __sprt_ldbl_nearbyintl_toint
#include "../../../musl-libc/src/math/nearbyintl.c"
#undef toint
#include "../../../musl-libc/src/math/nextafterl.c"
#include "../../../musl-libc/src/math/nexttowardl.c"
#include "../../../musl-libc/src/math/__polevll.c"
#define P __sprt_ldbl_powl_P
#define Q __sprt_ldbl_powl_Q
#include "../../../musl-libc/src/math/powl.c"
#undef P
#undef Q
#include "../../../musl-libc/src/math/scalblnl.c"
#include "../../../musl-libc/src/math/scalbnl.c"
#include "../../../musl-libc/src/math/sinhl.c"
#include "../../../musl-libc/src/math/__sinl.c"
#undef POLY

#include "../../../musl-libc/src/math/sinl.c"
#include "../../../musl-libc/src/math/tanhl.c"
#define pio4 __sprt_ldbl_tanl_pio4
#define pio4lo __sprt_ldbl_tanl_pio4lo
#include "../../../musl-libc/src/math/__tanl.c"
#undef pio4
#undef pio4lo
#include "../../../musl-libc/src/math/tanl.c"
#define P __sprt_ldbl_tgammal_P
#define Q __sprt_ldbl_tgammal_Q
#include "../../../musl-libc/src/math/tgammal.c"
#undef P
#undef Q

void sincos(double x, double *sin, double *cos);

#endif // SPRT_EMBOX
