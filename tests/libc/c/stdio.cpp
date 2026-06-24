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

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <limits.h>
#include <string.h>

namespace sprt::test {

// snprintf into a fixed buffer, then print the formatted text together with the
// return value (number of chars that would have been written). Quoting makes
// trailing spaces visible.
#define FMT1(fmt, arg) \
	do { \
		char b[128]; \
		int r = snprintf(b, sizeof(b), fmt, arg); \
		printf("%-18s -> [%s] (r=%d)\n", fmt, b, r); \
	} while (0)

void performPrintfIntTest() {
	// %d / %i with flags, width, precision
	FMT1("%d", 0);
	FMT1("%d", 42);
	FMT1("%d", -42);
	FMT1("%+d", 42);
	FMT1("% d", 42);
	FMT1("%5d", 42);
	FMT1("%-5d", 42);
	FMT1("%05d", 42);
	FMT1("%05d", -42);
	FMT1("%+05d", 42);
	FMT1("%5.3d", 42);
	FMT1("%.0d", 0);
	FMT1("%.5d", 42);
	FMT1("%-+8.4d", -42);
	FMT1("%d", INT_MIN);
	FMT1("%d", INT_MAX);

	// unsigned / octal / hex
	FMT1("%u", 42u);
	FMT1("%u", UINT_MAX);
	FMT1("%o", 42u);
	FMT1("%#o", 42u);
	FMT1("%x", 0xABCDu);
	FMT1("%X", 0xABCDu);
	FMT1("%#x", 0xABCDu);
	FMT1("%#X", 0xABCDu);
	FMT1("%08x", 0xABu);
	FMT1("%#010x", 0xABu);
	FMT1("%.4x", 0xAu);
	FMT1("%x", 0u);
	FMT1("%#x", 0u);

	// length modifiers. `long` is 32-bit on Windows (LLP64) and 64-bit on Linux
	// (LP64), so %ld/%lu use operands that fit a 32-bit long (identical on both);
	// 64-bit width is covered by the %ll* conversions, which are 64-bit on every
	// target.
	FMT1("%ld", 1234567890L);
	FMT1("%lld", -1234567890123456789LL);
	FMT1("%lu", 4000000000uL);
	FMT1("%llu", 12345678901234567890ULL);
	FMT1("%llx", 0xDEADBEEFCAFEULL);
	FMT1("%hd", (short)-1);
	FMT1("%hhu", (unsigned char)300);
	FMT1("%hhd", (signed char)-1);

	{
		char b[128];
		int r = snprintf(b, sizeof(b), "%zu %td %jd", (size_t)123, (ptrdiff_t)-5, (intmax_t)99);
		printf("z/t/j -> [%s] (r=%d)\n", b, r);
	}

	// multiple args + %%
	{
		char b[128];
		int r = snprintf(b, sizeof(b), "%d%%-%05d|%+d", 1, 2, 3);
		printf("multi -> [%s] (r=%d)\n", b, r);
	}

	// truncation: small buffer still returns the full length
	{
		char b[4];
		int r = snprintf(b, sizeof(b), "%d", 123456);
		printf("trunc -> [%s] (r=%d)\n", b, r);
	}
}

void performPrintfFloatTest() {
	static const double vals[] = {0.0, -0.0, 1.0, -1.5, 3.14159265358979, 0.1, 100.0,
		123456.789, 0.000123456, 1e20, 1e-20, 2.5, 0.5, 9.999999};
	for (double v : vals) {
		char b[128];
		snprintf(b, sizeof(b), "%f|%.2f|%e|%g|%.10g|%a", v, v, v, v, v, v);
		printf("%.17g -> [%s]\n", v, b);
	}
	// width / flags on floats
	FMT1("%10.3f", 3.14159);
	FMT1("%-10.3f", 3.14159);
	FMT1("%+.3f", 3.14159);
	FMT1("%010.3f", 3.14159);
	FMT1("%#.0f", 5.0);
	FMT1("%.0f", 0.5);  // round-half-to-even -> 0
	FMT1("%.0f", 1.5);  // -> 2
	FMT1("%.0f", 2.5);  // -> 2
	FMT1("%g", 100000.0);
	FMT1("%g", 1000000.0);
	FMT1("%g", 0.0001);
	FMT1("%g", 0.00001);
	FMT1("%G", 1e-7);
	FMT1("%.3e", 0.0);
	FMT1("%E", 12345.678);

	// non-finite
	double inf = 1e308 * 10.0;
	double nan = inf - inf;
	char b[128];
	snprintf(b, sizeof(b), "%f %F %e %g", inf, -inf, inf, inf);
	printf("inf -> [%s]\n", b);
	snprintf(b, sizeof(b), "%f %F %e %g", nan, nan, nan, nan);
	printf("nan -> [%s]\n", b);
}

void performPrintfStringTest() {
	FMT1("%c", 'A');
	FMT1("%5c", 'A');
	FMT1("%-5c", 'A');
	FMT1("%s", "hello");
	FMT1("%10s", "hello");
	FMT1("%-10s", "hello");
	FMT1("%.3s", "hello");
	FMT1("%10.3s", "hello");
	FMT1("%-10.3s", "hello");
	FMT1("%s", "");
	FMT1("%.0s", "hello");

	// NULL string: glibc prints "(null)"; verify the impl matches.
	FMT1("%s", (char *)nullptr);

	// pointer formatting of a fixed value (numeric part is deterministic)
	{
		char b[64];
		snprintf(b, sizeof(b), "%p", (void *)0x1234abcd);
		printf("ptr(0x1234abcd) -> [%s]\n", b);
		snprintf(b, sizeof(b), "%p", (void *)0);
		printf("ptr(0) -> [%s]\n", b);
	}

	// positional / repeated specifiers
	{
		char b[64];
		int r = snprintf(b, sizeof(b), "[%5s][%-5s][%c]", "ab", "cd", '!');
		printf("mix -> [%s] (r=%d)\n", b, r);
	}
}

void performScanfTest() {
	{
		int a = 0, b = 0, c = 0;
		int n = sscanf("12 34 56", "%d %d %d", &a, &b, &c);
		printf("sscanf(3 ints)=%d -> %d %d %d\n", n, a, b, c);
	}
	{
		int a = 0;
		unsigned u = 0;
		int n = sscanf("-7 0xFF", "%d %x", &a, &u);
		printf("sscanf(d,x)=%d -> %d %u\n", n, a, u);
	}
	{
		double d = 0;
		float f = 0;
		int n = sscanf("3.14 2.5e3", "%lf %f", &d, &f);
		printf("sscanf(f)=%d -> %.5g %.5g\n", n, d, (double)f);
	}
	{
		char word[32] = {0};
		int n = sscanf("  hello world", "%s", word);
		printf("sscanf(%%s)=%d -> [%s]\n", n, word);
	}
	{
		char w1[16] = {0}, w2[16] = {0};
		int n = sscanf("abc,def", "%[^,],%s", w1, w2);
		printf("sscanf(scanset)=%d -> [%s][%s]\n", n, w1, w2);
	}
	{
		int a = 0, b = 0;
		int n = sscanf("100abc", "%d%n", &a, &b);
		printf("sscanf(%%n)=%d -> a=%d n=%d\n", n, a, b);
	}
	{
		int a = 0;
		int n = sscanf("   42", "%d", &a);
		printf("sscanf(lead ws)=%d -> %d\n", n, a);
	}
	{
		int a = 0;
		int n = sscanf("abc", "%d", &a);
		printf("sscanf(no match)=%d\n", n);
	}
	{
		int a = 0, b = 0;
		int n = sscanf("12:34", "%d:%d", &a, &b);
		printf("sscanf(lit)=%d -> %d %d\n", n, a, b);
	}
	{
		int a = 0;
		int n = sscanf("123456", "%3d", &a);
		printf("sscanf(width)=%d -> %d\n", n, a);
	}
	{
		long l = 0;
		long long ll = 0;
		// %ld scans into a `long` (32-bit on Windows), so the operand fits a
		// 32-bit long; the 64-bit path is exercised by %lld.
		int n = sscanf("123456789 -8888888888", "%ld %lld", &l, &ll);
		printf("sscanf(l,ll)=%d -> %ld %lld\n", n, l, ll);
	}
}

#undef FMT1

} // namespace sprt::test
