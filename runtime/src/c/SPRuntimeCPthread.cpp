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

#define __SPRT_BUILD 1

#include <c/__sprt_pthread.h>

#include <pthread.h>

namespace sprt {

__SPRT_C_FUNC int __SPRT_ID(pthread_create)(__SPRT_ID(pthread_t) * __SPRT_RESTRICT thread,
		const __SPRT_ID(pthread_attr_t) * __SPRT_RESTRICT attr, void *(*cb)(void *),
		void *__SPRT_RESTRICT arg) {
	return ::pthread_create(thread, (pthread_attr_t *)attr, cb, arg);
}

__SPRT_C_FUNC int __SPRT_ID(pthread_detach)(__SPRT_ID(pthread_t) thread) {
	return ::pthread_detach(thread);
}

__SPRT_C_FUNC __SPRT_NORETURN void __SPRT_ID(pthread_exit)(void *ret) { ::pthread_exit(ret); }

__SPRT_C_FUNC int __SPRT_ID(pthread_join)(__SPRT_ID(pthread_t) thread, void **ret) {
	return ::pthread_join(thread, ret);
}

__SPRT_C_FUNC __SPRT_ID(pthread_t) __SPRT_ID(pthread_self)(void) { return ::pthread_self(); }

__SPRT_C_FUNC int __SPRT_ID(pthread_equal)(__SPRT_ID(pthread_t) t1, __SPRT_ID(pthread_t) t2) {
	return ::pthread_equal(t1, t2);
}

__SPRT_C_FUNC int __SPRT_ID(pthread_setcancelstate)(int v, int *p) {
	return ::pthread_setcancelstate(v, p);
}
__SPRT_C_FUNC int __SPRT_ID(pthread_setcanceltype)(int v, int *p) {
	return ::pthread_setcanceltype(v, p);
}

__SPRT_C_FUNC void __SPRT_ID(pthread_testcancel)(void) { ::pthread_testcancel(); }

__SPRT_C_FUNC int __SPRT_ID(pthread_cancel)(__SPRT_ID(pthread_t) thread) {
	return ::pthread_cancel(thread);
}

__SPRT_C_FUNC int __SPRT_ID(pthread_getschedparam)(__SPRT_ID(pthread_t) thread,
		int *__SPRT_RESTRICT n, struct __SPRT_ID(sched_param) * __SPRT_RESTRICT p) {
	struct sched_param param;
	auto ret = ::pthread_getschedparam(thread, n, &param);

	if (p) {
		p->sched_priority = param.sched_priority;
	}
	return ret;
}

__SPRT_C_FUNC int __SPRT_ID(pthread_setschedparam)(__SPRT_ID(pthread_t) thread, int n,
		const struct __SPRT_ID(sched_param) * p) {

	struct sched_param param;
	if (p) {
		param.sched_priority = p->sched_priority;
	}
	return ::pthread_setschedparam(thread, n, p ? &param : nullptr);
}

__SPRT_C_FUNC int __SPRT_ID(pthread_setschedprio)(__SPRT_ID(pthread_t) thread, int p) {
	return ::pthread_setschedprio(thread, p);
}

__SPRT_C_FUNC int __SPRT_ID(pthread_once)(__SPRT_ID(pthread_once_t) * once, void (*cb)(void)) {
	return ::pthread_once(once, cb);
}

__SPRT_C_FUNC int __SPRT_ID(pthread_mutex_init)(__SPRT_ID(pthread_mutex_t) * __SPRT_RESTRICT mutex,
		const __SPRT_ID(pthread_mutexattr_t) * __SPRT_RESTRICT attr) {
	return ::pthread_mutex_init((pthread_mutex_t *)mutex, (pthread_mutexattr_t *)attr);
}

__SPRT_C_FUNC int __SPRT_ID(pthread_mutex_lock)(__SPRT_ID(pthread_mutex_t) * mutex) {
	return ::pthread_mutex_lock((pthread_mutex_t *)mutex);
}

__SPRT_C_FUNC int __SPRT_ID(pthread_mutex_unlock)(__SPRT_ID(pthread_mutex_t) * mutex) {
	return ::pthread_mutex_unlock((pthread_mutex_t *)mutex);
}

__SPRT_C_FUNC int __SPRT_ID(pthread_mutex_trylock)(__SPRT_ID(pthread_mutex_t) * mutex) {
	return ::pthread_mutex_trylock((pthread_mutex_t *)mutex);
}

__SPRT_C_FUNC int __SPRT_ID(
		pthread_mutex_timedlock)(__SPRT_ID(pthread_mutex_t) * __SPRT_RESTRICT mutex,
		const __SPRT_TIMESPEC_NAME *__SPRT_RESTRICT tv) {
	struct timespec native;
	if (tv) {
		native.tv_nsec = tv->tv_nsec;
		native.tv_sec = tv->tv_sec;
	}
	return ::pthread_mutex_timedlock((pthread_mutex_t *)mutex, tv ? &native : nullptr);
}
__SPRT_C_FUNC int __SPRT_ID(pthread_mutex_destroy)(__SPRT_ID(pthread_mutex_t) * mutex) {
	return ::pthread_mutex_destroy((pthread_mutex_t *)mutex);
}
__SPRT_C_FUNC int __SPRT_ID(pthread_mutex_consistent)(__SPRT_ID(pthread_mutex_t) * mutex) {
	return ::pthread_mutex_consistent((pthread_mutex_t *)mutex);
}

__SPRT_C_FUNC int __SPRT_ID(pthread_mutex_getprioceiling)(
		const __SPRT_ID(pthread_mutex_t) * __SPRT_RESTRICT mutex, int *__SPRT_RESTRICT p) {
	return ::pthread_mutex_getprioceiling((pthread_mutex_t *)mutex, p);
}
__SPRT_C_FUNC int __SPRT_ID(pthread_mutex_setprioceiling)(
		__SPRT_ID(pthread_mutex_t) * __SPRT_RESTRICT mutex, int v, int *__SPRT_RESTRICT p) {
	return ::pthread_mutex_setprioceiling((pthread_mutex_t *)mutex, v, p);
}

__SPRT_C_FUNC int __SPRT_ID(pthread_cond_init)(__SPRT_ID(pthread_cond_t) * __SPRT_RESTRICT cond,
		const __SPRT_ID(pthread_condattr_t) * __SPRT_RESTRICT attr) {
	return ::pthread_cond_init((pthread_cond_t *)cond, (pthread_condattr_t *)attr);
}

__SPRT_C_FUNC int __SPRT_ID(pthread_cond_destroy)(__SPRT_ID(pthread_cond_t) * cond) {
	return ::pthread_cond_destroy((pthread_cond_t *)cond);
}

__SPRT_C_FUNC int __SPRT_ID(pthread_cond_wait)(__SPRT_ID(pthread_cond_t) * __SPRT_RESTRICT cond,
		__SPRT_ID(pthread_mutex_t) * __SPRT_RESTRICT mutex) {
	return ::pthread_cond_wait((pthread_cond_t *)cond, (pthread_mutex_t *)mutex);
}

__SPRT_C_FUNC int __SPRT_ID(
		pthread_cond_timedwait)(__SPRT_ID(pthread_cond_t) * __SPRT_RESTRICT cond,
		__SPRT_ID(pthread_mutex_t) * __SPRT_RESTRICT mutex,
		const __SPRT_TIMESPEC_NAME *__SPRT_RESTRICT tv) {
	struct timespec native;
	if (tv) {
		native.tv_nsec = tv->tv_nsec;
		native.tv_sec = tv->tv_sec;
	}
	return ::pthread_cond_timedwait((pthread_cond_t *)cond, (pthread_mutex_t *)mutex,
			tv ? &native : nullptr);
}

__SPRT_C_FUNC int __SPRT_ID(pthread_cond_broadcast)(__SPRT_ID(pthread_cond_t) * cond) {
	return ::pthread_cond_broadcast((pthread_cond_t *)cond);
}
__SPRT_C_FUNC int __SPRT_ID(pthread_cond_signal)(__SPRT_ID(pthread_cond_t) * cond) {
	return ::pthread_cond_signal((pthread_cond_t *)cond);
}

__SPRT_C_FUNC int __SPRT_ID(pthread_rwlock_init)(__SPRT_ID(pthread_rwlock_t) * __SPRT_RESTRICT lock,
		const __SPRT_ID(pthread_rwlockattr_t) * __SPRT_RESTRICT attr) {
	return ::pthread_rwlock_init((pthread_rwlock_t *)lock, (pthread_rwlockattr_t *)attr);
}

__SPRT_C_FUNC int __SPRT_ID(pthread_rwlock_destroy)(__SPRT_ID(pthread_rwlock_t) * lock) {
	return ::pthread_rwlock_destroy((pthread_rwlock_t *)lock);
}

__SPRT_C_FUNC int __SPRT_ID(pthread_rwlock_rdlock)(__SPRT_ID(pthread_rwlock_t) * lock) {
	return ::pthread_rwlock_rdlock((pthread_rwlock_t *)lock);
}
__SPRT_C_FUNC int __SPRT_ID(pthread_rwlock_tryrdlock)(__SPRT_ID(pthread_rwlock_t) * lock) {
	return ::pthread_rwlock_tryrdlock((pthread_rwlock_t *)lock);
}
__SPRT_C_FUNC int __SPRT_ID(
		pthread_rwlock_timedrdlock)(__SPRT_ID(pthread_rwlock_t) * __SPRT_RESTRICT lock,
		const __SPRT_TIMESPEC_NAME *__SPRT_RESTRICT tv) {
	struct timespec native;
	if (tv) {
		native.tv_nsec = tv->tv_nsec;
		native.tv_sec = tv->tv_sec;
	}
	return ::pthread_rwlock_timedrdlock((pthread_rwlock_t *)lock, tv ? &native : nullptr);
}

__SPRT_C_FUNC int __SPRT_ID(pthread_rwlock_wrlock)(__SPRT_ID(pthread_rwlock_t) * lock) {
	return ::pthread_rwlock_wrlock((pthread_rwlock_t *)lock);
}
__SPRT_C_FUNC int __SPRT_ID(pthread_rwlock_trywrlock)(__SPRT_ID(pthread_rwlock_t) * lock) {
	return ::pthread_rwlock_trywrlock((pthread_rwlock_t *)lock);
}
__SPRT_C_FUNC int __SPRT_ID(
		pthread_rwlock_timedwrlock)(__SPRT_ID(pthread_rwlock_t) * __SPRT_RESTRICT lock,
		const __SPRT_TIMESPEC_NAME *__SPRT_RESTRICT tv) {
	struct timespec native;
	if (tv) {
		native.tv_nsec = tv->tv_nsec;
		native.tv_sec = tv->tv_sec;
	}
	return ::pthread_rwlock_timedwrlock((pthread_rwlock_t *)lock, tv ? &native : nullptr);
}
__SPRT_C_FUNC int __SPRT_ID(pthread_rwlock_unlock)(__SPRT_ID(pthread_rwlock_t) * lock) {
	return pthread_rwlock_unlock((pthread_rwlock_t *)lock);
}

__SPRT_C_FUNC int __SPRT_ID(pthread_spin_init)(__SPRT_ID(pthread_spinlock_t) * spin, int v) {
	return ::pthread_spin_init(spin, v);
}
__SPRT_C_FUNC int __SPRT_ID(pthread_spin_destroy)(__SPRT_ID(pthread_spinlock_t) * spin) {
	return ::pthread_spin_destroy(spin);
}
__SPRT_C_FUNC int __SPRT_ID(pthread_spin_lock)(__SPRT_ID(pthread_spinlock_t) * spin) {
	return ::pthread_spin_lock(spin);
}
__SPRT_C_FUNC int __SPRT_ID(pthread_spin_trylock)(__SPRT_ID(pthread_spinlock_t) * spin) {
	return ::pthread_spin_trylock(spin);
}
__SPRT_C_FUNC int __SPRT_ID(pthread_spin_unlock)(__SPRT_ID(pthread_spinlock_t) * spin) {
	return ::pthread_spin_unlock(spin);
}

__SPRT_C_FUNC int __SPRT_ID(
		pthread_barrier_init)(__SPRT_ID(pthread_barrier_t) * __SPRT_RESTRICT bar,
		const __SPRT_ID(pthread_barrierattr_t) * __SPRT_RESTRICT attr, unsigned v) {
	return ::pthread_barrier_init((pthread_barrier_t *)bar, (pthread_barrierattr_t *)attr, v);
}
__SPRT_C_FUNC int __SPRT_ID(pthread_barrier_destroy)(__SPRT_ID(pthread_barrier_t) * bar) {
	return ::pthread_barrier_destroy((pthread_barrier_t *)bar);
}
__SPRT_C_FUNC int __SPRT_ID(pthread_barrier_wait)(__SPRT_ID(pthread_barrier_t) * bar) {
	return ::pthread_barrier_wait((pthread_barrier_t *)bar);
}

__SPRT_C_FUNC int __SPRT_ID(
		pthread_key_create)(__SPRT_ID(pthread_key_t) * key, void (*cb)(void *)) {
	return ::pthread_key_create(key, cb);
}
__SPRT_C_FUNC int __SPRT_ID(pthread_key_delete)(__SPRT_ID(pthread_key_t) key) {
	return ::pthread_key_delete(key);
}
__SPRT_C_FUNC void *__SPRT_ID(pthread_getspecific)(__SPRT_ID(pthread_key_t) key) {
	return ::pthread_getspecific(key);
}
__SPRT_C_FUNC int __SPRT_ID(pthread_setspecific)(__SPRT_ID(pthread_key_t) key, const void *val) {
	return ::pthread_setspecific(key, val);
}

__SPRT_C_FUNC int __SPRT_ID(pthread_attr_init)(__SPRT_ID(pthread_attr_t) * attr) {
	return ::pthread_attr_init((pthread_attr_t *)attr);
}
__SPRT_C_FUNC int __SPRT_ID(pthread_attr_destroy)(__SPRT_ID(pthread_attr_t) * attr) {
	return ::pthread_attr_destroy((pthread_attr_t *)attr);
}

__SPRT_C_FUNC int __SPRT_ID(
		pthread_attr_getguardsize)(const __SPRT_ID(pthread_attr_t) * __SPRT_RESTRICT attr,
		__SPRT_ID(size_t) * __SPRT_RESTRICT ret) {
	return ::pthread_attr_getguardsize((pthread_attr_t *)attr, ret);
}
__SPRT_C_FUNC int __SPRT_ID(
		pthread_attr_setguardsize)(__SPRT_ID(pthread_attr_t) * attr, __SPRT_ID(size_t) v) {
	return ::pthread_attr_setguardsize((pthread_attr_t *)attr, v);
}
__SPRT_C_FUNC int __SPRT_ID(
		pthread_attr_getstacksize)(const __SPRT_ID(pthread_attr_t) * __SPRT_RESTRICT attr,
		__SPRT_ID(size_t) * __SPRT_RESTRICT ret) {
	return ::pthread_attr_getstacksize((pthread_attr_t *)attr, ret);
}
__SPRT_C_FUNC int __SPRT_ID(
		pthread_attr_setstacksize)(__SPRT_ID(pthread_attr_t) * attr, __SPRT_ID(size_t) v) {
	return ::pthread_attr_setstacksize((pthread_attr_t *)attr, v);
}
__SPRT_C_FUNC int __SPRT_ID(
		pthread_attr_getdetachstate)(const __SPRT_ID(pthread_attr_t) * attr, int *ret) {
	return ::pthread_attr_getdetachstate((pthread_attr_t *)attr, ret);
}
__SPRT_C_FUNC int __SPRT_ID(pthread_attr_setdetachstate)(__SPRT_ID(pthread_attr_t) * attr, int v) {
	return ::pthread_attr_setdetachstate((pthread_attr_t *)attr, v);
}
__SPRT_C_FUNC int __SPRT_ID(
		pthread_attr_getstack)(const __SPRT_ID(pthread_attr_t) * __SPRT_RESTRICT attr,
		void **__SPRT_RESTRICT ret, __SPRT_ID(size_t) * __SPRT_RESTRICT sz) {
	return ::pthread_attr_getstack((pthread_attr_t *)attr, ret, sz);
}
__SPRT_C_FUNC int __SPRT_ID(
		pthread_attr_setstack)(__SPRT_ID(pthread_attr_t) * attr, void *ptr, __SPRT_ID(size_t) sz) {
	return ::pthread_attr_setstack((pthread_attr_t *)attr, ptr, sz);
}
__SPRT_C_FUNC int __SPRT_ID(pthread_attr_getscope)(
		const __SPRT_ID(pthread_attr_t) * __SPRT_RESTRICT attr, int *__SPRT_RESTRICT ret) {
	return ::pthread_attr_getscope((pthread_attr_t *)attr, ret);
}
__SPRT_C_FUNC int __SPRT_ID(pthread_attr_setscope)(__SPRT_ID(pthread_attr_t) * attr, int v) {
	return ::pthread_attr_setscope((pthread_attr_t *)attr, v);
}
__SPRT_C_FUNC int __SPRT_ID(pthread_attr_getschedpolicy)(
		const __SPRT_ID(pthread_attr_t) * __SPRT_RESTRICT attr, int *__SPRT_RESTRICT ret) {
	return ::pthread_attr_getschedpolicy((pthread_attr_t *)attr, ret);
}
__SPRT_C_FUNC int __SPRT_ID(pthread_attr_setschedpolicy)(__SPRT_ID(pthread_attr_t) * attr, int v) {
	return ::pthread_attr_setschedpolicy((pthread_attr_t *)attr, v);
}
__SPRT_C_FUNC int __SPRT_ID(
		pthread_attr_getschedparam)(const __SPRT_ID(pthread_attr_t) * __SPRT_RESTRICT attr,
		struct __SPRT_ID(sched_param) * __SPRT_RESTRICT ret) {
	struct sched_param native;
	auto r = ::pthread_attr_getschedparam((pthread_attr_t *)attr, &native);
	if (ret) {
		ret->sched_priority = native.sched_priority;
	}
	return r;
}
__SPRT_C_FUNC int __SPRT_ID(
		pthread_attr_setschedparam)(__SPRT_ID(pthread_attr_t) * __SPRT_RESTRICT attr,
		const struct __SPRT_ID(sched_param) * __SPRT_RESTRICT val) {
	struct sched_param native;
	if (val) {
		native.sched_priority = val->sched_priority;
	}
	return ::pthread_attr_setschedparam((pthread_attr_t *)attr, val ? &native : nullptr);
}
__SPRT_C_FUNC int __SPRT_ID(pthread_attr_getinheritsched)(
		const __SPRT_ID(pthread_attr_t) * __SPRT_RESTRICT attr, int *__SPRT_RESTRICT ret) {
	return ::pthread_attr_getinheritsched((pthread_attr_t *)attr, ret);
}
__SPRT_C_FUNC int __SPRT_ID(pthread_attr_setinheritsched)(__SPRT_ID(pthread_attr_t) * attr, int v) {
	return ::pthread_attr_setinheritsched((pthread_attr_t *)attr, v);
}

__SPRT_C_FUNC int __SPRT_ID(pthread_mutexattr_destroy)(__SPRT_ID(pthread_mutexattr_t) * attr) {
	return ::pthread_mutexattr_destroy((pthread_mutexattr_t *)attr);
}
__SPRT_C_FUNC int __SPRT_ID(pthread_mutexattr_getprioceiling)(
		const __SPRT_ID(pthread_mutexattr_t) * __SPRT_RESTRICT attr, int *__SPRT_RESTRICT ret) {
	return ::pthread_mutexattr_getprioceiling((pthread_mutexattr_t *)attr, ret);
}
__SPRT_C_FUNC int __SPRT_ID(pthread_mutexattr_getprotocol)(
		const __SPRT_ID(pthread_mutexattr_t) * __SPRT_RESTRICT attr, int *__SPRT_RESTRICT ret) {
	return ::pthread_mutexattr_getprotocol((pthread_mutexattr_t *)attr, ret);
}
__SPRT_C_FUNC int __SPRT_ID(pthread_mutexattr_getpshared)(
		const __SPRT_ID(pthread_mutexattr_t) * __SPRT_RESTRICT attr, int *__SPRT_RESTRICT ret) {
	return ::pthread_mutexattr_getpshared((pthread_mutexattr_t *)attr, ret);
}
__SPRT_C_FUNC int __SPRT_ID(pthread_mutexattr_getrobust)(
		const __SPRT_ID(pthread_mutexattr_t) * __SPRT_RESTRICT attr, int *__SPRT_RESTRICT ret) {
	return ::pthread_mutexattr_getrobust((pthread_mutexattr_t *)attr, ret);
}
__SPRT_C_FUNC int __SPRT_ID(pthread_mutexattr_gettype)(
		const __SPRT_ID(pthread_mutexattr_t) * __SPRT_RESTRICT attr, int *__SPRT_RESTRICT ret) {
	return ::pthread_mutexattr_gettype((pthread_mutexattr_t *)attr, ret);
}
__SPRT_C_FUNC int __SPRT_ID(pthread_mutexattr_init)(__SPRT_ID(pthread_mutexattr_t) * attr) {
	return ::pthread_mutexattr_init((pthread_mutexattr_t *)attr);
}
__SPRT_C_FUNC int __SPRT_ID(
		pthread_mutexattr_setprioceiling)(__SPRT_ID(pthread_mutexattr_t) * attr, int v) {
	return ::pthread_mutexattr_setprioceiling((pthread_mutexattr_t *)attr, v);
}
__SPRT_C_FUNC int __SPRT_ID(
		pthread_mutexattr_setprotocol)(__SPRT_ID(pthread_mutexattr_t) * attr, int v) {
	return ::pthread_mutexattr_setprotocol((pthread_mutexattr_t *)attr, v);
}
__SPRT_C_FUNC int __SPRT_ID(
		pthread_mutexattr_setpshared)(__SPRT_ID(pthread_mutexattr_t) * attr, int v) {
	return ::pthread_mutexattr_setpshared((pthread_mutexattr_t *)attr, v);
}
__SPRT_C_FUNC int __SPRT_ID(
		pthread_mutexattr_setrobust)(__SPRT_ID(pthread_mutexattr_t) * attr, int v) {
	return ::pthread_mutexattr_setrobust((pthread_mutexattr_t *)attr, v);
}
__SPRT_C_FUNC int __SPRT_ID(
		pthread_mutexattr_settype)(__SPRT_ID(pthread_mutexattr_t) * attr, int v) {
	return ::pthread_mutexattr_settype((pthread_mutexattr_t *)attr, v);
}

__SPRT_C_FUNC int __SPRT_ID(pthread_condattr_init)(__SPRT_ID(pthread_condattr_t) * attr) {
	return ::pthread_condattr_init((pthread_condattr_t *)attr);
}
__SPRT_C_FUNC int __SPRT_ID(pthread_condattr_destroy)(__SPRT_ID(pthread_condattr_t) * attr) {
	return ::pthread_condattr_destroy((pthread_condattr_t *)attr);
}
__SPRT_C_FUNC int __SPRT_ID(pthread_condattr_setclock)(__SPRT_ID(pthread_condattr_t) * attr,
		__SPRT_ID(clockid_t) clock) {
	return ::pthread_condattr_setclock((pthread_condattr_t *)attr, clock);
}
__SPRT_C_FUNC int __SPRT_ID(
		pthread_condattr_setpshared)(__SPRT_ID(pthread_condattr_t) * attr, int v) {
	return ::pthread_condattr_setpshared((pthread_condattr_t *)attr, v);
}
__SPRT_C_FUNC int __SPRT_ID(
		pthread_condattr_getclock)(const __SPRT_ID(pthread_condattr_t) * __SPRT_RESTRICT attr,
		__SPRT_ID(clockid_t) * __SPRT_RESTRICT clock) {
	return ::pthread_condattr_getclock((pthread_condattr_t *)attr, clock);
}
__SPRT_C_FUNC int __SPRT_ID(pthread_condattr_getpshared)(
		const __SPRT_ID(pthread_condattr_t) * __SPRT_RESTRICT attr, int *__SPRT_RESTRICT ret) {
	return ::pthread_condattr_getpshared((pthread_condattr_t *)attr, ret);
}

__SPRT_C_FUNC int __SPRT_ID(pthread_rwlockattr_init)(__SPRT_ID(pthread_rwlockattr_t) * attr) {
	return ::pthread_rwlockattr_init((pthread_rwlockattr_t *)attr);
}
__SPRT_C_FUNC int __SPRT_ID(pthread_rwlockattr_destroy)(__SPRT_ID(pthread_rwlockattr_t) * attr) {
	return ::pthread_rwlockattr_destroy((pthread_rwlockattr_t *)attr);
}
__SPRT_C_FUNC int __SPRT_ID(
		pthread_rwlockattr_setpshared)(__SPRT_ID(pthread_rwlockattr_t) * attr, int v) {
	return ::pthread_rwlockattr_setpshared((pthread_rwlockattr_t *)attr, v);
}
__SPRT_C_FUNC int __SPRT_ID(pthread_rwlockattr_getpshared)(
		const __SPRT_ID(pthread_rwlockattr_t) * __SPRT_RESTRICT attr, int *__SPRT_RESTRICT ret) {
	return ::pthread_rwlockattr_getpshared((pthread_rwlockattr_t *)attr, ret);
}

__SPRT_C_FUNC int __SPRT_ID(pthread_barrierattr_destroy)(__SPRT_ID(pthread_barrierattr_t) * attr) {
	return ::pthread_barrierattr_destroy((pthread_barrierattr_t *)attr);
}
__SPRT_C_FUNC int __SPRT_ID(pthread_barrierattr_getpshared)(
		const __SPRT_ID(pthread_barrierattr_t) * __SPRT_RESTRICT attr, int *__SPRT_RESTRICT ret) {
	return ::pthread_barrierattr_getpshared((pthread_barrierattr_t *)attr, ret);
}
__SPRT_C_FUNC int __SPRT_ID(pthread_barrierattr_init)(__SPRT_ID(pthread_barrierattr_t) * attr) {
	return ::pthread_barrierattr_init((pthread_barrierattr_t *)attr);
}
__SPRT_C_FUNC int __SPRT_ID(
		pthread_barrierattr_setpshared)(__SPRT_ID(pthread_barrierattr_t) * attr, int v) {
	return ::pthread_barrierattr_setpshared((pthread_barrierattr_t *)attr, v);
}

__SPRT_C_FUNC int __SPRT_ID(
		pthread_atfork)(void (*prepare)(void), void (*parent)(void), void (*child)(void)) {
	return ::pthread_atfork(prepare, parent, child);
}

__SPRT_C_FUNC int __SPRT_ID(pthread_getconcurrency)(void) { return ::pthread_getconcurrency(); }
__SPRT_C_FUNC int __SPRT_ID(pthread_setconcurrency)(int v) { return ::pthread_setconcurrency(v); }

__SPRT_C_FUNC int __SPRT_ID(
		pthread_getcpuclockid)(__SPRT_ID(pthread_t) thread, __SPRT_ID(clockid_t) * clock) {
	return ::pthread_getcpuclockid(thread, clock);
}

__SPRT_C_FUNC void __SPRT_ID(pthread_cleanup_push)(void (*cb)(void *), void *v) {
#warning TODO
}
__SPRT_C_FUNC void __SPRT_ID(pthread_cleanup_pop)(int exec) {
#warning TODO
}

__SPRT_C_FUNC int __SPRT_ID(pthread_getaffinity_np)(__SPRT_ID(pthread_t) thread,
		__SPRT_ID(size_t) n, __SPRT_ID(cpu_set_t) * set) {
	return ::pthread_getaffinity_np(thread, n, (cpu_set_t *)set);
}
__SPRT_C_FUNC int __SPRT_ID(pthread_setaffinity_np)(__SPRT_ID(pthread_t) thread,
		__SPRT_ID(size_t) n, const __SPRT_ID(cpu_set_t) * set) {
	return ::pthread_setaffinity_np(thread, n, (const cpu_set_t *)set);
}
__SPRT_C_FUNC int __SPRT_ID(
		pthread_getattr_np)(__SPRT_ID(pthread_t) thread, __SPRT_ID(pthread_attr_t) * attr) {
	return ::pthread_getattr_np(thread, (pthread_attr_t *)attr);
}
__SPRT_C_FUNC int __SPRT_ID(pthread_setname_np)(__SPRT_ID(pthread_t) thread, const char *name) {
	return ::pthread_setname_np(thread, name);
}
__SPRT_C_FUNC int __SPRT_ID(
		pthread_getname_np)(__SPRT_ID(pthread_t) thread, char *buf, __SPRT_ID(size_t) len) {
	return ::pthread_getname_np(thread, buf, len);
}
__SPRT_C_FUNC int __SPRT_ID(pthread_getattr_default_np)(__SPRT_ID(pthread_attr_t) * attr) {
	return ::pthread_getattr_default_np((pthread_attr_t *)attr);
}
__SPRT_C_FUNC int __SPRT_ID(pthread_setattr_default_np)(const __SPRT_ID(pthread_attr_t) * attr) {
	return ::pthread_setattr_default_np((const pthread_attr_t *)attr);
}
__SPRT_C_FUNC int __SPRT_ID(pthread_tryjoin_np)(__SPRT_ID(pthread_t) thread, void **ret) {
	return ::pthread_tryjoin_np(thread, ret);
}
__SPRT_C_FUNC int __SPRT_ID(pthread_timedjoin_np)(__SPRT_ID(pthread_t) thread, void **ret,
		const __SPRT_TIMESPEC_NAME *tv) {
	struct timespec native;
	if (tv) {
		native.tv_nsec = tv->tv_nsec;
		native.tv_sec = tv->tv_sec;
	}
	return ::pthread_timedjoin_np(thread, ret, tv ? &native : nullptr);
}

} // namespace sprt
