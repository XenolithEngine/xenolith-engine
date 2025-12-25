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

#ifndef CORE_RUNTIME_INCLUDE_LIBC_LIMITS_H_
#define CORE_RUNTIME_INCLUDE_LIBC_LIMITS_H_

#ifdef __SPRT_BUILD

#include_next <limits.h>

#else

#include <c/__sprt_limits.h>

#define CHAR_MIN __SPRT_CHAR_MIN
#define CHAR_MAX __SPRT_CHAR_MAX
#define CHAR_BIT __SPRT_CHAR_BIT
#define SCHAR_MIN __SPRT_SCHAR_MIN
#define SCHAR_MAX __SPRT_SCHAR_MAX
#define UCHAR_MAX __SPRT_UCHAR_MAX
#define SHRT_MIN __SPRT_SHRT_MIN
#define INT_MIN __SPRT_INT_MIN
#define LONG_MIN __SPRT_LONG_MIN
#define LLONG_MIN __SPRT_LLONG_MIN
#define MB_LEN_MAX __SPRT_MB_LEN_MAX
#define PIPE_BUF __SPRT_PIPE_BUF
#define FILESIZEBITS __SPRT_FILESIZEBITS
#define NAME_MAX __SPRT_NAME_MAX
#define PATH_MAX __SPRT_PATH_MAX
#define NGROUPS_MAX __SPRT_NGROUPS_MAX
#define ARG_MAX __SPRT_ARG_MAX
#define IOV_MAX __SPRT_IOV_MAX
#define SYMLOOP_MAX __SPRT_SYMLOOP_MAX
#define WORD_BIT __SPRT_WORD_BIT
#define SSIZE_MAX __SPRT_SSIZE_MAX
#define TZNAME_MAX __SPRT_TZNAME_MAX
#define TTY_NAME_MAX __SPRT_TTY_NAME_MAX
#define HOST_NAME_MAX __SPRT_HOST_NAME_MAX

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_LIMITS_H_
