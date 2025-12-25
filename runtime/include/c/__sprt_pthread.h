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

#ifndef CORE_RUNTIME_INCLUDE_C___SPRT_PTHREAD_H_
#define CORE_RUNTIME_INCLUDE_C___SPRT_PTHREAD_H_

#include <c/cross/__sprt_pthread.h>
#include <c/bits/__sprt_size_t.h>
#include <c/bits/__sprt_time_t.h>
#include <c/bits/__sprt_sigset_t.h>
#include <c/__sprt_sched.h>

#define __SPRT_PTHREAD_CREATE_JOINABLE 0
#define __SPRT_PTHREAD_CREATE_DETACHED 1

#define __SPRT_PTHREAD_MUTEX_NORMAL 0
#define __SPRT_PTHREAD_MUTEX_DEFAULT 0
#define __SPRT_PTHREAD_MUTEX_RECURSIVE 1
#define __SPRT_PTHREAD_MUTEX_ERRORCHECK 2

#define __SPRT_PTHREAD_MUTEX_STALLED 0
#define __SPRT_PTHREAD_MUTEX_ROBUST 1

#define __SPRT_PTHREAD_PRIO_NONE 0
#define __SPRT_PTHREAD_PRIO_INHERIT 1
#define __SPRT_PTHREAD_PRIO_PROTECT 2

#define __SPRT_PTHREAD_INHERIT_SCHED 0
#define __SPRT_PTHREAD_EXPLICIT_SCHED 1

#define __SPRT_PTHREAD_SCOPE_SYSTEM 0
#define __SPRT_PTHREAD_SCOPE_PROCESS 1

#define __SPRT_PTHREAD_PROCESS_PRIVATE 0
#define __SPRT_PTHREAD_PROCESS_SHARED 1

#define __SPRT_PTHREAD_MUTEX_INITIALIZER {{{0}}}
#define __SPRT_PTHREAD_RWLOCK_INITIALIZER {{{0}}}
#define __SPRT_PTHREAD_COND_INITIALIZER {{{0}}}
#define __SPRT_PTHREAD_ONCE_INIT 0

#define __SPRT_PTHREAD_CANCEL_ENABLE 0
#define __SPRT_PTHREAD_CANCEL_DISABLE 1
#define __SPRT_PTHREAD_CANCEL_MASKED 2

#define __SPRT_PTHREAD_CANCEL_DEFERRED 0
#define __SPRT_PTHREAD_CANCEL_ASYNCHRONOUS 1

#define __SPRT_PTHREAD_CANCELED ((void *)-1)

#define __SPRT_PTHREAD_BARRIER_SERIAL_THREAD (-1)

#define __SPRT_PTHREAD_NULL (( __SPRT_ID(pthread_t))0)

__SPRT_BEGIN_DECL

SPRT_API int __SPRT_ID(pthread_create)(__SPRT_ID(pthread_t) * __SPRT_RESTRICT,
		const __SPRT_ID(pthread_attr_t) * __SPRT_RESTRICT, void *(*)(void *),
		void *__SPRT_RESTRICT);
SPRT_API int __SPRT_ID(pthread_detach)(__SPRT_ID(pthread_t));
SPRT_API __SPRT_NORETURN void __SPRT_ID(pthread_exit)(void *);
SPRT_API int __SPRT_ID(pthread_join)(__SPRT_ID(pthread_t), void **);

SPRT_API __SPRT_ID(pthread_t) __SPRT_ID(pthread_self)(void);

SPRT_API int __SPRT_ID(pthread_equal)(__SPRT_ID(pthread_t), __SPRT_ID(pthread_t));

SPRT_API int __SPRT_ID(pthread_setcancelstate)(int, int *);
SPRT_API int __SPRT_ID(pthread_setcanceltype)(int, int *);
SPRT_API void __SPRT_ID(pthread_testcancel)(void);
SPRT_API int __SPRT_ID(pthread_cancel)(__SPRT_ID(pthread_t));

