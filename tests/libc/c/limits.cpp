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

// std::numeric_limits conformance. On the freestanding x86_64-pc-windows-msvc build
// <limits> is the sprt-backed STL header (include_libc/stl/limits, built on
// sprt::Max/Min/Digits/...); on the Linux host it is the system <limits>. compare.sh
// diffs the two, so the sprt implementation is checked against the system reference.
//
// IMPORTANT: only width-stable fundamental types are exercised. long / unsigned long
// (LP64 vs LLP64), wchar_t (4 vs 2 bytes) and long double (80-bit vs 64-bit) have
// different widths on the two targets and would legitimately diverge, so they are
// not printed. The static_asserts use per-target builtin macros, so they validate
// the system limits on the host and the sprt limits on Windows.

#include <stdio.h>

#include <limits>

namespace sprt::test {

namespace {

using std::numeric_limits;

// --- compile-time conformance (validated on both targets) ---
static_assert(numeric_limits<int>::is_specialized && numeric_limits<int>::is_signed
		&& numeric_limits<int>::is_integer && numeric_limits<int>::is_exact);
static_assert(numeric_limits<int>::radix == 2 && numeric_limits<int>::digits == 31);
static_assert(numeric_limits<int>::max() == __INT_MAX__);
static_assert(numeric_limits<int>::min() == (-__INT_MAX__ - 1));
static_assert(numeric_limits<unsigned>::max() == ~0u && numeric_limits<unsigned>::is_modulo);
static_assert(numeric_limits<long long>::max() == __LONG_LONG_MAX__);
static_assert(numeric_limits<unsigned long long>::max() == ~0ull);
static_assert(numeric_limits<bool>::digits == 1 && numeric_limits<bool>::max() == true);
static_assert(numeric_limits<char16_t>::digits == 16 && numeric_limits<char32_t>::digits == 32);
static_assert(numeric_limits<float>::is_iec559 && numeric_limits<float>::has_infinity
		&& !numeric_limits<float>::is_integer);
static_assert(numeric_limits<float>::digits == __FLT_MANT_DIG__);
static_assert(numeric_limits<float>::max() == __FLT_MAX__);
static_assert(numeric_limits<float>::lowest() == -__FLT_MAX__);
static_assert(numeric_limits<float>::min() == __FLT_MIN__);
static_assert(numeric_limits<double>::digits == __DBL_MANT_DIG__);
static_assert(numeric_limits<double>::max() == __DBL_MAX__);
static_assert(numeric_limits<double>::infinity() > __DBL_MAX__);
// cv-qualified forwards + non-arithmetic primary
static_assert(numeric_limits<const int>::max() == __INT_MAX__);
struct __not_arith { };
static_assert(!numeric_limits<__not_arith>::is_specialized);

} // namespace

void performLimitsTest() {
	printf("int    : digits=%d signed=%d max=%d min=%d\n", numeric_limits<int>::digits,
			(int) numeric_limits<int>::is_signed, numeric_limits<int>::max(),
			numeric_limits<int>::min());
	printf("uint   : digits=%d modulo=%d max=%u\n", numeric_limits<unsigned>::digits,
			(int) numeric_limits<unsigned>::is_modulo, numeric_limits<unsigned>::max());
	printf("short  : digits=%d max=%d min=%d\n", numeric_limits<short>::digits,
			(int) numeric_limits<short>::max(), (int) numeric_limits<short>::min());
	printf("schar  : digits=%d max=%d min=%d\n", numeric_limits<signed char>::digits,
			(int) numeric_limits<signed char>::max(), (int) numeric_limits<signed char>::min());
	printf("uchar  : digits=%d max=%d\n", numeric_limits<unsigned char>::digits,
			(int) numeric_limits<unsigned char>::max());
	printf("llong  : digits=%d max=%lld min=%lld\n", numeric_limits<long long>::digits,
			numeric_limits<long long>::max(), numeric_limits<long long>::min());
	printf("ullong : digits=%d max=%llu\n", numeric_limits<unsigned long long>::digits,
			numeric_limits<unsigned long long>::max());
	printf("bool   : digits=%d max=%d min=%d integer=%d\n", numeric_limits<bool>::digits,
			(int) numeric_limits<bool>::max(), (int) numeric_limits<bool>::min(),
			(int) numeric_limits<bool>::is_integer);
	printf("float  : digits=%d radix=%d iec559=%d inf=%d max=%g min=%g eps=%g maxexp=%d minexp=%d\n",
			numeric_limits<float>::digits, numeric_limits<float>::radix,
			(int) numeric_limits<float>::is_iec559, (int) numeric_limits<float>::has_infinity,
			(double) numeric_limits<float>::max(), (double) numeric_limits<float>::min(),
			(double) numeric_limits<float>::epsilon(), numeric_limits<float>::max_exponent,
			numeric_limits<float>::min_exponent);
	printf("double : digits=%d max=%g lowest=%g min=%g eps=%g maxexp=%d\n",
			numeric_limits<double>::digits, numeric_limits<double>::max(),
			numeric_limits<double>::lowest(), numeric_limits<double>::min(),
			numeric_limits<double>::epsilon(), numeric_limits<double>::max_exponent);
	// infinity / NaN behaviour (deterministic)
	printf("special: inf>max=%d nan!=nan=%d digits10(d)=%d max_digits10(d)=%d\n",
			(int) (numeric_limits<double>::infinity() > numeric_limits<double>::max()),
			(int) (numeric_limits<double>::quiet_NaN() != numeric_limits<double>::quiet_NaN()),
			numeric_limits<double>::digits10, numeric_limits<double>::max_digits10);
}

} // namespace sprt::test
