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

#include <sprt/runtime/utils/time.h>
#include <sprt/runtime/platform.h>
#include <sprt/c/__sprt_time.h>

extern "C" double round(double) __SPRT_NOEXCEPT;

namespace sprt::time {

time_exp_t time_exp_t::get(bool localtime) {
	time_exp_t ret;
	auto clock = __sprt_clock_gettime_nsec_np(__SPRT_CLOCK_REALTIME) / 1'000;
	time_t time = clock / __USEC_PER_SEC;
	auto usec = clock % __USEC_PER_SEC;

	if (localtime) {
		__sprt_localtime_r(&time, &ret);
	} else {
		__sprt_gmtime_r(&time, &ret);
	}

	ret.tm_usec = usec;
	return ret;
}

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

time_exp_t::time_exp_t(const __SPRT_TM_NAME &s) : __SPRT_TM_NAME(s) { }

time_exp_t::time_exp_t(int64_t t, int32_t offset, bool use_localtime)
: time_exp_t(t, use_localtime) {
	tm_gmtoff = offset;
	tm_gmt_type = (offset == 0) ? gmt_set : gmt_local;
}

time_exp_t::time_exp_t(int64_t t, int32_t offs) : time_exp_t(t, offs, false) { }

time_exp_t::time_exp_t(int64_t t) : time_exp_t(t, false) { }

time_exp_t::time_exp_t(int64_t t, bool use_localtime) {
	__sprt_time_t tt = __sprt_time_t(t / int64_t(__USEC_PER_SEC));

	if (use_localtime) {
		__sprt_localtime_r(&tt, this);
	} else {
		__sprt_gmtime_r(&tt, this);
	}
	tm_usec = t % int64_t(__USEC_PER_SEC);
}

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

	return int64_t(
			(((days * 24 + tm_hour) * 60 + tm_min) * 60 + tm_sec) * __USEC_PER_SEC + tm_usec);
}

int64_t time_exp_t::gmt_geti() const {
	auto ret = int64_t(geti());
	if (tm_gmt_type != __sprt_gmt_set) {
		return ret - int64_t(tm_gmtoff) * __USEC_PER_SEC;
	}
	return ret;
}

int64_t time_exp_t::ltz_geti() const {
	if (tm_gmt_type != __sprt_gmt_set) {
		return int64_t(geti());
	} else {
		__sprt_time_t t = ::__sprt_time(nullptr);
		struct __SPRT_TM_NAME lt = {0};

		__sprt_localtime_r(&t, &lt);
		return int64_t(geti() - lt.tm_gmtoff * __USEC_PER_SEC);
	}
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
			if (!chars::isupper(d)) {
				return false;
			}
			break;
		case '$':
			if (!chars::islower(d)) {
				return false;
			}
			break;
		case '#':
			if (!chars::isdigit(d)) {
				return false;
			}
			break;
		case '&':
			if (!chars::isxdigit(d)) {
				return false;
			}
			break;
		case '~':
			if ((d != ' ') && !chars::isdigit(d)) {
				return false;
			}
			break;
		default:
			if (*mask != d) {
				return false;
			}
			break;
		}
		++mask;
		++data;
	}

	while (data.empty() && mask.is('*')) { ++mask; }

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

static inline bool sp_time_exp_read_gmt(time_exp_t &ds, StringView gmtstr) {
	ds.tm_gmtoff = 0;
	ds.tm_gmt_type = time_exp_t::gmt_unset;

	/* Do we have a timezone ? */
	if (!gmtstr.empty()) {
		if (gmtstr == "GMT") {
			ds.tm_gmt_type = time_exp_t::gmt_set;
			return true;
		}
		int sign = 0;
		switch (*gmtstr) {
		case '-': sign = -1; break;
		case '+': sign = 1; break;
		case 'Z': ds.tm_gmt_type = time_exp_t::gmt_set; break;
		default: break;
		}


		++gmtstr;
		auto off1 = gmtstr.readChars<StringView::CharGroup<CharGroupId::Numbers>>();
		if (off1.size() == 2 && gmtstr.is(':')) {
			++gmtstr;
			auto off2 = gmtstr.readChars<StringView::CharGroup<CharGroupId::Numbers>>();
			if (off2.size() == 2) {
				ds.tm_gmtoff += sign * off1.readInteger(10).get() * 60 * 60;
				ds.tm_gmtoff += sign * off2.readInteger(10).get() * 60;
				ds.tm_gmt_type = (ds.tm_gmtoff != 0) ? time_exp_t::gmt_local : time_exp_t::gmt_set;
			}
		} else if (off1.size() == 4) {
			auto offset = off1.readInteger().get();
			ds.tm_gmtoff += sign * (offset / 100) * 60 * 60;
			ds.tm_gmtoff += sign * (offset % 100) * 60;
			ds.tm_gmt_type = (ds.tm_gmtoff != 0) ? time_exp_t::gmt_local : time_exp_t::gmt_set;
		}
	}
	return true;
}


