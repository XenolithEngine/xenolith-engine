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

#define __SPRT_BUILD 1

#include "pthread_native.h"

namespace sprt::_thread {

// based on https://www.remlab.net/op/futex-condvar.shtml

static uint64_t __cond_mutex_id(void *mutex) {
	return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(mutex));
}

static Status __cond_mutex_lock(void *mutex) {
	return status::errnoToStatus(
			reinterpret_cast<mutex_t *>(mutex)->lock(__SPRT_SPRT_TIMEOUT_INFINITE));
}

static Status __cond_mutex_unlock(void *mutex) {
	return status::errnoToStatus(reinterpret_cast<mutex_t *>(mutex)->unlock());
}

int cond_t::wait(mutex_t *mutex, __sprt_sprt_timeout_t timeout) {
	__sprt_sprt_lock_flags_t condFlag = 0;
	if (hasFlag(CondAttrFlags(data.padding), CondAttrFlags::Shared)) {
		condFlag = __SPRT_SPRT_LOCK_FLAG_SHARED;
	}
	if (hasFlag(CondAttrFlags(data.padding), CondAttrFlags::ClockRealtime)) {
		condFlag = __SPRT_SPRT_LOCK_FLAG_CLOCK_REALTIME;
	}

	Status ret = Status::Ok;
	if (timeout == __SPRT_SPRT_TIMEOUT_INFINITE) {
		ret = qcondvar_base::_wait<__sprt_sprt_qlock_wait, nullptr, __cond_mutex_id,
				__cond_mutex_lock, __cond_mutex_unlock>(&data, mutex, nullptr, condFlag);
	} else {
		ret = qcondvar_base::_wait<__sprt_sprt_qlock_wait, __sprt_sprt_qlock_now, __cond_mutex_id,
				__cond_mutex_lock, __cond_mutex_unlock>(&data, mutex, &timeout, condFlag);
	}

	switch (ret) {
	case Status::Done: return 0; break;
	case Status::Ok: return 0; break;
	case Status::Timeout: return ETIMEDOUT; break;
	default: status::toErrno(ret); break;
	}
	return 0;
}

int cond_t::signal() {
	__sprt_sprt_lock_flags_t condFlag = 0;
	if (hasFlag(CondAttrFlags(data.padding), CondAttrFlags::Shared)) {
		condFlag = __SPRT_SPRT_LOCK_FLAG_SHARED;
	}
	if (hasFlag(CondAttrFlags(data.padding), CondAttrFlags::ClockRealtime)) {
		condFlag = __SPRT_SPRT_LOCK_FLAG_CLOCK_REALTIME;
	}

	auto ret = qcondvar_base::_signal<__sprt_sprt_qlock_wake_one>(&data, condFlag);
	return status::toErrno(ret);
}

int cond_t::broadcast() {
	__sprt_sprt_lock_flags_t condFlag = 0;
	if (hasFlag(CondAttrFlags(data.padding), CondAttrFlags::Shared)) {
		condFlag = __SPRT_SPRT_LOCK_FLAG_SHARED;
	}
	if (hasFlag(CondAttrFlags(data.padding), CondAttrFlags::ClockRealtime)) {
		condFlag = __SPRT_SPRT_LOCK_FLAG_CLOCK_REALTIME;
	}

	auto ret = qcondvar_base::_signal<__sprt_sprt_qlock_wake_all>(&data, condFlag);
	return status::toErrno(ret);
}

__SPRT_C_FUNC int __SPRT_ID(pthread_condattr_init)(__SPRT_ID(pthread_condattr_t) * attr) {
	if (!attr) {
		return EINVAL;
	}

	new (reinterpret_cast<_thread::condattr_t *>(attr), nothrow) _thread::condattr_t();
	return 0;
}
__SPRT_C_FUNC int __SPRT_ID(pthread_condattr_destroy)(__SPRT_ID(pthread_condattr_t) * attr) {
	if (!attr) {
		return EINVAL;
	}

	reinterpret_cast<_thread::condattr_t *>(attr)->~condattr_t();
	return 0;
}

