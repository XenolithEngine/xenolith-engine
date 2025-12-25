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

#ifndef CORE_RUNTIME_INCLUDE_SPRUNTIMETIME_H_
#define CORE_RUNTIME_INCLUDE_SPRUNTIMETIME_H_

#include "SPRuntimeInt.h"
#include "SPRuntimeString.h"

namespace sprt::time {

static constexpr uint64_t USEC_PER_SEC(1'000'000);

struct SPRT_API time_exp_t {
	enum tm_gmt_e {
		gmt_unset,
		gmt_local,
		gmt_set,
	};

	int32_t tm_usec; /** microseconds past tm_sec */
	int32_t tm_sec; /** (0-61) seconds past tm_min */
	int32_t tm_min; /** (0-59) minutes past tm_hour */
	int32_t tm_hour; /** (0-23) hours past midnight */
	int32_t tm_mday; /** (1-31) day of the month */
	int32_t tm_mon; /** (0-11) month of the year */
	int32_t tm_year; /** year since 1900 */
	int32_t tm_wday; /** (0-6) days since Sunday */
	int32_t tm_yday; /** (0-365) days since January 1 */
	int32_t tm_isdst; /** daylight saving time */
	int32_t tm_gmtoff; /** seconds east of UTC */
	tm_gmt_e tm_gmt_type = gmt_unset;

	time_exp_t();
	time_exp_t(int64_t t, int32_t offset, bool use_localtime);
	time_exp_t(int64_t t, int32_t offs);
	time_exp_t(int64_t t);
	time_exp_t(int64_t t, bool use_localtime);

	/*
	 * Parses an HTTP date in one of three standard forms:
	 *
	 *     Sun, 06 Nov 1994 08:49:37 GMT  ; RFC 822, updated by RFC 1123
	 *     Sunday, 06-Nov-94 08:49:37 GMT ; RFC 850, obsoleted by RFC 1036
	 *     Sun Nov  6 08:49:37 1994       ; ANSI C's asctime() format
	 *     2011-04-28T06:34:00+09:00      ; Atom time format
	 */
	bool read(StringView);

	int64_t geti() const;
	int64_t gmt_geti() const;
	int64_t ltz_geti() const;

	size_t encodeRfc822(char *) const;
	size_t encodeCTime(char *) const;
	size_t encodeIso8601(char *, size_t precision) const;
};

SPRT_API size_t strftime(char *buf, size_t maxsize, const char *format, uint64_t usec);

} // namespace sprt::time

#endif // CORE_RUNTIME_INCLUDE_SPRUNTIMETIME_H_
