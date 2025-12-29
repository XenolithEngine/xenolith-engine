typedef struct {
	unsigned long _Fe_ctl, _Fe_stat;
} __sprt_fenv_t;

typedef unsigned long __sprt_fexcept_t;

#define __SPRT_FE_INEXACT    0x0001
#define __SPRT_FE_UNDERFLOW  0x0002
#define __SPRT_FE_OVERFLOW    0x0004
#define __SPRT_FE_DIVBYZERO  0x0008
#define __SPRT_FE_INVALID    0x0010
#define __SPRT_FE_DENORMAL   0x0

#define __SPRT_FE_ALL_EXCEPT (__SPRT_FE_DIVBYZERO | __SPRT_FE_INEXACT | __SPRT_FE_INVALID | __SPRT_FE_OVERFLOW | __SPRT_FE_UNDERFLOW)

#define __SPRT_FE_TONEAREST  0x0000
#define __SPRT_FE_UPWARD     0x0100
#define __SPRT_FE_DOWNWARD   0x0200
#define __SPRT_FE_TOWARDZERO 0x0300
#define __SPRT_FE_ROUND_MASK 0x0300
