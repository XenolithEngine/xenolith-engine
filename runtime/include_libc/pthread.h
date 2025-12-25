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

#ifdef __SPRT_BUILD

#include_next <pthread.h>

#else

#include <c/__sprt_pthread.h>

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

SPRT_FORCEINLINE inline int pthread_create(pthread_t *__SPRT_RESTRICT thread,
		const pthread_attr_t *__SPRT_RESTRICT attr, void *(*cb)(void *),
		void *__SPRT_RESTRICT arg) {
	return __sprt_pthread_create(thread, attr, cb, arg);
}

SPRT_FORCEINLINE inline int pthread_detach(pthread_t thread) {
	return __sprt_pthread_detach(thread);
}

SPRT_FORCEINLINE inline __SPRT_NORETURN void pthread_exit(void *ret) { __sprt_pthread_exit(ret); }

SPRT_FORCEINLINE inline int pthread_join(pthread_t thread, void **ret) {
	return __sprt_pthread_join(thread, ret);
}

SPRT_FORCEINLINE inline pthread_t pthread_self(void) { return __sprt_pthread_self(); }

SPRT_FORCEINLINE inline int pthread_equal(pthread_t t1, pthread_t t2) {
	return __sprt_pthread_equal(t1, t2);
}

SPRT_FORCEINLINE inline int pthread_setcancelstate(int v, int *p) {
	return __sprt_pthread_setcancelstate(v, p);
}
SPRT_FORCEINLINE inline int pthread_setcanceltype(int v, int *p) {
	return __sprt_pthread_setcanceltype(v, p);
}

SPRT_FORCEINLINE inline void pthread_testcancel(void) { __sprt_pthread_testcancel(); }

SPRT_FORCEINLINE inline int pthread_cancel(pthread_t thread) {
	return __sprt_pthread_cancel(thread);
}

SPRT_FORCEINLINE inline int pthread_getschedparam(pthread_t thread, int *__SPRT_RESTRICT n,
		sched_param *__SPRT_RESTRICT p) {
	return __sprt_pthread_getschedparam(thread, n, p);
}

SPRT_FORCEINLINE inline int pthread_setschedparam(pthread_t thread, int n, const sched_param *p) {
	return __sprt_pthread_setschedparam(thread, n, p);
}

SPRT_FORCEINLINE inline int pthread_setschedprio(pthread_t thread, int p) {
	return __sprt_pthread_setschedprio(thread, p);
}

SPRT_FORCEINLINE inline int pthread_once(pthread_once_t *once, void (*cb)(void)) {
	return __sprt_pthread_once(once, cb);
}

SPRT_FORCEINLINE inline int pthread_mutex_init(pthread_mutex_t *__SPRT_RESTRICT mutex,
		const pthread_mutexattr_t *__SPRT_RESTRICT attr) {
	return __sprt_pthread_mutex_init(mutex, attr);
}

SPRT_FORCEINLINE inline int pthread_mutex_lock(pthread_mutex_t *mutex) {
	return __sprt_pthread_mutex_lock(mutex);
}

SPRT_FORCEINLINE inline int pthread_mutex_unlock(pthread_mutex_t *mutex) {
	return __sprt_pthread_mutex_unlock(mutex);
}

SPRT_FORCEINLINE inline int pthread_mutex_trylock(pthread_mutex_t *mutex) {
	return __sprt_pthread_mutex_trylock(mutex);
}

SPRT_FORCEINLINE inline int pthread_mutex_timedlock(pthread_mutex_t *__SPRT_RESTRICT mutex,
		const timespec *__SPRT_RESTRICT tv) {
	return __sprt_pthread_mutex_timedlock(mutex, tv);
}
SPRT_FORCEINLINE inline int pthread_mutex_destroy(pthread_mutex_t *mutex) {
	return __sprt_pthread_mutex_destroy(mutex);
}
SPRT_FORCEINLINE inline int pthread_mutex_consistent(pthread_mutex_t *mutex) {
	return __sprt_pthread_mutex_consistent(mutex);
}

SPRT_FORCEINLINE inline int pthread_mutex_getprioceiling(
		const pthread_mutex_t *__SPRT_RESTRICT mutex, int *__SPRT_RESTRICT p) {
	return __sprt_pthread_mutex_getprioceiling(mutex, p);
}
SPRT_FORCEINLINE inline int pthread_mutex_setprioceiling(pthread_mutex_t *__SPRT_RESTRICT mutex,
		int v, int *__SPRT_RESTRICT p) {
	return __sprt_pthread_mutex_setprioceiling(mutex, v, p);
}

