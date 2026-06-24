/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

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

#ifndef CORE_RUNTIME_INCLUDE_C___SPRT_THREADS_H_
#define CORE_RUNTIME_INCLUDE_C___SPRT_THREADS_H_

// C11 <threads.h> built on SPRT's pthread implementation: every C11 primitive
// maps directly onto the corresponding pthread object, and each thrd_*/mtx_*/
// cnd_*/tss_*/call_once entry forwards to the matching __sprt_pthread_* function
// (see runtime/core/pthread/c11threads.cc). The impl symbols live next to the
// pthread ones in runtime_core.

#include <sprt/c/__sprt_pthread.h>
#include <sprt/c/__sprt_time.h>

typedef __SPRT_ID(pthread_t) __SPRT_ID(thrd_t);
typedef __SPRT_ID(pthread_mutex_t) __SPRT_ID(mtx_t);
typedef __SPRT_ID(pthread_cond_t) __SPRT_ID(cnd_t);
typedef __SPRT_ID(pthread_key_t) __SPRT_ID(tss_t);
typedef __SPRT_ID(pthread_once_t) __SPRT_ID(once_flag);

typedef int (*__SPRT_ID(thrd_start_t))(void *);
typedef void (*__SPRT_ID(tss_dtor_t))(void *);

// thrd_* return codes
#define __SPRT_THRD_SUCCESS 0
#define __SPRT_THRD_BUSY 1
#define __SPRT_THRD_ERROR 2
#define __SPRT_THRD_NOMEM 3
#define __SPRT_THRD_TIMEDOUT 4

// mtx_init types
#define __SPRT_MTX_PLAIN 0
#define __SPRT_MTX_RECURSIVE 1
#define __SPRT_MTX_TIMED 2

#define __SPRT_TSS_DTOR_ITERATIONS __SPRT_PTHREAD_DESTRUCTOR_ITERATIONS
#define __SPRT_ONCE_FLAG_INIT __SPRT_PTHREAD_ONCE_INIT

__SPRT_BEGIN_DECL

// threads
SPRT_API int __SPRT_ID(thrd_create)(__SPRT_ID(thrd_t) *, __SPRT_ID(thrd_start_t), void *);
SPRT_API int __SPRT_ID(thrd_equal)(__SPRT_ID(thrd_t), __SPRT_ID(thrd_t));
SPRT_API __SPRT_ID(thrd_t) __SPRT_ID(thrd_current)(void);
SPRT_API int __SPRT_ID(thrd_sleep)(const struct __SPRT_TIMESPEC_NAME *, struct __SPRT_TIMESPEC_NAME *);
SPRT_API void __SPRT_ID(thrd_yield)(void);
SPRT_API __SPRT_NORETURN void __SPRT_ID(thrd_exit)(int);
SPRT_API int __SPRT_ID(thrd_detach)(__SPRT_ID(thrd_t));
SPRT_API int __SPRT_ID(thrd_join)(__SPRT_ID(thrd_t), int *);

// mutexes
SPRT_API int __SPRT_ID(mtx_init)(__SPRT_ID(mtx_t) *, int);
SPRT_API int __SPRT_ID(mtx_lock)(__SPRT_ID(mtx_t) *);
SPRT_API int __SPRT_ID(mtx_trylock)(__SPRT_ID(mtx_t) *);
SPRT_API int __SPRT_ID(mtx_timedlock)(__SPRT_ID(mtx_t) * __SPRT_RESTRICT,
		const struct __SPRT_TIMESPEC_NAME * __SPRT_RESTRICT);
SPRT_API int __SPRT_ID(mtx_unlock)(__SPRT_ID(mtx_t) *);
SPRT_API void __SPRT_ID(mtx_destroy)(__SPRT_ID(mtx_t) *);

// condition variables
SPRT_API int __SPRT_ID(cnd_init)(__SPRT_ID(cnd_t) *);
SPRT_API int __SPRT_ID(cnd_signal)(__SPRT_ID(cnd_t) *);
SPRT_API int __SPRT_ID(cnd_broadcast)(__SPRT_ID(cnd_t) *);
SPRT_API int __SPRT_ID(cnd_wait)(__SPRT_ID(cnd_t) *, __SPRT_ID(mtx_t) *);
SPRT_API int __SPRT_ID(cnd_timedwait)(__SPRT_ID(cnd_t) * __SPRT_RESTRICT,
		__SPRT_ID(mtx_t) * __SPRT_RESTRICT, const struct __SPRT_TIMESPEC_NAME * __SPRT_RESTRICT);
SPRT_API void __SPRT_ID(cnd_destroy)(__SPRT_ID(cnd_t) *);

// thread-specific storage
SPRT_API int __SPRT_ID(tss_create)(__SPRT_ID(tss_t) *, __SPRT_ID(tss_dtor_t));
SPRT_API void *__SPRT_ID(tss_get)(__SPRT_ID(tss_t));
SPRT_API int __SPRT_ID(tss_set)(__SPRT_ID(tss_t), void *);
SPRT_API void __SPRT_ID(tss_delete)(__SPRT_ID(tss_t));

// one-time initialization
SPRT_API void __SPRT_ID(call_once)(__SPRT_ID(once_flag) *, void (*)(void));

__SPRT_END_DECL

#endif // CORE_RUNTIME_INCLUDE_C___SPRT_THREADS_H_
