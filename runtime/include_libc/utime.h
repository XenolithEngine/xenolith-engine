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

#ifndef CORE_RUNTIME_INCLUDE_LIBC_UTIME_H_
#define CORE_RUNTIME_INCLUDE_LIBC_UTIME_H_

/*
	Dispatch header for the POSIX <utime.h>:
	- hosted SPRT build -> forwards to the system <utime.h> (#include_next)
	- otherwise         -> SPRT's own declarations (defined inline below)

	Public surface provided by the SPRT-own path (internal __sprt_* helpers excluded):

	Types:
	  struct utimbuf - holds the access/modification times to apply
	                   (members: time_t actime, time_t modtime;
	                    brought in via <sprt/c/__sprt_utime.h>)

	Functions:
	  utime - set a file's access and modification times from a struct utimbuf
	          (a NULL buffer sets both times to the current time)
*/

#if defined(__SPRT_BUILD) && __STDC_HOSTED__ == 1

#include_next <utime.h>

#else

#include <sprt/c/__sprt_utime.h>

__SPRT_BEGIN_DECL

SPRT_UMBRELLA_FUNC
int utime(const char *path, const struct __SPRT_UTIMBUF_NAME *buf) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_utime(path, buf);
}
#endif

__SPRT_END_DECL

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_UTIME_H_