__SPRT_C_FUNC int __SPRT_ID(pthread_condattr_setclock)(__SPRT_ID(pthread_condattr_t) * attr,
		__SPRT_ID(clockid_t) clock) {
	if (!attr || !_thread::native::validate_condattr_setclock(clock)) {
		return EINVAL;
	}

	switch (clock) {
	case __SPRT_CLOCK_MONOTONIC:
		reinterpret_cast<_thread::condattr_t *>(attr)->flags &= ~CondAttrFlags::ClockRealtime;
		break;
	case __SPRT_CLOCK_REALTIME:
		reinterpret_cast<_thread::condattr_t *>(attr)->flags |= CondAttrFlags::ClockRealtime;
		break;
	}

	return 0;
}

__SPRT_C_FUNC int __SPRT_ID(
		pthread_condattr_getclock)(const __SPRT_ID(pthread_condattr_t) * __SPRT_RESTRICT attr,
		__SPRT_ID(clockid_t) * __SPRT_RESTRICT clock) {
	if (!attr || !clock) {
		return EINVAL;
	}

	if (hasFlag(reinterpret_cast<const _thread::condattr_t *>(attr)->flags,
				CondAttrFlags::ClockRealtime)) {
		*clock = __SPRT_CLOCK_REALTIME;
	} else {
		*clock = __SPRT_CLOCK_MONOTONIC;
	}
	return 0;
}

__SPRT_C_FUNC int __SPRT_ID(
		pthread_condattr_setpshared)(__SPRT_ID(pthread_condattr_t) * attr, int v) {
	if (!attr || !_thread::native::validate_condattr_setpshared(v)) {
		return EINVAL;
	}

	auto tattr = reinterpret_cast<_thread::condattr_t *>(attr);

	switch (v) {
	case __SPRT_PTHREAD_PROCESS_PRIVATE: tattr->flags &= ~_thread::CondAttrFlags::Shared; break;
	case __SPRT_PTHREAD_PROCESS_SHARED: tattr->flags |= _thread::CondAttrFlags::Shared; break;
	default: return ENOTSUP; break;
	}

	return 0;
}

__SPRT_C_FUNC int __SPRT_ID(pthread_condattr_getpshared)(
		const __SPRT_ID(pthread_condattr_t) * __SPRT_RESTRICT attr, int *__SPRT_RESTRICT ret) {
	if (!attr || !ret) {
		return EINVAL;
	}

	auto tattr = reinterpret_cast<const _thread::condattr_t *>(attr);
	if (hasFlag(tattr->flags, _thread::CondAttrFlags::Shared)) {
		*ret = __SPRT_PTHREAD_PROCESS_SHARED;
	} else {
		*ret = __SPRT_PTHREAD_PROCESS_PRIVATE;
	}
	return 0;
}

__SPRT_C_FUNC int __SPRT_ID(pthread_cond_init)(__SPRT_ID(pthread_cond_t) * __SPRT_RESTRICT cond,
		const __SPRT_ID(pthread_condattr_t) * __SPRT_RESTRICT attr) {
	if (!cond) {
		return EINVAL;
	}

	auto tcond = new (reinterpret_cast<_thread::cond_t *>(cond), nothrow) _thread::cond_t();
	if (attr) {
		tcond->data.padding = toInt(reinterpret_cast<const _thread::condattr_t *>(attr)->flags);
	}
	return 0;
}

__SPRT_C_FUNC int __SPRT_ID(pthread_cond_destroy)(__SPRT_ID(pthread_cond_t) * cond) {
	if (!cond) {
		return EINVAL;
	}

	reinterpret_cast<_thread::cond_t *>(cond)->~cond_t();

	return 0;
}

__SPRT_C_FUNC int __SPRT_ID(pthread_cond_wait)(__SPRT_ID(pthread_cond_t) * __SPRT_RESTRICT cond,
		__SPRT_ID(pthread_mutex_t) * __SPRT_RESTRICT mutex) {
	if (!cond || !mutex) {
		return EINVAL;
	}

	auto mtx = reinterpret_cast<_thread::mutex_t *>(mutex);
	auto tcond = reinterpret_cast<_thread::cond_t *>(cond);

	if (mtx->has_ownedship()) {
		if (!mtx->is_owned_by_this_thread()) {
			return EINVAL;
		}
	} else {
		if (!mtx->is_locked()) {
			return EINVAL;
		}
	}

	return tcond->wait(mtx, __SPRT_SPRT_TIMEOUT_INFINITE);
}

