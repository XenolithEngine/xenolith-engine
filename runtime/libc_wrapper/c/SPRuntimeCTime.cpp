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

#define __SPRT_BUILD 1

#include <sprt/c/__sprt_errno.h>
#include <sprt/c/__sprt_time.h>
#include <sprt/c/__sprt_unistd.h>

#include <sprt/runtime/log.h>

#if SPRT_ANDROID && !defined(__LP64__)
#include <time64.h>
#endif

#if SPRT_MACOS
#include <xlocale.h>
#include <unistd.h>
#endif

#if __STDC_HOSTED__ == 0
#include "locale.h"
#include "time.h"
#else
#if SPRT_MACOS
#define CLOCK_MONOTONIC_COARSE CLOCK_MONOTONIC_RAW_APPROX
#define CLOCK_BOOTTIME CLOCK_UPTIME_RAW
#endif

static_assert(CLOCK_REALTIME == __SPRT_CLOCK_REALTIME);
static_assert(CLOCK_MONOTONIC == __SPRT_CLOCK_MONOTONIC);
static_assert(CLOCK_PROCESS_CPUTIME_ID == __SPRT_CLOCK_PROCESS_CPUTIME_ID);
static_assert(CLOCK_THREAD_CPUTIME_ID == __SPRT_CLOCK_THREAD_CPUTIME_ID);
static_assert(CLOCK_MONOTONIC_RAW == __SPRT_CLOCK_MONOTONIC_RAW);
static_assert(CLOCK_MONOTONIC_COARSE == __SPRT_CLOCK_MONOTONIC_COARSE);
static_assert(CLOCK_BOOTTIME == __SPRT_CLOCK_BOOTTIME);

#ifdef CLOCK_REALTIME_COARSE
static_assert(CLOCK_REALTIME_COARSE == __SPRT_CLOCK_REALTIME_COARSE);
#endif

#ifdef CLOCK_REALTIME_ALARM
static_assert(CLOCK_REALTIME_ALARM == __SPRT_CLOCK_REALTIME_ALARM);
#endif

#ifdef CLOCK_SGI_CYCLE
static_assert(CLOCK_SGI_CYCLE == __SPRT_CLOCK_SGI_CYCLE);
#endif

#ifdef CLOCK_TAI
static_assert(CLOCK_TAI == __SPRT_CLOCK_TAI);
#endif
#endif

#include "time/timespec_get.cc"

