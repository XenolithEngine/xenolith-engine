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

#ifdef SPRT_WASM

typedef unsigned long nfds_t;

struct pollfd {
	int fd; // file descriptor
	short events; // requested events
	short revents; // returned events
};

// clang-format off
#define POLLIN     0x001
#define POLLPRI    0x002
#define POLLOUT    0x004
#define POLLERR    0x008
#define POLLHUP    0x010
#define POLLNVAL   0x020
#define POLLRDNORM 0x040
#define POLLRDBAND 0x080
#define POLLWRNORM 0x100
#define POLLWRBAND 0x200
#define POLLMSG    0x400
#define POLLRDHUP  0x2000
// clang-format on

__SPRT_BEGIN_DECL

SPRT_API int poll(struct pollfd *__fds, nfds_t __nfds, int __timeout);

__SPRT_END_DECL

#endif // SPRT_WASM

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_POLL_H_
