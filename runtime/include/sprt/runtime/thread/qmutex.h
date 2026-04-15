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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_THREAD_QMUTEX_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_THREAD_QMUTEX_H_

#include <sprt/cxx/atomic>
#include <sprt/c/sys/__sprt_sprt.h>
#include <sprt/runtime/status.h>

namespace sprt {

class qmutex_base {
public:
	using value_type = __sprt_sprt_qlock_t;
	using timeout_type = __sprt_sprt_timeout_t;
	using flags_type = __sprt_sprt_lock_flags_t;

	static constexpr value_type LOCK_BIT = 0b0001;
	static constexpr value_type WAIT_BIT = 0b0010;
	static constexpr value_type COMPLETE_BIT = 0b0100;

	template <int (*WaitFn)(value_type *, value_type, timeout_type, flags_type),
			timeout_type (*ClockFn)(flags_type)>
	static Status _lock(value_type *__value, timeout_type *timeout, flags_type flags) {
		// try to mark futex to own it
		timeout_type now = 0, next = 0;
		if constexpr (ClockFn != nullptr) {
			if (timeout) {
				now = ClockFn(flags);
			}
		}

		uint32_t c = _atomic::fetchOr(__value, LOCK_BIT);
		if ((c & LOCK_BIT) != 0) {
			// prev value already has LOCK flag, wait
			do {
				if constexpr (ClockFn != nullptr) {
					if (timeout && *timeout == 0) {
						return Status::Timeout;
					}
				}

				// if wait flag is already set or we still locked
				if ((c & WAIT_BIT) != 0 || (_atomic::fetchOr(__value, WAIT_BIT) & LOCK_BIT) != 0) {
					// wait for unlock, if futex has both WAIT_BIT and LOCK_BIT
					auto st = WaitFn(__value, LOCK_BIT | WAIT_BIT,
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

				// Try to set LOCK_BIT, if fetched value & LOCK_BIT == 0 - we become owner;
				// We alse set WAIT_BIT to unlock other potential waiters,
				// цhich is not required if mutex was not locked on function entrance
			} while (((c = _atomic::fetchOr(__value, LOCK_BIT | WAIT_BIT)) & LOCK_BIT) != 0);
		}

		return Status::Ok;
	}

	static Status _try_lock(value_type *__value) {
		// returns true if successfully locked
		// set LOCK flag, return true if it was not set (so, we successfully locked)
		if ((_atomic::fetchOr(__value, LOCK_BIT) & LOCK_BIT) == 0) {
			return Status::Ok;
		} else {
			return Status::ErrorBusy;
		}
	}

	template <int (*WakeFn)(value_type *, flags_type)>
	static Status _unlock(value_type *__value, flags_type flags) {
		// unset all, return true if WAIT was set
		if ((_atomic::exchange(__value, uint32_t(0)) & WAIT_BIT) != 0) {
			// WAIT was set, we need to signal
			if (WakeFn(__value, flags) == 0) {
				return Status::Ok;
			}
			return status::errnoToStatus(__sprt_errno);
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
	using native_handle_type = value_type;

	~qmutex();

	qmutex() { }

	qmutex(const qmutex &) = delete;
	qmutex &operator=(const qmutex &) = delete;

	void lock() { _lock<__sprt_sprt_qlock_wait, nullptr>(&_data, 0, 0); }

	bool try_lock() { return _try_lock(&_data) == Status::Ok; }

	// returns true if we have some waiters
	bool unlock() { return _unlock<__sprt_sprt_qlock_wake_one>(&_data, 0) == Status::Ok; }

	native_handle_type native_handle() const noexcept { return _data; }

protected:
	value_type _data;
};

} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_THREAD_QMUTEX_H_
