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

#include <ctype.h>
#include <stdio.h>

namespace sprt::test {

// Classify every byte value 0..255 in the "C" locale and emit one bitmask line
// per value. Any divergence in classification shows up as a single changed line.
void performCtypeTest() {
	for (int c = 0; c <= 255; ++c) {
		unsigned m = 0;
		m |= isalnum(c) ? 1u << 0 : 0;
		m |= isalpha(c) ? 1u << 1 : 0;
		m |= isblank(c) ? 1u << 2 : 0;
		m |= iscntrl(c) ? 1u << 3 : 0;
		m |= isdigit(c) ? 1u << 4 : 0;
		m |= isgraph(c) ? 1u << 5 : 0;
		m |= islower(c) ? 1u << 6 : 0;
		m |= isprint(c) ? 1u << 7 : 0;
		m |= ispunct(c) ? 1u << 8 : 0;
		m |= isspace(c) ? 1u << 9 : 0;
		m |= isupper(c) ? 1u << 10 : 0;
		m |= isxdigit(c) ? 1u << 11 : 0;
		printf("%3d mask=%04x lower=%d upper=%d\n", c, m, tolower(c), toupper(c));
	}

	// EOF handling
	printf("isspace(EOF)=%d isalpha(EOF)=%d\n", isspace(EOF) ? 1 : 0, isalpha(EOF) ? 1 : 0);
	printf("tolower(EOF)=%d toupper(EOF)=%d\n", tolower(EOF), toupper(EOF));
}

} // namespace sprt::test
