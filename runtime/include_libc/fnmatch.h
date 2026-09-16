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

#ifndef CORE_RUNTIME_INCLUDE_LIBC_FNMATCH_H_
#define CORE_RUNTIME_INCLUDE_LIBC_FNMATCH_H_

/*
	Dispatch header for the POSIX <fnmatch.h> (filename pattern matching):
	- hosted SPRT build -> forwards to the system <fnmatch.h> (#include_next)
	- otherwise         -> SPRT's own declaration, routed to __sprt_fnmatch on
	                       hosted consumers, or to the freestanding musl fnmatch
	                       (plain symbol) on wasm/Windows.

	Flags: FNM_PATHNAME, FNM_NOESCAPE, FNM_PERIOD, FNM_LEADING_DIR, FNM_CASEFOLD,
	       FNM_FILE_NAME (alias of FNM_PATHNAME). Returns 0 on match, FNM_NOMATCH
	       otherwise.
*/

#if defined(__SPRT_BUILD) && __STDC_HOSTED__ == 1

#include_next <fnmatch.h>

#else

#include <sprt/c/__sprt_fnmatch.h>

#define FNM_PATHNAME    __SPRT_FNM_PATHNAME
#define FNM_NOESCAPE    __SPRT_FNM_NOESCAPE
#define FNM_PERIOD      __SPRT_FNM_PERIOD
#define FNM_LEADING_DIR __SPRT_FNM_LEADING_DIR
#define FNM_CASEFOLD    __SPRT_FNM_CASEFOLD
#define FNM_FILE_NAME   FNM_PATHNAME
#define FNM_NOMATCH     __SPRT_FNM_NOMATCH
#define FNM_NOSYS       __SPRT_FNM_NOSYS

__SPRT_BEGIN_DECL

SPRT_UMBRELLA_FUNC
int fnmatch(const char *__pattern, const char *__string, int __flags) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_fnmatch(__pattern, __string, __flags);
}
#endif

__SPRT_END_DECL

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_FNMATCH_H_
