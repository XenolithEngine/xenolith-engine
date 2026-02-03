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

#include <sprt/c/bits/__sprt_def.h>
#include <sprt/c/__sprt_errno.h>
#include <sprt/c/__sprt_time.h>

#include <sprt/runtime/log.h>

#if SPRT_ANDROID && !defined(__LP64__)
#include <time64.h>
#endif

#include <time.h>

#if SPRT_WINDOWS
#include "platform/windows/time.cc"
#endif

#include "private/SPRTSpecific.h"
#include "private/SPRTTime.h"

#if SPRT_WINDOWS
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif

namespace sprt {

thread_local struct __SPRT_TM_NAME s_gmtime_val;
thread_local struct __SPRT_TM_NAME s_localtime_val;

__SPRT_C_FUNC __SPRT_ID(clock_t) __SPRT_ID(clock)(void) { return ::clock(); }

__SPRT_C_FUNC __SPRT_ID(time_t) __SPRT_ID(time)(__SPRT_ID(time_t) * t) {
#if SPRT_ANDROID && !defined(__LP64__)
	::time_t native;
	auto ret = ::time(&native);
	if (t) {
		*t = native;
	}
	return ret;
#elif SPRT_WINDOWS
	return ::_time64(t);
#else
	return ::time(t);
#endif
}

__SPRT_C_FUNC double __SPRT_ID(difftime)(__SPRT_ID(time_t) a, __SPRT_ID(time_t) b) {
#if SPRT_WINDOWS
	return ::_difftime64(a, b);
#else
	return ::difftime(a, b);
#endif
}

__SPRT_C_FUNC __SPRT_ID(time_t) __SPRT_ID(mktime)(struct __SPRT_TM_NAME *_tm) {
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
#if SPRT_WINDOWS
		if (ret != -1) {
			_tm->tm_gmtoff = __getLocalGmtOff(_tm->tm_isdst);
			_tm->tm_gmt_type = __SPRT_ID(gmt_local);
		}
#endif
	}
	return ret;
}

__SPRT_C_FUNC __SPRT_ID(time_t)
		__SPRT_ID(strftime)(char *__SPRT_RESTRICT buf, __SPRT_ID(size_t) size,
				const char *__SPRT_RESTRICT fmt, const struct __SPRT_TM_NAME *__SPRT_RESTRICT _tm) {
	auto native = internal::getNativeTm(_tm);
	return strftime(buf, size, fmt, &native);
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
	auto native = internal::getNativeTm(_tm);
#if SPRT_ANDROID && !defined(__LP64__)
	return ::asctime64(&native);
#else
	return ::asctime(&native);
#endif
}

__SPRT_C_FUNC char *__SPRT_ID(ctime)(const __SPRT_ID(time_t) * t) {
#if SPRT_ANDROID && !defined(__LP64__)
	::time64_t native = *t;
	return ::ctime64(&native);
#elif SPRT_WINDOWS
	return ::_ctime64(t);
#else
	::time_t native = *t;
	return ::ctime(&native);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(timespec_get)(__SPRT_TIMESPEC_NAME *spec, int base) {
#if SPRT_ANDROID
	struct timespec nativeSpec;
	if (!platform::_timespec_get) {
		log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
				" not available for this platform (Android: API not available)");
		*__sprt___errno_location() = ENOSYS;
		return -1;
	}
	auto ret = platform::_timespec_get(&nativeSpec, base);
	spec->tv_sec = nativeSpec.tv_sec;
	spec->tv_nsec = nativeSpec.tv_nsec;
	return ret;
#elif SPRT_WINDOWS
	struct _timespec64 nativeSpec;
	auto ret = ::_timespec64_get(&nativeSpec, base);
	spec->tv_sec = nativeSpec.tv_sec;
	spec->tv_nsec = nativeSpec.tv_nsec;
	return ret;
#else
	struct timespec nativeSpec;
	auto ret = ::timespec_get(&nativeSpec, base);
	spec->tv_sec = nativeSpec.tv_sec;
	spec->tv_nsec = nativeSpec.tv_nsec;
	return ret;
#endif
}

__SPRT_C_FUNC struct __SPRT_TM_NAME *__SPRT_ID(
		gmtime_r)(const __SPRT_ID(time_t) * t, struct __SPRT_TM_NAME *_tm) {
	if (!t || !_tm) {
		return nullptr;
	}

	auto native = internal::getNativeTm(_tm);
#if SPRT_WINDOWS
	auto ret = ::_gmtime64_s(&native, t);
	if (ret == 0) {
		internal::getRuntimeTm(_tm, native);
		_tm->tm_gmtoff = 0;
		_tm->tm_gmt_type = __SPRT_ID(gmt_set);
		return _tm;
	}
	return nullptr;
#elif SPRT_ANDROID && !defined(__LP64__)
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
}

