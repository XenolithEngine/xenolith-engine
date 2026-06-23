// RISC-V (hard-float, F/D extensions) floating-point environment. Both glibc and
// musl expose fenv_t/fexcept_t as a plain unsigned int (the fcsr image), so the
// SPRT ABI mirrors that scalar shape — fenv.cc asserts sizeof()/is_same against
// the native types. Exception/rounding bits are the fcsr layout from the RISC-V
// ISA (identical in glibc bits/fenv.h and musl bits/fenv.h).
typedef unsigned int __sprt_fenv_t;
typedef unsigned int __sprt_fexcept_t;

// clang-format off
#define __SPRT_FE_INVALID    16
#define __SPRT_FE_DIVBYZERO  8
#define __SPRT_FE_OVERFLOW   4
#define __SPRT_FE_UNDERFLOW  2
#define __SPRT_FE_INEXACT    1
#define __SPRT_FE_ALL_EXCEPT 31
#define __SPRT_FE_TONEAREST  0
#define __SPRT_FE_TOWARDZERO 1
#define __SPRT_FE_DOWNWARD   2
#define __SPRT_FE_UPWARD     3
// clang-format on

#define __SPRT_FE_DFL_ENV	((const __sprt_fenv_t *) -1)
