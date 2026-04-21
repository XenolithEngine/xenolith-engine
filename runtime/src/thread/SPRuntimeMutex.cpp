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

#include <sprt/runtime/thread/qmutex.h>
#include <sprt/runtime/thread/rmutex.h>
#include <sprt/runtime/log.h>
#include <sprt/runtime/status.h>
#include <sprt/runtime/platform.h>

#include <sprt/c/__sprt_time.h>
#include <sprt/c/__sprt_stdlib.h>
#include <sprt/c/__sprt_errno.h>
#include <sprt/c/sys/__sprt_futex.h>
#include <sprt/cxx/__mutex/recursive_timed_mutex.h>

#include <sys/winapi.h>
#include <sys/darwin.h>
#include <unistd.h>

#include "private/SPRTSpecific.h"

namespace sprt {

#if SPRT_LINUX || SPRT_ANDROID

static inline auto sys_gettid() { return uint32_t(::syscall(SYS_GETTID)); }

#elif SPRT_WINDOWS

static inline auto sys_gettid() { return ::_GetCurrentThreadId(); }

#elif SPRT_MACOS

static inline auto sys_gettid() { return ::gettid(); }

#else
#error TODO
#endif

static thread_local uint32_t tl_tid = sys_gettid();

qmutex::~qmutex() {
	auto value = _atomic::exchange(&_data, uint32_t(0));
	if ((value & LOCK_BIT) != 0) {
		oslog::vprint(oslog::LogType::Fatal, __SPRT_LOCATION, "sprt::qmutex",
				"Mutex is locked when it's destroyed, aborting");
		::__sprt_abort();
	}
}

rmutex::~rmutex() {
	value_type zero = {0};
	auto value = _atomic::exchange(getNativeValue(_data), *getNativeValue(zero)) & VALUE_MASK;
	if (value != 0) {
		oslog::vprint(oslog::LogType::Fatal, __SPRT_LOCATION, "sprt::recursive_qmutex",
				"Mutex is locked when it's destroyed, aborting: tid:", value);
		::__sprt_abort();
	}
}

void rmutex::lock() {
	// we can not use thread locals until full initialization is complete
	// becouse some static inits may use mutexes, but thread locals can be initialized after statics
	__sprt_sprt_rlock_t tid;
	if (isInitialized()) {
		*getNativeValue(tid) = tl_tid;
	} else {
		*getNativeValue(tid) = sys_gettid();
	}

	auto res = _lock<__sprt_sprt_rlock_wait, nullptr,
			bool(__SPRT_SPRT_RLOCK_PI_REQUIRES_EXTENDED_CALL)>(_data.value, tid, 0, 0);
	switch (res) {
	case Status::Ok:
	case Status::Propagate: ++_data.counter; break;
	default: break;
	}
}

bool rmutex::try_lock() {
	// we can not use thread locals until full initialization is complete
	// becouse some static inits may use mutexes, but thread locals can be initialized after statics
	__sprt_sprt_rlock_t tid;
	if (isInitialized()) {
		*getNativeValue(tid) = tl_tid;
	} else {
		*getNativeValue(tid) = sys_gettid();
	}

	auto res = _try_lock<__sprt_sprt_rlock_try_wait>(_data.value, tid, 0);
	switch (res) {
	case Status::Ok:
	case Status::Propagate:
		++_data.counter;
		return true;
		break;
	default: break;
	}
	return false;
}

bool rmutex::unlock() {
	// we can not use thread locals until full initialization is complete
	// becouse some static inits may use mutexes, but thread locals can be initialized after statics
	__sprt_sprt_rlock_t tid;
	if (isInitialized()) {
		*getNativeValue(tid) = tl_tid;
	} else {
		*getNativeValue(tid) = sys_gettid();
	}

	return _unlock<__sprt_sprt_rlock_wake>(_data.value, tid, &_data.counter, 0) == Status::Ok;
}

recursive_timed_mutex::~recursive_timed_mutex() {
	rmutex_base::value_type zero = {0};
	auto value = _atomic::exchange(rmutex_base::getNativeValue(_mutex),
						 *rmutex_base::getNativeValue(zero))
			& rmutex_base::VALUE_MASK;
	if (value != 0) {
		oslog::vprint(oslog::LogType::Fatal, __SPRT_LOCATION, "sprt::recursive_qmutex",
				"Mutex is locked when it's destroyed, aborting: tid:", value);
		::__sprt_abort();
	}
}

void recursive_timed_mutex::lock() {
	// we can not use thread locals until full initialization is complete
	// becouse some static inits may use mutexes, but thread locals can be initialized after statics
	__sprt_sprt_rlock_t tid;
	if (isInitialized()) {
		*rmutex_base::getNativeValue(tid) = tl_tid;
	} else {
		*rmutex_base::getNativeValue(tid) = sys_gettid();
	}

	auto res = rmutex_base::_lock<__sprt_sprt_rlock_wait, nullptr,
			bool(__SPRT_SPRT_RLOCK_PI_REQUIRES_EXTENDED_CALL)>(_mutex.value, tid, 0, 0);
	switch (res) {
	case Status::Ok:
	case Status::Propagate: ++_mutex.counter; break;
	default: break;
	}
}

bool recursive_timed_mutex::try_lock() noexcept {
	// we can not use thread locals until full initialization is complete
	// becouse some static inits may use mutexes, but thread locals can be initialized after statics
	__sprt_sprt_rlock_t tid;
	if (isInitialized()) {
		*rmutex_base::getNativeValue(tid) = tl_tid;
	} else {
		*rmutex_base::getNativeValue(tid) = sys_gettid();
	}

	auto res = rmutex_base::_try_lock<__sprt_sprt_rlock_try_wait>(_mutex.value, tid, 0);
	switch (res) {
	case Status::Ok:
	case Status::Propagate:
		++_mutex.counter;
		return true;
		break;
	default: break;
	}
	return false;
}

void recursive_timed_mutex::unlock() {
	// we can not use thread locals until full initialization is complete
	// becouse some static inits may use mutexes, but thread locals can be initialized after statics
	__sprt_sprt_rlock_t tid;
	if (isInitialized()) {
		*rmutex_base::getNativeValue(tid) = tl_tid;
	} else {
		*rmutex_base::getNativeValue(tid) = sys_gettid();
	}

	rmutex_base::_unlock<__sprt_sprt_rlock_wake>(_mutex.value, tid, &_mutex.counter, 0);
}

} // namespace sprt
