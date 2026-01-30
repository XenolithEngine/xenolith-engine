/**
Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_MUTEX_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_MUTEX_H_

#include <sprt/runtime/int.h>
#include <sprt/runtime/atomic.h>
#include <sprt/runtime/status.h>
#include <sprt/c/__sprt_assert.h>

namespace sprt {

#if SPRT_LINUX || SPRT_ANDROID
struct __qmutex_data {
	using value_type = uint32_t;

	value_type value = 0;
};

struct __rmutex_data {
	// Futex PI Values
	static constexpr uint32_t VALUE_MASK = 0x3fff'ffffU;
	static constexpr uint32_t OWNER_DIED = 0x4000'0000U;
	static constexpr uint32_t WAITERS_BIT = 0x8000'0000U;

	using value_type = uint32_t;

	value_type value = 0;
	uint32_t counter = 0;
};
#elif SPRT_WINDOWS
struct __qmutex_data {
	using value_type = uint32_t;

	value_type value = 0;
};

struct __rmutex_data {
	static constexpr uint64_t VALUE_MASK = 0xFFFF'FFFFLLU;
	static constexpr uint64_t OWNER_DIED = 0x1'0000'0000LLU;
	static constexpr uint64_t WAITERS_BIT = 0x2'0000'0000LLU;

	using value_type = uint64_t;

	value_type value = 0;
	uint32_t counter = 0;
};
#else
#error TODO
#endif

class qmutex_base {
public:
	using value_type = __qmutex_data::value_type;
	using timeout_type = uint64_t;

	static constexpr value_type LOCK_BIT = 0b0001;
	static constexpr value_type WAIT_BIT = 0b0010;

	template <Status (*WaitFn)(volatile value_type *, value_type, timeout_type),
			timeout_type (*ClockFn)()>
	static Status _lock(volatile value_type *__value, timeout_type *timeout) {
		// try to mark futex to own it
		timeout_type now = 0, next = 0;
		if constexpr (ClockFn != nullptr) {
			if (timeout) {
				now = ClockFn();
			}
		}

		uint32_t c = _atomic::fetchOr(__value, LOCK_BIT);
		if ((c & LOCK_BIT) != 0) {
			// prev value already has LOCK flag, wait
			do {
				if constexpr (ClockFn != nullptr) {
					if (timeout && *timeout == 0) {
						return Status::ErrorTimeout;
					}
				}

				// if wait flag is already set or we still locked
				if ((c & WAIT_BIT) != 0 || (_atomic::fetchOr(__value, WAIT_BIT) & LOCK_BIT) != 0) {
					// wait for unlock, if futex has both WAIT_BIT and LOCK_BIT
					auto st = WaitFn(__value, LOCK_BIT | WAIT_BIT, timeout ? *timeout : 0);
					if (st != Status::Ok) {
						return st;
					}

					if constexpr (ClockFn != nullptr) {
						if (timeout) {
							next = ClockFn();
							*timeout -= min((next - now), *timeout);
							now = next;
						}
					}
				}

				// Try to set LOCK_BIT, if fetched value & LOCK_BIT == 0 - we become owner;
				// We alse set WAIT_BIT to unlock other potential waiters,
				// цhich is not required if mutex was not locked on function entrance
			} while (((c = _atomic::fetchOr(__value, LOCK_BIT | WAIT_BIT)) & LOCK_BIT) != 0);
		}

		return Status::Ok;
	}

	static Status _try_lock(volatile value_type *__value) {
		// returns true if successfully locked
		// set LOCK flag, return true if it was not set (so, we successfully locked)
		if ((_atomic::fetchOr(__value, LOCK_BIT) & LOCK_BIT) == 0) {
			return Status::Ok;
		} else {
			return Status::ErrorBusy;
		}
	}

	template <void (*WakeFn)(volatile value_type *)>
	static Status _unlock(volatile value_type *__value) {
		// unset all, return true if WAIT was set
		if ((_atomic::exchange(__value, uint32_t(0)) & WAIT_BIT) != 0) {
			// WAIT was set, we need to signal
			WakeFn(__value);
			return Status::Ok;
		}
		return Status::Done;
	}
};

class rmutex_base {
public:
	using value_type = __rmutex_data::value_type;
	using timeout_type = uint64_t;

	static constexpr value_type VALUE_MASK = __rmutex_data::VALUE_MASK;
	static constexpr value_type OWNER_DIED = __rmutex_data::OWNER_DIED;
	static constexpr value_type WAITERS_BIT = __rmutex_data::WAITERS_BIT;

	/*
		Set SyscallLock to true when whole locking process should be performed with WaitFn
	*/
	template <Status (*WaitFn)(volatile value_type *, value_type *, timeout_type),
			timeout_type (*ClockFn)(), bool SyscallLock>
	static Status _lock(__rmutex_data &data, value_type threadId, timeout_type *timeout) {
		value_type tid = threadId & VALUE_MASK;
		value_type expected = 0;

		timeout_type now = 0, next = 0;
		if constexpr (ClockFn != nullptr) {
			now = ClockFn();
		}

		// try to capture mutex with our tid
		// We need not set WAITERS_BIT hee, as we assume that mutex is not locked and no waiters exists
		if (!_atomic::compareSwap(&data.value, &expected, tid)) [[unlikely]] {
			// failed - should lock
			do {
				if ((expected & OWNER_DIED) != 0) {
					return Status::ErrorNotRecoverable;
				}

				// check if we are the owner
				if ((expected & VALUE_MASK) == tid) {
					// we are the owner, increment and return
					break;
				}

				if constexpr (ClockFn != nullptr) {
					if (*timeout && timeout == 0) {
						return Status::ErrorTimeout;
					}
				}

				Status st = Status::Ok;
				if constexpr (SyscallLock) {
					st = WaitFn(&data.value, &expected, timeout ? *timeout : 0);

					// Check if lock still in place by CAS
					// We will lock it with 0 -> TID or expected become TID;
					// If not - try lock one more time
					expected = 0;
				} else {
					// If value bits is empty - we should try to CAS out TID again, but with updated
					// expected value
					if (((expected & VALUE_MASK) == 0)
							// Or, if WAITERS_BIT is set - we should wait unconditionally
							&& ((expected & WAITERS_BIT) != 0
									// If it was not set - try to set it, then read current value to expected;
									// expected should be (fetched | WAITERS_BIT);
									// then we check it's value part against nonzero;
									// If value bits is empty after WAITERS_BIT was set become empty - try to lock mutex without waiting
									|| ((expected = _atomic::fetchOr(&data.value, WAITERS_BIT)
														| WAITERS_BIT)
											   & VALUE_MASK)
											!= 0)) {
						// we successfully set waiters flag (or it was set before)
						st = WaitFn(&data.value, &expected, timeout ? *timeout : 0);

						// value was changed, now it's 0 or other thread id
						// assume 0 and try to set to our thread id
						expected = 0;
					} else {
						// we have 0 or 0 | StateWaiters in expected, try to CompareSwap with it
					}
				}

				if (st != Status::Ok) {
					if (st == Status::ErrorDeadLock) {
						++data.counter;
					}
					return st;
				}

				if constexpr (ClockFn != nullptr) {
					if (timeout) {
						next = ClockFn();
						*timeout -= min((next - now), *timeout);
						now = next;
					}
				}

				// Here we must assume that other threads may also be waiting to lock the mutex
				// That is, we must set WAITERS_BIT in order to correctly transfer control to them later
			} while (!_atomic::compareSwap(&data.value, &expected, tid | WAITERS_BIT));
		}

		++data.counter;
		return Status::Ok;
	}

	template <bool (*TryLockFn)(volatile value_type *)>
	static Status _try_lock(__rmutex_data &data, value_type threadId) {
		value_type tid = threadId & VALUE_MASK;
		value_type expected = 0;
		// try to capture mutex with out tid
		if (!_atomic::compareSwap(&data.value, &expected, tid)) [[unlikely]] {
			// check if we are the owner
			if ((expected & VALUE_MASK) == tid) {
				// we are the owner, increment and return
				++data.counter;
				return Status::Ok;
			}

			if constexpr (TryLockFn != nullptr) {
				if (TryLockFn(&data.value)) {
					++data.counter;
					return Status::Ok;
				}
			}
			return Status::ErrorBusy;
		} else {
			++data.counter;
			return Status::Ok;
		}
	}

	template <void (*WakeFn)(volatile value_type *)>
	static Status _unlock(__rmutex_data &data, value_type threadId) {
		value_type tid = threadId & VALUE_MASK;
		auto expected = _atomic::loadRel(&data.value);

		if ((expected & VALUE_MASK) != tid || data.counter <= 0) {
			return Status::ErrorNotPermitted;
		}

		if (--data.counter > 0) {
			// some recursive locks still in place
			return Status::Suspended;
		}

		// We check if we already know about the waiting threads.
		// If we don’t know, then we try to atomically unlock the mutex.
		if ((expected & WAITERS_BIT) != 0
				|| !_atomic::compareSwap(&data.value, &expected, value_type(0))) {
			// Now we know for sure that there are waiting threads
			WakeFn(&data.value);
			return Status::Ok;
		}
		return Status::Done;
	}
};

