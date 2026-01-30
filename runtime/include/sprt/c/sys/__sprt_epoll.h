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

#ifndef CORE_RUNTIME_INCLUDE_C_SYS___SPRT_EPOLL_H_
#define CORE_RUNTIME_INCLUDE_C_SYS___SPRT_EPOLL_H_

#include <sprt/c/cross/__sprt_config.h>

#include <sprt/c/__sprt_stdint.h>
#include <sprt/c/cross/__sprt_signal.h>
#include <sprt/c/sys/__sprt_types.h>
#include <sprt/c/__sprt_fcntl.h>

#define __SPRT_EPOLL_CLOEXEC __SPRT_O_CLOEXEC
#define __SPRT_EPOLL_NONBLOCK __SPRT_O_NONBLOCK

#define __SPRT_EPOLLIN 0x001
#define __SPRT_EPOLLPRI 0x002
#define __SPRT_EPOLLOUT 0x004
#define __SPRT_EPOLLRDNORM 0x040
#define __SPRT_EPOLLNVAL 0x020
#define __SPRT_EPOLLRDBAND 0x080
#define __SPRT_EPOLLWRNORM 0x100
#define __SPRT_EPOLLWRBAND 0x200
#define __SPRT_EPOLLMSG 0x400
#define __SPRT_EPOLLERR 0x008
#define __SPRT_EPOLLHUP 0x010
#define __SPRT_EPOLLRDHUP 0x2000
#define __SPRT_EPOLLEXCLUSIVE (1U<<28)
#define __SPRT_EPOLLWAKEUP (1U<<29)
#define __SPRT_EPOLLONESHOT (1U<<30)
#define __SPRT_EPOLLET (1U<<31)

#define __SPRT_EPOLL_CTL_ADD 1
#define __SPRT_EPOLL_CTL_DEL 2
#define __SPRT_EPOLL_CTL_MOD 3

#ifdef __SPRT_BUILD
#define __SPRT_EPOLL_DATA_NAME __SPRT_ID(epoll_data)
#define __SPRT_EPOLL_EVENT_NAME __SPRT_ID(epoll_event)
#else
#define __SPRT_EPOLL_DATA_NAME epoll_data
#define __SPRT_EPOLL_EVENT_NAME epoll_event
#endif

typedef union __SPRT_EPOLL_DATA_NAME {
	void *ptr;
	int fd;
	__SPRT_ID(uint32_t) u32;
	__SPRT_ID(uint64_t) u64;
} __SPRT_ID(epoll_data_t);

struct __SPRT_EPOLL_EVENT_NAME {
	__SPRT_ID(uint32_t) events;
	__SPRT_ID(epoll_data_t) data;
}
#ifdef __x86_64__
__attribute__((__packed__))
#endif
;

#if __SPRT_CONFIG_HAVE_EPOLL || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS

__SPRT_BEGIN_DECL

__SPRT_CONFIG_HAVE_EPOLL_NOTICE
SPRT_API int __SPRT_ID(epoll_create)(int);

__SPRT_CONFIG_HAVE_EPOLL_NOTICE
SPRT_API int __SPRT_ID(epoll_create1)(int);

__SPRT_CONFIG_HAVE_EPOLL_NOTICE
SPRT_API int __SPRT_ID(epoll_ctl)(int, int, int, struct __SPRT_EPOLL_EVENT_NAME *);

__SPRT_CONFIG_HAVE_EPOLL_NOTICE
SPRT_API int __SPRT_ID(epoll_wait)(int, struct __SPRT_EPOLL_EVENT_NAME *, int, int);

__SPRT_CONFIG_HAVE_EPOLL_NOTICE
SPRT_API int __SPRT_ID(
		epoll_pwait)(int, struct __SPRT_EPOLL_EVENT_NAME *, int, int, const __SPRT_ID(sigset_t) *);

__SPRT_CONFIG_HAVE_EPOLL_NOTICE
SPRT_API int __SPRT_ID(epoll_pwait2)(int, struct __SPRT_EPOLL_EVENT_NAME *, int,
		const struct __SPRT_TIMESPEC_NAME *, const __SPRT_ID(sigset_t) * __mask);

__SPRT_END_DECL

#endif

#endif // CORE_RUNTIME_INCLUDE_C_SYS___SPRT_EPOLL_H_