SPRT_FORCEINLINE inline int pthread_cond_init(pthread_cond_t *__SPRT_RESTRICT cond,
		const pthread_condattr_t *__SPRT_RESTRICT attr) {
	return __sprt_pthread_cond_init(cond, attr);
}

SPRT_FORCEINLINE inline int pthread_cond_destroy(pthread_cond_t *cond) {
	return __sprt_pthread_cond_destroy(cond);
}

SPRT_FORCEINLINE inline int pthread_cond_wait(pthread_cond_t *__SPRT_RESTRICT cond,
		pthread_mutex_t *__SPRT_RESTRICT mutex) {
	return __sprt_pthread_cond_wait(cond, mutex);
}

SPRT_FORCEINLINE inline int pthread_cond_timedwait(pthread_cond_t *__SPRT_RESTRICT cond,
		pthread_mutex_t *__SPRT_RESTRICT mutex, const timespec *__SPRT_RESTRICT tv) {
	return __sprt_pthread_cond_timedwait(cond, mutex, tv);
}

SPRT_FORCEINLINE inline int pthread_cond_broadcast(pthread_cond_t *cond) {
	return __sprt_pthread_cond_broadcast(cond);
}
SPRT_FORCEINLINE inline int pthread_cond_signal(pthread_cond_t *cond) {
	return __sprt_pthread_cond_signal(cond);
}

SPRT_FORCEINLINE inline int pthread_rwlock_init(pthread_rwlock_t *__SPRT_RESTRICT lock,
		const pthread_rwlockattr_t *__SPRT_RESTRICT attr) {
	return __sprt_pthread_rwlock_init(lock, attr);
}

SPRT_FORCEINLINE inline int pthread_rwlock_destroy(pthread_rwlock_t *lock) {
	return __sprt_pthread_rwlock_destroy(lock);
}

SPRT_FORCEINLINE inline int pthread_rwlock_rdlock(pthread_rwlock_t *lock) {
	return __sprt_pthread_rwlock_rdlock(lock);
}
SPRT_FORCEINLINE inline int pthread_rwlock_tryrdlock(pthread_rwlock_t *lock) {
	return __sprt_pthread_rwlock_tryrdlock(lock);
}
SPRT_FORCEINLINE inline int pthread_rwlock_timedrdlock(pthread_rwlock_t *__SPRT_RESTRICT lock,
		const timespec *__SPRT_RESTRICT tv) {
	return __sprt_pthread_rwlock_timedrdlock(lock, tv);
}

SPRT_FORCEINLINE inline int pthread_rwlock_wrlock(pthread_rwlock_t *lock) {
	return __sprt_pthread_rwlock_wrlock(lock);
}
SPRT_FORCEINLINE inline int pthread_rwlock_trywrlock(pthread_rwlock_t *lock) {
	return __sprt_pthread_rwlock_trywrlock(lock);
}
SPRT_FORCEINLINE inline int pthread_rwlock_timedwrlock(pthread_rwlock_t *__SPRT_RESTRICT lock,
		const timespec *__SPRT_RESTRICT tv) {
	return __sprt_pthread_rwlock_timedwrlock(lock, tv);
}
SPRT_FORCEINLINE inline int pthread_rwlock_unlock(pthread_rwlock_t *lock) {
	return __sprt_pthread_rwlock_unlock(lock);
}

SPRT_FORCEINLINE inline int pthread_spin_init(pthread_spinlock_t *spin, int v) {
	return __sprt_pthread_spin_init(spin, v);
}
SPRT_FORCEINLINE inline int pthread_spin_destroy(pthread_spinlock_t *spin) {
	return __sprt_pthread_spin_destroy(spin);
}
SPRT_FORCEINLINE inline int pthread_spin_lock(pthread_spinlock_t *spin) {
	return __sprt_pthread_spin_lock(spin);
}
SPRT_FORCEINLINE inline int pthread_spin_trylock(pthread_spinlock_t *spin) {
	return __sprt_pthread_spin_trylock(spin);
}
SPRT_FORCEINLINE inline int pthread_spin_unlock(pthread_spinlock_t *spin) {
	return __sprt_pthread_spin_unlock(spin);
}

