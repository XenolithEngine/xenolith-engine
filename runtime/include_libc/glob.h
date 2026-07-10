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

#ifndef CORE_RUNTIME_INCLUDE_LIBC_GLOB_H_
#define CORE_RUNTIME_INCLUDE_LIBC_GLOB_H_

/*
	Dispatch header for the POSIX <glob.h> (pathname pattern expansion):
	- hosted SPRT build -> forwards to the system <glob.h> (#include_next)
	- otherwise         -> SPRT's own glob_t + declarations, routed to __sprt_*
	                       on hosted consumers, or to the freestanding musl glob
	                       (plain symbols) on wasm/Windows.

	The hosted wrapper forwards to the native glob(); Android (Bionic ships no
	glob()) borrows musl's, by analogy with the complex-math fallback.

	Type:  glob_t { size_t gl_pathc; char **gl_pathv; size_t gl_offs; ... }.
	Flags: GLOB_ERR, GLOB_MARK, GLOB_NOSORT, GLOB_DOOFFS, GLOB_NOCHECK,
	       GLOB_APPEND, GLOB_NOESCAPE, GLOB_PERIOD, GLOB_TILDE, GLOB_TILDE_CHECK.
	Errors: GLOB_NOSPACE, GLOB_ABORTED, GLOB_NOMATCH.
*/

#if defined(__SPRT_BUILD) && __STDC_HOSTED__ == 1

#include_next <glob.h>

#else

#include <sprt/c/__sprt_glob.h>

typedef __SPRT_ID(glob_t) glob_t;

#define GLOB_ERR         __SPRT_GLOB_ERR
#define GLOB_MARK        __SPRT_GLOB_MARK
#define GLOB_NOSORT      __SPRT_GLOB_NOSORT
#define GLOB_DOOFFS      __SPRT_GLOB_DOOFFS
#define GLOB_NOCHECK     __SPRT_GLOB_NOCHECK
#define GLOB_APPEND      __SPRT_GLOB_APPEND
#define GLOB_NOESCAPE    __SPRT_GLOB_NOESCAPE
#define GLOB_PERIOD      __SPRT_GLOB_PERIOD
#define GLOB_TILDE       __SPRT_GLOB_TILDE
#define GLOB_TILDE_CHECK __SPRT_GLOB_TILDE_CHECK

#define GLOB_NOSPACE __SPRT_GLOB_NOSPACE
#define GLOB_ABORTED __SPRT_GLOB_ABORTED
#define GLOB_NOMATCH __SPRT_GLOB_NOMATCH

__SPRT_BEGIN_DECL

SPRT_UMBRELLA_FUNC
int glob(const char *__pattern, int __flags, int (*__errfunc)(const char *__epath, int __eerrno),
		glob_t *__pglob) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_glob(__pattern, __flags, __errfunc, __pglob);
}
#endif

SPRT_UMBRELLA_FUNC
void globfree(glob_t *__pglob) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	__sprt_globfree(__pglob);
}
#endif

__SPRT_END_DECL

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_GLOB_H_
