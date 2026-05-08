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

#ifndef CORE_RUNTIME_INCLUDE_LIBC_PTHREAD_H_
#define CORE_RUNTIME_INCLUDE_LIBC_PTHREAD_H_

#if defined(__SPRT_BUILD) && __STDC_HOSTED__ == 1

#include_next <pthread.h>

#else

#include <sprt/c/__sprt_pthread.h>

#include <time.h>
#include <sched.h>

#define PTHREAD_CREATE_JOINABLE __SPRT_PTHREAD_CREATE_JOINABLE
#define PTHREAD_CREATE_DETACHED __SPRT_PTHREAD_CREATE_DETACHED

#define PTHREAD_MUTEX_NORMAL __SPRT_PTHREAD_MUTEX_NORMAL
#define PTHREAD_MUTEX_DEFAULT __SPRT_PTHREAD_MUTEX_DEFAULT
#define PTHREAD_MUTEX_RECURSIVE __SPRT_PTHREAD_MUTEX_RECURSIVE
#define PTHREAD_MUTEX_ERRORCHECK __SPRT_PTHREAD_MUTEX_ERRORCHECK

#define PTHREAD_MUTEX_STALLED __SPRT_PTHREAD_MUTEX_STALLED
#define PTHREAD_MUTEX_ROBUST __SPRT_PTHREAD_MUTEX_ROBUST

#define PTHREAD_PRIO_NONE __SPRT_PTHREAD_PRIO_NONE
#define PTHREAD_PRIO_INHERIT __SPRT_PTHREAD_PRIO_INHERIT
#define PTHREAD_PRIO_PROTECT __SPRT_PTHREAD_PRIO_PROTECT

#define PTHREAD_INHERIT_SCHED __SPRT_PTHREAD_INHERIT_SCHED
#define PTHREAD_EXPLICIT_SCHED __SPRT_PTHREAD_EXPLICIT_SCHED

#define PTHREAD_SCOPE_SYSTEM __SPRT_PTHREAD_SCOPE_SYSTEM
#define PTHREAD_SCOPE_PROCESS __SPRT_PTHREAD_SCOPE_PROCESS

#define PTHREAD_PROCESS_PRIVATE __SPRT_PTHREAD_PROCESS_PRIVATE
#define PTHREAD_PROCESS_SHARED __SPRT_PTHREAD_PROCESS_SHARED

#define PTHREAD_MUTEX_INITIALIZER __SPRT_PTHREAD_MUTEX_INITIALIZER
#define PTHREAD_RWLOCK_INITIALIZER __SPRT_PTHREAD_RWLOCK_INITIALIZER
#define PTHREAD_COND_INITIALIZER __SPRT_PTHREAD_COND_INITIALIZER
#define PTHREAD_ONCE_INIT __SPRT_PTHREAD_ONCE_INIT

#define PTHREAD_CANCEL_ENABLE __SPRT_PTHREAD_CANCEL_ENABLE
#define PTHREAD_CANCEL_DISABLE __SPRT_PTHREAD_CANCEL_DISABLE
#define PTHREAD_CANCEL_MASKED __SPRT_PTHREAD_CANCEL_MASKED

#define PTHREAD_CANCEL_DEFERRED __SPRT_PTHREAD_CANCEL_DEFERRED
#define PTHREAD_CANCEL_ASYNCHRONOUS __SPRT_PTHREAD_CANCEL_ASYNCHRONOUS

#define PTHREAD_CANCELED __SPRT_PTHREAD_CANCELED

#define PTHREAD_BARRIER_SERIAL_THREAD __SPRT_PTHREAD_BARRIER_SERIAL_THREAD

#define PTHREAD_NULL __SPRT_PTHREAD_NULL

typedef __SPRT_ID(size_t) size_t;
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

__SPRT_BEGIN_DECL

SPRT_UMBRELLA_FUNC
int pthread_create(pthread_t *__SPRT_RESTRICT thread, const pthread_attr_t *__SPRT_RESTRICT attr,
		void *(*cb)(void *), void *__SPRT_RESTRICT arg) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_create(thread, attr, cb, arg);
}
#endif

