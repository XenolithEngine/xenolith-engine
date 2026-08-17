// Embox does NOT share the glibc FP_* classification numbering: its
// src/compat/libc/math/math_builtins.h (the math module the aarch64/qemu template
// enables) swaps NORMAL and ZERO relative to Linux. __sprt_fpclassify() forwards
// the libc's answer untranslated, and outside __SPRT_BUILD these macros ARE the
// application's FP_*, so the numbers have to be Embox's own.
//
// (The alternative math module, third-party openlibm, uses a third numbering -
// bit flags 0x01..0x10. Should the template ever switch to it, the asserts in
// SPRuntimeCMath.cpp are what will say so.)

// clang-format off
#define __SPRT_FP_NAN       0
#define __SPRT_FP_INFINITE  1
#define __SPRT_FP_NORMAL    2 // Linux: 4
#define __SPRT_FP_SUBNORMAL 3
#define __SPRT_FP_ZERO      4 // Linux: 2

// Embox declares neither, and its math is clang builtins without errno
// reporting: nothing is raised, so the honest answer is "no error reporting".
#define __SPRT_math_errhandling 0

#define __SPRT_FP_ILOGBNAN (-1-0x7fffffff)
#define __SPRT_FP_ILOGB0 __SPRT_FP_ILOGBNAN
// clang-format on
