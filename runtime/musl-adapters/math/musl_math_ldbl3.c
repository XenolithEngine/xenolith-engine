#include "../include/defs.h"

#if __SPRT_ARCH_ID == __SPRT_ARCH_ID_X86_64 && !SPRT_WINDOWS

#else
#include "../musl-libc/src/math/log1pl.c"
#endif

#include "../musl-libc/src/math/roundl.c"
