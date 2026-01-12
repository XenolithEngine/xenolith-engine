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

#include <sprt/runtime/mutex.h>
#include <sprt/runtime/waitable_address.h>
#include <sprt/runtime/log.h>
#include <sprt/runtime/status.h>
#include "private/SPRTSpecific.h"

#include <sprt/c/__sprt_time.h>
#include <sprt/c/__sprt_stdlib.h>
#include <sprt/c/__sprt_errno.h>
#include <sprt/c/sys/__sprt_futex.h>

namespace sprt {

SPRT_UNUSED static constexpr uint32_t LOCK_VALUE = 0b0001;
SPRT_UNUSED static constexpr uint32_t WAIT_VALUE = 0b0010;
SPRT_UNUSED static constexpr uint32_t FULL_VALUE = LOCK_VALUE | WAIT_VALUE;

static uint32_t sys_gettid() { return uint32_t(::syscall(SYS_GETTID)); }

static void reportError(StringView tag, int err) {
	if (err != EAGAIN) {
		auto st = status::errnoToStatus(err);
		status::getStatusDescription(st, [&](StringView str) {
			log::vprint(log::LogType::Error, __SPRT_LOCATION, tag, "Syscall error: ", str);
		});
	}
}

static uint32_t atomicLoadSeq(volatile uint32_t *ptr) {
	return __atomic_load_n(ptr, __ATOMIC_SEQ_CST);
}

/*static void atomicStoreSeq(volatile uint32_t *ptr, uint32_t value) {
	__atomic_store_n(ptr, value, __ATOMIC_SEQ_CST);
}*/

static uint32_t atomicFetchOr(volatile uint32_t *ptr, uint32_t value) {
	return __atomic_fetch_or(ptr, value, __ATOMIC_SEQ_CST);
}

SPRT_UNUSED static uint32_t atomicFetchAnd(volatile uint32_t *ptr, uint32_t value) {
	return __atomic_fetch_and(ptr, value, __ATOMIC_SEQ_CST);
}

static uint32_t atomicExchange(volatile uint32_t *ptr, uint32_t value) {
	return __atomic_exchange_n(ptr, value, __ATOMIC_SEQ_CST);
}

static bool atomicCompareSwap(volatile uint32_t *ptr, uint32_t *expected, uint32_t desired) {
	return __atomic_compare_exchange_n(ptr, expected, desired, false, __ATOMIC_SEQ_CST,
			__ATOMIC_SEQ_CST);
}

qmutex::~qmutex() {
	auto value = atomicExchange(&_data.value, 0);
	if (value != 0) {
		log::vprint(log::LogType::Fatal, __SPRT_LOCATION, "sprt::qmutex",
				"Mutex is locked when it's destroyed, aborting");
		::__sprt_abort();
	}
}

void qmutex::lock() {
	// try to mark futex to own it
	uint32_t c = atomicFetchOr(&_data.value, LOCK_VALUE);
	if ((c & LOCK_VALUE) != 0) {
		// prev value already has LOCK flag, wait
		do {
			// if wait flag is set already or we still locked
			if ((c & WAIT_VALUE) != 0
					|| (atomicFetchOr(&_data.value, WAIT_VALUE) & LOCK_VALUE) != 0) {
				// wait for unlock, if futex has both WAIT_VALUE and LOCK_VALUE
				auto ret = __sprt_futex_wait(&_data.value, __SPRT_FUTEX_PRIVATE_FLAG, FULL_VALUE,
						nullptr);
				if (ret < 0) {
					reportError(__PRETTY_FUNCTION__, __sprt_errno);
				}
			}
			// check if lock still in place by fetching value and set all flags
		} while (((c = atomicFetchOr(&_data.value, FULL_VALUE)) & LOCK_VALUE) != 0);
	}
}

bool qmutex::try_lock() {
	// returns true if successfully locked
	// set LOCK flag, return true if it was not set (so, we successfully locked)
	return (atomicFetchOr(&_data.value, LOCK_VALUE) & LOCK_VALUE) == 0;
}

bool qmutex::unlock() {
	// unset all, return true if WAIT was set
	if ((atomicExchange(&_data.value, 0) & WAIT_VALUE) != 0) {
		// WAIT was set, we need to signal
		auto ret = __sprt_futex_wake(&_data.value, __SPRT_FUTEX_PRIVATE_FLAG, 1);
		if (ret < 0) {
			reportError(__PRETTY_FUNCTION__, __sprt_errno);
		}
		return true;
	}
	return false;
}

static thread_local uint32_t tl_tid = sys_gettid();

recursive_qmutex::~recursive_qmutex() {
	auto value = atomicExchange(&_data.value, 0);
	if (value != 0) {
		log::vprint(log::LogType::Fatal, __SPRT_LOCATION, "sprt::recursive_qmutex",
				"Mutex is locked when it's destroyed, aborting");
		::__sprt_abort();
	}
}

void recursive_qmutex::lock() {
	auto tid = tl_tid & __SPRT_FUTEX_TID_MASK;
	uint32_t expected = 0;
	// try to capture mutex with out tid
	if (!atomicCompareSwap(&_data.value, &expected, tid)) [[unlikely]] {
		// failed - should lock
		do {
			// check if we are the owner
			if (expected == tid) {
				// we are the owner, increment and return
				++_data.counter;
				return;
			} else {
				if (__sprt_futex_lock_pi(&_data.value, __SPRT_FUTEX_PRIVATE_FLAG, nullptr) < 0) {
					auto err = __sprt_errno;
					switch (err) {
					case EAGAIN: break;
					case ETIMEDOUT: break;
					case EDEADLK:
						++_data.counter;
						return;
						break;
					default: reportError(__PRETTY_FUNCTION__, err); break;
					}
				}

				// Check if lock still in place by CAS
				// We will lock it with 0 -> TID or expected become TID;
				// If not - locke one more time
				expected = 0;
			}
		} while (!atomicCompareSwap(&_data.value, &expected, tid));
	} else {
		++_data.counter;
	}
}

bool recursive_qmutex::try_lock() {
	auto tid = tl_tid & __SPRT_FUTEX_TID_MASK;
	uint32_t expected = 0;
	// try to capture mutex with out tid
	if (!atomicCompareSwap(&_data.value, &expected, tid)) [[unlikely]] {
		// check if we are the owner
		if (expected == tid) {
			// we are the owner, increment and return
			++_data.counter;
			return true;
		} else {
			auto ret = __sprt_futex_trylock_pi(&_data.value, __SPRT_FUTEX_PRIVATE_FLAG);
			if (ret == 0) {
				++_data.counter;
				return true;
			} else if (ret < 0) {
				auto err = __sprt_errno;
				switch (err) {
				case EAGAIN: break;
				case ETIMEDOUT: break;
				case EDEADLK:
					++_data.counter;
					return true;
					break;
				default: reportError(__PRETTY_FUNCTION__, err); break;
				}
			}
			return false;
		}
	} else {
		++_data.counter;
		return true;
	}
}

bool recursive_qmutex::unlock() {
	auto tid = tl_tid & __SPRT_FUTEX_TID_MASK;
	auto value = atomicLoadSeq(&_data.value);
	if ((value & __SPRT_FUTEX_TID_MASK) == tid) {
		--_data.counter;
		if (_data.counter > 0) {
			// some recursive locks still in place
			return false;
		}

		if ((value & __SPRT_FUTEX_WAITERS) != 0) {
			// we know there are waiters, unlock via syscall
			__sprt_futex_unlock_pi(&_data.value, __SPRT_FUTEX_PRIVATE_FLAG);
			return true;
		} else {
			// try to release atomically
			uint32_t expected = tid;
			if (!atomicCompareSwap(&_data.value, &expected, 0)) {
				// there are waiters or an error, we don't care what - just unlock
				auto ret = __sprt_futex_unlock_pi(&_data.value, __SPRT_FUTEX_PRIVATE_FLAG);
				if (ret < 0) {
					reportError(__PRETTY_FUNCTION__, __sprt_errno);
				}
				return true;
			} else {
				// successful, no waiters
				return false;
			}
		}
	}
	log::vprint(log::LogType::Fatal, __SPRT_LOCATION, "sprt::recursive_qmutex",
			"Mutex is locked by another thread");
	::__sprt_abort();
	return false;
}

static constexpr waitable_address::value_type WaitersBit = 0x8000'0000;

waitable_address::~waitable_address() { }

void waitable_address::wait_value(value_type val) {
	// optimistic load, if success - returns without setting WaitersBit
	auto v = (atomicLoadSeq(&_data.value));
	if ((v & value_mask) != val) {
		int ret = 0;
		// wait on futex
		do {
			if ((v & WaitersBit) != 0) {
				// WaitersBit was not dropped, just wait
				ret = __sprt_futex_wait(&_data.value, __SPRT_FUTEX_PRIVATE_FLAG, v, nullptr);
				if (ret < 0) {
					reportError(__PRETTY_FUNCTION__, __sprt_errno);
				}
			} else {
				// set waiters bit, then wait if needed
				v = atomicFetchOr(&_data.value, WaitersBit);
				if ((v & value_mask) != val) {
					// if &_data.value is still == v - wait until signal
					ret = __sprt_futex_wait(&_data.value, __SPRT_FUTEX_PRIVATE_FLAG, FULL_VALUE,
							nullptr);
					if (ret < 0) {
						reportError(__PRETTY_FUNCTION__, __sprt_errno);
					}
				}
			}

			v = (atomicLoadSeq(&_data.value));

			/*ret = __sprt_futex_wait(&_data.value, __SPRT_FUTEX_PRIVATE_FLAG, v, nullptr);
			if (ret < 0) {
				reportError(__PRETTY_FUNCTION__, __sprt_errno);
			}
			v = (atomicLoadSeq(&_data.value) & value_mask);*/
		} while ((v & value_mask) != val);
	}
}

bool waitable_address::try_value(value_type val) {
	return (atomicLoadSeq(&_data.value) & value_mask) == val;
}

void waitable_address::set_and_signal(value_type val) {
	if (atomicExchange(&_data.value, val) & WaitersBit) {
		auto ret = __sprt_futex_wake(&_data.value, __SPRT_FUTEX_PRIVATE_FLAG, __SPRT_INT_MAX);
		if (ret < 0) {
			reportError(__PRETTY_FUNCTION__, __sprt_errno);
		}
	}

	/*atomicStoreSeq(&_data.value, val);
	auto ret = __sprt_futex_wake(&_data.value, __SPRT_FUTEX_PRIVATE_FLAG, __SPRT_INT_MAX);
	if (ret < 0) {
		reportError(__PRETTY_FUNCTION__, __sprt_errno);
	}*/
}

} // namespace sprt
