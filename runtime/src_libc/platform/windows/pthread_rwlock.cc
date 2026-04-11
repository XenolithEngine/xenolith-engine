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

static Status _rwMutexWait(volatile uint32_t *value, uint32_t expected, uint64_t timeout) {
	auto ret = WaitOnAddress(value, &expected, sizeof(uint32_t), DWORD(timeout));
	if (!ret) {
		if (GetLastError() == ERROR_TIMEOUT) {
			return Status::Timeout;
		}
	}
	return Status::Ok;
}

static void _rwMutexWake(volatile uint32_t *value) { WakeByAddressSingle((void *)value); }

static void _rwMutexWakeAll(volatile uint32_t *value) { WakeByAddressAll((void *)value); }

int pthread_rwlock_t::rdlock(DWORD dtimeout) {
	auto self = _pthread_t::self();
	if (self->has_wrlock(this)) {
		return EDEADLK;
	}

	uint64_t itimeout = dtimeout;
	// mutexLock will update timeout, and returns false if expired

	if (dtimeout == INFINITE) {
		qmutex_base::_lock<_rwMutexWait, nullptr>(&mutex, nullptr);
	} else {
		if (qmutex_base::_lock<_rwMutexWait, _pthread_t::now>(&mutex, &itimeout)
				== Status::ErrorTimeout) {
			return ETIMEDOUT;
		}
	}

	qmutex_base::_unlock<_rwMutexWakeAll>(&mutex);

	ULONGLONG now = _pthread_t::now(), next = 0;

	uint32_t desired = ReadLock;
	uint32_t expected = 0;

	// We want to set value from 0 to WriteLock
	if (!_atomic::compareSwap(&value, &expected, desired)) [[unlikely]] {
		// failed - should lock

		// Assume that there are other waiters
		desired |= Waiters;
		do {
			// check if read lock already set
			if ((expected & desired) == desired) {
				// read lock is set, return
				break; // from do ... while
			}

			if (itimeout == 0) {
				return ETIMEDOUT;
			}

			// set waiters flag
			if ((expected & Waiters) != 0
					|| ((expected = _atomic::fetchOr(&value, Waiters) | Waiters) & ~Waiters) != 0) {
				// we successfully set waiters flag (or it was set before)
				if (!WaitOnAddress(&value, &expected, sizeof(uint64_t), DWORD(itimeout))) {
					if (GetLastError() == ERROR_TIMEOUT) {
						return ETIMEDOUT;
					}
				}

				if (itimeout != INFINITE) {
					next = _pthread_t::now();
					itimeout -= min((next - now), itimeout);
				}
				now = next;
			}

			// value was changed, now it's 0 or other lock value
			// assume 0 and try to set lock with WriteLock
			expected = 0;
		} while (!_atomic::compareSwap(&value, &expected, desired));
	}

	// we now read-locked
	_atomic::fetchAdd(&counter, uint32_t(1));

	auto it = self->threadRdLocks->find(this);
	if (it != self->threadRdLocks->end()) {
		++it->second;
	} else {
		self->threadRdLocks->emplace(this, 1);
	}

	return 0;
}

int pthread_rwlock_t::tryrdlock() {
	auto self = _pthread_t::self();
	if (self->has_wrlock(this)) {
		return EDEADLK;
	}

	uint32_t desired = ReadLock;
	uint32_t expected = 0;

	if (qmutex_base::_try_lock(&mutex) == Status::ErrorBusy) {
		return EBUSY;
	}

	qmutex_base::_unlock<_rwMutexWakeAll>(&mutex);

	// We want to set value from 0 to WriteLock
	if (!_atomic::compareSwap(&value, &expected, desired)) [[unlikely]] {
		// check if read lock already set
		if ((expected & desired) != desired) {
			// read lock is NOT set, return with error
			return EBUSY;
		}
	}

	// we now read-locked
	_atomic::fetchAdd(&counter, uint32_t(1));

	auto it = self->threadRdLocks->find(this);
	if (it != self->threadRdLocks->end()) {
		++it->second;
	} else {
		self->threadRdLocks->emplace(this, 1);
	}

	return 0;
}

