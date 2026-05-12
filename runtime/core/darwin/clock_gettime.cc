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

#include <sprt/c/__sprt_errno.h>
#include <sprt/c/__sprt_time.h>

#include <sprt/cxx/detail/ctypes.h>

#include <mach/mach.h>
#include <time.h>

namespace sprt {

static constexpr unsigned ThreadIdBit = 0x8000'0000;

static int _clock_getres(unsigned clk_id, struct __SPRT_TIMESPEC_NAME *out) {
	if (clk_id & ThreadIdBit) {
		if (out) {
			out->tv_nsec = 1'000;
			out->tv_sec = 0;
		}
		return 0;
	} else {
		struct timespec rem;
		auto ret = ::clock_getres(clockid_t(clk_id), &rem);
		if (out) {
			out->tv_nsec = rem.tv_nsec;
			out->tv_sec = rem.tv_sec;
		}
		return ret;
	}
}

static int _clock_gettime(unsigned clk_id, struct __SPRT_TIMESPEC_NAME *out) {
	if (clk_id & ThreadIdBit) {
		thread_inspect_t mach_thread = static_cast<thread_inspect_t>(clk_id & ~ThreadIdBit);

		thread_basic_info_data_t info;
		mach_msg_type_number_t count = THREAD_BASIC_INFO_COUNT;

		kern_return_t kr =
				thread_info(mach_thread, THREAD_BASIC_INFO, (thread_info_t)&info, &count);

		if (kr != KERN_SUCCESS) {
			__sprt_errno = EINVAL;
			return -1; // Thread not found
		}

		// 3. Calculate Total CPU Time
		// info.user_time and info.system_time are in time_value_t (sec, usec)
		long total_sec = info.user_time.seconds + info.system_time.seconds;
		long total_usec = info.user_time.microseconds + info.system_time.microseconds;

		// Normalize microseconds
		if (total_usec >= 1'000'000) {
			total_sec += total_usec / 1'000'000;
			total_usec = total_usec % 1'000'000;
		}

		// 4. Fill timespec
		out->tv_sec = total_sec;
		out->tv_nsec = total_usec * 1'000;
		return 0;
	} else {
		struct timespec rem;
		auto ret = ::clock_gettime(clockid_t(clk_id), &rem);
		if (out) {
			out->tv_nsec = rem.tv_nsec;
			out->tv_sec = rem.tv_sec;
		}
		return ret;
	}
}

static int _clock_settime(unsigned clk_id, const struct __SPRT_TIMESPEC_NAME *ts) {
	if (clk_id & ThreadIdBit) {
		__sprt_errno = EINVAL;
		return -1;
	} else {
		struct timespec native;
		if (ts) {
			native.tv_nsec = ts->tv_nsec;
			native.tv_sec = ts->tv_sec;
		}

		return ::clock_settime(clockid_t(clk_id), ts ? &native : nullptr);
	}
}

} // namespace sprt
