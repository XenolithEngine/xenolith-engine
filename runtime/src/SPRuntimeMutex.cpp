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
#include <sprt/runtime/log.h>
#include <sprt/runtime/status.h>
#include "private/SPRTSpecific.h"

#include <sprt/c/__sprt_time.h>
#include <sprt/c/__sprt_stdlib.h>
#include <sprt/c/__sprt_errno.h>

namespace sprt {

SPRT_UNUSED static constexpr uint32_t FUTEX_V1_WAIT = 0;
SPRT_UNUSED static constexpr uint32_t FUTEX_V1_WAKE = 1;
SPRT_UNUSED static constexpr uint32_t FUTEX_V1_LOCK_PI = 6;
SPRT_UNUSED static constexpr uint32_t FUTEX_V1_UNLOCK_PI = 7;
SPRT_UNUSED static constexpr uint32_t FUTEX_V1_TRYLOCK_PI = 8;
SPRT_UNUSED static constexpr uint32_t FUTEX_V1_WAIT_BITSET = 9;
SPRT_UNUSED static constexpr uint32_t FUTEX_V1_WAKE_BITSET = 10;

SPRT_UNUSED static constexpr uint32_t FUTEX_V1_PRIVATE_FLAG = 128;
static constexpr uint32_t FUTEX_V1_CLOCK_REALTIME = 256;

SPRT_UNUSED SPRT_UNUSED static constexpr uint32_t FUTEX_V1_WAITERS = 0x8000'0000;
SPRT_UNUSED static constexpr uint32_t FUTEX_V1_OWNER_DIED = 0x4000'0000;
SPRT_UNUSED static constexpr uint32_t FUTEX_V1_TID_MASK = 0x3fff'ffff;

SPRT_UNUSED static constexpr uint32_t LOCK_VALUE = 0b0001;
SPRT_UNUSED static constexpr uint32_t WAIT_VALUE = 0b0010;
SPRT_UNUSED static constexpr uint32_t FULL_VALUE = LOCK_VALUE | WAIT_VALUE;

SPRT_UNUSED static constexpr uint32_t CLIENT_MASK = 0x01;
SPRT_UNUSED static constexpr uint32_t SERVER_MASK = 0x02;
SPRT_UNUSED static constexpr uint32_t FULL_MASK = CLIENT_MASK | SERVER_MASK;

SPRT_UNUSED static constexpr uint32_t FLAG_SIZE_U8 = 0x00;
SPRT_UNUSED static constexpr uint32_t FLAG_SIZE_U16 = 0x01;
SPRT_UNUSED static constexpr uint32_t FLAG_SIZE_U32 = 0x02;
SPRT_UNUSED static constexpr uint32_t FLAG_SIZE_U64 = 0x03;
SPRT_UNUSED static constexpr uint32_t FLAG_NUMA = 0x04;
SPRT_UNUSED static constexpr uint32_t FLAG_MPOL = 0x08;
SPRT_UNUSED static constexpr uint32_t FLAG_PRIVATE = 128;

static bool s_hasFutex2 = false;

static uint32_t sys_gettid() { return uint32_t(::syscall(SYS_GETTID)); }

static long futex_lock(volatile uint32_t *uaddr, uint32_t flags, _linux_timespec *timespec) {
	auto op = FUTEX_V1_LOCK_PI;
	if (hasFlag(flags, FLAG_PRIVATE)) {
		op |= FUTEX_V1_PRIVATE_FLAG;
	}

	return ::syscall(SYS_FUTEX_V1, uaddr, op, 0, &timespec);
}

static long futex_try_lock(volatile uint32_t *uaddr, uint32_t flags) {
	auto op = FUTEX_V1_TRYLOCK_PI;
	if (hasFlag(flags, FLAG_PRIVATE)) {
		op |= FUTEX_V1_PRIVATE_FLAG;
	}

	return ::syscall(SYS_FUTEX_V1, uaddr, op);
}

static long futex_unlock(volatile uint32_t *uaddr, uint32_t flags) {
	auto op = FUTEX_V1_UNLOCK_PI;
	if (hasFlag(flags, FLAG_PRIVATE)) {
		op |= FUTEX_V1_PRIVATE_FLAG;
	}

	return ::syscall(SYS_FUTEX_V1, uaddr, op);
}

static long futex_wake_v1(volatile uint32_t *uaddr, uint32_t bitset, int nr_wake, uint32_t flags) {
	auto op = bitset ? FUTEX_V1_WAKE_BITSET : FUTEX_V1_WAKE;
	if (hasFlag(flags, FLAG_PRIVATE)) {
		op |= FUTEX_V1_PRIVATE_FLAG;
	}

	if (bitset) {
		return ::syscall(SYS_FUTEX_V1, uaddr, op, nr_wake, nullptr, nullptr, bitset);
	} else {
		return ::syscall(SYS_FUTEX_V1, uaddr, op, nr_wake);
	}
}

static long futex_wait_v1(volatile uint32_t *uaddr, uint32_t val, uint32_t mask, uint32_t flags,
		_linux_timespec *timespec, __sprt_clockid_t clockid) {
	auto op = mask ? FUTEX_V1_WAIT_BITSET : FUTEX_V1_WAIT;
	if (hasFlag(flags, FLAG_PRIVATE)) {
		op |= FUTEX_V1_PRIVATE_FLAG;
	}

	if (clockid == __SPRT_CLOCK_REALTIME) {
		op |= FUTEX_V1_CLOCK_REALTIME;
	}

	if (mask) {
		return ::syscall(SYS_FUTEX_V1, uaddr, op, val, timespec, nullptr, mask);
	} else {
		return ::syscall(SYS_FUTEX_V1, uaddr, op, val, timespec);
	}
}

static long futex_wake(volatile uint32_t *uaddr, uint32_t bitset, int nr_wake, uint32_t flags) {
	if (s_hasFutex2) {
		return syscall(SYS_FUTEX_V2_WAKE, uaddr, bitset, nr_wake, flags);
	} else {
		return futex_wake_v1(uaddr, bitset, nr_wake, flags);
	}
}

static long futex_wait(volatile uint32_t *uaddr, uint32_t val, uint32_t mask, uint32_t flags,
		_linux_timespec *timespec, __sprt_clockid_t clockid) {
	if (s_hasFutex2) {
		return syscall(SYS_FUTEX_V2_WAIT, uaddr, val, mask, flags, timespec, clockid);
	} else {
		return futex_wait_v1(uaddr, val, mask, flags, timespec, clockid);
	}
}

static uint32_t atomicLoadSeq(volatile uint32_t *ptr) {
	uint32_t ret;
	__atomic_load(ptr, &ret, __ATOMIC_SEQ_CST);
	return ret;
}

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
				// futex should have all three flags set at this moment
				// wait for unlock
				auto ret = futex_wait(&_data.value, FULL_VALUE, s_hasFutex2 ? FULL_MASK : 0,
						FLAG_SIZE_U32 | FLAG_PRIVATE, nullptr, __SPRT_CLOCK_MONOTONIC);
				if (ret < 0) {
					auto err = __sprt_errno;
					log::vprint(log::LogType::Error, __SPRT_LOCATION, "sprt::qmutex",
							"Syscall error: ", err);
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
		auto ret = futex_wake(&_data.value, s_hasFutex2 ? FULL_MASK : 0, 1,
				FLAG_SIZE_U32 | FLAG_PRIVATE);
		if (ret < 0) {
			auto err = __sprt_errno;
			log::vprint(log::LogType::Error, __SPRT_LOCATION, "sprt::qmutex",
					"Syscall error: ", err);
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
	auto tid = tl_tid & FUTEX_V1_TID_MASK;
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
				if (futex_lock(&_data.value, FLAG_PRIVATE, nullptr) < 0) {
					auto err = __sprt_errno;
					switch (err) {
					case EAGAIN: break;
					case ETIMEDOUT: break;
					case EDEADLK:
						++_data.counter;
						return;
						break;
					default:
						log::vprint(log::LogType::Error, __SPRT_LOCATION, "sprt::recursive_qmutex",
								"Syscall error: ", err);
						break;
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
	auto tid = tl_tid & FUTEX_V1_TID_MASK;
	uint32_t expected = 0;
	// try to capture mutex with out tid
	if (!atomicCompareSwap(&_data.value, &expected, tid)) [[unlikely]] {
		// check if we are the owner
		if (expected == tid) {
			// we are the owner, increment and return
			++_data.counter;
			return true;
		} else {
			auto ret = futex_try_lock(&_data.value, FLAG_PRIVATE);
			if (futex_try_lock(&_data.value, FLAG_PRIVATE) == 0) {
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
				default:
					log::vprint(log::LogType::Error, __SPRT_LOCATION, "sprt::recursive_qmutex",
							"Syscall error: ", err);
					break;
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
	auto tid = tl_tid & FUTEX_V1_TID_MASK;
	auto value = atomicLoadSeq(&_data.value);
	if ((value & FUTEX_V1_TID_MASK) == tid) {
		--_data.counter;
		if (_data.counter > 0) {
			// some recursive locks still in place
			return false;
		}

		if ((value & FUTEX_V1_WAITERS) != 0) {
			// we know there are waiters, unlock via syscall
			futex_unlock(&_data.value, FLAG_PRIVATE);
			return true;
		} else {
			// try to release atomically
			uint32_t expected = tid;
			if (!atomicCompareSwap(&_data.value, &expected, 0)) {
				// there are waiters or an error, we don't care what - just unlock
				auto ret = futex_unlock(&_data.value, FLAG_PRIVATE);
				if (ret < 0) {
					log::vprint(log::LogType::Error, __SPRT_LOCATION, "sprt::recursive_qmutex",
							"Syscall error: ", __sprt_errno);
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


void setFutexVersion(int v) {
	if (v > 1) {
		s_hasFutex2 = true;
	}
}

} // namespace sprt