SPRT_API int __SPRT_ID(pthread_getschedparam)(__SPRT_ID(pthread_t), int *__SPRT_RESTRICT,
		struct __SPRT_ID(sched_param) * __SPRT_RESTRICT);
SPRT_API int __SPRT_ID(
		pthread_setschedparam)(__SPRT_ID(pthread_t), int, const struct __SPRT_ID(sched_param) *);
SPRT_API int __SPRT_ID(pthread_setschedprio)(__SPRT_ID(pthread_t), int);

SPRT_API int __SPRT_ID(pthread_once)(__SPRT_ID(pthread_once_t) *, void (*)(void));

SPRT_API int __SPRT_ID(pthread_mutex_init)(__SPRT_ID(pthread_mutex_t) * __SPRT_RESTRICT,
		const __SPRT_ID(pthread_mutexattr_t) * __SPRT_RESTRICT);
SPRT_API int __SPRT_ID(pthread_mutex_lock)(__SPRT_ID(pthread_mutex_t) *);
SPRT_API int __SPRT_ID(pthread_mutex_unlock)(__SPRT_ID(pthread_mutex_t) *);
SPRT_API int __SPRT_ID(pthread_mutex_trylock)(__SPRT_ID(pthread_mutex_t) *);
SPRT_API int __SPRT_ID(pthread_mutex_timedlock)(__SPRT_ID(pthread_mutex_t) * __SPRT_RESTRICT,
		const __SPRT_TIMESPEC_NAME *__SPRT_RESTRICT);
SPRT_API int __SPRT_ID(pthread_mutex_destroy)(__SPRT_ID(pthread_mutex_t) *);
SPRT_API int __SPRT_ID(pthread_mutex_consistent)(__SPRT_ID(pthread_mutex_t) *);

SPRT_API int __SPRT_ID(pthread_mutex_getprioceiling)(
		const __SPRT_ID(pthread_mutex_t) * __SPRT_RESTRICT, int *__SPRT_RESTRICT);
SPRT_API int __SPRT_ID(pthread_mutex_setprioceiling)(__SPRT_ID(pthread_mutex_t) * __SPRT_RESTRICT,
		int, int *__SPRT_RESTRICT);

SPRT_API int __SPRT_ID(pthread_cond_init)(__SPRT_ID(pthread_cond_t) * __SPRT_RESTRICT,
		const __SPRT_ID(pthread_condattr_t) * __SPRT_RESTRICT);
SPRT_API int __SPRT_ID(pthread_cond_destroy)(__SPRT_ID(pthread_cond_t) *);
SPRT_API int __SPRT_ID(pthread_cond_wait)(__SPRT_ID(pthread_cond_t) * __SPRT_RESTRICT,
		__SPRT_ID(pthread_mutex_t) * __SPRT_RESTRICT);
SPRT_API int __SPRT_ID(pthread_cond_timedwait)(__SPRT_ID(pthread_cond_t) * __SPRT_RESTRICT,
		__SPRT_ID(pthread_mutex_t) * __SPRT_RESTRICT, const __SPRT_TIMESPEC_NAME *__SPRT_RESTRICT);
SPRT_API int __SPRT_ID(pthread_cond_broadcast)(__SPRT_ID(pthread_cond_t) *);
SPRT_API int __SPRT_ID(pthread_cond_signal)(__SPRT_ID(pthread_cond_t) *);

SPRT_API int __SPRT_ID(pthread_rwlock_init)(__SPRT_ID(pthread_rwlock_t) * __SPRT_RESTRICT,
		const __SPRT_ID(pthread_rwlockattr_t) * __SPRT_RESTRICT);
SPRT_API int __SPRT_ID(pthread_rwlock_destroy)(__SPRT_ID(pthread_rwlock_t) *);
SPRT_API int __SPRT_ID(pthread_rwlock_rdlock)(__SPRT_ID(pthread_rwlock_t) *);
SPRT_API int __SPRT_ID(pthread_rwlock_tryrdlock)(__SPRT_ID(pthread_rwlock_t) *);
SPRT_API int __SPRT_ID(pthread_rwlock_timedrdlock)(__SPRT_ID(pthread_rwlock_t) * __SPRT_RESTRICT,
		const __SPRT_TIMESPEC_NAME *__SPRT_RESTRICT);
