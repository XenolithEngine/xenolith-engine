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

#include <c/sys/__sprt_time.h>
#include <c/__sprt_stdio.h>
#include <c/__sprt_errno.h>

#include "private/SPRTFilename.h"
#include "private/SPRTPrivate.h"
#include "SPRuntimeLog.h"

#include <sys/time.h>

namespace sprt {

__SPRT_C_FUNC int __SPRT_ID(gettimeofday)(struct __SPRT_TIMEVAL_NAME *__SPRT_RESTRICT __tv,
		struct __SPRT_TIMEZONE_NAME *__SPRT_RESTRICT __tz) {
	struct timeval nativeTv;
	struct timezone nativeTz;
	auto ret = ::gettimeofday(&nativeTv, &nativeTz);
	if (ret == 0) {
		if (__tv) {
			__tv->tv_sec = nativeTv.tv_sec;
			__tv->tv_usec = nativeTv.tv_usec;
		}
		if (__tz) {
			__tz->tz_dsttime = nativeTz.tz_dsttime;
			__tz->tz_minuteswest = nativeTz.tz_minuteswest;
		}
	}
	return ret;
}

__SPRT_C_FUNC int __SPRT_ID(
		settimeofday)(const __SPRT_TIMEVAL_NAME *__tv, const struct __SPRT_ID(timezone) * __tz) {
	struct timeval nativeTv;
	struct timezone nativeTz;

	if (__tv) {
		nativeTv.tv_sec = __tv->tv_sec;
		nativeTv.tv_usec = __tv->tv_usec;
	}

	if (__tz) {
		nativeTz.tz_dsttime = __tz->tz_dsttime;
		nativeTz.tz_minuteswest = __tz->tz_minuteswest;
	}

	return ::settimeofday(__tv ? &nativeTv : nullptr, __tz ? &nativeTz : nullptr);
}

__SPRT_C_FUNC int __SPRT_ID(getitimer)(int __w, struct __SPRT_ID(itimerval) * __tv) {
	struct itimerval nativeTv;
	auto ret = ::getitimer(__w, &nativeTv);
	if (ret == 0 && __tv) {
		__tv->it_interval.tv_sec = nativeTv.it_interval.tv_sec;
		__tv->it_interval.tv_usec = nativeTv.it_interval.tv_usec;
		__tv->it_value.tv_sec = nativeTv.it_value.tv_sec;
		__tv->it_value.tv_usec = nativeTv.it_value.tv_usec;
	}
	return ret;
}

__SPRT_C_FUNC int __SPRT_ID(setitimer)(int __w,
		const struct __SPRT_ID(itimerval) * __SPRT_RESTRICT __tv,
		struct __SPRT_ID(itimerval) * __SPRT_RESTRICT __atv) {
	struct itimerval nativeInTv;
	struct itimerval nativeOutTv;

	if (__tv) {
		nativeInTv.it_interval.tv_sec = __tv->it_interval.tv_sec;
		nativeInTv.it_interval.tv_usec = __tv->it_interval.tv_usec;
		nativeInTv.it_value.tv_sec = __tv->it_value.tv_sec;
		nativeInTv.it_value.tv_usec = __tv->it_value.tv_usec;
	}

	auto ret = ::setitimer(__w, __tv ? &nativeInTv : nullptr, __atv ? &nativeOutTv : nullptr);
	if (ret == 0 && __atv) {
		__atv->it_interval.tv_sec = nativeOutTv.it_interval.tv_sec;
		__atv->it_interval.tv_usec = nativeOutTv.it_interval.tv_usec;
		__atv->it_value.tv_sec = nativeOutTv.it_value.tv_sec;
		__atv->it_value.tv_usec = nativeOutTv.it_value.tv_usec;
	}
	return ret;
}

__SPRT_C_FUNC int __SPRT_ID(utimes)(const char *path, const __SPRT_TIMEVAL_NAME ts[2]) {
	struct timeval nativeTs[2];
	nativeTs[0].tv_sec = ts[0].tv_sec;
	nativeTs[0].tv_usec = ts[0].tv_usec;
	nativeTs[1].tv_sec = ts[1].tv_sec;
	nativeTs[1].tv_usec = ts[1].tv_usec;

	return internal::performWithNativePath(path, [&](const char *target) {
		// call with native path
		return ::utimes(target, nativeTs);
	}, -1);
}

__SPRT_C_FUNC int __SPRT_ID(futimes)(int fd, const __SPRT_TIMEVAL_NAME ts[2]) {
	struct timeval nativeTs[2];
	nativeTs[0].tv_sec = ts[0].tv_sec;
	nativeTs[0].tv_usec = ts[0].tv_usec;
	nativeTs[1].tv_sec = ts[1].tv_sec;
	nativeTs[1].tv_usec = ts[1].tv_usec;

#if SPRT_ANDROID
	if (platform::_futimes) {
		return platform::_futimes(fd, nativeTs);
	}
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (Android: API not available)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#else
	return ::futimes(fd, nativeTs);
#endif
}

__SPRT_C_FUNC int __SPRT_ID(futimesat)(int fd, const char *path, const __SPRT_TIMEVAL_NAME ts[2]) {
	struct timeval nativeTs[2];
	nativeTs[0].tv_sec = ts[0].tv_sec;
	nativeTs[0].tv_usec = ts[0].tv_usec;
	nativeTs[1].tv_sec = ts[1].tv_sec;
	nativeTs[1].tv_usec = ts[1].tv_usec;

	return internal::performWithNativePath(path, [&](const char *target) {
#if SPRT_ANDROID
		if (platform::_futimesat) {
			return platform::_futimesat(fd, target, nativeTs);
		}
		log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
				" not available for this platform (Android: API not available)");
		*__sprt___errno_location() = ENOSYS;
		return -1;
#else
		return ::futimesat(fd, target, nativeTs);
#endif
	}, -1);
}

__SPRT_C_FUNC int __SPRT_ID(lutimes)(const char *path, const __SPRT_TIMEVAL_NAME ts[2]) {
	struct timeval nativeTs[2];
	nativeTs[0].tv_sec = ts[0].tv_sec;
	nativeTs[0].tv_usec = ts[0].tv_usec;
	nativeTs[1].tv_sec = ts[1].tv_sec;
	nativeTs[1].tv_usec = ts[1].tv_usec;

	return internal::performWithNativePath(path, [&](const char *target) {
#if SPRT_ANDROID
		if (platform::_lutimes) {
			return platform::_lutimes(target, nativeTs);
		}
		log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
				" not available for this platform (Android: API not available)");
		*__sprt___errno_location() = ENOSYS;
		return -1;
#else
		return ::lutimes(target, nativeTs);
#endif
	}, -1);
}

__SPRT_C_FUNC int __SPRT_ID(adjtime)(const __SPRT_TIMEVAL_NAME *__tv, __SPRT_TIMEVAL_NAME *__otv) {
#if !__SPRT_CONFIG_HAVE_ADJTIME
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_ADJTIME)");
	*__sprt___errno_location() = ENOSYS;
	return -1;
#else
	struct timeval nativeInTv;
	struct timeval nativeOutTv;

	if (__tv) {
		nativeInTv.tv_sec = __tv->tv_sec;
		nativeInTv.tv_usec = __tv->tv_usec;
	}

	auto ret = ::adjtime(__tv ? &nativeInTv : nullptr, __otv ? &nativeOutTv : nullptr);
	if (ret == 0 && __otv) {
		__otv->tv_sec = nativeOutTv.tv_sec;
		__otv->tv_usec = nativeOutTv.tv_usec;
	}
	return ret;
#endif
}

} // namespace sprt
