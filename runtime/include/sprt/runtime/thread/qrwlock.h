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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_THREAD_QRWLOCK_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_THREAD_QRWLOCK_H_

#include <sprt/runtime/thread/qmutex.h>

namespace sprt {

class qrwlock_base {
public:
	using value_type = __sprt_sprt_qlock_t;
	using timeout_type = __sprt_sprt_timeout_t;
	using flags_type = __sprt_sprt_lock_flags_t;

	static constexpr value_type ReadLock = 1;
	static constexpr value_type WriteLock = 2;
	static constexpr value_type Waiters = 4;

	struct rwlock_data {
		// Lock flags
		value_type value = 0;

		// Prevent writer starwation by protect writing lock process by simple mutex;
		// rdlock will perfrom lock/unlock on this mutex before readlock process,
		// so, if there are writer - read locking process will wait on this mutex until
		// writer unlocks it
		value_type gatekeeper_mutex = 0;

		/*
			The read-unlock operation must be atomic.
			Since the counter is separate from the main mutex, a dedicated mutex is
			required to ensure this atomicity. Otherwise, if incrementing the counter
			and initiating the read lock occur between decrementing the counter and
			performing the unlock, a deadlock will arise.

			Deadlock pattern is:
			T1					T2
			-					decrement to 0 (causes rdunlock)
			increment to 1		-
			rdlock				-
			-					rdunlock

			With mutex, it become:
			T1					T2
			-					mtx lock
			-					decrement to 0 (causes rdunlock)
			-					rdunlock
			-					mtx unlock
			mtx lock			-
			increment to 1		-
			mtx unlock			-  (note, that we do not protect rdlock, only it's relative ordering)
			rdlock				-
			-					
		*/
		value_type counter_mutex = 0;

		/*
			The counter must be separated from the main mutex to avoid unnecessary wakeups when
			a new reader is added (the futex may be woken because the value has changed).
		*/
		uint32_t counter = 0;
	};

	template <int (*WakeFn)(value_type *, flags_type)>
	static Status _funlock(value_type *__value, flags_type flags, value_type unlockMode) {
		auto expected = _atomic::loadSeq(__value);
		if ((expected & unlockMode) != 0) {
			expected = _atomic::exchange(__value, value_type(0));
			if ((expected & Waiters) != 0) {
				if (WakeFn(__value, flags) != 0) {
					return status::errnoToStatus(__sprt_errno);
				}
			}
			return Status::Ok;
		}
		return Status::Declined;
	}

	template <int (*WakeFn)(value_type *, flags_type)>
	static Status _unlock(value_type *__value, flags_type flags) {
		auto current = _atomic::exchange(__value, value_type(0));
		if ((current & Waiters) != 0) {
			if (WakeFn(__value, flags) != 0) {
				return status::errnoToStatus(__sprt_errno);
			}
		}
		return Status::Ok;
	}

	static Status _rd_decrement(rwlock_data *__data, flags_type flags) {
		auto st = qmutex_base::_lock<__sprt_sprt_qlock_wait, nullptr>(&__data->counter_mutex,
				nullptr, flags);

		if (status::isSuccessful(st)) {
			_atomic::fetchSub(&__data->counter, uint32_t(1));

			st = qmutex_base::_unlock<__sprt_sprt_qlock_wake_one>(&__data->counter_mutex, flags);
		}
		return st;
	}

	static Status _rd_decrement_unlock(rwlock_data *__data, flags_type flags, value_type expected) {
		auto st = qmutex_base::_lock<__sprt_sprt_qlock_wait, nullptr>(&__data->counter_mutex,
				nullptr, flags);

		if (status::isSuccessful(st)) {
			if (expected == 0) {
				expected = _atomic::loadSeq(&__data->value);
			}

			bool shouldWake = false;
			auto fs = _atomic::fetchSub(&__data->counter, uint32_t(1));

			if ((expected & ReadLock) != 0) {
				if (fs == 1) {
					auto current = _atomic::exchange(&__data->value, value_type(0));
					if ((current & Waiters) != 0) {
						shouldWake = true;
					}
				}
			}

			st = qmutex_base::_unlock<__sprt_sprt_qlock_wake_one>(&__data->counter_mutex, flags);

			if (shouldWake) {
				if (__sprt_sprt_qlock_wake_all(&__data->value, flags) != 0) {
					st = status::errnoToStatus(__sprt_errno);
				}
			}
		}
		return st;
	}

	static Status _rd_increment(rwlock_data *__data, flags_type flags) {
		auto st = qmutex_base::_lock<__sprt_sprt_qlock_wait, nullptr>(&__data->counter_mutex,
				nullptr, flags);

		if (sprt::status::isSuccessful(st)) {
			_atomic::fetchAdd(&__data->counter, uint32_t(1));

			st = qmutex_base::_unlock<__sprt_sprt_qlock_wake_one>(&__data->counter_mutex, flags);
		}
		return st;
	}

