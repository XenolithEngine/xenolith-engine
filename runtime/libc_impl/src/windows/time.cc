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

#include <time.h>
#include <errno.h>

#include "specific.h"

#include <sprt/wrappers/windows/time_api.h>
#include <sprt/wrappers/windows/thread_api.h>
#include <sprt/wrappers/windows/windows.h>
#include <sprt/wrappers/windows/process_api.h>

#include "../../include/__impl_libc.h"

static constexpr time_t TICKS_PER_SECOND = 10'000'000ULL;
static constexpr time_t EPOCH_DIFFERENCE = 11'644'473'600ULL;

namespace sprt {

/*static int __getLocalGmtOff(bool isDst) {
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
}*/

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

__SPRT_C_FUNC int nanosleep(const struct __SPRT_TIMESPEC_NAME *req,
		struct __SPRT_TIMESPEC_NAME *rem) __SPRT_NOEXCEPT {
	static LARGE_INTEGER qpc_freq = {{0, 0}};

	if (qpc_freq.QuadPart == 0) {
		QueryPerformanceFrequency(&qpc_freq); //
	}

	constexpr const ULONGLONG SPIN_NS = 100'000ULL; // 100mks spin limit

	if (!req) {
		__sprt_errno = EFAULT;
		return -1;
	}

	if (req->tv_sec < 0 || req->tv_nsec < 0 || req->tv_nsec >= 1'000'000'000L) {
		__sprt_errno = EINVAL;
		return -1;
	}

	auto nanoStart = __nano_now();
	auto nanoToSleep = req->tv_nsec + req->tv_sec * 1'000'000'000ULL;

	ULONGLONG nanoStop, elapsed;

	if (nanoToSleep > SPIN_NS) {
		HANDLE timer = CreateWaitableTimerW(NULL, TRUE, NULL);
		if (!timer || timer == INVALID_HANDLE_VALUE) {
			__sprt_errno = ENOSYS;
			return -1;
		}

		LARGE_INTEGER due;
		due.QuadPart = -((req->tv_sec * TICKS_PER_SECOND) + (req->tv_nsec / 100LL));

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

__SPRT_C_FUNC int clock_nanosleep(__SPRT_ID(clockid_t) clock, int v,
		const __SPRT_TIMESPEC_NAME *ireq, __SPRT_TIMESPEC_NAME *rem) __SPRT_NOEXCEPT {
	constexpr const ULONGLONG SPIN_NS = 100'000ULL; // 100mks spin limit

	if (!ireq || !rem) {
		__sprt_errno = EFAULT;
		return -1;
	}

	if (ireq->tv_sec < 0 || ireq->tv_nsec < 0 || ireq->tv_nsec >= 1'000'000'000L) {
		__sprt_errno = EINVAL;
		return -1;
	}

	const __SPRT_TIMESPEC_NAME *req = ireq;
	__SPRT_TIMESPEC_NAME diff;

	if (v & __SPRT_TIMER_ABSTIME) {
		__SPRT_TIMESPEC_NAME curTv;
		__sprt_clock_gettime(clock, &curTv);

		auto diffTv = __sprt_timespec_diff(req, &curTv);

		if (diffTv.tv_sec < 0) {
			return 0;
		}

		diff.tv_nsec = diffTv.tv_nsec;
		diff.tv_sec = diffTv.tv_sec;

		req = &diff;
	}

	auto nanoStart = __sprt_clock_gettime_nsec_np(clock);
	auto nanoToSleep = req->tv_nsec + req->tv_sec * 1'000'000'000ULL;

	if (nanoToSleep <= SPIN_NS) {
		// Spin-wait
		while (1) {
			YieldProcessor();
			auto v = __sprt_clock_gettime_nsec_np(clock);
			if (v >= nanoToSleep) {
				break;
			}
			nanoToSleep -= v;
		}
	} else {
		HANDLE timer = CreateWaitableTimerW(NULL, TRUE, NULL);
		if (!timer || timer == INVALID_HANDLE_VALUE) {
			__sprt_errno = ENOSYS;
			return -1;
		}

		LARGE_INTEGER due;
		due.QuadPart = -((req->tv_sec * TICKS_PER_SECOND) + (req->tv_nsec / 100LL));

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

	auto elapsed = __sprt_clock_gettime_nsec_np(clock) - nanoStart;
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

static time_t FileTimeToTime(FILETIME *input) {
	time_t temp;
	temp = (static_cast<time_t>(input->dwHighDateTime) << 32 | input->dwLowDateTime)
			/ TICKS_PER_SECOND;
	temp = temp - EPOCH_DIFFERENCE;
	return temp;
}

__SPRT_C_FUNC struct tm *localtime_r(const time_t *__restrict t,
		struct tm *__restrict tm) __SPRT_NOEXCEPT {
	// get local SYSTEMTIME
	SYSTEMTIME st, localTime;

	ULARGE_INTEGER uli;
	uli.QuadPart = ((ULONGLONG)*t * TICKS_PER_SECOND) + EPOCH_DIFFERENCE * TICKS_PER_SECOND;

	FILETIME ft, localFt;
	ft.dwLowDateTime = uli.LowPart;
	ft.dwHighDateTime = uli.HighPart;

	if (!FileTimeToSystemTime(&ft, &st) || !SystemTimeToTzSpecificLocalTime(NULL, &st, &localTime)
			|| !SystemTimeToFileTime(&localTime, &localFt)) {
		errno = sprt::platform::lastErrorToErrno(GetLastError());
		return nullptr;
	}

	auto local = FileTimeToTime(&localFt);
	if (gmtime_r(&local, tm)) {
		// Get TZ info for specified year
		TIME_ZONE_INFORMATION tzi;
		if (!GetTimeZoneInformationForYear(localTime.wYear, nullptr, &tzi)) {
			errno = sprt::platform::lastErrorToErrno(GetLastError());
			return nullptr;
		}

		auto dstOffset = -(tzi.Bias + tzi.DaylightBias) * 60;

		auto off = local - *t;
		tm->tm_usec = 0;
		tm->tm_gmtoff = off;
		tm->tm_isdst = (dstOffset == off) ? 1 : 0;
		tm->tm_gmt_type = __sprt_gmt_local;

		if (off == 0) {
			tm->tm_zone = __utc;
		} else {
			size_t len = 0;
			char tzBuf[64];
			if (sprt::unicode::toUtf8(tzBuf, 64,
						(const char16_t *)(tm->tm_isdst ? tzi.DaylightName : tzi.StandardName),
						&len)
					== sprt::Status::Ok) {
				tm->tm_zone = sprt::__libc::get()->preserve_tz_name(sprt::StringView(tzBuf, len));
			}
		}

		return tm;
	}

	return nullptr;
}
