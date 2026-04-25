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

#define __SPRT_BUILD 1

#include <sprt/c/__sprt_errno.h>
#include <sprt/c/__sprt_time.h>

#include <sprt/cxx/detail/ctypes.h>

#if SPRT_MACOS
#include <mach/mach.h>
#endif

#include <time.h>

namespace sprt {

#if SPRT_WINDOWS
static int _clock_getres(__SPRT_ID(clockid_t) clk_id, struct __SPRT_TIMESPEC_NAME *tp) {
	if (!tp) {
		__sprt_errno = EFAULT;
		return -1;
	}

	switch (clk_id) {
	case __SPRT_CLOCK_REALTIME: {
		tp->tv_sec = 0;
		tp->tv_nsec = 1'000;
		return 0;
	}

	case __SPRT_CLOCK_REALTIME_COARSE: {
		tp->tv_sec = 0;
		tp->tv_nsec = 16'000'000;
		return 0;
	}

	case __SPRT_CLOCK_MONOTONIC: {
		LARGE_INTEGER qpc_freq;
		QueryPerformanceFrequency(&qpc_freq);
		tp->tv_sec = 0;
		// if more then nanosec resolution
		if (qpc_freq.QuadPart > 1'000'000'000ULL) {
			tp->tv_nsec = 1;
		} else {
			tp->tv_nsec = 1'000'000'000 / qpc_freq.QuadPart;
		}
		return 0;
	}

	case __SPRT_CLOCK_MONOTONIC_COARSE: {
		tp->tv_sec = 0;
		tp->tv_nsec = 16'000'000;
		return 0;
	}

	case __SPRT_CLOCK_PROCESS_CPUTIME_ID: {
		tp->tv_sec = 0;
		tp->tv_nsec = 100;
		return 0;
	}

	case __SPRT_CLOCK_THREAD_CPUTIME_ID: {
		tp->tv_sec = 0;
		tp->tv_nsec = 100;
		return 0;
	}

	default: break;
	}

	__sprt_errno = EINVAL;
	return -1;
}

// @AI-generated
static int _clock_gettime(__SPRT_ID(clockid_t) clk_id, struct __SPRT_TIMESPEC_NAME *tp) {
	if (!tp) {
		__sprt_errno = EFAULT;
		return -1;
	}

	static LARGE_INTEGER qpc_freq = {{0, 0}};

	if (qpc_freq.QuadPart == 0) {
		QueryPerformanceFrequency(&qpc_freq); //
	}

	switch (clk_id) {
	case __SPRT_CLOCK_REALTIME: {
		FILETIME ft;
		GetSystemTimePreciseAsFileTime(&ft); // ~1μs precision

		ULARGE_INTEGER ul;
		ul.LowPart = ft.dwLowDateTime;
		ul.HighPart = ft.dwHighDateTime;
		ul.QuadPart -= 116'444'736'000'000'000ULL; // 1601→1970

		tp->tv_sec = ul.QuadPart / 10'000'000;
		tp->tv_nsec = (ul.QuadPart % 10'000'000) * 100;
		return 0;
	}


	case __SPRT_CLOCK_REALTIME_COARSE: {
		FILETIME ft;
		GetSystemTimeAsFileTime(&ft); // 100ns units

		ULARGE_INTEGER ul;
		ul.LowPart = ft.dwLowDateTime;
		ul.HighPart = ft.dwHighDateTime;

		// Convert 100ns → seconds + nanoseconds (1601 epoch → 1970)
		ul.QuadPart -= 116'444'736'000'000'000ULL;
		tp->tv_sec = ul.QuadPart / 10'000'000;
		tp->tv_nsec = (ul.QuadPart % 10'000'000) * 100;
		return 0;
	}

	case __SPRT_CLOCK_MONOTONIC: {
		LARGE_INTEGER pc;
		QueryPerformanceCounter(&pc);

		// Nanoseconds since some monotonic epoch
		auto ns = pc.QuadPart * 1'000'000'000ULL / qpc_freq.QuadPart;
		tp->tv_sec = ns / 1'000'000'000ULL;
		tp->tv_nsec = ns % 1'000'000'000ULL;
		return 0;
	}

	case __SPRT_CLOCK_MONOTONIC_COARSE: {
		auto tickCountMillis = GetTickCount64();
		tp->tv_sec = tickCountMillis / 1'000;
		tp->tv_nsec = (tickCountMillis % 1'000) * 1'000'000ULL;
		return 0;
	}

	case __SPRT_CLOCK_PROCESS_CPUTIME_ID: {
		FILETIME creation, exit, kernel, user;
		GetProcessTimes(GetCurrentProcess(), &creation, &exit, &kernel, &user);

		ULARGE_INTEGER ul;
		ul.LowPart = user.dwLowDateTime;
		ul.HighPart = user.dwHighDateTime;

		tp->tv_sec = ul.QuadPart / 10'000'000;
		tp->tv_nsec = (ul.QuadPart % 10'000'000) * 100;
		return 0;
	}

	case __SPRT_CLOCK_THREAD_CPUTIME_ID: {
		FILETIME creation, exit, kernel, user;
		GetThreadTimes(GetCurrentThread(), &creation, &exit, &kernel, &user);

		ULARGE_INTEGER ul;
		ul.LowPart = user.dwLowDateTime;
		ul.HighPart = user.dwHighDateTime;

		tp->tv_sec = ul.QuadPart / 10'000'000;
		tp->tv_nsec = (ul.QuadPart % 10'000'000) * 100;
		return 0;
	}

	default: break;
	}

	__sprt_errno = EINVAL;
	return -1;
}

static int _clock_settime(unsigned clk_id, const struct __SPRT_TIMESPEC_NAME *ts) {
	__sprt_errno = ENOSYS;
	return -1;
}
#endif


#if SPRT_LINUX || SPRT_ANDROID
static int _clock_getres(unsigned clk_id, struct __SPRT_TIMESPEC_NAME *out) {
	struct timespec rem;
	auto ret = ::clock_getres(clockid_t(clk_id), &rem);
	if (out) {
		out->tv_nsec = rem.tv_nsec;
		out->tv_sec = rem.tv_sec;
	}
	return ret;
}

static int _clock_gettime(unsigned clk_id, struct __SPRT_TIMESPEC_NAME *out) {
	struct timespec rem;
	auto ret = ::clock_gettime(clockid_t(clk_id), &rem);
	if (out) {
		out->tv_nsec = rem.tv_nsec;
		out->tv_sec = rem.tv_sec;
	}
	return ret;
}

static int _clock_settime(unsigned clk_id, const struct __SPRT_TIMESPEC_NAME *ts) {
	struct timespec native;
	if (ts) {
		native.tv_nsec = ts->tv_nsec;
		native.tv_sec = ts->tv_sec;
	}

	return ::clock_settime(clockid_t(clk_id), ts ? &native : nullptr);
}
#endif


#if SPRT_MACOS
static constexpr unsigned ThreadIdBit = 0x800'0000;

static int _clock_getres(unsigned clk_id, struct __SPRT_TIMESPEC_NAME *out) {
	if (clk_id & ThreadIdBit) {
		if (out) {
			out->tv_nsec = 1'000;
			out->tv_sec = 0;
		}
		return 0;
	} else {
		struct timespec rem;
		auto ret = ::clock_getres(clockid_t(clk_id), &rem);
		if (out) {
			out->tv_nsec = rem.tv_nsec;
			out->tv_sec = rem.tv_sec;
		}
		return ret;
	}
}

static int _clock_gettime(unsigned clk_id, struct __SPRT_TIMESPEC_NAME *out) {
	if (clk_id & ThreadIdBit) {
		thread_inspect_t mach_thread = static_cast<thread_inspect_t>(clk_id & ~ThreadIdBit);

		thread_basic_info_data_t info;
		mach_msg_type_number_t count = THREAD_BASIC_INFO_COUNT;

		kern_return_t kr =
				thread_info(mach_thread, THREAD_BASIC_INFO, (thread_info_t)&info, &count);

		if (kr != KERN_SUCCESS) {
			__sprt_errno = EINVAL;
			return -1; // Thread not found
		}

		// 3. Calculate Total CPU Time
		// info.user_time and info.system_time are in time_value_t (sec, usec)
		long total_sec = info.user_time.seconds + info.system_time.seconds;
		long total_usec = info.user_time.microseconds + info.system_time.microseconds;

		// Normalize microseconds
		if (total_usec >= 1'000'000) {
			total_sec += total_usec / 1'000'000;
			total_usec = total_usec % 1'000'000;
		}

		// 4. Fill timespec
		out->tv_sec = total_sec;
		out->tv_nsec = total_usec * 1'000;
		return 0;
	} else {
		struct timespec rem;
		auto ret = ::clock_gettime(clockid_t(clk_id), &rem);
		if (out) {
			out->tv_nsec = rem.tv_nsec;
			out->tv_sec = rem.tv_sec;
		}
		return ret;
	}
}

static int _clock_settime(unsigned clk_id, const struct __SPRT_TIMESPEC_NAME *ts) {
	if (clk_id & ThreadIdBit) {
		__sprt_errno = EINVAL;
		return -1;
	} else {
		struct timespec native;
		if (ts) {
			native.tv_nsec = ts->tv_nsec;
			native.tv_sec = ts->tv_sec;
		}

		return ::clock_settime(clockid_t(clk_id), ts ? &native : nullptr);
	}
}
#endif

__SPRT_C_FUNC int __SPRT_ID(clock_getres)(__SPRT_ID(clockid_t) clock, __SPRT_TIMESPEC_NAME *out) {
	return _clock_getres(clock, out);
}

__SPRT_C_FUNC int __SPRT_ID(clock_gettime)(__SPRT_ID(clockid_t) clock, __SPRT_TIMESPEC_NAME *out) {
	return _clock_gettime(clock, out);
}

__SPRT_C_FUNC int __SPRT_ID(
		clock_settime)(__SPRT_ID(clockid_t) clock, const __SPRT_TIMESPEC_NAME *ts) {
	return _clock_settime(clock, ts);
}

__SPRT_C_FUNC __SPRT_ID(uint64_t) __SPRT_ID(clock_gettime_nsec_np)(__SPRT_ID(clockid_t) __clock) {
	struct __SPRT_TIMESPEC_NAME __tp;
	_clock_gettime(__clock, &__tp);
	return static_cast<uint64_t>(__tp.tv_sec) * static_cast<uint64_t>(1'000'000'000LLU)
			+ static_cast<uint64_t>(__tp.tv_nsec);
}

} // namespace sprt
