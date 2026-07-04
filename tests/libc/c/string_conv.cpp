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

// std::to_string / to_wstring / stoi-family / operator""s conformance, via the <string>
// STL wrapper. On the freestanding x86_64-pc-windows-msvc build these are the sprt
// implementations (to_string over snprintf, stoi over the strtol family); on the Linux
// host they are libstdc++. compare.sh diffs the two, so the sprt forms are validated
// against libstdc++ (this also confirms snprintf(nullptr,0,...) sizing and %f byte-parity).
// Output is deterministic (fixed formats, exactly-representable float values).

#include <stdio.h>

#include <string>

namespace sprt::test {

void performStringConvTest() {
	// to_string, integer overloads (incl. INT_MIN, ULLONG_MAX)
	printf("ts_int: %s %s %s\n", std::to_string(0).c_str(), std::to_string(42).c_str(),
			std::to_string(-2147483647 - 1).c_str());
	printf("ts_long: %s %s\n", std::to_string(9000000000L).c_str(),
			std::to_string(-9000000000L).c_str());
	printf("ts_uns: %s %s\n", std::to_string(4000000000U).c_str(),
			std::to_string(18446744073709551615ULL).c_str());

	// to_string, floating overloads ("%f" -> 6 decimals; "%Lf" for long double)
	printf("ts_flt: %s %s %s %s\n", std::to_string(3.5).c_str(), std::to_string(-0.25).c_str(),
			std::to_string(2.0f).c_str(), std::to_string(1.5L).c_str());

	// to_wstring: same ASCII digits, widened. Narrow each element back for printing.
	std::wstring w = std::to_wstring(-12345);
	printf("to_wstring: len=%d text=", (int) w.size());
	for (wchar_t c : w) { putchar((int) (char) c); }
	putchar('\n');

	// stoi family
	printf("stoi: %d %ld %lu %lld %llu\n", std::stoi("42"), std::stol("-100"),
			std::stoul("4000000000"), std::stoll("-9000000000"),
			std::stoull("18446744073709551615"));

	// base + pos out-parameter
	size_t pos = 0;
	int hx = std::stoi("ff zzz", &pos, 16);
	printf("stoi_base16: %d pos=%d\n", hx, (int) pos);
	size_t p2 = 0;
	long v = std::stol("  -123abc", &p2);
	printf("stol_pos: %ld pos=%d\n", v, (int) p2);

	// floating parses (print via a fixed format so both targets format identically)
	printf("stod: %.4f %.4f %.4f\n", std::stod("2.5"), (double) std::stof("1.25"),
			(double) std::stold("3.75"));

	// operator""s
	using namespace std::string_literals;
	auto s = "hello"s;
	printf("literal_s: %s size=%d\n", s.c_str(), (int) s.size());

	// round trip
	printf("roundtrip: %d\n", std::stoi(std::to_string(123456)));
}

} // namespace sprt::test
