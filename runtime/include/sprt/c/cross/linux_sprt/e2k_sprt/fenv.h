// e2k glibc bits/fenv.h: flags live in the FPCR (bits 0-4 layout below), the
// rounding mode is the PFPFR field; glibc wraps them in a three-word struct
// and exposes fexcept_t as unsigned int.
typedef struct {
	unsigned int __fpcr;
	unsigned int __fpsr;
	unsigned int __pfpfr;
} __sprt_fenv_t;

typedef unsigned int __sprt_fexcept_t;

// clang-format off
#define __SPRT_FE_INVALID    0x01
#define __SPRT_FE_DIVBYZERO  0x04
#define __SPRT_FE_OVERFLOW   0x08
#define __SPRT_FE_UNDERFLOW  0x10
#define __SPRT_FE_INEXACT    0x20
#define __SPRT_FE_ALL_EXCEPT 0x3D
#define __SPRT_FE_TONEAREST  0x0000
#define __SPRT_FE_DOWNWARD   0x2000
#define __SPRT_FE_UPWARD     0x4000
#define __SPRT_FE_TOWARDZERO 0x6000
// clang-format on

#define __SPRT_FE_DFL_ENV	((const __sprt_fenv_t *) -1)
