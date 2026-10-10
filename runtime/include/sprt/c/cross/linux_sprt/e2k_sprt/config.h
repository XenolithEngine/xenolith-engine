// e2k glibc ships its own kernel mmap ABI in bits/mman.h (divergent from
// asm-generic): MAP_FIXED is 0x100, the anonymous-mapping flag 0x10 and
// MAP_NORESERVE 0x10000. The libc wrapper's static asserts pin the sprt
// constants to these.
#define __SPRT_MAP_FIXED       0x100
#define __SPRT_MAP_ANON        0x10
#define __SPRT_MAP_ANONYMOUS   __SPRT_MAP_ANON
#define __SPRT_MAP_NORESERVE   0x10000
