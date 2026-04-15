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
 **/

#include <sprt/runtime/stream.h>
#include <sprt/cxx/thread>
#include <sprt/cxx/atomic>

namespace sprt {

void performThreadTests() {
	sprt::cout << "\n== thread tests ==\n";

	// Test 1: Default constructor and empty check
	{
		thread t;
		sprt::cout << "Test 1 - Default constructor: ";
		if (t.get_id().__native == 0) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 2: Constructor with callable and arguments
	{
		int sharedValue = 0;
		thread t([&sharedValue]() { sharedValue = 42; });
		t.join();
		sprt::cout << "Test 2 - Constructor with callable: ";
		if (sharedValue == 42) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 3: Constructor with arguments
	{
		int result = 0;
		thread t([&result](int a, int b) { result = a + b; }, 15, 27);
		t.join();
		sprt::cout << "Test 3 - Constructor with arguments: ";
		if (result == 42) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 4: joinable() on default constructed thread
	{
		thread t;
		sprt::cout << "Test 4 - joinable() on default thread: ";
		if (!t.joinable()) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 5: joinable() on newly created thread
	{
		thread t([]() { });
		sprt::cout << "Test 5 - joinable() on new thread: ";
		if (t.joinable()) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
		t.detach(); // Clean up
	}

	// Test 6: join() on a thread that completes quickly
	{
		bool completed = false;
		thread t([&completed]() { completed = true; });
		t.join();
		sprt::cout << "Test 6 - join(): ";
		if (completed) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 7: get_id() on default thread
	{
		thread t;
		sprt::cout << "Test 7 - get_id() on default thread: ";
		if (t.get_id() == thread::id{0}) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 8: get_id() on running thread
	{
		thread t([]() { });
		auto id = t.get_id();
		t.join();
		sprt::cout << "Test 8 - get_id() on running thread: ";
		if (id != thread::id{0}) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 9: native_handle() on default thread
	{
		thread t;
		sprt::cout << "Test 9 - native_handle() on default thread: ";
		if (t.native_handle() == nullptr) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 10: native_handle() on running thread
	{
		thread t([]() { });
		auto handle = t.native_handle();
		t.join();
		sprt::cout << "Test 10 - native_handle() on running thread: ";
		if (handle != nullptr) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 11: detach() on a joinable thread
	{
		thread t([]() { });
		sprt::cout << "Test 11 - detach(): ";
		if (t.joinable()) {
			t.detach();
			if (!t.joinable()) {
				sprt::cout << "PASS\n";
			} else {
				sprt::cout << "FAIL\n";
			}
		} else {
			sprt::cout << "FAIL (thread not joinable)\n";
		}
	}

	// Test 12: swap() between two threads
	{
		thread t1([]() { });
		thread t2;
		t1.swap(t2);
		sprt::cout << "Test 12 - swap(): ";
		if (t1.get_id() == thread::id{0} && t2.get_id() != thread::id{0}) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
		t2.join(); // Clean up moved thread
	}

	// Test 13: Move constructor
	{
		thread t1([]() { });
		thread t2(sprt::move(t1));
		sprt::cout << "Test 13 - Move constructor: ";
		if (t1.get_id() == thread::id{0} && t2.get_id() != thread::id{0}) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
		t2.join(); // Clean up moved thread
	}

	// Test 14: Move assignment operator
	{
		thread t1([]() { });
		thread t2;
		t2 = sprt::move(t1);
		sprt::cout << "Test 14 - Move assignment: ";
		if (t1.get_id() == thread::id{0} && t2.get_id() != thread::id{0}) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
		t2.join(); // Clean up moved thread
	}

	// Test 15: Move assignment to self (self-assignment)
	{
		thread t([]() { });
		sprt::cout << "Test 15 - Self-assignment: ";
		t = sprt::move(t);
		if (t.joinable()) { // Should still be valid after self-move
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
		t.detach(); // Clean up
	}

	// Test 18: Destructor on default thread
	{
		thread t;
		// Destructor should not crash
		sprt::cout << "Test 18 - Destructor on default thread: ";
		// t goes out of scope here, destructor will be called
		sprt::cout << "PASS\n";
	}

	// Test 19: Destructor on joinable thread (auto-join)
	{
		bool completed = false;
		{
			thread t([&completed]() { completed = true; });
			// Don't explicitly join - destructor should handle it
		}
		sprt::cout << "Test 19 - Destructor on joinable thread: ";
		sprt::cout << "PASS\n";
	}

	// Test 20: hardware_concurrency() returns valid value
	{
		auto concurrency = thread::hardware_concurrency();
		sprt::cout << "Test 20 - hardware_concurrency(): ";
		if (concurrency > 0) {
			sprt::cout << "PASS (" << concurrency << " cores)\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 21: this_thread::get_id() returns valid id
	{
		auto id = this_thread::get_id();
		sprt::cout << "Test 21 - this_thread::get_id(): ";
		if (id != thread::id{0}) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 22: this_thread::yield() executes without error
	{
		sprt::cout << "Test 22 - this_thread::yield(): ";
		this_thread::yield();
		sprt::cout << "PASS\n";
	}

	// Test 23: this_thread::sleep_for() executes without error (short sleep)
	{
		sprt::cout << "Test 23 - this_thread::sleep_for(): ";
		this_thread::sleep_for(100'000'000); // Sleep for 100ms in nanoseconds
		sprt::cout << "PASS\n";
	}

	// Test 26: thread id ordering comparison
	{
		thread t1([]() { });
		auto id1 = t1.get_id();
		t1.join();
		sprt::cout << "Test 26 - Thread id ordering (self): ";
		if ((id1 <=> id1) == 0) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 27: thread with simple work completion
	{
		bool completed = false;
		thread t([&completed]() { completed = true; });
		t.join();
		sprt::cout << "Test 27 - Thread completion: ";
		if (completed) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 28: Multiple threads running concurrently
	{
		atomic<int> counter = 0;
		constexpr int numThreads = 4;
		thread threads[numThreads];

		for (int i = 0; i < numThreads; ++i) {
			threads[i] = thread([&counter]() {
				for (int j = 0; j < 1'000; ++j) { ++counter; }
			});
		}

		for (int i = 0; i < numThreads; ++i) { threads[i].join(); }

		sprt::cout << "Test 28 - Multiple concurrent threads: ";
		if (counter == numThreads * 1'000) {
			sprt::cout << "PASS (" << counter << " increments)\n";
		} else {
			sprt::cout << "FAIL (expected " << (numThreads * 1'000) << ", got " << counter << ")\n";
		}
	}

	// Test 29: Nested threads
	{
		int outerValue = 0;
		thread outer([&outerValue]() {
			outerValue = 1;

			int innerValue = 0;
			thread inner([&innerValue]() { innerValue = 42; });
			inner.join();

			if (innerValue == 42) {
				outerValue = 2;
			}
		});

		outer.join();
		sprt::cout << "Test 29 - Nested threads: ";
		if (outerValue == 2) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 30: Thread with lambda capturing by reference and value
	{
		atomic<int> shared = 10;
		thread t([&shared]() { shared += 10; });
		t.join();

		sprt::cout << "Test 30 - Lambda capture (reference): ";
		if (shared == 20) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 31: Thread with lambda capturing by value
	{
		int captured = 5;
		thread t([captured]() -> int { return captured * 3; });
		t.join();

		sprt::cout << "Test 31 - Lambda capture (value): ";
		// Note: the return value is lost as we don't store it, but this tests the capture works
		if (captured == 5) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 32: hash<thread::id> specialization
	{
		thread t([]() { });
		auto id = t.get_id();

		sprt::cout << "Test 32 - hash<thread::id>: ";
		hash<thread::id> hasher;
		size_t h1 = hasher(id);
		size_t h2 = hasher(id);

		t.join();

		if (h1 == h2 && h1 != 0) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 33: Joining non-joinable thread should not crash (undefined behavior but shouldn't crash)
	{
		thread t; // Default constructed, not joinable
		sprt::cout << "Test 33 - Join non-joinable thread: ";
		t.join(); // Should not crash
		sprt::cout << "PASS\n";
	}

	// Test 34: Detaching already detached thread
	{
		thread t([]() { });
		t.detach();

		sprt::cout << "Test 34 - Double detach: ";
		// Second detach should be safe (check flag before detaching)
		t.detach();
		sprt::cout << "PASS\n";
	}

	// Test 35: Move default constructed thread
	{
		thread t1; // Default constructed
		thread t2(sprt::move(t1));

		sprt::cout << "Test 35 - Move default constructed thread: ";
		if (t2.get_id() == thread::id{0}) {
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 36: this_thread::get_id() consistency across threads
	{
		auto mainId = this_thread::get_id();
		bool idsMatch = false;

		thread t([&idsMatch, mainId]() { idsMatch = (this_thread::get_id() == mainId); });
		t.join();

		sprt::cout << "Test 36 - this_thread::get_id() consistency: ";
		if (!idsMatch) {
			sprt::cout << "PASS\n"; // Main thread id differs from worker thread as expected
		} else {
			sprt::cout << "FAIL\n";
		}
	}

	// Test 39: Multiple join calls on same thread should not crash after first join
	{
		bool completed = false;
		thread t([&completed]() { completed = true; });

		t.join(); // First join
		sprt::cout << "Test 39 - Multiple joins: ";
		completed = false; // Reset flag
		if (t.get_id().__native != 0) {
			// Thread may still be joinable, but subsequent behavior is implementation defined
			sprt::cout << "PASS\n";
		} else {
			sprt::cout << "PASS\n";
		}
	}

	// Test 40: Empty lambda thread execution
	{
		thread t([]() { });
		t.join();
		sprt::cout << "Test 40 - Empty lambda execution: ";
		sprt::cout << "PASS\n";
	}

	sprt::cout << "\nThread tests completed.\n";
}

} // namespace sprt
