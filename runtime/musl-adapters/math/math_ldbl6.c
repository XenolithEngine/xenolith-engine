#include "../include/defs.h"

#define M_PI_4 __SPRT_M_PI_4

#if __SPRT_ARCH_ID == __SPRT_ARCH_ID_X86_64
#include "../musl-libc/src/math/x86_64/rintl.c"
#include "../musl-libc/src/math/x86_64/sqrtl.c"
#else
#include "../musl-libc/src/math/rintl.c"
#include "../musl-libc/src/math/sqrtl.c"
#endif

#include "../musl-libc/src/math/powl.c"
#include "../musl-libc/src/math/scalblnl.c"
#include "../musl-libc/src/math/scalbnl.c"
#include "../musl-libc/src/math/__signbitl.c"
#include "../musl-libc/src/math/sincosl.c"
#include "../musl-libc/src/math/sinhl.c"
#include "../musl-libc/src/math/__sinl.c"
#include "../musl-libc/src/math/sinl.c"
#include "../musl-libc/src/math/tanhl.c"
#include "../musl-libc/src/math/__tanl.c"
#include "../musl-libc/src/math/tanl.c"
