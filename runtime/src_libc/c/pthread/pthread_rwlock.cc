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

rwlock_t::rwlock_t() {
	if (__sprt_sprt_qlock_supports(__SPRT_SPRT_LOCK_FLAG_CLOCK_REALTIME)) {
		flags |= RwlockAttrFlags::ClockRealtime;
	}
}

int rwlock_t::rdlock(timeout_t dtimeout) {
	__sprt_sprt_lock_flags_t fl = 0;
	if (hasFlag(flags, RwlockAttrFlags::Shared)) {
		fl |= __SPRT_SPRT_LOCK_FLAG_SHARED;
	}
	if (hasFlag(flags, RwlockAttrFlags::ClockRealtime)) {
		fl |= __SPRT_SPRT_LOCK_FLAG_CLOCK_REALTIME;
	}

	auto self = thread_t::self();
	if (self->has_wrlock(this)) {
		return EDEADLK;
	}

	Status st = Status::Ok;
	auto itimeout = dtimeout;

	// do lock-unlock cycle on mutex
	// this mutex prevents reader to lock if there are writer waiting
	if (dtimeout == Infinite) {
		st = qrwlock_base::_rdlock_fair<__sprt_sprt_qlock_wait, __sprt_sprt_qlock_wake_one,
				nullptr>(&data, nullptr, fl);
	} else {
		st = qrwlock_base::_rdlock_fair<__sprt_sprt_qlock_wait, __sprt_sprt_qlock_wake_one,
				__sprt_sprt_qlock_now>(&data, &itimeout, fl);
	}

	if (st == Status::Timeout || dtimeout == 0) {
		return ETIMEDOUT;
	} else if (!status::isSuccessful(st)) {
		return status::toErrno(st);
	}

	// we now read-locked
	auto it = self->threadRdLocks->find(this);
	if (it != self->threadRdLocks->end()) {
		++it->second;
	} else {
		self->threadRdLocks->emplace(this, 1);
	}

	return 0;
}

int rwlock_t::tryrdlock() {
	__sprt_sprt_lock_flags_t fl = 0;
	if (hasFlag(flags, RwlockAttrFlags::Shared)) {
		fl |= __SPRT_SPRT_LOCK_FLAG_SHARED;
	}
	if (hasFlag(flags, RwlockAttrFlags::ClockRealtime)) {
		fl |= __SPRT_SPRT_LOCK_FLAG_CLOCK_REALTIME;
	}

	auto self = thread_t::self();
	if (self->has_wrlock(this)) {
		return EDEADLK;
	}

	auto st = qrwlock_base::_tryrdlock_fair<__sprt_sprt_qlock_wake_one>(&data, fl);
	if (st == Status::ErrorBusy) {
		return EBUSY;
	} else if (!status::isSuccessful(st)) {
		return status::toErrno(st);
	}

	// we now read-locked

	auto it = self->threadRdLocks->find(this);
	if (it != self->threadRdLocks->end()) {
		++it->second;
	} else {
		self->threadRdLocks->emplace(this, 1);
	}

	return 0;
}

int rwlock_t::wrlock(timeout_t dtimeout) {
	__sprt_sprt_lock_flags_t fl = 0;
	if (hasFlag(flags, RwlockAttrFlags::Shared)) {
		fl |= __SPRT_SPRT_LOCK_FLAG_SHARED;
	}
	if (hasFlag(flags, RwlockAttrFlags::ClockRealtime)) {
		fl |= __SPRT_SPRT_LOCK_FLAG_CLOCK_REALTIME;
	}

	auto self = thread_t::self();
	if (self->has_wrlock(this)) {
		return EDEADLK;
	}

	Status st = Status::Ok;
	auto itimeout = dtimeout;

	// do lock-unlock cycle on mutex
	// this mutex prevents reader to lock if there are writer waiting
	if (dtimeout == Infinite) {
		st = qrwlock_base::_wrlock_fair<__sprt_sprt_qlock_wait, __sprt_sprt_qlock_wake_one,
				nullptr>(&data, nullptr, fl);
	} else {
		st = qrwlock_base::_wrlock_fair<__sprt_sprt_qlock_wait, __sprt_sprt_qlock_wake_one,
				__sprt_sprt_qlock_now>(&data, &itimeout, fl);
	}

	if (st == Status::Timeout || dtimeout == 0) {
		return ETIMEDOUT;
	} else if (!status::isSuccessful(st)) {
		return status::toErrno(st);
	}

	// we now write-locked
	self->threadWrLocks->emplace(this);

	return 0;
}

