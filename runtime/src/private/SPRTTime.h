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

#ifndef CORE_RUNTIME_PRIVATE_SPRTTIME_H_
#define CORE_RUNTIME_PRIVATE_SPRTTIME_H_

#include <c/__sprt_time.h>
#include <time.h>

namespace sprt::internal {

static struct tm getNativeTm(const struct __SPRT_TM_NAME *_tm) {
	struct tm target{
		.tm_sec = _tm->tm_sec,
		.tm_min = _tm->tm_min,
		.tm_hour = _tm->tm_hour,
		.tm_mday = _tm->tm_mday,
		.tm_mon = _tm->tm_mon,
		.tm_year = _tm->tm_year,
		.tm_wday = _tm->tm_wday,
		.tm_yday = _tm->tm_yday,
		.tm_isdst = _tm->tm_isdst,
		.tm_gmtoff = _tm->tm_gmtoff,
		.tm_zone = _tm->tm_zone,
	};
	return target;
}

static void getRuntimeTm(struct __SPRT_TM_NAME *_tm, const struct tm &native) {
#if !defined(SPRT_ANDROID) && !defined(SPRT_LINUX)
	_tm->tm_sec = native.tm_sec;
#else
	_tm->tm_sec = 0;
#endif
	_tm->tm_sec = native.tm_sec;
	_tm->tm_min = native.tm_min;
	_tm->tm_hour = native.tm_hour;
	_tm->tm_mday = native.tm_mday;
	_tm->tm_mon = native.tm_mon;
	_tm->tm_year = native.tm_year;
	_tm->tm_wday = native.tm_wday;
	_tm->tm_yday = native.tm_yday;
	_tm->tm_isdst = native.tm_isdst;
	_tm->tm_gmtoff = native.tm_gmtoff;
	_tm->tm_zone = native.tm_zone;
}

} // namespace sprt::internal

#endif // CORE_RUNTIME_PRIVATE_SPRTTIME_H_
