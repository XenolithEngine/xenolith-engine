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

// std::list / std::forward_list conformance, via the <list>/<forward_list> STL wrappers. Also
// exercises the list operations (remove/remove_if/unique/sort/merge/reverse/splice[_after]) added
// to sprt, std::erase/erase_if, and the deduction guides. Written to also compile on the system
// stdlib so the same source can be diffed against a real std::list. Output is deterministic (the
// sequences are printed after each operation; sort uses distinct values so stability is moot).

#include <stdio.h>

#include <list>
#include <forward_list>
#include <type_traits>

namespace sprt::test {

namespace {

template <class _Container>
void dump(const char *__label, const _Container &__c) {
	printf("%s:", __label);
	for (const auto &__v : __c) { printf(" %d", __v); }
	printf("\n");
}

} // namespace

void performListTest() {
	// ---- std::list ----
	std::list l {5, 3, 1, 4, 2}; // CTAD -> list<int>
	static_assert(std::is_same_v<decltype(l), std::list<int>>);
	dump("list_ctad", l);
	printf("list_meta: size=%d empty=%d front=%d back=%d\n", (int) l.size(), (int) l.empty(),
			l.front(), l.back());

	l.push_back(99);
	l.push_front(0);
	l.pop_back();
	auto pos = l.begin();
	++pos;
	++pos;
	l.insert(pos, 77);
	l.emplace(pos, 88);
	dump("list_modify", l);

	l.sort();
	dump("list_sort", l);
	l.reverse();
	dump("list_reverse", l);

	std::list<int> d {1, 2, 2, 3, 3, 3, 4, 4};
	auto uq = d.unique();
	printf("list_unique: removed=%d", (int) uq);
	dump("", d);

	std::list<int> rl {1, 2, 3, 2, 4, 2};
	auto rm = rl.remove(2);
	auto rmi = rl.remove_if([](int x) { return x > 3; });
	printf("list_remove: removed=%d removed_if=%d", (int) rm, (int) rmi);
	dump("", rl);

	std::list<int> m1 {1, 3, 5, 7}, m2 {2, 4, 6};
	m1.merge(m2);
	printf("list_merge:");
	dump("", m1);
	printf("list_merge_src_empty: %d\n", (int) m2.empty());

	std::list<int> s1 {1, 2, 3}, s2 {40, 50};
	auto sp = s1.begin();
	++sp;
	s1.splice(sp, s2); // insert s2 before position 2
	printf("list_splice:");
	dump("", s1);
	printf("list_splice_src_empty: %d\n", (int) s2.empty());

	std::list<int> e {1, 2, 3, 4, 5, 6};
	auto en = std::erase(e, 3);
	auto eni = std::erase_if(e, [](int x) { return x % 2 == 0; });
	printf("list_erase: erased=%d erased_if=%d", (int) en, (int) eni);
	dump("", e);

	// iterator-range CTAD + copy + ==
	int arr[] = {10, 20, 30};
	std::list li(arr, arr + 3);
	static_assert(std::is_same_v<decltype(li), std::list<int>>);
	std::list<int> lc = li;
	printf("list_iter_ctad_copy_eq: %d %d\n", (int) (li == lc), (int) (li == m1));

	// ---- std::forward_list ----
	std::forward_list fl {5, 3, 1, 4, 2};
	static_assert(std::is_same_v<decltype(fl), std::forward_list<int>>);
	dump("flist_ctad", fl);
	printf("flist_meta: empty=%d front=%d\n", (int) fl.empty(), fl.front());

	fl.push_front(0);
	auto fpos = fl.before_begin();
	++fpos; // now at first element
	fl.insert_after(fpos, 77);
	fl.emplace_after(fpos, 88);
	dump("flist_modify", fl);

	fl.sort();
	dump("flist_sort", fl);
	fl.reverse();
	dump("flist_reverse", fl);

	std::forward_list<int> fd {1, 1, 2, 3, 3, 4};
	auto fuq = fd.unique();
	printf("flist_unique: removed=%d", (int) fuq);
	dump("", fd);

	std::forward_list<int> frl {1, 2, 3, 2, 4, 2};
	auto frm = frl.remove(2);
	printf("flist_remove: removed=%d", (int) frm);
	dump("", frl);

	std::forward_list<int> fm1 {1, 3, 5, 7}, fm2 {2, 4, 6};
	fm1.merge(fm2);
	printf("flist_merge:");
	dump("", fm1);
	printf("flist_merge_src_empty: %d\n", (int) fm2.empty());

	std::forward_list<int> fs1 {1, 2, 3}, fs2 {40, 50};
	fs1.splice_after(fs1.before_begin(), fs2); // insert s2 at the front
	printf("flist_splice:");
	dump("", fs1);

	std::forward_list<int> fe {1, 2, 3, 4, 5, 6};
	auto fen = std::erase(fe, 3);
	auto feni = std::erase_if(fe, [](int x) { return x % 2 == 0; });
	printf("flist_erase: erased=%d erased_if=%d", (int) fen, (int) feni);
	dump("", fe);

	std::forward_list fli(arr, arr + 3);
	static_assert(std::is_same_v<decltype(fli), std::forward_list<int>>);
	std::forward_list<int> flc = fli;
	printf("flist_iter_ctad_copy_eq: %d\n", (int) (fli == flc));
}

} // namespace sprt::test
