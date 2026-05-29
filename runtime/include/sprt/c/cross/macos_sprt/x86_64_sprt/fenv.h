typedef struct {
	unsigned short __control; /* x87 control word               */
	unsigned short __status; /* x87 status word                */
	unsigned int __mxcsr; /* SSE status/control register    */
	char __reserved[8]; /* Reserved for future expansion  */
} __SPRT_ID(fenv_t);

typedef unsigned short __SPRT_ID(fexcept_t);

#define __SPRT_FE_INEXACT          0x0020
#define __SPRT_FE_UNDERFLOW        0x0010
#define __SPRT_FE_OVERFLOW         0x0008
#define __SPRT_FE_DIVBYZERO        0x0004
#define __SPRT_FE_INVALID          0x0001
#define __SPRT_FE_DENORMALOPERAND  0x0002
#define __SPRT_FE_ALL_EXCEPT       0x003f
#define __SPRT_FE_TONEAREST        0x0000
#define __SPRT_FE_DOWNWARD         0x0400
#define __SPRT_FE_UPWARD           0x0800
#define __SPRT_FE_TOWARDZERO       0x0c00
