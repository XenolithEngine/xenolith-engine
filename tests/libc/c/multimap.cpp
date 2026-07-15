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

#include <stdio.h>

#include <map>
#include <string>

namespace sprt::test {

namespace {

void dump(const char *label, const std::multimap<int, std::string> &m) {
	printf("%s:", label);
	for (const auto &kv : m) { printf(" %d=%s", kv.first, kv.second.c_str()); }
	printf("\n");
}

} // namespace

void performMultimapTest() {
	std::multimap<int, std::string> m;
	// equal keys must retain insertion order (append), distinct keys stay sorted
	m.insert({1, "a"});
	m.insert({1, "b"});
	m.insert({2, "x"});
	m.insert({1, "c"});
	m.emplace(0, "zero");
	dump("order", m);
	printf("size=%zu count(1)=%zu count(9)=%zu\n", m.size(), m.count(1), m.count(9));

	// equal_range over the duplicate key
	{
		auto r = m.equal_range(1);
		printf("equal_range(1):");
		for (auto it = r.first; it != r.second; ++it) { printf(" %s", it->second.c_str()); }
		printf("\n");
	}

	// lower_bound / upper_bound around key 1
	printf("lower_bound(1)=%s upper_bound(1)=%d\n", m.lower_bound(1)->second.c_str(),
			m.upper_bound(1)->first);

	// find + contains
	printf("find(2)=%s contains(2)=%d contains(5)=%d\n", m.find(2)->second.c_str(),
			(int)m.contains(2), (int)m.contains(5));

	// erase(key) removes every equivalent element and returns the count
	size_t removed = m.erase(1);
	printf("erase(1)=%zu size=%zu\n", removed, m.size());
	dump("after erase(1)", m);

	// erase(iterator)
	auto it = m.find(2);
	m.erase(it);
	printf("after erase(it@2): size=%zu\n", m.size());

	// initializer_list ctor + range ctor
	std::multimap<int, std::string> il{{5, "e"}, {5, "E"}, {3, "c"}, {5, "e2"}};
	dump("il_ctor", il);
	printf("il count(5)=%zu\n", il.count(5));

	std::multimap<int, std::string> cp(il.begin(), il.end());
	printf("range_ctor size=%zu equal? %d\n", cp.size(), (int)(cp.size() == il.size()));
}

} // namespace sprt::test
