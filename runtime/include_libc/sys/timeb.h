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

#ifndef CORE_RUNTIME_INCLUDE_LIBC_SYS_TIMEB_H_
#define CORE_RUNTIME_INCLUDE_LIBC_SYS_TIMEB_H_

/*
	Legacy <sys/timeb.h> - the pre-POSIX millisecond clock, superseded by
	clock_gettime but still reached for by portable code (googletest includes it on
	every platform it considers "Windows").

	- hosted SPRT build -> forwards to the system <sys/timeb.h> (#include_next)
	- otherwise         -> SPRT's own declarations below

	Public surface provided by the SPRT-own path:

	Types:
	  struct timeb / struct _timeb - seconds + milliseconds since the epoch

	Functions:
	  ftime / _ftime   - fill a struct timeb with the current time

	sprt's time_t is 64-bit on every target, so `struct timeb` has the layout MSVC
	calls __timeb64 (and matches glibc's, which is also time_t-wide).

	timezone and dstflag are reported as zero. They describe the local-time offset
	the way the SVID interface did, which is neither what tzset() computes here nor
	something any live caller reads - ftime's remaining users want the millisecond
	timestamp.
*/

#if defined(__SPRT_BUILD) && __STDC_HOSTED__ == 1

#include_next <sys/timeb.h>

#else

#include <sprt/c/bits/__sprt_def.h>
#include <sprt/c/__sprt_time.h>

struct timeb {
	__SPRT_ID(time_t) time;
	unsigned short millitm;
	short timezone;
	short dstflag;
};

#define _timeb timeb

__SPRT_BEGIN_DECL

// Inline over __sprt_clock_gettime rather than an umbrella entry point of its own (the
// gmtime_s model in <time.h>): the whole body is one call plus arithmetic, so there is
// nothing for a runtime symbol to carry.
SPRT_FORCEINLINE int ftime(struct timeb *__tp) __SPRT_NOEXCEPT {
	struct __SPRT_TIMESPEC_NAME __ts;
	if (!__tp) {
		return -1;
	}
	if (__SPRT_ID(clock_gettime)(__SPRT_CLOCK_REALTIME, &__ts) != 0) {
		return -1;
	}
	__tp->time = __ts.tv_sec;
	__tp->millitm = (unsigned short)(__ts.tv_nsec / 1000000);
	__tp->timezone = 0;
	__tp->dstflag = 0;
	return 0;
}

// MSVC spells it with the underscore and gates the unprefixed name behind
// _CRT_NONSTDC_NO_WARNINGS; both names denote the same function.
SPRT_FORCEINLINE int _ftime(struct timeb *__tp) __SPRT_NOEXCEPT { return ftime(__tp); }

__SPRT_END_DECL

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_SYS_TIMEB_H_