SPRT_UMBRELLA_FUNC
int pthread_detach(pthread_t thread) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_detach(thread);
}
#endif

SPRT_UMBRELLA_FUNC
__SPRT_NORETURN void pthread_exit(void *ret) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	__sprt_pthread_exit(ret);
}
#endif

SPRT_UMBRELLA_FUNC
int pthread_join(pthread_t thread, void **ret) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_join(thread, ret);
}
#endif

SPRT_UMBRELLA_FUNC
pthread_t pthread_self(void) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_self();
}
#endif

SPRT_UMBRELLA_FUNC
int pthread_equal(pthread_t t1, pthread_t t2) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_equal(t1, t2);
}
#endif

SPRT_UMBRELLA_FUNC
int pthread_setcancelstate(int v, int *p) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_setcancelstate(v, p);
}
#endif
SPRT_UMBRELLA_FUNC
int pthread_setcanceltype(int v, int *p) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_setcanceltype(v, p);
}
#endif

SPRT_UMBRELLA_FUNC
void pthread_testcancel(void) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	__sprt_pthread_testcancel();
}
#endif

SPRT_UMBRELLA_FUNC
int pthread_cancel(pthread_t thread) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_cancel(thread);
}
#endif

SPRT_UMBRELLA_FUNC
int pthread_getschedparam(pthread_t thread, int *__SPRT_RESTRICT n,
		struct __SPRT_SCHED_PARAM_NAME *__SPRT_RESTRICT p) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_getschedparam(thread, n, p);
}
#endif

SPRT_UMBRELLA_FUNC
int pthread_setschedparam(pthread_t thread, int n,
		const struct __SPRT_SCHED_PARAM_NAME *p) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_setschedparam(thread, n, p);
}
#endif

SPRT_UMBRELLA_FUNC
int pthread_setschedprio(pthread_t thread, int p) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_setschedprio(thread, p);
}
#endif

SPRT_UMBRELLA_FUNC
int pthread_once(pthread_once_t *once, void (*cb)(void)) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_once(once, cb);
}
#endif

SPRT_UMBRELLA_FUNC
int pthread_mutex_init(pthread_mutex_t *__SPRT_RESTRICT mutex,
		const pthread_mutexattr_t *__SPRT_RESTRICT attr) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_mutex_init(mutex, attr);
}
#endif

SPRT_UMBRELLA_FUNC
int pthread_mutex_lock(pthread_mutex_t *mutex) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_mutex_lock(mutex);
}
#endif

SPRT_UMBRELLA_FUNC
int pthread_mutex_unlock(pthread_mutex_t *mutex) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_mutex_unlock(mutex);
}
#endif

SPRT_UMBRELLA_FUNC
int pthread_mutex_trylock(pthread_mutex_t *mutex) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_mutex_trylock(mutex);
}
#endif

SPRT_UMBRELLA_FUNC
int pthread_mutex_timedlock(pthread_mutex_t *__SPRT_RESTRICT mutex,
		const timespec *__SPRT_RESTRICT tv) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_mutex_timedlock(mutex, tv);
}
#endif

SPRT_UMBRELLA_FUNC
int pthread_mutex_destroy(pthread_mutex_t *mutex) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_mutex_destroy(mutex);
}
#endif

SPRT_UMBRELLA_FUNC
int pthread_mutex_consistent(pthread_mutex_t *mutex) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_mutex_consistent(mutex);
}
#endif

SPRT_UMBRELLA_FUNC
int pthread_mutex_getprioceiling(const pthread_mutex_t *__SPRT_RESTRICT mutex,
		int *__SPRT_RESTRICT p) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_mutex_getprioceiling(mutex, p);
}
#endif
SPRT_UMBRELLA_FUNC
int pthread_mutex_setprioceiling(pthread_mutex_t *__SPRT_RESTRICT mutex, int v,
		int *__SPRT_RESTRICT p) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_mutex_setprioceiling(mutex, v, p);
}
#endif

SPRT_UMBRELLA_FUNC
int pthread_cond_init(pthread_cond_t *__SPRT_RESTRICT cond,
		const pthread_condattr_t *__SPRT_RESTRICT attr) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_cond_init(cond, attr);
}
#endif

