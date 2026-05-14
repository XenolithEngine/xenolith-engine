#define __SPRT_BUILD
#define _BSD_SOURCE

#include "../include/defs.h"

#ifndef __LONG_MAX
#define __LONG_MAX __SPRT_LONG_MAX
#endif

#pragma clang diagnostic ignored "-Wlogical-op-parentheses"
#pragma clang diagnostic ignored "-Wignored-attributes"

#include "../../musl-libc/src/stdlib/qsort.c"

#include "../../musl-libc/src/stdlib/atof.c"
#include "../../musl-libc/src/stdlib/atoi.c"
#include "../../musl-libc/src/stdlib/atol.c"
#include "../../musl-libc/src/stdlib/atoll.c"
