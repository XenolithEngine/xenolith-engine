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

#ifndef CORE_RUNTIME_INCLUDE_C___SPRT_TIME_H_
#define CORE_RUNTIME_INCLUDE_C___SPRT_TIME_H_

#include <c/bits/__sprt_def.h>
#include <c/bits/__sprt_null.h>
#include <c/bits/__sprt_time_t.h>
#include <c/bits/__sprt_size_t.h>
#include <c/cross/__sprt_locale.h>
#include <c/cross/__sprt_sysid.h>

// clang-format off
#define __SPRT_CLOCKS_PER_SEC 1000000L
#define __SPRT_TIME_UTC 1
// clang-format on

#define __SPRT_CLOCK_REALTIME           0
#define __SPRT_CLOCK_MONOTONIC          1
#define __SPRT_CLOCK_PROCESS_CPUTIME_ID 2
#define __SPRT_CLOCK_THREAD_CPUTIME_ID  3
#define __SPRT_CLOCK_MONOTONIC_RAW      4
#define __SPRT_CLOCK_REALTIME_COARSE    5
#define __SPRT_CLOCK_MONOTONIC_COARSE   6
#define __SPRT_CLOCK_BOOTTIME           7
#define __SPRT_CLOCK_REALTIME_ALARM     8
#define __SPRT_CLOCK_BOOTTIME_ALARM     9
#define __SPRT_CLOCK_SGI_CYCLE         10
#define __SPRT_CLOCK_TAI               11

__SPRT_BEGIN_DECL

struct __SPRT_TM_NAME {
	int tm_usec; /** microseconds past tm_sec */
	int tm_sec; /** (0-61) seconds past tm_min */
	int tm_min; /** (0-59) minutes past tm_hour */
	int tm_hour; /** (0-23) hours past midnight */
	int tm_mday; /** (1-31) day of the month */
	int tm_mon; /** (0-11) month of the year */
	int tm_year; /** year since 1900 */
	int tm_wday; /** (0-6) days since Sunday */
	int tm_yday; /** (0-365) days since January 1 */
	int tm_isdst; /** daylight saving time */
	long tm_gmtoff; /** seconds east of UTC */
	const char *tm_zone;
};

SPRT_API __SPRT_ID(clock_t) __SPRT_ID(clock)(void);
SPRT_API __SPRT_ID(time_t) __SPRT_ID(time)(__SPRT_ID(time_t) *);
SPRT_API double __SPRT_ID(difftime)(__SPRT_ID(time_t), __SPRT_ID(time_t));
SPRT_API __SPRT_ID(time_t) __SPRT_ID(mktime)(struct __SPRT_TM_NAME *);
SPRT_API __SPRT_ID(time_t) __SPRT_ID(strftime)(char *__SPRT_RESTRICT, __SPRT_ID(size_t),
		const char *__SPRT_RESTRICT, const struct __SPRT_TM_NAME *__SPRT_RESTRICT);
SPRT_API struct __SPRT_TM_NAME *__SPRT_ID(gmtime)(const __SPRT_ID(time_t) *);
SPRT_API struct __SPRT_TM_NAME *__SPRT_ID(localtime)(const __SPRT_ID(time_t) *);
SPRT_API char *__SPRT_ID(asctime)(const struct __SPRT_TM_NAME *);
SPRT_API char *__SPRT_ID(ctime)(const __SPRT_ID(time_t) *);
SPRT_API int __SPRT_ID(timespec_get)(__SPRT_TIMESPEC_NAME *, int);

SPRT_API struct __SPRT_TM_NAME *__SPRT_ID(
		gmtime_r)(const __SPRT_ID(time_t) *, struct __SPRT_TM_NAME *);
SPRT_API struct __SPRT_TM_NAME *__SPRT_ID(
		localtime_r)(const __SPRT_ID(time_t) *, struct __SPRT_TM_NAME *);

SPRT_API __SPRT_ID(size_t)
		__SPRT_ID(strftime_l)(char *__SPRT_RESTRICT, __SPRT_ID(size_t), const char *__SPRT_RESTRICT,
				const struct __SPRT_TM_NAME *__SPRT_RESTRICT, __SPRT_ID(locale_t));

SPRT_API char *__SPRT_ID(
		asctime_r)(const struct __SPRT_TM_NAME *__SPRT_RESTRICT, char *__SPRT_RESTRICT);
SPRT_API char *__SPRT_ID(ctime_r)(const __SPRT_ID(time_t) *, char *);

SPRT_API void __SPRT_ID(tzset)(void);

SPRT_API int __SPRT_ID(nanosleep)(const __SPRT_TIMESPEC_NAME *, __SPRT_TIMESPEC_NAME *);
SPRT_API int __SPRT_ID(clock_getres)(__SPRT_ID(clockid_t), __SPRT_TIMESPEC_NAME *);
SPRT_API int __SPRT_ID(clock_gettime)(__SPRT_ID(clockid_t), __SPRT_TIMESPEC_NAME *);
SPRT_API int __SPRT_ID(clock_settime)(__SPRT_ID(clockid_t), const __SPRT_TIMESPEC_NAME *);
SPRT_API int __SPRT_ID(clock_nanosleep)(__SPRT_ID(clockid_t), int, const __SPRT_TIMESPEC_NAME *,
		__SPRT_TIMESPEC_NAME *);
SPRT_API int __SPRT_ID(clock_getcpuclockid)(__SPRT_ID(pid_t), __SPRT_ID(clockid_t) *);

__SPRT_END_DECL

#endif // CORE_RUNTIME_INCLUDE_C___SPRT_TIME_H_
