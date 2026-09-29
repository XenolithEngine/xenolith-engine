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

#ifndef CORE_RUNTIME_INCLUDE_LIBC_SYS_FILE_H_
#define CORE_RUNTIME_INCLUDE_LIBC_SYS_FILE_H_

/*
	<sys/file.h> - flock(2), advisory whole-file locks.

	- hosted SPRT build -> forwards to the system <sys/file.h> (#include_next)
	- otherwise         -> SPRT's own declarations below

	Constants: LOCK_SH, LOCK_EX, LOCK_NB, LOCK_UN
	Functions: flock - where the platform has it (__SPRT_CONFIG_HAVE_FLOCK),
	           ENOSYS elsewhere
*/

#if defined(__SPRT_BUILD) && __STDC_HOSTED__ == 1

#include_next <sys/file.h>

#else

#include <sprt/c/sys/__sprt_file.h>

#define LOCK_SH __SPRT_LOCK_SH
#define LOCK_EX __SPRT_LOCK_EX
#define LOCK_NB __SPRT_LOCK_NB
#define LOCK_UN __SPRT_LOCK_UN

__SPRT_BEGIN_DECL

#if __SPRT_CONFIG_HAVE_FLOCK || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS

SPRT_UMBRELLA_FUNC
int flock(int fd, int op) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_flock(fd, op);
}
#endif

#endif

__SPRT_END_DECL

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_SYS_FILE_H_
