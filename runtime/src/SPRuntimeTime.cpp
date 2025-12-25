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

#include "SPRuntimeTime.h"
#include <c/__sprt_time.h>

#include <math.h>
#include <stdlib.h>
#include <time.h>

namespace sprt::time {

time_exp_t::time_exp_t() {
	tm_usec = 0;
	tm_sec = 0;
	tm_min = 0;
	tm_hour = 0;
	tm_mday = 0;
	tm_mon = 0;
	tm_year = 0;
	tm_wday = 0;
	tm_yday = 0;
	tm_isdst = 0;
	tm_gmtoff = 0;
}

time_exp_t::time_exp_t(int64_t t, int32_t offset, bool use_localtime) {
	struct __SPRT_TM_NAME tm;
	__sprt_time_t tt = __sprt_time_t(t / int64_t(USEC_PER_SEC));
	tm_usec = t % int64_t(USEC_PER_SEC);

	if (use_localtime) {
		__sprt_localtime_r(&tt, &tm);
		tm_gmt_type = gmt_local;
	} else {
		__sprt_gmtime_r(&tt, &tm);
		tm_gmt_type = gmt_set;
	}

	tm_sec = tm.tm_sec;
	tm_min = tm.tm_min;
	tm_hour = tm.tm_hour;
	tm_mday = tm.tm_mday;
	tm_mon = tm.tm_mon;
	tm_year = tm.tm_year;
	tm_wday = tm.tm_wday;
	tm_yday = tm.tm_yday;
	tm_isdst = tm.tm_isdst;
	tm_gmtoff = int32_t(tm.tm_gmtoff);
}

time_exp_t::time_exp_t(int64_t t, int32_t offs) : time_exp_t(t, offs, false) { tm_gmtoff = offs; }

time_exp_t::time_exp_t(int64_t t) : time_exp_t(t, 0, false) { tm_gmtoff = 0; }

time_exp_t::time_exp_t(int64_t t, bool use_localtime) : time_exp_t(t, 0, use_localtime) { }

int64_t time_exp_t::geti() const {
	auto year = tm_year;
	int64_t days = 0;
	static const int dayoffset[12] = {306, 337, 0, 31, 61, 92, 122, 153, 184, 214, 245, 275};

	/* shift new year to 1st March in order to make leap year calc easy */

	if (tm_mon < 2) {
		year--;
	}

	/* Find number of days since 1st March 1900 (in the Gregorian calendar). */

	days = year * 365 + year / 4 - year / 100 + (year / 100 + 3) / 4;
	days += dayoffset[tm_mon] + tm_mday - 1;
	days -= 25'508; /* 1 jan 1970 is 25508 days since 1 mar 1900 */

	return int64_t((((days * 24 + tm_hour) * 60 + tm_min) * 60 + tm_sec) * USEC_PER_SEC + tm_usec);
}

int64_t time_exp_t::gmt_geti() const { return int64_t(geti() - tm_gmtoff * USEC_PER_SEC); }

int64_t time_exp_t::ltz_geti() const {
	__sprt_time_t t = ::__sprt_time(nullptr);
	struct __SPRT_TM_NAME lt = {0};

	__sprt_localtime_r(&t, &lt);
	return int64_t(geti() - lt.tm_gmtoff * USEC_PER_SEC);
}

/*
 * Compare a string to a mask
 * Mask characters (arbitrary maximum is 256 characters, just in case):
 *   @ - uppercase letter
 *   $ - lowercase letter
 *   & - hex digit
 *   # - digit
 *   ~ - digit or space
 *   * - swallow remaining characters
 *  <x> - exact match for any other character
 */
static bool sp_date_checkmask(StringView data, StringView mask) {
	while (!mask.empty() && !data.empty()) {
		auto d = data.front();
		switch (mask.front()) {
		case '\0': return (d == '\0');

		case '*': return true;

		case '@':
			if (!isupper(d)) {
				return false;
			}
			break;
		case '$':
			if (!islower(d)) {
				return false;
			}
			break;
		case '#':
			if (!isdigit(d)) {
				return false;
			}
			break;
		case '&':
			if (!isxdigit(d)) {
				return false;
			}
			break;
		case '~':
			if ((d != ' ') && !isdigit(d)) {
				return false;
			}
			break;
		default:
			if (mask.at(0) != d) {
				return false;
			}
			break;
		}
		mask.offset(1);
		data.offset(1);
	}

	while (data.empty() && mask.is('*')) { mask.offset(1); }

	return mask.empty() && data.empty();
}

/*
 * Parses an HTTP date in one of three standard forms:
 *
 *     Sun, 06 Nov 1994 08:49:37 GMT  ; RFC 822, updated by RFC 1123
 *     Sunday, 06-Nov-94 08:49:37 GMT ; RFC 850, obsoleted by RFC 1036
 *     Sun Nov  6 08:49:37 1994       ; ANSI C's asctime() format
 *     2011-04-28T06:34:00+09:00      ; Atom time format
 *
 * and returns the apr_time_t number of microseconds since 1 Jan 1970 GMT,
 * or APR_DATE_BAD if this would be out of range or if the date is invalid.
 *
 * The restricted HTTP syntax is
 *
 *     HTTP-date    = rfc1123-date | rfc850-date | asctime-date
 *
 *     rfc1123-date = wkday "," SP date1 SP time SP "GMT"
 *     rfc850-date  = weekday "," SP date2 SP time SP "GMT"
 *     asctime-date = wkday SP date3 SP time SP 4DIGIT
 *
 *     date1        = 2DIGIT SP month SP 4DIGIT
 *                    ; day month year (e.g., 02 Jun 1982)
 *     date2        = 2DIGIT "-" month "-" 2DIGIT
 *                    ; day-month-year (e.g., 02-Jun-82)
 *     date3        = month SP ( 2DIGIT | ( SP 1DIGIT ))
 *                    ; month day (e.g., Jun  2)
 *
 *     time         = 2DIGIT ":" 2DIGIT ":" 2DIGIT
 *                    ; 00:00:00 - 23:59:59
 *
 *     wkday        = "Mon" | "Tue" | "Wed"
 *                  | "Thu" | "Fri" | "Sat" | "Sun"
 *
 *     weekday      = "Monday" | "Tuesday" | "Wednesday"
 *                  | "Thursday" | "Friday" | "Saturday" | "Sunday"
 *
 *     month        = "Jan" | "Feb" | "Mar" | "Apr"
 *                  | "May" | "Jun" | "Jul" | "Aug"
 *                  | "Sep" | "Oct" | "Nov" | "Dec"
 *
 * However, for the sake of robustness (and Netscapeness), we ignore the
 * weekday and anything after the time field (including the timezone).
 *
 * This routine is intended to be very fast; 10x faster than using sscanf.
 *
 * Originally from Andrew Daviel <andrew@vancouver-webpages.com>, 29 Jul 96
 * but many changes since then.
 *
 */

static const int s_months[12] = {('J' << 16) | ('a' << 8) | 'n', ('F' << 16) | ('e' << 8) | 'b',
	('M' << 16) | ('a' << 8) | 'r', ('A' << 16) | ('p' << 8) | 'r', ('M' << 16) | ('a' << 8) | 'y',
	('J' << 16) | ('u' << 8) | 'n', ('J' << 16) | ('u' << 8) | 'l', ('A' << 16) | ('u' << 8) | 'g',
	('S' << 16) | ('e' << 8) | 'p', ('O' << 16) | ('c' << 8) | 't', ('N' << 16) | ('o' << 8) | 'v',
	('D' << 16) | ('e' << 8) | 'c'};

static inline bool sp_time_exp_read_time(time_exp_t &ds, StringView timstr) {
	ds.tm_hour = ((timstr[0] - '0') * 10) + (timstr[1] - '0');
	ds.tm_min = ((timstr[3] - '0') * 10) + (timstr[4] - '0');
	ds.tm_sec = ((timstr[6] - '0') * 10) + (timstr[7] - '0');

	if ((ds.tm_hour > 23) || (ds.tm_min > 59) || (ds.tm_sec > 61)) {
		return false;
	}

	return true;
}

static inline bool sp_time_exp_check_mon(time_exp_t &ds) {
	if (ds.tm_mday <= 0 || ds.tm_mday > 31) {
		return false;
	}
	if (ds.tm_mon >= 12) {
		return false;
	}
	if ((ds.tm_mday == 31)
			&& (ds.tm_mon == 3 || ds.tm_mon == 5 || ds.tm_mon == 8 || ds.tm_mon == 10)) {
		return false;
	}

	if ((ds.tm_mon == 1)
			&& ((ds.tm_mday > 29)
					|| ((ds.tm_mday == 29)
							&& ((ds.tm_year & 3)
									|| (((ds.tm_year % 100) == 0)
											&& (((ds.tm_year % 400) != 100))))))) {
		return false;
	}
	return true;
}

static inline bool sp_time_exp_read_mon(time_exp_t &ds, StringView monstr) {
	int mon = 0;
	if (ds.tm_mday <= 0 || ds.tm_mday > 31) {
		return false;
	}

	if (monstr.size() >= 3) {
		auto mint = (monstr[0] << 16) | (monstr[1] << 8) | monstr[2];
		for (mon = 0; mon < 12; mon++) {
			if (mint == s_months[mon]) {
				break;
			}
		}
	} else {
		mon = ds.tm_mon - 1;
	}

	if (mon >= 12) {
		return false;
	}
	if ((ds.tm_mday == 31) && (mon == 3 || mon == 5 || mon == 8 || mon == 10)) {
		return false;
	}

	if ((mon == 1)
			&& ((ds.tm_mday > 29)
					|| ((ds.tm_mday == 29)
							&& ((ds.tm_year & 3)
									|| (((ds.tm_year % 100) == 0)
											&& (((ds.tm_year % 400) != 100))))))) {
		return false;
	}

	ds.tm_mon = mon;
	return true;
}

static StringView readNumbers(StringView &str) {
	auto ptr = str.data();
	size_t len = 0;
	while (isdigit(str.at(0))) {
		str.offset(1);
		++len;
	}
	return StringView(ptr, len);
}

static StringView readUntilNumbers(StringView &str) {
	auto ptr = str.data();
	size_t len = 0;
	while (!str.empty() && !isdigit(str.at(0))) {
		str.offset(1);
		++len;
	}
	return StringView(ptr, len);
}

static StringView readSpace(StringView &str) {
	auto ptr = str.data();
	size_t len = 0;
	while (isspace(str.at(0))) {
		str.offset(1);
		++len;
	}
	return StringView(ptr, len);
}

static long readNumber(StringView &str) {
	auto ptr = new char[str.size() + 1];
	::__sprt_memcpy(ptr, str.data(), str.size());
	ptr[str.size()] = 0;

	char *endptr = ptr;
	auto ret = ::strtol(ptr, &endptr, 10);

	str.offset(endptr - ptr);
	return ret;
}

static double readDouble(StringView &str) {
	auto ptr = new char[str.size() + 1];
	::__sprt_memcpy(ptr, str.data(), str.size());
	ptr[str.size()] = 0;

	char *endptr = ptr;
	auto d = ::strtod(ptr, &endptr);

	str.offset(endptr - ptr);
	return d;
}

static inline bool sp_time_exp_read_gmt(time_exp_t &ds, StringView gmtstr) {
	ds.tm_gmtoff = 0;
	/* Do we have a timezone ? */
	if (!gmtstr.empty()) {
		if (::__sprt_strncmp(gmtstr.data(), "GMT", gmtstr.size()) == 0) {
			ds.tm_gmt_type = time_exp_t::gmt_set;
			return true;
		}
		int sign = 0;
		switch (gmtstr.at(0)) {
		case '-': sign = -1; break;
		case '+': sign = 1; break;
		case 'Z': ds.tm_gmt_type = time_exp_t::gmt_set; break;
		default: break;
		}

		gmtstr.offset(1);

		auto off1 = readNumbers(gmtstr);
		if (off1.size() == 2 && gmtstr.is(':')) {
			gmtstr.offset(1);
			auto off2 = readNumbers(gmtstr);
			if (off2.size() == 2) {
				ds.tm_gmtoff += sign * readNumber(off1) * 60 * 60;
				ds.tm_gmtoff += sign * readNumber(off2) * 60;
				ds.tm_gmt_type = time_exp_t::gmt_set;
			}
		} else if (off1.size() == 4) {
			auto offset = readNumber(off1);
			ds.tm_gmtoff += sign * (offset / 100) * 60 * 60;
			ds.tm_gmtoff += sign * (offset % 100) * 60;
			ds.tm_gmt_type = time_exp_t::gmt_set;
		}
	} else {
		ds.tm_gmt_type = time_exp_t::gmt_local;
	}
	return true;
}

/*
 * Parses an HTTP date in one of three standard forms:
 *
 *     Sun, 06 Nov 1994 08:49:37 GMT  ; RFC 822, updated by RFC 1123
 *     Sunday, 06-Nov-94 08:49:37 GMT ; RFC 850, obsoleted by RFC 1036
 *     Sun Nov  6 08:49:37 1994       ; ANSI C's asctime() format
 *     2011-04-28T06:34:00+09:00      ; Atom time format
 */

bool time_exp_t::read(StringView r) {
	StringView monstr, timstr, gmtstr;

	readSpace(r);

	if (r.empty()) {
		return false;
	}

	auto tmp = r;
	while (!tmp.empty() && !tmp.is(' ')) { tmp.offset(1); }

	tm_gmt_type = gmt_unset;
	if (!tmp.is(' ')) {
		if (sp_date_checkmask(r, "####-##-##T##:##:##*")) {
			// 2011-04-28T06:34:00+09:00 ; Atom time format
			tm_year = ((r[0] - '0') * 10 + (r[1] - '0') - 19) * 100;
			if (tm_year < 0) {
				return false;
			}

			tm_year += ((r[2] - '0') * 10) + (r[3] - '0');
			tm_mon = ((r[5] - '0') * 10) + (r[6] - '0') - 1;
			tm_mday = ((r[8] - '0') * 10) + (r[9] - '0');

			r.offset(11);
			if (!sp_time_exp_read_time(*this, r.sub(0, 8))) {
				return false;
			}
			if (!sp_time_exp_check_mon(*this)) {
				return false;
			}
			r.offset(8);

			if (r.is('.')) {
				double v = readDouble(r);
				tm_usec = 1'000'000 * v;
			}
			return sp_time_exp_read_gmt(*this, r.empty() ? "Z" : r);
		} else if (sp_date_checkmask(r, "####-##-##*")) {
			// 2011-04-28 ; Atom date format
			tm_year = ((r[0] - '0') * 10 + (r[1] - '0') - 19) * 100;
			if (tm_year < 0) {
				return false;
			}

			tm_year += ((r[2] - '0') * 10) + (r[3] - '0');
			tm_mon = ((r[5] - '0') * 10) + (r[6] - '0') - 1;
			tm_mday = ((r[8] - '0') * 10) + (r[9] - '0');
			if (!sp_time_exp_check_mon(*this)) {
				return false;
			}
			r.offset(10);
			return sp_time_exp_read_gmt(*this, r.empty() ? "Z" : r);
		} else if (sp_date_checkmask(r, "##.##.####")) {
			// 12.03.2010
			tm_year = ((r[6] - '0') * 10 + (r[7] - '0') - 19) * 100;
			if (tm_year < 0) {
				return false;
			}

			tm_year += ((r[8] - '0') * 10) + (r[9] - '0');
			tm_mday = ((r[0] - '0') * 10) + (r[1] - '0');
			tm_mon = ((r[3] - '0') * 10) + (r[4] - '0') - 1;
			if (!sp_time_exp_check_mon(*this)) {
				return false;
			}
			return sp_time_exp_read_gmt(*this, StringView("Z"));
		}
		return false;
	}

	if (sp_date_checkmask(r, "@$$ @$$ ~# ##:##:## *")) {
		// Sun Nov  6 08:49:37 1994       ; ANSI C's asctime() format
		auto ydate = r.sub(20); // StringView(r.data() + 20, r.size() - 20);
		readUntilNumbers(ydate);
		if (ydate.size() < 4) {
			return false;
		}

		tm_year = ((ydate[0] - '0') * 10 + (ydate[1] - '0') - 19) * 100;
		if (tm_year < 0) {
			return false;
		}
		tm_year += ((ydate[2] - '0') * 10) + (ydate[3] - '0');
		tm_mday = (r[8] == ' ') ? (r[9] - '0') : (((r[8] - '0') * 10) + (r[9] - '0'));

		monstr = r.sub(4, 3);
		timstr = r.sub(11, 8);

		if (!sp_time_exp_read_time(*this, timstr)) {
			return false;
		}
		if (!sp_time_exp_read_mon(*this, monstr)) {
			return false;
		}

		tm_usec = 0;
		tm_gmtoff = 0;
		tm_gmt_type = gmt_local;
		return true;
	}

	readUntilNumbers(r);

	if (sp_date_checkmask(r, "## @$$ #### ##:##:## *")) {
		// Sun, 06 Nov 1994 08:49:37 GMT  ; RFC 822, updated by RFC 1123
		tm_year = ((r[7] - '0') * 10 + (r[8] - '0') - 19) * 100;
		if (tm_year < 0) {
			return false;
		}

		tm_year += ((r[9] - '0') * 10) + (r[10] - '0');
		tm_mday = ((r[0] - '0') * 10) + (r[1] - '0');

		monstr = r.sub(3, 3);
		timstr = r.sub(12, 8);
		gmtstr = r.sub(21);
	} else if (sp_date_checkmask(r, "# @$$ #### ##:##:## *")) {
		/* RFC 1123 format with one day */
		tm_year = ((r[6] - '0') * 10 + (r[7] - '0') - 19) * 100;
		if (tm_year < 0) {
			return false;
		}

		tm_year += ((r[8] - '0') * 10) + (r[9] - '0');
		tm_mday = (r[0] - '0');

		monstr = r.sub(2, 3);
		timstr = r.sub(11, 8);
		gmtstr = r.sub(20);
	} else if (sp_date_checkmask(r, "##-@$$-## ##:##:## *")) {
		// Sunday, 06-Nov-94 08:49:37 GMT ; RFC 850, obsoleted by RFC 1036
		tm_year = ((r[7] - '0') * 10) + (r[8] - '0');
		if (tm_year < 70) {
			tm_year += 100;
		}

		tm_mday = ((r[0] - '0') * 10) + (r[1] - '0');

		monstr = r.sub(3, 3);
		timstr = r.sub(10, 8);
		gmtstr = r.sub(19);
	} else {
		return false;
	}

	if (!sp_time_exp_read_time(*this, timstr)) {
		return false;
	}
	if (!sp_time_exp_read_mon(*this, monstr)) {
		return false;
	}

	tm_usec = 0;

	if (!gmtstr.empty()) {
		if (!sp_time_exp_read_gmt(*this, gmtstr)) {
			return false;
		}
	} else {
		tm_gmtoff = 0;
	}

	return true;
}

static const char sp_month_snames[12][4] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug",
	"Sep", "Oct", "Nov", "Dec"};
