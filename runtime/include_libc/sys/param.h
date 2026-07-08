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

#ifndef CORE_RUNTIME_INCLUDE_LIBC_SYS_PARAM_H_
#define CORE_RUNTIME_INCLUDE_LIBC_SYS_PARAM_H_

/*
	BSD/glibc-style <sys/param.h> grab-bag header. Most portable code includes it
	only defensively (e.g. OpenSSL's internal/sockets.h behind NO_SYS_PARAM_H) to
	pull in a handful of legacy BSD macros; it carries no unique declarations of its
	own. Like <endian.h> this shim is self-contained (no __SPRT_BUILD dispatch): on
	the hosted build it forwards to the system header, otherwise it re-exports
	<limits.h> + <endian.h> and defines the classic BSD helper macros.

	Macros:
	  MIN(a,b), MAX(a,b)      - minimum / maximum
	  howmany(x,y)            - ceil(x / y)
	  roundup(x,y)            - x rounded up to the next multiple of y
	  rounddown(x,y)          - x rounded down to a multiple of y
	  powerof2(x)             - non-zero iff x is a power of two (or zero)
	  NBBY                    - number of bits in a byte (8)
	  MAXPATHLEN, MAXHOSTNAMELEN, MAXSYMLINKS - legacy path/name limits
*/

#if defined(__SPRT_BUILD) && __STDC_HOSTED__ == 1

#include_next <sys/param.h>

#else

#include <limits.h>
#include <endian.h>

#ifdef SPRT_WASM

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif

#ifndef howmany
#define howmany(x, y) (((x) + ((y) - 1)) / (y))
#endif
#ifndef roundup
#define roundup(x, y) ((((x) + ((y) - 1)) / (y)) * (y))
#endif
#ifndef rounddown
#define rounddown(x, y) (((x) / (y)) * (y))
#endif
#ifndef powerof2
#define powerof2(x) ((((x) - 1) & (x)) == 0)
#endif

#ifndef NBBY
#define NBBY 8
#endif

#ifndef MAXPATHLEN
#ifdef PATH_MAX
#define MAXPATHLEN PATH_MAX
#else
#define MAXPATHLEN 4'096
#endif
#endif

#ifndef MAXHOSTNAMELEN
#ifdef HOST_NAME_MAX
#define MAXHOSTNAMELEN HOST_NAME_MAX
#else
#define MAXHOSTNAMELEN 256
#endif
#endif

#ifndef MAXSYMLINKS
#define MAXSYMLINKS 20
#endif

#endif // SPRT_WASM

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_SYS_PARAM_H_
