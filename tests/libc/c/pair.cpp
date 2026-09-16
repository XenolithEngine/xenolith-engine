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

// std::pair conformance, via the <utility> STL wrapper. On the freestanding
// x86_64-pc-windows-msvc build std::pair is sprt::pair; on the Linux host it is the
// system pair. compare.sh diffs the two, so sprt::pair is validated against libstdc++.
// Exercises the standard-conformance features refactored in: the default constructor,
// piecewise_construct (which replaced the sprt-only pair_emplace_construct_t), the
// three-way comparison, non-member swap, make_pair, and the tuple protocol
// (tuple_size / tuple_element / get / structured bindings). Output is deterministic.

#include <stdio.h>

#include <utility>
#include <tuple>
#include <string>
#include <type_traits>

namespace sprt::test {

namespace {

static int sgn(int v) noexcept { return v < 0 ? -1 : (v > 0 ? 1 : 0); }

template <class _Ord>
static int osgn(_Ord o) noexcept {
	return o < 0 ? -1 : (o > 0 ? 1 : 0);
}

// Standard member typedefs + tuple protocol, validated on both targets.
static_assert(std::is_same_v<std::pair<int, char>::first_type, int>);
static_assert(std::is_same_v<std::pair<int, char>::second_type, char>);
static_assert(std::tuple_size_v<std::pair<int, char>> == 2);
static_assert(std::is_same_v<std::tuple_element_t<0, std::pair<int, char>>, int>);
static_assert(std::is_same_v<std::tuple_element_t<1, std::pair<int, char>>, char>);
static_assert(std::is_default_constructible_v<std::pair<int, long>>);

struct NoDefault {
	NoDefault() = delete;
	NoDefault(int) { }
};
static_assert(!std::is_default_constructible_v<std::pair<NoDefault, int>>);

} // namespace

void performPairTest() {
	// default constructor (value-initialises both elements)
	std::pair<int, long> d {};
	printf("default: %d %ld\n", d.first, (long) d.second);

	// value constructor + make_pair
	std::pair<int, long> p {7, 42L};
	auto mp = std::make_pair(3, 9L);
	printf("value: %d %ld | make_pair: %d %ld\n", p.first, (long) p.second, mp.first,
			(long) mp.second);

	// converting constructor (int,int) -> (long,double)
	std::pair<long, double> cv = std::pair<int, int> {5, 6};
	printf("convert: %ld %.1f\n", cv.first, cv.second);

	// piecewise_construct — first from a 2-arg tuple, second from a 1-arg tuple
	std::pair<std::pair<int, int>, std::string> pw(std::piecewise_construct,
			std::forward_as_tuple(11, 22), std::forward_as_tuple("piece"));
	printf("piecewise: (%d,%d) %s\n", pw.first.first, pw.first.second, pw.second.c_str());

	// comparisons: ==, !=, <, >, <=, >=, and <=>
	std::pair<int, int> a {1, 2}, b {1, 3}, c {1, 2};
	printf("cmp: eq=%d ne=%d lt=%d gt=%d le=%d ge=%d 3way=%d\n", (int) (a == c), (int) (a != b),
			(int) (a < b), (int) (b > a), (int) (a <= c), (int) (a >= c), osgn(a <=> b));

	// non-member swap
	std::pair<int, int> s1 {1, 2}, s2 {3, 4};
	std::swap(s1, s2);
	printf("swap: (%d,%d) (%d,%d)\n", s1.first, s1.second, s2.first, s2.second);

	// get<> + structured bindings
	std::pair<int, char> g {8, 'Z'};
	printf("get: %d %c\n", std::get<0>(g), std::get<1>(g));
	auto [gi, gc] = g;
	printf("bind: %d %c\n", gi, gc);

	// nested pair-in-pair via make_pair
	auto np = std::make_pair(std::make_pair(1, 2), 3);
	printf("nested: (%d,%d) %d\n", np.first.first, np.first.second, np.second);

	(void) sgn;
}

} // namespace sprt::test
