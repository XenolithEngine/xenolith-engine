#include "include/defs.h"

#if __SPRT_ARCH_ID == __SPRT_ARCH_ID_X86_64

#else
#include "../musl-libc/src/fenv/fenv.c"
#endif

#include "../musl-libc/src/fenv/fegetexceptflag.c"
#include "../musl-libc/src/fenv/feholdexcept.c"
#include "../musl-libc/src/fenv/fesetexceptflag.c"
#include "../musl-libc/src/fenv/fesetround.c"
#include "../musl-libc/src/fenv/feupdateenv.c"
#include "../musl-libc/src/fenv/__flt_rounds.c"
