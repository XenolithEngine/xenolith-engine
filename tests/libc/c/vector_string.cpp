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

// std::vector / std::string conformance, via the <vector>/<string> STL wrappers. On the
// freestanding x86_64-pc-windows-msvc build these are sprt::__vector / sprt::__basic_string
// backed by the (now full sprt-allocator-policy) std::allocator; on the Linux host they are
// the system containers. compare.sh diffs the two, so the sprt containers are validated
// against libstdc++. Output is deterministic (no sizeof/capacity/addresses). The custom
// char-traits section proves Traits is a real template parameter that governs comparison
// and search identically on both implementations.

#include <stdio.h>

#include <vector>
#include <string>
#include <compare>
#include <type_traits>

namespace sprt::test {

namespace {

static int sgn(int v) noexcept { return v < 0 ? -1 : (v > 0 ? 1 : 0); }

template <class _Ord>
static int osgn(_Ord o) noexcept {
	return o < 0 ? -1 : (o > 0 ? 1 : 0);
}

// Case-insensitive char traits: proves basic_string honours a user Traits parameter.
// Both libstdc++ and sprt route comparison/search through it, so the result is identical.
struct ci_traits : std::char_traits<char> {
	static char lower(char __c) noexcept {
		return (__c >= 'A' && __c <= 'Z') ? char(__c - 'A' + 'a') : __c;
	}
	static bool eq(char __a, char __b) noexcept { return lower(__a) == lower(__b); }
	static bool lt(char __a, char __b) noexcept {
		return static_cast<unsigned char>(lower(__a)) < static_cast<unsigned char>(lower(__b));
	}
	static int compare(const char *__s1, const char *__s2, size_t __n) {
		for (size_t __i = 0; __i < __n; ++__i) {
			if (lt(__s1[__i], __s2[__i])) {
				return -1;
			}
			if (lt(__s2[__i], __s1[__i])) {
				return 1;
			}
		}
		return 0;
	}
	static const char *find(const char *__s, size_t __n, char __a) {
		for (size_t __i = 0; __i < __n; ++__i) {
			if (eq(__s[__i], __a)) {
				return __s + __i;
			}
		}
		return nullptr;
	}
};
using ci_string = std::basic_string<char, ci_traits>;

// Standard member typedefs, validated on both targets.
static_assert(std::is_same_v<std::string::value_type, char>);
static_assert(std::is_same_v<std::string::traits_type, std::char_traits<char>>);
static_assert(std::is_same_v<std::string::allocator_type, std::allocator<char>>);
static_assert(std::is_same_v<std::vector<int>::value_type, int>);
static_assert(std::is_same_v<std::vector<int>::allocator_type, std::allocator<int>>);
static_assert(std::is_same_v<ci_string::traits_type, ci_traits>);

} // namespace

void performVectorTest() {
	std::vector<int> v;
	for (int i = 1; i <= 5; ++i) { v.push_back(i * 10); }
	printf("vec size=%d empty=%d\n", (int)v.size(), (int)v.empty());
	printf("vec idx: [0]=%d [4]=%d at2=%d front=%d back=%d\n", v[0], v[4], v.at(2), v.front(),
			v.back());
	int sum = 0;
	for (int x : v) { sum += x; }
	printf("vec sum=%d\n", sum);

	// insert / erase(iterator)
	v.insert(v.begin() + 2, 999);
	printf("insert: size=%d [2]=%d\n", (int)v.size(), v[2]);
	v.erase(v.begin() + 2);
	printf("erase_it: size=%d [2]=%d\n", (int)v.size(), v[2]);

	// std::erase / std::erase_if free functions (return removed count)
	std::vector<int> w {1, 2, 2, 3, 2, 4};
	auto n1 = std::erase(w, 2);
	printf("erase(2): removed=%d size=%d\n", (int)n1, (int)w.size());
	auto n2 = std::erase_if(w, [](int x) { return x % 2 == 0; });
	int ws = 0;
	for (int x : w) { ws += x; }
	printf("erase_if(even): removed=%d size=%d sum=%d\n", (int)n2, (int)w.size(), ws);

	// resize / clear
	std::vector<int> r(3, 7);
	r.resize(5, 9);
	printf("resize: size=%d [2]=%d [4]=%d\n", (int)r.size(), r[2], r[4]);
	r.resize(2);
	printf("shrink: size=%d [1]=%d\n", (int)r.size(), r[1]);
	r.clear();
	printf("clear: size=%d empty=%d\n", (int)r.size(), (int)r.empty());

	// comparisons ==, !=, <=> (relationals rewritten from <=> in C++20)
	std::vector<int> a {1, 2, 3}, b {1, 2, 3}, c {1, 2, 4};
	printf("cmp: eq=%d ne=%d lt=%d 3way=%d\n", (int)(a == b), (int)(a != c), (int)(a < c),
			osgn(a <=> c));

	// nested container: vector<string>
	std::vector<std::string> vs;
	vs.push_back("alpha");
	vs.push_back("beta");
	vs.emplace_back("gamma");
	printf("vec<str>: size=%d [0]=%s [2]=%s\n", (int)vs.size(), vs[0].c_str(), vs[2].c_str());
}

void performStringTest() {
	std::string s = "Hello";
	printf("str: %s size=%d empty=%d\n", s.c_str(), (int)s.size(), (int)s.empty());
	printf("idx: [0]=%c at4=%c front=%c back=%c\n", s[0], s.at(4), s.front(), s.back());

	s += ", ";
	s.append("World");
	printf("append: %s size=%d\n", s.c_str(), (int)s.size());

	std::string a = "foo", b = "bar";
	printf("concat: %s\n", (a + b).c_str());
	printf("concat_cstr: %s / %s\n", (a + "X").c_str(), ("Y" + b).c_str());

	// comparisons: ==, !=, <, >, compare, <=>  (relationals rewritten from <=>)
	std::string p = "abc", q = "abd", r = "abc";
	printf("cmp: eq=%d ne=%d lt=%d gt=%d cmp=%d 3way=%d\n", (int)(p == r), (int)(p != q),
			(int)(p < q), (int)(q > p), sgn(p.compare(q)), osgn(p <=> q));
	printf("cmp_cstr: eq=%d lt=%d\n", (int)(p == "abc"), (int)(p < "abd"));

	// search
	std::string h = "the quick brown fox";
	long z = (h.find("zzz") == std::string::npos) ? -1 : (long)h.find("zzz");
	printf("find: q=%d miss=%ld rfind_o=%d first_of=%d first_not_of=%d\n", (int)h.find("quick"),
			z, (int)h.rfind('o'), (int)h.find_first_of("xoq"), (int)h.find_first_not_of("teh "));

	// substr / replace / insert / erase
	printf("substr: %s\n", h.substr(4, 5).c_str());
	std::string m = "aaa bbb ccc";
	m.replace(4, 3, "XYZ");
	printf("replace: %s\n", m.c_str());
	m.insert(0, ">> ");
	printf("insert: %s\n", m.c_str());
	m.erase(0, 3);
	printf("erase: %s\n", m.c_str());

	// Custom Traits (case-insensitive) — proves Traits governs comparison + search.
	ci_string ca = "HELLO", cb = "hello", cc = "World";
	printf("ci: eq=%d ne=%d lt=%d cmp=%d find_l=%d\n", (int)(ca == cb), (int)(ca != cc),
			(int)(ca < cc), sgn(ca.compare(cb)), (int)ca.find('l'));
}

} // namespace sprt::test