int rwlock_t::trywrlock() {
	__sprt_sprt_lock_flags_t fl = 0;
	if (hasFlag(flags, RwlockAttrFlags::Shared)) {
		fl |= __SPRT_SPRT_LOCK_FLAG_SHARED;
	}
	if (hasFlag(flags, RwlockAttrFlags::ClockRealtime)) {
		fl |= __SPRT_SPRT_LOCK_FLAG_CLOCK_REALTIME;
	}

	auto self = thread_t::self();
	if (self->has_wrlock(this)) {
		return EDEADLK;
	}

	auto st = qrwlock_base::_trywrlock_fair<__sprt_sprt_qlock_wake_one>(&data, fl);
	if (st == Status::ErrorBusy) {
		return EBUSY;
	} else if (!status::isSuccessful(st)) {
		return status::toErrno(st);
	}

	// we now write-locked
	self->threadWrLocks->emplace(this);

	return 0;
}

int rwlock_t::unlock() {
	__sprt_sprt_lock_flags_t fl = 0;
	if (hasFlag(flags, RwlockAttrFlags::Shared)) {
		fl |= __SPRT_SPRT_LOCK_FLAG_SHARED;
	}
	if (hasFlag(flags, RwlockAttrFlags::ClockRealtime)) {
		fl |= __SPRT_SPRT_LOCK_FLAG_CLOCK_REALTIME;
	}

	auto self = thread_t::self();
	auto rd_it = self->threadRdLocks->find(this);
	auto wr_it = self->threadWrLocks->find(this);
	auto has_rd = rd_it != self->threadRdLocks->end();
	auto has_wr = wr_it != self->threadWrLocks->end();

	if (!has_rd && !has_wr) {
		return EPERM;
	}

	auto st = qrwlock_base::_unlock_fair<__sprt_sprt_qlock_wake_one>(&data, fl);
	if (st == Status::Propagate) {
		if (has_rd) {
			if (rd_it->second > 1) {
				--rd_it->second;
			} else {
				self->threadRdLocks->erase(rd_it);
			}
		}
		return 0;
	} else if (!status::isSuccessful(st)) {
		return status::toErrno(st);
	}

	if (has_rd) {
		self->threadRdLocks->erase(rd_it);
	}
	if (has_wr) {
		self->threadWrLocks->erase(wr_it);
	}

	return 0;
}

void rwlock_t::force_unlock(bool isReadLock) {
	__sprt_sprt_lock_flags_t fl = 0;
	if (hasFlag(flags, RwlockAttrFlags::Shared)) {
		fl |= __SPRT_SPRT_LOCK_FLAG_SHARED;
	}
	if (hasFlag(flags, RwlockAttrFlags::ClockRealtime)) {
		fl |= __SPRT_SPRT_LOCK_FLAG_CLOCK_REALTIME;
	}

	qrwlock_base::_unlock_fair<__sprt_sprt_qlock_wake_one>(&data, fl);
}


__SPRT_C_FUNC int __SPRT_ID(pthread_rwlockattr_init)(__SPRT_ID(pthread_rwlockattr_t) * attr) {
	if (!attr) {
		return EINVAL;
	}

	new (reinterpret_cast<_thread::rwlockattr_t *>(attr), nothrow) _thread::rwlockattr_t();
	return 0;
}

__SPRT_C_FUNC int __SPRT_ID(pthread_rwlockattr_destroy)(__SPRT_ID(pthread_rwlockattr_t) * attr) {
	if (!attr) {
		return EINVAL;
	}

	reinterpret_cast<_thread::rwlockattr_t *>(attr)->~rwlockattr_t();
	return 0;
}

__SPRT_C_FUNC int __SPRT_ID(
		pthread_rwlockattr_setpshared)(__SPRT_ID(pthread_rwlockattr_t) * attr, int v) {
	if (!attr || !_thread::native::validate_rwlockattr_setpshared(v)) {
		return EINVAL;
	}

	auto tattr = reinterpret_cast<_thread::rwlockattr_t *>(attr);

	switch (v) {
	case __SPRT_PTHREAD_PROCESS_PRIVATE: tattr->flags &= ~_thread::RwlockAttrFlags::Shared; break;
	case __SPRT_PTHREAD_PROCESS_SHARED: tattr->flags |= _thread::RwlockAttrFlags::Shared; break;
	default: return ENOTSUP; break;
	}

	return 0;
}

__SPRT_C_FUNC int __SPRT_ID(pthread_rwlockattr_getpshared)(
		const __SPRT_ID(pthread_rwlockattr_t) * __SPRT_RESTRICT attr, int *__SPRT_RESTRICT ret) {
	if (!attr || !ret) {
		return EINVAL;
	}

	auto tattr = reinterpret_cast<const _thread::rwlockattr_t *>(attr);
	if (hasFlag(tattr->flags, _thread::RwlockAttrFlags::Shared)) {
		*ret = __SPRT_PTHREAD_PROCESS_SHARED;
	} else {
		*ret = __SPRT_PTHREAD_PROCESS_PRIVATE;
	}
	return 0;
}

