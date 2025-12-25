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

#ifndef CORE_RUNTIME_INCLUDE_C_SYS___SPRT_SELECT_H_
#define CORE_RUNTIME_INCLUDE_C_SYS___SPRT_SELECT_H_

#include <c/bits/__sprt_size_t.h>
#include <c/bits/__sprt_time_t.h>
#include <c/bits/__sprt_sigset_t.h>
#include <c/cross/__sprt_config.h>

#include <c/bits/fdset.h>

__SPRT_BEGIN_DECL

#if __SPRT_CONFIG_HAVE_SELECT || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS

SPRT_API int __SPRT_ID(select)(int, __SPRT_ID(fd_set) * __SPRT_RESTRICT,
		__SPRT_ID(fd_set) * __SPRT_RESTRICT, __SPRT_ID(fd_set) * __SPRT_RESTRICT,
		__SPRT_TIMEVAL_NAME *__SPRT_RESTRICT);
SPRT_API int __SPRT_ID(pselect)(int, __SPRT_ID(fd_set) * __SPRT_RESTRICT,
		__SPRT_ID(fd_set) * __SPRT_RESTRICT, __SPRT_ID(fd_set) * __SPRT_RESTRICT,
		const __SPRT_TIMESPEC_NAME *__SPRT_RESTRICT, const __SPRT_ID(sigset_t) * __SPRT_RESTRICT);

#endif

__SPRT_END_DECL

#define __SPRT_NFDBITS (8*(int)sizeof(long))

#endif // CORE_RUNTIME_INCLUDE_C_SYS___SPRT_SELECT_H_
