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

#ifndef CORE_RUNTIME_INCLUDE_C_SYS___SPRT_TIME_H_
#define CORE_RUNTIME_INCLUDE_C_SYS___SPRT_TIME_H_

#include <c/bits/__sprt_time_t.h>
#include <c/bits/atfile.h>
#include <c/cross/__sprt_config.h>

#define __SPRT_ITIMER_REAL    0
#define __SPRT_ITIMER_VIRTUAL 1
#define __SPRT_ITIMER_PROF    2

#ifdef __SPRT_BUILD
#define __SPRT_ITIMERVAL_NAME __SPRT_ID(itimerval)
#define __SPRT_TIMEZONE_NAME __SPRT_ID(timezone)
#else
#define __SPRT_ITIMERVAL_NAME itimerval
#define __SPRT_TIMEZONE_NAME timezone
#endif

__SPRT_BEGIN_DECL

struct __SPRT_ITIMERVAL_NAME {
	__SPRT_TIMEVAL_NAME it_interval;
	__SPRT_TIMEVAL_NAME it_value;
};

struct __SPRT_TIMEZONE_NAME {
	int tz_minuteswest;
	int tz_dsttime;
};

SPRT_API int __SPRT_ID(gettimeofday)(struct __SPRT_TIMEVAL_NAME *__SPRT_RESTRICT,
		struct __SPRT_TIMEZONE_NAME *__SPRT_RESTRICT);
SPRT_API int __SPRT_ID(
		settimeofday)(const struct __SPRT_TIMEVAL_NAME *, const struct __SPRT_TIMEZONE_NAME *);

SPRT_API int __SPRT_ID(getitimer)(int, struct __SPRT_ITIMERVAL_NAME *);
SPRT_API int __SPRT_ID(setitimer)(int, const struct __SPRT_ITIMERVAL_NAME *__SPRT_RESTRICT,
		struct __SPRT_ITIMERVAL_NAME *__SPRT_RESTRICT);
SPRT_API int __SPRT_ID(utimes)(const char *, const __SPRT_TIMEVAL_NAME[2]);

SPRT_API int __SPRT_ID(futimes)(int, const __SPRT_TIMEVAL_NAME[2]);
SPRT_API int __SPRT_ID(futimesat)(int, const char *, const __SPRT_TIMEVAL_NAME[2]);
SPRT_API int __SPRT_ID(lutimes)(const char *, const __SPRT_TIMEVAL_NAME[2]);


#if __SPRT_CONFIG_HAVE_ADJTIME || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS
SPRT_API int __SPRT_ID(adjtime)(const __SPRT_TIMEVAL_NAME *, __SPRT_TIMEVAL_NAME *);
#endif

#define __sprt_timerisset(t) ((t)->tv_sec || (t)->tv_usec)
#define __sprt_timerclear(t) ((t)->tv_sec = (t)->tv_usec = 0)
#define __sprt_timercmp(s, t, op) ((s)->tv_sec == (t)->tv_sec ? \
	(s)->tv_usec op (t)->tv_usec : (s)->tv_sec op (t)->tv_sec)
#define __sprt_timeradd(s, t, a) (void) ( (a)->tv_sec = (s)->tv_sec + (t)->tv_sec, \
	((a)->tv_usec = (s)->tv_usec + (t)->tv_usec) >= 1'000'000 && \
	((a)->tv_usec -= 1'000'000, (a)->tv_sec++) )
#define __sprt_timersub(s, t, a) (void) ( (a)->tv_sec = (s)->tv_sec - (t)->tv_sec, \
	((a)->tv_usec = (s)->tv_usec - (t)->tv_usec) < 0 && \
	((a)->tv_usec += 1'000'000, (a)->tv_sec--) )

#define __SPRT_TIMEVAL_TO_TIMESPEC(tv, ts) ( \
	(ts)->tv_sec = (tv)->tv_sec, \
	(ts)->tv_nsec = (tv)->tv_usec * 1'000, \
	(void)0 )
#define __SPRT_TIMESPEC_TO_TIMEVAL(tv, ts) ( \
	(tv)->tv_sec = (ts)->tv_sec, \
	(tv)->tv_usec = (ts)->tv_nsec / 1'000, \
	(void)0 )

__SPRT_END_DECL

#endif // CORE_RUNTIME_INCLUDE_C_SYS___SPRT_TIME_H_
