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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_TIME_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_TIME_H_

#include <sprt/runtime/int.h>
#include <sprt/runtime/stringview.h>
#include <sprt/c/__sprt_time.h>

namespace sprt::time {

static constexpr uint64_t USEC_PER_SEC(1'000'000);

struct SPRT_API time_exp_t : public __SPRT_TM_NAME {
	static constexpr size_t Rfc822BufferSize = 32;
	static constexpr size_t CTimeBufferSize = 25;
	static constexpr size_t Iso8601BufferSize = 33; // with max 6 precision signs

	static constexpr auto gmt_unset = __SPRT_ID(gmt_unset);
	static constexpr auto gmt_local = __SPRT_ID(gmt_local);
	static constexpr auto gmt_set = __SPRT_ID(gmt_set);

	static time_exp_t get(bool localtime = false);

	time_exp_t();

	// Convert UTC time in microseconds to tm
	time_exp_t(int64_t t);

	// Convert UTC time in microseconds to tm in UTC or local timezone
	time_exp_t(int64_t t, bool use_localtime);

	// Convert UTC time in microseconds to tm in UTC or local timezone, then set GMT offet to value specified
	time_exp_t(int64_t t, int32_t offset_seconds, bool use_localtime);

	// Convert UTC time in microseconds to tm in UTC, then set GMT offet to value specified
	time_exp_t(int64_t t, int32_t offset_seconds);


	/*
	 * Parses an HTTP date in one of three standard forms:
	 *
	 *     Sun, 06 Nov 1994 08:49:37 GMT  ; RFC 822, updated by RFC 1123
	 *     Sunday, 06-Nov-94 08:49:37 GMT ; RFC 850, obsoleted by RFC 1036
	 *     Sun Nov  6 08:49:37 1994       ; ANSI C's asctime() format
	 *     2011-04-28T06:34:00+09:00      ; Atom time format
	 */
	time_exp_t(StringView);

	bool read(StringView);

	int64_t geti() const;
	int64_t gmt_geti() const;
	int64_t ltz_geti() const;

	size_t encodeRfc822(char *, size_t) const;
	size_t encodeCTime(char *, size_t) const;
	size_t encodeIso8601(char *, size_t, size_t precision) const;
};

SPRT_API size_t strftime(char *buf, size_t maxsize, const char *format, uint64_t usec);

} // namespace sprt::time

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_TIME_H_
