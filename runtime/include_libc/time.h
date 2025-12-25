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

#ifndef CORE_RUNTIME_INCLUDE_LIBC_TIME_H_
#define CORE_RUNTIME_INCLUDE_LIBC_TIME_H_

#ifdef __SPRT_BUILD

#include_next <time.h>

#else

#include <c/__sprt_time.h>

#define CLOCKS_PER_SEC __SPRT_CLOCKS_PER_SEC
#define TIME_UTC __SPRT_TIME_UTC

#define CLOCK_REALTIME __SPRT_CLOCK_REALTIME
#define CLOCK_MONOTONIC __SPRT_CLOCK_MONOTONIC
#define CLOCK_PROCESS_CPUTIME_ID __SPRT_CLOCK_PROCESS_CPUTIME_ID
#define CLOCK_THREAD_CPUTIME_ID __SPRT_CLOCK_THREAD_CPUTIME_ID
#define CLOCK_MONOTONIC_RAW __SPRT_CLOCK_MONOTONIC_RAW
#define CLOCK_REALTIME_COARSE __SPRT_CLOCK_REALTIME_COARSE
#define CLOCK_MONOTONIC_COARSE __SPRT_CLOCK_MONOTONIC_COARSE
#define CLOCK_BOOTTIME __SPRT_CLOCK_BOOTTIME
#define CLOCK_REALTIME_ALARM __SPRT_CLOCK_REALTIME_ALARM
#define CLOCK_BOOTTIME_ALARM __SPRT_CLOCK_BOOTTIME_ALARM
#define CLOCK_SGI_CYCLE __SPRT_CLOCK_SGI_CYCLE
#define CLOCK_TAI __SPRT_CLOCK_TAI

typedef __SPRT_ID(size_t) size_t;
typedef __SPRT_ID(time_t) time_t;
typedef __SPRT_ID(clock_t) clock_t;
typedef __SPRT_ID(clockid_t) clockid_t;
typedef __SPRT_ID(locale_t) locale_t;
typedef __SPRT_ID(pid_t) pid_t;

__SPRT_BEGIN_DECL

SPRT_FORCEINLINE inline clock_t clock(void) { return __sprt_clock(); }

SPRT_FORCEINLINE inline time_t time(time_t *t) { return __sprt_time(t); }

SPRT_FORCEINLINE inline double difftime(time_t a, time_t b) { return __sprt_difftime(a, b); }

SPRT_FORCEINLINE inline time_t mktime(struct tm *_tm) { return __sprt_mktime(_tm); }

SPRT_FORCEINLINE inline time_t strftime(char *__SPRT_RESTRICT buf, size_t size,
		const char *__SPRT_RESTRICT fmt, const struct tm *__SPRT_RESTRICT _tm) {
	return __sprt_strftime(buf, size, fmt, _tm);
}

SPRT_FORCEINLINE inline struct tm *gmtime(const time_t *t) { return __sprt_gmtime(t); }

SPRT_FORCEINLINE inline struct tm *localtime(const time_t *t) { return __sprt_localtime(t); }

SPRT_FORCEINLINE inline char *asctime(const struct tm *_tm) { return __sprt_asctime(_tm); }

SPRT_FORCEINLINE inline char *ctime(const time_t *t) { return __sprt_ctime(t); }

SPRT_FORCEINLINE inline int timespec_get(struct timespec *spec, int base) {
	return __sprt_timespec_get(spec, base);
}

SPRT_FORCEINLINE inline struct tm *gmtime_r(const time_t *t, struct tm *_tm) {
	return __sprt_gmtime_r(t, _tm);
}

SPRT_FORCEINLINE inline struct tm *localtime_r(const time_t *t, struct tm *_tm) {
	return __sprt_localtime_r(t, _tm);
}

SPRT_FORCEINLINE inline size_t strftime_l(char *__SPRT_RESTRICT buf, size_t size,
		const char *__SPRT_RESTRICT fmt, const struct tm *__SPRT_RESTRICT ts, locale_t loc) {
	return __sprt_strftime_l(buf, size, fmt, ts, loc);
}

SPRT_FORCEINLINE inline char *asctime_r(const struct tm *__SPRT_RESTRICT ts,
		char *__SPRT_RESTRICT buf) {
	return __sprt_asctime_r(ts, buf);
}
SPRT_FORCEINLINE inline char *ctime_r(const time_t *t, char *buf) { return __sprt_ctime_r(t, buf); }

SPRT_FORCEINLINE inline void tzset(void) { __sprt_tzset(); }

SPRT_FORCEINLINE inline int nanosleep(const struct timespec *ts, struct timespec *out) {
	return __sprt_nanosleep(ts, out);
}

SPRT_FORCEINLINE inline int clock_getres(clockid_t clock, struct timespec *out) {
	return __sprt_clock_getres(clock, out);
}

SPRT_FORCEINLINE inline int clock_gettime(clockid_t clock, struct timespec *out) {
	return __sprt_clock_gettime(clock, out);
}

SPRT_FORCEINLINE inline int clock_settime(clockid_t clock, const struct timespec *ts) {
	return __sprt_clock_settime(clock, ts);
}

SPRT_FORCEINLINE inline int clock_nanosleep(clockid_t clock, int v, const struct timespec *ts,
		struct timespec *out) {
	return __sprt_clock_nanosleep(clock, v, ts, out);
}

SPRT_FORCEINLINE inline int clock_getcpuclockid(pid_t pid, clockid_t *clock) {
	return __sprt_clock_getcpuclockid(pid, clock);
}

__SPRT_END_DECL

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_TIME_H_
