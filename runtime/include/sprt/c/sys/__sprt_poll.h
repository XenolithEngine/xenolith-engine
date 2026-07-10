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

#ifndef CORE_RUNTIME_INCLUDE_C_SYS___SPRT_POLL_H_
#define CORE_RUNTIME_INCLUDE_C_SYS___SPRT_POLL_H_

#include <sprt/c/bits/__sprt_time_t.h>
#include <sprt/c/cross/__sprt_signal.h>
#include <sprt/c/cross/__sprt_config.h>

// struct pollfd, nfds_t and the POLL* constants are defined per-platform to be
// ABI-compatible with the native <poll.h> / winsock (SPRuntimeCSysPoll.cpp
// static_asserts them against the system header).
#include <sprt/c/cross/__sprt_polltypes.h>

__SPRT_BEGIN_DECL

#if __SPRT_CONFIG_HAVE_POLL || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS

__SPRT_CONFIG_HAVE_POLL_NOTICE
SPRT_API int __SPRT_ID(poll)(struct __SPRT_POLLFD_NAME *, __SPRT_ID(nfds_t), int);

__SPRT_CONFIG_HAVE_POLL_NOTICE
SPRT_API int __SPRT_ID(ppoll)(struct __SPRT_POLLFD_NAME *, __SPRT_ID(nfds_t),
		const struct __SPRT_TIMESPEC_NAME *, const __SPRT_ID(sigset_t) *);

#endif

__SPRT_END_DECL

#endif // CORE_RUNTIME_INCLUDE_C_SYS___SPRT_POLL_H_
