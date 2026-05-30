#include "../include/defs.h"

#if __SPRT_ARCH_ID == __SPRT_ARCH_ID_X86_64
#include "../musl-libc/src/math/x86_64/fabs.c"
#include "../musl-libc/src/math/x86_64/fma.c"
#include "../musl-libc/src/math/x86_64/llrint.c"
#include "../musl-libc/src/math/x86_64/lrint.c"
#include "../musl-libc/src/math/x86_64/sqrt.c"
#else
#include "../musl-libc/src/math/fabs.c"
#include "../musl-libc/src/math/fma.c"
#include "../musl-libc/src/math/llrint.c"
#include "../musl-libc/src/math/lrint.c"
#include "../musl-libc/src/math/sqrt.c"
#endif

#include "../musl-libc/src/math/acosh.c"
#include "../musl-libc/src/math/asinh.c"

#define pi atan2_pi
#define pi_lo atan2_pi_lo
#include "../musl-libc/src/math/atan2.c"
#undef pi
#undef pi_lo

#include "../musl-libc/src/math/atan.c"
#include "../musl-libc/src/math/atanh.c"
#include "../musl-libc/src/math/cbrt.c"

#define toint ceil_toint
#include "../musl-libc/src/math/ceil.c"
#undef toint

#include "../musl-libc/src/math/copysign.c"
#include "../musl-libc/src/math/__cos.c"
#include "../musl-libc/src/math/cos.c"
#include "../musl-libc/src/math/cosh.c"
#include "../musl-libc/src/math/erf.c"
#include "../musl-libc/src/math/exp10.c"

#define specialcase exp2_specialcase
#define top12 exp2_top12
#include "../musl-libc/src/math/exp2.c"
#undef specialcase
#undef top12
#undef N
#undef Shift
#undef T
#undef C1
#undef C2
#undef C3
#undef C4
#undef C5

#define o_threshold expm1_o_threshold
#define ln2_hi expm1_ln2_hi
#define ln2_lo expm1_ln2_lo
#define invln2 expm1_invln2
#define Q1 expm1_Q1
#define Q2 expm1_Q2
#define Q3 expm1_Q3
#define Q4 expm1_Q4
#define Q5 expm1_Q5
#include "../musl-libc/src/math/expm1.c"
#undef o_threshold
#undef ln2_hi
#undef ln2_lo
#undef invln2
#undef Q1
#undef Q2
#undef Q3
#undef Q4
#undef Q5

#include "../musl-libc/src/math/__expo2.c"
#include "../musl-libc/src/math/fdim.c"
#include "../musl-libc/src/math/finite.c"
#include "../musl-libc/src/math/fmax.c"
#include "../musl-libc/src/math/fmin.c"
#include "../musl-libc/src/math/fmod.c"
#include "../musl-libc/src/math/__fpclassify.c"
#include "../musl-libc/src/math/frexp.c"
#include "../musl-libc/src/math/hypot.c"
#include "../musl-libc/src/math/ilogb.c"
#include "../musl-libc/src/math/ldexp.c"
#include "../musl-libc/src/math/lgamma.c"
#include "../musl-libc/src/math/llround.c"

#define ivln10hi log10_ivln10hi
#define ivln10lo  log10_ivln10lo
#define log10_2hi log10_log10_2hi
#define log10_2lo log10_log10_2lo
#define Lg1 log10_Lg1
#define Lg2 log10_Lg2
#define Lg3 log10_Lg3
#define Lg4 log10_Lg4
#define Lg5 log10_Lg5
#define Lg6 log10_Lg6
#define Lg7 log10_Lg7
#include "../musl-libc/src/math/log10.c"
#undef ivln10hi
#undef ivln10lo
#undef log10_2hi
#undef log10_2lo
#undef Lg1
#undef Lg2
#undef Lg3
#undef Lg4
#undef Lg5
#undef Lg6
#undef Lg7

#include "../musl-libc/src/math/logb.c"
#include "../musl-libc/src/math/lround.c"

#define pio2_hi asin_pio2_hi
#define pio2_lo asin_pio2_lo
#define pS0 asin_pS0
#define pS1 asin_pS1
#define pS2 asin_pS2
#define pS3 asin_pS3
#define pS4 asin_pS4
#define pS5 asin_pS5
#define qS1 asin_qS1
#define qS2 asin_qS2
#define qS3 asin_qS3
#define qS4 asin_qS4
#define R asin_R

#include "../musl-libc/src/math/asin.c"

#undef pio2_hi
#undef pio2_lo
#undef pS0
#undef pS1
#undef pS2
#undef pS3
#undef pS4
#undef pS5
#undef qS1
#undef qS2
#undef qS3
#undef qS4
#undef R

