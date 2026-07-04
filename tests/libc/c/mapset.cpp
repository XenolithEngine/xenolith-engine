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

// std::map / std::set conformance, via the <map>/<set> STL wrappers (sprt::__map/__set red-black
// trees, std::allocator). compare.sh diffs the freestanding sprt build against the Linux host
// (libstdc++). Ordered iteration is deterministic (both sort by key). operator[] is read only on
// PRESENT keys (the sprt access_token converts to mapped_type& only when the key exists, aborting
// otherwise; std inserts) and used to insert via `m[k] = v` (works on both). Also exercises the
// deduction guides. Never dereferences end() nor calls at()/operator[] on an absent key.

#include <stdio.h>

#include <map>
#include <set>
#include <string>
#include <utility>
#include <type_traits>

namespace sprt::test {

void performMapSetTest() {
	// ---- CTAD (init-list) + ordered iteration ----
	std::map m {std::pair {3, 30}, std::pair {1, 10}, std::pair {2, 20}};
	static_assert(std::is_same_v<decltype(m), std::map<int, int>>);
	printf("map_ctad:");
	for (auto &kv : m) { printf(" %d=%d", kv.first, kv.second); }
	printf("\n");

	// CTAD (iterator range)
	std::pair<int, int> arr[] = {{5, 50}, {4, 40}};
	std::map mi(arr, arr + 2);
	static_assert(std::is_same_v<decltype(mi), std::map<int, int>>);
	printf("map_iter_ctad: %d %d\n", mi.begin()->first, mi.begin()->second);

	// ---- modifiers ----
	std::map<int, int> a;
	a.insert({1, 10});
	a.insert(std::pair<const int, int>(3, 30));
	a.emplace(5, 50);
	a.try_emplace(7, 70);
	a.try_emplace(5, 999); // no-op, key exists
	auto io1 = a.insert_or_assign(3, 33); // update
	auto io2 = a.insert_or_assign(9, 90); // insert
	printf("mods: 3=%d 5=%d io1_ins=%d io2_ins=%d size=%d\n", a.find(3)->second, a.find(5)->second,
			(int) io1.second, (int) io2.second, (int) a.size());

	// ---- operator[] (present-key reference semantics) ----
	a[11] = 110; // insert via token operator=
	int v11 = a[11]; // read via conversion to int&
	a[11] += 5; // read-modify-write
	int total = 0;
	for (auto &kv : a) { total += kv.second; }
	total += a[1]; // read present key in an expression
	printf("subscript: v11=%d a11=%d total=%d\n", v11, a.find(11)->second, total);

	// ---- lookup ----
	printf("lookup: find7=%d count5=%d has9=%d miss=%d\n", a.find(7)->second, (int) a.count(5),
			(int) a.contains(9), (int) (a.find(100) == a.end()));
	printf("bounds: lb4=%d ub5=%d eq5_found=%d eq6_found=%d\n", a.lower_bound(4)->first,
			a.upper_bound(5)->first, (int) (a.equal_range(5).first != a.equal_range(5).second),
			(int) (a.equal_range(6).first != a.equal_range(6).second));

	// ---- erase / erase_if / copy / == ----
	a.erase(7);
	auto removed = std::erase_if(a, [](const std::pair<const int, int> &kv) { return kv.second > 90; });
	printf("erase: size=%d erased_if=%d\n", (int) a.size(), (int) removed);
	std::map<int, int> b = a;
	printf("copy_eq: %d ne=%d\n", (int) (a == b), (int) (a != mi));

	// const at()
	const std::map<int, int> &ca = a;
	printf("const_at: %d\n", (int) ca.at(1));

	// ---- string-keyed map (ordered), subscripted with C-string literals (the heterogeneous
	// access_token operator[](const char*) path, now that the token key is decayed) ----
	std::map<std::string, int> sm;
	sm["banana"] = 2; // insert via literal subscript
	sm["apple"] = 1;
	sm["cherry"] = 3;
	sm["apple"] += 100; // present-key modify via literal subscript
	printf("strmap: find_apple=%d has_grape=%d", sm.find("apple")->second,
			(int) sm.contains("grape"));
	for (auto &kv : sm) { printf(" %s=%d", kv.first.c_str(), kv.second); }
	printf("\n");

	// ---- set: CTAD, dedup, ordered ----
	std::set s {4, 1, 3, 1, 2};
	static_assert(std::is_same_v<decltype(s), std::set<int>>);
	printf("set_ctad:");
	for (int v : s) { printf(" %d", v); }
	printf("\n");

	std::set<int> ss;
	ss.insert(10);
	ss.emplace(5);
	auto ins = ss.insert(10); // duplicate
	printf("set_mods: size=%d dup_inserted=%d has5=%d count10=%d find5_ok=%d\n", (int) ss.size(),
			(int) ins.second, (int) ss.contains(5), (int) ss.count(10),
			(int) (ss.find(5) != ss.end()));

	std::set<int> big {1, 2, 3, 4, 5, 6};
	auto sremoved = std::erase_if(big, [](int v) { return v % 2 == 0; });
	printf("set_erase_if: removed=%d rest:", (int) sremoved);
	for (int v : big) { printf(" %d", v); }
	printf("\n");
	std::set<int> bc = big;
	printf("set_copy_eq: %d lower3=%d upper3=%d\n", (int) (big == bc), *big.lower_bound(3),
			*big.upper_bound(3));
}

} // namespace sprt::test