SPRT_UMBRELLA_FUNC
int pthread_cond_destroy(pthread_cond_t *cond) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_cond_destroy(cond);
}
#endif

SPRT_UMBRELLA_FUNC
int pthread_cond_wait(pthread_cond_t *__SPRT_RESTRICT cond,
		pthread_mutex_t *__SPRT_RESTRICT mutex) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_cond_wait(cond, mutex);
}
#endif

SPRT_UMBRELLA_FUNC
int pthread_cond_timedwait(pthread_cond_t *__SPRT_RESTRICT cond,
		pthread_mutex_t *__SPRT_RESTRICT mutex,
		const timespec *__SPRT_RESTRICT tv) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_cond_timedwait(cond, mutex, tv);
}
#endif

SPRT_UMBRELLA_FUNC
int pthread_cond_broadcast(pthread_cond_t *cond) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_cond_broadcast(cond);
}
#endif
SPRT_UMBRELLA_FUNC
int pthread_cond_signal(pthread_cond_t *cond) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_cond_signal(cond);
}
#endif

SPRT_UMBRELLA_FUNC
int pthread_rwlock_init(pthread_rwlock_t *__SPRT_RESTRICT lock,
		const pthread_rwlockattr_t *__SPRT_RESTRICT attr) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_rwlock_init(lock, attr);
}
#endif

SPRT_UMBRELLA_FUNC
int pthread_rwlock_destroy(pthread_rwlock_t *lock) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_rwlock_destroy(lock);
}
#endif

SPRT_UMBRELLA_FUNC
int pthread_rwlock_rdlock(pthread_rwlock_t *lock) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_rwlock_rdlock(lock);
}
#endif
SPRT_UMBRELLA_FUNC
int pthread_rwlock_tryrdlock(pthread_rwlock_t *lock) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_rwlock_tryrdlock(lock);
}
#endif

SPRT_UMBRELLA_FUNC
int pthread_rwlock_timedrdlock(pthread_rwlock_t *__SPRT_RESTRICT lock,
		const timespec *__SPRT_RESTRICT tv) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_rwlock_timedrdlock(lock, tv);
}
#endif

SPRT_UMBRELLA_FUNC
int pthread_rwlock_wrlock(pthread_rwlock_t *lock) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_rwlock_wrlock(lock);
}
#endif
SPRT_UMBRELLA_FUNC
int pthread_rwlock_trywrlock(pthread_rwlock_t *lock) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_rwlock_trywrlock(lock);
}
#endif

SPRT_UMBRELLA_FUNC
int pthread_rwlock_timedwrlock(pthread_rwlock_t *__SPRT_RESTRICT lock,
		const timespec *__SPRT_RESTRICT tv) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_rwlock_timedwrlock(lock, tv);
}
#endif

SPRT_UMBRELLA_FUNC
int pthread_rwlock_unlock(pthread_rwlock_t *lock) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_rwlock_unlock(lock);
}
#endif

SPRT_UMBRELLA_FUNC
int pthread_spin_init(pthread_spinlock_t *spin, int v) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_spin_init(spin, v);
}
#endif
SPRT_UMBRELLA_FUNC
int pthread_spin_destroy(pthread_spinlock_t *spin) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_spin_destroy(spin);
}
#endif
SPRT_UMBRELLA_FUNC
int pthread_spin_lock(pthread_spinlock_t *spin) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_spin_lock(spin);
}
#endif
SPRT_UMBRELLA_FUNC
int pthread_spin_trylock(pthread_spinlock_t *spin) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_spin_trylock(spin);
}
#endif
SPRT_UMBRELLA_FUNC
int pthread_spin_unlock(pthread_spinlock_t *spin) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_spin_unlock(spin);
}
#endif

SPRT_UMBRELLA_FUNC
int pthread_barrier_init(pthread_barrier_t *__SPRT_RESTRICT bar,
		const pthread_barrierattr_t *__SPRT_RESTRICT attr, unsigned v) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_barrier_init(bar, attr, v);
}
#endif
SPRT_UMBRELLA_FUNC
int pthread_barrier_destroy(pthread_barrier_t *bar) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_barrier_destroy(bar);
}
#endif
SPRT_UMBRELLA_FUNC
int pthread_barrier_wait(pthread_barrier_t *bar) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_barrier_wait(bar);
}
#endif

