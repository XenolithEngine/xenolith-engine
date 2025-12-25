typedef struct {
	unsigned short __control_word;
	unsigned short __unused1;
	unsigned short __status_word;
	unsigned short __unused2;
	unsigned short __tags;
	unsigned short __unused3;
	unsigned int __eip;
	unsigned short __cs_selector;
	unsigned int __opcode  : 11;
	unsigned int __unused4 : 5;
	unsigned int __data_offset;
	unsigned short __data_selector;
	unsigned short __unused5;
	unsigned int __mxcsr;
} __sprt_fenv_t;

typedef unsigned short __sprt_fexcept_t;

#define __SPRT_FE_INVALID    1
#define __SPRT_FE_DENORM     2
#define __SPRT_FE_DIVBYZERO  4
#define __SPRT_FE_OVERFLOW   8
#define __SPRT_FE_UNDERFLOW  16
#define __SPRT_FE_INEXACT    32
#define __SPRT_FE_ALL_EXCEPT 63
#define __SPRT_FE_TONEAREST  0
#define __SPRT_FE_DOWNWARD   0x400
#define __SPRT_FE_UPWARD     0x800
#define __SPRT_FE_TOWARDZERO 0xc00

#define __SPRT_FE_DFL_ENV	((const __sprt_fenv_t *) -1)
