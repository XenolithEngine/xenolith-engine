// Embox's <math.h> is musl's: the kernel's libm is (xenolith-os
// board/common/musl, BF-54), and so is the FP_* numbering -- Linux's.
// __sprt_fpclassify() forwards the libc's answer untranslated, and outside
// __SPRT_BUILD these macros ARE the application's FP_*; the asserts in
// SPRuntimeCMath.cpp keep the two in step.

// clang-format off
#define __SPRT_FP_NAN       0
#define __SPRT_FP_INFINITE  1
#define __SPRT_FP_ZERO      2
#define __SPRT_FP_SUBNORMAL 3
#define __SPRT_FP_NORMAL    4

// musl's libm reports through the floating-point exceptions, not errno.
#define __SPRT_math_errhandling 2 // MATH_ERREXCEPT

#define __SPRT_FP_ILOGBNAN (-1-0x7fffffff)
#define __SPRT_FP_ILOGB0 __SPRT_FP_ILOGBNAN
// clang-format on
