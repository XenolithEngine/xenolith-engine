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

#include <sprt/cxx/optional>
#include <sprt/runtime/stream.h>
#include <stdlib.h>

namespace sprt {

void performOptionalTests() {
	sprt::cout << "\n== optional tests ==\n";

	int failures = 0;

	// Test 1: Default construction and empty state
	{
		sprt::cout << "Test: Default construction and empty state\n";

		optional<int> opt;
		if (!opt.has_value()) {
			sprt::cout << "  PASS: Default constructed optional is empty\n";
		} else {
			sprt::cout << "  FAIL: Default constructed optional should be empty\n";
			failures++;
		}
	}

	// Test 2: nullopt construction
	{
		sprt::cout << "Test: nullopt construction\n";

		optional<int> opt(nullopt);
		if (!opt.has_value()) {
			sprt::cout << "  PASS: nullopt constructed optional is empty\n";
		} else {
			sprt::cout << "  FAIL: nullopt constructed optional should be empty\n";
			failures++;
		}
	}

	// Test 3: Value initialization with in_place
	{
		sprt::cout << "Test: Value initialization with in_place\n";

		optional<int> opt(in_place, 42);
		if (opt.has_value() && *opt == 42) {
			sprt::cout << "  PASS: in_place constructed optional has value 42\n";
		} else {
			sprt::cout << "  FAIL: in_place constructed optional should have value 42\n";
			failures++;
		}
	}

	// Test 4: Copy construction from non-empty optional
	{
		sprt::cout << "Test: Copy construction\n";

		optional<int> opt1(in_place, 100);
		optional<int> opt2(opt1);
		if (opt2.has_value() && *opt2 == 100) {
			sprt::cout << "  PASS: Copied optional has correct value\n";
		} else {
			sprt::cout << "  FAIL: Copied optional should have value 100\n";
			failures++;
		}
	}

	// Test 5: Move construction from non-empty optional
	{
		sprt::cout << "Test: Move construction\n";

		optional<int> opt1(in_place, 200);
		optional<int> opt2(sprt::move(opt1));
		if (opt2.has_value() && *opt2 == 200) {
			sprt::cout << "  PASS: Moved optional has correct value\n";
		} else {
			sprt::cout << "  FAIL: Moved optional should have value 200\n";
			failures++;
		}
	}

	// Test 6: Copy assignment
	{
		sprt::cout << "Test: Copy assignment\n";

		optional<int> opt1;
		optional<int> opt2(in_place, 300);
		opt1 = opt2;
		if (opt1.has_value() && *opt1 == 300) {
			sprt::cout << "  PASS: Copy assignment works correctly\n";
		} else {
			sprt::cout << "  FAIL: Copy assignment should result in value 300\n";
			failures++;
		}
	}

	// Test 7: Move assignment
	{
		sprt::cout << "Test: Move assignment\n";

		optional<int> opt1;
		optional<int> opt2(in_place, 400);
		opt1 = sprt::move(opt2);
		if (opt1.has_value()) {
			sprt::cout << "  PASS: Move assignment works correctly\n";
		} else {
			sprt::cout << "  FAIL: Move assignment should result in value\n";
			failures++;
		}
	}

	// Test 8: Assignment from nullopt (reset)
	{
		sprt::cout << "Test: Assignment from nullopt\n";

		optional<int> opt(in_place, 500);
		opt = nullopt;
		if (!opt.has_value()) {
			sprt::cout << "  PASS: Assignment from nullopt clears optional\n";
		} else {
			sprt::cout << "  FAIL: Optional should be empty after nullopt assignment\n";
			failures++;
		}
	}

	// Test 9: Assignment from value (to non-empty)
	{
		sprt::cout << "Test: Assignment from value to non-empty optional\n";

		optional<int> opt(in_place, 7);
		opt = 10;
		if (opt.has_value() && *opt == 10) {
			sprt::cout << "  PASS: Assignment to existing value works\n";
		} else {
			sprt::cout << "  FAIL: Value should be updated to 10\n";
			failures++;
		}
	}

	// Test 10: operator->
	{
		sprt::cout << "Test: operator->\n";

		struct S {
			int x = 5;
		};
		optional<S> opt(in_place, S{123});
		if (opt->x == 123) {
			sprt::cout << "  PASS: operator-> gives access to member\n";
		} else {
			sprt::cout << "  FAIL: operator-> should give access to x=123\n";
			failures++;
		}
	}

	// Test 11: operator* (const lvalue reference)
	{
		sprt::cout << "Test: operator* const &\n";

		const optional<int> opt(in_place, 42);
		if (*opt == 42) {
			sprt::cout << "  PASS: operator* on const works\n";
		} else {
			sprt::cout << "  FAIL: operator* should return value 42\n";
			failures++;
		}
	}

	// Test 12: operator* (lvalue reference)
	{
		sprt::cout << "Test: operator* &\n";

		optional<int> opt(in_place, 99);
		if (*opt == 99) {
			sprt::cout << "  PASS: operator* works\n";
		} else {
			sprt::cout << "  FAIL: operator* should return value 99\n";
			failures++;
		}
	}

	// Test 13: explicit bool conversion
	{
		sprt::cout << "Test: Explicit bool conversion\n";

		optional<int> empty;
		optional<int> with_value(in_place, 123);

		if (!empty && with_value) {
			sprt::cout << "  PASS: Bool conversion works correctly\n";
		} else {
			sprt::cout << "  FAIL: Empty should be false, non-empty should be true\n";
			failures++;
		}
	}

	// Test 14: has_value() check
	{
		sprt::cout << "Test: has_value()\n";

		optional<int> opt;
		optional<int> opt2(in_place, 50);
		if (!opt.has_value() && opt2.has_value()) {
			sprt::cout << "  PASS: has_value() works correctly\n";
		} else {
			sprt::cout << "  FAIL: has_value() should report correct state\n";
			failures++;
		}
	}

	// Test 15: value() getter
	{
		sprt::cout << "Test: value()\n";

		optional<int> opt(in_place, 87);
		const int &val = opt.value();
		if (val == 87) {
			sprt::cout << "  PASS: value() returns correct value\n";
		} else {
			sprt::cout << "  FAIL: value() should return 87\n";
			failures++;
		}
	}

	// Test 16: value_or with default when empty
	{
		sprt::cout << "Test: value_or (empty)\n";

		optional<int> opt;
		int result = opt.value_or(42);
		if (result == 42) {
			sprt::cout << "  PASS: value_or returns default for empty optional\n";
		} else {
			sprt::cout << "  FAIL: value_or should return 42 for empty optional\n";
			failures++;
		}
	}

	// Test 17: value_or with value when not empty
	{
		sprt::cout << "Test: value_or (with value)\n";

		optional<int> opt(in_place, 99);
		int result = opt.value_or(42);
		if (result == 99) {
			sprt::cout << "  PASS: value_or returns contained value\n";
		} else {
			sprt::cout << "  FAIL: value_or should return 99 for non-empty optional\n";
			failures++;
		}
	}

	// Test 18: emplace to reinitialize
	{
		sprt::cout << "Test: emplace\n";

		optional<int> opt(in_place, 10);
		opt.emplace(50);
		if (*opt == 50) {
			sprt::cout << "  PASS: emplace updates value correctly\n";
		} else {
			sprt::cout << "  FAIL: emplace should update value to 50\n";
			failures++;
		}
	}

	// Test 19: reset() to clear value
	{
		sprt::cout << "Test: reset()\n";

		optional<int> opt(in_place, 60);
		opt.reset();
		if (!opt.has_value()) {
			sprt::cout << "  PASS: reset() clears optional\n";
		} else {
			sprt::cout << "  FAIL: reset() should clear the optional\n";
			failures++;
		}
	}

	// Test 20: swap with another optional
	{
		sprt::cout << "Test: swap()\n";

		optional<int> opt1(in_place, 10);
		optional<int> opt2(in_place, 20);
		opt1.swap(opt2);
		if (*opt1 == 20 && *opt2 == 10) {
			sprt::cout << "  PASS: swap exchanges values correctly\n";
		} else {
			sprt::cout << "  FAIL: swap should exchange values (expected opt1=20, opt2=10)\n";
			failures++;
		}
	}

	// Test 21: Iterator support (begin/end)
	{
		sprt::cout << "Test: Iterator support\n";

		optional<int> opt(in_place, 42);
		auto it = opt.begin();
		if (it == &opt.value()) {
			sprt::cout << "  PASS: begin() returns pointer to value\n";
		} else {
			sprt::cout << "  FAIL: begin() should return pointer to value\n";
			failures++;
		}

		opt.reset();
		if (!opt.begin()) {
			sprt::cout << "  PASS: begin() on empty optional returns nullptr\n";
		} else {
			sprt::cout << "  FAIL: begin() on empty should return nullptr\n";
			failures++;
		}
	}

	// Test 22: Comparison with nullopt
	{
		sprt::cout << "Test: Comparison with nullopt\n";

		optional<int> opt1;
		optional<int> opt2(in_place, 5);

		bool test1 = (opt1 == nullopt);
		bool test2 = !(opt2 == nullopt);
		if (test1 && test2) {
			sprt::cout << "  PASS: Comparison with nullopt works\n";
		} else {
			sprt::cout << "  FAIL: Empty should equal nullopt, non-empty should not\n";
			failures++;
		}
	}

	// Test 23: Comparison between optionals
	{
		sprt::cout << "Test: Comparison between optionals\n";

		optional<int> opt1(in_place, 10);
		optional<int> opt2(in_place, 20);
		optional<int> opt3;

		if (opt1 < opt2 && !(opt1 > opt2)) {
			sprt::cout << "  PASS: Comparison operators work\n";
		} else {
			sprt::cout << "  FAIL: Comparison between optionals failed\n";
			failures++;
		}
	}

	// Test 24: Comparison with T (lvalue)
	{
		sprt::cout << "Test: Comparison with T\n";

		optional<int> opt(in_place, 50);
		int value = 50;

		if (opt == value) {
			sprt::cout << "  PASS: Comparison with T works\n";
		} else {
			sprt::cout << "  FAIL: Optional should equal its contained value\n";
			failures++;
		}
	}

	// Test 25: make_optional
	{
		sprt::cout << "Test: make_optional\n";

		auto opt1 = sprt::make_optional(42);
		auto opt2 = sprt::make_optional<int>(100);

		if (opt1.has_value() && *opt1 == 42 && opt2.has_value() && *opt2 == 100) {
			sprt::cout << "  PASS: make_optional works correctly\n";
		} else {
			sprt::cout << "  FAIL: make_optional should create optionals with correct values\n";
			failures++;
		}
	}

	// Test 26: Copy assignment from empty optional
	{
		sprt::cout << "Test: Copy assignment from empty\n";

		optional<int> opt1(in_place, 75);
		optional<int> opt2;
		opt1 = opt2; // assign empty to non-empty
		if (!opt1.has_value()) {
			sprt::cout << "  PASS: Non-empty assigned from empty becomes empty\n";
		} else {
			sprt::cout << "  FAIL: Non-empty should become empty when assigned from empty\n";
			failures++;
		}
	}

	// Test 27: Move assignment making source empty
	{
		sprt::cout << "Test: Move assignment with source becoming empty\n";

		optional<int> opt1(in_place, 80);
		optional<int> opt2;
		opt2 = sprt::move(opt1); // move from non-empty to empty
		if (!opt1.has_value() && opt2.has_value()) {
			sprt::cout << "  PASS: Move assignment transfers value correctly\n";
		} else {
			sprt::cout << "  FAIL: Source should be empty, target should have value after move\n";
			failures++;
		}
	}

	// Test 28: Self-assignment
	{
		sprt::cout << "Test: Self-assignment\n";

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wself-assign-overloaded"
		optional<int> opt(in_place, 95);
		opt = opt; // self copy assignment
		if (opt.has_value() && *opt == 95) {
			sprt::cout << "  PASS: Self-copy assignment is safe\n";
		} else {
			sprt::cout << "  FAIL: Self-copy should preserve value\n";
			failures++;
		}
#pragma clang diagnostic pop

		optional<int> opt2(in_place, 100);
		opt2 = sprt::move(opt2); // self move assignment
		if (opt2.has_value() && *opt2 == 100) {
			sprt::cout << "  PASS: Self-move assignment is safe\n";
		} else {
			sprt::cout << "  FAIL: Self-move should preserve value\n";
			failures++;
		}
	}

	// Summary
	sprt::cout << "\n== optional tests complete ==\n";
	if (failures == 0) {
		sprt::cout << "All tests passed!\n";
	} else {
		sprt::cout << failures << " test(s) failed.\n";
	}
	sprt::cout << "\n";
}

} // namespace sprt