static const char sp_day_snames[7][4] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

size_t time_exp_t::encodeRfc822(char *date_str) const {
	auto start = date_str;
	const char *s;
	int real_year;

	/* example: "Sat, 08 Jan 2000 18:31:41 GMT" */
	/*           12345678901234567890123456789  */

	s = &sp_day_snames[tm_wday][0];
	*date_str++ = *s++;
	*date_str++ = *s++;
	*date_str++ = *s++;
	*date_str++ = ',';
	*date_str++ = ' ';
	*date_str++ = tm_mday / 10 + '0';
	*date_str++ = tm_mday % 10 + '0';
	*date_str++ = ' ';
	s = &sp_month_snames[tm_mon][0];
	*date_str++ = *s++;
	*date_str++ = *s++;
	*date_str++ = *s++;
	*date_str++ = ' ';
	real_year = 1'900 + tm_year;
	/* This routine isn't y10k ready. */
	*date_str++ = real_year / 1'000 + '0';
	*date_str++ = real_year % 1'000 / 100 + '0';
	*date_str++ = real_year % 100 / 10 + '0';
	*date_str++ = real_year % 10 + '0';
	*date_str++ = ' ';
	*date_str++ = tm_hour / 10 + '0';
	*date_str++ = tm_hour % 10 + '0';
	*date_str++ = ':';
	*date_str++ = tm_min / 10 + '0';
	*date_str++ = tm_min % 10 + '0';
	*date_str++ = ':';
	*date_str++ = tm_sec / 10 + '0';
	*date_str++ = tm_sec % 10 + '0';
	*date_str++ = ' ';
	*date_str++ = 'G';
	*date_str++ = 'M';
	*date_str++ = 'T';
	*date_str++ = 0;
	return date_str - start - 1;
}