SPRT_UMBRELLA_FUNC
int pthread_key_create(pthread_key_t *key, void (*cb)(void *)) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_key_create(key, cb);
}
#endif
SPRT_UMBRELLA_FUNC
int pthread_key_delete(pthread_key_t key) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_key_delete(key);
}
#endif
SPRT_UMBRELLA_FUNC
void *pthread_getspecific(pthread_key_t key) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_getspecific(key);
}
#endif
SPRT_UMBRELLA_FUNC
int pthread_setspecific(pthread_key_t key, const void *val) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_setspecific(key, val);
}
#endif

SPRT_UMBRELLA_FUNC
int pthread_attr_init(pthread_attr_t *attr) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_attr_init(attr);
}
#endif
SPRT_UMBRELLA_FUNC
int pthread_attr_destroy(pthread_attr_t *attr) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_attr_destroy(attr);
}
#endif

SPRT_UMBRELLA_FUNC
int pthread_attr_getguardsize(const pthread_attr_t *__SPRT_RESTRICT attr,
		size_t *__SPRT_RESTRICT ret) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_attr_getguardsize(attr, ret);
}
#endif
SPRT_UMBRELLA_FUNC
int pthread_attr_setguardsize(pthread_attr_t *attr, size_t v) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_attr_setguardsize(attr, v);
}
#endif
SPRT_UMBRELLA_FUNC
int pthread_attr_getstacksize(const pthread_attr_t *__SPRT_RESTRICT attr,
		size_t *__SPRT_RESTRICT ret) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_attr_getstacksize(attr, ret);
}
#endif
SPRT_UMBRELLA_FUNC
int pthread_attr_setstacksize(pthread_attr_t *attr, size_t v) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_attr_setstacksize(attr, v);
}
#endif
SPRT_UMBRELLA_FUNC
int pthread_attr_getdetachstate(const pthread_attr_t *attr, int *ret) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_attr_getdetachstate(attr, ret);
}
#endif
SPRT_UMBRELLA_FUNC
int pthread_attr_setdetachstate(pthread_attr_t *attr, int v) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_attr_setdetachstate(attr, v);
}
#endif
SPRT_UMBRELLA_FUNC
int pthread_attr_getstack(const pthread_attr_t *__SPRT_RESTRICT attr, void **__SPRT_RESTRICT ret,
		size_t *__SPRT_RESTRICT sz) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_attr_getstack(attr, ret, sz);
}
#endif
SPRT_UMBRELLA_FUNC
int pthread_attr_setstack(pthread_attr_t *attr, void *ptr, size_t sz) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_attr_setstack(attr, ptr, sz);
}
#endif
SPRT_UMBRELLA_FUNC
int pthread_attr_getscope(const pthread_attr_t *__SPRT_RESTRICT attr,
		int *__SPRT_RESTRICT ret) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_attr_getscope(attr, ret);
}
#endif
SPRT_UMBRELLA_FUNC
int pthread_attr_setscope(pthread_attr_t *attr, int v) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_attr_setscope(attr, v);
}
#endif
SPRT_UMBRELLA_FUNC
int pthread_attr_getschedpolicy(const pthread_attr_t *__SPRT_RESTRICT attr,
		int *__SPRT_RESTRICT ret) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_attr_getschedpolicy(attr, ret);
}
#endif
SPRT_UMBRELLA_FUNC
int pthread_attr_setschedpolicy(pthread_attr_t *attr, int v) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_attr_setschedpolicy(attr, v);
}
#endif
SPRT_UMBRELLA_FUNC
int pthread_attr_getschedparam(const pthread_attr_t *__SPRT_RESTRICT attr,
		struct __SPRT_SCHED_PARAM_NAME *__SPRT_RESTRICT ret) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_attr_getschedparam(attr, ret);
}
#endif
SPRT_UMBRELLA_FUNC
int pthread_attr_setschedparam(pthread_attr_t *__SPRT_RESTRICT attr,
		const struct __SPRT_SCHED_PARAM_NAME *__SPRT_RESTRICT val) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_attr_setschedparam(attr, val);
}
#endif

