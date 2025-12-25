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

#ifndef CORE_RUNTIME_INCLUDE_C___SPRT_LIMITS_H_
#define CORE_RUNTIME_INCLUDE_C___SPRT_LIMITS_H_

#include <c/bits/__sprt_def.h>
#include <c/bits/__sprt_int.h>

// clang-format off
#if '\xff' > 0
#define __SPRT_CHAR_MIN 0
#define __SPRT_CHAR_MAX 255
#else
#define __SPRT_CHAR_MIN (-128)
#define __SPRT_CHAR_MAX 127
#endif

#ifdef __CHAR_BIT__
#define __SPRT_CHAR_BIT __CHAR_BIT__
#else
#define __SPRT_CHAR_BIT 8
#endif

#define __SPRT_SCHAR_MIN (-128)
#define __SPRT_SCHAR_MAX 127
#define __SPRT_UCHAR_MAX 255

#define __SPRT_SHRT_MIN  (-1-__SPRT_SHRT_MAX)
#define __SPRT_INT_MIN  (-1-__SPRT_INT_MAX)
#define __SPRT_LONG_MIN (-1L-__SPRT_LONG_MAX)
#define __SPRT_LLONG_MIN (-1LL-__SPRT_LLONG_MAX)

#define __SPRT_MB_LEN_MAX 4

#define __SPRT_PIPE_BUF 4096
#define __SPRT_FILESIZEBITS 64

#ifndef NAME_MAX
#define __SPRT_NAME_MAX 255
#else
#define __SPRT_NAME_MAX NAME_MAX
#endif

#define __SPRT_PATH_MAX 4096
#define __SPRT_NGROUPS_MAX 32
#define __SPRT_ARG_MAX 131072
#define __SPRT_IOV_MAX 1024
#define __SPRT_SYMLOOP_MAX 40
#define __SPRT_WORD_BIT 32
#define __SPRT_SSIZE_MAX LONG_MAX
#define __SPRT_TZNAME_MAX 6
#define __SPRT_TTY_NAME_MAX 32
#define __SPRT_HOST_NAME_MAX 255

// clang-format on

#endif // CORE_RUNTIME_INCLUDE_C___SPRT_LIMITS_H_
