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

/*
	Dispatch header for <time.h>:
	- hosted SPRT build -> forwards to the system <time.h> (#include_next)
	- otherwise         -> SPRT's own declarations (defined inline below)

	Public surface provided by the SPRT-own path (internal __sprt_* helpers excluded).
	struct tm and struct timespec come in via <sprt/c/__sprt_time.h>.
	A function tagged [gate: X] is declared only when __SPRT_CONFIG_HAVE_X is set for
	the target (or when __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS forces all of them).

	Macros:
	  CLOCKS_PER_SEC, TIME_UTC, TIMER_ABSTIME, and the POSIX clock ids
	  CLOCK_REALTIME, CLOCK_MONOTONIC, CLOCK_PROCESS_CPUTIME_ID,
	  CLOCK_THREAD_CPUTIME_ID, CLOCK_MONOTONIC_RAW, CLOCK_REALTIME_COARSE,
	  CLOCK_MONOTONIC_COARSE, CLOCK_BOOTTIME, CLOCK_REALTIME_ALARM,
	  CLOCK_BOOTTIME_ALARM, CLOCK_SGI_CYCLE, CLOCK_TAI

	Types:
	  size_t, time_t, clock_t, clockid_t, locale_t, pid_t

	ISO C functions:
	  clock        - processor time consumed by the program
	  time         - current calendar time as time_t
	  difftime     - difference between two times, in seconds
	  mktime       - convert broken-down local time to time_t
	  strftime     - format broken-down time into a string
	  gmtime       - convert time_t to broken-down UTC time
	  localtime    - convert time_t to broken-down local time
	  asctime      - render broken-down time as a fixed-form string
	  ctime        - render a time_t as a fixed-form string
	  timespec_get - read the current time into a timespec

	POSIX / extensions:
	  gmtime_r/localtime_r   - reentrant gmtime/localtime
	  asctime_r/ctime_r      - reentrant asctime/ctime
	  strftime_l             - strftime using an explicit locale
	  tzset                  - initialize timezone state from the environment
	  nanosleep              - high-resolution sleep
	  clock_getres           - query a clock's resolution
	  clock_gettime          - read a clock's current value
	  clock_settime          - set a clock's value  [gate: TIME_CLOCK_SETTIME]
	  clock_nanosleep        - sleep measured against a specific clock
	  clock_getcpuclockid    - obtain a process's CPU-time clock id
	  clock_gettime_nsec_np  - read a clock as nanoseconds (Apple non-portable)

	MSVC bounds-checked extensions:
	  gmtime_s   - bounds-checked gmtime into a caller buffer
*/

#if defined(__SPRT_BUILD) && __STDC_HOSTED__ == 1

#include_next <time.h>

#else

#include <sprt/c/__sprt_time.h>

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

#define TIMER_ABSTIME __SPRT_TIMER_ABSTIME

typedef __SPRT_ID(size_t) size_t;
typedef __SPRT_ID(time_t) time_t;
typedef __SPRT_ID(clock_t) clock_t;
typedef __SPRT_ID(clockid_t) clockid_t;
typedef __SPRT_ID(locale_t) locale_t;
typedef __SPRT_ID(pid_t) pid_t;

__SPRT_BEGIN_DECL

SPRT_UMBRELLA_FUNC
clock_t clock(void) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_clock();
}
#endif

SPRT_UMBRELLA_FUNC
time_t time(time_t *t) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_time(t);
}
#endif

SPRT_UMBRELLA_FUNC
double difftime(time_t a, time_t b) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_difftime(a, b);
}
#endif

SPRT_UMBRELLA_FUNC
time_t mktime(struct tm *_tm) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_mktime(_tm);
}
#endif

SPRT_UMBRELLA_FUNC
size_t strftime(char *__SPRT_RESTRICT buf, size_t size, const char *__SPRT_RESTRICT fmt,
		const struct tm *__SPRT_RESTRICT _tm) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_strftime(buf, size, fmt, _tm);
}
#endif

SPRT_UMBRELLA_FUNC
struct tm *gmtime(const time_t *t) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_gmtime(t);
}
#endif

SPRT_UMBRELLA_FUNC
struct tm *localtime(const time_t *t) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_localtime(t);
}
#endif

SPRT_UMBRELLA_FUNC
char *asctime(const struct tm *_tm) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_asctime(_tm);
}
#endif

SPRT_UMBRELLA_FUNC
char *ctime(const time_t *t) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_ctime(t);
}
#endif

SPRT_UMBRELLA_FUNC
int timespec_get(struct timespec *spec, int base) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_timespec_get(spec, base);
}
#endif

SPRT_UMBRELLA_FUNC
struct tm *gmtime_r(const time_t *t, struct tm *_tm) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_gmtime_r(t, _tm);
}
#endif

SPRT_FORCEINLINE int gmtime_s(struct tm *const _Tm, const __SPRT_ID(time_t) * _Time) SPRT_NOEXCEPT {
	if (__sprt_gmtime_r(_Time, _Tm) != __SPRT_NULL) {
		return 0;
	}
	return -1;
}

SPRT_UMBRELLA_FUNC
struct tm *localtime_r(const time_t *t, struct tm *_tm) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_localtime_r(t, _tm);
}
#endif

SPRT_UMBRELLA_FUNC
size_t strftime_l(char *__SPRT_RESTRICT buf, size_t size, const char *__SPRT_RESTRICT fmt,
		const struct tm *__SPRT_RESTRICT ts, locale_t loc) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_strftime_l(buf, size, fmt, ts, loc);
}
#endif

SPRT_UMBRELLA_FUNC
char *asctime_r(const struct tm *__SPRT_RESTRICT ts, char *__SPRT_RESTRICT buf) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_asctime_r(ts, buf);
}
#endif
SPRT_UMBRELLA_FUNC
char *ctime_r(const time_t *t, char *buf) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_ctime_r(t, buf);
}
#endif

SPRT_UMBRELLA_FUNC
void tzset(void) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	__sprt_tzset();
}
#endif

SPRT_UMBRELLA_FUNC
int nanosleep(const struct timespec *ts, struct timespec *out) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_nanosleep(ts, out);
}
#endif

SPRT_UMBRELLA_FUNC
int clock_getres(clockid_t clock, struct timespec *out) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_clock_getres(clock, out);
}
#endif

SPRT_UMBRELLA_FUNC
int clock_gettime(clockid_t clock, struct timespec *out) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_clock_gettime(clock, out);
}
#endif

#if __SPRT_CONFIG_HAVE_TIME_CLOCK_SETTIME || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS
SPRT_UMBRELLA_FUNC
int clock_settime(clockid_t clock, const struct timespec *ts) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_clock_settime(clock, ts);
}
#endif
#endif

SPRT_UMBRELLA_FUNC
int clock_nanosleep(clockid_t clock, int v, const struct timespec *ts,
		struct timespec *out) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_clock_nanosleep(clock, v, ts, out);
}
#endif

SPRT_UMBRELLA_FUNC
int clock_getcpuclockid(pid_t pid, clockid_t *clock) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_clock_getcpuclockid(pid, clock);
}
#endif

SPRT_UMBRELLA_FUNC
__SPRT_ID(uint64_t)
clock_gettime_nsec_np(clockid_t clock) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_clock_gettime_nsec_np(clock);
}
#endif

__SPRT_END_DECL

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_TIME_H_