SPRT_UMBRELLA_FUNC
int pthread_attr_getinheritsched(const pthread_attr_t *__SPRT_RESTRICT attr,
		int *__SPRT_RESTRICT ret) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_attr_getinheritsched(attr, ret);
}
#endif
SPRT_UMBRELLA_FUNC
int pthread_attr_setinheritsched(pthread_attr_t *attr, int v) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_attr_setinheritsched(attr, v);
}
#endif

SPRT_UMBRELLA_FUNC
int pthread_mutexattr_destroy(pthread_mutexattr_t *attr) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_mutexattr_destroy(attr);
}
#endif

SPRT_UMBRELLA_FUNC
int pthread_mutexattr_getprioceiling(const pthread_mutexattr_t *__SPRT_RESTRICT attr,
		int *__SPRT_RESTRICT ret) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_mutexattr_getprioceiling(attr, ret);
}
#endif
SPRT_UMBRELLA_FUNC
int pthread_mutexattr_setprioceiling(pthread_mutexattr_t *attr, int v) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_mutexattr_setprioceiling(attr, v);
}
#endif
SPRT_UMBRELLA_FUNC
int pthread_mutexattr_getprotocol(const pthread_mutexattr_t *__SPRT_RESTRICT attr,
		int *__SPRT_RESTRICT ret) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_mutexattr_getprotocol(attr, ret);
}
#endif
SPRT_UMBRELLA_FUNC
int pthread_mutexattr_setprotocol(pthread_mutexattr_t *attr, int v) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_mutexattr_setprotocol(attr, v);
}
#endif

SPRT_UMBRELLA_FUNC
int pthread_mutexattr_getrobust(const pthread_mutexattr_t *__SPRT_RESTRICT attr,
		int *__SPRT_RESTRICT ret) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_mutexattr_getrobust(attr, ret);
}
#endif
SPRT_UMBRELLA_FUNC
int pthread_mutexattr_setrobust(pthread_mutexattr_t *attr, int v) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_mutexattr_setrobust(attr, v);
}
#endif

SPRT_UMBRELLA_FUNC
int pthread_mutexattr_getpshared(const pthread_mutexattr_t *__SPRT_RESTRICT attr,
		int *__SPRT_RESTRICT ret) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_mutexattr_getpshared(attr, ret);
}
#endif
SPRT_UMBRELLA_FUNC
int pthread_mutexattr_gettype(const pthread_mutexattr_t *__SPRT_RESTRICT attr,
		int *__SPRT_RESTRICT ret) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_mutexattr_gettype(attr, ret);
}
#endif
SPRT_UMBRELLA_FUNC
int pthread_mutexattr_init(pthread_mutexattr_t *attr) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_mutexattr_init(attr);
}
#endif
SPRT_UMBRELLA_FUNC
int pthread_mutexattr_setpshared(pthread_mutexattr_t *attr, int v) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_mutexattr_setpshared(attr, v);
}
#endif
SPRT_UMBRELLA_FUNC
int pthread_mutexattr_settype(pthread_mutexattr_t *attr, int v) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_mutexattr_settype(attr, v);
}
#endif

SPRT_UMBRELLA_FUNC
int pthread_condattr_init(pthread_condattr_t *attr) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_condattr_init(attr);
}
#endif
SPRT_UMBRELLA_FUNC
int pthread_condattr_destroy(pthread_condattr_t *attr) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_condattr_destroy(attr);
}
#endif

SPRT_UMBRELLA_FUNC
int pthread_condattr_setclock(pthread_condattr_t *attr, clockid_t clock) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_condattr_setclock(attr, clock);
}
#endif
SPRT_UMBRELLA_FUNC
int pthread_condattr_getclock(const pthread_condattr_t *__SPRT_RESTRICT attr,
		clockid_t *__SPRT_RESTRICT clock) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_condattr_getclock(attr, clock);
}
#endif
SPRT_UMBRELLA_FUNC
int pthread_getcpuclockid(pthread_t thread, clockid_t *clock) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_getcpuclockid(thread, clock);
}
#endif

