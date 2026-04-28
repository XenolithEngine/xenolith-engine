#include "../include/defs.h"

#if __SPRT_ARCH_ID == __SPRT_ARCH_ID_X86_64

#else
#include "../musl-libc/src/math/expm1l.c"
#include "../musl-libc/src/math/log10l.c"
#include "../musl-libc/src/math/floorl.c"
#endif

#include "../musl-libc/src/math/hypotl.c"
