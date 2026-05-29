#define __SPRT_BUILD
#define _BSD_SOURCE

#include "../include/defs.h"

typedef int (*cmpfun_r)(const void *, const void *, void *);
void __qsort_r(void *base, size_t nel, size_t width, cmpfun_r cmp, void *arg);

#include "../../musl-libc/src/stdlib/qsort_nr.c"
#include "../../musl-libc/src/stdlib/abs.c"
#include "../../musl-libc/src/stdlib/labs.c"
#include "../../musl-libc/src/stdlib/llabs.c"
#include "../../musl-libc/src/stdlib/div.c"
#include "../../musl-libc/src/stdlib/ldiv.c"
#include "../../musl-libc/src/stdlib/lldiv.c"