SPRT_FORCEINLINE inline int pthread_barrier_init(pthread_barrier_t *__SPRT_RESTRICT bar,
		const pthread_barrierattr_t *__SPRT_RESTRICT attr, unsigned v) {
	return __sprt_pthread_barrier_init(bar, attr, v);
}
SPRT_FORCEINLINE inline int pthread_barrier_destroy(pthread_barrier_t *bar) {
	return __sprt_pthread_barrier_destroy(bar);
}
SPRT_FORCEINLINE inline int pthread_barrier_wait(pthread_barrier_t *bar) {
	return __sprt_pthread_barrier_wait(bar);
}

SPRT_FORCEINLINE inline int pthread_key_create(pthread_key_t *key, void (*cb)(void *)) {
	return __sprt_pthread_key_create(key, cb);
}
SPRT_FORCEINLINE inline int pthread_key_delete(pthread_key_t key) {
	return __sprt_pthread_key_delete(key);
}
SPRT_FORCEINLINE inline void *pthread_getspecific(pthread_key_t key) {
	return __sprt_pthread_getspecific(key);
}
SPRT_FORCEINLINE inline int pthread_setspecific(pthread_key_t key, const void *val) {
	return __sprt_pthread_setspecific(key, val);
}

SPRT_FORCEINLINE inline int pthread_attr_init(pthread_attr_t *attr) {
	return __sprt_pthread_attr_init(attr);
}
SPRT_FORCEINLINE inline int pthread_attr_destroy(pthread_attr_t *attr) {
	return __sprt_pthread_attr_destroy(attr);
}

SPRT_FORCEINLINE inline int pthread_attr_getguardsize(const pthread_attr_t *__SPRT_RESTRICT attr,
		size_t *__SPRT_RESTRICT ret) {
	return __sprt_pthread_attr_getguardsize(attr, ret);
}
SPRT_FORCEINLINE inline int pthread_attr_setguardsize(pthread_attr_t *attr, size_t v) {
	return __sprt_pthread_attr_setguardsize(attr, v);
}
SPRT_FORCEINLINE inline int pthread_attr_getstacksize(const pthread_attr_t *__SPRT_RESTRICT attr,
		size_t *__SPRT_RESTRICT ret) {
	return __sprt_pthread_attr_getstacksize(attr, ret);
}
SPRT_FORCEINLINE inline int pthread_attr_setstacksize(pthread_attr_t *attr, size_t v) {
	return __sprt_pthread_attr_setstacksize(attr, v);
}
SPRT_FORCEINLINE inline int pthread_attr_getdetachstate(const pthread_attr_t *attr, int *ret) {
	return __sprt_pthread_attr_getdetachstate(attr, ret);
}
SPRT_FORCEINLINE inline int pthread_attr_setdetachstate(pthread_attr_t *attr, int v) {
	return __sprt_pthread_attr_setdetachstate(attr, v);
}
SPRT_FORCEINLINE inline int pthread_attr_getstack(const pthread_attr_t *__SPRT_RESTRICT attr,
		void **__SPRT_RESTRICT ret, size_t *__SPRT_RESTRICT sz) {
	return __sprt_pthread_attr_getstack(attr, ret, sz);
}
SPRT_FORCEINLINE inline int pthread_attr_setstack(pthread_attr_t *attr, void *ptr, size_t sz) {
	return __sprt_pthread_attr_setstack(attr, ptr, sz);
}
SPRT_FORCEINLINE inline int pthread_attr_getscope(const pthread_attr_t *__SPRT_RESTRICT attr,
		int *__SPRT_RESTRICT ret) {
	return __sprt_pthread_attr_getscope(attr, ret);
}
SPRT_FORCEINLINE inline int pthread_attr_setscope(pthread_attr_t *attr, int v) {
	return __sprt_pthread_attr_setscope(attr, v);
}
SPRT_FORCEINLINE inline int pthread_attr_getschedpolicy(const pthread_attr_t *__SPRT_RESTRICT attr,
		int *__SPRT_RESTRICT ret) {
	return __sprt_pthread_attr_getschedpolicy(attr, ret);
}
SPRT_FORCEINLINE inline int pthread_attr_setschedpolicy(pthread_attr_t *attr, int v) {
	return __sprt_pthread_attr_setschedpolicy(attr, v);
}
SPRT_FORCEINLINE inline int pthread_attr_getschedparam(const pthread_attr_t *__SPRT_RESTRICT attr,
		sched_param *__SPRT_RESTRICT ret) {
	return __sprt_pthread_attr_getschedparam(attr, ret);
}
SPRT_FORCEINLINE inline int pthread_attr_setschedparam(pthread_attr_t *__SPRT_RESTRICT attr,
		const sched_param *__SPRT_RESTRICT val) {
	return __sprt_pthread_attr_setschedparam(attr, val);
}
SPRT_FORCEINLINE inline int pthread_attr_getinheritsched(const pthread_attr_t *__SPRT_RESTRICT attr,
		int *__SPRT_RESTRICT ret) {
	return __sprt_pthread_attr_getinheritsched(attr, ret);
}
SPRT_FORCEINLINE inline int pthread_attr_setinheritsched(pthread_attr_t *attr, int v) {
	return __sprt_pthread_attr_setinheritsched(attr, v);
}

