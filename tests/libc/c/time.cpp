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
}

} // namespace sprt::test
