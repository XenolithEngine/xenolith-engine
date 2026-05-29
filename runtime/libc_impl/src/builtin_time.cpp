/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without __restriction, including without limitation the rights
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

#include <sprt/cxx/mutex>
#include <sprt/c/__sprt_time.h>
#include <sprt/runtime/utils/time.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <sys/time.h>

static const char __utc[] = "UTC";

#if SPRT_WINDOWS
#include "windows/time.cc"
#include "windows/tz.cc"
#endif

__SPRT_C_FUNC time_t time(time_t *t) __SPRT_NOEXCEPT {
	struct timespec ts;
	if (__sprt_clock_gettime(CLOCK_REALTIME, &ts) == 0) {
		if (t) {
			*t = ts.tv_sec;
		}
		return ts.tv_sec;
	}
	return -1;
}

__SPRT_C_FUNC size_t strftime_l(char *__restrict s, size_t n, const char *__restrict f,
		const struct tm *__restrict tm, locale_t loc) __SPRT_NOEXCEPT {
	if (!s || !f || !tm) {
		errno = EINVAL;
		return -1;
	}
	sprt::time::time_exp_t exp(*tm);
	return exp.strftime(s, n, f);
}

__SPRT_C_FUNC size_t strftime(char *__restrict s, size_t n, const char *__restrict f,
		const struct tm *__restrict tm) __SPRT_NOEXCEPT {
	return strftime_l(s, n, f, tm, nullptr);
}

__SPRT_C_FUNC size_t wcsftime_l(wchar_t *__restrict ptr, size_t s, const wchar_t *__restrict fmt,
		const struct tm *__restrict value, locale_t loc) __SPRT_NOEXCEPT {
	if (!ptr || fmt || value) {
		errno = EINVAL;
		return -1;
	}

	size_t ret = 0;
	sprt::unicode::toUtf8([&](sprt::StringView str) {
		auto buf = __sprt_typed_malloca(char, s + 1);
		auto len = strftime_l(buf, s, str.data(), value, loc);
		sprt::unicode::toUtf16((char16_t *)ptr, s, sprt::StringView(buf, len), &ret);
	}, sprt::WideStringView((char16_t *)fmt));
	return ret;
}

__SPRT_C_FUNC size_t wcsftime(wchar_t *__restrict ptr, size_t s, const wchar_t *__restrict fmt,
		const struct tm *__restrict value) __SPRT_NOEXCEPT {
	return wcsftime_l(ptr, s, fmt, value, nullptr);
}

__SPRT_C_FUNC char *asctime_r(const struct tm *__restrict tm,
		char *__restrict buf) __SPRT_NOEXCEPT {
	sprt::time::time_exp_t exp(*tm);
	exp.asctime(buf, 27);
	return buf;
}

__SPRT_C_FUNC time_t mktime(struct tm *tm) __SPRT_NOEXCEPT {
	sprt::time::time_exp_t exp(*tm);
	return exp.gmt_geti();
}

__SPRT_C_FUNC int gettimeofday(struct timeval *__SPRT_RESTRICT __tv,
		struct timezone *__SPRT_RESTRICT __tz) __SPRT_NOEXCEPT {
	if (__tv) {
		struct __SPRT_TIMESPEC_NAME ts;
		if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
			__tv->tv_sec = ts.tv_sec;
			__tv->tv_usec = ts.tv_nsec / 100;
		} else {
			return -1;
		}
	}

	if (__tz) {
		tzset();
		__tz->tz_dsttime = daylight;
		__tz->tz_minuteswest = timezone / 60;
	}

	return 0;
}

/* 2000-03-01 (mod 400 year, immediately after feb29 */
#define LEAPOCH (946'684'800LL + 86'400*(31+29))

#define DAYS_PER_400Y (365*400 + 97)
#define DAYS_PER_100Y (365*100 + 24)
#define DAYS_PER_4Y   (365*4   + 1)

