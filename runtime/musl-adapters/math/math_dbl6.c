#include "../include/defs.h"

#if __SPRT_ARCH_ID == __SPRT_ARCH_ID_X86_64
#include "../musl-libc/src/math/x86_64/sqrt.c"
#else
#include "../musl-libc/src/math/sqrt.c"
#endif

#include "../musl-libc/src/math/pow_data.c"
#include "../musl-libc/src/math/scalb.c"
#include "../musl-libc/src/math/scalbln.c"
#include "../musl-libc/src/math/scalbn.c"
#include "../musl-libc/src/math/__signbit.c"
#include "../musl-libc/src/math/signgam.c"
#include "../musl-libc/src/math/significand.c"
#include "../musl-libc/src/math/__sin.c"
#include "../musl-libc/src/math/sin.c"
#include "../musl-libc/src/math/sincos.c"
#include "../musl-libc/src/math/sinh.c"
#include "../musl-libc/src/math/sqrt_data.c"
#include "../musl-libc/src/math/__tan.c"
#include "../musl-libc/src/math/tan.c"
#include "../musl-libc/src/math/tanh.c"
#include "../musl-libc/src/math/trunc.c"
