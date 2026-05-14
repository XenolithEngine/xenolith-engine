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
#warning TODO
	return 0;
}

__SPRT_C_FUNC size_t strftime(char *__restrict s, size_t n, const char *__restrict f,
		const struct tm *__restrict tm) __SPRT_NOEXCEPT {
#warning TODO
	return 0;
}

__SPRT_C_FUNC size_t wcsftime_l(wchar_t *__restrict ptr, size_t s, const wchar_t *__restrict fmt,
		const struct tm *__restrict value, locale_t loc) __SPRT_NOEXCEPT {
#warning TODO
	return 0;
}

__SPRT_C_FUNC size_t wcsftime(wchar_t *__restrict ptr, size_t s, const wchar_t *__restrict fmt,
		const struct tm *__restrict value) __SPRT_NOEXCEPT {
#warning TODO
	return 0;
}

__SPRT_C_FUNC struct tm *__gmtime_r(const time_t *__restrict t,
		struct tm *__restrict tm) __SPRT_NOEXCEPT {
	return nullptr;
}

__SPRT_C_FUNC struct tm *__localtime_r(const time_t *__restrict t,
		struct tm *__restrict tm) __SPRT_NOEXCEPT {
	return nullptr;
}

__SPRT_C_FUNC char *__asctime_r(const struct tm *__restrict tm,
		char *__restrict buf) __SPRT_NOEXCEPT {
	return nullptr;
}

__SPRT_C_FUNC time_t mktime(struct tm *tm) __SPRT_NOEXCEPT {
	sprt::time::time_exp_t exp(*tm);
	return exp.gmt_geti();
}


#define weak_alias(old, new) \
	extern __typeof(old) new __attribute__((__weak__, __alias__(#old)))

weak_alias(__localtime_r, localtime_r);
weak_alias(__gmtime_r, gmtime_r);
weak_alias(__asctime_r, asctime_r);
