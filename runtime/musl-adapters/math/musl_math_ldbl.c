#include "../include/defs.h"

#pragma clang diagnostic ignored "-Wignored-pragmas"

double exp10(double x);

#if __SPRT_ARCH_ID == __SPRT_ARCH_ID_X86_64 && !SPRT_WINDOWS
#include "../musl-libc/src/math/x86_64/fabsl.c"
#include "../musl-libc/src/math/x86_64/fmodl.c"
#include "../musl-libc/src/math/x86_64/llrintl.c"
#include "../musl-libc/src/math/x86_64/lrintl.c"
#include "../musl-libc/src/math/x86_64/remainderl.c"
#include "../musl-libc/src/math/x86_64/remquol.c"
#include "../musl-libc/src/math/x86_64/rintl.c"
#include "../musl-libc/src/math/x86_64/sqrtl.c"
#else
#include "../musl-libc/src/math/acosl.c"
#include "../musl-libc/src/math/asinl.c"
#include "../musl-libc/src/math/atan2l.c"
#include "../musl-libc/src/math/atanl.c"
#include "../musl-libc/src/math/exp2l.c"
#define P __sprt_ldbl_expl_P
#define Q __sprt_ldbl_expl_Q
#include "../musl-libc/src/math/expl.c"
#undef P
#undef Q
#define toint __sprt_ldbl_ceill_toint
#include "../musl-libc/src/math/ceill.c"
#undef toint
#include "../musl-libc/src/math/fabsl.c"
#include "../musl-libc/src/math/fmodl.c"
#include "../musl-libc/src/math/llrintl.c"
#include "../musl-libc/src/math/lrintl.c"
#include "../musl-libc/src/math/expm1l.c"
#include "../musl-libc/src/math/log10l.c"
#define toint __sprt_ldbl_floorl_toint
#include "../musl-libc/src/math/floorl.c"
#undef toint
#include "../musl-libc/src/math/log1pl.c"
#include "../musl-libc/src/math/log2l.c"
#include "../musl-libc/src/math/logl.c"
#include "../musl-libc/src/math/remainderl.c"
#include "../musl-libc/src/math/remquol.c"
#define toint __sprt_ldbl_rintl_toint
#include "../musl-libc/src/math/rintl.c"
#undef toint
#include "../musl-libc/src/math/sqrtl.c"
#define toint __sprt_ldbl_truncl_toint
#include "../musl-libc/src/math/truncl.c"
#undef toint
#endif

#include "../musl-libc/src/math/acoshl.c"
#include "../musl-libc/src/math/asinhl.c"
#include "../musl-libc/src/math/atanhl.c"
#include "../musl-libc/src/math/cbrtl.c"
#include "../musl-libc/src/math/copysignl.c"
#include "../musl-libc/src/math/coshl.c"
#include "../musl-libc/src/math/__cosl.c"
#undef POLY

#include "../musl-libc/src/math/cosl.c"
#include "../musl-libc/src/math/erfl.c"
#include "../musl-libc/src/math/exp10l.c"
#include "../musl-libc/src/math/fdiml.c"

#include "../musl-libc/src/math/fmal.c"
#undef SPLIT

#include "../musl-libc/src/math/fmaxl.c"
#include "../musl-libc/src/math/fminl.c"
#include "../musl-libc/src/math/__fpclassifyl.c"
#include "../musl-libc/src/math/frexpl.c"
#include "../musl-libc/src/math/ilogbl.c"
#include "../musl-libc/src/math/__invtrigl.c"
#include "../musl-libc/src/math/ldexpl.c"
#include "../musl-libc/src/math/lgammal.c"
#include "../musl-libc/src/math/llroundl.c"
#include "../musl-libc/src/math/logbl.c"
#include "../musl-libc/src/math/lroundl.c"

#include "../musl-libc/src/math/hypotl.c"
#undef SPLIT

#define toint __sprt_ldbl_roundl_toint
#include "../musl-libc/src/math/roundl.c"
#undef toint
#define toint __sprt_ldbl_rem_pio2l_toint
#define pio4 __sprt_ldbl_rem_pio2l_pio4
#define pio4lo __sprt_ldbl_rem_pio2l_pio4lo
#include "../musl-libc/src/math/__rem_pio2l.c"
#undef toint
#undef pio4
#undef pio4lo
#include "../musl-libc/src/math/__math_invalidl.c"
#define toint __sprt_ldbl_modfl_toint
#include "../musl-libc/src/math/modfl.c"
#undef toint
#include "../musl-libc/src/math/nanl.c"
#define toint __sprt_ldbl_nearbyintl_toint
#include "../musl-libc/src/math/nearbyintl.c"
#undef toint
#include "../musl-libc/src/math/nextafterl.c"
#include "../musl-libc/src/math/nexttowardl.c"
#include "../musl-libc/src/math/__polevll.c"
#define P __sprt_ldbl_powl_P
#define Q __sprt_ldbl_powl_Q
#include "../musl-libc/src/math/powl.c"
#undef P
#undef Q
#include "../musl-libc/src/math/scalblnl.c"
#include "../musl-libc/src/math/scalbnl.c"
#include "../musl-libc/src/math/__signbitl.c"
#include "../musl-libc/src/math/sinhl.c"
#include "../musl-libc/src/math/__sinl.c"
#undef POLY

#include "../musl-libc/src/math/sinl.c"
#include "../musl-libc/src/math/tanhl.c"
#define pio4 __sprt_ldbl_tanl_pio4
#define pio4lo __sprt_ldbl_tanl_pio4lo
#include "../musl-libc/src/math/__tanl.c"
#undef pio4
#undef pio4lo
#include "../musl-libc/src/math/tanl.c"
#define P __sprt_ldbl_tgammal_P
#define Q __sprt_ldbl_tgammal_Q
#include "../musl-libc/src/math/tgammal.c"
#undef P
#undef Q

void sincos(double x, double *sin, double *cos);

#include "../musl-libc/src/math/sincosl.c"