SPRT_FORCEINLINE inline int pthread_mutexattr_destroy(pthread_mutexattr_t *attr) {
	return __sprt_pthread_mutexattr_destroy(attr);
}
SPRT_FORCEINLINE inline int pthread_mutexattr_getprioceiling(
		const pthread_mutexattr_t *__SPRT_RESTRICT attr, int *__SPRT_RESTRICT ret) {
	return __sprt_pthread_mutexattr_getprioceiling(attr, ret);
}
SPRT_FORCEINLINE inline int pthread_mutexattr_getprotocol(
		const pthread_mutexattr_t *__SPRT_RESTRICT attr, int *__SPRT_RESTRICT ret) {
	return __sprt_pthread_mutexattr_getprotocol(attr, ret);
}
SPRT_FORCEINLINE inline int pthread_mutexattr_getpshared(
		const pthread_mutexattr_t *__SPRT_RESTRICT attr, int *__SPRT_RESTRICT ret) {
	return __sprt_pthread_mutexattr_getpshared(attr, ret);
}
SPRT_FORCEINLINE inline int pthread_mutexattr_getrobust(
		const pthread_mutexattr_t *__SPRT_RESTRICT attr, int *__SPRT_RESTRICT ret) {
	return __sprt_pthread_mutexattr_getrobust(attr, ret);
}
SPRT_FORCEINLINE inline int pthread_mutexattr_gettype(
		const pthread_mutexattr_t *__SPRT_RESTRICT attr, int *__SPRT_RESTRICT ret) {
	return __sprt_pthread_mutexattr_gettype(attr, ret);
}
SPRT_FORCEINLINE inline int pthread_mutexattr_init(pthread_mutexattr_t *attr) {
	return __sprt_pthread_mutexattr_init(attr);
}
SPRT_FORCEINLINE inline int pthread_mutexattr_setprioceiling(pthread_mutexattr_t *attr, int v) {
	return __sprt_pthread_mutexattr_setprioceiling(attr, v);
}
SPRT_FORCEINLINE inline int pthread_mutexattr_setprotocol(pthread_mutexattr_t *attr, int v) {
	return __sprt_pthread_mutexattr_setprotocol(attr, v);
}
SPRT_FORCEINLINE inline int pthread_mutexattr_setpshared(pthread_mutexattr_t *attr, int v) {
	return __sprt_pthread_mutexattr_setpshared(attr, v);
}
SPRT_FORCEINLINE inline int pthread_mutexattr_setrobust(pthread_mutexattr_t *attr, int v) {
	return __sprt_pthread_mutexattr_setrobust(attr, v);
}
SPRT_FORCEINLINE inline int pthread_mutexattr_settype(pthread_mutexattr_t *attr, int v) {
	return __sprt_pthread_mutexattr_settype(attr, v);
}

SPRT_FORCEINLINE inline int pthread_condattr_init(pthread_condattr_t *attr) {
	return __sprt_pthread_condattr_init(attr);
}
SPRT_FORCEINLINE inline int pthread_condattr_destroy(pthread_condattr_t *attr) {
	return __sprt_pthread_condattr_destroy(attr);
}
SPRT_FORCEINLINE inline int pthread_condattr_setclock(pthread_condattr_t *attr, clockid_t clock) {
	return __sprt_pthread_condattr_setclock(attr, clock);
}
SPRT_FORCEINLINE inline int pthread_condattr_setpshared(pthread_condattr_t *attr, int v) {
	return __sprt_pthread_condattr_setpshared(attr, v);
}
SPRT_FORCEINLINE inline int pthread_condattr_getclock(
		const pthread_condattr_t *__SPRT_RESTRICT attr, clockid_t *__SPRT_RESTRICT clock) {
	return __sprt_pthread_condattr_getclock(attr, clock);
}
SPRT_FORCEINLINE inline int pthread_condattr_getpshared(
		const pthread_condattr_t *__SPRT_RESTRICT attr, int *__SPRT_RESTRICT ret) {
	return __sprt_pthread_condattr_getpshared(attr, ret);
}

