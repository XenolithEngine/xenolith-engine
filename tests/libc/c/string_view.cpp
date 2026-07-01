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

// std::string_view conformance, via the <string_view> STL wrapper. On the freestanding
// x86_64-pc-windows-msvc build this is sprt::basic_string_view; on the Linux host it is the
// system view. compare.sh diffs the two, so sprt::basic_string_view is validated against
// libstdc++. Also exercises the std::string <-> std::string_view interop and the C++20/23
// string members (starts_with/ends_with/contains). Output is deterministic; find results
// are normalised to -1 for npos so the exact npos value never appears.

#include <stdio.h>

#include <string_view>
#include <string>
#include <compare>
#include <type_traits>

namespace sprt::test {

namespace {

static long np(std::string_view::size_type v) noexcept {
	return v == std::string_view::npos ? -1 : (long) v;
}

template <class _Ord>
static int osgn(_Ord o) noexcept {
	return o < 0 ? -1 : (o > 0 ? 1 : 0);
}

static int sgn(int v) noexcept {
	return v < 0 ? -1 : (v > 0 ? 1 : 0);
}

static_assert(std::is_same_v<std::string_view::value_type, char>);
static_assert(std::is_same_v<std::string_view::traits_type, std::char_traits<char>>);
// constexpr conformance (also proves char_traits<char>::find is constexpr-usable now)
static_assert(std::string_view("hello world").starts_with("hello"));
static_assert(std::string_view("hello world").find_first_of("xyzw") == 6);
static_assert(std::string_view("abcabc").rfind("bc") == 4);
static_assert((std::string_view("abc") <=> std::string_view("abd")) < 0);

} // namespace

void performStringViewTest() {
	std::string_view v = "the quick brown fox";
	printf("basics: size=%d empty=%d [4]=%c front=%c back=%c\n", (int) v.size(), (int) v.empty(),
			v[4], v.front(), v.back());

	// substr / remove_prefix / remove_suffix
	printf("substr: %.*s\n", (int) v.substr(4, 5).size(), v.substr(4, 5).data());
	std::string_view t = v;
	t.remove_prefix(4);
	t.remove_suffix(4);
	printf("trim: %.*s\n", (int) t.size(), t.data());

	// comparisons (== != < > <=> against sv and literal)
	std::string_view a = "abc", b = "abd", c = "abc";
	printf("cmp: eq=%d ne=%d lt=%d gt=%d 3way=%d cmp=%d lit=%d\n", (int) (a == c), (int) (a != b),
			(int) (a < b), (int) (b > a), osgn(a <=> b), sgn(a.compare(b)), (int) (a == "abc"));

	// starts_with / ends_with (view, char, const char*). NB: contains() is C++23 — the
	// host libstdc++ reference is built as C++20, so it is exercised only in the direct
	// compile probes, not here (compare.sh compiles one source for both targets).
	printf("affix: sw=%d sw_c=%d ew=%d ew_c=%d\n", (int) v.starts_with("the"),
			(int) v.starts_with('t'), (int) v.ends_with("fox"), (int) v.ends_with('x'));

	// search family (normalised via np())
	printf("find: q=%ld miss=%ld rfind_o=%ld first_of=%ld last_of=%ld first_not=%ld last_not=%ld\n",
			np(v.find("quick")), np(v.find("zzz")), np(v.rfind('o')), np(v.find_first_of("xoq")),
			np(v.find_last_of("aeiou")), np(v.find_first_not_of("the ")),
			np(v.find_last_not_of("xof ")));

	// iterate (forward sum, reverse sum) — deterministic integer
	int fs = 0;
	for (char ch : v) { fs += (unsigned char) ch; }
	int rs = 0;
	for (auto it = v.rbegin(); it != v.rend(); ++it) { rs += (unsigned char) *it; }
	printf("iterate: fwd=%d rev=%d equal=%d\n", fs, rs, (int) (fs == rs));

	// std::string <-> std::string_view interop
	std::string s = "hello world";
	std::string_view sv = s; // implicit string -> view
	std::string s2(sv); // explicit view -> string
	std::string s3(sv, 6, 5); // view-substring ctor
	printf("interop: view=%.*s from_view=%s substr=%s eq=%d\n", (int) sv.size(), sv.data(),
			s2.c_str(), s3.c_str(), (int) (s == sv));

	// string's own C++20 starts_with/ends_with (contains() is C++23; see note above)
	printf("str_affix: sw=%d ew=%d sw_c=%d ew_c=%d\n", (int) s.starts_with("hello"),
			(int) s.ends_with("world"), (int) s.starts_with('h'), (int) s.ends_with('d'));

	// sv literal
	using namespace std::string_view_literals;
	auto lit = "literal"sv;
	printf("literal: %.*s size=%d\n", (int) lit.size(), lit.data(), (int) lit.size());
}

} // namespace sprt::test
