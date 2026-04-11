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

#include <sprt/c/__sprt_pthread.h>
#include <sprt/c/__sprt_sched.h>
#include <sprt/runtime/log.h>

#include "private/SPRTThread.h"

#include "pthread/pthread.cc"
#include "pthread/pthread_attr.cc"
#include "pthread/pthread_key.cc"
#include "pthread/pthread_mutex.cc"
#include "pthread/pthread_cond.cc"
#include "pthread/pthread_rwlock.cc"
#include "pthread/pthread_barrier.cc"
#include "pthread/pthread_spin.cc"
#include "pthread/pthread_np.cc"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wundefined-internal"

namespace sprt {

static_assert(sizeof(sprt::_thread::thread_t *) == sizeof(__sprt_pthread_t));
static_assert(sizeof(sprt::_thread::attr_t) == sizeof(__sprt_pthread_attr_t));
static_assert(sizeof(sprt::_thread::once_t) == sizeof(__sprt_pthread_once_t));
static_assert(sizeof(sprt::_thread::key_t) == sizeof(__sprt_pthread_key_t));
static_assert(sizeof(sprt::_thread::mutex_t) == sizeof(__sprt_pthread_mutex_t));
static_assert(sizeof(sprt::_thread::mutexattr_t) <= sizeof(__sprt_pthread_mutexattr_t));
static_assert(sizeof(sprt::_thread::cond_t) <= sizeof(__sprt_pthread_cond_t));
static_assert(sizeof(sprt::_thread::condattr_t) <= sizeof(__sprt_pthread_condattr_t));
static_assert(sizeof(sprt::_thread::rwlock_t) == sizeof(__sprt_pthread_rwlock_t));
static_assert(sizeof(sprt::_thread::rwlockattr_t) == sizeof(__sprt_pthread_rwlockattr_t));

static_assert(sizeof(sprt::_thread::spinlock_t) == sizeof(__sprt_pthread_spinlock_t));
static_assert(sizeof(sprt::_thread::barrier_t) == sizeof(__sprt_pthread_barrier_t));
static_assert(sizeof(sprt::_thread::barrierattr_t) <= sizeof(__sprt_pthread_barrierattr_t));

__SPRT_C_FUNC int __SPRT_ID(pthread_create)(__SPRT_ID(pthread_t) * __SPRT_RESTRICT outthread,
		const __SPRT_ID(pthread_attr_t) * __SPRT_RESTRICT attr, void *(*cb)(void *),
		void *__SPRT_RESTRICT arg) {
	_thread::thread_t *__t = nullptr;

	auto ret = _thread::thread_t::create(&__t, reinterpret_cast< const _thread::attr_t *>(attr), cb,
			arg);
	if (ret == 0) {
		*outthread = reinterpret_cast<__SPRT_ID(pthread_t)>(__t);
		return ret;
	}
	return ret;
}

__SPRT_C_FUNC int __SPRT_ID(pthread_detach)(__SPRT_ID(pthread_t) __t) {
	return _thread::thread_t::detach(reinterpret_cast<_thread::thread_t *>(__t));
}

__SPRT_C_FUNC __SPRT_NORETURN void __SPRT_ID(pthread_exit)(void *ret) {
	_thread::thread_t::exit(ret);
}

__SPRT_C_FUNC int __SPRT_ID(pthread_join)(__SPRT_ID(pthread_t) __t, void **ret) {
	return _thread::thread_t::join(reinterpret_cast<_thread::thread_t *>(__t), ret);
}

__SPRT_C_FUNC __SPRT_ID(pthread_t) __SPRT_ID(pthread_self)(void) {
	return reinterpret_cast<__SPRT_ID(pthread_t)>(_thread::thread_t::self());
}

__SPRT_C_FUNC int __SPRT_ID(pthread_equal)(__SPRT_ID(pthread_t) t1, __SPRT_ID(pthread_t) t2) {
	return t1 == t2;
}

__SPRT_C_FUNC int __SPRT_ID(pthread_setcancelstate)(int v, int *p) {
	return _thread::thread_t::setcancelstate(v, p);
}
__SPRT_C_FUNC int __SPRT_ID(pthread_setcanceltype)(int v, int *p) {
	return _thread::thread_t::setcanceltype(v, p);
}

__SPRT_C_FUNC void __SPRT_ID(pthread_testcancel)(void) { return _thread::thread_t::testcancel(); }

__SPRT_C_FUNC int __SPRT_ID(pthread_cancel)(__SPRT_ID(pthread_t) __t) {
	return _thread::thread_t::cancel(reinterpret_cast<_thread::thread_t *>(__t));
}

__SPRT_C_FUNC int __SPRT_ID(pthread_getschedparam)(__SPRT_ID(pthread_t) __t, int *__SPRT_RESTRICT n,
		struct __SPRT_ID(sched_param) * __SPRT_RESTRICT p) {
	return _thread::thread_t::getschedparam(reinterpret_cast<_thread::thread_t *>(__t), n, p);
}

__SPRT_C_FUNC int __SPRT_ID(pthread_setschedparam)(__SPRT_ID(pthread_t) __t, int n,
		const struct __SPRT_ID(sched_param) * p) {
	return _thread::thread_t::setschedparam(reinterpret_cast<_thread::thread_t *>(__t), n, p);
}

__SPRT_C_FUNC int __SPRT_ID(pthread_setschedprio)(__SPRT_ID(pthread_t) __t, int p) {
	return _thread::thread_t::setschedprio(reinterpret_cast<_thread::thread_t *>(__t), p);
}

__SPRT_C_FUNC int __SPRT_ID(pthread_once)(__SPRT_ID(pthread_once_t) * once, void (*cb)(void)) {
	if (!once || !cb) {
		return EINVAL;
	}

	return qonce::perform(once, [&] { cb(); });
}

__SPRT_C_FUNC int __SPRT_ID(
		pthread_atfork)(void (*prepare)(void), void (*parent)(void), void (*child)(void)) {
	return ENOSYS;
}

__SPRT_C_FUNC int __SPRT_ID(pthread_getconcurrency)(void) { return 0; }

__SPRT_C_FUNC int __SPRT_ID(pthread_setconcurrency)(int val) {
	if (val < 0) {
		return EINVAL;
	}
	if (val > 0) {
		return EAGAIN;
	}
	return 0;
}

__SPRT_C_FUNC int __SPRT_ID(
		pthread_getcpuclockid)(__SPRT_ID(pthread_t) thread, __SPRT_ID(clockid_t) * clock) {
	if (!thread || !clock) {
		return EINVAL;
	}

	return reinterpret_cast<_thread::thread_t *>(thread)->getcpuclockid(clock);
}

} // namespace sprt

#pragma clang diagnostic pop
