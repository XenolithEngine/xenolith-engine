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

#ifndef CORE_RUNTIME_INCLUDE_LIBC_SYS_IOCTL_H_
#define CORE_RUNTIME_INCLUDE_LIBC_SYS_IOCTL_H_

#if defined(__SPRT_BUILD) && __STDC_HOSTED__ == 1

#include_next <sys/ioctl.h>

#else

#include <sprt/c/sys/__sprt_ioctl.h>
#include <sprt/c/__sprt_stdarg.h>

#define TIOCGWINSZ __SPRT_TIOCGWINSZ
#define TIOCSWINSZ __SPRT_TIOCSWINSZ

typedef __SPRT_ID(intptr_t) intptr_t;

struct winsize {
	unsigned short ws_row;
	unsigned short ws_col;
	unsigned short ws_xpixel; /* unused */
	unsigned short ws_ypixel; /* unused */
};

__SPRT_BEGIN_DECL

SPRT_UMBRELLA_FUNC
int ioctl(int fd, int op, ...) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	intptr_t arg;
	__sprt_va_list ap;
	__sprt_va_start(ap, __cmd);
	arg = __sprt_va_arg(ap, intptr_t);
	__sprt_va_end(ap);

	return __sprt_ioctl(fd, op, arg);
}
#endif

__SPRT_END_DECL

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_SYS_IOCTL_H_
