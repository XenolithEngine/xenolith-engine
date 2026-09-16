/**
Copyright (c) 2026 Xenolith Team <admin@stappler.org>

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

#ifndef CORE_RUNTIME_INCLUDE_LIBC_SYS_SELECT_H_
#define CORE_RUNTIME_INCLUDE_LIBC_SYS_SELECT_H_

#if defined(__SPRT_BUILD) && __STDC_HOSTED__ == 1

#include_next <sys/select.h>

#else

#include <sprt/c/sys/__sprt_select.h>

#if SPRT_WINDOWS
#include <sprt/wrappers/windows/basic_types.h> // __SPRT_WIN_IMPORT WINAPI
#endif

typedef __SPRT_ID(fd_set) fd_set;

#ifndef FD_SETSIZE
#define FD_SETSIZE __SPRT_FD_SETSIZE
#endif

#define FD_CLR(fd, set) __SPRT_FD_CLR(fd, set)
#define FD_SET(fd, set) __SPRT_FD_SET(fd, set)
#define FD_ZERO(set) __SPRT_FD_ZERO(set)
#define FD_ISSET(fd, set) __SPRT_FD_ISSET(fd, set)

__SPRT_BEGIN_DECL

#if SPRT_WINDOWS
// winsock forward declaration

__SPRT_WIN_IMPORT WINAPI int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
		const struct __SPRT_TIMEVAL_NAME *timeout);

#else
// sprt own umbrella

SPRT_UMBRELLA_FUNC int select(int __nfds, __SPRT_ID(fd_set) * __SPRT_RESTRICT __rfds,
		__SPRT_ID(fd_set) * __SPRT_RESTRICT __wfds, __SPRT_ID(fd_set) * __SPRT_RESTRICT __efds,
		const struct __SPRT_TIMEVAL_NAME *__SPRT_RESTRICT __tv) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __SPRT_ID(select)(__nfds, __rfds, __wfds, __efds, __tv);
}
#endif

#endif // SPRT_WINDOWS


SPRT_UMBRELLA_FUNC int pselect(int __nfds, __SPRT_ID(fd_set) * __SPRT_RESTRICT __rfds,
		__SPRT_ID(fd_set) * __SPRT_RESTRICT __wfds, __SPRT_ID(fd_set) * __SPRT_RESTRICT __efds,
		const struct __SPRT_TIMESPEC_NAME *__SPRT_RESTRICT __tv,
		const __SPRT_ID(sigset_t) * __SPRT_RESTRICT __sig) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __SPRT_ID(pselect)(__nfds, __rfds, __wfds, __efds, __tv, __sig);
}
#endif

__SPRT_END_DECL

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_SYS_SELECT_H_
