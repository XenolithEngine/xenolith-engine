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

// <algorithm> and <iterator> conformance for the sprt-backed STL. On the
// x86_64-pc-windows-msvc and Linux/glibc sprt builds these are the freestanding
// runtime headers; under Makefile.system they resolve to the host libstdc++.
// compare.sh diffs the two sprt targets and Makefile.system supplies the
// libstdc++ reference. Output is deterministic (int/char, fixed sequences).

#include <stdio.h>

#include <algorithm>
#include <iterator>
#include <numeric>
#include <utility>
#include <vector>
#include <array>

namespace sprt::test {

namespace {

void printv(const char *label, const int *p, size_t n) {
	printf("%s=[", label);
	for (size_t i = 0; i < n; ++i) { printf("%s%d", i ? "," : "", p[i]); }
	printf("]\n");
}

template <typename C>
void printc(const char *label, const C &c) {
	printf("%s=[", label);
	bool first = true;
	for (auto v : c) {
		printf("%s%d", first ? "" : ",", (int) v);
		first = false;
	}
	printf("]\n");
}

// compile-time iterator-adapter and trait checks
static_assert(std::is_same_v<std::reverse_iterator<int *>::value_type, int>);
static_assert(std::is_same_v<std::iterator_traits<const char *>::reference, const char &>);
static_assert(std::is_same_v<std::iterator_traits<const char *>::pointer, const char *>);

} // namespace

void performAlgorithmTest() {
	// ---- filling / copying ------------------------------------------------
	int a[6];
	std::fill(a, a + 6, 7);
	printv("fill", a, 6);
	std::fill_n(a, 3, 2);
	printv("fill_n", a, 6);

	int src[5] = {1, 2, 3, 4, 5};
	int dst[5] = {0, 0, 0, 0, 0};
	std::copy_n(src, 5, dst);
	printv("copy_n", dst, 5);

	// ---- range move / move_backward (3-arg std::move) ---------------------
	int m1[5] = {10, 20, 30, 40, 50};
	int m2[5] = {0, 0, 0, 0, 0};
	std::move(m1, m1 + 5, m2);
	printv("move", m2, 5);
	int m3[6] = {1, 2, 3, 4, 5, 0};
	std::move_backward(m3, m3 + 5, m3 + 6);
	printv("move_backward", m3, 6);

	// ---- rotate / swap_ranges ---------------------------------------------
	int r[5] = {1, 2, 3, 4, 5};
	std::rotate(r, r + 2, r + 5);
	printv("rotate", r, 5);
	int s1[3] = {1, 2, 3};
	int s2[3] = {4, 5, 6};
	std::swap_ranges(s1, s1 + 3, s2);
	printv("swap_ranges.a", s1, 3);
	printv("swap_ranges.b", s2, 3);

	// ---- replace family ----------------------------------------------------
	int rp[6] = {1, 2, 1, 3, 1, 4};
	std::replace(rp, rp + 6, 1, 9);
	printv("replace", rp, 6);
	int ri[6] = {1, 2, 3, 4, 5, 6};
	std::replace_if(ri, ri + 6, [](int x) { return x % 2 == 0; }, 0);
	printv("replace_if", ri, 6);
	int rc[6] = {1, 2, 3, 2, 1, 2};
	int rco[6];
	std::replace_copy(rc, rc + 6, rco, 2, 8);
	printv("replace_copy", rco, 6);
	int rcio[6];
	std::replace_copy_if(rc, rc + 6, rcio, [](int x) { return x > 1; }, 0);
	printv("replace_copy_if", rcio, 6);

	// ---- clamp -------------------------------------------------------------
	printf("clamp=%d,%d,%d\n", std::clamp(5, 1, 10), std::clamp(-3, 1, 10),
			std::clamp(15, 1, 10));

	// ---- counting / searching ---------------------------------------------
	int c[8] = {1, 2, 2, 3, 3, 3, 4, 4};
	printf("count=%d count_if=%d\n", (int) std::count(c, c + 8, 3),
			(int) std::count_if(c, c + 8, [](int x) { return x > 2; }));
	printf("min_el=%d max_el=%d\n", *std::min_element(c, c + 8), *std::max_element(c, c + 8));
	auto mm = std::minmax_element(c, c + 8);
	printf("minmax=%d,%d\n", *mm.first, *mm.second);
	printf("binsearch: y=%d n=%d\n", (int) std::binary_search(c, c + 8, 3),
			(int) std::binary_search(c, c + 8, 9));
	printf("is_sorted=%d\n", (int) std::is_sorted(c, c + 8));

	// ---- for_each / transform ---------------------------------------------
	int sum = 0;
	std::for_each(c, c + 8, [&](int x) { sum += x; });
	printf("for_each_sum=%d\n", sum);
	int tr[5] = {1, 2, 3, 4, 5};
	int tro[5];
	std::transform(tr, tr + 5, tro, [](int x) { return x * x; });
	printv("transform", tro, 5);

	// ---- partition family --------------------------------------------------
	int pt[8] = {1, 2, 3, 4, 5, 6, 7, 8};
	auto pp = std::partition(pt, pt + 8, [](int x) { return x % 2 == 0; });
	printf("partition_point_at=%d\n", (int) (pp - pt));
	int sp[8] = {1, 2, 3, 4, 5, 6, 7, 8};
	auto spp = std::stable_partition(sp, sp + 8, [](int x) { return x % 2 == 0; });
	printv("stable_partition", sp, 8);
	printf("stable_partition_at=%d\n", (int) (spp - sp));
	int srt[6] = {2, 2, 4, 4, 6, 6};
	auto ppt = std::partition_point(srt, srt + 6, [](int x) { return x < 4; });
	printf("partition_point=%d\n", (int) (ppt - srt));

	// ---- unique ------------------------------------------------------------
	int uq[8] = {1, 1, 2, 3, 3, 3, 4, 4};
	auto ue = std::unique(uq, uq + 8);
	printv("unique", uq, (size_t) (ue - uq));
	int uc[8] = {1, 1, 2, 2, 2, 3, 4, 4};
	int uco[8];
	auto uce = std::unique_copy(uc, uc + 8, uco);
	printv("unique_copy", uco, (size_t) (uce - uco));

	// ---- stable_sort (stability: sort pairs by key, tie keeps input order) -
	std::pair<int, int> ps[6] = {{3, 0}, {1, 1}, {3, 2}, {1, 3}, {2, 4}, {1, 5}};
	std::stable_sort(ps, ps + 6, [](auto &l, auto &r) { return l.first < r.first; });
	printf("stable_sort=");
	for (auto &e : ps) { printf("(%d.%d)", e.first, e.second); }
	printf("\n");

	// ---- set operations ----------------------------------------------------
	int A[5] = {1, 2, 3, 4, 5};
	int B[3] = {2, 4, 6};
	int so[8];
	auto de = std::set_difference(A, A + 5, B, B + 3, so);
	printv("set_difference", so, (size_t) (de - so));
	auto ie = std::set_intersection(A, A + 5, B, B + 3, so);
	printv("set_intersection", so, (size_t) (ie - so));
	auto ne = std::set_union(A, A + 5, B, B + 3, so);
	printv("set_union", so, (size_t) (ne - so));
	printf("includes: y=%d n=%d\n", (int) std::includes(A, A + 5, B, B + 2),
			(int) std::includes(A, A + 5, B, B + 3));

	// ---- iterator adapters -------------------------------------------------
	int ra[5] = {1, 2, 3, 4, 5};
	printf("reverse=");
	for (auto it = std::make_reverse_iterator(ra + 5); it != std::make_reverse_iterator(ra); ++it) {
		printf("%d", *it);
	}
	printf("\n");

	std::vector<int> rv(std::rbegin(ra), std::rend(ra));
	printc("rbegin_rend", rv);

	// std::next / std::prev single-arg (prev requires a bidirectional iterator)
	std::vector<int> pn = {10, 20, 30, 40};
	printf("next_prev: n=%d p=%d n2=%d p2=%d\n", *std::next(pn.begin()), *std::prev(pn.end()),
			*std::next(pn.begin(), 2), *std::prev(pn.end(), 2));

	// back_inserter / inserter / front_inserter via move_iterator source
	std::vector<int> bi;
	std::copy(ra, ra + 5, std::back_inserter(bi));
	printc("back_inserter", bi);
	std::vector<int> ins = {1, 5};
	std::vector<int> mid = {2, 3, 4};
	std::copy(mid.begin(), mid.end(), std::inserter(ins, ins.begin() + 1));
	printc("inserter", ins);

	// move_iterator: move strings out of a vector, leaving them empty
	std::vector<int> mv = {9, 8, 7};
	std::vector<int> mvout(std::make_move_iterator(mv.begin()), std::make_move_iterator(mv.end()));
	printc("move_iterator", mvout);

	// ---- adjacent_find (default equality + binary predicate) --------------
	int af[7] = {1, 3, 3, 5, 7, 7, 9};
	auto af1 = std::adjacent_find(af, af + 7);
	printf("adjacent_find=%d\n", af1 == af + 7 ? -1 : (int) (af1 - af));
	auto af2 = std::adjacent_find(af, af + 7, [](int x, int y) { return y == x + 2; });
	printf("adjacent_find_pred=%d\n", af2 == af + 7 ? -1 : (int) (af2 - af));
	int afn[4] = {1, 2, 3, 4};
	printf("adjacent_find_none=%d\n", (int) (std::adjacent_find(afn, afn + 4) == afn + 4));

	// ---- equal (two-range 5-arg form, C++14) ------------------------------
	int e1[4] = {1, 2, 3, 4};
	int e2[4] = {1, 2, 3, 4};
	int e3[3] = {1, 2, 3};
	auto eqp = [](int x, int y) { return x == y; };
	printf("equal2: same=%d shorter2nd=%d shorter1st=%d\n",
			(int) std::equal(e1, e1 + 4, e2, e2 + 4, eqp),
			(int) std::equal(e1, e1 + 4, e3, e3 + 3, eqp),
			(int) std::equal(e3, e3 + 3, e1, e1 + 4, eqp));

	// ---- lexicographical_compare (5-arg with custom comparator) -----------
	int lc1[3] = {1, 2, 3};
	int lc2[3] = {1, 2, 4};
	printf("lexcmp: less=%d rev=%d eqpfx=%d\n",
			(int) std::lexicographical_compare(lc1, lc1 + 3, lc2, lc2 + 3,
					[](int x, int y) { return x < y; }),
			(int) std::lexicographical_compare(lc1, lc1 + 3, lc2, lc2 + 3,
					[](int x, int y) { return x > y; }),
			(int) std::lexicographical_compare(lc1, lc1 + 2, lc2, lc2 + 3,
					[](int x, int y) { return x < y; }));

	// ---- <numeric> accumulate/iota (surfaced neighbours) ------------------
	int ac[5] = {1, 2, 3, 4, 5};
	printf("accumulate=%d\n", std::accumulate(ac, ac + 5, 0));
}

} // namespace sprt::test
