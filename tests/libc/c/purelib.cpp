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

// Pure-library conformance: <numeric>, <span>, <functional> (arithmetic/logical/bitwise
// objects + bind_front/bind_back/mem_fn/not_fn), <charconv> (integer), and the <compare>
// order CPOs. On x86_64-pc-windows-msvc these are the sprt implementations; on the Linux
// host they are libstdc++. compare.sh diffs the two. errc is compared by name (never printed
// as a raw value, which is platform-dependent).

#include <stdio.h>

#include <numeric>
#include <span>
#include <functional>
#include <charconv>
#include <compare>

namespace sprt::test {

namespace {

template <class _Ord>
int osgn(_Ord o) noexcept {
	return o < 0 ? -1 : (o > 0 ? 1 : 0);
}

struct Point {
	int x;
	int scaled(int k) const { return x * k; }
};

} // namespace

void performPureLibTest() {
	// ---- <numeric> ----
	int a[5] = {1, 2, 3, 4, 5};
	printf("acc=%d reduce=%d inner=%d\n", std::accumulate(a, a + 5, 0),
			std::reduce(a, a + 5, 100), std::inner_product(a, a + 5, a, 0));
	printf("treduce=%d\n", std::transform_reduce(a, a + 5, a, 0));
	int ps[5], ad[5];
	std::partial_sum(a, a + 5, ps);
	std::adjacent_difference(a, a + 5, ad);
	printf("partial_sum=%d,%d,%d,%d,%d adj_diff=%d,%d,%d,%d,%d\n", ps[0], ps[1], ps[2], ps[3],
			ps[4], ad[0], ad[1], ad[2], ad[3], ad[4]);
	int it[4];
	std::iota(it, it + 4, 7);
	printf("iota=%d,%d,%d,%d\n", it[0], it[1], it[2], it[3]);
	printf("gcd=%d lcm=%d midpoint_i=%d midpoint_neg=%d\n", std::gcd(48, 36), std::lcm(4, 6),
			std::midpoint(10, 20), std::midpoint(-7, 7));

	// ---- <functional> operations ----
	printf("ops: plus=%d minus=%d mul=%d div=%d mod=%d neg=%d\n", std::plus<int> {}(3, 4),
			std::minus<> {}(10, 3), std::multiplies<int> {}(6, 7), std::divides<> {}(20, 4),
			std::modulus<int> {}(17, 5), std::negate<> {}(9));
	printf("bits: and=%d or=%d xor=%d not=%d land=%d lor=%d lnot=%d\n", std::bit_and<int> {}(6, 3),
			std::bit_or<int> {}(6, 1), std::bit_xor<int> {}(6, 3),
			(int) (unsigned char) std::bit_not<unsigned char> {}(0x0F),
			(int) std::logical_and<> {}(true, false), (int) std::logical_or<> {}(false, true),
			(int) std::logical_not<> {}(false));

	// ---- bind_front / bind_back / mem_fn / not_fn ----
	// std::bind_back is C++23 (__cpp_lib_bind_back); gate the call on the library
	// feature-test macro so this suite still builds against a pre-C++23 standard
	// library. The printed value is computed identically in both branches, so the
	// cross-target diff is unaffected.
	auto sub = [](int x, int y, int z) { return x - y - z; };
	auto bf = std::bind_front(sub, 100);
#if defined(__cpp_lib_bind_back)
	int backResult = std::bind_back(sub, 1, 2)(50);
#else
	int backResult = sub(50, 1, 2);
#endif
	printf("bind: front=%d back=%d\n", bf(10, 5), backResult);
	Point pt {21};
	auto m = std::mem_fn(&Point::scaled);
	printf("mem_fn=%d not_fn=%d\n", m(pt, 2),
			(int) std::not_fn([](int v) { return v < 0; })(5));

	// ---- <span> ----
	std::span<int> sp(a, 5);
	std::span<int, 5> sf(a);
	printf("span: size=%d extent=%d front=%d back=%d [2]=%d bytes=%d empty=%d\n", (int) sp.size(),
			(int) sf.extent, sp.front(), sp.back(), sp[2], (int) sp.size_bytes(), (int) sp.empty());
	auto s2 = sp.subspan(1, 3);
	auto f2 = sp.first(2);
	auto l2 = sp.last(2);
	printf("subspan: sub=%d,%d,%d first=%d,%d last=%d,%d\n", s2[0], s2[1], s2[2], f2[0], f2[1],
			l2[0], l2[1]);
	int rsum = 0;
	for (auto it2 = sp.rbegin(); it2 != sp.rend(); ++it2) { rsum = rsum * 10 + *it2; }
	printf("span_reverse=%d\n", rsum);

	// ---- <charconv> (integer) ----
	char buf[32];
	auto r10 = std::to_chars(buf, buf + 32, 305419896);
	*r10.ptr = '\0';
	printf("to_chars dec: %s ok=%d\n", buf, (int) (r10.ec == std::errc {}));
	auto r16 = std::to_chars(buf, buf + 32, 305419896, 16);
	*r16.ptr = '\0';
	printf("to_chars hex: %s\n", buf);
	auto rneg = std::to_chars(buf, buf + 32, -42);
	*rneg.ptr = '\0';
	printf("to_chars neg: %s\n", buf);
	int v = 0;
	auto fr = std::from_chars(buf, rneg.ptr, v);
	printf("from_chars: v=%d consumed_all=%d\n", v, (int) (fr.ptr == rneg.ptr));
	int vb = 0;
	const char *bad = "xyz";
	auto frbad = std::from_chars(bad, bad + 3, vb);
	printf("from_chars invalid: is_invalid=%d\n", (int) (frbad.ec == std::errc::invalid_argument));
	long long huge = 0;
	const char *big = "999999999999999999999999";
	auto frov = std::from_chars(big, big + 24, huge);
	printf("from_chars overflow: is_range=%d\n",
			(int) (frov.ec == std::errc::result_out_of_range));
	char tiny[2];
	auto frsmall = std::to_chars(tiny, tiny + 2, 12345);
	printf("to_chars small_buf: is_toolarge=%d\n",
			(int) (frsmall.ec == std::errc::value_too_large));

	// ---- <charconv> (floating point, via sprt::dtoa / __sprt_strtod) ----
	// Use exactly-representable values so host and freestanding never diverge on rounding.
	char fbuf[40];
	auto rf = std::to_chars(fbuf, fbuf + 40, 0.5);
	*rf.ptr = '\0';
	double back = 0.0;
	auto rb = std::from_chars(fbuf, rf.ptr, back);
	printf("to_chars float: %s roundtrip=%d\n", fbuf,
			(int) (rf.ec == std::errc {} && rb.ec == std::errc {} && rb.ptr == rf.ptr
					&& back == 0.5));
	double parsed = 0.0;
	const char *fs = "2.5";
	auto ffr = std::from_chars(fs, fs + 3, parsed);
	printf("from_chars float: ok=%d exact=%d\n", (int) (ffr.ec == std::errc {}),
			(int) (parsed == 2.5));
	double bad_f = 1.0;
	const char *nf = "abc";
	auto nfr = std::from_chars(nf, nf + 3, bad_f);
	printf("from_chars float invalid: is_invalid=%d\n",
			(int) (nfr.ec == std::errc::invalid_argument));
	char nzbuf[8];
	auto nz = std::to_chars(nzbuf, nzbuf + 8, -0.0);
	*nz.ptr = '\0';
	printf("to_chars neg_zero: %s\n", nzbuf); // dtoa now renders negative zero with its sign

	// ---- <compare> order CPOs ----
	printf("order: s12=%d w12=%d p12=%d\n", osgn(std::strong_order(1.0, 2.0)),
			osgn(std::weak_order(1, 2)), osgn(std::partial_order(1.0, 2.0)));
	// IEEE totalOrder: -0.0 sorts before +0.0 for strong_order, equivalent for weak_order
	printf("order_zero: strong_neg0_lt=%d weak_neg0_eq=%d\n",
			osgn(std::strong_order(-0.0, 0.0)), osgn(std::weak_order(-0.0, 0.0)));
}

} // namespace sprt::test