__SPRT_C_FUNC int __SPRT_ID(pthread_rwlock_init)(__SPRT_ID(pthread_rwlock_t) * __SPRT_RESTRICT lock,
		const __SPRT_ID(pthread_rwlockattr_t) * __SPRT_RESTRICT attr) {
	if (!lock) {
		return EINVAL;
	}

	auto tlock = new (reinterpret_cast<_thread::rwlock_t *>(lock), nothrow) _thread::rwlock_t();
	if (attr) {
		tlock->flags = reinterpret_cast<const _thread::rwlockattr_t *>(attr)->flags;
	}
	return 0;
}

__SPRT_C_FUNC int __SPRT_ID(pthread_rwlock_destroy)(__SPRT_ID(pthread_rwlock_t) * lock) {
	if (!lock) {
		return EINVAL;
	}

	auto tlock = reinterpret_cast<_thread::rwlock_t *>(lock);
	tlock->~rwlock_t();
	return 0;
}

__SPRT_C_FUNC int __SPRT_ID(pthread_rwlock_rdlock)(__SPRT_ID(pthread_rwlock_t) * lock) {
	if (!lock) {
		return EINVAL;
	}

	return reinterpret_cast<_thread::rwlock_t *>(lock)->rdlock(__SPRT_SPRT_TIMEOUT_INFINITE);
}

__SPRT_C_FUNC int __SPRT_ID(pthread_rwlock_tryrdlock)(__SPRT_ID(pthread_rwlock_t) * lock) {
	if (!lock) {
		return EINVAL;
	}

	return reinterpret_cast<_thread::rwlock_t *>(lock)->tryrdlock();
}

__SPRT_C_FUNC int __SPRT_ID(
		pthread_rwlock_timedrdlock)(__SPRT_ID(pthread_rwlock_t) * __SPRT_RESTRICT lock,
		const __SPRT_TIMESPEC_NAME *__SPRT_RESTRICT tv) {
	if (!lock || !tv || tv->tv_nsec < 0 || tv->tv_nsec >= 1'000'000'000) {
		return EINVAL;
	}

	__SPRT_TIMESPEC_NAME curTv;
	__sprt_clock_gettime(__SPRT_CLOCK_REALTIME, &curTv);

	auto diffTv = __sprt_timespec_diff(tv, &curTv);

	if (diffTv.tv_sec < 0) {
		return ETIMEDOUT;
	}

	return reinterpret_cast<_thread::rwlock_t *>(lock)->rdlock(
			diffTv.tv_sec * 1'000'000'000 + diffTv.tv_nsec);
}

__SPRT_C_FUNC int __SPRT_ID(pthread_rwlock_wrlock)(__SPRT_ID(pthread_rwlock_t) * lock) {
	if (!lock) {
		return EINVAL;
	}

	return reinterpret_cast<_thread::rwlock_t *>(lock)->wrlock(__SPRT_SPRT_TIMEOUT_INFINITE);
}

__SPRT_C_FUNC int __SPRT_ID(pthread_rwlock_trywrlock)(__SPRT_ID(pthread_rwlock_t) * lock) {
	if (!lock) {
		return EINVAL;
	}

	return reinterpret_cast<_thread::rwlock_t *>(lock)->trywrlock();
}

__SPRT_C_FUNC int __SPRT_ID(
		pthread_rwlock_timedwrlock)(__SPRT_ID(pthread_rwlock_t) * __SPRT_RESTRICT lock,
		const __SPRT_TIMESPEC_NAME *__SPRT_RESTRICT tv) {
	if (!lock || !tv || tv->tv_nsec < 0 || tv->tv_nsec >= 1'000'000'000) {
		return EINVAL;
	}

	__SPRT_TIMESPEC_NAME curTv;
	__sprt_clock_gettime(__SPRT_CLOCK_REALTIME, &curTv);

	auto diffTv = __sprt_timespec_diff(tv, &curTv);

	if (diffTv.tv_sec < 0) {
		return ETIMEDOUT;
	}

	return reinterpret_cast<_thread::rwlock_t *>(lock)->wrlock(
			diffTv.tv_sec * 1'000'000'000 + diffTv.tv_nsec);
}

__SPRT_C_FUNC int __SPRT_ID(pthread_rwlock_unlock)(__SPRT_ID(pthread_rwlock_t) * lock) {
	if (!lock) {
		return EINVAL;
	}

	return reinterpret_cast<_thread::rwlock_t *>(lock)->unlock();
}

} // namespace sprt::_thread
