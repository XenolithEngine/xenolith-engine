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

#include "private/SPRTAtomics.h"

#ifdef __clang__
#pragma clang diagnostic ignored "-Wunused-function"
#endif

namespace sprt {

int pthread_mutex_t::lock(DWORD timeout) {
	uint64_t desired = static_cast<uint64_t>(GetCurrentThreadId());
	uint64_t expected = 0;

	ULONGLONG now = _pthread_t::now(), next = 0;

	// We want to set value from 0 to thread_id
	if (!atomicCompareSwap(&value, &expected, desired)) [[unlikely]] {
		// failed - should lock
		do {
			if ((expected & pthread_mutex_t::StateNotRecoverable) != 0) {
				// we are the owner, increment and return

				return ENOTRECOVERABLE;
			}

			// check if we are actually owner, but with wait flag set
			if ((expected & pthread_mutex_t::StateThreadMask) == desired) {
				// we are the owner, return
				break; // from do ... while
			}

			if (timeout == 0) {
				return ETIMEDOUT;
			}

			// set waiters flag
			if ((expected & pthread_mutex_t::StateWaiters) != 0
					// expected should be (fetch | StateWaiters), then we check it's value part against nonzero
					|| ((expected = atomicFetchOr(&value, pthread_mutex_t::StateWaiters)
										| pthread_mutex_t::StateWaiters)
							   & StateThreadMask)
							!= 0) {
				// we successfully set waiters flag (or it was set before)
				if (!WaitOnAddress(&value, &expected, sizeof(uint64_t), timeout)) {
					if (GetLastError() == ERROR_TIMEOUT) {
						return ETIMEDOUT;
					}
				}

				if (timeout != INFINITE) {
					next = _pthread_t::now();
					timeout -= min((next - now), timeout);
				}
				now = next;

				// value was changed, now it's 0 or other thread id
				// assume 0 and try to set to our thread id
				expected = 0;
			} else {
				// we have 0 or 0 | StateWaiters in expected, try to CompareSwap with it
			}

		} while (!atomicCompareSwap(&value, &expected, desired));
	}

	// value is now thread_id, we owns mmutex
	++counter;

	if (counter == 1 && hasFlag(attr.flags, MutexAttrFlags::Robust)) {
		_pthread_t::self()->threadRobustMutexes->emplace(this);
	}

	if (hasFlag(attr.flags, MutexAttrFlags::OwnerDied)) {
		return EOWNERDEAD;
	}
	return 0;
}

int pthread_mutex_t::unlock() {
	uint64_t desired = static_cast<uint64_t>(GetCurrentThreadId());
	uint64_t expected = atomicLoadSeq(&expected);

	if ((expected & pthread_mutex_t::StateThreadMask) != desired) {
		// Mutex is not ours

		return EPERM;
	}

	--counter;
	if (--counter > 0) {
		// some recursive locks still in place
		return 0;
	}

	if (hasFlag(attr.flags, MutexAttrFlags::Robust)) {
		_pthread_t::self()->threadRobustMutexes->erase(this);
	}

	if (hasFlag(attr.flags, MutexAttrFlags::OwnerDied)) {
		// make mutex inconsistent and notify all about it
		attr.flags |= MutexAttrFlags::NotRecoverable;
		atomicFetchOr(&value, pthread_mutex_t::StateNotRecoverable);
		WakeByAddressAll(&value);

		return ENOTRECOVERABLE;
	} else if ((expected & pthread_mutex_t::StateWaiters) != 0) {
		// we know there are waiters, unlock via syscall
		atomicStoreSeq(&value, uint64_t(0));
		WakeByAddressSingle(&value);
	} else {
		// try to release atomically
		if (!atomicCompareSwap(&value, &expected, uint64_t(0))) {
			// there are new waiters or an error, we don't care what - just unlock with syscall
			atomicStoreSeq(&value, uint64_t(0));
			WakeByAddressSingle(&value);
		} else {
			// successful, no waiters
		}
	}
	return 0;
}

int pthread_mutex_t::try_lock() {
	uint64_t desired = static_cast<uint64_t>(GetCurrentThreadId());
	uint64_t expected = 0;

	if (!atomicCompareSwap(&value, &expected, desired)) [[unlikely]] {
		// check if we are the owner
		if ((expected & pthread_mutex_t::StateNotRecoverable) != 0) {
			// we are the owner, increment and return
			return ENOTRECOVERABLE;
		}

		if ((expected & pthread_mutex_t::StateThreadMask) == desired) {
			// we are the owner, increment and return
			++counter;
		} else {
			return EBUSY;
		}
	} else {
		++counter;
	}

	if (hasFlag(attr.flags, MutexAttrFlags::Robust)) {
		_pthread_t::self()->threadRobustMutexes->emplace(this);
	}

	if (hasFlag(attr.flags, MutexAttrFlags::OwnerDied)) {
		return EOWNERDEAD;
	}
	return 0;
}

void pthread_mutex_t::force_unlock() {
	uint64_t desired = static_cast<uint64_t>(GetCurrentThreadId());
	uint64_t expected = atomicLoadSeq(&value);

	// ensure that we are owner
	if ((expected & pthread_mutex_t::StateThreadMask) == desired) {
		attr.flags |= MutexAttrFlags::OwnerDied;

		// remove all recursive locks
		counter = 0;
		if ((expected & pthread_mutex_t::StateWaiters) != 0) {
			// we know there are waiters, unlock via syscall
			atomicStoreSeq(&value, uint64_t(0));
			WakeByAddressSingle(&value);
		} else {
			// try to release atomically
			if (!atomicCompareSwap(&value, &expected, uint64_t(0))) {
				// there are new waiters or an error, we don't care what - just unlock with syscall
				atomicStoreSeq(&value, uint64_t(0));
				WakeByAddressSingle(&value);
			} else {
				// successful, no waiters
			}
		}
	}
}

bool pthread_mutex_t::is_owned_by_this_thread() {
	uint64_t desired = static_cast<uint64_t>(GetCurrentThreadId());
	uint64_t expected = atomicLoadSeq(&value);

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

	new (mutex, nothrow) pthread_mutex_t{*attr};
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

	auto flags = MutexAttrFlags(atomicLoadSeq((uint16_t *)&mutex->attr.flags));
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
