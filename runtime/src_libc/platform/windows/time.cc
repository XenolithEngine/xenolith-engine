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

#ifndef __SPRT_BUILD
#define __SPRT_BUILD
#endif

#include <sprt/c/__sprt_time.h>
#include <sprt/c/__sprt_errno.h>

#include <Windows.h>

#include "private/SPRTSpecific.h"

#ifdef __clang__
#pragma clang diagnostic ignored "-Wunused-function"
#endif

namespace sprt {

static int clock_getres(int clk_id, struct __SPRT_TIMESPEC_NAME *tp) {
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
static int clock_gettime(int clk_id, struct __SPRT_TIMESPEC_NAME *tp) {
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

static int __getLocalGmtOff(bool isDst) {
	TIME_ZONE_INFORMATION tzi;
	GetTimeZoneInformation(&tzi);
	long bias = tzi.Bias;
	if (isDst) {
		if (tzi.DaylightDate.wMonth) {
			bias += tzi.DaylightBias;
		} else if (tzi.StandardDate.wMonth) {
			bias += tzi.StandardBias;
		}
	} else {
		if (tzi.StandardDate.wMonth) {
			bias += tzi.StandardBias;
		}
	}

	return -bias * 60;
}

static ULONGLONG __nano_now() {
	static LARGE_INTEGER qpc_freq = {{0, 0}};

	if (qpc_freq.QuadPart == 0) {
		QueryPerformanceFrequency(&qpc_freq); //
	}

	LARGE_INTEGER pc;
	QueryPerformanceCounter(&pc);

	// Nanoseconds since some monotonic epoch
	return pc.QuadPart * 1'000'000'000ULL / qpc_freq.QuadPart;
}

static int nanosleep(const struct __SPRT_TIMESPEC_NAME *req, struct __SPRT_TIMESPEC_NAME *rem) {
	static LARGE_INTEGER qpc_freq = {{0, 0}};

	if (qpc_freq.QuadPart == 0) {
		QueryPerformanceFrequency(&qpc_freq); //
	}

	constexpr const ULONGLONG SPIN_NS = 100'000ULL; // 100mks spin limit

	if (!req) {
		errno = EFAULT;
		return -1;
	}

	if (req->tv_sec < 0 || req->tv_nsec < 0 || req->tv_nsec >= 1'000'000'000L) {
		errno = EINVAL;
		return -1;
	}

	auto nanoStart = __nano_now();
	auto nanoToSleep = req->tv_nsec + req->tv_sec * 1'000'000'000ULL;

	ULONGLONG nanoStop, elapsed;

	if (nanoToSleep > SPIN_NS) {
		HANDLE timer = CreateWaitableTimerW(NULL, TRUE, NULL);
		if (!timer || timer == INVALID_HANDLE_VALUE) {
			errno = ENOSYS;
			return -1;
		}

		LARGE_INTEGER due;
		due.QuadPart = -((req->tv_sec * 10'000'000LL) + (req->tv_nsec / 100LL));

		timeBeginPeriod(1);
		BOOL ok = SetWaitableTimer(timer, &due, 0, NULL, NULL, FALSE);
		if (ok) {
			WaitForSingleObject(timer, INFINITE);
		}
		CloseHandle(timer);
		timeEndPeriod(1);

		if (!ok) {
			__sprt_errno = platform::lastErrorToErrno(GetLastError());
			return -1;
		}

		nanoStop = __nano_now();
		elapsed = nanoStop - nanoStart;

		if (nanoToSleep > elapsed && nanoToSleep - elapsed <= SPIN_NS) {
			nanoToSleep -= elapsed;

			// Spin-wait
			LARGE_INTEGER now;
			ULONGLONG target_ticks = nanoStop + (nanoToSleep * qpc_freq.QuadPart / 1'000'000'000LL);
			while (1) {
				YieldProcessor();
				now.QuadPart = 0;
				QueryPerformanceCounter(&now);
				if (now.QuadPart >= target_ticks) {
					break;
				}
				target_ticks -= now.QuadPart;
			}
		}
	} else {
		// Spin-wait
		LARGE_INTEGER now;
		ULONGLONG target_ticks = nanoStart + (nanoToSleep * qpc_freq.QuadPart / 1'000'000'000LL);
		while (1) {
			YieldProcessor();
			now.QuadPart = 0;
			QueryPerformanceCounter(&now);
			if (now.QuadPart >= target_ticks) {
				break;
			}
			target_ticks -= now.QuadPart;
		}
	}

	nanoStop = __nano_now();
	elapsed = nanoStop - nanoStart;

	if (rem) {
		if (nanoToSleep > elapsed) {
			auto remains = nanoToSleep - elapsed;
			rem->tv_sec = remains / 1'000'000'000ULL;
			rem->tv_nsec = remains % 1'000'000'000ULL;
		} else {
			rem->tv_sec = 0;
			rem->tv_nsec = 0;
		}
	}
	return 0;
}

static ULONGLONG __nano_now(__SPRT_ID(clockid_t) clock) {
	__SPRT_TIMESPEC_NAME ts;
	clock_gettime(clock, &ts);
	return ts.tv_nsec + ts.tv_sec * 1'000'000'000ULL;
}

static int clock_nanosleep(__SPRT_ID(clockid_t) clock, int v, const __SPRT_TIMESPEC_NAME *ireq,
		__SPRT_TIMESPEC_NAME *rem) {
	constexpr const ULONGLONG SPIN_NS = 100'000ULL; // 100mks spin limit

	if (!ireq || !rem) {
		errno = EFAULT;
		return -1;
	}

	if (ireq->tv_sec < 0 || ireq->tv_nsec < 0 || ireq->tv_nsec >= 1'000'000'000L) {
		errno = EINVAL;
		return -1;
	}

	const __SPRT_TIMESPEC_NAME *req = ireq;
	__SPRT_TIMESPEC_NAME diff;

	if (v & __SPRT_TIMER_ABSTIME) {
		__SPRT_TIMESPEC_NAME curTv;
		clock_gettime(clock, &curTv);

		auto diffTv = __sprt_timespec_diff(req, &curTv);

		if (diffTv.tv_sec < 0) {
			return 0;
		}

		diff.tv_nsec = diffTv.tv_nsec;
		diff.tv_sec = diffTv.tv_sec;

		req = &diff;
	}

	auto nanoStart = __nano_now(clock);
	auto nanoToSleep = req->tv_nsec + req->tv_sec * 1'000'000'000ULL;

	if (nanoToSleep <= SPIN_NS) {
		// Spin-wait
		while (1) {
			YieldProcessor();
			auto v = __nano_now(clock);
			if (v >= nanoToSleep) {
				break;
			}
			nanoToSleep -= v;
		}
	} else {
		HANDLE timer = CreateWaitableTimerW(NULL, TRUE, NULL);
		if (!timer || timer == INVALID_HANDLE_VALUE) {
			errno = ENOSYS;
			return -1;
		}

		LARGE_INTEGER due;
		due.QuadPart = -((req->tv_sec * 10'000'000LL) + (req->tv_nsec / 100LL));

		timeBeginPeriod(1);
		BOOL ok = SetWaitableTimer(timer, &due, 0, NULL, NULL, FALSE);
		if (ok) {
			WaitForSingleObject(timer, INFINITE);
		}
		CloseHandle(timer);
		timeEndPeriod(1);

		if (!ok) {
			__sprt_errno = platform::lastErrorToErrno(GetLastError());
			return -1;
		}
	}

	auto elapsed = __nano_now(clock) - nanoStart;
	if (rem) {
		if (nanoToSleep > elapsed) {
			auto remains = nanoToSleep - elapsed;
			rem->tv_sec = remains / 1'000'000'000ULL;
			rem->tv_nsec = remains % 1'000'000'000ULL;
		} else {
			rem->tv_sec = 0;
			rem->tv_nsec = 0;
		}
	}
	return 0;
}

} // namespace sprt