size_t time_exp_t::encodeCTime(char *date_str) const {
	auto start = date_str;
	const char *s;
	int real_year;

	/* example: "Wed Jun 30 21:49:08 1993" */
	/*           123456789012345678901234  */

	s = &sp_day_snames[tm_wday][0];
	*date_str++ = *s++;
	*date_str++ = *s++;
	*date_str++ = *s++;
	*date_str++ = ' ';
	s = &sp_month_snames[tm_mon][0];
	*date_str++ = *s++;
	*date_str++ = *s++;
	*date_str++ = *s++;
	*date_str++ = ' ';
	*date_str++ = tm_mday / 10 + '0';
	*date_str++ = tm_mday % 10 + '0';
	*date_str++ = ' ';
	*date_str++ = tm_hour / 10 + '0';
	*date_str++ = tm_hour % 10 + '0';
	*date_str++ = ':';
	*date_str++ = tm_min / 10 + '0';
	*date_str++ = tm_min % 10 + '0';
	*date_str++ = ':';
	*date_str++ = tm_sec / 10 + '0';
	*date_str++ = tm_sec % 10 + '0';
	*date_str++ = ' ';
	real_year = 1'900 + tm_year;
	*date_str++ = real_year / 1'000 + '0';
	*date_str++ = real_year % 1'000 / 100 + '0';
	*date_str++ = real_year % 100 / 10 + '0';
	*date_str++ = real_year % 10 + '0';
	*date_str++ = 0;
	return date_str - start - 1;
}

