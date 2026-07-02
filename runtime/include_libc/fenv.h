/**
Copyright (c) 2025 Stappler Team <admin@stappler.org>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
**/

#ifndef CORE_RUNTIME_INCLUDE_LIBC_FENV_H_
#define CORE_RUNTIME_INCLUDE_LIBC_FENV_H_

/*
	Dispatch header for <fenv.h> (floating-point environment access):
	- hosted SPRT build -> forwards to the system <fenv.h> (#include_next)
	- otherwise         -> SPRT's own declarations (defined inline below)

	Public surface provided by the SPRT-own path (internal __sprt_* helpers excluded):

	Macros:
	  rounding directions: FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO
	  FE_ALL_EXCEPT - bitmask of all supported exceptions (the individual exception
	                  flags such as FE_INVALID/FE_DIVBYZERO/... come via __sprt_fenv.h)
	  FP_NAN, FP_INFINITE, FP_ZERO, FP_SUBNORMAL, FP_NORMAL - classification constants
	                  (also exposed via <math.h>), defined here only if not already

	Types:
	  fexcept_t - holds the state of the exception flags
	  fenv_t    - holds the entire floating-point environment

	Exception-flag functions:
	  feclearexcept    - clear the given exception flags
	  fetestexcept     - return which of the given exceptions are currently raised
	  feraiseexcept    - raise the given exceptions
	  fegetexceptflag  - save the current state of the given exception flags
	  fesetexceptflag  - restore previously saved exception-flag state

	Rounding-mode functions:
	  fegetround       - read the current rounding direction
	  fesetround       - set the rounding direction

	Environment functions:
	  fegetenv         - save the whole floating-point environment
	  fesetenv         - restore a saved environment
	  feholdexcept     - save the environment and clear exceptions (non-stop mode)
	  feupdateenv      - restore an environment, then re-raise pending exceptions
*/

#if defined(__SPRT_BUILD) && __STDC_HOSTED__ == 1

#include_next <fenv.h>

#else

#include <sprt/c/__sprt_fenv.h>

#ifndef FP_NAN
#define FP_NAN __SPRT_FP_NAN
#define FP_INFINITE __SPRT_FP_INFINITE
#define FP_ZERO __SPRT_FP_ZERO
#define FP_SUBNORMAL __SPRT_FP_SUBNORMAL
#define FP_NORMAL __SPRT_FP_NORMAL
#endif

#ifndef FE_TONEAREST
#define FE_TONEAREST __SPRT_FE_TONEAREST
#define FE_DOWNWARD __SPRT_FE_DOWNWARD
#define FE_UPWARD __SPRT_FE_UPWARD
#define FE_TOWARDZERO __SPRT_FE_TOWARDZERO
#define FE_ALL_EXCEPT __SPRT_FE_ALL_EXCEPT
#endif

// Individual floating-point exception flags. Each is defined only where the
// target actually supports it (soft-float targets may omit some).
#if defined(__SPRT_FE_DIVBYZERO) && !defined(FE_DIVBYZERO)
#define FE_DIVBYZERO __SPRT_FE_DIVBYZERO
#endif
#if defined(__SPRT_FE_INEXACT) && !defined(FE_INEXACT)
#define FE_INEXACT __SPRT_FE_INEXACT
#endif
#if defined(__SPRT_FE_INVALID) && !defined(FE_INVALID)
#define FE_INVALID __SPRT_FE_INVALID
#endif
#if defined(__SPRT_FE_OVERFLOW) && !defined(FE_OVERFLOW)
#define FE_OVERFLOW __SPRT_FE_OVERFLOW
#endif
#if defined(__SPRT_FE_UNDERFLOW) && !defined(FE_UNDERFLOW)
#define FE_UNDERFLOW __SPRT_FE_UNDERFLOW
#endif

#ifndef FE_DFL_ENV
#define FE_DFL_ENV __SPRT_FE_DFL_ENV
#endif


__SPRT_BEGIN_DECL

typedef __SPRT_ID(fexcept_t) fexcept_t;
typedef __SPRT_ID(fenv_t) fenv_t;

#define SPRT_FUNC_BEGIN SPRT_UMBRELLA_FUNC
#define SPRT_FUNC_END SPRT_UMBRELLA_END
#define SPRT_FUNC_BODY SPRT_UMBRELLA_REQUIRED

#include <sprt/wrappers/libc/fenv_impl.h>

#undef SPRT_FUNC_BEGIN
#undef SPRT_FUNC_END
#undef SPRT_FUNC_BODY

__SPRT_END_DECL

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_FENV_H_
