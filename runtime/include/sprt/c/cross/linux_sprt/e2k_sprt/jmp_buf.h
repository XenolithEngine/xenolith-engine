typedef unsigned long long __SPRT_ID(__jmp_buf)[10];

#define __SPRT_NATIVE_JMP_BUF_DEFINED 1
typedef __SPRT_ID(__jmp_buf) __SPRT_ID(native_jmp_buf)[1];
typedef __SPRT_ID(native_jmp_buf) __SPRT_ID(native_sigjmp_buf);
