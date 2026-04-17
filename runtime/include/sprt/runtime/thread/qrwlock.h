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
		value_type mutex = 0;

		// Readers counter, reader should not release lock if this counter is not 0
		uint32_t counter = 0;
	};

	template <int (*WakeFn)(value_type *, flags_type)>
	static Status _funlock(value_type *__value, flags_type flags, value_type unlockMode) {
		auto expected = _atomic::loadSeq(__value);
		if ((expected & unlockMode) != 0) {
			_atomic::storeSeq(__value, value_type(0));
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

	template <int (*WakeFn)(value_type *, flags_type)>
	static Status _unlock_fair(rwlock_data *__data, flags_type flags, value_type type = 0) {
		Status ret = Status::Ok;

		auto expected = _atomic::loadSeq(&__data->value);

		if (type != 0 && (expected & type) == 0) {
			return Status::ErrorInvalidArguemnt;
		}

		if ((expected & ReadLock) != 0) {
			bool shouldUnlock = (_atomic::fetchSub(&__data->counter, uint32_t(1)) == 1);
			if (!shouldUnlock) {
				return Status::Propagate;
			}
		}

		auto current = _atomic::exchange(&__data->value, value_type(0));
		if ((current & Waiters) != 0) {
			if (WakeFn(&__data->value, flags) != 0) {
				ret = status::errnoToStatus(__sprt_errno);
			}
		}
		if ((current & WriteLock) != 0) {
			qmutex_base::_unlock<WakeFn>(&__data->mutex, flags);
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

				// set waiters flag
				if ((expected & Waiters) != 0
						// If we set Waiters flag, and original value is not WriteLock - we should not wait
						|| ((expected = _atomic::fetchOr(__value, Waiters) | Waiters) & ~Waiters)
								== WriteLock) {
					// we successfully set waiters flag (or it was set before)
					auto st = WaitFn(__value, expected,
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
				}

				// value was changed, now it's 0 or other lock value
				// assume 0 and try to set lock with ReadLock
				expected = 0;
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

		// do lock-unlock cycle on mutex
		// this mutex prevents reader to lock if there are writer waiting
		st = qmutex_base::_lock<WaitFn, ClockFn>(&__data->mutex, timeout, flags);

		if (st == Status::Timeout || (timeout && *timeout == 0)) {
			return Status::Timeout;
		} else if (!status::isSuccessful(st)) {
			return st;
		}

		st = qmutex_base::_unlock<WakeFn>(&__data->mutex, flags);
		if (!status::isSuccessful(st)) {
			return st;
		}

		// Pre-increment counter;
		// This prevents existing readlocks from unlocking while this thread is active
		_atomic::fetchAdd(&__data->counter, value_type(1));

		st = qrwlock_base::_rdlock<WaitFn, ClockFn>(&__data->value, timeout, flags);

		// for timeout and errors we should decrement counter;
		// If counter was 1 before decrement, we possibly missed read wakeup;
		// We should perform it here;
		//
		// Theoretically, with fair API, there should not be read-waiters on this stage if
		// we are write-locked (readers will be blocked on mutex), but we should gracefully
		// process edge-cases
		if (st == Status::Timeout || (timeout && *timeout == 0)) {
			if (_atomic::fetchSub(&__data->counter, value_type(1)) == 0) {
				qrwlock_base::_funlock<WakeFn>(&__data->value, flags, ReadLock);
			}
			return Status::Timeout;
		} else if (!status::isSuccessful(st)) {
			if (_atomic::fetchSub(&__data->counter, value_type(1)) == 0) {
				qrwlock_base::_funlock<WakeFn>(&__data->value, flags, ReadLock);
			}
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

	template <int (*WakeFn)(value_type *, flags_type)>
	static Status _tryrdlock_fair(rwlock_data *__data, flags_type flags) {
		// First - try gatekeeper mutex
		if (qmutex_base::_try_lock(&__data->mutex) == Status::ErrorBusy) {
			return Status::ErrorBusy;
		}

		// Gatekeeper passed - unlock it
		Status st = qmutex_base::_unlock<WakeFn>(&__data->mutex, flags);
		if (!status::isSuccessful(st)) {
			return st;
		}

		// Pre-increment counter;
		// This prevents existing readlocks from unlocking while this thread is active
		_atomic::fetchAdd(&__data->counter, value_type(1));

		if (qrwlock_base::_tryrdlock(&__data->value) == Status::ErrorBusy) {
			if (_atomic::fetchSub(&__data->counter, value_type(1)) == 0) {
				qrwlock_base::_funlock<WakeFn>(&__data->value, flags, ReadLock);
			}
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
			do {
				if constexpr (ClockFn != nullptr) {
					if (timeout && *timeout == 0) {
						return Status::Timeout;
					}
				}

				// set waiters flag
				if ((expected & Waiters) != 0
						|| ((expected = _atomic::fetchOr(__value, Waiters) | Waiters) & ~Waiters)
								!= 0) {
					// we successfully set waiters flag (or it was set before)
					auto st = WaitFn(__value, expected,
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
				}

				// value was changed, now it's 0 or other lock value
				// assume 0 and try to set lock with WriteLock
				expected = 0;
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
		st = qmutex_base::_lock<WaitFn, ClockFn>(&__data->mutex, timeout, flags);
		if (!status::isSuccessful(st)) {
			return st;
		}

		return qrwlock_base::_wrlock<WaitFn, ClockFn>(&__data->value, timeout, flags);
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

	template <int (*WakeFn)(value_type *, flags_type)>
	static Status _trywrlock_fair(rwlock_data *__data, flags_type flags) {
		if (qmutex_base::_try_lock(&__data->mutex) == Status::ErrorBusy) {
			return Status::ErrorBusy;
		}

		auto st = _trywrlock(&__data->value);
		if (st == Status::ErrorBusy) {
			qmutex_base::_unlock<WakeFn>(&__data->mutex, flags);
			return Status::ErrorBusy;
		}
		return st;
	}
};

} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_THREAD_QRWLOCK_H_
