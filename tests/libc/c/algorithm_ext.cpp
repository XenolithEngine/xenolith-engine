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

// <algorithm> additions that landed in this changeset: the heap family
// (make/push/pop/sort_heap, is_heap[_until]), merge / inplace_merge,
// is_permutation, search / search_n, nth_element, generate / generate_n,
// copy_backward, minmax (two-arg + initializer_list), the comparator overloads
// of set_intersection / set_union, and the 4-iterator equal(). Output is
// deterministic so the sprt Linux/glibc and x86_64-pc-windows-msvc runs diff
// identically; Makefile.system supplies the libstdc++ reference.

#include <stdio.h>

#include <algorithm>
#include <functional>
#include <random>
#include <vector>

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

} // namespace

void performAlgorithmExtTest() {
	// ---- heap family -------------------------------------------------------
	int h[7] = {3, 1, 4, 1, 5, 9, 2};
	std::make_heap(h, h + 7);
	printf("is_heap=%d root=%d\n", (int) std::is_heap(h, h + 7), h[0]);

	// push a larger element into the heap
	int hp[8] = {3, 1, 4, 1, 5, 9, 2, 0};
	std::make_heap(hp, hp + 7);
	hp[7] = 8;
	std::push_heap(hp, hp + 8);
	printf("push_heap: is_heap=%d root=%d\n", (int) std::is_heap(hp, hp + 8), hp[0]);

	// pop moves the max to the back
	std::pop_heap(hp, hp + 8);
	printf("pop_heap: back=%d is_heap_rest=%d\n", hp[7], (int) std::is_heap(hp, hp + 7));

	// sort_heap turns a heap into an ascending run
	int hs[7] = {3, 1, 4, 1, 5, 9, 2};
	std::make_heap(hs, hs + 7);
	std::sort_heap(hs, hs + 7);
	printv("sort_heap", hs, 7);

	// is_heap_until on a partially-heaped range
	int hu[6] = {9, 5, 4, 1, 1, 8};
	printf("is_heap_until=%d\n", (int) (std::is_heap_until(hu, hu + 6) - hu));

	// min-heap via greater<>
	int hg[5] = {5, 2, 8, 1, 9};
	std::make_heap(hg, hg + 5, std::greater<int>());
	printf("minheap_root=%d is_heap=%d\n", hg[0],
			(int) std::is_heap(hg, hg + 5, std::greater<int>()));

	// ---- merge / inplace_merge --------------------------------------------
	int ma[4] = {1, 3, 5, 7};
	int mb[4] = {2, 4, 6, 8};
	int mo[8];
	std::merge(ma, ma + 4, mb, mb + 4, mo);
	printv("merge", mo, 8);

	// merge with a comparator over descending inputs
	int da[3] = {5, 3, 1};
	int db[3] = {6, 4, 2};
	int dmo[6];
	std::merge(da, da + 3, db, db + 3, dmo, std::greater<int>());
	printv("merge_greater", dmo, 6);

	// inplace_merge of two consecutive sorted runs
	int im[7] = {1, 4, 6, 2, 3, 5, 7};
	std::inplace_merge(im, im + 3, im + 7);
	printv("inplace_merge", im, 7);

	// ---- is_permutation ----------------------------------------------------
	int pa[5] = {1, 2, 3, 4, 5};
	int pb[5] = {3, 5, 1, 4, 2};
	int pc[5] = {1, 2, 3, 4, 6};
	printf("is_permutation: yes=%d no=%d\n", (int) std::is_permutation(pa, pa + 5, pb),
			(int) std::is_permutation(pa, pa + 5, pc));

	// ---- search / search_n -------------------------------------------------
	int hay[10] = {1, 2, 3, 4, 2, 3, 5, 2, 3, 4};
	int need[2] = {2, 3};
	printf("search=%d\n", (int) (std::search(hay, hay + 10, need, need + 2) - hay));
	int miss[2] = {9, 9};
	printf("search_miss=%d\n",
			(int) (std::search(hay, hay + 10, miss, miss + 2) == hay + 10));
	int run[8] = {1, 4, 4, 4, 2, 4, 4, 3};
	printf("search_n=%d\n", (int) (std::search_n(run, run + 8, 3, 4) - run));

	// ---- nth_element -------------------------------------------------------
	int ne[7] = {7, 2, 5, 1, 6, 3, 4};
	std::nth_element(ne, ne + 3, ne + 7);
	// the nth element is the one that would be at index 3 in sorted order (== 4);
	// everything before is <=, everything after is >=
	bool nth_lo = true, nth_hi = true;
	for (int i = 0; i < 3; ++i) {
		if (ne[i] > ne[3]) { nth_lo = false; }
	}
	for (int i = 4; i < 7; ++i) {
		if (ne[i] < ne[3]) { nth_hi = false; }
	}
	printf("nth_element: nth=%d lo_ok=%d hi_ok=%d\n", ne[3], (int) nth_lo, (int) nth_hi);

	// ---- generate / generate_n --------------------------------------------
	int g[5];
	int gc = 0;
	std::generate(g, g + 5, [&gc]() { return gc++ * 2; });
	printv("generate", g, 5);
	int gn[6] = {0, 0, 0, 0, 0, 0};
	int gnc = 10;
	std::generate_n(gn, 4, [&gnc]() { return gnc--; });
	printv("generate_n", gn, 6);

	// ---- copy_backward -----------------------------------------------------
	int cb[6] = {1, 2, 3, 0, 0, 0};
	std::copy_backward(cb, cb + 3, cb + 6);
	printv("copy_backward", cb, 6);

	// ---- minmax (two-arg + initializer_list) ------------------------------
	auto mm = std::minmax(7, 3);
	printf("minmax2=%d,%d\n", mm.first, mm.second);
	auto mmi = std::minmax({4, 1, 7, 3, 9, 2});
	printf("minmax_il=%d,%d\n", mmi.first, mmi.second);

	// ---- set_intersection / set_union with a comparator (descending) ------
	int sa[5] = {9, 7, 5, 3, 1};
	int sb[4] = {8, 5, 3, 0};
	int si[8];
	auto sie = std::set_intersection(sa, sa + 5, sb, sb + 4, si, std::greater<int>());
	printv("set_intersection_cmp", si, (size_t) (sie - si));
	int su[9];
	auto sue = std::set_union(sa, sa + 5, sb, sb + 4, su, std::greater<int>());
	printv("set_union_cmp", su, (size_t) (sue - su));

	// ---- equal (4-iterator form) ------------------------------------------
	int e1[4] = {1, 2, 3, 4};
	int e2[4] = {1, 2, 3, 4};
	int e3[3] = {1, 2, 3};
	printf("equal4: same=%d shorter2nd=%d shorter1st=%d\n",
			(int) std::equal(e1, e1 + 4, e2, e2 + 4),
			(int) std::equal(e1, e1 + 4, e3, e3 + 3),
			(int) std::equal(e3, e3 + 3, e1, e1 + 4));

	// ---- shuffle: order depends on the engine, so only the multiset
	// invariant is checked (a shuffle must be a permutation of its input) ----
	std::vector<int> shf = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
	std::vector<int> orig = shf;
	std::mt19937 eng(12345u);
	std::shuffle(shf.begin(), shf.end(), eng);
	printf("shuffle_is_perm=%d size=%d\n",
			(int) std::is_permutation(shf.begin(), shf.end(), orig.begin()),
			(int) shf.size());
}

} // namespace sprt::test
