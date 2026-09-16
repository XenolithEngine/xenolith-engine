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

#include <uchar.h>
#include <string.h>
#include <locale.h>
#include <stdio.h>

namespace sprt::test {

// <uchar.h> UTF-8 <-> UTF-16/UTF-32 conversions. char16_t/char32_t are fixed
// widths on every target, so code-unit and byte values diff identically; an
// astral code point (U+1D11E, the G-clef) exercises the surrogate-pair paths.
// glibc decodes UTF-8 only under a UTF-8 locale; the freestanding libc_impl
// always assumes UTF-8. setlocale's return differs and is not printed.
void performUcharTest() {
	if (!setlocale(LC_ALL, "C.UTF-8")) {
		setlocale(LC_ALL, "en_US.UTF-8");
	}

	// "AΩ𝄞Я€": the astral U+1D11E sits mid-string so mbrtoc16's surrogate
	// continuation (the (size_t)-3 low-surrogate return) is exercised in the loop.
	const char *mb = "AΩ\U0001D11EЯ€";
	printf("strlen=%zu\n", strlen(mb));

	// mbrtoc32: one code point per call.
	{
		mbstate_t st = {};
		const char *p = mb;
		size_t left = strlen(mb);
		int i = 0;
		while (left > 0) {
			char32_t c32 = 0;
			size_t r = mbrtoc32(&c32, p, left, &st);
			if (r == (size_t)-1 || r == (size_t)-2 || r == 0) {
				printf("mbrtoc32 stop=%lld\n", (long long)r);
				break;
			}
			printf("mbrtoc32[%d] consumed=%zu cp=%x\n", i++, r, (unsigned)c32);
			p += r;
			left -= r;
		}
	}

	// mbrtoc16: code units; an astral point yields a high surrogate (bytes
	// consumed) followed by a (size_t)-3 low surrogate (no bytes consumed).
	{
		mbstate_t st = {};
		const char *p = mb;
		size_t left = strlen(mb);
		int i = 0;
		while (left > 0) {
			char16_t c16 = 0;
			size_t r = mbrtoc16(&c16, p, left, &st);
			if (r == (size_t)-3) {
				printf("mbrtoc16[%d] low=%x\n", i++, (unsigned)c16);
				continue; // no bytes consumed
			}
			if (r == (size_t)-1 || r == (size_t)-2 || r == 0) {
				printf("mbrtoc16 stop=%lld\n", (long long)r);
				break;
			}
			printf("mbrtoc16[%d] consumed=%zu u=%x\n", i++, r, (unsigned)c16);
			p += r;
			left -= r;
		}
	}

	// c32rtomb: code point -> UTF-8.
	{
		static const char32_t cps[] = {0x41, 0x3A9, 0x42F, 0x20AC, 0x1D11E};
		mbstate_t st = {};
		for (char32_t cp : cps) {
			char buf[8];
			size_t r = c32rtomb(buf, cp, &st);
			printf("c32rtomb(%x) len=%lld:", (unsigned)cp, (long long)r);
			for (size_t j = 0; j < r && r != (size_t)-1; ++j) { printf(" %02x", (unsigned char)buf[j]); }
			printf("\n");
		}
	}

	// c16rtomb: code units -> UTF-8; a surrogate pair (U+1D11E == D834 DD1E)
	// emits nothing on the high surrogate and the full sequence on the low one.
	{
		static const char16_t units[] = {0x41, 0x3A9, 0xD834, 0xDD1E};
		mbstate_t st = {};
		for (char16_t u : units) {
			char buf[8];
			size_t r = c16rtomb(buf, u, &st);
			printf("c16rtomb(%x) len=%lld:", (unsigned)u, (long long)r);
			for (size_t j = 0; j < r && r != (size_t)-1; ++j) { printf(" %02x", (unsigned char)buf[j]); }
			printf("\n");
		}
	}

	setlocale(LC_ALL, "C");
}

} // namespace sprt::test
