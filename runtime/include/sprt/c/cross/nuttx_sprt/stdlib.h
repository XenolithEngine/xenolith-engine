// NuttX libc stdlib.h workarounds.
//
// NuttX <stdlib.h> defines several names as macros that the sprt umbrella
// re-declares as functions, causing redefinition errors after macro expansion:
//   - srandom(s)        -> srand(s)              (sprt declares both as functions)
//   - strtold_l(s,e,l)  -> strtold(s,e)          (sprt declares strtold_l)
//   - strtoll_l(s,e,b,l)-> strtoll(s,e,b)        (sprt declares strtoll_l)
//   - strtoull_l(...)   -> strtoull(...)         (sprt declares strtoull_l)
//   - strtof_l(s,e,l)   -> strtof(s,e)           (sprt declares strtof_l)
//   - strtod_l(s,e,l)   -> strtod(s,e)           (sprt declares strtod_l)
//
// Undefine them before sprt's stdlib_impl.h is parsed so the sprt prototypes
// survive intact. The NuttX libc defines the underlying plain functions
// (srand/strtoll/...); sprt's umbrella forwards to the __sprt_-prefixed spellings.
#undef srandom
#undef strtold_l
#undef strtoll_l
#undef strtoull_l
#undef strtof_l
#undef strtod_l
#undef strtol_l
#undef strtoul_l