static int __secs_to_tm(long long t, struct tm *tm) {
	long long days, secs, years;
	int remdays, remsecs, remyears;
	int qc_cycles, c_cycles, q_cycles;
	int months;
	int wday, yday, leap;
	static const char days_in_month[] = {31, 30, 31, 30, 31, 31, 30, 31, 30, 31, 31, 29};

	/* Reject time_t values whose year would overflow int */
	if (t < INT_MIN * 31'622'400LL || t > INT_MAX * 31'622'400LL) {
		return -1;
	}

	secs = t - LEAPOCH;
	days = secs / 86'400;
	remsecs = secs % 86'400;
	if (remsecs < 0) {
		remsecs += 86'400;
		days--;
	}

	wday = (3 + days) % 7;
	if (wday < 0) {
		wday += 7;
	}

	qc_cycles = days / DAYS_PER_400Y;
	remdays = days % DAYS_PER_400Y;
	if (remdays < 0) {
		remdays += DAYS_PER_400Y;
		qc_cycles--;
	}

	c_cycles = remdays / DAYS_PER_100Y;
	if (c_cycles == 4) {
		c_cycles--;
	}
	remdays -= c_cycles * DAYS_PER_100Y;

	q_cycles = remdays / DAYS_PER_4Y;
	if (q_cycles == 25) {
		q_cycles--;
	}
	remdays -= q_cycles * DAYS_PER_4Y;

	remyears = remdays / 365;
	if (remyears == 4) {
		remyears--;
	}
	remdays -= remyears * 365;

	leap = !remyears && (q_cycles || !c_cycles);
	yday = remdays + 31 + 28 + leap;
	if (yday >= 365 + leap) {
		yday -= 365 + leap;
	}

	years = remyears + 4 * q_cycles + 100 * c_cycles + 400LL * qc_cycles;

	for (months = 0; days_in_month[months] <= remdays; months++) {
		remdays -= days_in_month[months];
	}

	if (months >= 10) {
		months -= 12;
		years++;
	}

	if (years + 100 > INT_MAX || years + 100 < INT_MIN) {
		return -1;
	}

	tm->tm_year = years + 100;
	tm->tm_mon = months + 2;
	tm->tm_mday = remdays + 1;
	tm->tm_wday = wday;
	tm->tm_yday = yday;

	tm->tm_hour = remsecs / 3'600;
	tm->tm_min = remsecs / 60 % 60;
	tm->tm_sec = remsecs % 60;

	return 0;
}

__SPRT_C_FUNC char *ctime_r(const time_t *t, char *buf) __SPRT_NOEXCEPT {
	struct tm tm, *tm_p = localtime_r(t, &tm);
	return tm_p ? asctime_r(tm_p, buf) : 0;
}

__SPRT_C_FUNC struct tm *gmtime_r(const time_t *__restrict t,
		struct tm *__restrict _tm) __SPRT_NOEXCEPT {
	if (__secs_to_tm(*t, _tm) < 0) {
		errno = EOVERFLOW;
		return 0;
	}
	_tm->tm_usec = 0;
	_tm->tm_isdst = 0;
	_tm->tm_gmtoff = 0;
	_tm->tm_zone = __utc;
	_tm->tm_gmt_type = __sprt_gmt_set;
	return _tm;
}

thread_local struct tm gmtime_tm;

__SPRT_C_FUNC struct tm *gmtime(const time_t *t) __SPRT_NOEXCEPT {
	return gmtime_r(t, &gmtime_tm); //
}

thread_local struct tm localtime_tm;

__SPRT_C_FUNC struct tm *localtime(const time_t *t) __SPRT_NOEXCEPT {
	return localtime_r(t, &localtime_tm); //
}

thread_local char buf[27];

__SPRT_C_FUNC char *asctime(const struct tm *tm) __SPRT_NOEXCEPT {
	return asctime_r(tm, buf); //
}

__SPRT_C_FUNC char *ctime(const time_t *t) __SPRT_NOEXCEPT {
	struct tm *tm = localtime(t);
	if (!tm) {
		return 0;
	}
	return asctime(tm);
}

__SPRT_C_FUNC double difftime(time_t t1, time_t t0) __SPRT_NOEXCEPT { return t1 - t0; }
