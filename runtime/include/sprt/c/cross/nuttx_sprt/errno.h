// NuttX errno values.
//
// NuttX libc carries the asm-generic errno table (identical numbers to Linux
// for almost everything), but with one important split: ENOTSUP and EOPNOTSUPP
// are distinct (138 and 95), while Linux/glibc collapses them to the same 95.
// sprt's canonical errno numbers follow Linux, so override __SPRT_ENOTSUP here
// to match the NuttX value, then pull in the rest of the linux_sprt table for
// the values that do match.
#ifndef __SPRT_EOPNOTSUPP
#define __SPRT_EOPNOTSUPP 95
#endif

#ifndef __SPRT_ENOTSUP
#define __SPRT_ENOTSUP 138
#endif

// The rest of the asm-generic errno values match Linux/glibc verbatim.
#include <sprt/c/cross/linux_sprt/errno.h>

// Re-assert NuttX's split value after the linux include (linux_sprt re-defines
// __SPRT_ENOTSUP to __SPRT_EOPNOTSUPP for the glibc collapse).
#undef __SPRT_ENOTSUP
#define __SPRT_ENOTSUP 138
