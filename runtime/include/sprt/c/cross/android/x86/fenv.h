typedef struct {
	unsigned short __control;
	unsigned short __mxcsr_hi;
	unsigned short __status;
	unsigned short __mxcsr_lo;
	unsigned int __tag;
	char __other[16];
} __sprt_fenv_t;

typedef unsigned short __sprt_fexcept_t;

/* Exception flags */
#define __SPRT_FE_INVALID    0x01
#define __SPRT_FE_DENORMAL   0x02
#define __SPRT_FE_DIVBYZERO  0x04
#define __SPRT_FE_OVERFLOW   0x08
#define __SPRT_FE_UNDERFLOW  0x10
#define __SPRT_FE_INEXACT    0x20
#define __SPRT_FE_ALL_EXCEPT (__SPRT_FE_DIVBYZERO | __SPRT_FE_DENORMAL | __SPRT_FE_INEXACT | \
                       __SPRT_FE_INVALID | __SPRT_FE_OVERFLOW | __SPRT_FE_UNDERFLOW)

/* Rounding modes */
#define __SPRT_FE_TONEAREST  0x0000
#define __SPRT_FE_DOWNWARD   0x0400
#define __SPRT_FE_UPWARD     0x0800
#define __SPRT_FE_TOWARDZERO 0x0c00
