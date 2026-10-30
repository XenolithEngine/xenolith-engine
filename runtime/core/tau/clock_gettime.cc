/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
    10|    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
**/

// Tau clocks: every clock reads the virtual counter since boot (there is no clock of the
// day); setting a clock is not supported.

#ifndef __SPRT_BUILD
#define __SPRT_BUILD 1
#endif

#include <sprt/c/__sprt_errno.h>
#include <sprt/c/__sprt_time.h>
#include <sprt/c/cross/tau_sprt/sprt_tau.h>

namespace sprt {

__SPRT_C_FUNC int clock_gettime(__SPRT_ID(clockid_t), struct __SPRT_TIMESPEC_NAME *tp) __SPRT_NOEXCEPT {
	if (!tp) {
		__sprt_errno = EFAULT;
		return -1;
	}
	unsigned long long ns = __sprt_tau_clock_ns();
	tp->tv_sec = (__SPRT_ID(time_t))(ns / 1'000'000'000ull);
	tp->tv_nsec = (long)(ns % 1'000'000'000ull);
	return 0;
}

__SPRT_C_FUNC int clock_getres(__SPRT_ID(clockid_t), struct __SPRT_TIMESPEC_NAME *tp) __SPRT_NOEXCEPT {
	if (!tp) {
		__sprt_errno = EFAULT;
		return -1;
	}
	tp->tv_sec = 0;
	tp->tv_nsec = (long)__sprt_tau_clock_res_ns();
	return 0;
}

static int _clock_settime(unsigned, const struct __SPRT_TIMESPEC_NAME *) {
	__sprt_errno = ENOSYS;
	return -1;
}

} // namespace sprt
