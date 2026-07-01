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

// Coexistence test: the C standard-library headers (<string.h>, <stdlib.h>, ...)
// and their STL counterparts (<cstring>, <cstdlib>, ...) must be includable in the
// same translation unit, and unqualified global calls must be UNAMBIGUOUS.
//
// On the freestanding x86_64-pc-windows-msvc build these resolve to the sprt
// wrappers (include_libc/stl + sprt/cxx); on the Linux host they resolve to the
// system headers. compare.sh diffs the two, so identical output proves both stacks
// behave the same.
//
// Two invariants are checked here:
//  1. AMBIGUITY: this file must COMPILE. Each unqualified call (strlen, abs, sqrt,
//     isdigit, ...) and its std:: form must resolve to exactly one function. In the
//     sprt stack the C library functions are *not* injected into the global/std
//     scope as plain C declarations in C++ mode; the C++ inline overloads are the
//     primary (and only) candidates, so the calls are unambiguous. NB: abs/labs/
//     llabs are C-only in this runtime; in C++ the abs() overload set comes from
//     <cmath>, hence `abs(-5)` is unambiguously the C++ overload (see stdlib.cpp).
//  2. NO INFINITE RECURSION: every printed value below is the *correct* result. The
//     C++ inline wrappers forward to __builtin_*/__sprt_* leaf functions, never to
//     themselves, so a correct result that is actually printed proves the call
//     returned (a self-recursing wrapper would overflow the stack instead).

#include <stdio.h>

// C standard-library headers ...
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
// ... included alongside their STL counterparts (order intentionally mixed).
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <cctype>

namespace sprt::test {

void performCoexistTest() {
	// === <string.h> + <cstring> ===
	char buf[16];
	strcpy(buf, "abc");
	strcat(buf, "de"); // buf == "abcde"
	// Unqualified global call and the std:: form must agree (same underlying fn).
	printf("str: len=%zu/%zu cmp=%d memcmp=%d chr=%td\n", strlen(buf), std::strlen(buf),
			strcmp(buf, "abcde"), memcmp(buf, "abcde", 5), strchr(buf, 'c') - buf);

	char src[8], dst[8];
	memset(src, '=', 5);
	src[5] = '\0';
	memcpy(dst, src, 6);
	printf("str: memset=%s memcpy=%s spn=%zu\n", src, dst, strspn("aabbc", "ab"));

	// === <stdlib.h> + <cstdlib> ===
	// abs(-5) is the C++ overload (the C abs/labs/llabs are freestanding-only).
	int ai = abs(-5);
	long al = std::abs(-7L);
	double ad = std::abs(-2.5);
	div_t dv = div(17, 5);
	printf("num: abs=%d labs=%ld dabs=%g div=%d,%d atoi=%d strtol=%ld\n", ai, al, ad, dv.quot,
			dv.rem, atoi("42"), strtol("100", nullptr, 10));

	// === <math.h> + <cmath> ===
	double sg = sqrt(4.0), ps = pow(2.0, 3.0), fb = fabs(-2.5), fl = floor(2.7);
	double ss = std::sqrt(9.0), cl = std::ceil(2.1);
	printf("math: sqrt=%g/%g pow=%g fabs=%g floor=%g ceil=%g\n", sg, ss, ps, fb, fl, cl);

	// === <ctype.h> + <cctype> ===
	// Normalize predicates to 0/1: the raw non-zero value is implementation-defined.
	printf("ctype: digit=%d alpha=%d punct=%d up=%c lo=%c stdup=%c\n", isdigit('7') ? 1 : 0,
			isalpha('q') ? 1 : 0, ispunct('!') ? 1 : 0, (char) toupper('a'), (char) tolower('Z'),
			(char) std::toupper('m'));
}

} // namespace sprt::test
