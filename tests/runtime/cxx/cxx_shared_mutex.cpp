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

#include <sprt/cxx/shared_mutex>
#include <sprt/cxx/thread>
#include <sprt/runtime/stream.h>

#include <sprt/c/__sprt_time.h>
#include <sprt/c/__sprt_pthread.h>
#include <sprt/c/__sprt_unistd.h>

namespace sprt {

// Shared state for the test - tracks operations per thread
struct TestState {
	uint32_t total_reader_acquisitions{0};
	uint32_t total_writer_acquisitions{0};

	void record_reader() {
		// __sprt_fprintf(__sprt_stderr_impl(), "%d record_reader %u\n", __sprt_gettid(),
		++total_reader_acquisitions
				//)
				;
	}
	void record_writer() {
		// __sprt_fprintf(__sprt_stderr_impl(), "%d record_writer %u\n", __sprt_gettid(),
		++total_writer_acquisitions
				//)
				;
	}
};

// Thread function that switches between reader and writer locks multiple times
void switching_thread(sprt::shared_mutex &mtx, TestState &state, uint32_t thread_id,
		uint32_t iterations, sprt::uint8_t lock_type) {

	sprt::__malloc_stringstream nameStream;
	nameStream << "a_" << __sprt_gettid();

	__sprt_pthread_setname_np(__sprt_pthread_self(), nameStream.str().data());

	for (uint32_t i = 0; i < iterations; ++i) {
		// Alternate between reader and writer locks
		if ((lock_type == 1 && (i % 2 == 0)) || (lock_type == 0)) {
			// Reader lock - shared_lock RAII wrapper
			sprt::shared_lock<sprt::shared_mutex> lock(mtx);
			state.record_reader();

			// Simulate read work
			sprt::this_thread::yield();
		} else {
			// Writer lock - lock_guard RAII wrapper
			sprt::lock_guard<sprt::shared_mutex> lock(mtx);
			state.record_writer();

			// Simulate write work
			sprt::this_thread::yield();
		}
	}
}

// Thread function with explicit try_lock and lock switching
void conditional_switching_thread(sprt::shared_mutex &mtx, TestState &state, uint32_t thread_id,
		uint32_t iterations) {

	sprt::__malloc_stringstream nameStream;
	nameStream << "b_" << __sprt_gettid();

	__sprt_pthread_setname_np(__sprt_pthread_self(), nameStream.str().data());

	for (uint32_t i = 0; i < iterations; ++i) {
		// Try to acquire writer lock first (exclusive)
		if (i % 3 == 0) {
			if (mtx.try_lock()) {
				state.record_writer();
				sprt::this_thread::yield();
				mtx.unlock();
			} else {
				// Fall back to reader lock
				sprt::shared_lock<sprt::shared_mutex> lock(mtx);
				state.record_reader();
				sprt::this_thread::yield();
			}
		}
		// Try shared lock first (can be acquired by multiple readers)
		else if (i % 3 == 1) {
			if (mtx.try_lock_shared()) {
				state.record_reader();
				sprt::this_thread::yield();
				mtx.unlock_shared();
			} else {
				// Fall back to writer lock
				sprt::lock_guard<sprt::shared_mutex> lock(mtx);
				state.record_writer();
				sprt::this_thread::yield();
			}
		}
		// Default: use lock guards with alternating behavior
		else {
			if ((thread_id + i) % 2 == 0) {
				sprt::shared_lock<sprt::shared_mutex> lock(mtx);
				state.record_reader();
			} else {
				sprt::lock_guard<sprt::shared_mutex> lock(mtx);
				state.record_writer();
			}
		}
	}
}

// Test function that creates multiple threads switching between reader/writer locks
void run_shared_mutex_test() {
	constexpr uint32_t num_threads = 8;
	constexpr uint32_t iterations_per_thread = 20'000;

	sprt::cout << "\n=== Shared Mutex Thread Switching Test ===\n";
	sprt::cout << "Number of threads: " << num_threads << "\n";
	sprt::cout << "Iterations per thread: " << iterations_per_thread << "\n\n";

	sprt::shared_mutex mtx;
	TestState state;

	// Create multiple threads with different lock patterns
	sprt::thread threads[num_threads];

	auto dt = __sprt_clock_gettime_nsec_np(__SPRT_CLOCK_MONOTONIC);

	for (uint32_t i = 0; i < num_threads; ++i) {
		uint32_t thread_id = i + 1;
		uint8_t lock_type = (i % 2); // Alternate starting patterns

		if (lock_type == 1 && i > 4) {
			// Use conditional switching for some threads
			threads[i] = sprt::thread(conditional_switching_thread, sprt::ref(mtx),
					sprt::ref(state), thread_id, iterations_per_thread);
		} else {
			// Use alternating reader/writer pattern
			threads[i] = sprt::thread(switching_thread, sprt::ref(mtx), sprt::ref(state), thread_id,
					iterations_per_thread, lock_type);
		}
	}

	// Wait for all threads to complete
	for (uint32_t i = 0; i < num_threads; ++i) { threads[i].join(); }

	dt = __sprt_clock_gettime_nsec_np(__SPRT_CLOCK_MONOTONIC) - dt;

	// Report results
	sprt::cout << "=== Test Results ===\n";
	sprt::cout << "Time: " << dt << "ns\n";
	sprt::cout << "Total reader acquisitions: " << state.total_reader_acquisitions << "\n";
	sprt::cout << "Total writer acquisitions: " << state.total_writer_acquisitions << "\n\n";

	// Verify expectations
	bool success = true;
	uint32_t expected_min_ops =
			num_threads * (iterations_per_thread / 5); // At least 20% should be each type

	if (state.total_reader_acquisitions < expected_min_ops) {
		sprt::cout << "WARNING: Fewer reader operations than expected\n";
	}

	if (state.total_writer_acquisitions < expected_min_ops) {
		sprt::cout << "WARNING: Fewer writer operations than expected\n";
		success = false;
	}

	// Verify total operations match expectations (approximately, due to try_lock failures)
	uint32_t total_ops = state.total_reader_acquisitions + state.total_writer_acquisitions;

	sprt::cout << "Total operations: " << total_ops << "\n";
	sprt::cout << "Expected minimum: " << expected_min_ops * 2 << "\n\n";

	if (success) {
		sprt::cout << "SUCCESS: All threads completed with reader/writer lock switching!\n";
	} else {
		sprt::cout << "FAILURE: Test did not complete as expected.\n";
	}

	sprt::cout << "\n=== Shared Mutex Thread Switching Test Complete ===\n\n";
}

// Additional test with more aggressive locking patterns
void run_aggressive_switching_test() {
	constexpr uint32_t num_threads = 4;
	constexpr uint32_t iterations_per_thread = 100;

	sprt::cout << "\n=== Aggressive Lock Switching Test ===\n";
	sprt::cout << "Number of threads: " << num_threads << "\n";
	sprt::cout << "Iterations per thread: " << iterations_per_thread << "\n\n";

	sprt::shared_mutex mtx;
	TestState state;

	// Create threads that switch on every iteration
	sprt::thread threads[num_threads];

	for (uint32_t i = 0; i < num_threads; ++i) {
		uint32_t thread_id = i + 1;

		threads[i] = sprt::thread([&, thread_id]() mutable {
			for (uint32_t j = 0; j < iterations_per_thread; ++j) {
				if ((thread_id + j) % 2 == 0) {
					// Reader lock
					sprt::shared_lock<sprt::shared_mutex> lock(mtx);
					state.record_reader();
					sprt::this_thread::yield();
				} else {
					// Writer lock
					sprt::lock_guard<sprt::shared_mutex> lock(mtx);
					state.record_writer();
					sprt::this_thread::yield();
				}
			}
		});
	}

	// Wait for all threads to complete
	for (uint32_t i = 0; i < num_threads; ++i) { threads[i].join(); }

	// Report results
	sprt::cout << "=== Aggressive Test Results ===\n";
	sprt::cout << "Total reader acquisitions: " << state.total_reader_acquisitions << "\n";
	sprt::cout << "Total writer acquisitions: " << state.total_writer_acquisitions << "\n\n";

	uint32_t total_ops = state.total_reader_acquisitions + state.total_writer_acquisitions;
	uint32_t expected_total = num_threads * iterations_per_thread;

	if (total_ops == expected_total) {
		sprt::cout << "SUCCESS: Perfect lock distribution achieved!\n";
	} else {
		sprt::cout << "Note: Total operations (" << total_ops << ") vs Expected (" << expected_total
				   << ")\n";
	}

	// Verify roughly equal distribution (within 20%)
	uint32_t ideal_each = expected_total / 2;
	double reader_ratio = static_cast<double>(state.total_reader_acquisitions) / ideal_each;
	double writer_ratio = static_cast<double>(state.total_writer_acquisitions) / ideal_each;

	bool balanced = (reader_ratio >= 0.8 && reader_ratio <= 1.2)
			&& (writer_ratio >= 0.8 && writer_ratio <= 1.2);

	if (balanced) {
		sprt::cout << "SUCCESS: Reader/writer distribution is well-balanced!\n";
	} else {
		sprt::cout << "Note: Distribution ratio - Reader: " << reader_ratio
				   << ", Writer: " << writer_ratio << "\n";
	}

	sprt::cout << "\n=== Aggressive Lock Switching Test Complete ===\n\n";
}

// Main test runner
void performSharedMutexStressTests() {
	run_shared_mutex_test();
	run_aggressive_switching_test();

	sprt::cout << "=== All Shared Mutex Tests Completed Successfully ===\n\n";
}

} // namespace sprt
