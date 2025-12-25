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

#ifndef CORE_RUNTIME_INCLUDE_LIBC_SYS_TYPES_H_
#define CORE_RUNTIME_INCLUDE_LIBC_SYS_TYPES_H_

#ifdef __SPRT_BUILD

#include_next <sys/types.h>

#else

#include <c/cross/__sprt_pthread.h>
#include <c/cross/__sprt_socket.h>
#include <c/bits/__sprt_size_t.h>
#include <c/bits/__sprt_time_t.h>
#include <c/bits/__sprt_ssize_t.h>
#include <c/bits/__sprt_time_t.h>
#include <c/bits/__sprt_sigset_t.h>
#include <c/bits/fdset.h>

typedef __SPRT_ID(size_t) size_t;
typedef __SPRT_ID(rsize_t) rsize_t;
typedef __SPRT_ID(off_t) off_t;
typedef __SPRT_ID(ssize_t) ssize_t;
typedef __SPRT_ID(time_t) time_t;
typedef __SPRT_ID(clock_t) clock_t;
typedef __SPRT_ID(clockid_t) clockid_t;
typedef __SPRT_ID(pthread_t) pthread_t;
typedef __SPRT_ID(pthread_once_t) pthread_once_t;
typedef __SPRT_ID(pthread_key_t) pthread_key_t;
typedef __SPRT_ID(pthread_spinlock_t) pthread_spinlock_t;
typedef __SPRT_ID(pthread_mutexattr_t) pthread_mutexattr_t;
typedef __SPRT_ID(pthread_cond_t) pthread_cond_t;
typedef __SPRT_ID(pthread_condattr_t) pthread_condattr_t;
typedef __SPRT_ID(pthread_rwlockattr_t) pthread_rwlockattr_t;
typedef __SPRT_ID(pthread_barrierattr_t) pthread_barrierattr_t;
typedef __SPRT_ID(pthread_mutex_t) pthread_mutex_t;
typedef __SPRT_ID(pthread_attr_t) pthread_attr_t;
typedef __SPRT_ID(pthread_rwlock_t) pthread_rwlock_t;
typedef __SPRT_ID(pthread_barrier_t) pthread_barrier_t;

typedef __SPRT_ID(fd_set) fd_set;

typedef __SPRT_ID(socklen_t) socklen_t;
typedef __SPRT_ID(sa_family_t) sa_family_t;

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_SYS_TYPES_H_
