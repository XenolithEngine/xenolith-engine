#define __SPRT_BUILD
#define _BSD_SOURCE

#include "../include/defs.h"

typedef int (*cmpfun_r)(const void *, const void *, void *);
void __qsort_r(void *base, size_t nel, size_t width, cmpfun_r cmp, void *arg);

#define cmpfun qsort_nr_cmpfun
#include "../../musl-libc/src/stdlib/qsort_nr.c"
#undef cmpfun

#include "../../musl-libc/src/stdlib/abs.c"
#include "../../musl-libc/src/stdlib/labs.c"
#include "../../musl-libc/src/stdlib/llabs.c"
#include "../../musl-libc/src/stdlib/div.c"
#include "../../musl-libc/src/stdlib/ldiv.c"
#include "../../musl-libc/src/stdlib/lldiv.c"

#ifndef __LONG_MAX
#define __LONG_MAX __SPRT_LONG_MAX
#endif

#pragma clang diagnostic ignored "-Wlogical-op-parentheses"
#pragma clang diagnostic ignored "-Wignored-attributes"

#define cmpfun qsort_cmpfun
#include "../../musl-libc/src/stdlib/qsort.c"
#undef cmpfun

#include "../../musl-libc/src/stdlib/atof.c"
#include "../../musl-libc/src/stdlib/atoi.c"
#include "../../musl-libc/src/stdlib/atol.c"
#include "../../musl-libc/src/stdlib/atoll.c"
