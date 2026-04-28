#include "../include/defs.h"

#if __SPRT_ARCH_ID == __SPRT_ARCH_ID_X86_64

#else
#include "../musl-libc/src/math/truncl.c"
#endif

#include "../musl-libc/src/math/tgammal.c"
