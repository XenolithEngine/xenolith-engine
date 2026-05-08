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

#define __SPRT_BUILD

#include <sprt/c/sys/__sprt_sprt.h>
#include <sprt/c/sys/__sprt_futex.h>
#include <sprt/cxx/__utility/common.h>
#include <sprt/cxx/atomic>
#include <sprt/c/__sprt_time.h>
#include <sprt/c/__sprt_stdio.h>
#include <sprt/c/__sprt_errno.h>
#include <sprt/runtime/enum.h>
#include <sprt/runtime/thread/qonce.h>
#include <sprt/c/sys/__sprt_utsname.h>
#include <sprt/c/__sprt_pthread.h>

#if SPRT_LINUX || SPRT_ANDROID
#include <unistd.h>
#include <time.h>
#endif

#if SPRT_LINUX || SPRT_ANDROID
#include "linux/sprt_lock.cc"
#elif SPRT_MACOS
#include "darwin/sprt_lock.cc"
#elif SPRT_WINDOWS
#include "windows/sprt_lock.cc"
#else
#error Not implemented
#endif

namespace sprt {

__SPRT_C_FUNC int __SPRT_ID(sprt_qlock_supports)(__SPRT_ID(sprt_lock_flags_t) flags) {
	if (flags == 0) {
		return 0;
	}

	return sprt_qlock_supports(flags);
}

__SPRT_C_FUNC int __SPRT_ID(sprt_qlock_wait)(__SPRT_ID(sprt_qlock_t) * value,
		__SPRT_ID(sprt_qlock_t) expected, __SPRT_ID(sprt_timeout_t) timeout,
		__SPRT_ID(sprt_lock_flags_t) flags) {
	int result = sprt_qlock_wait(value, expected, timeout, flags);
#if DEBUG
	if (result != 0) {
		auto err = __sprt_errno;
		if (err != EAGAIN && err != ETIMEDOUT) {
			__sprt_perror("sprt_qlock_wake_one error: ");
		}
	}
#endif
	return result;
}

__SPRT_C_FUNC int __SPRT_ID(
		sprt_qlock_wake_one)(__SPRT_ID(sprt_qlock_t) * value, __SPRT_ID(sprt_lock_flags_t) flags) {
	int result = sprt_qlock_wake_one(value, flags);
#if DEBUG
	if (result != 0) {
		auto err = __sprt_errno;
		if (err != EAGAIN && err != ENOENT && err != ETIMEDOUT) {
			__sprt_perror("sprt_qlock_wake_one error: ");
		}
	}
#endif
	return result;
}

__SPRT_C_FUNC int __SPRT_ID(
		sprt_qlock_wake_all)(__SPRT_ID(sprt_qlock_t) * value, __SPRT_ID(sprt_lock_flags_t) flags) {
	int result = sprt_qlock_wake_all(value, flags);
#if DEBUG
	if (result != 0) {
		auto err = __sprt_errno;
		if (err != EAGAIN && err != ENOENT && err != ETIMEDOUT) {
			__sprt_perror("sprt_qlock_wake_one error: ");
		}
	}
#endif
	return result;
}

__SPRT_C_FUNC int __SPRT_ID(sprt_rlock_supports)(__SPRT_ID(sprt_lock_flags_t) flags) {
	if (flags == 0) {
		return 0;
	}

	return sprt_rlock_supports(flags);
}

__SPRT_C_FUNC int __SPRT_ID(sprt_rlock_wait)(__SPRT_ID(sprt_rlock_t) * value,
		__SPRT_ID(sprt_rlock_t) * expected, __SPRT_ID(sprt_timeout_t) timeout,
		__SPRT_ID(sprt_lock_flags_t) flags) {
	int result = sprt_rlock_wait(value, expected, timeout, flags);
#if DEBUG
	if (result != 0 && __sprt_errno != EAGAIN && __sprt_errno != ETIMEDOUT) {
		__sprt_perror("sprt_rlock_wait error: ");
	}
#endif
	return result;
}

__SPRT_C_FUNC int __SPRT_ID(
		sprt_rlock_try_wait)(__SPRT_ID(sprt_rlock_t) * value, __SPRT_ID(sprt_lock_flags_t) flags) {
	return sprt_rlock_try_wait(value, flags);
}

__SPRT_C_FUNC int __SPRT_ID(
		sprt_rlock_wake)(__SPRT_ID(sprt_rlock_t) * value, __SPRT_ID(sprt_lock_flags_t) flags) {
	int result = sprt_rlock_wake(value, flags);
#if DEBUG
	if (result != 0 && __sprt_errno != EAGAIN && __sprt_errno != ETIMEDOUT) {
		__sprt_perror("sprt_rlock_wake error: ");
	}
#endif
	return result;
}

__SPRT_C_FUNC __SPRT_ID(sprt_timeout_t)
		__SPRT_ID(sprt_qlock_now)(__SPRT_ID(sprt_lock_flags_t) flags) {
	return __sprt_clock_gettime_nsec_np(__sprt_sprt_qlock_getclock(flags));
}

__SPRT_C_FUNC __SPRT_ID(clockid_t)
		__SPRT_ID(sprt_qlock_getclock)(__SPRT_ID(sprt_lock_flags_t) flags) {
	return sprt_qlock_getclock(flags);
}

__SPRT_C_FUNC __SPRT_ID(sprt_timeout_t)
		__SPRT_ID(sprt_rlock_now)(__SPRT_ID(sprt_lock_flags_t) flags) {
	return __sprt_clock_gettime_nsec_np(__sprt_sprt_rlock_getclock(flags));
}

__SPRT_C_FUNC __SPRT_ID(clockid_t)
		__SPRT_ID(sprt_rlock_getclock)(__SPRT_ID(sprt_lock_flags_t) flags) {
	return sprt_rlock_getclock(flags);
}

static __sprt_utsname s_unameBuf;
static sprt::qonce s_nameBUfOnce;

__SPRT_C_FUNC const char *__SPRT_ID(sprt_get_os_kernel_version)() {
	s_nameBUfOnce([] { __sprt_uname(&s_unameBuf); });

	return s_unameBuf.release;
}

__SPRT_C_FUNC int __SPRT_ID(sprt_compare_os_kernel_version)(const char *cmp) {
	return __sprt_strverscmp(__sprt_sprt_get_os_kernel_version(), cmp);
}

__SPRT_C_FUNC int __SPRT_ID(sprt_qlock_oneshot_lock)(__SPRT_ID(sprt_qlock_t) * value,
		__SPRT_ID(sprt_lock_flags_t) flags) {
	auto ret = qmutex_base::_lock<__sprt_sprt_qlock_wait, nullptr>(value, nullptr, flags);
	if (ret != Status::Ok) {
		__sprt_errno = status::toErrno(ret);
		return -1;
	}
	return 0;
}

__SPRT_C_FUNC int __SPRT_ID(sprt_qlock_oneshot_trylock)(__SPRT_ID(sprt_qlock_t) * value) {
	auto ret = qmutex_base::_try_lock(value);
	if (ret != Status::Ok) {
		__sprt_errno = status::toErrno(ret);
		return -1;
	}
	return 0;
}

__SPRT_C_FUNC int __SPRT_ID(sprt_qlock_oneshot_unlock)(__SPRT_ID(sprt_qlock_t) * value,
		__SPRT_ID(sprt_lock_flags_t) flags) {
	auto ret = qmutex_base::_unlock<__sprt_sprt_qlock_wake_one>(value, flags);
	if (ret != Status::Ok && ret != Status::Done) {
		__sprt_errno = status::toErrno(ret);
		return -1;
	}
	return 0;
}

} // namespace sprt