	static Status _unlock_fair(rwlock_data *__data, flags_type flags, value_type type = 0) {
		Status ret = Status::Ok;

		auto expected = _atomic::loadSeq(&__data->value);

		if (type != 0 && (expected & type) == 0) {
			return Status::ErrorInvalidArguemnt;
		}

		if ((expected & ReadLock) != 0) {
			return _rd_decrement_unlock(__data, flags, expected);
		}

		auto current = _atomic::exchange(&__data->value, value_type(0));
		if ((current & WriteLock) != 0) {
			qmutex_base::_unlock<__sprt_sprt_qlock_wake_one>(&__data->gatekeeper_mutex, flags);
		}

		if ((current & Waiters) != 0) {
			if (__sprt_sprt_qlock_wake_all(&__data->value, flags) != 0) {
				ret = status::errnoToStatus(__sprt_errno);
			}
		}
		return ret;
	}

	template <int (*WaitFn)(value_type *, value_type, timeout_type, flags_type),
			timeout_type (*ClockFn)(flags_type)>
	static Status _rdlock(value_type *__value, timeout_type *timeout, flags_type flags) {
		timeout_type now = 0, next = 0;
		if constexpr (ClockFn != nullptr) {
			if (timeout) {
				now = ClockFn(flags);
			}
		}

		value_type desired = ReadLock;
		value_type expected = 0;

		// Read-lock is successful when we successfully perform 0 -> ReadLock
		// Or ReadLock alredy set
		if (!_atomic::compareSwap(__value, &expected, desired)) [[unlikely]] {
			// failed - should lock

			// Assume that there are other waiters
			// If our lock will be successful - set Waiters flag immediately
			desired |= Waiters;

			auto doLock = [&](value_type lockValue) {
				auto st = WaitFn(__value, lockValue,
						timeout ? *timeout : __SPRT_SPRT_TIMEOUT_INFINITE, flags);
				if (st != 0) {
					if (__sprt_errno == ETIMEDOUT) {
						return Status::Timeout;
					}
					if (__sprt_errno != EAGAIN) {
						return status::errnoToStatus(__sprt_errno);
					}
				}

				if constexpr (ClockFn != nullptr) {
					if (timeout) {
						next = ClockFn(flags);
						*timeout -= min((next - now), *timeout);
						now = next;
					}
				}

				// value was changed, now it's 0 or other lock value
				// assume 0 and try to set lock with ReadLock
				expected = 0;
				return Status::Ok;
			};

			do {
				// check if read lock already set
				if ((expected & ReadLock) == ReadLock) {
					// read lock is set, return
					break; // from do ... while
				}

				if constexpr (ClockFn != nullptr) {
					if (timeout && *timeout == 0) {
						return Status::Timeout;
					}
				}

				if ((expected & WriteLock) == WriteLock) {
					if ((expected & Waiters) == Waiters) {
						auto st = doLock(expected);
						if (st != Status::Ok) {
							return st;
						}
					} else {
						expected = _atomic::fetchOr(__value, Waiters) | Waiters;
						if ((expected & WriteLock) == WriteLock) {
							auto st = doLock(expected);
							if (st != Status::Ok) {
								return st;
							}
						}
					}
				}
			} while (!_atomic::compareSwap(__value, &expected, desired));
		}
		return Status::Ok;
	}

	template <
			// Function to wait on lock
			int (*WaitFn)(value_type *, value_type, timeout_type, flags_type),

			// Function to wake single waiter
			int (*WakeFn)(value_type *, flags_type),

			// Timeout function
			timeout_type (*ClockFn)(flags_type)>
	static Status _rdlock_fair(rwlock_data *__data, timeout_type *timeout, flags_type flags) {
		Status st = Status::Ok;

		// do lock-unlock cycle on Gatekeeper mutex
		// this mutex prevents reader to lock if there are writer waiting
		st = qmutex_base::_lock<WaitFn, ClockFn>(&__data->gatekeeper_mutex, timeout, flags);
		if (st == Status::Timeout || (timeout && *timeout == 0)) {
			return Status::Timeout;
		} else if (!status::isSuccessful(st)) {
			return st;
		}

		st = qmutex_base::_unlock<WakeFn>(&__data->gatekeeper_mutex, flags);
		if (!status::isSuccessful(st)) {
			return st;
		}

		// Pre-increment counter;
		// This prevents existing readlocks from unlocking while this thread is active
		_rd_increment(__data, flags);

		st = qrwlock_base::_rdlock<WaitFn, ClockFn>(&__data->value, timeout, flags);

		// for timeout and errors we should decrement counter;
		// If counter was 1 before decrement, we possibly missed read wakeup;
		// We should perform it here;
		//
		// Theoretically, with fair API, there should not be read-waiters on this stage if
		// we are write-locked (readers will be blocked on mutex), but we should gracefully
		// process edge-cases
		if (st == Status::Timeout || (timeout && *timeout == 0)) {
			_rd_decrement_unlock(__data, flags, 0);
			return Status::Timeout;
		} else if (!status::isSuccessful(st)) {
			_rd_decrement_unlock(__data, flags, 0);
			return st;
		}

		return Status::Ok;
	}

