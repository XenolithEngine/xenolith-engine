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

#ifndef CORE_RUNTIME_INCLUDE_LIBC_NETINET_TCP_H_
#define CORE_RUNTIME_INCLUDE_LIBC_NETINET_TCP_H_

/*
	POSIX <netinet/tcp.h> - TCP protocol (IPPROTO_TCP) socket option constants.
	- hosted SPRT build -> forwards to the system <netinet/tcp.h> (#include_next)
	- otherwise         -> the option constants below (Linux/musl values).
*/

#if defined(__SPRT_BUILD) && __STDC_HOSTED__ == 1

#include_next <netinet/tcp.h>

#else

#include <sprt/c/cross/__sprt_socket.h>

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_NETINET_TCP_H_