/*
	A simpliest and fastest mutex. Designed for minimal synchronization, potentially
	does not implement priority management functions and other tasks critical to
	overall performance. Use where mutual locks are not expected and the locks
	themselves take minimal time.
*/
class qmutex final : private qmutex_base {
public:
	~qmutex();

	void lock();
	bool try_lock();

	// returns true if we have some waiters
	bool unlock();

protected:
	__qmutex_data _data;
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

/*
	Timeline (Timeline Semaphore) - this is a synchronization primitive that allows threads
	to wait for changes in the lifecycle of a protected object.

	It is based on a monotonically increasing unsigned counter. Each call to 'signal'
	increments the counter by 1 or more units.
	
	The 'wait' call blocks the thread, waiting
	for a value greater than or equal to the counter value. If the value is already greater
	than or equal to the target, the thread is not blocked.
	
	The `try_wait` function returns true if current value is greater than or equal to the target,
	or false otherwise.

	The 'get_value' function returns the current timeline value to calculate the next value
	passed to 'wait' or 'try_wait'. Please do not use 'get_value' as 'try_wait' - this will
	lead to synchronization errors.

	This is a low-level primitive used for higher-level constructs like prommise/future and
	async/await options.
*/
class qtimeline final {
public:
	using value_type = __qmutex_data::value_type;

