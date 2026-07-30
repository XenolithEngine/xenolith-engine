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

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

namespace sprt::test {

// Fixed UTC instants (no dependency on the host timezone). gmtime/strftime only
// read struct-tm fields, so the output is fully deterministic.
void performTimeTest() {
	static const long long instants[] = {
		0LL,            // 1970-01-01 00:00:00
		86400LL,        // 1970-01-02
		1000000000LL,   // 2001-09-09 01:46:40
		1234567890LL,   // 2009-02-13 23:31:30
		951782400LL,    // 2000-02-29 (leap day)
		1582934400LL,   // 2020-02-29 (leap day)
		-86400LL,       // 1969-12-31
		-1LL,           // 1969-12-31 23:59:59
		1893456000LL,   // 2030-01-01
		68169600LL,     // 1972-02-29 (leap)
	};
	for (long long t : instants) {
		time_t tt = (time_t)t;
		struct tm g;
		g = {};
		gmtime_r(&tt, &g);
		printf("t=%lld Y=%d M=%d D=%d h=%d m=%d s=%d wday=%d yday=%d isdst=%d\n", t,
				g.tm_year + 1900, g.tm_mon + 1, g.tm_mday, g.tm_hour, g.tm_min, g.tm_sec, g.tm_wday,
				g.tm_yday, g.tm_isdst);

		char buf[128];
		// Field-only specifiers (no %Z/%z which would read the timezone).
		strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S | %a %A %b %B | %j %u %w %p %I", &g);
		printf("  strftime=[%s]\n", buf);

		char abuf[32];
		printf("  asctime=[%s]", asctime_r(&g, abuf)); // asctime keeps its trailing \n
	}

	// strftime corner specifiers on a fixed instant
	time_t fixed = 1234567890; // 2009-02-13 23:31:30 UTC, a Friday
	struct tm g;
	g = {};
	gmtime_r(&fixed, &g);
	static const char *fmts[] = {"%C", "%y", "%e", "%m/%d/%y", "%H:%M", "%R", "%T", "%D", "%F",
		"%n%t", "%%", "%G-W%V-%u", "%U", "%W", "%h", "%P"};
	for (auto f : fmts) {
		char buf[64];
		size_t r = strftime(buf, sizeof(buf), f, &g);
		printf("strftime(\"%s\")=%zu [%s]\n", f, r, buf);
	}

	// strftime truncation: returns 0 when the result would not fit
	{
		char buf[5];
		size_t r = strftime(buf, sizeof(buf), "%Y-%m-%d", &g);
		printf("strftime(small)=%zu\n", r);
	}

	// difftime
	printf("difftime(100,40)=%g\n", difftime((time_t)100, (time_t)40));
	printf("difftime(40,100)=%g\n", difftime((time_t)40, (time_t)100));
	printf("difftime(1000000000,0)=%g\n", difftime((time_t)1000000000, (time_t)0));

	// localtime_s (MSVC argument order: tm* first). Its filled fields depend on the
	// timezone, so only the deterministic contract is checked: 0 on success, nonzero
	// for a null tm or null time pointer.
	{
		time_t t = 1234567890;
		struct tm lt;
		lt = {};
		int ok = localtime_s(&lt, &t);
		int null_tm = localtime_s((struct tm *)0, &t);
		int null_t = localtime_s(&lt, (const time_t *)0);
		printf("localtime_s: ok=%d null_tm=%d null_t=%d\n", ok == 0, null_tm != 0, null_t != 0);
	}

	/*
		mktime returns SECONDS, and reads its fields as LOCAL time.

		Stated as a round-trip through localtime rather than against a fixed timestamp,
		so the check holds in any zone: the freestanding Windows libc does not honour TZ
		(localtime_r goes through SystemTimeToTzSpecificLocalTime, which only knows the
		system zone), and a hard-coded expected value would only be comparing that.

		Both halves were broken and neither round-trip noticed. mktime returned sprt's
		internal MICROSECONDS - llvm's Chrono test read back the year 2143 for a 2006
		timestamp - and it read the fields as UTC, because it applied the tm_gmtoff
		field that portable callers leave at zero instead of the zone in force.
	*/
	{
		struct tm tm = {};
		tm.tm_year = 106; // 2006
		tm.tm_mon = 0;
		tm.tm_mday = 2;
		tm.tm_hour = 15;
		tm.tm_min = 4;
		tm.tm_sec = 5;
		tm.tm_isdst = -1;

		time_t t = mktime(&tm);

		// Seconds: a 2006 timestamp is ~1.13e9. Microseconds would be 10^6 times that.
		printf("mktime magnitude ok=%d\n", t > 1'000'000'000LL && t < 2'000'000'000LL);

		// Local: reading it back as local time returns the fields it was given.
		struct tm back = {};
		localtime_r(&t, &back);
		printf("mktime localtime round-trip: %04d-%02d-%02d %02d:%02d:%02d\n",
				back.tm_year + 1900, back.tm_mon + 1, back.tm_mday, back.tm_hour, back.tm_min,
				back.tm_sec);

		// ...and mktime normalizes the struct it was handed (POSIX).
		printf("mktime normalized: wday-set=%d yday-set=%d\n", tm.tm_wday == back.tm_wday,
				tm.tm_yday == back.tm_yday);
	}

	/*
		gettimeofday's tv_usec is microseconds, so it never leaves 0..999999. A
		nanosecond source divided by the wrong constant lands outside that range - which
		no round-trip notices either.
	*/
	{
		struct timeval tv = {};
		int r = gettimeofday(&tv, nullptr);
		printf("gettimeofday: ok=%d usec-in-range=%d\n", r == 0,
				tv.tv_usec >= 0 && tv.tv_usec < 1'000'000);
	}
}

} // namespace sprt::test
