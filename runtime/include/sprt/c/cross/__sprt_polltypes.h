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

#ifndef CORE_RUNTIME_INCLUDE_C_CROSS___SPRT_POLLTYPES_H_
#define CORE_RUNTIME_INCLUDE_C_CROSS___SPRT_POLLTYPES_H_

#include <sprt/c/bits/__sprt_def.h>

// Per-platform, ABI-compatible <poll.h> surface (SPRuntimeCSysPoll.cpp static_asserts
// every value/layout below against the native <poll.h> on hosted targets, and against
// winsock's mstcpip POLL* / WSAPOLLFD on Windows).
//
// struct pollfd keeps the portable POSIX shape { int fd; short; short } on every
// platform - it is identical to glibc / musl / Bionic / BSD. Windows' WSAPoll takes a
// WSAPOLLFD whose fd is a 64-bit SOCKET, so the wrapper translates each entry there (the
// open64 pattern); the SPRT struct itself stays POSIX-shaped for portable callers.

#ifdef __SPRT_BUILD
#define __SPRT_POLLFD_NAME __SPRT_ID(pollfd)
#else
#define __SPRT_POLLFD_NAME pollfd
#endif

struct __SPRT_POLLFD_NAME {
	int fd; // file descriptor
	short events; // requested events
	short revents; // returned events
};

// nfds_t is a by-value count: glibc / musl / winsock (ULONG) use a long, Apple and
// Bionic use unsigned int. Matched per-platform so the static_assert on size passes.
#if SPRT_APPLE || SPRT_ANDROID
typedef unsigned int __SPRT_ID(nfds_t);
#else
typedef unsigned long __SPRT_ID(nfds_t);
#endif

// clang-format off
#if SPRT_APPLE

// BSD (macOS / iOS): POLLWRNORM aliases POLLOUT, no POLLMSG / POLLRDHUP.
#define __SPRT_POLLIN     0x0001
#define __SPRT_POLLPRI    0x0002
#define __SPRT_POLLOUT    0x0004
#define __SPRT_POLLRDNORM 0x0040
#define __SPRT_POLLWRNORM 0x0004
#define __SPRT_POLLRDBAND 0x0080
#define __SPRT_POLLWRBAND 0x0100
#define __SPRT_POLLERR    0x0008
#define __SPRT_POLLHUP    0x0010
#define __SPRT_POLLNVAL   0x0020

#elif SPRT_WINDOWS

// winsock (WSAPoll): POLLIN = POLLRDNORM|POLLRDBAND, POLLOUT = POLLWRNORM.
#define __SPRT_POLLRDNORM 0x0100
#define __SPRT_POLLRDBAND 0x0200
#define __SPRT_POLLIN     (__SPRT_POLLRDNORM | __SPRT_POLLRDBAND)
#define __SPRT_POLLPRI    0x0400
#define __SPRT_POLLWRNORM 0x0010
#define __SPRT_POLLOUT    (__SPRT_POLLWRNORM)
#define __SPRT_POLLWRBAND 0x0020
#define __SPRT_POLLERR    0x0001
#define __SPRT_POLLHUP    0x0002
#define __SPRT_POLLNVAL   0x0004

#else

// GNU (glibc / musl -> linux / wasm) and Bionic (Android) share these values.
#define __SPRT_POLLIN     0x001
#define __SPRT_POLLPRI    0x002
#define __SPRT_POLLOUT    0x004
#define __SPRT_POLLERR    0x008
#define __SPRT_POLLHUP    0x010
#define __SPRT_POLLNVAL   0x020
#define __SPRT_POLLRDNORM 0x040
#define __SPRT_POLLRDBAND 0x080
#define __SPRT_POLLWRNORM 0x100
#define __SPRT_POLLWRBAND 0x200
#define __SPRT_POLLMSG    0x400
#define __SPRT_POLLRDHUP  0x2000

#endif
// clang-format on

#endif // CORE_RUNTIME_INCLUDE_C_CROSS___SPRT_POLLTYPES_H_
