/**
 Copyright (c) 2026 Xenolith Team <admin@stappler.org>

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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_THREAD_RMUTEX_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_THREAD_RMUTEX_H_

#include <sprt/cxx/atomic>
#include <sprt/c/sys/__sprt_sprt.h>
#include <sprt/runtime/status.h>

namespace sprt {

struct __rmutex_data {
#if SPRT_LINUX || SPRT_ANDROID
	// Futex PI Values
	using tid_type = uint32_t;

	static constexpr tid_type THREAD_ID_MASK = 0x3fff'ffffU;
	static constexpr tid_type VALUE_MASK = 0x3fff'ffffU;
	static constexpr tid_type OWNER_DIED = 0x4000'0000U;
	static constexpr tid_type WAITERS_BIT = 0x8000'0000U;

	SPRT_FORCEINLINE static tid_type *getNativeValue(__sprt_sprt_rlock_t &lock) noexcept {
		return &lock.u32_2;
	}

	SPRT_FORCEINLINE static const tid_type *getNativeValue(
			const __sprt_sprt_rlock_t &lock) noexcept {
		return &lock.u32_2;
	}

#else
	using tid_type = uint64_t;

	static constexpr tid_type THREAD_ID_MASK = 0xFFFF'FFFFLLU;
	static constexpr tid_type VALUE_MASK = 0xFFFF'FFFF'FFFFLLU;
	static constexpr tid_type OWNER_DIED = 0x1'0000'0000'0000LLU;
	static constexpr tid_type WAITERS_BIT = 0x2'0000'0000'0000LLU;

	SPRT_FORCEINLINE static tid_type *getNativeValue(__sprt_sprt_rlock_t &lock) noexcept {
		return &lock.u64;
	}

	SPRT_FORCEINLINE static const tid_type *getNativeValue(
			const __sprt_sprt_rlock_t &lock) noexcept {
		return &lock.u64;
	}

#endif
	__sprt_sprt_rlock_t value = {0};
	uint32_t counter = 0;
};

class rmutex_base {
public:
	using value_type = __sprt_sprt_rlock_t;
	using tid_type = __rmutex_data::tid_type;
	using timeout_type = __sprt_sprt_timeout_t;
	using flags_type = __sprt_sprt_lock_flags_t;

	static constexpr auto THREAD_ID_MASK = __rmutex_data::THREAD_ID_MASK;
	static constexpr auto VALUE_MASK = __rmutex_data::VALUE_MASK;
	static constexpr auto OWNER_DIED = __rmutex_data::OWNER_DIED;
	static constexpr auto WAITERS_BIT = __rmutex_data::WAITERS_BIT;

	SPRT_FORCEINLINE static auto getNativeValue(__sprt_sprt_rlock_t &lock) noexcept {
		return __rmutex_data::getNativeValue(lock);
	}

	SPRT_FORCEINLINE static auto getNativeValue(__rmutex_data &lock) noexcept {
		return __rmutex_data::getNativeValue(lock.value);
	}

	SPRT_FORCEINLINE static auto getNativeValue(const __sprt_sprt_rlock_t &lock) noexcept {
		return __rmutex_data::getNativeValue(lock);
	}

	SPRT_FORCEINLINE static auto getNativeValue(const __rmutex_data &lock) noexcept {
		return __rmutex_data::getNativeValue(lock.value);
	}

	/*
		Set SyscallLock to true when whole locking process should be performed with WaitFn

		returns:
		- Status::Ok on success
		- Status::Timeout - on timeout
		= Status::Propagate on recursive locking
	*/
	template <int (*WaitFn)(value_type *, value_type *, timeout_type, flags_type),
			timeout_type (*ClockFn)(flags_type), bool SyscallLock>
	static Status _lock(value_type &data, value_type threadId, timeout_type *timeout,
			flags_type flags, const Callback<void(const value_type &)> &prioCb = nullptr) {
		bool priorityProtectionEnabled = false;
		tid_type tidWithPrio = *getNativeValue(threadId) & VALUE_MASK;
		value_type expected = {0};

		timeout_type now = 0, next = 0;
		if constexpr (ClockFn != nullptr) {
			now = ClockFn(flags);
		}

		// try to capture mutex with our tid
		// We need not set WAITERS_BIT hee, as we assume that mutex is not locked and no waiters exists
		if (!_atomic::compareSwap(getNativeValue(data), getNativeValue(expected), tidWithPrio))
				[[unlikely]] {
			// failed - should lock
			do {
				if ((*getNativeValue(expected) & OWNER_DIED) != 0) {
					return Status::ErrorNotRecoverable;
				}

				// check if we are the owner
				if ((*getNativeValue(expected) & THREAD_ID_MASK)
						== (tidWithPrio & THREAD_ID_MASK)) {
					// we are the owner, notify about recursive locking
					return Status::Propagate;
				}

				if constexpr (ClockFn != nullptr) {
					if (*timeout && timeout == 0) {
						return Status::Timeout;
					}
				}

				if (!priorityProtectionEnabled && prioCb) {
					// here we can enable priority protection protocol
					prioCb(expected);
					priorityProtectionEnabled = true;
				}

				Status st = Status::Ok;
				if constexpr (SyscallLock) {
					if (WaitFn(&data, &expected, timeout ? *timeout : __SPRT_SPRT_TIMEOUT_INFINITE,
								flags)
							!= 0) {
						if (__sprt_errno != EAGAIN) {
							st = status::errnoToStatus(__sprt_errno);
						}
					}

					// Check if lock still in place by CAS
					// We will lock it with 0 -> TID or expected become TID;
					// If not - try lock one more time
					*getNativeValue(expected) = 0;
				} else {
					// We should wait if:
					// - Some value is set within VALUE_MASK
					// AND
					//   - WAITERS_BIT is set
					//   OR
					//   - We set WAITERS_BIT, and value in VALUE_MASK was not empty after this
					if (((*getNativeValue(expected) & VALUE_MASK) != 0)
							&& ((*getNativeValue(expected) & WAITERS_BIT) != 0
									// If it was not set - try to set it, then read current value to expected;
									// expected should be (fetched | WAITERS_BIT);
									// then we check it's value part against nonzero;
									// If value bits after WAITERS_BIT was set become empty - try to lock mutex without waiting
									|| ((*getNativeValue(expected) =
														_atomic::fetchOr(getNativeValue(data),
																WAITERS_BIT)
														| WAITERS_BIT)
											   & VALUE_MASK)
											!= 0)) {
						// we successfully set waiters flag (or it was set before)
						if (WaitFn(&data, &expected,
									timeout ? *timeout : __SPRT_SPRT_TIMEOUT_INFINITE, flags)
								!= 0) {
							if (__sprt_errno != EAGAIN) {
								st = status::errnoToStatus(__sprt_errno);
							}
						}

						// value was changed, now it's 0 or other thread id
						// assume 0 and try to set to our thread id
						*getNativeValue(expected) = 0;
					} else {
						// we have 0 or 0 | StateWaiters in expected, try to CompareSwap with it
					}
				}

				if (st != Status::Ok) {
					if (st == Status::ErrorDeadLock) {
						// The futex word at uaddr is already locked by the caller.
						return Status::Propagate;
					} else if (st == Status::ErrorTimeout) {
						return Status::Timeout;
					}
					return st;
				}

				if constexpr (ClockFn != nullptr) {
					if (timeout) {
						next = ClockFn(flags);
						*timeout -= min((next - now), *timeout);
						now = next;
					}
				}

				// Here we must assume that other threads may also be waiting to lock the mutex
				// That is, we must set WAITERS_BIT in order to correctly transfer control to them later
			} while (!_atomic::compareSwap(getNativeValue(data), getNativeValue(expected),
					tidWithPrio | WAITERS_BIT));
		}

		return Status::Ok;
	}

	template <int (*TryLockFn)(value_type *, flags_type)>
	static Status _try_lock(value_type &data, value_type threadId, flags_type flags) {
		tid_type tidWithPrio = *getNativeValue(threadId) & VALUE_MASK;
		value_type expected = {0};

		// try to capture mutex with out tid
		if (!_atomic::compareSwap(getNativeValue(data), getNativeValue(expected), tidWithPrio))
				[[unlikely]] {
			// check if we are the owner
			if ((*getNativeValue(expected) & THREAD_ID_MASK) == (tidWithPrio & THREAD_ID_MASK)) {
				// we are the owner, notify about recursive locking
				return Status::Propagate;
			}

			if constexpr (TryLockFn != nullptr) {
				if (TryLockFn(&data, flags) != 0) {
					auto st = status::errnoToStatus(__sprt_errno);
					if (st == Status::ErrorDeadLock) {
						// The futex word at uaddr is already locked by the caller.
						return Status::Propagate;
					}
					return st;
				} else {
					return Status::Ok;
				}
			}
			return Status::ErrorBusy;
		} else {
			return Status::Ok;
		}
	}

	/*
		Returns:
		- Status::Ok - unlock with syscall
		- Status::Done - unlock without syscall
		- Status::Propagate - counter was reduced, but lock still im place
		- Status::ErrorNotPermitted - not locked by this thread
	*/
	template <int (*WakeFn)(value_type *, flags_type)>
	static Status _unlock(value_type &data, value_type threadId, uint32_t *counter,
			flags_type flags) {
		tid_type tidWithPrio = *getNativeValue(threadId) & VALUE_MASK;
		value_type zero = {0};

		value_type expected;
		*getNativeValue(expected) = _atomic::loadRel(getNativeValue(data));

		if (counter && *counter <= 0) {
			return Status::ErrorNotPermitted;
		}

		if ((*getNativeValue(expected) & THREAD_ID_MASK) != (tidWithPrio & THREAD_ID_MASK)) {
			return Status::ErrorNotPermitted;
		}

		if (counter && --*counter > 0) {
			// some recursive locks still in place
			return Status::Propagate;
		}

		// We check if we already know about the waiting threads.
		// If we don’t know, then we try to atomically unlock the mutex.
		bool unlocked = false;
		if ((*getNativeValue(expected) & WAITERS_BIT) != 0
				|| !(unlocked = _atomic::compareSwap(getNativeValue(data), getNativeValue(expected),
							 *getNativeValue(zero)))) {
			// Now we know for sure that there are waiting threads
			if (!unlocked) {
				// CAS failed (Waiters flag was added)
				// or WAITERS_BIT was already set, CAS was not performed
				//
				// Force-set data to 0;
				_atomic::storeSeq(getNativeValue(data), *getNativeValue(zero));
			}
			if (WakeFn(&data, flags) != 0) {
				return status::errnoToStatus(__sprt_errno);
			}
			return Status::Ok;
		}
		return Status::Done;
	}
};

/*
	A slower mutex that supports recursive locking and priority
	inheritance features. Use as a general purpose mutex.
*/
class rmutex final : private rmutex_base {
public:
	~rmutex();

	void lock();
	bool try_lock();

	// returns true if we have some waiters
	bool unlock();

protected:
	__rmutex_data _data;
};

} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_THREAD_RMUTEX_H_
