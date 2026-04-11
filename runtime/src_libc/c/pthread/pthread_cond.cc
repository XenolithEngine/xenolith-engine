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

int cond_t::wait(mutex_t *mutex, __sprt_sprt_timeout_t timeout) {
	uint64_t desired = static_cast<uint64_t>(bit_cast<uintptr_t>(mutex));
	uint64_t expected = 0;
	if (_atomic::compareSwap(&mutexid, &expected, desired)) {
		// condition captured by new mutex
		_atomic::fetchAdd(&counter, uint32_t(1));
	} else if (expected == desired) {
		// comdition was captured by this mutex
		_atomic::fetchAdd(&counter, uint32_t(1));
	} else {
		// captured by different mutex
		return EINVAL;
	}

	uint32_t v = _atomic::loadSeq(&value);
	_atomic::storeSeq(&previous, v);

	int result = 0;

	__sprt_sprt_lock_flags_t condFlag = 0;
	if (hasFlag(flags, CondAttrFlags::Shared)) {
		condFlag = __SPRT_SPRT_LOCK_FLAG_SHARED;
	}
	if (hasFlag(flags, CondAttrFlags::ClockRealtime)) {
		condFlag = __SPRT_SPRT_LOCK_FLAG_CLOCK_REALTIME;
	}

	__sprt_sprt_timeout_t now =
			(timeout == __SPRT_SPRT_TIMEOUT_INFINITE) ? 0 : __sprt_sprt_qlock_now(condFlag);
	__sprt_sprt_timeout_t next = 0;

	mutex->unlock();
	while (v == _atomic::loadSeq(&value)) {
		if (timeout == 0) {
			result = ETIMEDOUT;
			break;
		}

		result = __sprt_sprt_qlock_wait(&value, v, timeout, condFlag);
		if (result != 0) {
			if (__sprt_errno == ETIMEDOUT) {
				result = ETIMEDOUT;
				break;
			}
		}

		if (timeout != __SPRT_SPRT_TIMEOUT_INFINITE) {
			next = __sprt_sprt_qlock_now(condFlag);
			timeout -= min((next - now), timeout);
			now = next;
		}
	}
	result = mutex->lock(__SPRT_SPRT_TIMEOUT_INFINITE);

	if (result == 0) {
		if (_atomic::fetchSub(&counter, 1U) == 1) {
			_atomic::storeSeq(&mutexid, uint64_t(0));
		}
	}
	return result;
}

int cond_t::signal() {
	uint64_t mid = _atomic::loadSeq(&mutexid);
	if (mid == 0) {
		// no waiters
		return 0;
	}

	__sprt_sprt_lock_flags_t __condFlags = 0;
	if (hasFlag(flags, CondAttrFlags::Shared)) {
		__condFlags |= __SPRT_SPRT_LOCK_FLAG_SHARED;
	}

	uint32_t v = 1u + _atomic::loadSeq(&previous);
	_atomic::storeSeq(&value, v);
	__sprt_sprt_qlock_wake_one(&value, __condFlags);
	return 0;
}

int cond_t::broadcast() {
	uint64_t mid = _atomic::loadSeq(&mutexid);
	if (mid == 0) {
		// no waiters
		return 0;
	}

	__sprt_sprt_lock_flags_t __condFlags = 0;
	if (hasFlag(flags, CondAttrFlags::Shared)) {
		__condFlags |= __SPRT_SPRT_LOCK_FLAG_SHARED;
	}

	uint32_t v = 1u + _atomic::loadSeq(&previous);
	_atomic::storeSeq(&value, v);
	__sprt_sprt_qlock_wake_all(&value, __condFlags);
	return 0;
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
		tcond->flags = reinterpret_cast<const _thread::condattr_t *>(attr)->flags;
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
	if (hasFlag(tcond->flags, CondAttrFlags::ClockRealtime)) {
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
