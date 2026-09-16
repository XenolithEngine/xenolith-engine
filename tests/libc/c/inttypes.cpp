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

#include <inttypes.h>
#include <stdio.h>

namespace sprt::test {

// Exercises the fixed-width PRI*/SCN* format-string macros plus the imaxabs/
// imaxdiv/strto*max functions (intmax_t is 64-bit on every target, so the values
// are identical). The conversions strtoimax/strtoumax/wcstoimax/wcstoumax are
// already covered by the strtol/wcstol tests.
void performInttypesTest() {
	int64_t a = -1234567890123456789LL;
	uint64_t b = 12345678901234567890ULL;
	int32_t c = -2000000000;
	uint32_t d = 4000000000u;
	int8_t e = -100;
	uint16_t f = 50000;
	printf("PRId64=%" PRId64 " PRIu64=%" PRIu64 "\n", a, b);
	printf("PRIx64=%" PRIx64 " PRIX64=%" PRIX64 " PRIo64=%" PRIo64 "\n", b, b, b);
	printf("PRId32=%" PRId32 " PRIu32=%" PRIu32 " PRIX32=%" PRIX32 "\n", c, d, d);
	printf("PRId8=%" PRId8 " PRIu16=%" PRIu16 "\n", e, f);
	printf("PRIdLEAST32=%" PRIdLEAST32 " PRIdFAST32=%" PRIdFAST32 "\n", c, c);
	printf("PRIdPTR=%" PRIdPTR " PRIuPTR=%" PRIuPTR "\n", (intptr_t)-42, (uintptr_t)42);
	printf("PRIdMAX=%" PRIdMAX " PRIuMAX=%" PRIuMAX "\n", (intmax_t)-7, (uintmax_t)7);

	// SCN* scanning
	uint64_t s1 = 0;
	int64_t s2 = 0;
	int32_t s3 = 0;
	int n = sscanf("DEADBEEF -42 12345", "%" SCNx64 " %" SCNd64 " %" SCNd32, &s1, &s2, &s3);
	printf("scan n=%d -> %" PRIu64 " %" PRId64 " %" PRId32 "\n", n, s1, s2, s3);

	// imaxabs (intmax_t is 64-bit on both targets)
	printf("imaxabs(-5)=%" PRIdMAX " imaxabs(7)=%" PRIdMAX "\n", imaxabs((intmax_t)-5),
			imaxabs((intmax_t)7));
	printf("imaxabs(min+1)=%" PRIdMAX "\n", imaxabs((intmax_t)(-9223372036854775807LL)));

	// imaxdiv: sign rules match div/ldiv (truncate toward zero)
	static const intmax_t pairs[][2] = {{17, 5}, {-17, 5}, {17, -5}, {-17, -5}};
	for (auto &p : pairs) {
		imaxdiv_t r = imaxdiv(p[0], p[1]);
		printf("imaxdiv(%" PRIdMAX ",%" PRIdMAX ")=%" PRIdMAX ",%" PRIdMAX "\n", p[0], p[1], r.quot,
				r.rem);
	}
}

} // namespace sprt::test
