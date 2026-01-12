/**
 Copyright (c) 2026 Xenolith Team <admin@stappler.org>

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

#ifndef CORE_RUNTIME_INCLUDE_C_SYS___SPRT_TIMERFD_H_
#define CORE_RUNTIME_INCLUDE_C_SYS___SPRT_TIMERFD_H_

#include <sprt/c/cross/__sprt_config.h>

#include <sprt/c/__sprt_stdint.h>
#include <sprt/c/__sprt_fcntl.h>
#include <sprt/c/__sprt_time.h>

#define __SPRT_TFD_NONBLOCK __SPRT_O_NONBLOCK
#define __SPRT_TFD_CLOEXEC __SPRT_O_CLOEXEC

#define __SPRT_TFD_TIMER_ABSTIME 1
#define __SPRT_TFD_TIMER_CANCEL_ON_SET (1 << 1)

#if __SPRT_CONFIG_HAVE_TIMERFD || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS

__SPRT_BEGIN_DECL

SPRT_API int __SPRT_ID(timerfd_create)(int, int);
SPRT_API int __SPRT_ID(timerfd_settime)(int, int, const struct __SPRT_ITIMERSPEC_NAME *,
		struct __SPRT_ITIMERSPEC_NAME *);
SPRT_API int __SPRT_ID(timerfd_gettime)(int, struct __SPRT_ITIMERSPEC_NAME *);

__SPRT_END_DECL

#endif

#endif // CORE_RUNTIME_INCLUDE_C_SYS___SPRT_TIMERFD_H_
