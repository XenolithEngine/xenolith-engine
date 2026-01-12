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

#ifndef CORE_RUNTIME_INCLUDE_C_SYS___SPRT_URING_H_
#define CORE_RUNTIME_INCLUDE_C_SYS___SPRT_URING_H_

#include <sprt/c/cross/__sprt_config.h>

#include <sprt/c/bits/uring.h>
#include <sprt/c/bits/__sprt_sigset_t.h>
#include <sprt/c/bits/__sprt_size_t.h>

#if __SPRT_CONFIG_HAVE_URING || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS

__SPRT_BEGIN_DECL

SPRT_API int __SPRT_ID(io_uring_setup)(unsigned entries, struct io_uring_params *p);

SPRT_API int __SPRT_ID(io_uring_enter)(int ring_fd, unsigned int to_submit,
		unsigned int min_complete, unsigned int flags, const __SPRT_ID(sigset_t) *sig = nullptr);

SPRT_API int __SPRT_ID(io_uring_enter2)(int ring_fd, unsigned int to_submit,
		unsigned int min_complete, unsigned int flags, void *arg = nullptr,
		__SPRT_ID(size_t) argsize = 0);

SPRT_API int __SPRT_ID(io_uring_register)(unsigned int fd, unsigned int opcode, const void *arg,
		unsigned int nr_args);

__SPRT_END_DECL

#endif

#endif
