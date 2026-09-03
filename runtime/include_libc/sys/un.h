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

#ifndef CORE_RUNTIME_INCLUDE_LIBC_SYS_UN_H_
#define CORE_RUNTIME_INCLUDE_LIBC_SYS_UN_H_

/*
	POSIX <sys/un.h> - UNIX-domain (AF_UNIX) socket address.
	- hosted SPRT build -> forwards to the system <sys/un.h> (#include_next)
	- otherwise         -> struct sockaddr_un below (Linux/musl layout).
*/

#if defined(__SPRT_BUILD) && __STDC_HOSTED__ == 1

#include_next <sys/un.h>

#else

#include <sys/socket.h>

// In a non-hosted build the sprt libc shim IS the libc at compile time (deps build against sprt
// include_libc rather than the platform sysroot — see the NUTTX/EMBOX branches of
// common/configure.mk), so the AF_UNIX address type has to come from here. The Linux/musl layout
// matches what every one of those targets uses.
//
// This was previously narrowed to `SPRT_WASM || SPRT_HOSTED_RTOS`, contradicting the comment above
// it: a cross Linux or musl target got no declaration at all, so `struct sockaddr_un` was an
// incomplete type for anything above the libc — AF_UNIX was unusable on exactly the targets whose
// kernels support it best. SO_PEERCRED and the AF_UNIX constants were already exposed for them,
// which is the shape of an oversight rather than a decision.
struct sockaddr_un {
	__SPRT_ID(sa_family_t) sun_family; // AF_UNIX
	char sun_path[108]; // pathname
};

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_SYS_UN_H_
