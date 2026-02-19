typedef struct {
	unsigned long long __fpsr;
	unsigned long long __fpcr;
} __SPRT_ID(fenv_t);

typedef unsigned short __SPRT_ID(fexcept_t);

// clang-format off
#define __SPRT_FE_INEXACT          0x0010
#define __SPRT_FE_UNDERFLOW        0x0008
#define __SPRT_FE_OVERFLOW         0x0004
#define __SPRT_FE_DIVBYZERO        0x0002
#define __SPRT_FE_INVALID          0x0001
#define __SPRT_FE_FLUSHTOZERO      0x0080 
#define __SPRT_FE_ALL_EXCEPT       0x009f
#define __SPRT_FE_TONEAREST        0x00000000
#define __SPRT_FE_UPWARD           0x00400000
#define __SPRT_FE_DOWNWARD         0x00800000
#define __SPRT_FE_TOWARDZERO       0x00C00000
// clang-format on