SPRT_FORCEINLINE inline int pthread_rwlockattr_init(pthread_rwlockattr_t *attr) {
	return __sprt_pthread_rwlockattr_init(attr);
}
SPRT_FORCEINLINE inline int pthread_rwlockattr_destroy(pthread_rwlockattr_t *attr) {
	return __sprt_pthread_rwlockattr_destroy(attr);
}
SPRT_FORCEINLINE inline int pthread_rwlockattr_setpshared(pthread_rwlockattr_t *attr, int v) {
	return __sprt_pthread_rwlockattr_setpshared(attr, v);
}
SPRT_FORCEINLINE inline int pthread_rwlockattr_getpshared(
		const pthread_rwlockattr_t *__SPRT_RESTRICT attr, int *__SPRT_RESTRICT ret) {
	return __sprt_pthread_rwlockattr_getpshared(attr, ret);
}

SPRT_FORCEINLINE inline int pthread_barrierattr_destroy(pthread_barrierattr_t *attr) {
	return __sprt_pthread_barrierattr_destroy(attr);
}
SPRT_FORCEINLINE inline int pthread_barrierattr_getpshared(
		const pthread_barrierattr_t *__SPRT_RESTRICT attr, int *__SPRT_RESTRICT ret) {
	return __sprt_pthread_barrierattr_getpshared(attr, ret);
}
SPRT_FORCEINLINE inline int pthread_barrierattr_init(pthread_barrierattr_t *attr) {
	return __sprt_pthread_barrierattr_init(attr);
}
SPRT_FORCEINLINE inline int pthread_barrierattr_setpshared(pthread_barrierattr_t *attr, int v) {
	return __sprt_pthread_barrierattr_setpshared(attr, v);
}

SPRT_FORCEINLINE inline int pthread_atfork(void (*prepare)(void), void (*parent)(void),
		void (*child)(void)) {
	return __sprt_pthread_atfork(prepare, parent, child);
}

SPRT_FORCEINLINE inline int pthread_getconcurrency(void) { return __sprt_pthread_getconcurrency(); }
SPRT_FORCEINLINE inline int pthread_setconcurrency(int v) {
	return __sprt_pthread_setconcurrency(v);
}

SPRT_FORCEINLINE inline int pthread_getcpuclockid(pthread_t thread, clockid_t *clock) {
	return __sprt_pthread_getcpuclockid(thread, clock);
}

SPRT_FORCEINLINE inline void pthread_cleanup_push(void (*cb)(void *), void *v) {
	__sprt_pthread_cleanup_push(cb, v);
}
SPRT_FORCEINLINE inline void pthread_cleanup_pop(int exec) { __sprt_pthread_cleanup_pop(exec); }

SPRT_FORCEINLINE inline int pthread_getaffinity_np(pthread_t thread, size_t n, cpu_set_t *set) {
	return __sprt_pthread_getaffinity_np(thread, n, set);
}
SPRT_FORCEINLINE inline int pthread_setaffinity_np(pthread_t thread, size_t n,
		const cpu_set_t *set) {
	return __sprt_pthread_setaffinity_np(thread, n, set);
}
SPRT_FORCEINLINE inline int pthread_getattr_np(pthread_t thread, pthread_attr_t *attr) {
	return __sprt_pthread_getattr_np(thread, attr);
}
SPRT_FORCEINLINE inline int pthread_setname_np(pthread_t thread, const char *name) {
	return __sprt_pthread_setname_np(thread, name);
}
SPRT_FORCEINLINE inline int pthread_getname_np(pthread_t thread, char *buf, size_t len) {
	return __sprt_pthread_getname_np(thread, buf, len);
}
SPRT_FORCEINLINE inline int pthread_getattr_default_np(pthread_attr_t *attr) {
	return __sprt_pthread_getattr_default_np(attr);
}
SPRT_FORCEINLINE inline int pthread_setattr_default_np(const pthread_attr_t *attr) {
	return __sprt_pthread_setattr_default_np(attr);
}
SPRT_FORCEINLINE inline int pthread_tryjoin_np(pthread_t thread, void **ret) {
	return __sprt_pthread_tryjoin_np(thread, ret);
}
SPRT_FORCEINLINE inline int pthread_timedjoin_np(pthread_t thread, void **ret, const timespec *tv) {
	return __sprt_pthread_timedjoin_np(thread, ret, tv);
}

__SPRT_END_DECL

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_PTHREAD_H_
