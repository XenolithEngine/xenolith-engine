// LoongArch (hard-float, LP64D) floating-point environment. glibc and musl both
// wrap the FCSR image in a one-field struct (glibc: __fp_control_register, musl:
// __cw) and expose fexcept_t as unsigned int; fenv.cc asserts sizeof()/is_same
// against the native types. Exception flags are the FCSR Cause/Flags bits 16-20
// and the rounding mode is the RM field at bits 8-9 (identical in glibc and musl
// bits/fenv.h).
typedef struct {
	unsigned int __fp_control_register;
} __sprt_fenv_t;

typedef unsigned int __sprt_fexcept_t;

// clang-format off
#define __SPRT_FE_INEXACT    0x010000
#define __SPRT_FE_UNDERFLOW  0x020000
#define __SPRT_FE_OVERFLOW   0x040000
#define __SPRT_FE_DIVBYZERO  0x080000
#define __SPRT_FE_INVALID    0x100000
#define __SPRT_FE_ALL_EXCEPT 0x1F0000
#define __SPRT_FE_TONEAREST  0x000
#define __SPRT_FE_TOWARDZERO 0x100
#define __SPRT_FE_UPWARD     0x200
#define __SPRT_FE_DOWNWARD   0x300
// clang-format on

#define __SPRT_FE_DFL_ENV	((const __sprt_fenv_t *) -1)