SPRT_API int __SPRT_ID(pthread_rwlock_wrlock)(__SPRT_ID(pthread_rwlock_t) *);
SPRT_API int __SPRT_ID(pthread_rwlock_trywrlock)(__SPRT_ID(pthread_rwlock_t) *);
SPRT_API int __SPRT_ID(pthread_rwlock_timedwrlock)(__SPRT_ID(pthread_rwlock_t) * __SPRT_RESTRICT,
		const __SPRT_TIMESPEC_NAME *__SPRT_RESTRICT);
SPRT_API int __SPRT_ID(pthread_rwlock_unlock)(__SPRT_ID(pthread_rwlock_t) *);

SPRT_API int __SPRT_ID(pthread_spin_init)(__SPRT_ID(pthread_spinlock_t) *, int);
SPRT_API int __SPRT_ID(pthread_spin_destroy)(__SPRT_ID(pthread_spinlock_t) *);
SPRT_API int __SPRT_ID(pthread_spin_lock)(__SPRT_ID(pthread_spinlock_t) *);
SPRT_API int __SPRT_ID(pthread_spin_trylock)(__SPRT_ID(pthread_spinlock_t) *);
SPRT_API int __SPRT_ID(pthread_spin_unlock)(__SPRT_ID(pthread_spinlock_t) *);

SPRT_API int __SPRT_ID(pthread_barrier_init)(__SPRT_ID(pthread_barrier_t) * __SPRT_RESTRICT,
		const __SPRT_ID(pthread_barrierattr_t) * __SPRT_RESTRICT, unsigned);
SPRT_API int __SPRT_ID(pthread_barrier_destroy)(__SPRT_ID(pthread_barrier_t) *);
SPRT_API int __SPRT_ID(pthread_barrier_wait)(__SPRT_ID(pthread_barrier_t) *);

SPRT_API int __SPRT_ID(pthread_key_create)(__SPRT_ID(pthread_key_t) *, void (*)(void *));
SPRT_API int __SPRT_ID(pthread_key_delete)(__SPRT_ID(pthread_key_t));
SPRT_API void *__SPRT_ID(pthread_getspecific)(__SPRT_ID(pthread_key_t));
SPRT_API int __SPRT_ID(pthread_setspecific)(__SPRT_ID(pthread_key_t), const void *);

SPRT_API int __SPRT_ID(pthread_attr_init)(__SPRT_ID(pthread_attr_t) *);
SPRT_API int __SPRT_ID(pthread_attr_destroy)(__SPRT_ID(pthread_attr_t) *);

SPRT_API int __SPRT_ID(pthread_attr_getguardsize)(const __SPRT_ID(pthread_attr_t) * __SPRT_RESTRICT,
		__SPRT_ID(size_t) * __SPRT_RESTRICT);
SPRT_API int __SPRT_ID(pthread_attr_setguardsize)(__SPRT_ID(pthread_attr_t) *, __SPRT_ID(size_t));
SPRT_API int __SPRT_ID(pthread_attr_getstacksize)(const __SPRT_ID(pthread_attr_t) * __SPRT_RESTRICT,
		__SPRT_ID(size_t) * __SPRT_RESTRICT);
SPRT_API int __SPRT_ID(pthread_attr_setstacksize)(__SPRT_ID(pthread_attr_t) *, __SPRT_ID(size_t));
SPRT_API int __SPRT_ID(pthread_attr_getdetachstate)(const __SPRT_ID(pthread_attr_t) *, int *);
SPRT_API int __SPRT_ID(pthread_attr_setdetachstate)(__SPRT_ID(pthread_attr_t) *, int);
SPRT_API int __SPRT_ID(pthread_attr_getstack)(const __SPRT_ID(pthread_attr_t) * __SPRT_RESTRICT,
		void **__SPRT_RESTRICT, __SPRT_ID(size_t) * __SPRT_RESTRICT);