__SPRT_C_FUNC struct __SPRT_TM_NAME *__SPRT_ID(
		localtime_r)(const __SPRT_ID(time_t) * t, struct __SPRT_TM_NAME *_tm) {
	if (!t || !_tm) {
		return nullptr;
	}

	auto native = internal::getNativeTm(_tm);
#if SPRT_WINDOWS
	auto ret = ::_localtime64_s(&native, t);
	if (ret == 0) {
		internal::getRuntimeTm(_tm, native);
		_tm->tm_gmtoff = __getLocalGmtOff(native.tm_isdst);
		_tm->tm_gmt_type = __SPRT_ID(gmt_local);
		return _tm;
	}
	return nullptr;
#elif SPRT_ANDROID && !defined(__LP64__)
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
}

__SPRT_C_FUNC __SPRT_ID(size_t) __SPRT_ID(strftime_l)(char *__SPRT_RESTRICT buf,
		__SPRT_ID(size_t) size, const char *__SPRT_RESTRICT fmt,
		const struct __SPRT_TM_NAME *__SPRT_RESTRICT ts, __SPRT_ID(locale_t) loc) {
	auto native = internal::getNativeTm(ts);
#if SPRT_WINDOWS
	return ::_strftime_l(buf, size, fmt, &native, loc);
#else
	return ::strftime_l(buf, size, fmt, &native, loc);
#endif
}

__SPRT_C_FUNC char *__SPRT_ID(
		asctime_r)(const struct __SPRT_TM_NAME *__SPRT_RESTRICT ts, char *__SPRT_RESTRICT buf) {
	auto native = internal::getNativeTm(ts);
#if SPRT_ANDROID && !defined(__LP64__)
	return ::asctime64_r(&native, buf);
#elif SPRT_WINDOWS
	auto ret = ::asctime_s(buf, 27, &native);
	if (ret == 0) {
		return buf;
	}
	__sprt_errno = ret;
	return nullptr;
#else
	return ::asctime_r(&native, buf);
#endif
}
__SPRT_C_FUNC char *__SPRT_ID(ctime_r)(const __SPRT_ID(time_t) * t, char *buf) {
#if SPRT_ANDROID && !defined(__LP64__)
	return ::ctime64_r(t, buf);
#elif SPRT_WINDOWS
	auto ret = ::_ctime64_s(buf, 27, t);
	if (ret == 0) {
		return buf;
	}
	__sprt_errno = ret;
	return nullptr;
#else
	return ::ctime_r(t, buf);
#endif
}

__SPRT_C_FUNC void __SPRT_ID(tzset)(void) {
#if SPRT_WINDOWS
	::_tzset();
#else
	::tzset();
#endif
}

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

__SPRT_C_FUNC int __SPRT_ID(clock_getres)(__SPRT_ID(clockid_t) clock, __SPRT_TIMESPEC_NAME *out) {
#if SPRT_WINDOWS
	return clock_getres(clock, out);
#else
	struct timespec rem;
	auto ret = ::clock_getres(clock, &rem);
	if (out) {
		out->tv_nsec = rem.tv_nsec;
		out->tv_sec = rem.tv_sec;
	}
	return ret;
#endif
}

__SPRT_C_FUNC int __SPRT_ID(clock_gettime)(__SPRT_ID(clockid_t) clock, __SPRT_TIMESPEC_NAME *out) {
#if SPRT_WINDOWS
	return clock_gettime(clock, out);
#else
	struct timespec rem;
	auto ret = ::clock_gettime(clock, &rem);
	if (out) {
		out->tv_nsec = rem.tv_nsec;
		out->tv_sec = rem.tv_sec;
	}
	return ret;
#endif
}

__SPRT_C_FUNC int __SPRT_ID(
		clock_settime)(__SPRT_ID(clockid_t) clock, const __SPRT_TIMESPEC_NAME *ts) {
#if __SPRT_CONFIG_HAVE_TIME_CLOCK_SETTIME
	struct timespec native;
	if (ts) {
		native.tv_nsec = ts->tv_nsec;
		native.tv_sec = ts->tv_sec;
	}

	return ::clock_settime(clock, ts ? &native : nullptr);
#else
	__sprt_errno = ENOSYS;
	return -1;
#endif
}

__SPRT_C_FUNC int __SPRT_ID(clock_nanosleep)(__SPRT_ID(clockid_t) clock, int v,
		const __SPRT_TIMESPEC_NAME *ts, __SPRT_TIMESPEC_NAME *out) {
#if SPRT_WINDOWS
	return clock_nanosleep(clock, v, ts, out);
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
#if SPRT_WINDOWS
	__sprt_errno = ENOSYS;
	return -1;
#else
	return ::clock_getcpuclockid(pid, clock);
#endif
}

} // namespace sprt
