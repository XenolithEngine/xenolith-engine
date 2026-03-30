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
#include <sprt/cxx/list>
#include <sprt/cxx/vector>

namespace sprt {

void performMallocListTests() {
	using list = __malloc_list<int>;

	// Test 1: Default constructor and empty check
	{
		list l;
		sprt::cout << "Test 1 - Default constructor and empty check: ";
		if (l.empty()) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 2: Size check on empty list
	{
		list l;
		sprt::cout << "Test 2 - Size check on empty list: ";
		if (l.size() == 0) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 3: Constructor with count and value
	{
		list l(3, 42);
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
		list l{1, 2, 3, 4, 5};
		sprt::cout << "Test 4 - Iterator access: ";
		sprt::__malloc_vector<int> result;
		for (const auto &val : l) { result.push_back(val); }
		if (result.size() == 5 && result[0] == 1 && result[4] == 5) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 5: Reverse iterator access
	{
		list l{1, 2, 3, 4, 5};
		sprt::cout << "Test 5 - Reverse iterator access: ";
		sprt::__malloc_vector<int> result;
		for (auto it = l.rbegin(); it != l.rend(); ++it) { result.push_back(*it); }
		if (result.size() == 5 && result[0] == 5 && result[4] == 1) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 6: Front and back access
	{
		list l{1, 2, 3};
		sprt::cout << "Test 6 - Front and back access: ";
		if (!l.empty() && l.front() == 1 && l.back() == 3) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 7: push_front and pop_front
	{
		list l;
		l.push_front(1);
		l.push_front(2);
		sprt::cout << "Test 7 - push_front and pop_front: ";
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

	// Test 8: push_back and pop_back
	{
		list l;
		l.push_back(1);
		l.push_back(2);
		sprt::cout << "Test 8 - push_back and pop_back: ";
		if (!l.empty() && l.size() == 2 && l.back() == 2) {
			l.pop_back();
			if (l.size() == 1 && l.back() == 1) {
				sprt::cout << "PASS\n";
			} else {
				sprt::cout << "FAIL\n";
			}
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 9: emplace_front and emplace_back
	{
		list l;
		l.emplace_front(1);
		l.emplace_back(2);
		sprt::cout << "Test 9 - emplace_front and emplace_back: ";
		if (!l.empty() && l.size() == 2 && l.front() == 1 && l.back() == 2) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 10: Insert at beginning
	{
		list l{2, 3, 4};
		auto it = l.insert(l.begin(), 1);
		sprt::cout << "Test 10 - Insert at beginning: ";
		if (!l.empty() && l.size() == 4 && *it == 1 && l.front() == 1) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 11: Insert at end
	{
		list l{1, 2, 3};
		auto it = l.insert(l.end(), 4);
		sprt::cout << "Test 11 - Insert at end: ";
		if (!l.empty() && l.size() == 4 && *it == 4 && l.back() == 4) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 12: Insert with count
	{
		list l{1, 4};
		l.insert(++l.begin(), 3, 2);
		sprt::cout << "Test 12 - Insert with count: ";
		if (l.size() == 5 && l.front() == 1 && l.back() == 4) {
			sprt::__malloc_vector<int> expected{1, 2, 2, 2, 4};
			bool match = true;
			auto it = l.begin();
			for (const auto &val : expected) {
				if (*it != val) {
					match = false;
					break;
				}
				++it;
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

	// Test 13: Erase single element
	{
		list l{1, 2, 3, 4};
		auto it = l.erase(++l.begin());
		sprt::cout << "Test 13 - Erase single element: ";
		if (!l.empty() && l.size() == 3 && *it == 3) {
			sprt::__malloc_vector<int> expected{1, 3, 4};
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

	// Test 14: Erase range
	{
		list l{1, 2, 3, 4, 5};
		auto it = l.erase(++l.begin(), --l.end());
		sprt::cout << "Test 14 - Erase range: ";
		if (l.size() == 2 && *it == 5) {
			sprt::__malloc_vector<int> expected{1, 5};
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

	// Test 15: Resize
	{
		list l{1, 2, 3};
		l.resize(5, 9);
		sprt::cout << "Test 15 - Resize: ";
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

	// Test 16: Assign from range
	{
		list l{1, 2, 3};
		sprt::__malloc_vector<int> source{4, 5};
		l.assign(source.begin(), source.end());

		sprt::cout << "Test 16 - Assign from range: ";
		if (l.size() == 2 && l.front() == 4 && l.back() == 5) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 17: Assign from count and value
	{
		list l{1, 2, 3};
		l.assign(3, 7);
		sprt::cout << "Test 17 - Assign from count and value: ";
		if (l.size() == 3 && l.front() == 7 && l.back() == 7) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 18: Clear
	{
		list l{1, 2, 3};
		l.clear();
		sprt::cout << "Test 18 - Clear: ";
		if (l.empty() && l.size() == 0) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 19: Copy constructor
	{
		list l1{1, 2, 3};
		list l2(l1);
		sprt::cout << "Test 19 - Copy constructor: ";
		if (l2.size() == 3 && l2.front() == 1 && l2.back() == 3) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 20: Assignment operator
	{
		list l1{1, 2, 3};
		list l2{4, 5};
		l2 = l1;
		sprt::cout << "Test 20 - Assignment operator: ";
		if (l2.size() == 3 && l2.front() == 1 && l2.back() == 3) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 21: Equality comparison
	{
		list l1{1, 2, 3};
		list l2{1, 2, 3};
		list l3{1, 2, 4};
		sprt::cout << "Test 21 - Equality comparison: ";
		if (l1 == l2 && !(l1 == l3)) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 22: Swap
	{
		list l1{1, 2, 3};
		list l2{4, 5};
		l1.swap(l2);
		sprt::cout << "Test 22 - Swap: ";
		if (l1.size() == 2 && l2.size() == 3 && l1.front() == 4 && l2.front() == 1) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 23: cbegin iterator access
	{
		list l{1, 2, 3};
		sprt::cout << "Test 23 - cbegin iterator access: ";
		if (l.cbegin() != l.end()) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 24: cend iterator access
	{
		list l{1, 2, 3};
		sprt::cout << "Test 24 - cend iterator access: ";
		if (l.cend() != l.begin()) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 25: crbegin reverse iterator access
	{
		list l{1, 2, 3};
		sprt::cout << "Test 25 - crbegin reverse iterator access: ";
		if (l.crbegin() != l.rend()) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 26: crend reverse iterator access
	{
		list l{1, 2, 3};
		sprt::cout << "Test 26 - crend reverse iterator access: ";
		if (l.crend() != l.rbegin()) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 27: max_size
	{
		list l;
		sprt::cout << "Test 27 - max_size: ";
		if (l.max_size() > 0) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 28: memory_persistent setter
	{
		list l{1, 2, 3};
		l.memory_persistent(true);
		sprt::cout << "Test 28 - memory_persistent setter: ";
		if (l.memory_persistent()) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 29: memory_persistent getter
	{
		list l{1, 2, 3};
		l.memory_persistent(false);
		sprt::cout << "Test 29 - memory_persistent getter: ";
		if (!l.memory_persistent()) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 30: get_allocator
	{
		list l{1, 2, 3};
		auto alloc = l.get_allocator();
		sprt::cout << "Test 30 - get_allocator: ";
		if (alloc) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	sprt::cout << "\nList tests completed.\n";
}

} // namespace sprt