SPRT_UMBRELLA_FUNC
int pthread_condattr_setpshared(pthread_condattr_t *attr, int v) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_condattr_setpshared(attr, v);
}
#endif
SPRT_UMBRELLA_FUNC
int pthread_condattr_getpshared(const pthread_condattr_t *__SPRT_RESTRICT attr,
		int *__SPRT_RESTRICT ret) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_condattr_getpshared(attr, ret);
}
#endif

SPRT_UMBRELLA_FUNC
int pthread_rwlockattr_init(pthread_rwlockattr_t *attr) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_rwlockattr_init(attr);
}
#endif
SPRT_UMBRELLA_FUNC
int pthread_rwlockattr_destroy(pthread_rwlockattr_t *attr) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_rwlockattr_destroy(attr);
}
#endif
SPRT_UMBRELLA_FUNC
int pthread_rwlockattr_setpshared(pthread_rwlockattr_t *attr, int v) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_rwlockattr_setpshared(attr, v);
}
#endif
SPRT_UMBRELLA_FUNC
int pthread_rwlockattr_getpshared(const pthread_rwlockattr_t *__SPRT_RESTRICT attr,
		int *__SPRT_RESTRICT ret) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_rwlockattr_getpshared(attr, ret);
}
#endif

SPRT_UMBRELLA_FUNC
int pthread_barrierattr_destroy(pthread_barrierattr_t *attr) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_barrierattr_destroy(attr);
}
#endif
SPRT_UMBRELLA_FUNC
int pthread_barrierattr_getpshared(const pthread_barrierattr_t *__SPRT_RESTRICT attr,
		int *__SPRT_RESTRICT ret) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_barrierattr_getpshared(attr, ret);
}
#endif
SPRT_UMBRELLA_FUNC
int pthread_barrierattr_init(pthread_barrierattr_t *attr) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_barrierattr_init(attr);
}
#endif
SPRT_UMBRELLA_FUNC
int pthread_barrierattr_setpshared(pthread_barrierattr_t *attr, int v) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_barrierattr_setpshared(attr, v);
}
#endif

SPRT_UMBRELLA_FUNC
int pthread_atfork(void (*prepare)(void), void (*parent)(void),
		void (*child)(void)) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_atfork(prepare, parent, child);
}
#endif

SPRT_UMBRELLA_FUNC
int pthread_getconcurrency(void) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_getconcurrency();
}
#endif
SPRT_UMBRELLA_FUNC
int pthread_setconcurrency(int v) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_setconcurrency(v);
}
#endif

#if __SPRT_CONFIG_HAVE_PTHREAD_AFFINITY || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS
SPRT_UMBRELLA_FUNC
int pthread_getaffinity_np(pthread_t thread, size_t n, cpu_set_t *set) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_getaffinity_np(thread, n, set);
}
#endif
SPRT_UMBRELLA_FUNC
int pthread_setaffinity_np(pthread_t thread, size_t n, const cpu_set_t *set) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_setaffinity_np(thread, n, set);
}
#endif
#endif

SPRT_UMBRELLA_FUNC
int pthread_getattr_np(pthread_t thread, pthread_attr_t *attr) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_getattr_np(thread, attr);
}
#endif

SPRT_UMBRELLA_FUNC
int pthread_setname_np(pthread_t thread, const char *name) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_setname_np(thread, name);
}
#endif

SPRT_UMBRELLA_FUNC
int pthread_getname_np(pthread_t thread, char *buf, size_t len) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_getname_np(thread, buf, len);
}
#endif

SPRT_UMBRELLA_FUNC
int pthread_getattr_default_np(pthread_attr_t *attr) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_getattr_default_np(attr);
}
#endif
SPRT_UMBRELLA_FUNC
int pthread_setattr_default_np(const pthread_attr_t *attr) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_setattr_default_np(attr);
}
#endif

SPRT_UMBRELLA_FUNC
int pthread_tryjoin_np(pthread_t thread, void **ret) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_tryjoin_np(thread, ret);
}
#endif

SPRT_UMBRELLA_FUNC
int pthread_timedjoin_np(pthread_t thread, void **ret, const timespec *tv) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_pthread_timedjoin_np(thread, ret, tv);
}
#endif

__SPRT_END_DECL

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_PTHREAD_H_
