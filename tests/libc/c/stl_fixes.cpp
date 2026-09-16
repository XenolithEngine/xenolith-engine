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

// Behavioural coverage for the container / STL correctness fixes in this
// changeset: std::multiset (new), std::string operator+ with a single char
// (reserve/size fix), std::map::emplace, and move-only element support in the
// containers whose move constructor / move_from was corrected (vector, list,
// map, set). Deterministic output diffs identically across the sprt targets.

#include <stdio.h>

#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace sprt::test {

void performStlFixesTest() {
	// ---- std::multiset (duplicate keys) -----------------------------------
	std::multiset<int> ms = {5, 1, 3, 3, 5, 5, 2};
	printf("multiset: size=%d count5=%d count3=%d count9=%d\n", (int) ms.size(),
			(int) ms.count(5), (int) ms.count(3), (int) ms.count(9));
	printf("multiset_sorted=");
	for (int v : ms) { printf("%d", v); }
	printf("\n");
	ms.insert(3);
	printf("multiset: after_insert count3=%d\n", (int) ms.count(3));
	auto er = ms.equal_range(5);
	int rangeLen = 0;
	for (auto it = er.first; it != er.second; ++it) { ++rangeLen; }
	// upper_bound of the maximum key must be end(): regression guard for the
	// rbtree upper_bound_ptr fix (it used to return the root for key >= max).
	printf("multiset: ub_max_is_end=%d\n", (int) (ms.upper_bound(5) == ms.end()));
	printf("multiset: equal_range5_len=%d lb2=%d ub3=%d\n", rangeLen, *ms.lower_bound(2),
			*ms.upper_bound(3));
	// NB: sequence erase before size() — evaluating both inside one printf() call
	// would read size() in unspecified order relative to erase().
	auto erased5 = ms.erase(5);
	printf("multiset: erase5_count=%d remaining=%d\n", (int) erased5, (int) ms.size());

	// ---- std::string operator+ with a single char (reserve/size fix) ------
	std::string base = "abc";
	std::string sc = base + 'd';
	printf("str_plus_char: '%s' size=%d\n", sc.c_str(), (int) sc.size());
	std::string cs = 'z' + base;
	printf("char_plus_str: '%s' size=%d\n", cs.c_str(), (int) cs.size());
	// chaining, which is what surfaced the wrong-length reserve
	std::string chain = base + 'x' + 'y' + 'z';
	printf("str_chain: '%s' size=%d\n", chain.c_str(), (int) chain.size());

	// ---- std::map::emplace -------------------------------------------------
	std::map<int, std::string> mp;
	auto r1 = mp.emplace(1, "one");
	auto r2 = mp.emplace(2, "two");
	auto r3 = mp.emplace(1, "uno"); // duplicate key -> no insert
	printf("map_emplace: r1=%d r2=%d r3=%d size=%d val1=%s\n", (int) r1.second,
			(int) r2.second, (int) r3.second, (int) mp.size(), mp[1].c_str());

	// ---- move-only element support (container move constructor fix) --------
	// vector of unique_ptr, move-constructed: contents transfer, source empties.
	std::vector<std::unique_ptr<int>> vsrc;
	vsrc.push_back(std::make_unique<int>(10));
	vsrc.push_back(std::make_unique<int>(20));
	vsrc.push_back(std::make_unique<int>(30));
	std::vector<std::unique_ptr<int>> vdst = std::move(vsrc);
	int vsum = 0;
	for (auto &p : vdst) { vsum += *p; }
	printf("move_vector: dst_size=%d sum=%d src_size=%d\n", (int) vdst.size(), vsum,
			(int) vsrc.size());

	// list of unique_ptr, move-constructed.
	std::list<std::unique_ptr<int>> lsrc;
	lsrc.push_back(std::make_unique<int>(1));
	lsrc.push_back(std::make_unique<int>(2));
	std::list<std::unique_ptr<int>> ldst = std::move(lsrc);
	int lsum = 0;
	for (auto &p : ldst) { lsum += *p; }
	printf("move_list: dst_size=%d sum=%d\n", (int) ldst.size(), lsum);

	// map with move-only mapped type, move-constructed.
	std::map<int, std::unique_ptr<int>> msrc;
	msrc.emplace(1, std::make_unique<int>(100));
	msrc.emplace(2, std::make_unique<int>(200));
	std::map<int, std::unique_ptr<int>> mdst = std::move(msrc);
	printf("move_map: dst_size=%d v1=%d v2=%d\n", (int) mdst.size(), *mdst[1], *mdst[2]);

	// set with a move-only key? Keys must be comparable; use unique_ptr<int>
	// ordering by pointer is non-deterministic, so instead move a set of ints
	// to exercise the ordered-container move path with a stateless allocator.
	std::set<int> ssrc = {4, 2, 6, 1};
	std::set<int> sdst = std::move(ssrc);
	printf("move_set: dst_size=%d first=%d last=%d\n", (int) sdst.size(), *sdst.begin(),
			*sdst.rbegin());
}

} // namespace sprt::test
