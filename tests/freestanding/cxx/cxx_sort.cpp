/**
 * Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 * **/

#include <sprt/cxx/algorithm>
#include <sprt/cxx/vector>
#include <sprt/runtime/stream.h>

namespace sprt {

template <typename T>
using vector = __malloc_vector<T>;

// Helper function to check if a vector is sorted in ascending order
bool is_sorted_ascending(const vector<int> &vec) {
	for (size_t i = 1; i < vec.size(); ++i) {
		if (vec[i - 1] > vec[i]) {
			return false;
		}
	}
	return true;
}

// Helper function to check if a vector is sorted in descending order
bool is_sorted_descending(const vector<int> &vec) {
	for (size_t i = 1; i < vec.size(); ++i) {
		if (vec[i - 1] < vec[i]) {
			return false;
		}
	}
	return true;
}

void performSortTests() {
	sprt::cout << "\n== sort tests ==\n";

	int failures = 0;

	// Test 0: Basic ascending sort with default comparator
	{
		sprt::cout << "Test 0: Basic ascending sort (default comparator)\n";

		vector<int> vec{3, 2, 1, 4};
		sprt::sort(vec.begin(), vec.end());

		if (is_sorted_ascending(vec) && vec.size() == 4) {
			sprt::cout << "  PASS: Vector sorted in ascending order\n";
			sprt::cout << "\n";
		} else {
			sprt::cout << "  FAIL: Vector not sorted correctly\n";
			for (const auto &it : vec) { sprt::cout << it << " "; }
			sprt::cout << "\n";
			failures++;
		}
	}

	// Test 1: Basic ascending sort with default comparator
	{
		sprt::cout << "Test 1: Basic ascending sort (default comparator)\n";

		vector<int> vec{5, 2, 8, 1, 9, 3, 7, 4, 6};
		sprt::sort(vec.begin(), vec.end());

		if (is_sorted_ascending(vec) && vec.size() == 9) {
			sprt::cout << "  PASS: Vector sorted in ascending order\n";
			sprt::cout << "  Result: ";
			for (const auto &it : vec) { sprt::cout << it << " "; }
			sprt::cout << "\n";
		} else {
			sprt::cout << "  FAIL: Vector not sorted correctly\n";
			for (const auto &it : vec) { sprt::cout << it << " "; }
			sprt::cout << "\n";
			failures++;
		}
	}

	// Test 2: Sort with custom descending comparator
	{
		sprt::cout << "Test 2: Descending sort (custom comparator)\n";

		vector<int> vec{5, 2, 8, 1, 9, 3, 7, 4, 6};
		sprt::sort(vec.begin(), vec.end(), [](int l, int r) { return l > r; });

		if (is_sorted_descending(vec) && vec.size() == 9) {
			sprt::cout << "  PASS: Vector sorted in descending order\n";
			sprt::cout << "  Result: ";
			for (const auto &it : vec) { sprt::cout << it << " "; }
			sprt::cout << "\n";
		} else {
			sprt::cout << "  FAIL: Vector not sorted correctly\n";
			failures++;
		}
	}

	// Test 3: Empty vector
	{
		sprt::cout << "Test 3: Empty vector\n";

		vector<int> vec{};
		sprt::sort(vec.begin(), vec.end());

		if (vec.empty()) {
			sprt::cout << "  PASS: Empty vector handled correctly\n";
		} else {
			sprt::cout << "  FAIL: Empty vector not preserved\n";
			failures++;
		}
	}

	// Test 4: Single element vector
	{
		sprt::cout << "Test 4: Single element vector\n";

		vector<int> vec{42};
		sprt::sort(vec.begin(), vec.end());

		if (vec.size() == 1 && vec[0] == 42) {
			sprt::cout << "  PASS: Single element preserved correctly\n";
		} else {
			sprt::cout << "  FAIL: Single element not preserved\n";
			failures++;
		}
	}

	// Test 5: Already sorted vector (ascending)
	{
		sprt::cout << "Test 5: Already sorted vector (ascending)\n";

		vector<int> vec{1, 2, 3, 4, 5, 6, 7, 8, 9};
		auto original = vec;
		sprt::sort(vec.begin(), vec.end());

		if (vec == original && is_sorted_ascending(vec)) {
			sprt::cout << "  PASS: Already sorted vector remains unchanged\n";
		} else {
			sprt::cout << "  FAIL: Already sorted vector was modified incorrectly\n";
			failures++;
		}
	}

	// Test 6: Reverse sorted vector (should sort to ascending)
	{
		sprt::cout << "Test 6: Reverse sorted vector\n";

		vector<int> vec{9, 8, 7, 6, 5, 4, 3, 2, 1};
		sprt::sort(vec.begin(), vec.end());

		if (is_sorted_ascending(vec) && vec[0] == 1 && vec.back() == 9) {
			sprt::cout << "  PASS: Reverse sorted vector correctly sorted\n";
			sprt::cout << "  Result: ";
			for (const auto &it : vec) { sprt::cout << it << " "; }
			sprt::cout << "\n";
		} else {
			sprt::cout << "  FAIL: Reverse sorted vector not handled correctly\n";
			failures++;
		}
	}

	// Test 7: Vector with duplicate values
	{
		sprt::cout << "Test 7: Vector with duplicate values\n";

		vector<int> vec{5, 1, 3, 5, 2, 5, 4, 3, 1};
		sprt::sort(vec.begin(), vec.end());

		if (is_sorted_ascending(vec)) {
			sprt::cout << "  PASS: Vector with duplicates sorted correctly\n";
			sprt::cout << "  Result: ";
			for (const auto &it : vec) { sprt::cout << it << " "; }
			sprt::cout << "\n";
		} else {
			sprt::cout << "  FAIL: Vector with duplicates not sorted correctly\n";
			failures++;
		}
	}

	// Test 8: Two element vector
	{
		sprt::cout << "Test 8: Two element vector (out of order)\n";

		vector<int> vec{2, 1};
		sprt::sort(vec.begin(), vec.end());

		if (vec.size() == 2 && vec[0] == 1 && vec[1] == 2) {
			sprt::cout << "  PASS: Two element vector sorted correctly\n";
		} else {
			sprt::cout << "  FAIL: Two element vector not sorted correctly\n";
			failures++;
		}
	}

	// Test 9: Two element vector (already in order)
	{
		sprt::cout << "Test 9: Two element vector (in order)\n";

		vector<int> vec{1, 2};
		auto original = vec;
		sprt::sort(vec.begin(), vec.end());

		if (vec == original) {
			sprt::cout << "  PASS: Two element in-order vector preserved\n";
		} else {
			sprt::cout << "  FAIL: Two element in-order vector modified incorrectly\n";
			failures++;
		}
	}

	// Test 10: All same values
	{
		sprt::cout << "Test 10: Vector with all same values\n";

		vector<int> vec{7, 7, 7, 7, 7};
		sprt::sort(vec.begin(), vec.end());

		bool all_sevens = true;
		for (const auto &it : vec) {
			if (it != 7) {
				all_sevens = false;
				break;
			}
		}

		if (all_sevens && vec.size() == 5) {
			sprt::cout << "  PASS: Vector with all same values handled correctly\n";
		} else {
			sprt::cout << "  FAIL: Vector with all same values not preserved\n";
			failures++;
		}
	}

	// Test 11: Negative numbers
	{
		sprt::cout << "Test 11: Vector with negative numbers\n";

		vector<int> vec{-5, -2, -8, -1, -9, -3, -7, -4, -6};
		sprt::sort(vec.begin(), vec.end());

		if (is_sorted_ascending(vec) && vec[0] == -9 && vec.back() == -1) {
			sprt::cout << "  PASS: Negative numbers sorted correctly\n";
			sprt::cout << "  Result: ";
			for (const auto &it : vec) { sprt::cout << it << " "; }
			sprt::cout << "\n";
		} else {
			sprt::cout << "  FAIL: Negative numbers not sorted correctly\n";
			failures++;
		}
	}

	// Test 12: Mix of positive and negative numbers
	{
		sprt::cout << "Test 12: Mix of positive and negative numbers\n";

		vector<int> vec{-5, 3, -1, 7, -9, 0, 4, -2, 6};
		sprt::sort(vec.begin(), vec.end());

		if (is_sorted_ascending(vec) && vec[0] == -9 && vec.back() == 7) {
			sprt::cout << "  PASS: Mixed positive/negative sorted correctly\n";
			sprt::cout << "  Result: ";
			for (const auto &it : vec) { sprt::cout << it << " "; }
			sprt::cout << "\n";
		} else {
			sprt::cout << "  FAIL: Mixed positive/negative not sorted correctly\n";
			failures++;
		}
	}

	// Test 13: Large vector (stress test)
	{
		sprt::cout << "Test 13: Large vector (stress test)\n";

		constexpr int nIter = 101;

		vector<int> vec;
		vec.reserve(nIter);
		for (int i = nIter - 1; i >= 0; --i) { vec.push_back(i); }

		sprt::sort(vec.begin(), vec.end());

		if (is_sorted_ascending(vec) && vec.size() == nIter && vec[0] == 0
				&& vec.back() == nIter - 1) {
			sprt::cout << "  PASS: Large vector sorted correctly\n";
		} else {
			for (auto &it : vec) { sprt::cout << it << "\n"; }
			sprt::cout << "  FAIL: Large vector not sorted correctly\n";
			failures++;
		}
	}

	// Test 14: Custom comparator for absolute value sort
	{
		sprt::cout << "Test 14: Sort by absolute value\n";

		vector<int> vec{-5, 3, -2, 7, -1, 9};
		sprt::sort(vec.begin(), vec.end(),
				[](int a, int b) { return sprt::abs(a) < sprt::abs(b); });

		bool abs_sorted = true;
		for (size_t i = 1; i < vec.size(); ++i) {
			if (sprt::abs(vec[i - 1]) > sprt::abs(vec[i])) {
				abs_sorted = false;
				break;
			}
		}

		if (abs_sorted && vec.size() == 6) {
			sprt::cout << "  PASS: Absolute value sort works correctly\n";
			sprt::cout << "  Result: ";
			for (const auto &it : vec) { sprt::cout << it << " "; }
			sprt::cout << "\n";
		} else {
			sprt::cout << "  FAIL: Absolute value sort failed\n";
			failures++;
		}
	}

	// Test 15: Vector with zeros
	{
		sprt::cout << "Test 15: Vector with multiple zeros\n";

		vector<int> vec{0, 5, 0, -3, 0, 2, 0, 7};
		sprt::sort(vec.begin(), vec.end());

		if (is_sorted_ascending(vec)) {
			sprt::cout << "  PASS: Vector with zeros sorted correctly\n";
			sprt::cout << "  Result: ";
			for (const auto &it : vec) { sprt::cout << it << " "; }
			sprt::cout << "\n";
		} else {
			sprt::cout << "  FAIL: Vector with zeros not sorted correctly\n";
			failures++;
		}
	}

	// Test 16: Three element vector (all permutations)
	{
		sprt::cout << "Test 16: Various three element permutations\n";

		bool all_pass = true;

		vector<int> tests[][3] = {
			{{1, 2, 3}}, // already sorted
			{{3, 2, 1}}, // reverse sorted
			{{2, 1, 3}}, // swap first two
			{{1, 3, 2}}, // swap last two
			{{3, 1, 2}}, // cyclic shift
			{{2, 3, 1}} // other cyclic shift
		};

		for (auto &test : tests) {
			vector<int> vec{test[0][0], test[0][1], test[0][2]};
			sprt::sort(vec.begin(), vec.end());

			if (!is_sorted_ascending(vec) || vec.size() != 3) {
				all_pass = false;
				break;
			}
		}

		if (all_pass) {
			sprt::cout << "  PASS: All three element permutations sorted correctly\n";
		} else {
			sprt::cout << "  FAIL: Some three element permutation failed\n";
			failures++;
		}
	}

	// Test 17: Descending with custom comparator (reversed)
	{
		sprt::cout << "Test 17: Descending sort verification\n";

		vector<int> vec{-5, -2, 0, 3, 7};
		sprt::sort(vec.begin(), vec.end(), [](int a, int b) { return a > b; });

		if (is_sorted_descending(vec)) {
			sprt::cout << "  PASS: Descending sort verified\n";
			sprt::cout << "  Result: ";
			for (const auto &it : vec) { sprt::cout << it << " "; }
			sprt::cout << "\n";
		} else {
			sprt::cout << "  FAIL: Descending sort failed\n";
			failures++;
		}
	}

	// Test 18: Verify element preservation (no elements lost or added)
	{
		sprt::cout << "Test 18: Element preservation check\n";

		vector<int> original{42, -7, 0, 99, -3, 55, 1};
		vector<int> vec = original;
		sprt::sort(vec.begin(), vec.end());

		bool elements_preserved = true;

		if (vec.size() != original.size()) {
			elements_preserved = false;
		} else {
			for (const auto &orig : original) {
				bool found = false;
				for (const auto &sorted : vec) {
					if (orig == sorted) {
						found = true;
						break;
					}
				}
				if (!found) {
					elements_preserved = false;
					break;
				}
			}
		}

		if (elements_preserved && is_sorted_ascending(vec)) {
			sprt::cout << "  PASS: All elements preserved and sorted\n";
		} else {
			sprt::cout << "  FAIL: Elements not preserved or not sorted\n";
			failures++;
		}
	}

	// Summary
	sprt::cout << "\n== sort tests complete ==\n";
	if (failures == 0) {
		sprt::cout << "All tests passed!\n";
	} else {
		sprt::cout << failures << " test(s) failed.\n";
	}
	sprt::cout << "\n";
}

} // namespace sprt
