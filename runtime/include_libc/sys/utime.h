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

#ifndef CORE_RUNTIME_INCLUDE_LIBC_SYS_UTIME_H_
#define CORE_RUNTIME_INCLUDE_LIBC_SYS_UTIME_H_

/*
	MSVC-style <sys/utime.h> (file timestamp updates).
	Unlike most libc shims this is self-contained: there is no __SPRT_BUILD dispatch.
	It always pulls in the related headers
	  <sys/time.h>, <unistd.h>, <utime.h>
	and then adds the MSVC-named compatibility wrappers below. The POSIX utime() and
	struct utimbuf themselves come from <utime.h> (see include_libc/utime.h).

	Types:
	  struct _utimbuf - MSVC-named timestamp pair (members: time_t actime, modtime)

	Functions:
	  _utime - MSVC-named variant of utime: set a file's access and modification
	           times from a struct _utimbuf (a NULL buffer uses the current time)
*/

#include <sys/time.h>
#include <unistd.h>
#include <utime.h>

struct _utimbuf {
	__SPRT_ID(time_t) actime;
	__SPRT_ID(time_t) modtime;
};

__SPRT_BEGIN_DECL

SPRT_UMBRELLA_FUNC
int _utime(const char *path, const struct _utimbuf *buf) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_utime(path, buf);
}
#endif

__SPRT_END_DECL

#endif // CORE_RUNTIME_INCLUDE_LIBC_SYS_UTIME_H_
