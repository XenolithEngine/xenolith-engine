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

static rmutex_base::value_type getThreadId() {
	rmutex_base::value_type ret;
	auto threadId = __sprt_gettid();
	if constexpr (is_same_v<rmutex_base::tid_type, uint32_t>) {
		*rmutex_base::getNativeValue(ret) = threadId;
	} else {
		// write TID with original thread priority
		auto t = thread_t::self();
		auto prio = t->attr.prio;
		*rmutex_base::getNativeValue(ret) = uint64_t(threadId) | (uint64_t(prio) << 32);
	}
	return ret;
}

[[maybe_unused]]
static thread_t *boostThreadPrio(mutex_t *mtx, uint32_t threadId, int32_t dprio) {
	auto pool = __thread_pool::get();

	unique_lock globalLock(pool->mutex);

	auto it = pool->activeThreads.find(threadId);
	if (it == pool->activeThreads.end()) {
		return nullptr;
	}

	it->second->promoteMutex(mtx, dprio);
	return it->second;
}

bool mutex_t::isValid(const mutexattr_t &) { return true; }

int mutex_t::lock(timeout_t timeout) {
	__sprt_sprt_lock_flags_t __mutexFlags = 0;
	if (hasFlag(attr.flags, MutexAttrFlags::Shared)) {
		__mutexFlags |= __SPRT_SPRT_LOCK_FLAG_SHARED;
	}

	Status st = Status::Ok;

	// If we have no specific type or protocol requirements - use qlock,
	// Only PTHREAD_PRIO_NONE + PTHREAD_MUTEX_NORMAL can use qlock backend
	if (!hasFlag(attr.flags, MutexAttrFlags::TypeNMask | MutexAttrFlags::PrioMask)) {
		// use qlock
		if (timeout == __SPRT_SPRT_TIMEOUT_INFINITE) {
			st = qmutex_base::_lock<__sprt_sprt_qlock_wait, nullptr>(&qlock, nullptr, __mutexFlags);
		} else {
			st = qmutex_base::_lock<__sprt_sprt_qlock_wait, __sprt_sprt_qlock_now>(&qlock, &timeout,
					__mutexFlags);
		}

		// It's not an error in SPRT, but it's error in POSIX
		if (st == Status::Timeout) {
			st = Status::ErrorTimeout;
		}
	} else {
		// use rlock
		auto type = attr.flags & MutexAttrFlags::TypeNMask;
		auto protocol = attr.flags & MutexAttrFlags::PrioMask;
		auto self = thread_t::self();

		bool nativePiRequired = false;

		if (protocol == MutexAttrFlags::PrioProtect) {
			// if PTHREAD_PRIO_PROTECT - check, if we can lock the mutex
			if (self->attr.prio > attr.prio) {
				return EINVAL;
			}
		} else if (protocol == MutexAttrFlags::PrioInherit) {
			// if PTHREAD_PRIO_INHERIT - check if rlock supports PI natively
			nativePiRequired = (__sprt_sprt_rlock_supports(__SPRT_SPRT_LOCK_FLAG_PI) == 0);
			if (nativePiRequired) {
				__mutexFlags |= __SPRT_SPRT_LOCK_FLAG_PI;
			}
		}

		auto threadId = getThreadId();

		bool priorityProtocolEnabled = false;
		thread_t *boostedThread = nullptr;

		// Function to enable priority protocol
		auto runPriorityProtection = [&](const rmutex_base::value_type &threadData) {
			switch (protocol) {
			case MutexAttrFlags::PrioInherit:
				// if priority in threadData is lower then ours, boost that thrad to our priority
				priorityProtocolEnabled = true;
				if constexpr (is_same_v<rmutex_base::tid_type, uint64_t>) {
					auto dprio = self->dynamicPrio.load();
					auto prio = (threadData.u64 >> 32 & 0xFFFF);
					if (dprio > prio) {
						boostedThread = boostThreadPrio(this,
								uint32_t(threadData.u64 & 0xFFFF'FFFF), dprio);
						priorityProtocolEnabled = true;
					}
				}
				break;
			default: break;
			}
		};

		if (nativePiRequired) {
			// if we have native PI support - no need to use runPriorityProtection callback
			if (timeout == __SPRT_SPRT_TIMEOUT_INFINITE) {
				st = rmutex_base::_lock<__sprt_sprt_rlock_wait, nullptr,
						bool(__SPRT_SPRT_RLOCK_PI_REQUIRES_EXTENDED_CALL)>(rlock.value, threadId,
						nullptr, __mutexFlags);
			} else {
				st = rmutex_base::_lock<__sprt_sprt_rlock_wait, __sprt_sprt_rlock_now,
						bool(__SPRT_SPRT_RLOCK_PI_REQUIRES_EXTENDED_CALL)>(rlock.value, threadId,
						&timeout, __mutexFlags);
			}
		} else {
			if (timeout == __SPRT_SPRT_TIMEOUT_INFINITE) {
				st = rmutex_base::_lock<__sprt_sprt_rlock_wait, nullptr, false>(rlock.value,
						threadId, nullptr, __mutexFlags, runPriorityProtection);
			} else {
				st = rmutex_base::_lock<__sprt_sprt_rlock_wait, __sprt_sprt_rlock_now, false>(
						rlock.value, threadId, &timeout, __mutexFlags, runPriorityProtection);
			}
		}

		switch (st) {
		case Status::Ok: ++rlock.counter; break;
		case Status::Propagate:
			switch (type) {
			case MutexAttrFlags::TypeErrorCheck: st = Status::ErrorDeadLock; break;
			case MutexAttrFlags::TypeRecursive:
				++rlock.counter;
				st = Status::Ok;
				break;
			default:
				// PTHREAD_MUTEX_NORMAL, but we already know that it's an error
				// We have no other option that follow PTHREAD_MUTEX_ERRORCHECK rule
				st = Status::ErrorDeadLock;
				break;
			}
		case Status::Timeout:
			// It's not an error in SPRT, but it's error in POSIX
			st = Status::ErrorTimeout;
			break;
		default: break;
		}

		if (st != Status::Ok) {
			// If priority protocol was enabled, and we fail to lock mutex for some reason - we need to disable protocol
			if (priorityProtocolEnabled && boostedThread) {
				// For now, there is no graceful way to unboost thread due ABA problem
				// (we can lose boost from another thread).
				//
				// So, thread will remain boosted even if high ptiority thread cancel it's lock request
				//unboostThreadPrio(this, boostedThread);
			}
		} else
			// Perform this only once mutex is locked first recursive time
			if (rlock.counter == 1) {
				// For PTHREAD_MUTEX_ROBUST or any priority protocol, we should register our mutex in robust mutex list
				if (hasFlag(attr.flags, MutexAttrFlags::Robust)
						|| protocol != MutexAttrFlags::None) {
					// To ignore mutex prio when no PTHREAD_PRIO_PROTECT required, we use self->attr.prio,
					// With this value, no priority boost shuuld happen
					// This also mean, that user cant decrease thread's priority lower then it was before lock.
					self->addMutex(this,
							protocol == MutexAttrFlags::PrioProtect ? attr.prio : self->attr.prio);
				}
			}
	}

	if (st == Status::Ok) {
		// This can be set by thread_t for a robust mutex, return it as POSIX requires.
		// Mutex with dead owner should be locked successfully at first before call `consistent`
		if (hasFlag(attr.flags, MutexAttrFlags::OwnerDied)) {
			return EOWNERDEAD;
		}
	}

	return status::toErrno(st);
}

int mutex_t::unlock() {
	__sprt_sprt_lock_flags_t __mutexFlags = 0;
	if (hasFlag(attr.flags, MutexAttrFlags::Shared)) {
		__mutexFlags |= __SPRT_SPRT_LOCK_FLAG_SHARED;
	}

	if (hasFlag(attr.flags, MutexAttrFlags::OwnerDied)) {
		// We are here because mutex with EOWNERDEAD was not restored with `consistent`
		// before `unlock`.

		if (hasFlag(attr.flags, MutexAttrFlags::TypeNMask | MutexAttrFlags::PrioMask)) {
			// rlock backend
			auto protocol = attr.flags & MutexAttrFlags::PrioMask;

			// This unlock, however, can be done by other thread, check for it.
			if (rlock.counter <= 0) {
				return EPERM;
			}

			auto threadId = getThreadId();
			auto expected = _atomic::loadRel(rmutex_base::getNativeValue(rlock));
			if ((expected & rmutex_base::THREAD_ID_MASK)
					!= (*rmutex_base::getNativeValue(threadId) & rmutex_base::THREAD_ID_MASK)) {
				return EPERM;
			}

			if (protocol == MutexAttrFlags::PrioInherit) {
				// if PTHREAD_PRIO_INHERIT - check if rlock supports PI natively
				auto nativePiRequired = (__sprt_sprt_rlock_supports(__SPRT_SPRT_LOCK_FLAG_PI) == 0);
				if (nativePiRequired) {
					__mutexFlags |= __SPRT_SPRT_LOCK_FLAG_PI;
				}
			}

			// make mutex inconsistent and notify all about it
			attr.flags |= MutexAttrFlags::NotRecoverable;
			_atomic::fetchOr(rmutex_base::getNativeValue(rlock), rmutex_base::OWNER_DIED);

			__sprt_sprt_rlock_wake(&rlock.value, __mutexFlags);
		} else {
			// qlock backend will be unrecoverable in any case
			attr.flags |= MutexAttrFlags::NotRecoverable;
		}

		return ENOTRECOVERABLE;
	}

	Status st = Status::Ok;
	// If we have no specific type or protection requirements - use qlock
	if (!hasFlag(attr.flags, MutexAttrFlags::TypeNMask | MutexAttrFlags::PrioMask)) {
		// use qlock
		st = qmutex_base::_unlock<__sprt_sprt_qlock_wake_one>(&qlock, __mutexFlags);
	} else {
		// use rlock
		auto protocol = attr.flags & MutexAttrFlags::PrioMask;
		auto self = thread_t::self();

		if (protocol == MutexAttrFlags::PrioInherit) {
			// if PTHREAD_PRIO_INHERIT - check if rlock supports PI natively
			if (__sprt_sprt_rlock_supports(__SPRT_SPRT_LOCK_FLAG_PI) == 0) {
				__mutexFlags |= __SPRT_SPRT_LOCK_FLAG_PI;
			}
		}

		auto threadId = getThreadId();

		st = rmutex_base::_unlock<__sprt_sprt_rlock_wake>(rlock.value, threadId, &rlock.counter,
				__mutexFlags);

		switch (st) {
		case Status::Ok:
		case Status::Done:
			// Ok and Done means, that mutex no longer locked, we should remove anu priiority boostings from it
			if (hasFlag(attr.flags, MutexAttrFlags::Robust) || protocol != MutexAttrFlags::None) {
				self->removeMutex(this);
			}
			st = Status::Ok;
		default: break;
		}
	}

	return status::toErrno(st);
}

int mutex_t::try_lock() {
	__sprt_sprt_lock_flags_t __mutexFlags = 0;
	if (hasFlag(attr.flags, MutexAttrFlags::Shared)) {
		__mutexFlags |= __SPRT_SPRT_LOCK_FLAG_SHARED;
	}

	Status st = Status::Ok;
	// If we have no specific type or protection requirements - use qlock
	if (!hasFlag(attr.flags, MutexAttrFlags::TypeNMask | MutexAttrFlags::PrioMask)) {
		// use qlock
		st = qmutex_base::_try_lock(&qlock);
	} else {
		// use rlock
		auto type = attr.flags & MutexAttrFlags::TypeNMask;
		auto protocol = attr.flags & MutexAttrFlags::PrioMask;
		auto self = thread_t::self();

		bool nativePiRequired = false;

		if (protocol == MutexAttrFlags::PrioProtect) {
			// if PTHREAD_PRIO_PROTECT - check, if we can lock the mutex
			if (self->attr.prio > attr.prio) {
				return EINVAL;
			}
		} else if (protocol == MutexAttrFlags::PrioInherit) {
			// if PTHREAD_PRIO_INHERIT - check if rlock supports PI natively
			nativePiRequired = (__sprt_sprt_rlock_supports(__SPRT_SPRT_LOCK_FLAG_PI) == 0);
			if (nativePiRequired) {
				__mutexFlags |= __SPRT_SPRT_LOCK_FLAG_PI;
			}
		}

		auto threadId = getThreadId();

		st = rmutex_base::_try_lock<__sprt_sprt_rlock_try_wait>(rlock.value, threadId,
				__mutexFlags);

		switch (st) {
		case Status::Ok: ++rlock.counter; break;
		case Status::Propagate:
			switch (type) {
			case MutexAttrFlags::TypeErrorCheck: st = Status::ErrorDeadLock; break;
			case MutexAttrFlags::TypeRecursive:
				++rlock.counter;
				st = Status::Ok;
				break;
			default:
				// PTHREAD_MUTEX_NORMAL, but we already know that it's an error
				// We have no other option that follow PTHREAD_MUTEX_ERRORCHECK rule
				st = Status::ErrorDeadLock;
				break;
			}
		case Status::Timeout:
			// It's not an error in SPRT, but it's error in POSIX
			st = Status::ErrorTimeout;
			break;
		default: break;
		}

		if (st == Status::Ok && rlock.counter == 1) {
			// For PTHREAD_MUTEX_ROBUST or any priority protocol, we should register our mutex in robust mutex list
			if (hasFlag(attr.flags, MutexAttrFlags::Robust) || protocol != MutexAttrFlags::None) {
				// To ignore mutex prio when no PTHREAD_PRIO_PROTECT required, we use self->attr.prio,
				// With this value, no priority boost shuuld happen;
				// This also mean, that user cant decrease thread's priority lower then it was before lock.
				self->addMutex(this,
						protocol == MutexAttrFlags::PrioProtect ? attr.prio : self->attr.prio);
			}
		}
	}

	if (st == Status::Ok) {
		// This can be set by thread_t for a robust mutex, return it as POSIX requires.
		// Mutex with dead owner should be locked successfully at first before call `consistent`
		if (hasFlag(attr.flags, MutexAttrFlags::OwnerDied)) {
			return EOWNERDEAD;
		}
	}

	return status::toErrno(st);
}

bool mutex_t::is_locked() const {
	if (!hasFlag(attr.flags, MutexAttrFlags::TypeNMask | MutexAttrFlags::PrioMask)) {
		auto v = _atomic::loadSeq(&qlock);
		if (v & qmutex_base::LOCK_BIT) {
			return true;
		}
		return false;
	} else {
		auto expected = _atomic::loadSeq(rmutex_base::getNativeValue(rlock));
		if ((expected & rmutex_base::THREAD_ID_MASK) != 0) {
			return true;
		}
		return false;
	}
}

bool mutex_t::has_ownedship() const {
	if (!hasFlag(attr.flags, MutexAttrFlags::TypeNMask | MutexAttrFlags::PrioMask)) {
		return false; // no thread ownershup for qlock
	} else {
		return true;
	}
}

bool mutex_t::is_owned_by_this_thread() const {
	if (!hasFlag(attr.flags, MutexAttrFlags::TypeNMask | MutexAttrFlags::PrioMask)) {
		return false; // no thread ownershup for qlock
	}

	auto desired = getThreadId();
	auto expected = _atomic::loadSeq(rmutex_base::getNativeValue(rlock));

	// ensure that we are owner
	if ((expected & rmutex_base::THREAD_ID_MASK)
			== (*rmutex_base::getNativeValue(desired) & rmutex_base::THREAD_ID_MASK)) {
		return true;
	}
	return false;
}

void mutex_t::force_unlock() {
	if (!hasFlag(attr.flags, MutexAttrFlags::TypeNMask | MutexAttrFlags::PrioMask)) {
		return; // no thread ownershup for qlock
	}

	__sprt_sprt_lock_flags_t __mutexFlags = 0;
	if (hasFlag(attr.flags, MutexAttrFlags::Shared)) {
		__mutexFlags |= __SPRT_SPRT_LOCK_FLAG_SHARED;
	}

	auto protocol = attr.flags & MutexAttrFlags::PrioMask;
	if (protocol == MutexAttrFlags::PrioInherit) {
		// if PTHREAD_PRIO_INHERIT - check if rlock supports PI natively
		if (__sprt_sprt_rlock_supports(__SPRT_SPRT_LOCK_FLAG_PI) == 0) {
			__mutexFlags |= __SPRT_SPRT_LOCK_FLAG_PI;
		}
	}

	auto desired = getThreadId();
	rmutex_base::value_type expected;
	*rmutex_base::getNativeValue(expected) = _atomic::loadSeq(rmutex_base::getNativeValue(rlock));

	// ensure that we are owner
	if ((*rmutex_base::getNativeValue(expected) & rmutex_base::THREAD_ID_MASK)
			== (*rmutex_base::getNativeValue(desired) & rmutex_base::THREAD_ID_MASK)) {
		attr.flags |= MutexAttrFlags::OwnerDied;

		// remove all recursive locks
		rlock.counter = 1;

		rmutex_base::_unlock<__sprt_sprt_rlock_wake>(rlock.value, desired, &rlock.counter,
				__mutexFlags);
	}
}

int mutex_t::consistent() {
	if (hasFlag(attr.flags, MutexAttrFlags::NotRecoverable)) {
		return EINVAL;
	}
	auto protocol = attr.flags & MutexAttrFlags::PrioMask;
	if (hasFlag(attr.flags, MutexAttrFlags::Robust) || protocol != MutexAttrFlags::None) {
		auto flags = MutexAttrFlags(_atomic::loadSeq((uint16_t *)&attr.flags));
		if (!hasFlag(flags, MutexAttrFlags::OwnerDied)) {
			return EINVAL;
		}
		attr.flags &= ~MutexAttrFlags::OwnerDied;
	}
	return EINVAL;
}

__SPRT_C_FUNC int __SPRT_ID(pthread_mutexattr_init)(__SPRT_ID(pthread_mutexattr_t) * attr) {
	if (!attr) {
		return EINVAL;
	}

	new (attr, sprt::nothrow) _thread::mutexattr_t;
	return 0;
}

__SPRT_C_FUNC int __SPRT_ID(pthread_mutexattr_destroy)(__SPRT_ID(pthread_mutexattr_t) * attr) {
	if (!attr) {
		return EINVAL;
	}

	auto tattr = reinterpret_cast<_thread::mutexattr_t *>(attr);
	tattr->~mutexattr_t();
	return 0;
}

__SPRT_C_FUNC int __SPRT_ID(pthread_mutexattr_getprioceiling)(
		const __SPRT_ID(pthread_mutexattr_t) * __SPRT_RESTRICT attr, int *__SPRT_RESTRICT ret) {
	if (!attr || !ret) {
		return EINVAL;
	}

	auto tattr = reinterpret_cast<const _thread::mutexattr_t *>(attr);
	*ret = tattr->prio;
	return 0;
}

__SPRT_C_FUNC int __SPRT_ID(
		pthread_mutexattr_setprioceiling)(__SPRT_ID(pthread_mutexattr_t) * attr, int v) {
	if (!attr || !_thread::native::validate_mutexattr_setprioceiling(v)) {
		return EINVAL;
	}

	if (v < _thread::__thread_pool::get()->fifoPrioMin
			|| v > _thread::__thread_pool::get()->fifoPrioMax) {
		return EINVAL;
	}

	auto tattr = reinterpret_cast<_thread::mutexattr_t *>(attr);
	tattr->prio = static_cast<decltype(tattr->prio)>(v);
	return 0;
}

__SPRT_C_FUNC int __SPRT_ID(pthread_mutexattr_getprotocol)(
		const __SPRT_ID(pthread_mutexattr_t) * __SPRT_RESTRICT attr, int *__SPRT_RESTRICT ret) {
	if (!attr || !ret) {
		return EINVAL;
	}

	auto tattr = reinterpret_cast<const _thread::mutexattr_t *>(attr);
	auto m = tattr->flags & _thread::MutexAttrFlags::PrioMask;
	switch (m) {
	case _thread::MutexAttrFlags::PrioInherit: *ret = __SPRT_PTHREAD_PRIO_INHERIT; break;
	case _thread::MutexAttrFlags::PrioProtect: *ret = __SPRT_PTHREAD_PRIO_PROTECT; break;
	default: *ret = __SPRT_PTHREAD_PRIO_NONE; break;
	}
	return 0;
}
__SPRT_C_FUNC int __SPRT_ID(
		pthread_mutexattr_setprotocol)(__SPRT_ID(pthread_mutexattr_t) * attr, int v) {
	if (!attr || !_thread::native::validate_mutexattr_setprotocol(v)) {
		return EINVAL;
	}

	auto tattr = reinterpret_cast<_thread::mutexattr_t *>(attr);

	switch (v) {
	case __SPRT_PTHREAD_PRIO_INHERIT:
		tattr->flags &= ~_thread::MutexAttrFlags::PrioMask;
		tattr->flags |= _thread::MutexAttrFlags::PrioInherit;
		break;
	case __SPRT_PTHREAD_PRIO_PROTECT:
		tattr->flags &= ~_thread::MutexAttrFlags::PrioMask;
		tattr->flags |= _thread::MutexAttrFlags::PrioProtect;
		break;
	case __SPRT_PTHREAD_PRIO_NONE: tattr->flags &= _thread::MutexAttrFlags::PrioMask; break;
	default: return EINVAL;
	}

	return 0;
}

__SPRT_C_FUNC int __SPRT_ID(pthread_mutexattr_getrobust)(
		const __SPRT_ID(pthread_mutexattr_t) * __SPRT_RESTRICT attr, int *__SPRT_RESTRICT ret) {
	if (!attr || !ret) {
		return EINVAL;
	}

	auto tattr = reinterpret_cast<const _thread::mutexattr_t *>(attr);
	if (hasFlag(tattr->flags, _thread::MutexAttrFlags::Robust)) {
		*ret = __SPRT_PTHREAD_MUTEX_ROBUST;
	} else {
		*ret = __SPRT_PTHREAD_MUTEX_STALLED;
	}

	return 0;
}

__SPRT_C_FUNC int __SPRT_ID(
		pthread_mutexattr_setrobust)(__SPRT_ID(pthread_mutexattr_t) * attr, int v) {
	if (!attr) {
		return EINVAL;
	}

	auto tattr = reinterpret_cast<_thread::mutexattr_t *>(attr);

	switch (v) {
	case __SPRT_PTHREAD_MUTEX_STALLED: tattr->flags &= ~_thread::MutexAttrFlags::Robust; break;
	case __SPRT_PTHREAD_MUTEX_ROBUST: tattr->flags |= _thread::MutexAttrFlags::Robust; break;
	default: return EINVAL;
	}

	return 0;
}

__SPRT_C_FUNC int __SPRT_ID(pthread_mutexattr_getpshared)(
		const __SPRT_ID(pthread_mutexattr_t) * __SPRT_RESTRICT attr, int *__SPRT_RESTRICT ret) {
	if (!attr || !ret) {
		return EINVAL;
	}

	auto tattr = reinterpret_cast<const _thread::mutexattr_t *>(attr);
	if (hasFlag(tattr->flags, _thread::MutexAttrFlags::Shared)) {
		*ret = __SPRT_PTHREAD_PROCESS_SHARED;
	} else {
		*ret = __SPRT_PTHREAD_PROCESS_PRIVATE;
	}

	return 0;
}

__SPRT_C_FUNC int __SPRT_ID(
		pthread_mutexattr_setpshared)(__SPRT_ID(pthread_mutexattr_t) * attr, int v) {
	if (!attr || !_thread::native::validate_mutexattr_setpshared(v)) {
		return EINVAL;
	}

	auto tattr = reinterpret_cast<_thread::mutexattr_t *>(attr);

	switch (v) {
	case __SPRT_PTHREAD_PROCESS_PRIVATE: tattr->flags &= ~_thread::MutexAttrFlags::Shared; break;
	case __SPRT_PTHREAD_PROCESS_SHARED: tattr->flags |= _thread::MutexAttrFlags::Shared; break;
	default: return EINVAL;
	}

	return 0;
}

__SPRT_C_FUNC int __SPRT_ID(pthread_mutexattr_gettype)(
		const __SPRT_ID(pthread_mutexattr_t) * __SPRT_RESTRICT attr, int *__SPRT_RESTRICT ret) {
	if (!attr || !ret) {
		return EINVAL;
	}

	auto tattr = reinterpret_cast<const _thread::mutexattr_t *>(attr);
	auto m = tattr->flags & _thread::MutexAttrFlags::TypeNMask;
	switch (m) {
	case _thread::MutexAttrFlags::TypeErrorCheck: *ret = __SPRT_PTHREAD_MUTEX_ERRORCHECK; break;
	case _thread::MutexAttrFlags::TypeRecursive: *ret = __SPRT_PTHREAD_MUTEX_RECURSIVE; break;
	default: *ret = __SPRT_PTHREAD_MUTEX_NORMAL; break;
	}
	return 0;
}
__SPRT_C_FUNC int __SPRT_ID(
		pthread_mutexattr_settype)(__SPRT_ID(pthread_mutexattr_t) * attr, int v) {
	if (!attr) {
		return EINVAL;
	}

	auto tattr = reinterpret_cast<_thread::mutexattr_t *>(attr);

	switch (v) {
	case __SPRT_PTHREAD_MUTEX_ERRORCHECK:
		tattr->flags &= ~_thread::MutexAttrFlags::TypeNMask;
		tattr->flags |= _thread::MutexAttrFlags::TypeErrorCheck;
		break;
	case __SPRT_PTHREAD_MUTEX_RECURSIVE:
		tattr->flags &= ~_thread::MutexAttrFlags::TypeNMask;
		tattr->flags |= _thread::MutexAttrFlags::TypeRecursive;
		break;
	case __SPRT_PTHREAD_MUTEX_NORMAL: tattr->flags &= _thread::MutexAttrFlags::TypeNMask; break;
	default: return EINVAL;
	}

	return 0;
}


__SPRT_C_FUNC int __SPRT_ID(pthread_mutex_init)(__SPRT_ID(pthread_mutex_t) * __SPRT_RESTRICT mutex,
		const __SPRT_ID(pthread_mutexattr_t) * __SPRT_RESTRICT attr) {
	if (!mutex) {
		return EINVAL;
	}

	if (attr) {
		auto tattr = reinterpret_cast<const _thread::mutexattr_t *>(attr);
		if (_thread::mutex_t::isValid(*tattr)) {
			new (mutex, sprt::nothrow) _thread::mutex_t{*tattr};
			return 0;
		}
	} else {
		new (mutex, sprt::nothrow) _thread::mutex_t{_thread::mutexattr_t()};
		return 0;
	}

	return EINVAL;
}

__SPRT_C_FUNC int __SPRT_ID(pthread_mutex_destroy)(__SPRT_ID(pthread_mutex_t) * mutex) {
	if (!mutex) {
		return EINVAL;
	}

	auto mtx = reinterpret_cast<_thread::mutex_t *>(mutex);
	mtx->~mutex_t();
	return 0;
}

__SPRT_C_FUNC int __SPRT_ID(pthread_mutex_lock)(__SPRT_ID(pthread_mutex_t) * mutex) {
	if (!mutex) {
		return EINVAL;
	}

	auto mtx = reinterpret_cast<_thread::mutex_t *>(mutex);
	return mtx->lock(_thread::Infinite);
}

__SPRT_C_FUNC int __SPRT_ID(pthread_mutex_unlock)(__SPRT_ID(pthread_mutex_t) * mutex) {
	if (!mutex) {
		return EINVAL;
	}

	auto mtx = reinterpret_cast<_thread::mutex_t *>(mutex);
	return mtx->unlock();
}

__SPRT_C_FUNC int __SPRT_ID(pthread_mutex_trylock)(__SPRT_ID(pthread_mutex_t) * mutex) {
	if (!mutex) {
		return EINVAL;
	}

	auto mtx = reinterpret_cast<_thread::mutex_t *>(mutex);
	return mtx->try_lock();
}

__SPRT_C_FUNC int __SPRT_ID(
		pthread_mutex_timedlock)(__SPRT_ID(pthread_mutex_t) * __SPRT_RESTRICT mutex,
		const __SPRT_TIMESPEC_NAME *__SPRT_RESTRICT tv) {
	_thread::timeout_t nanotime = tv->tv_sec * 1'000'000'000 + tv->tv_nsec;

	auto mtx = reinterpret_cast<_thread::mutex_t *>(mutex);
	return mtx->lock(nanotime);
}

__SPRT_C_FUNC int __SPRT_ID(pthread_mutex_consistent)(__SPRT_ID(pthread_mutex_t) * mutex) {
	if (!mutex) {
		return EINVAL;
	}

	auto mtx = reinterpret_cast<_thread::mutex_t *>(mutex);
	return mtx->consistent();
}

__SPRT_C_FUNC int __SPRT_ID(pthread_mutex_getprioceiling)(
		const __SPRT_ID(pthread_mutex_t) * __SPRT_RESTRICT mutex, int *__SPRT_RESTRICT p) {
	if (!mutex || !p) {
		return EINVAL;
	}

	auto mtx = reinterpret_cast<const _thread::mutex_t *>(mutex);
	*p = static_cast<int>(mtx->attr.prio);
	return 0;
}

__SPRT_C_FUNC int __SPRT_ID(pthread_mutex_setprioceiling)(
		__SPRT_ID(pthread_mutex_t) * __SPRT_RESTRICT mutex, int v, int *__SPRT_RESTRICT p) {
	if (!mutex || !_thread::native::validate_mutexattr_setprioceiling(v)) {
		return EINVAL;
	}

	if (v < _thread::__thread_pool::get()->fifoPrioMin
			|| v > _thread::__thread_pool::get()->fifoPrioMax) {
		return EINVAL;
	}

	auto mtx = reinterpret_cast<_thread::mutex_t *>(mutex);

	// When we chainge prio value, we need to apply changes to protocol itself.
	// To do this, we need lock-unlock cycle, istead of just atomically set it
	auto ret = mtx->lock(_thread::Infinite);
	if (ret == 0) {
		mtx->attr.prio = static_cast<decltype(mtx->attr.prio)>(v);
		mtx->unlock();
	}
	return ret;
}

} // namespace sprt::_thread
