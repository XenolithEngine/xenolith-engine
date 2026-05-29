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

#include <sprt/wrappers/windows/thread_api.h>
#include <sprt/wrappers/windows/process_api.h>
#include <sprt/wrappers/windows/time_api.h>

namespace sprt {

static constexpr unsigned ThreadIdBit = 0x8000'0000;

__SPRT_C_FUNC int clock_getres(__SPRT_ID(clockid_t) clk_id,
		struct __SPRT_TIMESPEC_NAME *tp) __SPRT_NOEXCEPT {
	if (!tp) {
		__sprt_errno = EFAULT;
		return -1;
	}

	if (clk_id & ThreadIdBit) {
		tp->tv_sec = 0;
		tp->tv_nsec = 100;
		return 0; // actual resolution is around 15.6 ms
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
		return 0; // actual resolution is around 15.6 ms
	}

	default: break;
	}

	__sprt_errno = EINVAL;
	return -1;
}

// @AI-generated
__SPRT_C_FUNC int clock_gettime(__SPRT_ID(clockid_t) clk_id,
		struct __SPRT_TIMESPEC_NAME *tp) __SPRT_NOEXCEPT {
	if (!tp) {
		__sprt_errno = EFAULT;
		return -1;
	}

	// Note that this will not work for thread id greather then 0x7FFF'FFFF,
	// pthread_getcpuclockid will return ESRCH for them
	if (clk_id & ThreadIdBit) {
		FILETIME creation, exit, kernel, user;

		HANDLE hThread = OpenThread(THREAD_QUERY_INFORMATION, FALSE, clk_id & ~ThreadIdBit);

		GetThreadTimes(hThread, &creation, &exit, &kernel, &user);

		ULARGE_INTEGER ul;
		ul.LowPart = user.dwLowDateTime;
		ul.HighPart = user.dwHighDateTime;

		tp->tv_sec = ul.QuadPart / 10'000'000;
		tp->tv_nsec = (ul.QuadPart % 10'000'000) * 100;
		return 0;
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

} // namespace sprt