#define pio2_hi acos_pio2_hi
#define pio2_lo acos_pio2_lo
#define pS0 acos_pS0
#define pS1 acos_pS1
#define pS2 acos_pS2
#define pS3 acos_pS3
#define pS4 acos_pS4
#define pS5 acos_pS5
#define qS1 acos_qS1
#define qS2 acos_qS2
#define qS3 acos_qS3
#define qS4 acos_qS4
#define R acos_R

#include "../musl-libc/src/math/acos.c"

#undef pio2_hi
#undef pio2_lo
#undef pS0
#undef pS1
#undef pS2
#undef pS3
#undef pS4
#undef pS5
#undef qS1
#undef qS2
#undef qS3
#undef qS4
#undef R

#define specialcase exp_specialcase
#define top12 exp_top12
#include "../musl-libc/src/math/exp.c"
#undef specialcase
#undef top12
#undef N
#undef Shift
#undef T
#undef C1
#undef C2
#undef C3
#undef C4
#undef C5

#include "../musl-libc/src/math/exp_data.c"
#undef N

#define toint floor_toint
#include "../musl-libc/src/math/floor.c"
#undef toint

#define pi lgamma_r_pi
#define pi_lo lgamma_r_pi_lo
#include "../musl-libc/src/math/lgamma_r.c"
#undef pi
#undef pi_lo

#define ln2_hi log1p_ln2_hi
#define ln2_lo log1p_ln2_lo
#define Lg1 log1p_Lg1
#define Lg2 log1p_Lg2
#define Lg3 log1p_Lg3
#define Lg4 log1p_Lg4
#define Lg5 log1p_Lg5
#define Lg6 log1p_Lg6
#define Lg7 log1p_Lg7
#include "../musl-libc/src/math/log1p.c"
#undef ln2_hi
#undef ln2_lo
#undef Lg1
#undef Lg2
#undef Lg3
#undef Lg4
#undef Lg5
#undef Lg6
#undef Lg7

#define top16 log2_top16
#include "../musl-libc/src/math/log2.c"
#undef top16
#undef T
#undef T2
#undef B
#undef A
#undef InvLn2hi
#undef InvLn2lo
#undef N
#undef OFF
#undef LO
#undef HI

#include "../musl-libc/src/math/log2_data.c"
#undef N

#define toint round_toint
#include "../musl-libc/src/math/round.c"
#undef toint
#undef EPS

#define top16 log_top16
#include "../musl-libc/src/math/log.c"
#undef top16
#undef T
#undef T2
#undef B
#undef A
#undef Ln2hi
#undef Ln2lo
#undef N
#undef OFF
#undef LO
#undef HI

#include "../musl-libc/src/math/log_data.c"
#undef N

#include "../musl-libc/src/math/__math_divzero.c"
#include "../musl-libc/src/math/__math_invalid.c"
#include "../musl-libc/src/math/__math_oflow.c"
#include "../musl-libc/src/math/__math_uflow.c"
#include "../musl-libc/src/math/__math_xflow.c"
#include "../musl-libc/src/math/modf.c"
#include "../musl-libc/src/math/nan.c"
#include "../musl-libc/src/math/nearbyint.c"
#include "../musl-libc/src/math/nextafter.c"
#include "../musl-libc/src/math/nexttoward.c"
#include "../musl-libc/src/math/remainder.c"

#define toint __rem_pio2_toint
#include "../musl-libc/src/math/__rem_pio2.c"
#undef toint

#include "../musl-libc/src/math/__rem_pio2_large.c"
#include "../musl-libc/src/math/remquo.c"

#include "../musl-libc/src/math/pow.c"
#undef T
#undef A
#undef Ln2hi
#undef Ln2lo
#undef N
#undef OFF

#include "../musl-libc/src/math/rint.c"

#include "../musl-libc/src/math/pow_data.c"
#include "../musl-libc/src/math/scalb.c"
#include "../musl-libc/src/math/scalbln.c"
#include "../musl-libc/src/math/scalbn.c"
#include "../musl-libc/src/math/__signbit.c"
#include "../musl-libc/src/math/signgam.c"
#include "../musl-libc/src/math/significand.c"
#include "../musl-libc/src/math/__sin.c"
#include "../musl-libc/src/math/sin.c"
#include "../musl-libc/src/math/sincos.c"
#include "../musl-libc/src/math/sinh.c"
#include "../musl-libc/src/math/sqrt_data.c"

#define pio4 __tan_pio4
#include "../musl-libc/src/math/__tan.c"
#undef pio4

#include "../musl-libc/src/math/tan.c"
#include "../musl-libc/src/math/tanh.c"
#include "../musl-libc/src/math/trunc.c"
#undef N

#include "../musl-libc/src/math/tgamma.c"
