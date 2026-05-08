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
#include <sprt/cxx/forward_list>
#include <sprt/cxx/vector>

namespace sprt {

void performMallocForwardListTests() {
	using forward_list = __malloc_forward_list<int>;

	sprt::cout << "\n== forward_list tests ==\n";

	// Test 1: Default constructor and empty check
	{
		forward_list l;
		sprt::cout << "Test 1 - Default constructor and empty check: ";
		if (l.empty()) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 2: Size check on empty list
	{
		forward_list l;
		sprt::cout << "Test 2 - Size check on empty list: ";
		if (l.size() == 0) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 3: Constructor with count and value
	{
		forward_list l(3, 42);
		sprt::cout << "Test 3 - Constructor with count and value: ";
		if (l.size() == 3 && !l.empty()) {
			bool allEqual = true;
			for (const auto &val : l) {
				if (val != 42) {
					allEqual = false;
					break;
				}
			}
			if (allEqual) {
				sprt::cout << "PASS\n";
			} else {
				sprt::cout << "FAIL\n";
			}
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 4: Iterator access
	{
		forward_list l{1, 2, 3, 4, 5};
		sprt::cout << "Test 4 - Iterator access: ";
		sprt::__malloc_vector<int> result;
		for (const auto &val : l) { result.push_back(val); }
		if (result.size() == 5 && result[0] == 1 && result[4] == 5) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 5: Front element access
	{
		forward_list l{1, 2, 3};
		sprt::cout << "Test 5 - Front element access: ";
		if (!l.empty() && l.front() == 1) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 6: push_front and pop_front
	{
		forward_list l;
		l.push_front(1);
		l.push_front(2);
		sprt::cout << "Test 6 - push_front and pop_front: ";
		if (!l.empty() && l.size() == 2 && l.front() == 2) {
			l.pop_front();
			if (l.size() == 1 && l.front() == 1) {
				sprt::cout << "PASS\n";
			} else {
				sprt::cout << "FAIL\n";
			}
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 7: emplace_front
	{
		forward_list l;
		l.emplace_front(1);
		sprt::cout << "Test 7 - emplace_front: ";
		if (!l.empty() && l.size() == 1 && l.front() == 1) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 8: Insert after operations
	{
		forward_list l{2, 3, 4};
		auto it = l.insert_after(l.before_begin(), 1);
		sprt::cout << "Test 8 - Insert after operations: ";
		if (!l.empty() && l.size() == 4 && *it == 1 && l.front() == 1) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 9: Erase after operations
	{
		forward_list l{1, 2, 3, 4};
		auto it = l.erase_after(l.before_begin());
		sprt::cout << "Test 9 - Erase after operations: ";
		if (!l.empty() && l.size() == 3 && *it == 2 && l.front() == 2) {
			sprt::__malloc_vector<int> expected{2, 3, 4};
			bool match = true;
			auto l_it = l.begin();
			for (const auto &val : expected) {
				if (*l_it != val) {
					match = false;
					break;
				}
				++l_it;
			}
			if (match) {
				sprt::cout << "PASS\n";
			} else {
				sprt::cout << "FAIL\n";
			}
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 10: Resize
	{
		forward_list l{1, 2, 3};
		l.resize(5, 9);
		sprt::cout << "Test 10 - Resize: ";
		if (l.size() == 5) {
			sprt::__malloc_vector<int> expected{1, 2, 3, 9, 9};
			bool match = true;
			auto l_it = l.begin();
			for (const auto &val : expected) {
				if (*l_it != val) {
					match = false;
					break;
				}
				++l_it;
			}
			if (match) {
				sprt::cout << "PASS\n";
			} else {
				sprt::cout << "FAIL\n";
			}
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 11: Assign from count and value
	{
		forward_list l{1, 2, 3};
		l.assign(3, 7);
		sprt::cout << "Test 11 - Assign from count and value: ";
		if (l.size() == 3 && l.front() == 7) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 12: Clear
	{
		forward_list l{1, 2, 3};
		l.clear();
		sprt::cout << "Test 12 - Clear: ";
		if (l.empty() && l.size() == 0) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 13: Copy constructor
	{
		forward_list l1{1, 2, 3};
		forward_list l2(l1);
		sprt::cout << "Test 13 - Copy constructor: ";
		if (l2.size() == 3 && l2.front() == 1) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 14: Assignment operator
	{
		forward_list l1{1, 2, 3};
		forward_list l2{4, 5};
		l2 = l1;
		sprt::cout << "Test 14 - Assignment operator: ";
		if (l2.size() == 3 && l2.front() == 1) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 15: Equality comparison
	{
		forward_list l1{1, 2, 3};
		forward_list l2{1, 2, 3};
		forward_list l3{1, 2, 4};
		sprt::cout << "Test 15 - Equality comparison: ";
		if (l1 == l2 && !(l1 == l3)) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 16: Swap
	{
		forward_list l1{1, 2, 3};
		forward_list l2{4, 5};
		l1.swap(l2);
		sprt::cout << "Test 16 - Swap: ";
		if (l1.size() == 2 && l2.size() == 3 && l1.front() == 4 && l2.front() == 1) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 17: before_begin iterator access
	{
		forward_list l{1, 2, 3};
		sprt::cout << "Test 17 - before_begin iterator access: ";
		if (l.before_begin() != l.begin()) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 18: cbegin iterator access
	{
		forward_list l{1, 2, 3};
		sprt::cout << "Test 18 - cbegin iterator access: ";
		if (l.cbegin() != l.end()) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 19: cend iterator access
	{
		forward_list l{1, 2, 3};
		sprt::cout << "Test 19 - cend iterator access: ";
		if (l.cend() != l.begin()) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 20: max_size
	{
		forward_list l;
		sprt::cout << "Test 20 - max_size: ";
		if (l.max_size() > 0) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 21: memory_persistent setter
	{
		forward_list l{1, 2, 3};
		l.memory_persistent(true);
		sprt::cout << "Test 21 - memory_persistent setter: ";
		if (l.memory_persistent()) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 22: memory_persistent getter
	{
		forward_list l{1, 2, 3};
		l.memory_persistent(false);
		sprt::cout << "Test 22 - memory_persistent getter: ";
		if (!l.memory_persistent()) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 23: get_allocator
	{
		forward_list l{1, 2, 3};
		auto alloc = l.get_allocator();
		sprt::cout << "Test 23 - get_allocator: ";
		if (alloc) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}


	// Test 24: reserve function
	{
		forward_list l;
		l.reserve(10);
		sprt::cout << "Test 24 - reserve function: ";
		if (l.capacity() >= 10) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 25: capacity function
	{
		forward_list l;
		l.reserve(5);
		size_t initial_capacity = l.capacity();
		l.push_front(1);
		l.push_front(2);
		sprt::cout << "Test 25 - capacity function: ";
		if (l.capacity() >= initial_capacity) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 26: shrink_to_fit function
	{
		forward_list l;
		l.reserve(10);
		l.push_front(1);
		l.shrink_to_fit();
		sprt::cout << "Test 26 - shrink_to_fit function: ";
		if (l.capacity() >= l.size()) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 27: clear_deallocate function
	{
		forward_list l{1, 2, 3};
		l.clear_deallocate();
		sprt::cout << "Test 27 - clear_deallocate function: ";
		if (l.empty() && l.size() == 0 && l.capacity() == 0) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	sprt::cout << "\nForward list tests completed.\n";
}

} // namespace sprt