size_t time_exp_t::encodeIso8601(char *date_str, size_t precision) const {
	auto start = date_str;
	int real_year;

	real_year = 1'900 + tm_year;
	*date_str++ = real_year / 1'000 + '0'; // 1
	*date_str++ = real_year % 1'000 / 100 + '0'; // 2
	*date_str++ = real_year % 100 / 10 + '0'; // 3
	*date_str++ = real_year % 10 + '0'; // 4
	*date_str++ = '-'; // 5
	*date_str++ = (tm_mon + 1) / 10 + '0'; // 6
	*date_str++ = (tm_mon + 1) % 10 + '0'; // 7
	*date_str++ = '-'; // 8
	*date_str++ = tm_mday / 10 + '0'; // 9
	*date_str++ = tm_mday % 10 + '0'; // 10
	*date_str++ = 'T'; // 11
	*date_str++ = tm_hour / 10 + '0'; // 12
	*date_str++ = tm_hour % 10 + '0'; // 13
	*date_str++ = ':'; // 14
	*date_str++ = tm_min / 10 + '0'; // 15
	*date_str++ = tm_min % 10 + '0'; // 16
	*date_str++ = ':'; // 17
	*date_str++ = tm_sec / 10 + '0'; // 18
	*date_str++ = tm_sec % 10 + '0'; // 19

	if (precision > 0 && precision <= 6) {
		auto intpow = [](int val, int p) {
			int ret = 1;
			while (p > 0) {
				ret *= val;
				--p;
			}
			return ret;
		};

		*date_str++ = '.';
		const int desc = USEC_PER_SEC / intpow(10, int(precision));
		auto val = int32_t(::round(tm_usec / double(desc)));
		while (precision > 0) {
			auto d = val / intpow(10, int(precision - 1));
			*date_str++ = '0' + d;
			val = val % intpow(10, int(precision - 1));
			--precision;
		}
	}

	*date_str++ = 'Z'; // 20
	*date_str++ = 0;
	return date_str - start - 1;
}

size_t strftime(char *buf, size_t bufSize, const char *format, uint64_t usec) {
	struct __SPRT_TM_NAME tm;
	__sprt_time_t tt = usec / USEC_PER_SEC;
	__sprt_gmtime_r(&tt, &tm);
	return __sprt_strftime(buf, bufSize, format, &tm);
}

} // namespace sprt::time
