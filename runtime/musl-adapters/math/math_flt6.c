#include "../include/defs.h"

#define M_PI_2 __SPRT_M_PI_2

#if __SPRT_ARCH_ID == __SPRT_ARCH_ID_X86_64
#include "../musl-libc/src/math/x86_64/sqrtf.c"
#else
#include "../musl-libc/src/math/sqrtf.c"
#endif

#include "../musl-libc/src/math/powf_data.c"
#include "../musl-libc/src/math/rintf.c"
#include "../musl-libc/src/math/scalbf.c"
#include "../musl-libc/src/math/scalblnf.c"
#include "../musl-libc/src/math/scalbnf.c"
#include "../musl-libc/src/math/__signbitf.c"
#include "../musl-libc/src/math/significandf.c"
#include "../musl-libc/src/math/sincosf.c"
#include "../musl-libc/src/math/__sindf.c"
#include "../musl-libc/src/math/sinhf.c"
#include "../musl-libc/src/math/__tandf.c"
#include "../musl-libc/src/math/tanf.c"
#include "../musl-libc/src/math/tanhf.c"
#include "../musl-libc/src/math/tgammaf.c"
#include "../musl-libc/src/math/truncf.c"
