/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
**/

#include <pthread.h>

__SPRT_BEGIN_DECL

int pthread_create(pthread_t *__SPRT_RESTRICT thread, const pthread_attr_t *__SPRT_RESTRICT attr,
		void *(*cb)(void *), void *__SPRT_RESTRICT arg) __SPRT_NOEXCEPT {
	return __sprt_pthread_create(thread, attr, cb, arg);
}
int pthread_detach(pthread_t thread) __SPRT_NOEXCEPT { return __sprt_pthread_detach(thread); }
__SPRT_NORETURN void pthread_exit(void *ret) __SPRT_NOEXCEPT { __sprt_pthread_exit(ret); }
int pthread_join(pthread_t thread, void **ret) __SPRT_NOEXCEPT {
	return __sprt_pthread_join(thread, ret);
}
pthread_t pthread_self(void) __SPRT_NOEXCEPT { return __sprt_pthread_self(); }
int pthread_equal(pthread_t t1, pthread_t t2) __SPRT_NOEXCEPT {
	return __sprt_pthread_equal(t1, t2);
}
int pthread_setcancelstate(int v, int *p) __SPRT_NOEXCEPT {
	return __sprt_pthread_setcancelstate(v, p);
}
int pthread_setcanceltype(int v, int *p) __SPRT_NOEXCEPT {
	return __sprt_pthread_setcanceltype(v, p);
}
void pthread_testcancel(void) __SPRT_NOEXCEPT { __sprt_pthread_testcancel(); }
int pthread_cancel(pthread_t thread) __SPRT_NOEXCEPT { return __sprt_pthread_cancel(thread); }
int pthread_getschedparam(pthread_t thread, int *__SPRT_RESTRICT n,
		struct __SPRT_SCHED_PARAM_NAME *__SPRT_RESTRICT p) __SPRT_NOEXCEPT {
	return __sprt_pthread_getschedparam(thread, n, p);
}
int pthread_setschedparam(pthread_t thread, int n,
		const struct __SPRT_SCHED_PARAM_NAME *p) __SPRT_NOEXCEPT {
	return __sprt_pthread_setschedparam(thread, n, p);
}
int pthread_setschedprio(pthread_t thread, int p) __SPRT_NOEXCEPT {
	return __sprt_pthread_setschedprio(thread, p);
}
int pthread_sigmask(int how, const __SPRT_ID(sigset_t) * set,
		__SPRT_ID(sigset_t) * oldset) __SPRT_NOEXCEPT {
	return __sprt_pthread_sigmask(how, set, oldset);
}
int pthread_once(pthread_once_t *once, void (*cb)(void)) __SPRT_NOEXCEPT {
	return __sprt_pthread_once(once, cb);
}
int pthread_mutex_init(pthread_mutex_t *__SPRT_RESTRICT mutex,
		const pthread_mutexattr_t *__SPRT_RESTRICT attr) __SPRT_NOEXCEPT {
	return __sprt_pthread_mutex_init(mutex, attr);
}
int pthread_mutex_lock(pthread_mutex_t *mutex) __SPRT_NOEXCEPT {
	return __sprt_pthread_mutex_lock(mutex);
}
int pthread_mutex_unlock(pthread_mutex_t *mutex) __SPRT_NOEXCEPT {
	return __sprt_pthread_mutex_unlock(mutex);
}
int pthread_mutex_trylock(pthread_mutex_t *mutex) __SPRT_NOEXCEPT {
	return __sprt_pthread_mutex_trylock(mutex);
}
int pthread_mutex_timedlock(pthread_mutex_t *__SPRT_RESTRICT mutex,
		const timespec *__SPRT_RESTRICT tv) __SPRT_NOEXCEPT {
	return __sprt_pthread_mutex_timedlock(mutex, tv);
}
int pthread_mutex_destroy(pthread_mutex_t *mutex) __SPRT_NOEXCEPT {
	return __sprt_pthread_mutex_destroy(mutex);
}
int pthread_mutex_consistent(pthread_mutex_t *mutex) __SPRT_NOEXCEPT {
	return __sprt_pthread_mutex_consistent(mutex);
}
int pthread_mutex_getprioceiling(const pthread_mutex_t *__SPRT_RESTRICT mutex,
		int *__SPRT_RESTRICT p) __SPRT_NOEXCEPT {
	return __sprt_pthread_mutex_getprioceiling(mutex, p);
}
int pthread_mutex_setprioceiling(pthread_mutex_t *__SPRT_RESTRICT mutex, int v,
		int *__SPRT_RESTRICT p) __SPRT_NOEXCEPT {
	return __sprt_pthread_mutex_setprioceiling(mutex, v, p);
}
int pthread_cond_init(pthread_cond_t *__SPRT_RESTRICT cond,
		const pthread_condattr_t *__SPRT_RESTRICT attr) __SPRT_NOEXCEPT {
	return __sprt_pthread_cond_init(cond, attr);
}
int pthread_cond_destroy(pthread_cond_t *cond) __SPRT_NOEXCEPT {
	return __sprt_pthread_cond_destroy(cond);
}
int pthread_cond_wait(pthread_cond_t *__SPRT_RESTRICT cond,
		pthread_mutex_t *__SPRT_RESTRICT mutex) __SPRT_NOEXCEPT {
	return __sprt_pthread_cond_wait(cond, mutex);
}
int pthread_cond_timedwait(pthread_cond_t *__SPRT_RESTRICT cond,
		pthread_mutex_t *__SPRT_RESTRICT mutex,
		const timespec *__SPRT_RESTRICT tv) __SPRT_NOEXCEPT {
	return __sprt_pthread_cond_timedwait(cond, mutex, tv);
}
int pthread_cond_broadcast(pthread_cond_t *cond) __SPRT_NOEXCEPT {
	return __sprt_pthread_cond_broadcast(cond);
}
int pthread_cond_signal(pthread_cond_t *cond) __SPRT_NOEXCEPT {
	return __sprt_pthread_cond_signal(cond);
}
int pthread_rwlock_init(pthread_rwlock_t *__SPRT_RESTRICT lock,
		const pthread_rwlockattr_t *__SPRT_RESTRICT attr) __SPRT_NOEXCEPT {
	return __sprt_pthread_rwlock_init(lock, attr);
}
int pthread_rwlock_destroy(pthread_rwlock_t *lock) __SPRT_NOEXCEPT {
	return __sprt_pthread_rwlock_destroy(lock);
}
int pthread_rwlock_rdlock(pthread_rwlock_t *lock) __SPRT_NOEXCEPT {
	return __sprt_pthread_rwlock_rdlock(lock);
}
int pthread_rwlock_tryrdlock(pthread_rwlock_t *lock) __SPRT_NOEXCEPT {
	return __sprt_pthread_rwlock_tryrdlock(lock);
}
int pthread_rwlock_timedrdlock(pthread_rwlock_t *__SPRT_RESTRICT lock,
		const timespec *__SPRT_RESTRICT tv) __SPRT_NOEXCEPT {
	return __sprt_pthread_rwlock_timedrdlock(lock, tv);
}
int pthread_rwlock_wrlock(pthread_rwlock_t *lock) __SPRT_NOEXCEPT {
	return __sprt_pthread_rwlock_wrlock(lock);
}
int pthread_rwlock_trywrlock(pthread_rwlock_t *lock) __SPRT_NOEXCEPT {
	return __sprt_pthread_rwlock_trywrlock(lock);
}
int pthread_rwlock_timedwrlock(pthread_rwlock_t *__SPRT_RESTRICT lock,
		const timespec *__SPRT_RESTRICT tv) __SPRT_NOEXCEPT {
	return __sprt_pthread_rwlock_timedwrlock(lock, tv);
}
int pthread_rwlock_unlock(pthread_rwlock_t *lock) __SPRT_NOEXCEPT {
	return __sprt_pthread_rwlock_unlock(lock);
}
int pthread_spin_init(pthread_spinlock_t *spin, int v) __SPRT_NOEXCEPT {
	return __sprt_pthread_spin_init(spin, v);
}
int pthread_spin_destroy(pthread_spinlock_t *spin) __SPRT_NOEXCEPT {
	return __sprt_pthread_spin_destroy(spin);
}
int pthread_spin_lock(pthread_spinlock_t *spin) __SPRT_NOEXCEPT {
	return __sprt_pthread_spin_lock(spin);
}
int pthread_spin_trylock(pthread_spinlock_t *spin) __SPRT_NOEXCEPT {
	return __sprt_pthread_spin_trylock(spin);
}
int pthread_spin_unlock(pthread_spinlock_t *spin) __SPRT_NOEXCEPT {
	return __sprt_pthread_spin_unlock(spin);
}
int pthread_barrier_init(pthread_barrier_t *__SPRT_RESTRICT bar,
		const pthread_barrierattr_t *__SPRT_RESTRICT attr, unsigned v) __SPRT_NOEXCEPT {
	return __sprt_pthread_barrier_init(bar, attr, v);
}
int pthread_barrier_destroy(pthread_barrier_t *bar) __SPRT_NOEXCEPT {
	return __sprt_pthread_barrier_destroy(bar);
}
int pthread_barrier_wait(pthread_barrier_t *bar) __SPRT_NOEXCEPT {
	return __sprt_pthread_barrier_wait(bar);
}
int pthread_key_create(pthread_key_t *key, void (*cb)(void *)) __SPRT_NOEXCEPT {
	return __sprt_pthread_key_create(key, cb);
}
int pthread_key_delete(pthread_key_t key) __SPRT_NOEXCEPT { return __sprt_pthread_key_delete(key); }
void *pthread_getspecific(pthread_key_t key) __SPRT_NOEXCEPT {
	return __sprt_pthread_getspecific(key);
}
int pthread_setspecific(pthread_key_t key, const void *val) __SPRT_NOEXCEPT {
	return __sprt_pthread_setspecific(key, val);
}
int pthread_attr_init(pthread_attr_t *attr) __SPRT_NOEXCEPT {
	return __sprt_pthread_attr_init(attr);
}
int pthread_attr_destroy(pthread_attr_t *attr) __SPRT_NOEXCEPT {
	return __sprt_pthread_attr_destroy(attr);
}
int pthread_attr_getguardsize(const pthread_attr_t *__SPRT_RESTRICT attr,
		size_t *__SPRT_RESTRICT ret) __SPRT_NOEXCEPT {
	return __sprt_pthread_attr_getguardsize(attr, ret);
}
int pthread_attr_setguardsize(pthread_attr_t *attr, size_t v) __SPRT_NOEXCEPT {
	return __sprt_pthread_attr_setguardsize(attr, v);
}
int pthread_attr_getstacksize(const pthread_attr_t *__SPRT_RESTRICT attr,
		size_t *__SPRT_RESTRICT ret) __SPRT_NOEXCEPT {
	return __sprt_pthread_attr_getstacksize(attr, ret);
}
int pthread_attr_setstacksize(pthread_attr_t *attr, size_t v) __SPRT_NOEXCEPT {
	return __sprt_pthread_attr_setstacksize(attr, v);
}
int pthread_attr_getdetachstate(const pthread_attr_t *attr, int *ret) __SPRT_NOEXCEPT {
	return __sprt_pthread_attr_getdetachstate(attr, ret);
}
int pthread_attr_setdetachstate(pthread_attr_t *attr, int v) __SPRT_NOEXCEPT {
	return __sprt_pthread_attr_setdetachstate(attr, v);
}
int pthread_attr_getstack(const pthread_attr_t *__SPRT_RESTRICT attr, void **__SPRT_RESTRICT ret,
		size_t *__SPRT_RESTRICT sz) __SPRT_NOEXCEPT {
	return __sprt_pthread_attr_getstack(attr, ret, sz);
}
int pthread_attr_setstack(pthread_attr_t *attr, void *ptr, size_t sz) __SPRT_NOEXCEPT {
	return __sprt_pthread_attr_setstack(attr, ptr, sz);
}
int pthread_attr_getscope(const pthread_attr_t *__SPRT_RESTRICT attr,
		int *__SPRT_RESTRICT ret) __SPRT_NOEXCEPT {
	return __sprt_pthread_attr_getscope(attr, ret);
}
int pthread_attr_setscope(pthread_attr_t *attr, int v) __SPRT_NOEXCEPT {
	return __sprt_pthread_attr_setscope(attr, v);
}
int pthread_attr_getschedpolicy(const pthread_attr_t *__SPRT_RESTRICT attr,
		int *__SPRT_RESTRICT ret) __SPRT_NOEXCEPT {
	return __sprt_pthread_attr_getschedpolicy(attr, ret);
}
int pthread_attr_setschedpolicy(pthread_attr_t *attr, int v) __SPRT_NOEXCEPT {
	return __sprt_pthread_attr_setschedpolicy(attr, v);
}
int pthread_attr_getschedparam(const pthread_attr_t *__SPRT_RESTRICT attr,
		struct __SPRT_SCHED_PARAM_NAME *__SPRT_RESTRICT ret) __SPRT_NOEXCEPT {
	return __sprt_pthread_attr_getschedparam(attr, ret);
}
int pthread_attr_setschedparam(pthread_attr_t *__SPRT_RESTRICT attr,
		const struct __SPRT_SCHED_PARAM_NAME *__SPRT_RESTRICT val) __SPRT_NOEXCEPT {
	return __sprt_pthread_attr_setschedparam(attr, val);
}
int pthread_attr_getinheritsched(const pthread_attr_t *__SPRT_RESTRICT attr,
		int *__SPRT_RESTRICT ret) __SPRT_NOEXCEPT {
	return __sprt_pthread_attr_getinheritsched(attr, ret);
}
int pthread_attr_setinheritsched(pthread_attr_t *attr, int v) __SPRT_NOEXCEPT {
	return __sprt_pthread_attr_setinheritsched(attr, v);
}
int pthread_mutexattr_destroy(pthread_mutexattr_t *attr) __SPRT_NOEXCEPT {
	return __sprt_pthread_mutexattr_destroy(attr);
}
int pthread_mutexattr_getprioceiling(const pthread_mutexattr_t *__SPRT_RESTRICT attr,
		int *__SPRT_RESTRICT ret) __SPRT_NOEXCEPT {
	return __sprt_pthread_mutexattr_getprioceiling(attr, ret);
}
int pthread_mutexattr_setprioceiling(pthread_mutexattr_t *attr, int v) __SPRT_NOEXCEPT {
	return __sprt_pthread_mutexattr_setprioceiling(attr, v);
}
int pthread_mutexattr_getprotocol(const pthread_mutexattr_t *__SPRT_RESTRICT attr,
		int *__SPRT_RESTRICT ret) __SPRT_NOEXCEPT {
	return __sprt_pthread_mutexattr_getprotocol(attr, ret);
}
int pthread_mutexattr_setprotocol(pthread_mutexattr_t *attr, int v) __SPRT_NOEXCEPT {
	return __sprt_pthread_mutexattr_setprotocol(attr, v);
}
int pthread_mutexattr_getrobust(const pthread_mutexattr_t *__SPRT_RESTRICT attr,
		int *__SPRT_RESTRICT ret) __SPRT_NOEXCEPT {
	return __sprt_pthread_mutexattr_getrobust(attr, ret);
}
int pthread_mutexattr_setrobust(pthread_mutexattr_t *attr, int v) __SPRT_NOEXCEPT {
	return __sprt_pthread_mutexattr_setrobust(attr, v);
}
int pthread_mutexattr_getpshared(const pthread_mutexattr_t *__SPRT_RESTRICT attr,
		int *__SPRT_RESTRICT ret) __SPRT_NOEXCEPT {
	return __sprt_pthread_mutexattr_getpshared(attr, ret);
}
int pthread_mutexattr_gettype(const pthread_mutexattr_t *__SPRT_RESTRICT attr,
		int *__SPRT_RESTRICT ret) __SPRT_NOEXCEPT {
	return __sprt_pthread_mutexattr_gettype(attr, ret);
}
int pthread_mutexattr_init(pthread_mutexattr_t *attr) __SPRT_NOEXCEPT {
	return __sprt_pthread_mutexattr_init(attr);
}
int pthread_mutexattr_setpshared(pthread_mutexattr_t *attr, int v) __SPRT_NOEXCEPT {
	return __sprt_pthread_mutexattr_setpshared(attr, v);
}
int pthread_mutexattr_settype(pthread_mutexattr_t *attr, int v) __SPRT_NOEXCEPT {
	return __sprt_pthread_mutexattr_settype(attr, v);
}
int pthread_condattr_init(pthread_condattr_t *attr) __SPRT_NOEXCEPT {
	return __sprt_pthread_condattr_init(attr);
}
int pthread_condattr_destroy(pthread_condattr_t *attr) __SPRT_NOEXCEPT {
	return __sprt_pthread_condattr_destroy(attr);
}
int pthread_condattr_setclock(pthread_condattr_t *attr, clockid_t clock) __SPRT_NOEXCEPT {
	return __sprt_pthread_condattr_setclock(attr, clock);
}
int pthread_condattr_getclock(const pthread_condattr_t *__SPRT_RESTRICT attr,
		clockid_t *__SPRT_RESTRICT clock) __SPRT_NOEXCEPT {
	return __sprt_pthread_condattr_getclock(attr, clock);
}
int pthread_getcpuclockid(pthread_t thread, clockid_t *clock) __SPRT_NOEXCEPT {
	return __sprt_pthread_getcpuclockid(thread, clock);
}
int pthread_condattr_setpshared(pthread_condattr_t *attr, int v) __SPRT_NOEXCEPT {
	return __sprt_pthread_condattr_setpshared(attr, v);
}
int pthread_condattr_getpshared(const pthread_condattr_t *__SPRT_RESTRICT attr,
		int *__SPRT_RESTRICT ret) __SPRT_NOEXCEPT {
	return __sprt_pthread_condattr_getpshared(attr, ret);
}
int pthread_rwlockattr_init(pthread_rwlockattr_t *attr) __SPRT_NOEXCEPT {
	return __sprt_pthread_rwlockattr_init(attr);
}
int pthread_rwlockattr_destroy(pthread_rwlockattr_t *attr) __SPRT_NOEXCEPT {
	return __sprt_pthread_rwlockattr_destroy(attr);
}
int pthread_rwlockattr_setpshared(pthread_rwlockattr_t *attr, int v) __SPRT_NOEXCEPT {
	return __sprt_pthread_rwlockattr_setpshared(attr, v);
}
int pthread_rwlockattr_getpshared(const pthread_rwlockattr_t *__SPRT_RESTRICT attr,
		int *__SPRT_RESTRICT ret) __SPRT_NOEXCEPT {
	return __sprt_pthread_rwlockattr_getpshared(attr, ret);
}
int pthread_barrierattr_destroy(pthread_barrierattr_t *attr) __SPRT_NOEXCEPT {
	return __sprt_pthread_barrierattr_destroy(attr);
}
int pthread_barrierattr_getpshared(const pthread_barrierattr_t *__SPRT_RESTRICT attr,
		int *__SPRT_RESTRICT ret) __SPRT_NOEXCEPT {
	return __sprt_pthread_barrierattr_getpshared(attr, ret);
}
int pthread_barrierattr_init(pthread_barrierattr_t *attr) __SPRT_NOEXCEPT {
	return __sprt_pthread_barrierattr_init(attr);
}
int pthread_barrierattr_setpshared(pthread_barrierattr_t *attr, int v) __SPRT_NOEXCEPT {
	return __sprt_pthread_barrierattr_setpshared(attr, v);
}
int pthread_atfork(void (*prepare)(void), void (*parent)(void),
		void (*child)(void)) __SPRT_NOEXCEPT {
	return __sprt_pthread_atfork(prepare, parent, child);
}
int pthread_getconcurrency(void) __SPRT_NOEXCEPT { return __sprt_pthread_getconcurrency(); }
int pthread_setconcurrency(int v) __SPRT_NOEXCEPT { return __sprt_pthread_setconcurrency(v); }

