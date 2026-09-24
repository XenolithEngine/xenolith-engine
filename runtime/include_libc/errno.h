/**
Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#ifndef CORE_RUNTIME_INCLUDE_LIBC_ERRNO_H_
#define CORE_RUNTIME_INCLUDE_LIBC_ERRNO_H_

/*
	Dispatch header for <errno.h>:
	- hosted SPRT build -> forwards to the system <errno.h> (#include_next)
	- otherwise         -> SPRT's own definitions via sprt/wrappers/libc/errno.h

	This header declares only macros (no types or functions):

	  errno - the modifiable lvalue holding the last error code; on the SPRT-own path
	          it expands to a per-thread location (*__errno_location()), so it is
	          thread-local rather than a plain global

	  the E* error-code constants (~134 of them: the ISO C trio EDOM, ERANGE, EILSEQ
	          plus the full POSIX/Linux set such as EACCES, EAGAIN, EBADF, EINVAL,
	          ENOENT, ENOMEM, EINTR, EEXIST, ...). These come in transitively via
	          <sprt/c/__sprt_errno.h> and <sprt/c/bits/__sprt_errno.h>.
*/

#if defined(__SPRT_BUILD) && __STDC_HOSTED__ == 1

#include_next <errno.h>

#else

#include <sprt/wrappers/libc/errno.h>

#endif

// Where the libc's own error numbers end, for a C++ library to tell a code the
// libc knows from any other (libc++'s config_elast.h: system_category maps a
// code above it to itself instead of to generic_category). Embox user mode
// speaks Linux's errno numbers -- its libc is musl-based -- and so takes the
// value libc++ gives Linux and musl. A BSD libc defines ELAST the same way.
// Outside the branches above: the runtime's own libc++ sources are a hosted
// build and take the first one.
#if defined(__EMBOX_USER__) && !defined(ELAST)
#define ELAST 4095
#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_ERRNO_H_
