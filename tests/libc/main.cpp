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
#include <string.h>
#include <locale.h>

#include "tests.h"

namespace sprt::test {

static const LibcTest s_tests[] = {
	// string.h
	{"string_compare", &performStringCompareTest},
	{"string_search", &performStringSearchTest},
	{"string_copy", &performStringCopyTest},
	{"string_token", &performStringTokenTest},
	{"memory", &performMemoryTest},
	// ctype.h
	{"ctype", &performCtypeTest},
	// stdlib.h
	{"strtol", &performStrtolTest},
	{"strtod", &performStrtodTest},
	{"atoi", &performAtoiTest},
	{"qsort_bsearch", &performQsortBsearchTest},
	{"abs_div", &performAbsDivTest},
	// stdio.h
	{"printf_int", &performPrintfIntTest},
	{"printf_float", &performPrintfFloatTest},
	{"printf_string", &performPrintfStringTest},
	{"scanf", &performScanfTest},
	// stdio.h (FILE* I/O) / unistd.h (fd I/O) / path forms
	{"stdio_file", &performStdioFileTest},
	{"unistd", &performUnistdTest},
	{"path_posix", &performPathPosixTest},
	{"path_windows", &performPathWindowsTest},
	{"dirent", &performDirentTest},
	// wchar.h / multibyte / uchar.h
	{"wchar_string", &performWcharStringTest},
	{"wcstol", &performWcstolTest},
	{"multibyte", &performMultibyteTest},
	{"uchar", &performUcharTest},
	// ISO macro/type mappings
	{"macros", &performMacrosTest},
	// math.h
	{"math", &performMathTest},
	// complex.h
	{"complex", &performComplexTest},
	// threads.h (C11)
	{"threads", &performThreadsTest},
	// tgmath.h
	{"tgmath", &performTgmathTest},
	// stdatomic.h
	{"stdatomic", &performStdatomicTest},
	// time.h
	{"time", &performTimeTest},
	// inttypes.h
	{"inttypes", &performInttypesTest},

	{nullptr, nullptr},
};

const LibcTest *getLibcTests() { return s_tests; }

} // namespace sprt::test

using namespace sprt::test;

int main(int argc, const char *argv[]) {
	// Force the neutral "C" locale: glibc may otherwise honour LC_* from the
	// environment (e.g. comma decimal separator) while the freestanding
	// libc_impl always behaves as "C". The compare driver also sets LC_ALL=C.
	setlocale(LC_ALL, "C");

	const LibcTest *tests = getLibcTests();

	// `--list` prints every test name (one per line) so the compare driver can
	// enumerate and run them individually.
	if (argc >= 2 && strcmp(argv[1], "--list") == 0) {
		for (const LibcTest *t = tests; t->name; ++t) { printf("%s\n", t->name); }
		return 0;
	}

	if (argc >= 2) {
		for (const LibcTest *t = tests; t->name; ++t) {
			if (strcmp(t->name, argv[1]) == 0) {
				t->fn();
				return 0;
			}
		}
		fprintf(stderr, "Test not found: %s\n", argv[1]);
		return 1;
	}

	// No argument: run every test, framed by a header line so a full run can
	// also be diffed end to end.
	for (const LibcTest *t = tests; t->name; ++t) {
		printf("==== %s ====\n", t->name);
		t->fn();
	}
	return 0;
}
