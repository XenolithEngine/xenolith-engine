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

#include <sprt/c/__sprt_time.h>
#include <sprt/c/__sprt_stdlib.h>
#include <sprt/c/__sprt_errno.h>
#include <sprt/c/sys/__sprt_futex.h>

#include <sys/winapi.h>

#include "private/SPRTSpecific.h"

namespace sprt {

static void reportError(StringView tag, int err) {
	if (err != EAGAIN) {
		auto st = status::errnoToStatus(err);
		status::getStatusDescription(st, [&](StringView str) {
			log::vprint(log::LogType::Error, __SPRT_LOCATION, tag, "Syscall error: ", str);
		});
	}
}

#if SPRT_LINUX || SPRT_ANDROID

static inline auto sys_gettid() { return uint32_t(::syscall(SYS_GETTID)); }

static Status qmutex_wait(volatile uint32_t *value, uint32_t expected, uint64_t) {
	auto ret = __sprt_futex_wait(value, __SPRT_FUTEX_PRIVATE_FLAG, expected, nullptr);
	if (ret < 0) {
		reportError(__PRETTY_FUNCTION__, __sprt_errno);
	}
	return Status::Ok;
}

static void qmutex_wake(volatile uint32_t *value) {
	auto ret = __sprt_futex_wake(value, __SPRT_FUTEX_PRIVATE_FLAG, 1);
	if (ret < 0) {
		reportError(__PRETTY_FUNCTION__, __sprt_errno);
	}
}

static void qtimeline_wake(volatile uint32_t *value) {
	auto ret = __sprt_futex_wake(value, __SPRT_FUTEX_PRIVATE_FLAG, __SPRT_INT_MAX);
	if (ret < 0) {
		reportError(__PRETTY_FUNCTION__, __sprt_errno);
	}
}

static constexpr bool RMutexSyscallLock = true;

static Status rmutex_wait(volatile uint32_t *value, uint32_t *expected, uint64_t) {
	if (__sprt_futex_lock_pi(value, __SPRT_FUTEX_PRIVATE_FLAG, nullptr) < 0) {
		auto err = __sprt_errno;
		switch (err) {
		case EAGAIN: break;
		case ETIMEDOUT: break;
		case EDEADLK: break;
		default: reportError(__PRETTY_FUNCTION__, err); break;
		}
		return status::errnoToStatus(err);
	}
	return Status::Ok;
}

static bool rmutex_try(volatile uint32_t *value) {
	auto ret = __sprt_futex_trylock_pi(value, __SPRT_FUTEX_PRIVATE_FLAG);
	if (ret == 0) {
		return true;
	} else if (ret < 0) {
		auto err = __sprt_errno;
		switch (err) {
		case EAGAIN: break;
		case ETIMEDOUT: break;
		case EDEADLK: return true; break;
		default: reportError(__PRETTY_FUNCTION__, err); break;
		}
	}
	return false;
}

static void rmutex_wake(volatile uint32_t *value) {
	__sprt_futex_unlock_pi(value, __SPRT_FUTEX_PRIVATE_FLAG);
}

#elif SPRT_WINDOWS

static inline auto sys_gettid() { return ::_GetCurrentThreadId(); }

static Status qmutex_wait(volatile uint32_t *value, uint32_t expected, uint64_t) {
	auto ret = _WaitOnAddress(value, &expected, sizeof(uint32_t), _WINAPI_INFINITE);
	if (!ret) {
		reportError(__PRETTY_FUNCTION__, platform::lastErrorToErrno(_GetLastError()));
	}
	return Status::Ok;
}

static void qmutex_wake(volatile uint32_t *value) { _WakeByAddressSingle((void *)value); }

static void qtimeline_wake(volatile uint32_t *value) { _WakeByAddressAll((void *)value); }

static constexpr bool RMutexSyscallLock = false;

static Status rmutex_wait(volatile uint64_t *value, uint64_t *expected, uint64_t) {
	auto ret = _WaitOnAddress(value, expected, sizeof(uint64_t), _WINAPI_INFINITE);
	if (!ret) {
		reportError(__PRETTY_FUNCTION__, platform::lastErrorToErrno(_GetLastError()));
	}
	return Status::Ok;
}

static bool rmutex_try(volatile uint64_t *value) { return false; }

static void rmutex_wake(volatile uint64_t *value) {
	_atomic::storeSeq(value, uint64_t(0));
	_WakeByAddressSingle((void *)value);
}
#else
#error TODO
#endif

static thread_local uint32_t tl_tid = sys_gettid();

qmutex::~qmutex() {
	auto value = _atomic::exchange(&_data.value, uint32_t(0));
	if (value != 0) {
		log::vprint(log::LogType::Fatal, __SPRT_LOCATION, "sprt::qmutex",
				"Mutex is locked when it's destroyed, aborting");
		::__sprt_abort();
	}
}

void qmutex::lock() { _lock<qmutex_wait, nullptr>(&_data.value, 0); }

bool qmutex::try_lock() { return _try_lock(&_data.value) == Status::Ok; }

bool qmutex::unlock() { return _unlock<qmutex_wake>(&_data.value) == Status::Ok; }

rmutex::~rmutex() {
	auto value = _atomic::exchange(&_data.value, decltype(_data.value)(0));
	if (value != 0) {
		log::vprint(log::LogType::Fatal, __SPRT_LOCATION, "sprt::recursive_qmutex",
				"Mutex is locked when it's destroyed, aborting");
		::__sprt_abort();
	}
}

void rmutex::lock() { _lock<rmutex_wait, nullptr, RMutexSyscallLock>(_data, tl_tid, 0); }

bool rmutex::try_lock() { return _try_lock<rmutex_try>(_data, tl_tid) == Status::Ok; }

bool rmutex::unlock() { return _unlock<rmutex_wake>(_data, tl_tid) == Status::Ok; }

static constexpr qtimeline::value_type WaitersBit = 0x8000'0000;

qtimeline::qtimeline(value_type v) : _data({v}) { }

qtimeline::~qtimeline() { }

void qtimeline::wait(value_type val) {
	// optimistic load, if success - returns without setting WaitersBit
	auto v = (_atomic::loadSeq(&_data.value));
	if ((v & value_mask) < val) {
		// wait on futex
		do {
			if ((v & WaitersBit) != 0) {
				// WaitersBit was not dropped, just wait
				qmutex_wait(&_data.value, v, 0);
			} else {
				// set waiters bit, then wait if needed
				v = _atomic::fetchOr(&_data.value, WaitersBit);
				if ((v & value_mask) < val) {
					// if &_data.value is still == v -- wait until signal
					qmutex_wait(&_data.value, v, 0);
				}
			}

			v = (_atomic::loadSeq(&_data.value));
		} while ((v & value_mask) < val);
	}
}

bool qtimeline::try_wait(value_type val) {
	return (_atomic::loadSeq(&_data.value) & value_mask) >= val;
}

void qtimeline::signal(value_type val) {
	if (val == 0) {
		return;
	}
	if (_atomic::fetchAdd(&_data.value, val) & WaitersBit) {
		qtimeline_wake(&_data.value);
	}
}

} // namespace sprt
