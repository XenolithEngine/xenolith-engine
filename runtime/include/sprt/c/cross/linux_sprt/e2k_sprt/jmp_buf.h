// e2k glibc bits/setjmp.h: __jmp_buf is ten 64-bit slots; the type does not
// depend on the pointer mode (-m32/-m64/-m128), long long is 8 bytes in all.
typedef unsigned long long __SPRT_ID(__jmp_buf)[10];