/*
	Mutex ABI
*/

#include <sprt/runtime/thread/qmutex.h>
#include <sprt/runtime/thread/rmutex.h>
#include <sprt/wrappers/windows/thread_api.h>
#include <sprt/cxx/__mutex/recursive_timed_mutex.h>
#include <sprt/c/__sprt_unistd.h>
#include <sprt/runtime/log.h>

namespace sprt {

qmutex::~qmutex() {
	auto value = _atomic::exchange(&_data, uint32_t(0));
	if ((value & LOCK_BIT) != 0) {
		oslog::vprint(oslog::LogType::Fatal, __SPRT_LOCATION, "sprt::qmutex",
				"Mutex is locked when it's destroyed, aborting");
		::__sprt_abort();
	}
}

rmutex::~rmutex() {
	if (_data.value.u64 != Max<uint64_t>) {
		value_type zero = {0};
		auto value = _atomic::exchange(getNativeValue(_data), *getNativeValue(zero)) & VALUE_MASK;
		if (value != 0) {
			oslog::vprint(oslog::LogType::Fatal, __SPRT_LOCATION, "sprt::recursive_qmutex",
					"Mutex is locked when it's destroyed, aborting: tid:", value);
			::__sprt_abort();
		}
	}
}

void rmutex::lock() {
	if (_data.value.u64 == Max<uint64_t>) {
		return;
	}

	__sprt_sprt_rlock_t tid;
	*getNativeValue(tid) = __sprt_pthread_get_id_np();

	auto res = _lock<__sprt_sprt_rlock_wait, nullptr,
			bool(__SPRT_SPRT_RLOCK_PI_REQUIRES_EXTENDED_CALL)>(_data.value, tid, 0,
			__SPRT_SPRT_LOCK_FLAG_PI);
	switch (res) {
	case Status::Ok:
	case Status::Propagate: ++_data.counter; break;
	default: break;
	}
}

bool rmutex::try_lock() {
	if (_data.value.u64 == Max<uint64_t>) {
		return true;
	}

	__sprt_sprt_rlock_t tid;
	*getNativeValue(tid) = __sprt_pthread_get_id_np();

	auto res = _try_lock<__sprt_sprt_rlock_try_wait>(_data.value, tid, __SPRT_SPRT_LOCK_FLAG_PI);
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
	if (_data.value.u64 == Max<uint64_t>) {
		return true;
	}

	__sprt_sprt_rlock_t tid;
	*getNativeValue(tid) = __sprt_pthread_get_id_np();

	return _unlock<__sprt_sprt_rlock_wake>(_data.value, tid, &_data.counter,
				   __SPRT_SPRT_LOCK_FLAG_PI)
			== Status::Ok;
}

void rmutex::disable() {
	if ((*getNativeValue(_data.value) & THREAD_ID_MASK) == 0) {
		_data.value.u64 = Max<uint64_t>;
	}
}

void rmutex::enable() {
	if (_data.value.u64 == Max<uint64_t>) {
		_data.value.u64 = 0;
	}
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
	*rmutex_base::getNativeValue(tid) = __sprt_pthread_get_id_np();

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
	*rmutex_base::getNativeValue(tid) = __sprt_pthread_get_id_np();

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
	*rmutex_base::getNativeValue(tid) = __sprt_pthread_get_id_np();

	rmutex_base::_unlock<__sprt_sprt_rlock_wake>(_mutex.value, tid, &_mutex.counter, 0);
}

} // namespace sprt
