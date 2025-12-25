typedef unsigned int __sprt_fenv_t;

typedef unsigned int __sprt_fexcept_t;

/* Exception flags. */
#define __SPRT_FE_INVALID    0x01
#define __SPRT_FE_DIVBYZERO  0x02
#define __SPRT_FE_OVERFLOW   0x04
#define __SPRT_FE_UNDERFLOW  0x08
#define __SPRT_FE_INEXACT    0x10
#define __SPRT_FE_DENORMAL   0x80
#define __SPRT_FE_ALL_EXCEPT (__SPRT_FE_DIVBYZERO | __SPRT_FE_INEXACT | __SPRT_FE_INVALID \
	 | __SPRT_FE_OVERFLOW | __SPRT_FE_UNDERFLOW | __SPRT_FE_DENORMAL)

/* Rounding modes. */
#define __SPRT_FE_TONEAREST  0x0
#define __SPRT_FE_UPWARD     0x1
#define __SPRT_FE_DOWNWARD   0x2
#define __SPRT_FE_TOWARDZERO 0x3