__SPRT_C_FUNC int __SPRT_ID(
		pthread_cond_timedwait)(__SPRT_ID(pthread_cond_t) * __SPRT_RESTRICT cond,
		__SPRT_ID(pthread_mutex_t) * __SPRT_RESTRICT mutex,
		const __SPRT_TIMESPEC_NAME *__SPRT_RESTRICT tv) {
	if (!cond || !mutex || !tv || tv->tv_nsec < 0 || tv->tv_nsec >= 1'000'000'000) {
		return EINVAL;
	}

	auto mtx = reinterpret_cast<_thread::mutex_t *>(mutex);
	auto tcond = reinterpret_cast<_thread::cond_t *>(cond);

	if (mtx->has_ownedship()) {
		if (!mtx->is_owned_by_this_thread()) {
			return EINVAL;
		}
	} else {
		if (!mtx->is_locked()) {
			return EINVAL;
		}
	}

	__SPRT_TIMESPEC_NAME curTv;
	if (hasFlag(CondAttrFlags(tcond->data.padding), CondAttrFlags::ClockRealtime)) {
		__sprt_clock_gettime(__sprt_sprt_qlock_getclock(__SPRT_SPRT_LOCK_FLAG_CLOCK_REALTIME),
				&curTv);
	} else {
		__sprt_clock_gettime(__sprt_sprt_qlock_getclock(0), &curTv);
	}

	auto diffTv = __sprt_timespec_diff(tv, &curTv);

	if (diffTv.tv_sec < 0) {
		return ETIMEDOUT;
	}

	__sprt_sprt_timeout_t nanoTimeout = diffTv.tv_sec * 1'000'000'000 + diffTv.tv_nsec;

	return tcond->wait(mtx, nanoTimeout);
}

SPRT_API int __SPRT_ID(pthread_cond_clockwait)(__SPRT_ID(pthread_cond_t) * __SPRT_RESTRICT cond,
		__SPRT_ID(pthread_mutex_t) * __SPRT_RESTRICT mutex, __SPRT_ID(clockid_t) clock_id,
		const __SPRT_TIMESPEC_NAME *__SPRT_RESTRICT tv) {
	if (!cond || !mutex || !tv || tv->tv_nsec < 0 || tv->tv_nsec >= 1'000'000'000) {
		return EINVAL;
	}

	auto mtx = reinterpret_cast<_thread::mutex_t *>(mutex);
	auto tcond = reinterpret_cast<_thread::cond_t *>(cond);

	if (mtx->has_ownedship()) {
		if (!mtx->is_owned_by_this_thread()) {
			return EINVAL;
		}
	} else {
		if (!mtx->is_locked()) {
			return EINVAL;
		}
	}

	__SPRT_TIMESPEC_NAME curTv;
	if (__sprt_clock_gettime(clock_id, &curTv) != 0) {
		return __sprt_errno;
	}

	auto diffTv = __sprt_timespec_diff(tv, &curTv);

	if (diffTv.tv_sec < 0) {
		return ETIMEDOUT;
	}

	__sprt_sprt_timeout_t nanoTimeout = diffTv.tv_sec * 1'000'000'000 + diffTv.tv_nsec;

	return tcond->wait(mtx, nanoTimeout);
}

__SPRT_C_FUNC int __SPRT_ID(pthread_cond_broadcast)(__SPRT_ID(pthread_cond_t) * cond) {
	if (!cond) {
		return EINVAL;
	}

	return reinterpret_cast<_thread::cond_t *>(cond)->broadcast();
}

__SPRT_C_FUNC int __SPRT_ID(pthread_cond_signal)(__SPRT_ID(pthread_cond_t) * cond) {
	if (!cond) {
		return EINVAL;
	}

	return reinterpret_cast<_thread::cond_t *>(cond)->signal();
}

} // namespace sprt::_thread
