#ifndef CORE_RUNTIME_INCLUDE_SPRT_WRAPPERS_LIBC_FENV_H_
#define CORE_RUNTIME_INCLUDE_SPRT_WRAPPERS_LIBC_FENV_H_

/* libc++ <cfenv> checks for this guard to confirm it found a libc++-compatible
 * <fenv.h>. Define it so the sprt wrapper is accepted. */
#define _LIBCPP_FENV_H

#include <sprt/c/__sprt_fenv.h>

#if __STDC_HOSTED__ == 0 || !defined(__SPRT_BUILD)
#ifndef FE_DIVBYZERO
#define FE_DIVBYZERO __SPRT_FE_DIVBYZERO
#endif
#ifndef FE_INEXACT
#define FE_INEXACT __SPRT_FE_INEXACT
#endif
#ifndef FE_INVALID
#define FE_INVALID __SPRT_FE_INVALID
#endif
#ifndef FE_OVERFLOW
#define FE_OVERFLOW __SPRT_FE_OVERFLOW
#endif
#ifndef FE_UNDERFLOW
#define FE_UNDERFLOW __SPRT_FE_UNDERFLOW
#endif
#ifndef FE_ALL_EXCEPT
#define FE_ALL_EXCEPT __SPRT_FE_ALL_EXCEPT
#endif
#ifndef FE_TONEAREST
#define FE_TONEAREST __SPRT_FE_TONEAREST
#endif
#ifndef FE_DOWNWARD
#define FE_DOWNWARD __SPRT_FE_DOWNWARD
#endif
#ifndef FE_UPWARD
#define FE_UPWARD __SPRT_FE_UPWARD
#endif
#ifndef FE_TOWARDZERO
#define FE_TOWARDZERO __SPRT_FE_TOWARDZERO
#endif
#endif

#endif
