// LoongArch LP64D: the two libcs disagree on the size of __jmp_buf, so this is
// the one arch where the libc has to be known up front (make/os/linux.mk defines
// __SPRT_LINUX_MUSL for -linux-musl targets).
//  - musl: unsigned long __jmp_buf[23];
//  - glibc: struct { pc, sp, a reserved word (r21), fp, s0-s8; double fs0-fs7 }
//    = 21 words (sysdeps/loongarch/bits/setjmp.h, glibc 2.36).
// The resulting native_jmp_buf size is asserted in runtime_core_setjmp.cpp.
#if __SPRT_LINUX_MUSL
typedef unsigned long __SPRT_ID(__jmp_buf)[23];
#else
typedef unsigned long __SPRT_ID(__jmp_buf)[21];
#endif