	static constexpr value_type value_mask = 0x7fff'ffff;

	qtimeline(value_type initial = 0);
	~qtimeline();

	void wait(value_type);
	bool try_wait(value_type);

	void signal(value_type increment = 1);

	value_type get_value() const;

protected:
	__qmutex_data _data;
};

/*
	std::unique_lock
*/
template <class _Mutex>
class unique_lock {
public:
	typedef _Mutex mutex_type;

private:
	mutex_type *__m_;
	bool __owns_;

public:
	[[nodiscard]]
	unique_lock() noexcept
	: __m_(nullptr), __owns_(false) { }

	[[nodiscard]]
	explicit unique_lock(mutex_type &__m)
	: __m_(addressof(__m)), __owns_(true) {
		__m_->lock();
	}

	~unique_lock() {
		if (__owns_) {
			__m_->unlock();
		}
	}

	unique_lock(unique_lock const &) = delete;
	unique_lock &operator=(unique_lock const &) = delete;

	[[nodiscard]]
	unique_lock(unique_lock &&__u) noexcept
	: __m_(__u.__m_), __owns_(__u.__owns_) {
		__u.__m_ = nullptr;
		__u.__owns_ = false;
	}

	unique_lock &operator=(unique_lock &&__u) noexcept {
		if (__owns_) {
			__m_->unlock();
		}

		__m_ = __u.__m_;
		__owns_ = __u.__owns_;
		__u.__m_ = nullptr;
		__u.__owns_ = false;
		return *this;
	}

	void lock() noexcept {
		sprt_passert(__m_ == nullptr, "unique_lock::lock: references null mutex");
		sprt_passert(!__owns_, "unique_lock::lock: already locked");

		__m_->lock();
		__owns_ = true;
	}

	bool try_lock() noexcept {
		sprt_passert(__m_ == nullptr, "unique_lock::lock: references null mutex");
		sprt_passert(!__owns_, "unique_lock::lock: already locked");

		__owns_ = __m_->try_lock();
		return __owns_;
	}

	void unlock() {
		sprt_passert(__owns_, "unique_lock::unlock: not locked");

		__m_->unlock();
		__owns_ = false;
	}

	mutex_type *release() noexcept {
		mutex_type *__m = __m_;
		__m_ = nullptr;
		__owns_ = false;
		return __m;
	}

	bool owns_lock() const noexcept { return __owns_; }
	explicit operator bool() const noexcept { return __owns_; }
	mutex_type *mutex() const noexcept { return __m_; }
};

} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_MUTEX_H_
