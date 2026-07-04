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

// std::unordered_map / std::unordered_set conformance via the <unordered_map>/<unordered_set> STL
// wrappers, which use sprt::__unordered_* in node-indirection mode (each element in an individually
// heap-allocated node -> STABLE element addresses). The point of that mode is the standard's
// reference/pointer-stability guarantee, so the first block is the headline test: a pointer and a
// reference into the map must survive many rehashes and unrelated erases. Written to also compile on
// the system stdlib so the same source diffs against a real std::unordered_map (libstdc++). Bucket
// iteration order is unspecified and differs between implementations, so EVERYTHING that iterates is
// sorted by key first; all other checks look up by key (order-independent). Output is deterministic.

#include <stdio.h>

#include <unordered_map>
#include <unordered_set>
#include <string>
#include <utility>
#include <type_traits>

namespace sprt::test {

namespace {

// Insertion-sort a small (key -> value) snapshot and print it, so the output does not depend on the
// (unspecified, implementation-defined) bucket order.
void dump_map_sorted(const char *__label, const std::unordered_map<int, int> &__m) {
	struct KV {
		int k, v;
	};
	KV __buf[64];
	int __n = 0;
	for (const auto &__kv : __m) {
		if (__n < 64) {
			__buf[__n++] = KV {__kv.first, __kv.second};
		}
	}
	for (int __i = 1; __i < __n; ++__i) {
		KV __x = __buf[__i];
		int __j = __i - 1;
		while (__j >= 0 && __buf[__j].k > __x.k) {
			__buf[__j + 1] = __buf[__j];
			--__j;
		}
		__buf[__j + 1] = __x;
	}
	printf("%s:", __label);
	for (int __i = 0; __i < __n; ++__i) { printf(" %d=%d", __buf[__i].k, __buf[__i].v); }
	printf("\n");
}

void dump_set_sorted(const char *__label, const std::unordered_set<int> &__s) {
	int __buf[64];
	int __n = 0;
	for (int __v : __s) {
		if (__n < 64) {
			__buf[__n++] = __v;
		}
	}
	for (int __i = 1; __i < __n; ++__i) {
		int __x = __buf[__i];
		int __j = __i - 1;
		while (__j >= 0 && __buf[__j] > __x) {
			__buf[__j + 1] = __buf[__j];
			--__j;
		}
		__buf[__j + 1] = __x;
	}
	printf("%s:", __label);
	for (int __i = 0; __i < __n; ++__i) { printf(" %d", __buf[__i]); }
	printf("\n");
}


// Collect the mapped values for key k (via equal_range) and print them sorted, so the output is
// independent of bucket order. Also reports equal_range distance so it can be checked == count.
void dump_mm_key(const char *__label, const std::unordered_multimap<int, int> &__m, int __k) {
	int __buf[32];
	int __n = 0;
	auto __r = __m.equal_range(__k);
	for (auto __it = __r.first; __it != __r.second && __n < 32; ++__it) { __buf[__n++] = __it->second; }
	for (int __i = 1; __i < __n; ++__i) {
		int __x = __buf[__i], __j = __i - 1;
		while (__j >= 0 && __buf[__j] > __x) { __buf[__j + 1] = __buf[__j]; --__j; }
		__buf[__j + 1] = __x;
	}
	printf("%s k=%d cnt=%d er=%d vals:", __label, __k, (int) __m.count(__k), __n);
	for (int __i = 0; __i < __n; ++__i) { printf(" %d", __buf[__i]); }
	printf("\n");
}

} // namespace

void performUnorderedTest() {
	// ---- headline: reference / pointer stability across rehash (the node-indirection guarantee) ----
	std::unordered_map<int, int> m;
	m[100] = 4242;
	int *p = &m[100]; // pointer into a heap node
	int &r = m[100];  // reference into a heap node
	for (int i = 0; i < 200; ++i) { m[i] = i * 3; } // many inserts -> several rehashes
	m[100] = 555;                                   // mutate through the map
	printf("stability: p_same=%d r_same=%d p_val=%d r_val=%d map_val=%d\n", (int) (p == &m[100]),
			(int) (&r == &m[100]), *p, r, m[100]);

	// erase must not disturb OTHER elements' addresses
	int *pa = &m[50], *pb = &m[150];
	int va = *pa, vb = *pb;
	m.erase(75);
	m.erase(76);
	m.erase(77);
	printf("erase_stability: pa=%d pb=%d va=%d vb=%d size=%d has75=%d\n", (int) (pa == &m[50]),
			(int) (pb == &m[150]), (int) (*pa == va), (int) (*pb == vb), (int) m.size(),
			(int) m.count(75));

	// content after churn (order-independent aggregate)
	int sum = 0, cnt = 0;
	for (int i = 0; i < 200; ++i) {
		if (m.count(i)) {
			sum += m[i];
			++cnt;
		}
	}
	printf("content: size=%d cnt=%d sum=%d at100=%d\n", (int) m.size(), cnt, sum, m.at(100));

	// ---- small map: modifiers + lookup + sorted dump ----
	std::unordered_map<int, int> a;
	a.insert({1, 10});
	a.insert(std::pair<const int, int>(3, 30));
	a.emplace(5, 50);
	a.try_emplace(7, 70);
	a.try_emplace(5, 999);                // no-op, key exists
	auto io1 = a.insert_or_assign(3, 33); // update
	auto io2 = a.insert_or_assign(9, 90); // insert
	a[11] = 110;                          // operator[] insert
	int v11 = a[11];                      // read via mapped_type&
	a[11] += 5;                           // read-modify-write
	printf("mods: 3=%d 5=%d io1_ins=%d io2_ins=%d v11=%d a11=%d size=%d\n", a.find(3)->second,
			a.find(5)->second, (int) io1.second, (int) io2.second, v11, a.at(11), (int) a.size());
	printf("lookup: find7=%d count5=%d has9=%d miss=%d eq5=%d eq6=%d\n", a.find(7)->second,
			(int) a.count(5), (int) a.contains(9), (int) (a.find(100) == a.end()),
			(int) (a.equal_range(5).first != a.equal_range(5).second),
			(int) (a.equal_range(6).first != a.equal_range(6).second));
	dump_map_sorted("map_dump", a);

	// erase / erase_if / copy / move / ==
	a.erase(7);
	auto removed = std::erase_if(a, [](const std::pair<const int, int> &kv) { return kv.second > 90; });
	printf("erase: size=%d erased_if=%d\n", (int) a.size(), (int) removed);
	std::unordered_map<int, int> b = a;
	std::unordered_map<int, int> mv = static_cast<std::unordered_map<int, int> &&>(b);
	printf("copy_move_eq: eq=%d moved_size=%d moved_at1=%d\n", (int) (a == mv), (int) mv.size(),
			mv.at(1));
	const std::unordered_map<int, int> &ca = a;
	printf("const_at: %d\n", (int) ca.at(1));

	// ---- string-keyed map: operator[] with C-string literals + present-key modify ----
	std::unordered_map<std::string, int> sm;
	sm["banana"] = 2;
	sm["apple"] = 1;
	sm["cherry"] = 3;
	sm["apple"] += 100;
	printf("strmap: find_apple=%d has_grape=%d apple=%d banana=%d cherry=%d\n",
			sm.find("apple")->second, (int) sm.contains("grape"), sm.at("apple"), sm.at("banana"),
			sm.at("cherry"));

	// ---- CTAD ----
	std::unordered_map cm {std::pair {1, 2}, std::pair {3, 4}};
	static_assert(std::is_same_v<decltype(cm), std::unordered_map<int, int>>);
	printf("map_ctad: c1=%d c3=%d size=%d\n", cm.at(1), cm.at(3), (int) cm.size());

	// ---- unordered_set: churn + modifiers + sorted dump ----
	std::unordered_set<int> s;
	for (int i = 0; i < 100; ++i) { s.insert(i * 2); }
	int found = 0;
	for (int i = 0; i < 200; ++i) { found += (int) (s.count(i) != 0); }
	for (int i = 0; i < 50; ++i) { s.erase(i * 4); }
	printf("set_churn: size=%d evens_found=%d has8=%d has10=%d\n", (int) s.size(), found,
			(int) s.count(8), (int) s.count(10));

	std::unordered_set<int> ss;
	ss.insert(10);
	ss.emplace(5);
	auto ins = ss.insert(10); // duplicate
	ss.insert(7);
	ss.insert(3);
	auto sremoved = std::erase_if(ss, [](int v) { return v > 7; });
	printf("set_mods: dup_inserted=%d has5=%d count10=%d erased_if=%d\n", (int) ins.second,
			(int) ss.contains(5), (int) ss.count(10), (int) sremoved);
	dump_set_sorted("set_dump", ss);

	std::unordered_set<int> sc = ss;
	printf("set_copy_eq: %d\n", (int) (sc == ss));

	// set CTAD (init-list + iterator range with bucket count)
	std::unordered_set cs {4, 1, 3, 1, 2};
	static_assert(std::is_same_v<decltype(cs), std::unordered_set<int>>);
	int arr[] = {7, 8, 9};
	std::unordered_set is(arr, arr + 3, 8);
	static_assert(std::is_same_v<decltype(is), std::unordered_set<int>>);
	printf("set_ctad: dedup_size=%d iter_has8=%d\n", (int) cs.size(), (int) is.count(8));

	// ================= unordered_multimap / unordered_multiset =================
	std::unordered_multimap<int, int> mm;
	mm.insert({5, 50});
	mm.insert({7, 70});
	mm.emplace(5, 51);
	mm.insert(std::pair<const int, int>(9, 90));
	mm.emplace(5, 52);
	mm.emplace(7, 71);
	mm.insert({9, 91});
	mm.insert({9, 92});
	printf("mm_meta: size=%d count5=%d count7=%d count9=%d count3=%d find5=%d has3=%d\n",
			(int) mm.size(), (int) mm.count(5), (int) mm.count(7), (int) mm.count(9),
			(int) mm.count(3), (int) (mm.find(5) != mm.end()), (int) (mm.count(3) != 0));
	dump_mm_key("mm", mm, 5);   // equal_range value set (sorted): {50,51,52}, er==cnt
	dump_mm_key("mm", mm, 7);
	dump_mm_key("mm", mm, 9);

	// force rehash (equal_range must stay a valid contiguous range afterwards)
	for (int i = 100; i < 180; ++i) { mm.insert({i, i}); }
	dump_mm_key("mm_rehash", mm, 5);
	dump_mm_key("mm_rehash", mm, 9);
	printf("mm_rehash_meta: size=%d\n", (int) mm.size());

	// erase ALL of key 9 (order-independent), leave 5 and 7 intact
	auto er9 = mm.erase(9);
	printf("mm_erase9: erased=%d count9=%d count5=%d count7=%d size=%d\n", (int) er9,
			(int) mm.count(9), (int) mm.count(5), (int) mm.count(7), (int) mm.size());

	// erase_if by mapped value parity — deterministic (removes ALL even, regardless of order)
	auto mmr = std::erase_if(mm, [](const std::pair<const int, int> &kv) { return kv.second % 2 == 0; });
	printf("mm_erase_if: removed=%d count5=%d count7=%d\n", (int) mmr, (int) mm.count(5),
			(int) mm.count(7));
	dump_mm_key("mm_after", mm, 5); // {51} left (50,52 were even)
	dump_mm_key("mm_after", mm, 7); // {71} left (70 was even)

	// CTAD + copy
	std::unordered_multimap cmm {std::pair {1, 10}, std::pair {1, 11}, std::pair {2, 20}};
	static_assert(std::is_same_v<decltype(cmm), std::unordered_multimap<int, int>>);
	std::unordered_multimap<int, int> cmm2 = cmm;
	printf("mm_ctad_copy: c1=%d c2=%d copy1=%d size=%d\n", (int) cmm.count(1), (int) cmm.count(2),
			(int) cmm2.count(1), (int) cmm2.size());

	// ---- unordered_multiset ----
	std::unordered_multiset<int> ms;
	for (int v : {5, 7, 5, 9, 5, 7, 9, 9}) { ms.insert(v); }
	auto ms_er = [&](int k) {
		auto r = ms.equal_range(k);
		int d = 0; for (auto it = r.first; it != r.second; ++it) ++d; return d;
	};
	printf("ms_meta: size=%d c5=%d c7=%d c9=%d c3=%d er5=%d er7=%d er9=%d\n", (int) ms.size(),
			(int) ms.count(5), (int) ms.count(7), (int) ms.count(9), (int) ms.count(3), ms_er(5),
			ms_er(7), ms_er(9));
	for (int i = 200; i < 280; ++i) { ms.insert(i); }
	printf("ms_rehash: size=%d c5=%d er5=%d\n", (int) ms.size(), (int) ms.count(5), ms_er(5));
	auto mser9 = ms.erase(9);
	printf("ms_erase9: erased=%d c9=%d size=%d\n", (int) mser9, (int) ms.count(9), (int) ms.size());
	auto msr = std::erase_if(ms, [](int v) { return v >= 200; });
	printf("ms_erase_if: removed=%d size=%d c5=%d\n", (int) msr, (int) ms.size(), (int) ms.count(5));
	std::unordered_multiset cms {4, 4, 1, 4, 2, 1};
	static_assert(std::is_same_v<decltype(cms), std::unordered_multiset<int>>);
	printf("ms_ctad: c4=%d c1=%d c2=%d size=%d\n", (int) cms.count(4), (int) cms.count(1),
			(int) cms.count(2), (int) cms.size());
}

} // namespace sprt::test
