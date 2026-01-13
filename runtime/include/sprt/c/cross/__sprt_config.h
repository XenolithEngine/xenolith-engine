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

#ifndef CORE_RUNTIME_INCLUDE_C_CROSS___SPRT_CONFIG_H_
#define CORE_RUNTIME_INCLUDE_C_CROSS___SPRT_CONFIG_H_

// Per-OS/Per-arch Stappler Runtime configuration
#include <sprt/c/bits/__sprt_def.h>

#define SPRT_CROSS_CONFIG_NAME_(x) #x
#define SPRT_CROSS_CONFIG_NAME(x) SPRT_CROSS_CONFIG_NAME_(x)

// clang-format off
#include SPRT_CROSS_CONFIG_NAME(sprt/c/cross/__SPRT_PLATFORM_NAME/config.h)
#include SPRT_CROSS_CONFIG_NAME(sprt/c/cross/__SPRT_PLATFORM_NAME/__SPRT_ARCH_NAME/config.h)
// clang-format on

/*
	All fallback values should be defined there
	Use this list to create your own platform configurations
*/

#ifndef __SPRT_CONFIG_HAVE_ALLIGNED_ALLOC
#define __SPRT_CONFIG_HAVE_ALLIGNED_ALLOC 1
#endif

#ifndef __SPRT_CONFIG_HAVE_UNISTD_CHOWN
#define __SPRT_CONFIG_HAVE_UNISTD_CHOWN 1
#endif

#ifndef __SPRT_CONFIG_HAVE_UNISTD_DUP
#define __SPRT_CONFIG_HAVE_UNISTD_DUP 1
#endif

#ifndef __SPRT_CONFIG_HAVE_UNISTD_DUP3
#define __SPRT_CONFIG_HAVE_UNISTD_DUP3 1
#endif

#ifndef __SPRT_CONFIG_HAVE_UNISTD_EXEC
#define __SPRT_CONFIG_HAVE_UNISTD_EXEC 1
#endif

#ifndef __SPRT_CONFIG_HAVE_UNISTD_FEXEC
#define __SPRT_CONFIG_HAVE_UNISTD_FEXEC 1
#endif

#ifndef __SPRT_CONFIG_HAVE_UNISTD_SETLOGIN
#define __SPRT_CONFIG_HAVE_UNISTD_SETLOGIN 1
#endif

#ifndef __SPRT_CONFIG_HAVE_UNISTD_DOMAINNAME
#define __SPRT_CONFIG_HAVE_UNISTD_DOMAINNAME 1
#endif

#ifndef __SPRT_CONFIG_HAVE_UNISTD_NICE
#define __SPRT_CONFIG_HAVE_UNISTD_NICE 1
#endif

#ifndef __SPRT_CONFIG_HAVE_UNISTD_CONF
#define __SPRT_CONFIG_HAVE_UNISTD_CONF 1
#endif

#ifndef __SPRT_CONFIG_HAVE_UNISTD_GETPPID
#define __SPRT_CONFIG_HAVE_UNISTD_GETPPID 1
#endif

#ifndef __SPRT_CONFIG_HAVE_SELECT
#define __SPRT_CONFIG_HAVE_SELECT 1
#endif

#ifndef __SPRT_CONFIG_HAVE_ADJTIME
#define __SPRT_CONFIG_HAVE_ADJTIME 1
#endif

#ifndef __SPRT_CONFIG_HAVE_EPOLL
#define __SPRT_CONFIG_HAVE_EPOLL 0
#endif

#ifndef __SPRT_CONFIG_HAVE_EVENTFD
#define __SPRT_CONFIG_HAVE_EVENTFD 0
#endif

#ifndef __SPRT_CONFIG_HAVE_SIGNALFD
#define __SPRT_CONFIG_HAVE_SIGNALFD 0
#endif

#ifndef __SPRT_CONFIG_HAVE_TIMERFD
#define __SPRT_CONFIG_HAVE_TIMERFD 0
#endif

#ifndef __SPRT_CONFIG_HAVE_ALOOPER
#define __SPRT_CONFIG_HAVE_ALOOPER 0
#endif

#ifndef __SPRT_CONFIG_HAVE_URING
#define __SPRT_CONFIG_HAVE_URING 0
#endif

#ifndef __SPRT_CONFIG_HAVE_FUTEX
#define __SPRT_CONFIG_HAVE_FUTEX 0
#endif

#ifndef __SPRT_CONFIG_HAVE_STDLIB_MB
#define __SPRT_CONFIG_HAVE_STDLIB_MB 0
#endif

#ifndef __SPRT_CONFIG_HAVE_PTHREAD_CANCEL
#define __SPRT_CONFIG_HAVE_PTHREAD_CANCEL 1
#endif

#ifndef __SPRT_CONFIG_HAVE_PTHREAD_MUTEX_ROBUST
#define __SPRT_CONFIG_HAVE_PTHREAD_MUTEX_ROBUST 1
#endif

#ifndef __SPRT_CONFIG_HAVE_PTHREAD_MUTEX_PRIO
#define __SPRT_CONFIG_HAVE_PTHREAD_MUTEX_PRIO 1
#endif

#ifndef __SPRT_CONFIG_HAVE_PTHREAD_INHERITSCHED
#define __SPRT_CONFIG_HAVE_PTHREAD_INHERITSCHED 1
#endif

#ifndef __SPRT_CONFIG_HAVE_PTHREAD_AFFINITY
#define __SPRT_CONFIG_HAVE_PTHREAD_AFFINITY 1
#endif

#ifndef __SPRT_CONFIG_HAVE_PTHREAD_ATTRDEFAULT
#define __SPRT_CONFIG_HAVE_PTHREAD_ATTRDEFAULT 1
#endif

#ifndef __SPRT_CONFIG_HAVE_PTHREAD_CONCURRENCY
#define __SPRT_CONFIG_HAVE_PTHREAD_CONCURRENCY 1
#endif

#ifndef __SPRT_CONFIG_HAVE_PTHREAD_TRYJOIN
#define __SPRT_CONFIG_HAVE_PTHREAD_TRYJOIN 1
#endif

#ifndef __SPRT_CONFIG_HAVE_PTHREAD_TIMEDJOIN
#define __SPRT_CONFIG_HAVE_PTHREAD_TIMEDJOIN 1
#endif

#ifndef __SPRT_CONFIG_HAVE_PTHREAD_GETNAME
#define __SPRT_CONFIG_HAVE_PTHREAD_GETNAME 1
#endif

#endif