SPRT_API int __SPRT_ID(
		pthread_attr_setstack)(__SPRT_ID(pthread_attr_t) *, void *, __SPRT_ID(size_t));
SPRT_API int __SPRT_ID(pthread_attr_getscope)(const __SPRT_ID(pthread_attr_t) * __SPRT_RESTRICT,
		int *__SPRT_RESTRICT);
SPRT_API int __SPRT_ID(pthread_attr_setscope)(__SPRT_ID(pthread_attr_t) *, int);
SPRT_API int __SPRT_ID(pthread_attr_getschedpolicy)(
		const __SPRT_ID(pthread_attr_t) * __SPRT_RESTRICT, int *__SPRT_RESTRICT);
SPRT_API int __SPRT_ID(pthread_attr_setschedpolicy)(__SPRT_ID(pthread_attr_t) *, int);
SPRT_API int __SPRT_ID(
		pthread_attr_getschedparam)(const __SPRT_ID(pthread_attr_t) * __SPRT_RESTRICT,
		struct __SPRT_ID(sched_param) * __SPRT_RESTRICT);
SPRT_API int __SPRT_ID(pthread_attr_setschedparam)(__SPRT_ID(pthread_attr_t) * __SPRT_RESTRICT,
		const struct __SPRT_ID(sched_param) * __SPRT_RESTRICT);
SPRT_API int __SPRT_ID(pthread_attr_getinheritsched)(
		const __SPRT_ID(pthread_attr_t) * __SPRT_RESTRICT, int *__SPRT_RESTRICT);
SPRT_API int __SPRT_ID(pthread_attr_setinheritsched)(__SPRT_ID(pthread_attr_t) *, int);

SPRT_API int __SPRT_ID(pthread_mutexattr_destroy)(__SPRT_ID(pthread_mutexattr_t) *);
SPRT_API int __SPRT_ID(pthread_mutexattr_getprioceiling)(
		const __SPRT_ID(pthread_mutexattr_t) * __SPRT_RESTRICT, int *__SPRT_RESTRICT);
SPRT_API int __SPRT_ID(pthread_mutexattr_getprotocol)(
		const __SPRT_ID(pthread_mutexattr_t) * __SPRT_RESTRICT, int *__SPRT_RESTRICT);
SPRT_API int __SPRT_ID(pthread_mutexattr_getpshared)(
		const __SPRT_ID(pthread_mutexattr_t) * __SPRT_RESTRICT, int *__SPRT_RESTRICT);
SPRT_API int __SPRT_ID(pthread_mutexattr_getrobust)(
		const __SPRT_ID(pthread_mutexattr_t) * __SPRT_RESTRICT, int *__SPRT_RESTRICT);
SPRT_API int __SPRT_ID(pthread_mutexattr_gettype)(
		const __SPRT_ID(pthread_mutexattr_t) * __SPRT_RESTRICT, int *__SPRT_RESTRICT);
SPRT_API int __SPRT_ID(pthread_mutexattr_init)(__SPRT_ID(pthread_mutexattr_t) *);
SPRT_API int __SPRT_ID(pthread_mutexattr_setprioceiling)(__SPRT_ID(pthread_mutexattr_t) *, int);
SPRT_API int __SPRT_ID(pthread_mutexattr_setprotocol)(__SPRT_ID(pthread_mutexattr_t) *, int);
SPRT_API int __SPRT_ID(pthread_mutexattr_setpshared)(__SPRT_ID(pthread_mutexattr_t) *, int);
SPRT_API int __SPRT_ID(pthread_mutexattr_setrobust)(__SPRT_ID(pthread_mutexattr_t) *, int);
SPRT_API int __SPRT_ID(pthread_mutexattr_settype)(__SPRT_ID(pthread_mutexattr_t) *, int);

SPRT_API int __SPRT_ID(pthread_condattr_init)(__SPRT_ID(pthread_condattr_t) *);
SPRT_API int __SPRT_ID(pthread_condattr_destroy)(__SPRT_ID(pthread_condattr_t) *);
SPRT_API int __SPRT_ID(
		pthread_condattr_setclock)(__SPRT_ID(pthread_condattr_t) *, __SPRT_ID(clockid_t));
