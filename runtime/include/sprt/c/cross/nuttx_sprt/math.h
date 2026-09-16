// NuttX orders the fpclassify() results differently from glibc/musl. The wrapper
// forwards __sprt___fpclassify() to the libc's own fpclassify(), so its return
// value is a NuttX code; outside __SPRT_BUILD these macros ARE the FP_* the caller
// compares it against, and the Linux ordering would make every comparison wrong
// (a NaN would read as FP_INFINITE, a normal number as FP_ZERO).

// clang-format off
#define __SPRT_FP_INFINITE  0 // Linux: 1
#define __SPRT_FP_NAN       1 // Linux: 0
#define __SPRT_FP_NORMAL    2 // Linux: 4
#define __SPRT_FP_SUBNORMAL 3
#define __SPRT_FP_ZERO      4 // Linux: 2

// NuttX <math.h> declares no FP_ILOGB0/FP_ILOGBNAN and no MATH_ERRNO family, so
// nothing pins these; keep the values the compiler's own __builtin_ilogb reports
// and the standard MATH_* bits, which include_libc/math.h re-exports.
#define __SPRT_FP_ILOGBNAN (-1-0x7fffffff)
#define __SPRT_FP_ILOGB0 __SPRT_FP_ILOGBNAN

#ifdef __FAST_MATH__
#define __SPRT_math_errhandling	0
#elif defined __NO_MATH_ERRNO__
#define __SPRT_math_errhandling	(__SPRT_MATH_ERREXCEPT)
#else
#define __SPRT_math_errhandling	(__SPRT_MATH_ERRNO | __SPRT_MATH_ERREXCEPT)
#endif
// clang-format on
