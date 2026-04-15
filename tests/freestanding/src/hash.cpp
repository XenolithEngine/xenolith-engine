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

#include <sprt/runtime/stream.h>
#include <sprt/cxx/unordered_map>
#include <sprt/cxx/detail/allocator_malloc.h>

namespace sprt {

template <typename Key, typename Value>
using test_unordered_map = __unordered_map<Key, Value, sprt::hash<void>, sprt::equal_to<void>,
		sprt::detail::AllocatorMalloc<sprt::pair<const Key, Value>>>;

void performHashTests() {
	test_unordered_map<int, const char *> map;

	// Simple test: insert some values and check if they're accessible
	map[1] = "one";
	map[2] = "two";
	map[3] = "three";

	// Check that we can retrieve values using at() method
	const char *val1 = map.at(1).get();
	const char *val2 = map.at(2).get();
	const char *val3 = map.at(3).get();

	sprt::cout << "1 >> " << val1 << "\n";
	sprt::cout << "2 >> " << val2 << "\n";
	sprt::cout << "3 >> " << val3 << "\n";

	// Test count and contains functionality
	auto count1 = map.count(1);
	auto count2 = map.count(4); // non-existent key

	sprt::cout << "count (1) " << count1 << "\n";
	sprt::cout << "count (4) " << count2 << "\n";

	// Test find functionality
	auto it = map.find(2);
	bool found = (it != map.end());

	sprt::cout << "found (2) " << found << "\n";

	// Test erase functionality
	size_t erased_count = map.erase(2); // erase existing element
	sprt::cout << "erase (existed) " << erased_count << "\n";

	size_t erased_count2 = map.erase(99); // try to erase non-existing element
	sprt::cout << "erase (non-existed) " << erased_count2 << "\n";

	auto count_after_erase = map.count(2); // should be 0 now
	sprt::cout << "count (erased) " << count_after_erase << "\n";

	// Insert a new element and test erase with iterator
	map[4] = "four";
	auto it2 = map.find(4);
	if (it2 != map.end()) {
		map.erase(it2); // erase using iterator
	}
	sprt::cout << "contains (erased) " << map.contains(4) << "\n";
}

} // namespace sprt
