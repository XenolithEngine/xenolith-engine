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
#define __SPRT_FE_DOWNWARD   0x80'0000
#define __SPRT_FE_UPWARD     0x40'0000
#define __SPRT_FE_TOWARDZERO 0xc0'0000

#define __SPRT_FE_DFL_ENV	((const __sprt_fenv_t *) -1)
