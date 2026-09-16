// Modeled on musl-libc arch/aarch64/bits/fenv.h
// AArch64 floating-point environment: FPCR/FPSR (no x87/SSE state)

typedef struct {
	unsigned int __fpcr;
	unsigned int __fpsr;
} __sprt_fenv_t;

typedef unsigned int __sprt_fexcept_t;

#define __SPRT_FE_INVALID    1
#define __SPRT_FE_DIVBYZERO  2
#define __SPRT_FE_OVERFLOW   4
#define __SPRT_FE_UNDERFLOW  8
#define __SPRT_FE_INEXACT    16

#define __SPRT_FE_ALL_EXCEPT 31

#define __SPRT_FE_TONEAREST  0
#define __SPRT_FE_UPWARD     0x400000
#define __SPRT_FE_DOWNWARD   0x800000
#define __SPRT_FE_TOWARDZERO 0xc00000

#define __SPRT_FE_DFL_ENV    ((const __sprt_fenv_t *) -1)
