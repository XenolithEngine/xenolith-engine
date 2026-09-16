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

#ifndef CORE_RUNTIME_INCLUDE_LIBC_POLL_H_
#define CORE_RUNTIME_INCLUDE_LIBC_POLL_H_

/*
	POSIX <poll.h> - synchronous I/O multiplexing.
	- hosted SPRT build -> forwards to the system <poll.h> (#include_next)
	- otherwise         -> struct pollfd + poll() below.

	On freestanding wasm there are no pollable descriptors, so the sprt libc
	implements poll() as a no-op stub (reports the timeout with no ready fds);
	the declaration and struct layout still match Linux/musl so callers compile
	and behave predictably.
*/

#if defined(__SPRT_BUILD) && __STDC_HOSTED__ == 1

#include_next <poll.h>

#else

#include <sprt/c/bits/__sprt_def.h>
#include <sprt/c/sys/__sprt_poll.h>

typedef __SPRT_ID(nfds_t) nfds_t;

// clang-format off
#define POLLIN     __SPRT_POLLIN
#define POLLPRI    __SPRT_POLLPRI
#define POLLOUT    __SPRT_POLLOUT
#define POLLERR    __SPRT_POLLERR
#define POLLHUP    __SPRT_POLLHUP
#define POLLNVAL   __SPRT_POLLNVAL
#define POLLRDNORM __SPRT_POLLRDNORM
#define POLLRDBAND __SPRT_POLLRDBAND
#define POLLWRNORM __SPRT_POLLWRNORM
#define POLLWRBAND __SPRT_POLLWRBAND
#ifdef __SPRT_POLLMSG
#define POLLMSG    __SPRT_POLLMSG
#endif
#ifdef __SPRT_POLLRDHUP
#define POLLRDHUP  __SPRT_POLLRDHUP
#endif
// clang-format on

__SPRT_BEGIN_DECL

SPRT_UMBRELLA_FUNC
int poll(struct __SPRT_POLLFD_NAME *__fd, __SPRT_ID(nfds_t) __nfd, int __f) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __SPRT_ID(poll)(__fd, __nfd, __f);
}
#endif

SPRT_UMBRELLA_FUNC
int ppoll(struct __SPRT_POLLFD_NAME *__fd, __SPRT_ID(nfds_t) __nfd,
		const struct __SPRT_TIMESPEC_NAME *__ts,
		const __SPRT_ID(sigset_t) * __sig) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __SPRT_ID(ppoll)(__fd, __nfd, __ts, __sig);
}
#endif

__SPRT_END_DECL

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_POLL_H_