	static Status _tryrdlock(value_type *__value) {
		value_type desired = ReadLock;
		value_type expected = 0;

		// We want to set value from 0 to WriteLock
		if (!_atomic::compareSwap(__value, &expected, desired)) [[unlikely]] {
			// check if read lock already set
			if ((expected & ReadLock) != ReadLock) {
				// read lock is NOT set, return with error
				return Status::ErrorBusy;
			}
		}
		return Status::Ok;
	}

	static Status _tryrdlock_fair(rwlock_data *__data, flags_type flags) {
		// First - try gatekeeper mutex
		if (qmutex_base::_try_lock(&__data->gatekeeper_mutex) == Status::ErrorBusy) {
			return Status::ErrorBusy;
		}

		// Gatekeeper passed - unlock it
		Status st =
				qmutex_base::_unlock<__sprt_sprt_qlock_wake_one>(&__data->gatekeeper_mutex, flags);
		if (!status::isSuccessful(st)) {
			return st;
		}

		// Pre-increment counter;
		// This prevents existing readlocks from unlocking while this thread is active
		_rd_increment(__data, flags);

		if (qrwlock_base::_tryrdlock(&__data->value) == Status::ErrorBusy) {
			/*
				Failure to acquire a read lock means it is locked for writing.
				Nevertheless, between the attempt to acquire and the decrement
				of the counter, another thread may obtain a read lock followed
				by its release. This makes the current thread responsible for
				releasing the read lock. 
			*/
			_rd_decrement_unlock(__data, flags, 0);
			return Status::ErrorBusy;
		}

		return Status::Ok;
	}

	template <int (*WaitFn)(value_type *, value_type, timeout_type, flags_type),
			timeout_type (*ClockFn)(flags_type)>
	static Status _wrlock(value_type *__value, timeout_type *timeout, flags_type flags) {
		timeout_type now = 0, next = 0;
		if constexpr (ClockFn != nullptr) {
			if (timeout) {
				now = ClockFn(flags);
			}
		}

		value_type desired = WriteLock;
		value_type expected = 0;

		// Read-lock is successful when we successfully perform 0 -> WriteLock
		if (!_atomic::compareSwap(__value, &expected, desired)) [[unlikely]] {
			// failed - should lock

			// Assume that there are other waiters
			desired |= Waiters;

			auto doLock = [&](value_type lockValue) {
				auto st = WaitFn(__value, lockValue,
						timeout ? *timeout : __SPRT_SPRT_TIMEOUT_INFINITE, flags);
				if (st != 0) {
					if (__sprt_errno == ETIMEDOUT) {
						return Status::Timeout;
					}
					if (__sprt_errno != EAGAIN) {
						return status::errnoToStatus(__sprt_errno);
					}
				}

				if constexpr (ClockFn != nullptr) {
					if (timeout) {
						next = ClockFn(flags);
						*timeout -= min((next - now), *timeout);
						now = next;
					}
				}

				expected = 0;
				return Status::Ok;
			};

			do {
				if constexpr (ClockFn != nullptr) {
					if (timeout && *timeout == 0) {
						return Status::Timeout;
					}
				}

				if ((expected & ~Waiters) != 0) {
					if ((expected & Waiters) == Waiters) {
						auto st = doLock(expected);
						if (st != Status::Ok) {
							return st;
						}
					} else {
						expected = _atomic::fetchOr(__value, Waiters) | Waiters;
						if ((expected & ~Waiters) != 0) {
							auto st = doLock(expected);
							if (st != Status::Ok) {
								return st;
							}
						}
					}
				}
			} while (!_atomic::compareSwap(__value, &expected, desired));
		}
		return Status::Ok;
	}

	template <
			// Function to wait on lock
			int (*WaitFn)(value_type *, value_type, timeout_type, flags_type),

			// Function to wake single waiter
			int (*WakeFn)(value_type *, flags_type),

			// Timeout function
			timeout_type (*ClockFn)(flags_type)>
	static Status _wrlock_fair(rwlock_data *__data, timeout_type *timeout, flags_type flags) {
		Status st = Status::Ok;

		// lock gatekeeper mutex
		st = qmutex_base::_lock<WaitFn, ClockFn>(&__data->gatekeeper_mutex, timeout, flags);
		if (!status::isSuccessful(st)) {
			return st;
		}

		st = qrwlock_base::_wrlock<WaitFn, ClockFn>(&__data->value, timeout, flags);
		return st;
	}

	static Status _trywrlock(value_type *__value) {
		value_type desired = WriteLock;
		value_type expected = 0;

		// We want to set value from 0 to WriteLock
		if (!_atomic::compareSwap(__value, &expected, desired)) [[unlikely]] {
			return Status::ErrorBusy;
		}
		return Status::Ok;
	};

	static Status _trywrlock_fair(rwlock_data *__data, flags_type flags) {
		if (qmutex_base::_try_lock(&__data->gatekeeper_mutex) == Status::ErrorBusy) {
			return Status::ErrorBusy;
		}

		auto st = _trywrlock(&__data->value);
		if (st == Status::ErrorBusy) {
			qmutex_base::_unlock<__sprt_sprt_qlock_wake_one>(&__data->gatekeeper_mutex, flags);
			return Status::ErrorBusy;
		}
		return st;
	}
};

} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_THREAD_QRWLOCK_H_
