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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_THREAD_QCONDVAR_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_THREAD_QCONDVAR_H_

#include <sprt/runtime/thread/qmutex.h>

namespace sprt {

struct alignas(8) __qcondvar_data {
	uint64_t mutexid = 0;
	qmutex_base::value_type counter = 0;
	qmutex_base::value_type value = 0;
	qmutex_base::value_type previous = 0;
	uint32_t padding = 0;
};

class qcondvar_base : public qmutex_base {
public:
	template < int (*WaitFn)(value_type *, value_type, timeout_type, flags_type),
			timeout_type (*ClockFn)(flags_type), uint64_t (*MutexId)(void *),
			Status (*LockFn)(void *), // mutex lock fn
			Status (*UnlockFn)(void *) // mutex unlock fn
			>
	static Status _wait(__qcondvar_data *data, void *mutex, timeout_type *timeout,
			flags_type flags) {
		uint64_t desired = MutexId(mutex);
		uint64_t expected = 0;
		if (__atomic_compare_exchange_n(&data->mutexid, &expected, desired, false, __ATOMIC_SEQ_CST,
					__ATOMIC_SEQ_CST)) {
			// condition captured by new mutex
			_atomic::fetchAdd(&data->counter, uint32_t(1));
		} else if (expected == desired) {
			// comdition was captured by this mutex
			_atomic::fetchAdd(&data->counter, uint32_t(1));
		} else {
			// captured by different mutex
			return Status::ErrorInvalidArguemnt;
		}

		value_type v = __atomic_load_n(&data->value, __ATOMIC_SEQ_CST);
		__atomic_store_n(&data->previous, v, __ATOMIC_SEQ_CST);

		Status result = Status::Ok;

		timeout_type now = 0, next = 0;
		if constexpr (ClockFn != nullptr) {
			if (timeout) {
				now = ClockFn(flags);
			}
		}

		UnlockFn(mutex);
		while (v == __atomic_load_n(&data->value, __ATOMIC_SEQ_CST)) {
			if (timeout == 0) {
				result = Status::Ok;
				break;
			}

			int r = WaitFn(&data->value, v, timeout ? *timeout : __SPRT_SPRT_TIMEOUT_INFINITE,
					flags);
			if (r != 0) {
				if (__sprt_errno == ETIMEDOUT) {
					result = Status::Timeout;
					break;
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

		result = LockFn(mutex);
		if (result == Status::Ok) {
			if (_atomic::fetchSub(&data->counter, 1U) == 1) {
				__atomic_store_n(&data->mutexid, uint64_t(0), __ATOMIC_SEQ_CST);
			}
		}
		return result;
	}

	template <int (*WakeFn)(value_type *, flags_type)>
	static Status _signal(__qcondvar_data *data, flags_type flags) {
		uint64_t mid = __atomic_load_n(&data->mutexid, __ATOMIC_SEQ_CST);
		if (mid == 0) {
			// no waiters
			return Status::Ok;
		}

		value_type v = 1u + __atomic_load_n(&data->previous, __ATOMIC_SEQ_CST);
		__atomic_store_n(&data->value, v, __ATOMIC_SEQ_CST);
		if (WakeFn(&data->value, flags) != 0) {
			return status::errnoToStatus(__sprt_errno);
		}
		return Status::Ok;
	}
};

} // namespace sprt

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_THREAD_QCONDVAR_H_
