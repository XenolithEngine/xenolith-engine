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

// std::tuple conformance. On the freestanding x86_64-pc-windows-msvc build <tuple>,
// the pair tuple protocol in <utility> and the array tuple protocol in <array> are
// the sprt-backed STL headers; on the Linux host they are the system headers.
// compare.sh diffs the two, so the sprt implementation is checked against the system
// reference. Only width-stable element types (int/double/char) are used.

#include <stdio.h>

#include <tuple>
#include <utility>
#include <array>
#include <type_traits>
#include <compare>

namespace sprt::test {

namespace {

using std::tuple;
using std::get;
using std::make_tuple;

// compile-time protocol checks (validated on both targets)
static_assert(std::tuple_size_v<tuple<int, double, char>> == 3);
static_assert(std::is_same_v<std::tuple_element_t<1, tuple<int, double, char>>, double>);
static_assert(std::tuple_size_v<std::pair<int, char>> == 2);
static_assert(std::is_same_v<std::tuple_element_t<0, std::pair<int, char>>, int>);
static_assert(std::tuple_size_v<std::array<int, 4>> == 4);
static_assert(std::is_same_v<std::tuple_element_t<2, std::array<int, 4>>, int>);

constexpr int __sum3(int a, int b, int c) { return a + b + c; }

} // namespace

void performTupleTest() {
	// get<I> / get<T> / mutate through reference
	tuple<int, double, char> t(1, 2.5, 'z');
	get<0>(t) = 7;
	printf("get: i=%d d=%g c=%c byType=%g\n", get<0>(t), get<1>(t), get<2>(t), get<double>(t));

	// make_tuple + comparisons
	auto a = make_tuple(1, 2);
	auto b = make_tuple(1, 3);
	printf("cmp: lt=%d eq=%d ne=%d gt=%d ge=%d\n", (int) (a < b), (int) (a == b), (int) (a != b),
			(int) (b > a), (int) (a >= a));

	// three-way comparison (C++20): <=> plus the rewritten relational operators
	printf("ship: ltz=%d eqz=%d gtz=%d strong=%d\n", (int) ((a <=> b) < 0), (int) ((a <=> a) == 0),
			(int) ((b <=> a) > 0),
			(int) std::is_same_v<decltype(a <=> b), std::strong_ordering>);

	// converting construction (tuple<long-ish via int widening kept stable) + from pair
	tuple<int, double> c = make_tuple(4, 5.0);
	std::pair<int, char> p {8, 'Q'};
	tuple<int, char> tp = p;
	printf("conv: c0=%d c1=%g fromPair=%d,%c\n", get<0>(c), get<1>(c), get<0>(tp), get<1>(tp));

	// tie + ignore
	int x = 0;
	double y = 0;
	std::tie(x, std::ignore, y) = make_tuple(10, 20, 2.5);
	printf("tie: x=%d y=%g\n", x, y);

	// tuple_cat
	auto cat = std::tuple_cat(make_tuple(1, 2), make_tuple(3.0), make_tuple('w'));
	printf("cat: size=%d v0=%d v2=%g v3=%c\n", (int) std::tuple_size_v<decltype(cat)>, get<0>(cat),
			get<2>(cat), get<3>(cat));

	// apply + make_from_tuple
	printf("apply: sum=%d\n", std::apply(__sum3, make_tuple(1, 2, 3)));

	// swap
	auto u = make_tuple(1, 2);
	auto v = make_tuple(3, 4);
	std::swap(u, v);
	printf("swap: u0=%d v0=%d\n", get<0>(u), get<0>(v));

	// structured bindings: tuple, pair, array
	auto [bi, bd, bc] = t;
	printf("bind tuple: %d %g %c\n", bi, bd, bc);

	std::pair<int, char> pr {5, 'A'};
	auto &[pf, ps] = pr;
	pf = 6;
	printf("bind pair: first=%d second=%c (orig.first=%d)\n", pf, ps, pr.first);

	std::array<int, 3> arr {10, 20, 30};
	auto &[a0, a1, a2] = arr;
	a1 = 99;
	printf("bind array: %d %d %d (orig[1]=%d) get2=%d\n", a0, a1, a2, arr[1], get<2>(arr));

	// to_array
	auto ta = std::to_array({11, 22, 33, 44});
	printf("to_array: size=%d last=%d\n", (int) std::tuple_size_v<decltype(ta)>, ta[3]);
}

} // namespace sprt::test
