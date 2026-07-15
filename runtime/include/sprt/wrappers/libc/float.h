#include <sprt/c/__sprt_float.h>
#include <sprt/c/__sprt_math.h>

#if __STDC_HOSTED__ == 0 || !defined(__SPRT_BUILD)
#ifndef FP_NAN
#define FP_NAN __SPRT_FP_NAN
#define FP_INFINITE __SPRT_FP_INFINITE
#define FP_ZERO __SPRT_FP_ZERO
#define FP_SUBNORMAL __SPRT_FP_SUBNORMAL
#define FP_NORMAL __SPRT_FP_NORMAL
#endif

// Type-shared <float.h> macros. In the __SPRT_BUILD path these come from the
// compiler's own <float.h> via #include_next; on the app / libc++ / freestanding
// path that header is never reached (this wrapper does not chain to it), so define
// them here from the compiler predefines / __sprt_float.h helpers. Guarded so an
// upstream <float.h> that did define them wins.
#ifndef FLT_RADIX
#define FLT_RADIX __FLT_RADIX__
#endif
#ifndef FLT_ROUNDS
#define FLT_ROUNDS __SPRT_FLT_ROUNDS
#endif
#ifndef FLT_EVAL_METHOD
#define FLT_EVAL_METHOD __SPRT_FLT_EVAL_METHOD
#endif
#ifndef DECIMAL_DIG
#define DECIMAL_DIG __SPRT_DECIMAL_DIG
#endif

#define LDBL_DECIMAL_DIG __SPRT_LDBL_DECIMAL_DIG
#define LDBL_DENORM_MIN __SPRT_LDBL_DENORM_MIN
#define LDBL_DIG __SPRT_LDBL_DIG
#define LDBL_EPSILON __SPRT_LDBL_EPSILON
#define LDBL_HAS_DENORM __SPRT_LDBL_HAS_DENORM
#define LDBL_HAS_SUBNORM __SPRT_LDBL_HAS_SUBNORM
#define LDBL_HAS_INFINITY __SPRT_LDBL_HAS_INFINITY
#define LDBL_HAS_QUIET_NAN __SPRT_LDBL_HAS_QUIET_NAN
#define LDBL_MANT_DIG __SPRT_LDBL_MANT_DIG
#define LDBL_MAX_10_EXP __SPRT_LDBL_MAX_10_EXP
#define LDBL_MAX_EXP __SPRT_LDBL_MAX_EXP
#define LDBL_MAX __SPRT_LDBL_MAX
#define LDBL_MIN_10_EXP __SPRT_LDBL_MIN_10_EXP
#define LDBL_MIN_EXP __SPRT_LDBL_MIN_EXP
#define LDBL_MIN __SPRT_LDBL_MIN
#define LDBL_NORM_MAX __SPRT_LDBL_NORM_MAX
#define LDBL_TRUE_MIN __SPRT_LDBL_TRUE_MIN

#define DBL_DECIMAL_DIG __SPRT_DBL_DECIMAL_DIG
#define DBL_DENORM_MIN __SPRT_DBL_DENORM_MIN
#define DBL_DIG __SPRT_DBL_DIG
#define DBL_EPSILON __SPRT_DBL_EPSILON
#define DBL_HAS_DENORM __SPRT_DBL_HAS_DENORM
#define DBL_HAS_SUBNORM __SPRT_DBL_HAS_SUBNORM
#define DBL_HAS_INFINITY __SPRT_DBL_HAS_INFINITY
#define DBL_HAS_QUIET_NAN __SPRT_DBL_HAS_QUIET_NAN
#define DBL_MANT_DIG __SPRT_DBL_MANT_DIG
#define DBL_MAX_10_EXP __SPRT_DBL_MAX_10_EXP
#define DBL_MAX_EXP __SPRT_DBL_MAX_EXP
#define DBL_MAX __SPRT_DBL_MAX
#define DBL_MIN_10_EXP __SPRT_DBL_MIN_10_EXP
#define DBL_MIN_EXP __SPRT_DBL_MIN_EXP
#define DBL_MIN __SPRT_DBL_MIN
#define DBL_NORM_MAX __SPRT_DBL_NORM_MAX
#define DBL_TRUE_MIN __SPRT_DBL_TRUE_MIN

#define FLT_DECIMAL_DIG __SPRT_FLT_DECIMAL_DIG
#define FLT_DENORM_MIN __SPRT_FLT_DENORM_MIN
#define FLT_DIG __SPRT_FLT_DIG
#define FLT_EPSILON __SPRT_FLT_EPSILON
#define FLT_HAS_DENORM __SPRT_FLT_HAS_DENORM
#define FLT_HAS_SUBNORM __SPRT_FLT_HAS_SUBNORM
#define FLT_HAS_INFINITY __SPRT_FLT_HAS_INFINITY
#define FLT_HAS_QUIET_NAN __SPRT_FLT_HAS_QUIET_NAN
#define FLT_MANT_DIG __SPRT_FLT_MANT_DIG
#define FLT_MAX_10_EXP __SPRT_FLT_MAX_10_EXP
#define FLT_MAX_EXP __SPRT_FLT_MAX_EXP
#define FLT_MAX __SPRT_FLT_MAX
#define FLT_MIN_10_EXP __SPRT_FLT_MIN_10_EXP
#define FLT_MIN_EXP __SPRT_FLT_MIN_EXP
#define FLT_MIN __SPRT_FLT_MIN
#define FLT_NORM_MAX __SPRT_FLT_NORM_MAX
#define FLT_TRUE_MIN __SPRT_FLT_TRUE_MIN

#if SPRT_WINDOWS
// MSVC extension: <float.h> exposes _fpclass()/_fpclassf() and the _FPCLASS_*
// category bits.
#ifndef _FPCLASS_SNAN
#define _FPCLASS_SNAN 0x0001 // signaling NaN
#define _FPCLASS_QNAN 0x0002 // quiet NaN
#define _FPCLASS_NINF 0x0004 // negative infinity
#define _FPCLASS_NN 0x0008 // negative normal
#define _FPCLASS_ND 0x0010 // negative denormal
#define _FPCLASS_NZ 0x0020 // negative zero
#define _FPCLASS_PZ 0x0040 // positive zero
#define _FPCLASS_PD 0x0080 // positive denormal
#define _FPCLASS_PN 0x0100 // positive normal
#define _FPCLASS_PINF 0x0200 // positive infinity
#endif

static inline int _fpclass(double __x) {
	if (__builtin_isnan(__x)) {
		// SNaN vs QNaN is not distinguished; report quiet (the common case).
		return _FPCLASS_QNAN;
	}
	if (__builtin_isinf(__x)) {
		return __builtin_signbit(__x) ? _FPCLASS_NINF : _FPCLASS_PINF;
	}
	if (__x == 0.0) {
		return __builtin_signbit(__x) ? _FPCLASS_NZ : _FPCLASS_PZ;
	}
	if (__builtin_isnormal(__x)) {
		return __builtin_signbit(__x) ? _FPCLASS_NN : _FPCLASS_PN;
	}
	return __builtin_signbit(__x) ? _FPCLASS_ND : _FPCLASS_PD; // subnormal
}

static inline int _fpclassf(float __x) { return _fpclass((double) __x); }
#endif // SPRT_WINDOWS
#endif
