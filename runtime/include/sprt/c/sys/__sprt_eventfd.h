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

#ifndef CORE_RUNTIME_INCLUDE_C_SYS___SPRT_EVENTFD_H_
#define CORE_RUNTIME_INCLUDE_C_SYS___SPRT_EVENTFD_H_

#include <sprt/c/cross/__sprt_config.h>

#include <sprt/c/__sprt_stdint.h>
#include <sprt/c/__sprt_fcntl.h>

typedef __SPRT_ID(uint64_t) __SPRT_ID(eventfd_t);

#define __SPRT_EFD_SEMAPHORE 1
#define __SPRT_EFD_CLOEXEC __SPRT_O_CLOEXEC
#define __SPRT_EFD_NONBLOCK __SPRT_O_NONBLOCK

#if __SPRT_CONFIG_HAVE_EVENTFD || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS

__SPRT_BEGIN_DECL

SPRT_API int __SPRT_ID(eventfd)(unsigned int, int);
SPRT_API int __SPRT_ID(eventfd_read)(int, __SPRT_ID(eventfd_t) *);
SPRT_API int __SPRT_ID(eventfd_write)(int, __SPRT_ID(eventfd_t));

__SPRT_END_DECL

#endif

#endif // CORE_RUNTIME_INCLUDE_C_SYS___SPRT_EVENTFD_H_
