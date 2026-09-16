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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

namespace sprt::test {

static const char *errName(int e) {
	if (e == 0) {
		return "0";
	}
	if (e == ERANGE) {
		return "ERANGE";
	}
	if (e == EINVAL) {
		return "EINVAL";
	}
	return "OTHER";
}

void performStrtolTest() {
	// strtol/strtoul return `long`/`unsigned long`: 32-bit on LLP64 (Windows),
	// 64-bit on LP64 (Linux). Exact-value checks therefore use only inputs whose
	// result fits a 32-bit long (identical on both); overflow is checked through
	// the portable clamp-to-LONG_MAX/LONG_MIN + ERANGE contract, and the full
	// 64-bit range through strtoll/strtoull (64-bit on every target).
	static const char *cases[] = {
		"0", "  42", "+42", "-42", "  -17abc", "2147483647", "-2147483648", "0x1A",
		"0X1a", "  0xff ", "010", "0b101", "xyz", "", "   ", "  +", "0xG", "  0x",
	};
	for (auto c : cases) {
		char *end = nullptr;
		errno = 0;
		long v = strtol(c, &end, 0);
		printf("strtol(\"%s\",0)=%ld end=%zd errno=%s\n", c, v, end - c, errName(errno));
	}
	// base 10
	for (auto c : cases) {
		char *end = nullptr;
		errno = 0;
		long v = strtol(c, &end, 10);
		printf("strtol(\"%s\",10)=%ld end=%zd errno=%s\n", c, v, end - c, errName(errno));
	}
	// base 16 / 2 / 36 (results within 32-bit range; mixed-case digits)
	static const char *b16[] = {"ff", "0xFF", "-0x10", "7fffffff", "deadb", "g"};
	for (auto c : b16) {
		char *end = nullptr;
		errno = 0;
		long v = strtol(c, &end, 16);
		printf("strtol(\"%s\",16)=%ld end=%zd errno=%s\n", c, v, end - c, errName(errno));
	}
	printf("strtol(101,2)=%ld\n", strtol("101", nullptr, 2));
	printf("strtol(zz,36)=%ld\n", strtol("zz", nullptr, 36));

	// Overflow clamps to LONG_MAX/LONG_MIN with ERANGE. The clamp boundary is
	// platform-dependent (2^31-1 vs 2^63-1), so assert the contract, not a value.
	errno = 0;
	long ov1 = strtol("99999999999999999999999999", nullptr, 10);
	printf("strtol(+over)==LONG_MAX:%d errno=%s\n", ov1 == LONG_MAX, errName(errno));
	errno = 0;
	long ov2 = strtol("-99999999999999999999999999", nullptr, 10);
	printf("strtol(-over)==LONG_MIN:%d errno=%s\n", ov2 == LONG_MIN, errName(errno));

	// strtoul: in-range values are portable; "-1" wraps to ULONG_MAX (whose width
	// differs), so assert that rather than printing it.
	static const char *uc[] = {"0xFFFFFFFF", "4294967295", "+99", "  0"};
	for (auto c : uc) {
		char *end = nullptr;
		errno = 0;
		unsigned long v = strtoul(c, &end, 0);
		printf("strtoul(\"%s\")=%lu end=%zd errno=%s\n", c, v, end - c, errName(errno));
	}
	printf("strtoul(-1)==ULONG_MAX:%d\n", strtoul("-1", nullptr, 10) == ULONG_MAX);

	// strtoll / strtoull are 64-bit on every target: exact values are portable.
	printf("strtoll(9223372036854775807)=%lld\n", strtoll("9223372036854775807", nullptr, 10));
	printf("strtoll(-9223372036854775808)=%lld\n", strtoll("-9223372036854775808", nullptr, 10));
	errno = 0;
	long long ll = strtoll("99999999999999999999", nullptr, 10);
	printf("strtoll(over)==LLONG_MAX:%d errno=%s\n", ll == LLONG_MAX, errName(errno));
	printf("strtoull(-1)=%llu\n", strtoull("-1", nullptr, 10));
	printf("strtoull(18446744073709551615)=%llu\n", strtoull("18446744073709551615", nullptr, 10));
	errno = 0;
	unsigned long long ull = strtoull("99999999999999999999999999", nullptr, 10);
	printf("strtoull(over)==ULLONG_MAX:%d errno=%s\n", ull == ULLONG_MAX, errName(errno));
}

void performAtoiTest() {
	// atoi -> int (32-bit) and atoll -> long long (64-bit) are the same width on
	// both targets, so their results (including the wrap on int overflow) match.
	static const char *cases[] = {"0", "42", "-42", "  +7", "2147483647", "2147483648",
		"-2147483648", "abc", "12ab", "", "  -0", "+-3"};
	for (auto c : cases) {
		printf("atoi(\"%s\")=%d atoll=%lld\n", c, atoi(c), atoll(c));
	}
	// atol returns `long`, which is 32-bit on Windows and 64-bit on Linux, and on
	// overflow its behaviour is undefined; restrict it to operands that fit a
	// 32-bit long so the result is identical on both targets.
	static const char *lc[] = {"0", "2147483647", "-2147483648", "  -123", "999", "42abc"};
	for (auto c : lc) {
		printf("atol(\"%s\")=%ld\n", c, atol(c));
	}
}

void performStrtodTest() {
	static const char *cases[] = {
		"0", "0.0", "-0.0", "1", "1.5", "-1.5", "3.14159265358979",
		"  2.5e3", "1e-10", "1E10", ".5", "5.", "+1.25", "  -42.0abc",
		"1.7976931348623157e308", "1e309", "1e-400", "inf", "-inf",
		"infinity", "nan", "NAN", "0x1p4", "0x1.8p1", "0x1.fffffffffffffp+1023",
		"xyz", "", "   ", "1.0000000000000002", "123456789.123456789",
	};
	for (auto c : cases) {
		char *end = nullptr;
		errno = 0;
		double v = strtod(c, &end);
		// %a gives the exact bit pattern; %.17g a readable round-trip form.
		printf("strtod(\"%s\")=%.17g [%a] end=%zd errno=%s\n", c, v, v, end - c, errName(errno));
	}
	// strtof
	static const char *fc[] = {"1.5", "3.4028235e38", "1e39", "0.1", "1e-46"};
	for (auto c : fc) {
		errno = 0;
		float v = strtof(c, nullptr);
		printf("strtof(\"%s\")=%.9g [%a] errno=%s\n", c, (double)v, (double)v, errName(errno));
	}
	// atof
	printf("atof(3.14)=%.17g\n", atof("3.14"));
	printf("atof(  -2.5e2xyz)=%.17g\n", atof("  -2.5e2xyz"));
}

static int cmpInt(const void *a, const void *b) {
	int x = *(const int *)a;
	int y = *(const int *)b;
	return (x > y) - (x < y);
}

void performQsortBsearchTest() {
	int arr[] = {5, 3, 8, 1, 9, 2, 7, 4, 6, 0, 3, 8};
	size_t n = sizeof(arr) / sizeof(arr[0]);
	qsort(arr, n, sizeof(int), cmpInt);
	printf("qsort:");
	for (size_t i = 0; i < n; ++i) { printf(" %d", arr[i]); }
	printf("\n");

	static const int keys[] = {0, 3, 8, 9, -1, 10, 5};
	for (int key : keys) {
		void *r = bsearch(&key, arr, n, sizeof(int), cmpInt);
		printf("bsearch(%d)=%s\n", key, r ? "found" : "null");
	}

	// qsort stability is not guaranteed; just verify the sorted order of a
	// larger pseudo-random (but fixed) sequence.
	int big[40];
	unsigned seed = 12345;
	for (int i = 0; i < 40; ++i) {
		seed = seed * 1103515245u + 12345u;
		big[i] = (int)((seed >> 16) % 100);
	}
	qsort(big, 40, sizeof(int), cmpInt);
	printf("qsort40:");
	for (int i = 0; i < 40; ++i) { printf(" %d", big[i]); }
	printf("\n");
}

void performAbsDivTest() {
	// Note: abs/labs/llabs are "C / freestanding only" in this runtime; in C++
	// they are expected to come from <cstdlib>/<cmath> (std::abs), so they are
	// not exercised through the C <stdlib.h> surface here. div/ldiv/lldiv are.
	div_t d = div(17, 5);
	printf("div(17,5)=%d,%d\n", d.quot, d.rem);
	d = div(-17, 5);
	printf("div(-17,5)=%d,%d\n", d.quot, d.rem);
	d = div(17, -5);
	printf("div(17,-5)=%d,%d\n", d.quot, d.rem);
	d = div(-17, -5);
	printf("div(-17,-5)=%d,%d\n", d.quot, d.rem);

	ldiv_t ld = ldiv(100000L, 7L);
	printf("ldiv(100000,7)=%ld,%ld\n", ld.quot, ld.rem);
	lldiv_t lld = lldiv(-10000000001LL, 3LL);
	printf("lldiv=%lld,%lld\n", lld.quot, lld.rem);

	// MSVC CRT crash-UI stubs (Windows-only surface). sprt has no abort dialog /
	// report-fault UI, so these ignore their flags and report the previous value (0).
	// Guarded so the host and Windows outputs are identical (the host has no such API).
#if SPRT_WINDOWS
	unsigned prev_ab = _set_abort_behavior(_CALL_REPORTFAULT, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
	int prev_em = _set_error_mode(_OUT_TO_STDERR);
	printf("crt_ui: abort=%u errmode=%d\n", prev_ab, prev_em);
#else
	printf("crt_ui: abort=%u errmode=%d\n", 0u, 0);
#endif
}

} // namespace sprt::test
