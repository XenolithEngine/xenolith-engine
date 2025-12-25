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

#ifndef CORE_RUNTIME_INCLUDE_C_SYS___SPRT_STAT_H_
#define CORE_RUNTIME_INCLUDE_C_SYS___SPRT_STAT_H_

#include <c/bits/stat.h>
#include <c/bits/atfile.h>

__SPRT_BEGIN_DECL

SPRT_API int __SPRT_ID(stat)(const char *__SPRT_RESTRICT, struct __SPRT_STAT_NAME *__SPRT_RESTRICT);
SPRT_API int __SPRT_ID(fstat)(int, struct __SPRT_STAT_NAME *);
SPRT_API int __SPRT_ID(
		lstat)(const char *__SPRT_RESTRICT, struct __SPRT_STAT_NAME *__SPRT_RESTRICT);
SPRT_API int __SPRT_ID(
		fstatat)(int, const char *__SPRT_RESTRICT, struct __SPRT_STAT_NAME *__SPRT_RESTRICT, int);
SPRT_API int __SPRT_ID(chmod)(const char *, __SPRT_ID(mode_t));
SPRT_API int __SPRT_ID(fchmod)(int, __SPRT_ID(mode_t));
SPRT_API int __SPRT_ID(fchmodat)(int, const char *, __SPRT_ID(mode_t), int);
SPRT_API __SPRT_ID(mode_t) __SPRT_ID(umask)(__SPRT_ID(mode_t));
SPRT_API int __SPRT_ID(mkdir)(const char *, __SPRT_ID(mode_t));
SPRT_API int __SPRT_ID(mkfifo)(const char *, __SPRT_ID(mode_t));
SPRT_API int __SPRT_ID(mkdirat)(int, const char *, __SPRT_ID(mode_t));
SPRT_API int __SPRT_ID(mkfifoat)(int, const char *, __SPRT_ID(mode_t));
SPRT_API int __SPRT_ID(mknod)(const char *, __SPRT_ID(mode_t), __SPRT_ID(dev_t));
SPRT_API int __SPRT_ID(mknodat)(int, const char *, __SPRT_ID(mode_t), __SPRT_ID(dev_t));
SPRT_API int __SPRT_ID(futimens)(int, const struct __SPRT_TIMESPEC_NAME[2]);
SPRT_API int __SPRT_ID(utimensat)(int, const char *, const struct __SPRT_TIMESPEC_NAME[2], int);

__SPRT_END_DECL

#endif