SPRT_API int __SPRT_ID(pthread_condattr_setpshared)(__SPRT_ID(pthread_condattr_t) *, int);
SPRT_API int __SPRT_ID(
		pthread_condattr_getclock)(const __SPRT_ID(pthread_condattr_t) * __SPRT_RESTRICT,
		__SPRT_ID(clockid_t) * __SPRT_RESTRICT);
SPRT_API int __SPRT_ID(pthread_condattr_getpshared)(
		const __SPRT_ID(pthread_condattr_t) * __SPRT_RESTRICT, int *__SPRT_RESTRICT);

SPRT_API int __SPRT_ID(pthread_rwlockattr_init)(__SPRT_ID(pthread_rwlockattr_t) *);
SPRT_API int __SPRT_ID(pthread_rwlockattr_destroy)(__SPRT_ID(pthread_rwlockattr_t) *);
SPRT_API int __SPRT_ID(pthread_rwlockattr_setpshared)(__SPRT_ID(pthread_rwlockattr_t) *, int);
SPRT_API int __SPRT_ID(pthread_rwlockattr_getpshared)(
		const __SPRT_ID(pthread_rwlockattr_t) * __SPRT_RESTRICT, int *__SPRT_RESTRICT);

SPRT_API int __SPRT_ID(pthread_barrierattr_destroy)(__SPRT_ID(pthread_barrierattr_t) *);
SPRT_API int __SPRT_ID(pthread_barrierattr_getpshared)(
		const __SPRT_ID(pthread_barrierattr_t) * __SPRT_RESTRICT, int *__SPRT_RESTRICT);
SPRT_API int __SPRT_ID(pthread_barrierattr_init)(__SPRT_ID(pthread_barrierattr_t) *);
SPRT_API int __SPRT_ID(pthread_barrierattr_setpshared)(__SPRT_ID(pthread_barrierattr_t) *, int);

SPRT_API int __SPRT_ID(pthread_atfork)(void (*)(void), void (*)(void), void (*)(void));

SPRT_API int __SPRT_ID(pthread_getconcurrency)(void);
SPRT_API int __SPRT_ID(pthread_setconcurrency)(int);

SPRT_API int __SPRT_ID(pthread_getcpuclockid)(__SPRT_ID(pthread_t), __SPRT_ID(clockid_t) *);

SPRT_API void __SPRT_ID(pthread_cleanup_push)(void (*)(void *), void *);
SPRT_API void __SPRT_ID(pthread_cleanup_pop)(int);

SPRT_API int __SPRT_ID(
		pthread_getaffinity_np)(__SPRT_ID(pthread_t), __SPRT_ID(size_t), __SPRT_ID(cpu_set_t) *);
SPRT_API int __SPRT_ID(pthread_setaffinity_np)(__SPRT_ID(pthread_t), __SPRT_ID(size_t),
		const __SPRT_ID(cpu_set_t) *);
SPRT_API int __SPRT_ID(pthread_getattr_np)(__SPRT_ID(pthread_t), __SPRT_ID(pthread_attr_t) *);
SPRT_API int __SPRT_ID(pthread_setname_np)(__SPRT_ID(pthread_t), const char *);
SPRT_API int __SPRT_ID(pthread_getname_np)(__SPRT_ID(pthread_t), char *, __SPRT_ID(size_t));
SPRT_API int __SPRT_ID(pthread_getattr_default_np)(__SPRT_ID(pthread_attr_t) *);
SPRT_API int __SPRT_ID(pthread_setattr_default_np)(const __SPRT_ID(pthread_attr_t) *);
SPRT_API int __SPRT_ID(pthread_tryjoin_np)(__SPRT_ID(pthread_t), void **);
SPRT_API int __SPRT_ID(
		pthread_timedjoin_np)(__SPRT_ID(pthread_t), void **, const __SPRT_TIMESPEC_NAME *);

__SPRT_END_DECL

#endif
