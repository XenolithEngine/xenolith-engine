// RISC-V LP64D: __jmp_buf holds pc + 12 callee-saved integer regs (s0-s11) + sp
// + 12 callee-saved FP regs (fs0-fs11) = 26 unsigned longs (208 bytes). Matches
// both musl (__jmp_buf[26]) and glibc (struct of the same fields); the resulting
// native_jmp_buf size is asserted in runtime_core_setjmp.cpp.
typedef unsigned long __SPRT_ID(__jmp_buf)[26];
