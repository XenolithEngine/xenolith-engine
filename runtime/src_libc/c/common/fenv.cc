/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

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

#define __SPRT_BUILD 1

#include <sprt/c/__sprt_fenv.h>
#include <sprt/cxx/type_traits>

#include <fenv.h>

static_assert(FE_INEXACT == __SPRT_FE_INEXACT);
static_assert(FE_UNDERFLOW == __SPRT_FE_UNDERFLOW);
static_assert(FE_OVERFLOW == __SPRT_FE_OVERFLOW);
static_assert(FE_DIVBYZERO == __SPRT_FE_DIVBYZERO);
static_assert(FE_INVALID == __SPRT_FE_INVALID);

static_assert(FE_ALL_EXCEPT == __SPRT_FE_ALL_EXCEPT);
static_assert(FE_TONEAREST == __SPRT_FE_TONEAREST);
static_assert(FE_UPWARD == __SPRT_FE_UPWARD);
static_assert(FE_DOWNWARD == __SPRT_FE_DOWNWARD);
static_assert(FE_TOWARDZERO == __SPRT_FE_TOWARDZERO);

#ifdef FE_DENORMALOPERAND
static_assert(FE_DENORMALOPERAND == __SPRT_FE_DENORMALOPERAND);
#endif

#ifdef FE_FLUSHTOZERO
static_assert(FE_FLUSHTOZERO == __SPRT_FE_FLUSHTOZERO);
#endif

static_assert(sizeof(fenv_t) == sizeof(__sprt_fenv_t));
static_assert(sprt::is_same_v<fexcept_t, __sprt_fexcept_t>);

namespace sprt {

__SPRT_C_FUNC __sprt_fenv_t *__sprt_arch_FE_DFL_ENV_fn() { return (__sprt_fenv_t *)FE_DFL_ENV; }

__SPRT_C_FUNC int __SPRT_ID(feclearexcept)(int v) { return ::feclearexcept(v); }
__SPRT_C_FUNC int __SPRT_ID(fegetexceptflag)(__SPRT_ID(fexcept_t) * ex, int v) {
	return ::fegetexceptflag(ex, v);
}
__SPRT_C_FUNC int __SPRT_ID(feraiseexcept)(int v) { return ::feraiseexcept(v); }
__SPRT_C_FUNC int __SPRT_ID(fesetexceptflag)(const __SPRT_ID(fexcept_t) * ex, int v) {
	return ::fesetexceptflag(ex, v);
}
__SPRT_C_FUNC int __SPRT_ID(fetestexcept)(int v) { return ::fetestexcept(v); }

__SPRT_C_FUNC int __SPRT_ID(fegetround)(void) { return ::fegetround(); }
__SPRT_C_FUNC int __SPRT_ID(fesetround)(int v) { return ::fesetround(v); }

__SPRT_C_FUNC int __SPRT_ID(fegetenv)(__SPRT_ID(fenv_t) * ex) { return ::fegetenv((fenv_t *)ex); }
__SPRT_C_FUNC int __SPRT_ID(feholdexcept)(__SPRT_ID(fenv_t) * ex) {
	return ::feholdexcept((fenv_t *)ex);
}
__SPRT_C_FUNC int __SPRT_ID(fesetenv)(const __SPRT_ID(fenv_t) * ex) {
	return ::fesetenv((const fenv_t *)ex);
}
__SPRT_C_FUNC int __SPRT_ID(feupdateenv)(const __SPRT_ID(fenv_t) * ex) {
	return ::feupdateenv((const fenv_t *)ex);
}

} // namespace sprt
