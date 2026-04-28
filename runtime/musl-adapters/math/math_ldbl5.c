#include "../include/defs.h"

#if __SPRT_ARCH_ID == __SPRT_ARCH_ID_X86_64
#include "../musl-libc/src/math/x86_64/remainderl.c"
#include "../musl-libc/src/math/x86_64/remquol.c"
#else
#include "../musl-libc/src/math/logl.c"
#include "../musl-libc/src/math/remainderl.c"
#include "../musl-libc/src/math/remquol.c"
#endif

#include "../musl-libc/src/math/__math_invalidl.c"
#include "../musl-libc/src/math/modfl.c"
#include "../musl-libc/src/math/nanl.c"
#include "../musl-libc/src/math/nearbyintl.c"
#include "../musl-libc/src/math/nextafterl.c"
#include "../musl-libc/src/math/nexttowardl.c"
#include "../musl-libc/src/math/__polevll.c"