namespace sprt {

thread_local struct __SPRT_TM_NAME s_gmtime_val;
thread_local struct __SPRT_TM_NAME s_localtime_val;

__SPRT_C_FUNC __SPRT_ID(time_t) __SPRT_ID(time)(__SPRT_ID(time_t) * t) {
#if SPRT_ANDROID && !defined(__LP64__)
	::time_t native;
	auto ret = ::time(&native);
	if (t) {
		*t = native;
	}
	return ret;
#else
	return ::time(t);
#endif
}

__SPRT_C_FUNC double __SPRT_ID(difftime)(__SPRT_ID(time_t) a, __SPRT_ID(time_t) b) {
	return ::difftime(a, b);
}

__SPRT_C_FUNC __SPRT_ID(time_t) __SPRT_ID(mktime)(struct __SPRT_TM_NAME *_tm) {
#if __STDC_HOSTED__ == 0
	return ::mktime(_tm);
#else
	auto native = internal::getNativeTm(_tm);
#if SPRT_ANDROID && !defined(__LP64__)
	auto ret = ::mktime64(&native);
#elif SPRT_WINDOWS
	auto ret = ::_mktime64(&native);
#else
	auto ret = ::mktime(&native);
#endif
	if (_tm) {
		internal::getRuntimeTm(_tm, native);
	}
	return ret;
#endif
}

__SPRT_C_FUNC __SPRT_ID(time_t)
		__SPRT_ID(strftime)(char *__SPRT_RESTRICT buf, __SPRT_ID(size_t) size,
				const char *__SPRT_RESTRICT fmt, const struct __SPRT_TM_NAME *__SPRT_RESTRICT _tm) {
#if __STDC_HOSTED__ == 0
	return strftime(buf, size, fmt, _tm);
#else
	auto native = internal::getNativeTm(_tm);
	return strftime(buf, size, fmt, &native);
#endif
}

__SPRT_C_FUNC struct __SPRT_TM_NAME *__SPRT_ID(gmtime)(const __SPRT_ID(time_t) * t) {
	__SPRT_ID(gmtime_r)(t, &s_gmtime_val);
	return &s_gmtime_val;
}

__SPRT_C_FUNC struct __SPRT_TM_NAME *__SPRT_ID(localtime)(const __SPRT_ID(time_t) * t) {
	__SPRT_ID(localtime_r)(t, &s_gmtime_val);
	return &s_localtime_val;
}

__SPRT_C_FUNC char *__SPRT_ID(asctime)(const struct __SPRT_TM_NAME *_tm) {
#if __STDC_HOSTED__ == 0
	return ::asctime(_tm);
#else
	auto native = internal::getNativeTm(_tm);
#if SPRT_ANDROID && !defined(__LP64__)
	return ::asctime64(&native);
#else
	return ::asctime(&native);
#endif
#endif
}

__SPRT_C_FUNC char *__SPRT_ID(ctime)(const __SPRT_ID(time_t) * t) {
#if __STDC_HOSTED__ == 0
	return ::ctime(t);
#else
#if SPRT_ANDROID && !defined(__LP64__)
	::time64_t native = *t;
	return ::ctime64(&native);
#elif SPRT_WINDOWS
	return ::_ctime64(t);
#else
	::time_t native = *t;
	return ::ctime(&native);
#endif
#endif
}

__SPRT_C_FUNC struct __SPRT_TM_NAME *__SPRT_ID(
		gmtime_r)(const __SPRT_ID(time_t) * t, struct __SPRT_TM_NAME *_tm) {
	if (!t || !_tm) {
		return nullptr;
	}

#if __STDC_HOSTED__ == 0
	return ::gmtime_r(t, _tm);
#else
	auto native = internal::getNativeTm(_tm);
#if SPRT_ANDROID && !defined(__LP64__)
	::time64_t nativeT = *t;
	auto ret = ::gmtime64_r(&nativeT, &native);
	if (ret) {
		internal::getRuntimeTm(_tm, *ret);
		_tm->tm_gmtoff = 0;
		_tm->tm_gmt_type = __SPRT_ID(gmt_set);
		return _tm;
	}
	return nullptr;
#else
	::time_t nativeT = *t;
	auto ret = ::gmtime_r(&nativeT, &native);
	if (ret) {
		internal::getRuntimeTm(_tm, *ret);
		_tm->tm_gmtoff = 0;
		_tm->tm_gmt_type = __SPRT_ID(gmt_set);
		return _tm;
	}
	return nullptr;
#endif
#endif
}

__SPRT_C_FUNC struct __SPRT_TM_NAME *__SPRT_ID(
		localtime_r)(const __SPRT_ID(time_t) * t, struct __SPRT_TM_NAME *_tm) {
	if (!t || !_tm) {
		return nullptr;
	}

#if __STDC_HOSTED__ == 0
	return ::localtime_r(t, _tm);
#else
	auto native = internal::getNativeTm(_tm);
#if SPRT_ANDROID && !defined(__LP64__)
	::time64_t nativeT = *t;
	auto ret = ::localtime64_r(&nativeT, &native);
	if (ret) {
		internal::getRuntimeTm(_tm, native);
		_tm->tm_gmt_type = __SPRT_ID(gmt_local);
		return _tm;
	}
	return nullptr;
#else
	::time_t nativeT = *t;
	auto ret = ::localtime_r(&nativeT, &native);
	if (ret) {
		internal::getRuntimeTm(_tm, native);
		_tm->tm_gmt_type = __SPRT_ID(gmt_local);
		return _tm;
	}
	return nullptr;
#endif
#endif
}

__SPRT_C_FUNC __SPRT_ID(size_t) __SPRT_ID(strftime_l)(char *__SPRT_RESTRICT buf,
		__SPRT_ID(size_t) size, const char *__SPRT_RESTRICT fmt,
		const struct __SPRT_TM_NAME *__SPRT_RESTRICT ts, __SPRT_ID(locale_t) loc) {
#if __STDC_HOSTED__ == 0
	return ::strftime_l(buf, size, fmt, ts, loc);
#else
	auto native = internal::getNativeTm(ts);
	return ::strftime_l(buf, size, fmt, &native, loc);
#endif
}

__SPRT_C_FUNC char *__SPRT_ID(
		asctime_r)(const struct __SPRT_TM_NAME *__SPRT_RESTRICT ts, char *__SPRT_RESTRICT buf) {
#if __STDC_HOSTED__ == 0
	return ::asctime_r(ts, buf);
#else
	auto native = internal::getNativeTm(ts);
#if SPRT_ANDROID && !defined(__LP64__)
	return ::asctime64_r(&native, buf);
#else
	return ::asctime_r(&native, buf);
#endif
#endif
}

__SPRT_C_FUNC char *__SPRT_ID(ctime_r)(const __SPRT_ID(time_t) * t, char *buf) {
#if __STDC_HOSTED__ == 0
	return ::ctime_r(t, buf);
#else
#if SPRT_ANDROID && !defined(__LP64__)
	return ::ctime64_r(t, buf);
#else
	return ::ctime_r(t, buf);
#endif
#endif
}

__SPRT_C_FUNC void __SPRT_ID(tzset)(void) { ::tzset(); }

__SPRT_C_FUNC int __SPRT_ID(nanosleep)(const __SPRT_TIMESPEC_NAME *ts, __SPRT_TIMESPEC_NAME *out) {
#if SPRT_WINDOWS
	return nanosleep(ts, out);
#else
	struct timespec native;
	if (ts) {
		native.tv_nsec = ts->tv_nsec;
		native.tv_sec = ts->tv_sec;
	}

	struct timespec rem;
	auto ret = ::nanosleep(ts ? &native : nullptr, &rem);
	if (out) {
		out->tv_nsec = rem.tv_nsec;
		out->tv_sec = rem.tv_sec;
	}
	return ret;
#endif
}

__SPRT_C_FUNC int __SPRT_ID(clock_nanosleep)(__SPRT_ID(clockid_t) clock, int v,
		const __SPRT_TIMESPEC_NAME *ts, __SPRT_TIMESPEC_NAME *out) {
#if SPRT_WINDOWS
	return clock_nanosleep(clock, v, ts, out);
#elif SPRT_MACOS
	if (clock == CLOCK_REALTIME && v == 0) {
		if (__sprt_nanosleep(ts, out) != 0) {
			return *__sprt___errno_location();
		}
	}
	return EINVAL;
#else
	struct timespec native;
	if (ts) {
		native.tv_nsec = ts->tv_nsec;
		native.tv_sec = ts->tv_sec;
	}

	struct timespec rem;
	auto ret = ::clock_nanosleep(clock, v, ts ? &native : nullptr, &rem);
	if (out) {
		out->tv_nsec = rem.tv_nsec;
		out->tv_sec = rem.tv_sec;
	}
	return ret;
#endif
}

__SPRT_C_FUNC int __SPRT_ID(
		clock_getcpuclockid)(__SPRT_ID(pid_t) pid, __SPRT_ID(clockid_t) * clock) {
#if SPRT_WINDOWS || SPRT_MACOS
	if (pid != __sprt_getpid()) {
		return ENOSYS;
	}
	return __SPRT_CLOCK_PROCESS_CPUTIME_ID;
#else
	clockid_t id = 0;
	auto ret = ::clock_getcpuclockid(pid, &id);
	if (ret == 0) {
		*clock = static_cast<__sprt_clockid_t>(id);
		return 0;
	}
	return ret;
#endif
}

} // namespace sprt
