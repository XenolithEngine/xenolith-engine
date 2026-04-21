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

#include <sprt/cxx/cstring>
#include <sprt/runtime/stream.h>
#include <sprt/cxx/unordered_map>

namespace sprt {

template <typename Key, typename Value>
using edgecases1_unordered_map = __unordered_map<Key, Value, sprt::hash<void>, sprt::equal_to<void>,
		sprt::detail::AllocatorMalloc<sprt::pair<const Key, Value>>>;

static void edgecases1() {
	sprt::cout << "=== edgecases1 ===\n";
	edgecases1_unordered_map<int, const char *> map;

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
	sprt::cout << "=== edgecases1 complete ===\n";
}

void performMallocUnorderedMapTests() {
	using unordered_map = __malloc_unordered_map<const char *, const char *>;

	sprt::cout << "\n== unordered_map tests ==\n";

	edgecases1();

	// Test 1: Default constructor and empty check
	{
		unordered_map l;
		sprt::cout << "Test 1 - Default constructor: ";
		if (l.empty()) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 2: Size and max_size
	{
		unordered_map l;
		sprt::cout << "Test 2 - Size and max_size: ";
		if (l.empty() && l.size() == 0 && l.max_size() > 0) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 3: Copy constructor
	{
		unordered_map l1;
		l1.insert({"key1", "value1"});
		l1.insert({"key2", "value2"});

		unordered_map l2(l1);
		sprt::cout << "Test 3 - Copy constructor: ";
		if (l2.size() == 2 && l2.find("key1") != l2.end() && l1 == l2) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 4: Move constructor
	{
		unordered_map l1;
		l1.insert({"key1", "value1"});
		l1.insert({"key2", "value2"});

		unordered_map l2(sprt::move(l1));
		sprt::cout << "Test 4 - Move constructor: ";
		if (l2.size() == 2 && l1.empty()) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 5: Assignment operator
	{
		unordered_map l1;
		l1.insert({"key1", "value1"});

		unordered_map l2;
		l2 = l1;
		sprt::cout << "Test 5 - Assignment operator: ";
		if (l2.size() == 1 && l2.find("key1") != l2.end()) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 6: Move assignment operator
	{
		unordered_map l1;
		l1.insert({"key1", "value1"});

		unordered_map l2;
		l2 = sprt::move(l1);
		sprt::cout << "Test 6 - Move assignment operator: ";
		if (l2.size() == 1 && l1.empty()) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 7: Insert with value_type
	{
		unordered_map l;
		auto valueToInsert = unordered_map::value_type("key1", "value1");
		auto result = l.insert(valueToInsert);
		sprt::cout << "Test 7 - Insert with value_type: ";
		if (result.second && l.size() == 1) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 8: Insert with rvalue
	{
		unordered_map l;
		auto result = l.insert({"key1", "value1"});
		sprt::cout << "Test 8 - Insert with rvalue: ";
		if (result.second && l.size() == 1) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 9: Insert range
	{
		unordered_map l;
		sprt::pair<const char *, const char *> arr[] = {{"key1", "value1"}, {"key2", "value2"}};
		l.insert(arr, arr + 2);
		sprt::cout << "Test 9 - Insert range: ";
		if (l.size() == 2) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 10: Insert with initializer_list
	{
		unordered_map l;
		l.insert({{"key1", "value1"}, {"key2", "value2"}});
		sprt::cout << "Test 10 - Insert with initializer_list: ";
		if (l.size() == 2) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 11: Emplace
	{
		unordered_map l;
		auto result = l.emplace("key1", "value1");
		sprt::cout << "Test 11 - Emplace: ";
		if (result.second && l.size() == 1) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 12: Try_emplace
	{
		unordered_map l;
		auto result = l.try_emplace("key1", "value1");
		sprt::cout << "Test 12 - Try_emplace: ";
		if (result.second && l.size() == 1) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 13: Insert_or_assign
	{
		unordered_map l;
		auto result = l.insert_or_assign("key1", "value1");
		sprt::cout << "Test 13 - Insert_or_assign: ";
		if (result.second && l.size() == 1) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 14: Find
	{
		unordered_map l;
		l.insert({"key1", "value1"});
		l.insert({"key2", "value2"});
		l.insert({"key3", "value3"});
		auto it = l.find("key2");
		sprt::cout << "Test 14 - Find: ";
		if (it != l.end()) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 15: Count
	{
		unordered_map l;
		l.insert({"key1", "value1"});
		size_t count = l.count("key1");
		sprt::cout << "Test 15 - Count: ";
		if (count == 1) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 16: Contains
	{
		unordered_map l;
		l.insert({"key1", "value1"});
		bool contains = l.contains("key1");
		sprt::cout << "Test 16 - Contains: ";
		if (contains) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 17: Equal range
	{
		unordered_map l;
		l.insert({"key1", "value1"});
		auto range = l.equal_range("key1");
		sprt::cout << "Test 17 - Equal range: ";
		if (range.first != l.end() && range.second != l.end()) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 18: Operator[]
	{
		unordered_map l;
		l["key1"] = "value1";
		sprt::cout << "Test 18 - Operator[]: ";
		if (l.size() == 1) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 19: At
	{
		unordered_map l;
		l["key1"] = "value1";
		auto val = l.at("key1");
		sprt::cout << "Test 19 - At: ";
		if (strcmp(val.get(""), "value1") == 0) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 20: Load factor and max load factor
	{
		unordered_map l;
		float load_factor = l.load_factor();
		float max_load_factor = l.max_load_factor();
		sprt::cout << "Test 20 - Load factor and max load factor: ";
		if (load_factor >= 0 && max_load_factor >= 1.0f) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 21: Rehash
	{
		unordered_map l;
		l.rehash(10);
		sprt::cout << "Test 21 - Rehash: ";
		if (l.size() >= 0) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 22: Clear
	{
		unordered_map l;
		l.insert({"key1", "value1"});
		l.clear();
		sprt::cout << "Test 22 - Clear: ";
		if (l.empty()) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 23: Swap
	{
		unordered_map l1;
		l1.insert({"key1", "value1"});

		unordered_map l2;
		l2.insert({"key2", "value2"});

		l1.swap(l2);
		sprt::cout << "Test 23 - Swap: ";
		if (l1.size() == 1 && l2.size() == 1) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 24: Begin and end iterators
	{
		unordered_map l;
		l.insert({"key1", "value1"});
		auto begin_it = l.begin();
		auto end_it = l.end();
		sprt::cout << "Test 24 - Begin and end iterators: ";
		if (begin_it != end_it) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 25: Begin and end const iterators
	{
		unordered_map l;
		l.insert({"key1", "value1"});
		const unordered_map &cl = l;
		auto begin_it = cl.begin();
		auto end_it = cl.end();
		sprt::cout << "Test 25 - Begin and end const iterators: ";
		if (begin_it != end_it) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}


	sprt::cout << "\nUnordered map tests completed.\n";
}

} // namespace sprt