int pthread_getaffinity_np(pthread_t thread, size_t n, cpu_set_t *set) __SPRT_NOEXCEPT {
	return __sprt_pthread_getaffinity_np(thread, n, set);
}
int pthread_setaffinity_np(pthread_t thread, size_t n, const cpu_set_t *set) __SPRT_NOEXCEPT {
	return __sprt_pthread_setaffinity_np(thread, n, set);
}
int pthread_getattr_np(pthread_t thread, pthread_attr_t *attr) __SPRT_NOEXCEPT {
	return __sprt_pthread_getattr_np(thread, attr);
}
int pthread_setname_np(pthread_t thread, const char *name) __SPRT_NOEXCEPT {
	return __sprt_pthread_setname_np(thread, name);
}
int pthread_getname_np(pthread_t thread, char *buf, size_t len) __SPRT_NOEXCEPT {
	return __sprt_pthread_getname_np(thread, buf, len);
}
int pthread_getattr_default_np(pthread_attr_t *attr) __SPRT_NOEXCEPT {
	return __sprt_pthread_getattr_default_np(attr);
}
int pthread_setattr_default_np(const pthread_attr_t *attr) __SPRT_NOEXCEPT {
	return __sprt_pthread_setattr_default_np(attr);
}
int pthread_tryjoin_np(pthread_t thread, void **ret) __SPRT_NOEXCEPT {
	return __sprt_pthread_tryjoin_np(thread, ret);
}
int pthread_timedjoin_np(pthread_t thread, void **ret, const timespec *tv) __SPRT_NOEXCEPT {
	return __sprt_pthread_timedjoin_np(thread, ret, tv);
}
__SPRT_END_DECL
