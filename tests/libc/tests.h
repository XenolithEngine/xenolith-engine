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

#ifndef TESTS_LIBC_TESTS_H
#define TESTS_LIBC_TESTS_H

namespace sprt::test {

// Each entry is a self-contained deterministic test. Output must not depend on
// addresses, time, locale or uninitialised memory so that the host (Linux libc)
// and x86_64-pc-windows-msvc (freestanding libc_impl) runs can be diffed.
struct LibcTest {
	const char *name;
	void (*fn)();
};

// Returned list is sorted by name; terminated by a {nullptr, nullptr} entry.
const LibcTest *getLibcTests();

// string.h / strings.h
void performStringCompareTest();
void performStringSearchTest();
void performStringCopyTest();
void performStringTokenTest();
void performMemoryTest();

// ctype.h
void performCtypeTest();

// stdlib.h
void performStrtolTest();
void performStrtodTest();
void performAtoiTest();
void performQsortBsearchTest();
void performAbsDivTest();

// stdio.h (formatting)
void performPrintfIntTest();
void performPrintfFloatTest();
void performPrintfStringTest();
void performScanfTest();

// stdio.h (FILE* I/O) / unistd.h (fd I/O) / path forms / dirent.h
void performStdioFileTest();
void performUnistdTest();
void performPathPosixTest();
void performPathWindowsTest();
void performDirentTest();

// wchar.h / multibyte / uchar.h
void performWcharStringTest();
void performWcstolTest();
void performMultibyteTest();
void performUcharTest();

// ISO macro/type mappings (signal/fenv/math/wide/stdio/assert)
void performMacrosTest();

// math.h
void performMathTest();

// complex.h
void performComplexTest();

// threads.h (C11)
void performThreadsTest();

// tgmath.h (type-generic math; impl in tgmath_c.c)
void performTgmathTest();

// stdatomic.h (C11 atomics; impl in stdatomic_c.c)
void performStdatomicTest();

// time.h
void performTimeTest();

// inttypes.h
void performInttypesTest();

} // namespace sprt::test

#endif // TESTS_LIBC_TESTS_H
