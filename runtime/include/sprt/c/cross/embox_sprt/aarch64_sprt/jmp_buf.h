// Embox aarch64 setjmp: SP, LR, x19-x28 — 12 uint64_t (asm/setjmp.h _JBLEN).
typedef unsigned long __SPRT_ID(__jmp_buf)[12];