time_exp_t::time_exp_t(StringView str) { read(str); }
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

	r.skipChars<StringView::CharGroup<CharGroupId::WhiteSpace>>();

	if (r.empty()) {
		return false;
	}

	auto tmp = r;
	tmp.skipUntil<StringView::Chars<' '>>();

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

			r += 11;
			if (!sp_time_exp_read_time(*this, r.sub(0, 8))) {
				return false;
			}
			if (!sp_time_exp_check_mon(*this)) {
				return false;
			}
			r += 8;

			if (r.is('.')) {
				double v = r.readDouble().get();
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
			r += "####-##-##"_len;
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
		ydate.skipUntil<StringView::CharGroup<CharGroupId::Numbers>>();
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
		return true;
	}

	r.skipUntil<StringView::CharGroup<CharGroupId::Numbers>>();

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

static const char *sp_month_Bnames[] = {"January", "February", "March", "April", "May", "June",
	"July", "August", "September", "October", "November", "December"};

static const char sp_day_snames[7][4] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

static const char *sp_day_anames[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday",
	"Friday", "Saturday"};

// Sun Sep 16 01:03:52 1973\n\0
size_t time_exp_t::asctime(char *date_str, size_t bufSize) const {
	auto start = date_str;
	const char *s;
	int real_year;

	auto push = [&](char c) {
		if (bufSize > 0) {
			*date_str = c;
			++date_str;
			--bufSize;
		}
	};

	s = &sp_day_snames[tm_wday][0];
	push(*s++);
	push(*s++);
	push(*s++);
	push(' ');
	s = &sp_month_snames[tm_mon][0];
	push(*s++);
	push(*s++);
	push(*s++);
	push(' ');
	push(tm_mday / 10 + '0');
	push(tm_mday % 10 + '0');
	push(' ');
	push(tm_hour / 10 + '0');
	push(tm_hour % 10 + '0');
	push(':');
	push(tm_min / 10 + '0');
	push(tm_min % 10 + '0');
	push(':');
	push(tm_sec / 10 + '0');
	push(tm_sec % 10 + '0');
	push(' ');
	real_year = 1'900 + tm_year;
	/* This routine isn't y10k ready. */
	push(real_year / 1'000 + '0');
	push(real_year % 1'000 / 100 + '0');
	push(real_year % 100 / 10 + '0');
	push(real_year % 10 + '0');
	push('\n');
	push(0);
	return date_str - start - 1;
}

size_t time_exp_t::encodeRfc822(char *date_str, size_t bufSize) const {
	auto start = date_str;
	const char *s;
	int real_year;

	auto push = [&](char c) {
		if (bufSize > 0) {
			*date_str = c;
			++date_str;
			--bufSize;
		}
	};

	/* example: "Sat, 08 Jan 2000 18:31:41 GMT" */
	/*           12345678901234567890123456789  */

	s = &sp_day_snames[tm_wday][0];
	push(*s++);
	push(*s++);
	push(*s++);
	push(',');
	push(' ');
	push(tm_mday / 10 + '0');
	push(tm_mday % 10 + '0');
	push(' ');
	s = &sp_month_snames[tm_mon][0];
	push(*s++);
	push(*s++);
	push(*s++);
	push(' ');
	real_year = 1'900 + tm_year;
	/* This routine isn't y10k ready. */
	push(real_year / 1'000 + '0');
	push(real_year % 1'000 / 100 + '0');
	push(real_year % 100 / 10 + '0');
	push(real_year % 10 + '0');
	push(' ');
	push(tm_hour / 10 + '0');
	push(tm_hour % 10 + '0');
	push(':');
	push(tm_min / 10 + '0');
	push(tm_min % 10 + '0');
	push(':');
	push(tm_sec / 10 + '0');
	push(tm_sec % 10 + '0');
	push(' ');
	if (tm_gmtoff == 0) {
		push('G');
		push('M');
		push('T');
	} else {
		push((tm_gmtoff > 0) ? '+' : '-');
		auto gmtHours = tm_gmtoff / (60 * 60);
		auto gmtMins = (tm_gmtoff % (60 * 60)) / 60;

		push(gmtHours / 10 + '0');
		push(gmtHours % 10 + '0');
		push(gmtMins / 10 + '0');
		push(gmtMins % 10 + '0');
	}
	push(0);
	return date_str - start - 1;
}

size_t time_exp_t::encodeCTime(char *date_str, size_t bufSize) const {
	auto start = date_str;
	const char *s;
	int real_year;

	/* example: "Wed Jun 30 21:49:08 1993" */
	/*           123456789012345678901234  */

	auto push = [&](char c) {
		if (bufSize > 0) {
			*date_str = c;
			++date_str;
			--bufSize;
		}
	};

	s = &sp_day_snames[tm_wday][0];
	push(*s++);
	push(*s++);
	push(*s++);
	push(' ');
	s = &sp_month_snames[tm_mon][0];
	push(*s++);
	push(*s++);
	push(*s++);
	push(' ');
	push(tm_mday / 10 + '0');
	push(tm_mday % 10 + '0');
	push(' ');
	push(tm_hour / 10 + '0');
	push(tm_hour % 10 + '0');
	push(':');
	push(tm_min / 10 + '0');
	push(tm_min % 10 + '0');
	push(':');
	push(tm_sec / 10 + '0');
	push(tm_sec % 10 + '0');
	push(' ');
	real_year = 1'900 + tm_year;
	push(real_year / 1'000 + '0');
	push(real_year % 1'000 / 100 + '0');
	push(real_year % 100 / 10 + '0');
	push(real_year % 10 + '0');
	push(0);
	return date_str - start - 1;
}

size_t time_exp_t::encodeIso8601(char *date_str, size_t bufSize, size_t precision) const {
	auto start = date_str;
	int real_year;

	auto push = [&](char c) {
		if (bufSize > 0) {
			*date_str = c;
			++date_str;
			--bufSize;
		}
	};

	real_year = 1'900 + tm_year;
	push(real_year / 1'000 + '0'); // 1
	push(real_year % 1'000 / 100 + '0'); // 2
	push(real_year % 100 / 10 + '0'); // 3
	push(real_year % 10 + '0'); // 4
	push('-'); // 5
	push((tm_mon + 1) / 10 + '0'); // 6
	push((tm_mon + 1) % 10 + '0'); // 7
	push('-'); // 8
	push(tm_mday / 10 + '0'); // 9
	push(tm_mday % 10 + '0'); // 10
	push('T'); // 11
	push(tm_hour / 10 + '0'); // 12
	push(tm_hour % 10 + '0'); // 13
	push(':'); // 14
	push(tm_min / 10 + '0'); // 15
	push(tm_min % 10 + '0'); // 16
	push(':'); // 17
	push(tm_sec / 10 + '0'); // 18
	push(tm_sec % 10 + '0'); // 19

	if (precision > 0 && precision <= 6) {
		auto intpow = [](int val, int p) {
			int ret = 1;
			while (p > 0) {
				ret *= val;
				--p;
			}
			return ret;
		};

		push('.');
		const int desc = __USEC_PER_SEC / intpow(10, int(precision));
		auto val = int32_t(::round(tm_usec / double(desc)));
		while (precision > 0) {
			auto d = val / intpow(10, int(precision - 1));
			push('0' + d);
			val = val % intpow(10, int(precision - 1));
			--precision;
		}
	}

	if (tm_gmtoff != 0) {
		push((tm_gmtoff > 0) ? '+' : '-');
		auto gmtHours = tm_gmtoff / (60 * 60);
		auto gmtMins = (tm_gmtoff % (60 * 60)) / 60;

		push(gmtHours / 10 + '0');
		push(gmtHours % 10 + '0');
		push(':');
		push(gmtMins / 10 + '0');
		push(gmtMins % 10 + '0');
	} else {
		push('Z'); // 20
	}
	push(0);
	return date_str - start - 1;
}

static int __month_to_secs(int month, int is_leap) {
	static const int secs_through_month[] = {0, 31 * 86'400, 59 * 86'400, 90 * 86'400, 120 * 86'400,
		151 * 86'400, 181 * 86'400, 212 * 86'400, 243 * 86'400, 273 * 86'400, 304 * 86'400,
		334 * 86'400};
	int t = secs_through_month[month];
	if (is_leap && month >= 2) {
		t += 86'400;
	}
	return t;
}

static long long __year_to_secs(long long year, int *is_leap) {
	if (year - 2ULL <= 136) {
		int y = year;
		int leaps = (y - 68) >> 2;
		if (!((y - 68) & 3)) {
			leaps--;
			if (is_leap) {
				*is_leap = 1;
			}
		} else if (is_leap) {
			*is_leap = 0;
		}
		return 31'536'000 * (y - 70) + 86'400 * leaps;
	}

	int cycles, centuries, leaps, rem, dummy;

	if (!is_leap) {
		is_leap = &dummy;
	}
	cycles = (year - 100) / 400;
	rem = (year - 100) % 400;
	if (rem < 0) {
		cycles--;
		rem += 400;
	}
	if (!rem) {
		*is_leap = 1;
		centuries = 0;
		leaps = 0;
	} else {
		if (rem >= 200) {
			if (rem >= 300) {
				centuries = 3, rem -= 300;
			} else {
				centuries = 2, rem -= 200;
			}
		} else {
			if (rem >= 100) {
				centuries = 1, rem -= 100;
			} else {
				centuries = 0;
			}
		}
		if (!rem) {
			*is_leap = 0;
			leaps = 0;
		} else {
			leaps = rem / 4U;
			rem %= 4U;
			*is_leap = !rem;
		}
	}

	leaps += 97 * cycles + 24 * centuries - *is_leap;

	return (year - 100) * 31'536'000LL + leaps * 86'400LL + 946'684'800 + 86'400;
}

static long long __tm_to_secs(const struct tm *tm) {
	int is_leap;
	long long year = tm->tm_year;
	int month = tm->tm_mon;
	if (month >= 12 || month < 0) {
		int adj = month / 12;
		month %= 12;
		if (month < 0) {
			adj--;
			month += 12;
		}
		year += adj;
	}
	long long t = __year_to_secs(year, &is_leap);
	t += __month_to_secs(month, is_leap);
	t += 86'400LL * (tm->tm_mday - 1);
	t += 3'600LL * tm->tm_hour;
	t += 60LL * tm->tm_min;
	t += tm->tm_sec;
	return t;
}

static int is_leap(int y) {
	/* Avoid overflow */
	if (y > __SPRT_INT_MAX - 1'900) {
		y -= 2'000;
	}
	y += 1'900;
	return !(y % 4) && ((y % 100) || !(y % 400));
}

static int week_num(const struct tm *tm) {
	int val = (tm->tm_yday + 7U - (tm->tm_wday + 6U) % 7) / 7;
	/* If 1 Jan is just 1-3 days past Monday,
	 * the previous week is also in this year. */
	if ((tm->tm_wday + 371U - tm->tm_yday - 2) % 7 <= 2) {
		val++;
	}
	if (!val) {
		val = 52;
		/* If 31 December of prev year a Thursday,
		 * or Friday of a leap year, then the
		 * prev year has 53 weeks. */
		int dec31 = (tm->tm_wday + 7U - tm->tm_yday - 1) % 7;
		if (dec31 == 4 || (dec31 == 5 && is_leap(tm->tm_year % 400 - 1))) {
			val++;
		}
	} else if (val == 53) {
		/* If 1 January is not a Thursday, and not
		 * a Wednesday of a leap year, then this
		 * year has only 52 weeks. */
		int jan1 = (tm->tm_wday + 371U - tm->tm_yday) % 7;
		if (jan1 != 4 && (jan1 != 3 || !is_leap(tm->tm_year))) {
			val = 1;
		}
	}
	return val;
}

static const char *__strftime_fmt_1(char (*s)[100], size_t *l, int f, const time_exp_t *tm,
		int pad) {
	long long val;
	const char *item = nullptr;
	const char *fmt = "-";
	int width = 2, def_pad = '0';

	switch (f) {
	case 'a':
		if (tm->tm_wday > 6U) {
			goto string;
		}
		item = sp_day_snames[tm->tm_wday];
		goto nl_strcat;
	case 'A':
		if (tm->tm_wday > 6U) {
			goto string;
		}
		item = sp_day_anames[tm->tm_wday];
		goto nl_strcat;
	case 'h':
	case 'b':
		if (tm->tm_mon > 11U) {
			goto string;
		}
		item = sp_month_snames[tm->tm_mon];
		goto nl_strcat;
	case 'B':
		if (tm->tm_mon > 11U) {
			goto string;
		}
		item = sp_month_Bnames[tm->tm_mon];
		goto nl_strcat;
	case 'c': fmt = "%a %b %e %H:%M:%S %Y"; goto recu_strftime;
	case 'C': val = (1'900LL + tm->tm_year) / 100; goto number;
	case 'e': def_pad = '_';
	case 'd': val = tm->tm_mday; goto number;
	case 'D': fmt = "%m/%d/%y"; goto recu_strftime;
	case 'F': fmt = "%Y-%m-%d"; goto recu_strftime;
	case 'g':
	case 'G':
		val = tm->tm_year + 1'900LL;
		if (tm->tm_yday < 3 && week_num(tm) != 1) {
			val--;
		} else if (tm->tm_yday > 360 && week_num(tm) == 1) {
			val++;
		}
		if (f == 'g') {
			val %= 100;
		} else {
			width = 4;
		}
		goto number;
	case 'H': val = tm->tm_hour; goto number;
	case 'I':
		val = tm->tm_hour;
		if (!val) {
			val = 12;
		} else if (val > 12) {
			val -= 12;
		}
		goto number;
	case 'j':
		val = tm->tm_yday + 1;
		width = 3;
		goto number;
	case 'm': val = tm->tm_mon + 1; goto number;
	case 'M': val = tm->tm_min; goto number;
	case 'n': *l = 1; return "\n";
	case 'p': item = tm->tm_hour >= 12 ? "PM" : "AM"; goto nl_strcat;
	case 'r': fmt = "%I:%M:%S %p"; goto recu_strftime;
	case 'R': fmt = "%H:%M"; goto recu_strftime;
	case 's':
		val = __tm_to_secs(tm) - tm->tm_gmtoff;
		width = 1;
		goto number;
	case 'S': val = tm->tm_sec; goto number;
	case 't': *l = 1; return "\t";
	case 'T': fmt = "%H:%M:%S"; goto recu_strftime;
	case 'u':
		val = tm->tm_wday ? tm->tm_wday : 7;
		width = 1;
		goto number;
	case 'U': val = (tm->tm_yday + 7U - tm->tm_wday) / 7; goto number;
	case 'W': val = (tm->tm_yday + 7U - (tm->tm_wday + 6U) % 7) / 7; goto number;
	case 'V': val = week_num(tm); goto number;
	case 'w':
		val = tm->tm_wday;
		width = 1;
		goto number;
	case 'x': fmt = "%m/%d/%y"; goto recu_strftime;
	case 'X': fmt = "%H:%M:%S"; goto recu_strftime;
	case 'y':
		val = (tm->tm_year + 1'900LL) % 100;
		if (val < 0) {
			val = -val;
		}
		goto number;
	case 'Y':
		val = tm->tm_year + 1'900LL;
		if (val >= 10'000) {
			*l = __sprt_snprintf(*s, sizeof *s, "+%lld", val);
			return *s;
		}
		width = 4;
		goto number;
	case 'Z':
		if (tm->tm_isdst < 0) {
			*l = 0;
			return "";
		}
		if (tm->tm_gmtoff == 0) {
			fmt = "UTC";
			goto string;
		}
		[[fallthrough]];
	case 'z':
		if (tm->tm_isdst < 0) {
			*l = 0;
			return "";
		}
		*l = __sprt_snprintf(*s, sizeof *s, "%+.4ld",
				tm->tm_gmtoff / 3'600 * 100 + tm->tm_gmtoff % 3'600 / 60);
		return *s;
	case '%': *l = 1; return "%";
	default: return 0;
	}
number:
	switch (pad ? pad : def_pad) {
	case '-': *l = __sprt_snprintf(*s, sizeof *s, "%lld", val); break;
	case '_': *l = __sprt_snprintf(*s, sizeof *s, "%*lld", width, val); break;
	case '0':
	default: *l = __sprt_snprintf(*s, sizeof *s, "%0*lld", width, val); break;
	}
	return *s;
nl_strcat:
	fmt = item;
string:
	*l = strlen(fmt);
	return fmt;
recu_strftime:
	*l = tm->strftime(*s, sizeof *s, fmt);
	if (!*l) {
		return 0;
	}
	return *s;
}

size_t time_exp_t::strftime(char *s, size_t n, const char *f) const {
	size_t l, k;
	char buf[100];
	char *p;
	const char *t;
	int pad, plus;
	unsigned long width;
	for (l = 0; l < n; f++) {
		if (!*f) {
			s[l] = 0;
			return l;
		}
		if (*f != '%') {
			s[l++] = *f;
			continue;
		}
		f++;
		pad = 0;
		if (*f == '-' || *f == '_' || *f == '0') {
			pad = *f++;
		}
		if ((plus = (*f == '+'))) {
			f++;
		}
		if (__sprt_isdigit(*f)) {
			width = __sprt_strtoul(f, &p, 10);
		} else {
			width = 0;
			p = (char *)f;
		}
		if (*p == 'C' || *p == 'F' || *p == 'G' || *p == 'Y') {
			if (!width && p != f) {
				width = 1;
			}
		} else {
			width = 0;
		}
		f = p;
		if (*f == 'E' || *f == 'O') {
			f++;
		}
		t = __strftime_fmt_1(&buf, &k, *f, this, pad);
		if (!t) {
			break;
		}
		if (width) {
			/* Trim off any sign and leading zeros, then
			 * count remaining digits to determine behavior
			 * for the + flag. */
			if (*t == '+' || *t == '-') {
				t++, k--;
			}
			for (; *t == '0' && t[1] - '0' < 10U; t++, k--);
			if (width < k) {
				width = k;
			}
			size_t d;
			for (d = 0; t[d] - '0' < 10U; d++);
			if (tm_year < -1'900) {
				s[l++] = '-';
				width--;
			} else if (plus && d + (width - k) >= (*p == 'C' ? 3 : 5)) {
				s[l++] = '+';
				width--;
			}
			for (; width > k && l < n; width--) { s[l++] = '0'; }
		}
		if (k > n - l) {
			k = n - l;
		}
		memcpy(s + l, t, k);
		l += k;
	}
	if (n) {
		if (l == n) {
			l = n - 1;
		}
		s[l] = 0;
	}
	return 0;
}

size_t strftime(char *buf, size_t bufSize, const char *fmt, uint64_t usec) {
	time_exp_t exp(usec);
	return exp.strftime(buf, bufSize, fmt);
}

TimeInterval TimeInterval::Infinite(Max<uint64_t>);

TimeInterval TimeInterval::between(const Time &v1, const Time &v2) {
	if (v1 > v2) {
		return TimeInterval(v1._value - v2._value);
	} else {
		return TimeInterval(v2._value - v1._value);
	}
}

uint64_t TimeStorage::toMicroseconds() const { return _value; }
uint64_t TimeStorage::toMilliseconds() const { return _value / 1'000ULL; }
uint64_t TimeStorage::toSeconds() const { return _value / 1'000'000ULL; }
float TimeStorage::toFloatSeconds() const { return _value / 1000000.0f; }
double TimeStorage::toDoubleSeconds() const { return _value / 1000000.0; }

__SPRT_TM_NAME TimeStorage::asLocal() const {
	auto sec = sprt::time_t(toSeconds());
	tm tm;
	__sprt_localtime_r(&sec, &tm);
	return tm;
}

__SPRT_TM_NAME TimeStorage::asGmt() const {
	auto sec = sprt::time_t(toSeconds());
	tm tm;
	__sprt_gmtime_r(&sec, &tm);
	return tm;
}

void TimeStorage::setMicroseconds(uint64_t v) { _value = v; }
void TimeStorage::setMilliseconds(uint64_t v) { _value = v * 1'000ULL; }
void TimeStorage::setSeconds(sprt::time_t v) { _value = v * 1'000'000ULL; }

void TimeStorage::clear() { _value = 0; }

TimeInterval::TimeInterval(nullptr_t) { _value = 0; }
TimeInterval &TimeInterval::operator=(nullptr_t) {
	_value = 0;
	return *this;
}


Time Time::now() { return Time(__sprt_clock_gettime_nsec_np(__SPRT_CLOCK_REALTIME) / 1'000); }

Time Time::microseconds(uint64_t mksec) { return Time(mksec); }
Time Time::milliseconds(uint64_t msec) { return Time(msec * 1'000ULL); }
Time Time::seconds(time_t sec) { return Time(sec * 1'000'000ULL); }
Time Time::floatSeconds(float sec) { return Time(uint64_t(sec * 1000000.0f)); }

Time::Time(nullptr_t) : TimeStorage(0) { }
Time &Time::operator=(nullptr_t) {
	_value = 0;
	return *this;
}

Time Time::fromCompileTime(const char *date, const char *time) {
	sprt::time::time_exp_t ds;
	ds.tm_year = ((date[7] - '0') * 10 + (date[8] - '0') - 19) * 100;
	if (ds.tm_year < 0) {
		return Time();
	}

	ds.tm_year += ((date[9] - '0') * 10) + (date[10] - '0');
	ds.tm_mday = ((date[4] != ' ') ? ((date[4] - '0') * 10) : 0) + (date[5] - '0');

	int mint = (date[0] << 16) | (date[1] << 8) | date[2];
	int mon = 0;
	for (; mon < 12; mon++) {
		if (mint == s_months[mon]) {
			break;
		}
	}

	if (mon == 12) {
		return Time();
	}

	if (ds.tm_mday <= 0 || ds.tm_mday > 31) {
		return Time();
	}

	if ((ds.tm_mday == 31) && (mon == 3 || mon == 5 || mon == 8 || mon == 10)) {
		return Time();
	}

	if ((mon == 1)
			&& ((ds.tm_mday > 29)
					|| ((ds.tm_mday == 29)
							&& ((ds.tm_year & 3)
									|| (((ds.tm_year % 100) == 0)
											&& (((ds.tm_year % 400) != 100))))))) {
		return Time();
	}

	ds.tm_mon = mon;
	ds.tm_hour = ((time[0] - '0') * 10) + (time[1] - '0');
	ds.tm_min = ((time[3] - '0') * 10) + (time[4] - '0');
	ds.tm_sec = ((time[6] - '0') * 10) + (time[7] - '0');

	if ((ds.tm_hour > 23) || (ds.tm_min > 59) || (ds.tm_sec > 61)) {
		return Time();
	}

	ds.tm_usec = 0;
	ds.tm_gmtoff = 0;
	return Time::microseconds(ds.ltz_geti());
}

Time Time::fromHttp(StringView r) {
	sprt::time::time_exp_t ds;
	if (!ds.read(r)) {
		return Time();
	}

	switch (ds.tm_gmt_type) {
	case sprt::time::time_exp_t::gmt_set: return Time::microseconds(ds.gmt_geti());
	case sprt::time::time_exp_t::gmt_local: return Time::microseconds(ds.ltz_geti());
	case sprt::time::time_exp_t::gmt_unset: return Time::microseconds(ds.geti());
	}

	return Time();
}

size_t Time::encodeToFormat(char *buf, size_t bufSize, const char *fmt) const {
	return sprt::time::strftime(buf, bufSize, fmt, toMicroseconds());
}

} // namespace sprt::time
