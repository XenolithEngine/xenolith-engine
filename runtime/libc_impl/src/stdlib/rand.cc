#include "stdlib.h"

// The C names of the generator in libc_wrapper/c/common/rand.cc, for C code compiled freestanding,
// which calls them by these names.
__SPRT_C_FUNC int rand(void) __SPRT_NOEXCEPT { return __sprt_rand(); }

__SPRT_C_FUNC void srand(unsigned seed) __SPRT_NOEXCEPT { __sprt_srand(seed); }
