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

#include "pthread_win.h"

#ifdef __clang__
#pragma clang diagnostic ignored "-Wunused-function"
#endif

namespace sprt {

static Status _mutexLock(volatile uint64_t *value, uint64_t *expected, uint64_t timeout) {
	WaitOnAddress(value, expected, sizeof(uint64_t), INFINITE);
	return Status::Ok;
}

static Status _mutexTimedLock(volatile uint64_t *value, uint64_t *expected, uint64_t timeout) {
	if (!WaitOnAddress(value, expected, sizeof(uint64_t), timeout)) {
		if (GetLastError() == ERROR_TIMEOUT) {
			return Status::ErrorTimeout;
		}
	}
	return Status::Ok;
}

static void _mutexUnlock(volatile uint64_t *value) { WakeByAddressSingle((void *)value); }

int pthread_mutex_t::lock(DWORD timeout) {
	uint64_t itimeout = timeout;
	Status st = Status::Ok;
	if (timeout == INFINITE) {
		st = rmutex_base::_lock<_mutexLock, nullptr, false>(mtx,
				static_cast<uint64_t>(GetCurrentThreadId()), nullptr);
	} else {
		st = rmutex_base::_lock<_mutexTimedLock, _pthread_t::now, false>(mtx,
				static_cast<uint64_t>(GetCurrentThreadId()), &itimeout);
	}

	if (st == Status::Ok) {
		if (mtx.counter == 1 && hasFlag(attr.flags, MutexAttrFlags::Robust)) {
			_pthread_t::self()->threadRobustMutexes->emplace(this);
		}
		if (hasFlag(attr.flags, MutexAttrFlags::OwnerDied)) {
			return EOWNERDEAD;
		}
		return 0;
	}

	return status::toErrno(st);
}

int pthread_mutex_t::unlock() {
	if (hasFlag(attr.flags, MutexAttrFlags::OwnerDied)) {
		auto expected = _atomic::loadRel(&mtx.value);
		if ((expected & rmutex_base::VALUE_MASK) != GetCurrentThreadId() || mtx.counter <= 0) {
			return EPERM;
		}

		// make mutex inconsistent and notify all about it
		attr.flags |= MutexAttrFlags::NotRecoverable;
		_atomic::fetchOr(&mtx.value, pthread_mutex_t::StateNotRecoverable);
		WakeByAddressAll(&mtx.value);
		return ENOTRECOVERABLE;
	}

	auto st = rmutex_base::_unlock<_mutexUnlock>(mtx, GetCurrentThreadId());

	if (status::isSuccessful(st)) {
		if (st != Status::Suspended) {
			if (hasFlag(attr.flags, MutexAttrFlags::Robust)) {
				_pthread_t::self()->threadRobustMutexes->erase(this);
			}
		}
		return 0;
	}

	return status::toErrno(st);
}

int pthread_mutex_t::try_lock() {
	auto st = rmutex_base::_try_lock<nullptr>(mtx, GetCurrentThreadId());

	return status::toErrno(st);
}

void pthread_mutex_t::force_unlock() {
	uint64_t desired = static_cast<uint64_t>(GetCurrentThreadId());
	uint64_t expected = _atomic::loadSeq(&mtx.value);

	// ensure that we are owner
	if ((expected & pthread_mutex_t::StateThreadMask) == desired) {
		attr.flags |= MutexAttrFlags::OwnerDied;

		// remove all recursive locks
		mtx.counter = 0;
		if ((expected & pthread_mutex_t::StateWaiters) != 0) {
			// we know there are waiters, unlock via syscall
			_atomic::storeSeq(&mtx.value, uint64_t(0));
			WakeByAddressSingle(&mtx.value);
		} else {
			// try to release atomically
			if (!_atomic::compareSwap(&mtx.value, &expected, uint64_t(0))) {
				// there are new waiters or an error, we don't care what - just unlock with syscall
				_atomic::storeSeq(&mtx.value, uint64_t(0));
				WakeByAddressSingle(&mtx.value);
			} else {
				// successful, no waiters
			}
		}
	}
}

bool pthread_mutex_t::is_owned_by_this_thread() {
	uint64_t desired = static_cast<uint64_t>(GetCurrentThreadId());
	uint64_t expected = _atomic::loadSeq(&mtx.value);

	// ensure that we are owner
	if ((expected & pthread_mutex_t::StateThreadMask) == desired) {
		return true;
	}
	return false;
}

static int pthread_mutexattr_init(pthread_mutexattr_t *attr) {
	new (attr, nothrow) pthread_mutexattr_t;
	return 0;
}

static int pthread_mutexattr_destroy(pthread_mutexattr_t *attr) {
	if (!attr) {
		return EINVAL;
	}

	attr->~pthread_mutexattr_t();
	return 0;
}

static int pthread_mutexattr_getprioceiling(const pthread_mutexattr_t *__SPRT_RESTRICT attr,
		int *__SPRT_RESTRICT ret) {
	if (!attr || !ret) {
		return EINVAL;
	}

	*ret = attr->prio;
	return 0;
}

static int pthread_mutexattr_setprioceiling(pthread_mutexattr_t *attr, int v) {
	if (!attr) {
		return EINVAL;
	}

	if (v < __SPRT_SCHED_PRIO_MIN) {
		attr->prio = __SPRT_SCHED_PRIO_MIN;
	} else if (v > __SPRT_SCHED_PRIO_MAX) {
		attr->prio = __SPRT_SCHED_PRIO_MAX;
	} else {
		attr->prio = v;
	}
	return 0;
}

static int pthread_mutexattr_getprotocol(const pthread_mutexattr_t *__SPRT_RESTRICT attr,
		int *__SPRT_RESTRICT ret) {
	if (!attr || !ret) {
		return EINVAL;
	}

	*ret = __SPRT_PTHREAD_PRIO_NONE;
	return 0;
}

static int pthread_mutexattr_setprotocol(pthread_mutexattr_t *attr, int v) {
	if (!attr) {
		return EINVAL;
	}

	switch (v) {
	case __SPRT_PTHREAD_PRIO_NONE: break;
	default: return ENOTSUP; break;
	}

	return 0;
}

static int pthread_mutexattr_getrobust(const pthread_mutexattr_t *__SPRT_RESTRICT attr,
		int *__SPRT_RESTRICT ret) {
	if (!attr || !ret) {
		return EINVAL;
	}

	if (hasFlag(attr->flags, MutexAttrFlags::Robust)) {
		*ret = __SPRT_PTHREAD_MUTEX_ROBUST;
	} else {
		*ret = __SPRT_PTHREAD_MUTEX_STALLED;
	}
	return 0;
}

static int pthread_mutexattr_setrobust(pthread_mutexattr_t *attr, int v) {
	if (!attr) {
		return EINVAL;
	}

	switch (v) {
	case __SPRT_PTHREAD_MUTEX_STALLED: attr->flags &= ~MutexAttrFlags::Robust; break;
	case __SPRT_PTHREAD_MUTEX_ROBUST: attr->flags |= MutexAttrFlags::Robust; break;
	default: return ENOTSUP; break;
	}

	return 0;
}

static int pthread_mutexattr_getpshared(const pthread_mutexattr_t *__SPRT_RESTRICT attr,
		int *__SPRT_RESTRICT ret) {
	if (!attr || !ret) {
		return EINVAL;
	}

	*ret = __SPRT_PTHREAD_PROCESS_PRIVATE;
	return 0;
}

static int pthread_mutexattr_setpshared(pthread_mutexattr_t *attr, int v) {
	if (!attr) {
		return EINVAL;
	}

	switch (v) {
	case __SPRT_PTHREAD_PROCESS_PRIVATE: break;
	default: return ENOTSUP; break;
	}

	return 0;
}

static int pthread_mutexattr_gettype(const pthread_mutexattr_t *__SPRT_RESTRICT attr,
		int *__SPRT_RESTRICT ret) {
	if (!attr || !ret) {
		return EINVAL;
	}

	*ret = __SPRT_PTHREAD_MUTEX_RECURSIVE;
	return 0;
}

static int pthread_mutexattr_settype(pthread_mutexattr_t *attr, int v) {
	if (!attr) {
		return EINVAL;
	}

	switch (v) {
	case __SPRT_PTHREAD_MUTEX_RECURSIVE: break;
	case __SPRT_PTHREAD_MUTEX_DEFAULT: break;
	default: return ENOTSUP; break;
	}

	return 0;
}

static int pthread_mutex_init(pthread_mutex_t *__SPRT_RESTRICT mutex,
		const pthread_mutexattr_t *__SPRT_RESTRICT attr) {
	if (!mutex) {
		return EINVAL;
	}

	new (mutex, nothrow) pthread_mutex_t{attr ? *attr : pthread_mutexattr_t()};
	return 0;
}

static int pthread_mutex_destroy(pthread_mutex_t *mutex) {
	if (!mutex) {
		return EINVAL;
	}

	mutex->~pthread_mutex_t();
	return 0;
}

static int pthread_mutex_lock(pthread_mutex_t *mutex) {
	if (!mutex || hasFlag(mutex->attr.flags, MutexAttrFlags::NotRecoverable)) {
		return EINVAL;
	}

	return mutex->lock(INFINITE);
}

static int pthread_mutex_unlock(pthread_mutex_t *mutex) {
	if (!mutex || hasFlag(mutex->attr.flags, MutexAttrFlags::NotRecoverable)) {

		return EINVAL;
	}

	return mutex->unlock();
}

static int pthread_mutex_trylock(pthread_mutex_t *mutex) {
	if (!mutex || hasFlag(mutex->attr.flags, MutexAttrFlags::NotRecoverable)) {

		return EINVAL;
	}

	return mutex->try_lock();
}

static int pthread_mutex_timedlock(pthread_mutex_t *__SPRT_RESTRICT mutex,
		const __SPRT_TIMESPEC_NAME *__SPRT_RESTRICT tv) {
	if (!mutex || hasFlag(mutex->attr.flags, MutexAttrFlags::NotRecoverable) || !tv
			|| tv->tv_nsec < 0 || tv->tv_nsec >= 1'000'000'000) {

		return EINVAL;
	}

	__SPRT_TIMESPEC_NAME curTv;
	__sprt_clock_gettime(__SPRT_CLOCK_REALTIME, &curTv);

	auto diffTv = __sprt_timespec_diff(tv, &curTv);

	if (diffTv.tv_sec < 0) {
		return ETIMEDOUT;
	}

	DWORD millis = diffTv.tv_sec * 1'000 + diffTv.tv_nsec / 1'000'000;
	return mutex->lock(millis ? millis : 1);
}

static int pthread_mutex_consistent(pthread_mutex_t *mutex) {
	if (!mutex || !hasFlag(mutex->attr.flags, MutexAttrFlags::Robust)
			|| hasFlag(mutex->attr.flags, MutexAttrFlags::NotRecoverable)) {

		return EINVAL;
	}

	auto flags = MutexAttrFlags(_atomic::loadSeq((uint16_t *)&mutex->attr.flags));
	if (!hasFlag(flags, MutexAttrFlags::OwnerDied)) {
		return EINVAL;
	}

	mutex->attr.flags &= ~MutexAttrFlags::OwnerDied;

	return 0;
}

static int pthread_mutex_getprioceiling(const pthread_mutex_t *__SPRT_RESTRICT mutex,
		int *__SPRT_RESTRICT p) {
	if (!mutex || !p) {
		return EINVAL;
	}

	*p = mutex->attr.prio;
	return 0;
}

static int pthread_mutex_setprioceiling(pthread_mutex_t *__SPRT_RESTRICT mutex, int v,
		int *__SPRT_RESTRICT p) {
	if (!mutex) {
		return EINVAL;
	}

	if (v < __SPRT_SCHED_PRIO_MIN) {
		mutex->attr.prio = __SPRT_SCHED_PRIO_MIN;
	} else if (v > __SPRT_SCHED_PRIO_MAX) {
		mutex->attr.prio = __SPRT_SCHED_PRIO_MAX;
	} else {
		mutex->attr.prio = v;
	}
	return 0;
}

} // namespace sprt