int pthread_rwlock_t::wrlock(DWORD dtimeout) {
	auto self = _pthread_t::self();
	if (self->has_wrlock(this)) {
		return EDEADLK;
	}

	ULONGLONG now = _pthread_t::now(), next = 0;

	uint32_t desired = WriteLock;
	uint32_t expected = 0;

	uint64_t itimeout = dtimeout;

	if (dtimeout == INFINITE) {
		qmutex_base::_lock<_rwMutexWait, nullptr>(&mutex, nullptr);
	} else {
		if (qmutex_base::_lock<_rwMutexWait, _pthread_t::now>(&mutex, &itimeout)
				== Status::ErrorTimeout) {
			return ETIMEDOUT;
		}
	}

	// We want to set value from 0 to WriteLock
	if (!_atomic::compareSwap(&value, &expected, desired)) [[unlikely]] {
		// failed - should lock

		// Assume that there are other waiters
		desired |= Waiters;
		do {
			if (itimeout == 0) {
				qmutex_base::_unlock<_rwMutexWakeAll>(&mutex);
				return ETIMEDOUT;
			}

			// set waiters flag
			if ((expected & Waiters) != 0
					|| ((expected = _atomic::fetchOr(&value, Waiters) | Waiters) & ~Waiters) != 0) {

				// we successfully set waiters flag (or it was set before)
				if (!WaitOnAddress(&value, &expected, sizeof(uint64_t), DWORD(itimeout))) {
					if (GetLastError() == ERROR_TIMEOUT) {
						qmutex_base::_unlock<_rwMutexWakeAll>(&mutex);
						return ETIMEDOUT;
					}
				}

				if (dtimeout != INFINITE) {
					next = _pthread_t::now();
					itimeout -= min((next - now), itimeout);
				}
				now = next;
			}

			// value was changed, now it's 0 or other lock value
			// assume 0 and try to set lock with WriteLock
			expected = 0;
		} while (!_atomic::compareSwap(&value, &expected, desired));
	}

	_atomic::fetchAdd(&counter, uint32_t(1));

	// we now write-locked
	self->threadWrLocks->emplace(this);

	return 0;
}

int pthread_rwlock_t::trywrlock() {
	auto self = _pthread_t::self();
	if (self->has_wrlock(this)) {
		return EDEADLK;
	}

	uint32_t desired = WriteLock;
	uint32_t expected = 0;

	if (qmutex_base::_try_lock(&mutex) == Status::ErrorBusy) {
		return EBUSY;
	}

	// We want to set value from 0 to WriteLock
	if (!_atomic::compareSwap(&value, &expected, desired)) [[unlikely]] {
		qmutex_base::_unlock<_rwMutexWakeAll>(&mutex);
		return EBUSY;
	}

	_atomic::fetchAdd(&counter, uint32_t(1));

	// we now write-locked
	self->threadWrLocks->emplace(this);

	return 0;
}

int pthread_rwlock_t::unlock() {
	auto self = _pthread_t::self();
	auto rd_it = self->threadRdLocks->find(this);
	auto wr_it = self->threadWrLocks->find(this);
	auto has_rd = rd_it != self->threadRdLocks->end();
	auto has_wr = wr_it != self->threadWrLocks->end();

	if (!has_rd && !has_wr) {
		return EPERM;
	}

	bool shouldUnlock = (_atomic::fetchSub(&counter, uint32_t(1)) == 1);
	if (!shouldUnlock) {
		if (has_rd) {
			if (rd_it->second > 1) {
				--rd_it->second;
			} else {
				self->threadRdLocks->erase(rd_it);
			}
		}
		return 0;
	}

	auto v = _atomic::exchange(&value, uint32_t(0));
	if (v & Waiters) {
		WakeByAddressAll(&value);
	}

	if (has_rd) {
		self->threadRdLocks->erase(rd_it);
	}
	if (has_wr) {
		qmutex_base::_unlock<_rwMutexWakeAll>(&mutex);
		self->threadWrLocks->erase(wr_it);
	}

	return 0;
}

void pthread_rwlock_t::force_unlock(bool isReadLock) {
	bool shouldUnlock = (_atomic::fetchSub(&counter, uint32_t(1)) == 1);
	if (!shouldUnlock) {
		return;
	}

	auto v = _atomic::exchange(&value, uint32_t(0));
	if (v & Waiters) {
		WakeByAddressAll(&value);
	}

	if (!isReadLock) {
		qmutex_base::_unlock<_rwMutexWakeAll>(&mutex);
	}
}

static int pthread_rwlockattr_init(pthread_rwlockattr_t *attr) {
	new (attr, nothrow) pthread_rwlockattr_t();
	return 0;
}

static int pthread_rwlockattr_destroy(pthread_rwlockattr_t *attr) {
	if (!attr) {
		return EINVAL;
	}

	attr->~pthread_rwlockattr_t();
	return 0;
}

static int pthread_rwlockattr_setpshared(pthread_rwlockattr_t *attr, int v) {
	if (!attr) {
		return EINVAL;
	}

	switch (v) {
	case __SPRT_PTHREAD_PROCESS_PRIVATE: break;
	default: return ENOTSUP; break;
	}

	return 0;
}

static int pthread_rwlockattr_getpshared(const pthread_rwlockattr_t *__SPRT_RESTRICT attr,
		int *__SPRT_RESTRICT ret) {
	if (!attr || !ret) {
		return EINVAL;
	}

	*ret = __SPRT_PTHREAD_PROCESS_PRIVATE;
	return 0;
}

} // namespace sprt
