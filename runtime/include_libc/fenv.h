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


__SPRT_BEGIN_DECL

typedef __SPRT_ID(fexcept_t) fexcept_t;
typedef __SPRT_ID(fenv_t) fenv_t;

SPRT_UMBRELLA_FUNC
int feclearexcept(int v) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_feclearexcept(v);
}
#endif

SPRT_UMBRELLA_FUNC
int fegetexceptflag(fexcept_t *ex, int v) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_fegetexceptflag(ex, v);
}
#endif
SPRT_UMBRELLA_FUNC
int feraiseexcept(int v) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_feraiseexcept(v);
}
#endif

SPRT_UMBRELLA_FUNC
int fesetexceptflag(const fexcept_t *ex, int v) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_fesetexceptflag(ex, v);
}
#endif
SPRT_UMBRELLA_FUNC
int fetestexcept(int v) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_fetestexcept(v);
}
#endif


SPRT_UMBRELLA_FUNC
int fegetround(void) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_fegetround();
}
#endif

SPRT_UMBRELLA_FUNC
int fesetround(int v) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_fesetround(v);
}
#endif


SPRT_UMBRELLA_FUNC
int fegetenv(fenv_t *ex) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_fegetenv((fenv_t *)ex);
}
#endif

SPRT_UMBRELLA_FUNC
int feholdexcept(fenv_t *ex) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_feholdexcept((fenv_t *)ex);
}
#endif

SPRT_UMBRELLA_FUNC
int fesetenv(const fenv_t *ex) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_fesetenv((const fenv_t *)ex);
}
#endif

SPRT_UMBRELLA_FUNC
int feupdateenv(const fenv_t *ex) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_feupdateenv((const fenv_t *)ex);
}
#endif

__SPRT_END_DECL

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_FENV_H_
