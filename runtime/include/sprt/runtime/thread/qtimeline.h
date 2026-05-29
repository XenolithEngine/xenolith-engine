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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_THREAD_QTIMELINE_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_THREAD_QTIMELINE_H_

#include <sprt/runtime/thread/qmutex.h>
#include <sprt/c/__sprt_time.h>

namespace sprt {

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
	using value_type = __sprt_sprt_qlock_t;
	using timeout_type = __sprt_sprt_timeout_t;
	using flags_type = __sprt_sprt_lock_flags_t;

	static constexpr value_type VALUE_MASK = 0x7fff'ffffU;
	static constexpr value_type WAITERS_BIT = 0x8000'0000U;

	qtimeline(value_type initial = 0) : _data(initial) { }
	~qtimeline() { }

	Status wait(value_type val, flags_type f = 0) {
		// optimistic load, if success - returns without setting WaitersBit
		auto v = (_atomic::loadSeq(&_data));
		if ((v & VALUE_MASK) < val) {
			// wait on futex
			do {
				if ((v & WAITERS_BIT) != 0) {
					// WaitersBit was not dropped, just wait
					if (__sprt_sprt_qlock_wait(&_data, v, __SPRT_SPRT_TIMEOUT_INFINITE, f) != 0) {
						if (__sprt_errno != EAGAIN) {
							return status::errnoToStatus(__sprt_errno);
						}
					}
				} else {
					// set waiters bit, then wait if needed
					v = _atomic::fetchOr(&_data, WAITERS_BIT) | WAITERS_BIT;
					if ((v & VALUE_MASK) < val) {
						// if &_data.value is still == v -- wait until signal
						if (__sprt_sprt_qlock_wait(&_data, v, __SPRT_SPRT_TIMEOUT_INFINITE, f)
								!= 0) {
							if (__sprt_errno != EAGAIN) {
								return status::errnoToStatus(__sprt_errno);
							}
						}
					}
				}

				v = (_atomic::loadSeq(&_data));
			} while ((v & VALUE_MASK) < val);
		}
		return Status::Ok;
	}

	Status wait(value_type val, timeout_type __t, flags_type f = 0) {
		timeout_type now = __sprt_clock_gettime_nsec_np(__SPRT_CLOCK_MONOTONIC), next = 0;

		auto v = (_atomic::loadSeq(&_data));
		if ((v & VALUE_MASK) < val) {
			// wait on futex
			do {
				if (__t == 0) {
					return Status::ErrorTimeout;
				}

				if ((v & WAITERS_BIT) != 0) {
					// WaitersBit was not dropped, just wait
					if (__sprt_sprt_qlock_wait(&_data, v, __t, f) != 0) {
						if (__sprt_errno != EAGAIN) {
							return status::errnoToStatus(__sprt_errno);
						}
					}
				} else {
					// set waiters bit, then wait if needed
					v = _atomic::fetchOr(&_data, WAITERS_BIT) | WAITERS_BIT;
					if ((v & VALUE_MASK) < val) {
						// if &_data.value is still == v -- wait until signal
						if (__sprt_sprt_qlock_wait(&_data, v, __t, f) != 0) {
							if (__sprt_errno != EAGAIN) {
								return status::errnoToStatus(__sprt_errno);
							}
						}
					}
				}

				next = __sprt_sprt_qlock_now(f);
				__t -= min((next - now), __t);
				now = next;

				v = (_atomic::loadSeq(&_data));
			} while ((v & VALUE_MASK) < val);
		}
		return Status::Ok;
	}

	bool try_wait(value_type val) { return (_atomic::loadSeq(&_data) & VALUE_MASK) >= val; }

	void signal(value_type val, flags_type f = 0) {
		if (val == 0) {
			return;
		}
		if (_atomic::fetchAdd(&_data, val) & WAITERS_BIT) {
			_atomic::fetchAnd(&_data, VALUE_MASK); // Drop WAITERS_BIT
			__sprt_sprt_qlock_wake_all(&_data, f);
		}
	}

	void set_and_signal(value_type val, flags_type f = 0) {
		auto value = _atomic::exchange(&_data, val);
		if (value & WAITERS_BIT) {
			__sprt_sprt_qlock_wake_all(&_data, f);
		}
	}

	value_type get_value() const { return _atomic::loadSeq(&_data) & VALUE_MASK; }

protected:
	value_type _data;
};

} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_THREAD_QTIMELINE_H_
