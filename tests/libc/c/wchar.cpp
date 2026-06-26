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

#include <wchar.h>
#include <wctype.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <limits.h>
#include <stdio.h>

#include "test_util.h"

namespace sprt::test {

// wchar_t is 32-bit on the Linux/glibc target and 16-bit on Windows MSVC. For
// BMP code points (<= U+FFFF) a single wchar_t unit holds the same value on both
// (UTF-16 and UTF-32 agree), so BMP-only data is identity-safe. We therefore
// keep wide data within the BMP and print contents as hex code units.
static void printWide(const char *label, const wchar_t *s) {
	printf("%s=", label);
	if (!s) {
		printf("NULL\n");
		return;
	}
	for (; *s; ++s) { printf("%x.", (unsigned)(unsigned long)*s); }
	printf("\n");
}

static void printWideOffset(const char *label, const wchar_t *base, const wchar_t *p) {
	if (!p) {
		printf("%s=NULL\n", label);
	} else {
		printf("%s=%ld\n", label, (long)(p - base));
	}
}

void performWcharStringTest() {
	const wchar_t *s = L"Hello, Мир! Hello!"; // BMP only (Latin + Cyrillic)
	size_t len = wcslen(s);
	printf("wcslen=%zu\n", len);
	printf("wcsnlen(8)=%zu wcsnlen(100)=%zu\n", wcsnlen(s, 8), wcsnlen(s, 100));

	// wcscmp / wcsncmp — sign only
	printf("wcscmp(abc,abc)=%d\n", sgn(wcscmp(L"abc", L"abc")));
	printf("wcscmp(abc,abd)=%d\n", sgn(wcscmp(L"abc", L"abd")));
	printf("wcscmp(abd,abc)=%d\n", sgn(wcscmp(L"abd", L"abc")));
	printf("wcscmp(abc,abcd)=%d\n", sgn(wcscmp(L"abc", L"abcd")));
	printf("wcsncmp(abcX,abcY,3)=%d\n", sgn(wcsncmp(L"abcX", L"abcY", 3)));
	printf("wcsncmp(abcX,abcY,4)=%d\n", sgn(wcsncmp(L"abcX", L"abcY", 4)));
	// Cyrillic BMP ordering
	printf("wcscmp(Аб,Ав)=%d\n", sgn(wcscmp(L"Аб", L"Ав")));
	printf("wcscasecmp(ABC,abc)=%d\n", sgn(wcscasecmp(L"ABC", L"abc")));
	printf("wcsncasecmp(Hello,help,3)=%d\n", sgn(wcsncasecmp(L"Hello", L"help", 3)));

	// search
	printWideOffset("wcschr(M=0x41C)", s, wcschr(s, L'М'));
	printWideOffset("wcschr(o)", s, wcschr(s, L'o'));
	printWideOffset("wcsrchr(o)", s, wcsrchr(s, L'o'));
	printWideOffset("wcschr(z)", s, wcschr(s, L'z'));
	printWideOffset("wcsstr(Hello)", s, wcsstr(s, L"Hello"));
	printWideOffset("wcsstr(Мир)", s, wcsstr(s, L"Мир"));
	printWideOffset("wcsstr(xyz)", s, wcsstr(s, L"xyz"));
	printf("wcsspn(aabbc,ab)=%zu\n", wcsspn(L"aabbc", L"ab"));
	printf("wcscspn(abcde,cd)=%zu\n", wcscspn(L"abcde", L"cd"));
	printWideOffset("wcspbrk(!,)", s, wcspbrk(s, L"!,"));

	// copy / cat
	wchar_t buf[64];
	wcscpy(buf, L"foo");
	wcscat(buf, L"bar");
	printWide("wcscpy+cat", buf);
	wmemset(buf, L'#', 8);
	buf[8] = 0;
	wcsncpy(buf, L"hi", 5); // NUL-pads
	printf("wcsncpy units:");
	for (int i = 0; i < 8; ++i) { printf(" %x", (unsigned)(unsigned long)buf[i]); }
	printf("\n");

	// wmem*
	wchar_t wm[8];
	wmemset(wm, L'A', 4);
	wm[4] = 0;
	printWide("wmemset", wm);
	wmemcpy(wm, L"WXYZ", 4);
	printWide("wmemcpy", wm);
	printf("wmemcmp(abc,abd,3)=%d\n", sgn(wmemcmp(L"abc", L"abd", 3)));
	printWideOffset("wmemchr(Y)", wm, wmemchr(wm, L'Y', 4));

	// btowc / wctob (ASCII). btowc(EOF) yields WEOF, whose numeric value depends
	// on the width of wint_t (32-bit on Linux, 16-bit on Windows), so compare it
	// against WEOF rather than printing the raw value.
	printf("btowc(A)=%x wctob(0x42)=%d\n", (unsigned)(unsigned long)btowc('A'), wctob(L'B'));
	printf("btowc(EOF)==WEOF:%d\n", btowc(EOF) == WEOF);

	// wide ctype
	printf("iswalpha(A)=%d iswdigit(5)=%d iswspace(sp)=%d iswupper(A)=%d\n", iswalpha(L'A') ? 1 : 0,
			iswdigit(L'5') ? 1 : 0, iswspace(L' ') ? 1 : 0, iswupper(L'A') ? 1 : 0);
	printf("towlower(A)=%x towupper(a)=%x\n", (unsigned)(unsigned long)towlower(L'A'),
			(unsigned)(unsigned long)towupper(L'a'));
}

void performWcstolTest() {
	// wcstol returns `long` (32-bit on Windows / 64-bit on Linux): exact checks
	// use in-range inputs, overflow is checked via the clamp-to-LONG_MAX/ERANGE
	// contract, and the 64-bit range via wcstoll/wcstoull (64-bit everywhere).
	static const wchar_t *cases[] = {L"0", L"  42", L"-42", L"0x1A", L"2147483647", L"-2147483648",
		L"xyz", L"  -17abc", L"010"};
	for (auto c : cases) {
		wchar_t *end = nullptr;
		long v = wcstol(c, &end, 0);
		printf("wcstol=%ld end=%ld\n", v, (long)(end - c));
	}
	printf("wcstol(over)==LONG_MAX:%d\n", wcstol(L"99999999999999999999", nullptr, 10) == LONG_MAX);
	printf("wcstoul(-1)==ULONG_MAX:%d\n", wcstoul(L"-1", nullptr, 10) == ULONG_MAX);
	printf("wcstoll(big)=%lld\n", wcstoll(L"9223372036854775807", nullptr, 10));
	printf("wcstoull(max)=%llu\n", wcstoull(L"18446744073709551615", nullptr, 10));

	// wcstod / wcstof
	static const wchar_t *fc[] = {L"3.14", L"  -2.5e3", L".5", L"1e10", L"inf", L"nan", L"0x1p4",
		L"xyz"};
	for (auto c : fc) {
		wchar_t *end = nullptr;
		double v = wcstod(c, &end);
		printf("wcstod=%.17g end=%ld\n", v, (long)(end - c));
	}

	// wcstok
	wchar_t buf[] = L"a,b,,c";
	wchar_t *st = nullptr;
	int i = 0;
	for (wchar_t *t = wcstok(buf, L",", &st); t; t = wcstok(nullptr, L",", &st)) {
		printf("wcstok[%d] units:", i++);
		for (wchar_t *p = t; *p; ++p) { printf(" %x", (unsigned)(unsigned long)*p); }
		printf("\n");
	}
}

void performMultibyteTest() {
	// Multibyte <-> wide conversion is always UTF-8 on the freestanding libc_impl
	// and on glibc once a UTF-8 locale is installed; install one so the host
	// decoder matches. The libc_impl ignores the locale (UTF-8 is assumed), so
	// setlocale's *return* differs between targets and is deliberately not printed.
	//
	// Both the non-restartable <stdlib.h> family (mbstowcs/mbtowc/wctomb/wcstombs)
	// and the restartable <wchar.h> family (mbrtowc/wcrtomb/mbsrtowcs/wcsrtombs/
	// mbrlen/btowc/wctob/mbsinit) are exercised: the host libc_wrapper forwards
	// the former to glibc when __SPRT_CONFIG_HAVE_STDLIB_MB is enabled (which it
	// is for Linux), so both families compare identically. Per project guidance,
	// UTF-16-specific (surrogate) behaviour of the 16-bit Windows wchar_t is out
	// of scope, so data stays inside the BMP, where one code point is exactly one
	// wchar_t unit on both 16- and 32-bit wchar_t.
	if (!setlocale(LC_ALL, "C.UTF-8")) {
		setlocale(LC_ALL, "en_US.UTF-8");
	}

	// "AΩЯ€" : U+0041, U+03A9, U+042F, U+20AC  (1,2,2,3 UTF-8 bytes)
	const char *mb = "AΩЯ€";
	printf("strlen(mb)=%zu\n", strlen(mb));

	// --- non-restartable <stdlib.h> family ---
	wchar_t sbuf[16];
	size_t sn = mbstowcs(sbuf, mb, 16);
	printf("mbstowcs n=%zu units:", sn);
	for (size_t i = 0; i < sn; ++i) { printf(" %x", (unsigned)(unsigned long)sbuf[i]); }
	printf("\n");
	printf("mbstowcs(NULL)=%zu\n", mbstowcs(nullptr, mb, 0));
	char sc[32];
	size_t sm = wcstombs(sc, sbuf, sizeof(sc));
	printf("wcstombs m=%zu bytes:", sm);
	for (size_t i = 0; i < sm; ++i) { printf(" %02x", (unsigned char)sc[i]); }
	printf("\n");
	printf("wcstombs(NULL)=%zu\n", wcstombs(nullptr, sbuf, 0));
	{
		wchar_t wc = 0;
		int r = mbtowc(&wc, "A", 1);
		printf("mbtowc(A)=%d wc=%x\n", r, (unsigned)(unsigned long)wc);
		char one[8] = {};
		int w = wctomb(one, L'A');
		printf("wctomb(A)=%d byte=%02x\n", w, (unsigned char)one[0]);
		// UTF-8 is not a state-dependent encoding, so the reset query returns 0.
		printf("mbtowc(NULL state query)=%d\n", mbtowc(nullptr, nullptr, 0));
	}

	// --- restartable <wchar.h> family ---
	// mbrtowc step by step: bytes consumed + decoded code point.
	mbstate_t st = {};
	const char *p = mb;
	size_t left = strlen(mb);
	int idx = 0;
	while (left > 0) {
		wchar_t wc = 0;
		size_t r = mbrtowc(&wc, p, left, &st);
		printf("mbrtowc[%d] consumed=%lld wc=%x\n", idx++, (long long)r,
				(unsigned)(unsigned long)wc);
		if (r == 0 || r == (size_t)-1 || r == (size_t)-2) {
			break;
		}
		p += r;
		left -= r;
	}

	// mbrlen of each sequence (same byte counts, no wide output).
	st = {};
	p = mb;
	left = strlen(mb);
	idx = 0;
	while (left > 0) {
		size_t r = mbrlen(p, left, &st);
		printf("mbrlen[%d]=%lld\n", idx++, (long long)r);
		if (r == 0 || r == (size_t)-1 || r == (size_t)-2) {
			break;
		}
		p += r;
		left -= r;
	}

	// mblen (non-restartable <stdlib.h> form): same per-character byte counts.
	// mblen(NULL,0) reports whether the encoding is state-dependent (UTF-8 -> 0).
	printf("mblen(NULL)=%d\n", mblen(nullptr, 0));
	p = mb;
	left = strlen(mb);
	idx = 0;
	while (left > 0) {
		int r = mblen(p, left);
		printf("mblen[%d]=%d\n", idx++, r);
		if (r <= 0) {
			break;
		}
		p += r;
		left -= r;
	}

	// mbsrtowcs: whole-string decode (restartable form of mbstowcs).
	wchar_t wbuf[16];
	st = {};
	const char *src = mb;
	size_t n = mbsrtowcs(wbuf, &src, 16, &st);
	printf("mbsrtowcs n=%zu units:", n);
	for (size_t i = 0; i < n; ++i) { printf(" %x", (unsigned)(unsigned long)wbuf[i]); }
	printf("\n");
	// length query (dst == NULL): no characters stored, src untouched.
	st = {};
	src = mb;
	printf("mbsrtowcs(NULL)=%zu\n", mbsrtowcs(nullptr, &src, 0, &st));

	// wcrtomb: encode each wide unit back to UTF-8.
	st = {};
	for (size_t i = 0; i < n; ++i) {
		char tmp[8];
		size_t r = wcrtomb(tmp, wbuf[i], &st);
		printf("wcrtomb[%zu] len=%lld bytes:", i, (long long)r);
		for (size_t j = 0; j < r && r != (size_t)-1; ++j) {
			printf(" %02x", (unsigned char)tmp[j]);
		}
		printf("\n");
	}

	// wcsrtombs: whole-string encode (restartable form of wcstombs).
	char cbuf[32];
	st = {};
	const wchar_t *wsrc = wbuf;
	size_t m = wcsrtombs(cbuf, &wsrc, sizeof(cbuf), &st);
	printf("wcsrtombs m=%zu bytes:", m);
	for (size_t i = 0; i < m; ++i) { printf(" %02x", (unsigned char)cbuf[i]); }
	printf("\n");

	// btowc / wctob on ASCII, and mbsinit on the initial state.
	printf("btowc(A)=%x wctob(0x42)=%d\n", (unsigned)(unsigned long)btowc('A'), wctob(L'B'));
	st = {};
	printf("mbsinit(zero)=%d\n", mbsinit(&st) ? 1 : 0);

	// restore the neutral locale for any later tests in a full run
	setlocale(LC_ALL, "C");
}

} // namespace sprt::test
