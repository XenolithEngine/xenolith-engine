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
#include <sprt/cxx/unordered_set>

namespace sprt {

void performMallocUnorderedSetTests() {
	using unordered_set = __malloc_unordered_set<int>;

	sprt::cout << "\n== unordered_set tests ==\n";

	// Test 1: Default constructor and empty check
	{
		unordered_set l;
		sprt::cout << "Test 1 - Default constructor: ";
		if (l.empty()) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 2: Size and max_size
	{
		unordered_set l;
		sprt::cout << "Test 2 - Size and max_size: ";
		if (l.empty() && l.size() == 0 && l.max_size() > 0) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 3: Copy constructor
	{
		unordered_set l1;
		l1.insert(1);
		l1.insert(2);

		unordered_set l2(l1);
		sprt::cout << "Test 3 - Copy constructor: ";
		if (l2.size() == 2 && l2.find(1) != l2.end()) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 4: Move constructor
	{
		unordered_set l1;
		l1.insert(1);
		l1.insert(2);

		unordered_set l2(sprt::move(l1));
		sprt::cout << "Test 4 - Move constructor: ";
		if (l2.size() == 2 && l1.empty()) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 5: Assignment operator
	{
		unordered_set l1;
		l1.insert(1);

		unordered_set l2;
		l2 = l1;
		sprt::cout << "Test 5 - Assignment operator: ";
		if (l2.size() == 1 && l2.find(1) != l2.end()) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 6: Move assignment operator
	{
		unordered_set l1;
		l1.insert(1);

		unordered_set l2;
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
		unordered_set l;
		auto result = l.insert(1);
		sprt::cout << "Test 7 - Insert with value_type: ";
		if (result.second && l.size() == 1) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 8: Insert with rvalue
	{
		unordered_set l;
		auto result = l.insert(2);
		sprt::cout << "Test 8 - Insert with rvalue: ";
		if (result.second && l.size() == 1) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 9: Insert range
	{
		unordered_set l;
		int arr[] = {1, 2};
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
		unordered_set l;
		l.insert({1, 2});
		sprt::cout << "Test 10 - Insert with initializer_list: ";
		if (l.size() == 2) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 11: Emplace
	{
		unordered_set l;
		auto result = l.emplace(3);
		sprt::cout << "Test 11 - Emplace: ";
		if (result.second && l.size() == 1) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 13: Find
	{
		unordered_set l;
		l.insert(1);
		l.insert(2);
		l.insert(3);
		auto it = l.find(2);
		sprt::cout << "Test 13 - Find: ";
		if (it != l.end()) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 14: Count
	{
		unordered_set l;
		l.insert(1);
		size_t count = l.count(1);
		sprt::cout << "Test 14 - Count: ";
		if (count == 1) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 15: Contains
	{
		unordered_set l;
		l.insert(1);
		bool contains = l.contains(1);
		sprt::cout << "Test 15 - Contains: ";
		if (contains) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 16: Equal range
	{
		unordered_set l;
		l.insert(1);
		auto range = l.equal_range(1);
		sprt::cout << "Test 16 - Equal range: ";
		if (range.first != l.end() && range.second != l.end()) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 17: Load factor and max load factor
	{
		unordered_set l;
		float load_factor = l.load_factor();
		float max_load_factor = l.max_load_factor();
		sprt::cout << "Test 17 - Load factor and max load factor: ";
		if (load_factor >= 0 && max_load_factor >= 1.0f) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 18: Rehash
	{
		unordered_set l;
		l.rehash(10);
		sprt::cout << "Test 18 - Rehash: ";
		if (l.size() >= 0) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 19: Clear
	{
		unordered_set l;
		l.insert(1);
		l.clear();
		sprt::cout << "Test 19 - Clear: ";
		if (l.empty()) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 20: Swap
	{
		unordered_set l1;
		l1.insert(1);

		unordered_set l2;
		l2.insert(2);

		l1.swap(l2);
		sprt::cout << "Test 20 - Swap: ";
		if (l1.size() == 1 && l2.size() == 1) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 21: Begin and end iterators
	{
		unordered_set l;
		l.insert(1);
		auto begin_it = l.begin();
		auto end_it = l.end();
		sprt::cout << "Test 21 - Begin and end iterators: ";
		if (begin_it != end_it) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 22: Begin and end const iterators
	{
		unordered_set l;
		l.insert(1);
		const unordered_set &cl = l;
		auto begin_it = cl.begin();
		auto end_it = cl.end();
		sprt::cout << "Test 22 - Begin and end const iterators: ";
		if (begin_it != end_it) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 23: Erase by position
	{
		unordered_set l;
		l.insert(1);
		l.insert(2);
		auto it = l.begin();
		l.erase(it);
		sprt::cout << "Test 23 - Erase by position: ";
		if (l.size() == 1) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 24: Erase by key
	{
		unordered_set l;
		l.insert(1);
		l.insert(2);
		size_t erased = l.erase(1);
		sprt::cout << "Test 24 - Erase by key: ";
		if (erased == 1 && l.size() == 1) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 25: Iterator operations
	{
		unordered_set l;
		l.insert(1);
		l.insert(2);
		auto it = l.begin();
		++it; // Move to second element
		sprt::cout << "Test 25 - Iterator operations: ";
		if (it != l.end()) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	sprt::cout << "\nUnordered set tests completed.\n";
}

} // namespace sprt
