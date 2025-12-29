typedef struct {
	struct {
		unsigned int __control; /* Control word register */
		unsigned int __status; /* Status word register */
		unsigned int __tag; /* Tag word register */
		unsigned int __others[4]; /* EIP, Pointer Selector, etc */
	} __x87;
	unsigned int __mxcsr; /* Control, status register */
} __sprt_fenv_t;

typedef unsigned int __sprt_fexcept_t;

#define __SPRT_FE_INVALID    0x01
#define __SPRT_FE_DENORMAL   0x02
#define __SPRT_FE_DIVBYZERO  0x04
#define __SPRT_FE_OVERFLOW   0x08
#define __SPRT_FE_UNDERFLOW  0x10
#define __SPRT_FE_INEXACT    0x20

#define __SPRT_FE_ALL_EXCEPT   (__SPRT_FE_INVALID | __SPRT_FE_DENORMAL | __SPRT_FE_DIVBYZERO | \
                         __SPRT_FE_OVERFLOW | __SPRT_FE_UNDERFLOW | __SPRT_FE_INEXACT)

#define __SPRT_FE_TONEAREST  0x000
#define __SPRT_FE_DOWNWARD   0x400
#define __SPRT_FE_UPWARD     0x800
#define __SPRT_FE_TOWARDZERO 0xc00
