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

#define __SPRT_BUILD 1

#include <sprt/c/__sprt_errno.h>
#include <sprt/c/__sprt_time.h>

#include <sprt/cxx/detail/ctypes.h>

#include <time.h>

namespace sprt {

static int clock_getres(unsigned clk_id, struct __SPRT_TIMESPEC_NAME *out) {
	struct timespec rem;
	auto ret = ::clock_getres(clockid_t(clk_id), &rem);
	if (out) {
		out->tv_nsec = rem.tv_nsec;
		out->tv_sec = rem.tv_sec;
	}
	return ret;
}

static int clock_gettime(unsigned clk_id, struct __SPRT_TIMESPEC_NAME *out) {
	struct timespec rem;
	auto ret = ::clock_gettime(clockid_t(clk_id), &rem);
	if (out) {
		out->tv_nsec = rem.tv_nsec;
		out->tv_sec = rem.tv_sec;
	}
	return ret;
}

static int _clock_settime(unsigned clk_id, const struct __SPRT_TIMESPEC_NAME *ts) {
	struct timespec native;
	if (ts) {
		native.tv_nsec = ts->tv_nsec;
		native.tv_sec = ts->tv_sec;
	}

	return ::clock_settime(clockid_t(clk_id), ts ? &native : nullptr);
}

} // namespace sprt
