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

#include <sprt/runtime/dispatch/looper.h>
#include <sprt/runtime/dispatch/handle.h>
#include <sprt/runtime/platform.h>

#include <sprt/c/sys/__sprt_sprt.h>
#include <sprt/cxx/thread>

#if SPRT_LINUX
#include <sprt/c/cross/__sprt_syscall.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

__SPRT_C_FUNC long int syscall(long int __sysno, ...);
#endif

namespace sprt {

namespace {

struct AddressWaitChecks {
	int failures = 0;

	template <typename... Args>
	void operator()(bool cond, Args &&...args) {
		sprt::cout << (cond ? "  PASS: " : "  FAIL: ");
		((sprt::cout << args), ...);
		sprt::cout << "\n";
		if (!cond) {
			++failures;
		}
	}
};

static uint64_t nowUs() { return platform::clock(platform::ClockType::Monotonic); }

static void bump(uint32_t *word) {
	__atomic_add_fetch(word, uint32_t(1), __ATOMIC_SEQ_CST);
	__sprt_sprt_qlock_wake_all(word, __SPRT_SPRT_LOCK_FLAG_SHARED);
}

// Run the queue in short slices until `done` or the deadline.
template <typename Queue, typename Fn>
static void pumpUntil(Queue *queue, uint64_t timeoutUs, const Fn &done) {
	auto deadline = nowUs() + timeoutUs;
	while (!done() && nowUs() < deadline) { queue->run(dispatch::TimeInterval::milliseconds(20)); }
}

template <typename Queue>
static void runCases(Queue *queue, StringView engine, AddressWaitChecks &check) {
	// 1. changes from another thread arrive, possibly merged
	{
		uint32_t word = 0;
		uint32_t last = 0;
		uint32_t calls = 0;
		auto h = queue->waitOnAddress(&word, 0, [&](uint32_t value) -> Status {
			last = value;
			++calls;
			return Status::Ok;
		});
		check(h != nullptr, engine, ": waitOnAddress returns a handle");
		if (!h) {
			return;
		}

		sprt::thread writer([&] {
			for (uint32_t i = 0; i < 3; ++i) {
				sprt::this_thread::sleep_for(20'000'000);
				bump(&word);
			}
		});
		pumpUntil(queue, 2'000'000, [&] { return last == 3; });
		writer.join();

		check(last == 3 && calls >= 1 && calls <= 3, engine,
				": changes from another thread are delivered (calls: ", calls, ")");
		check(h->getLastValue() == 3, engine, ": handle tracks the last value");
		h->cancel();
	}

	// 2. a change made before arming is not lost
	{
		uint32_t word = 7;
		uint32_t last = 0;
		auto h = queue->waitOnAddress(&word, 0, [&](uint32_t value) -> Status {
			last = value;
			return Status::Ok;
		});
		pumpUntil(queue, 1'000'000, [&] { return last == 7; });
		check(last == 7, engine, ": mismatch with expected fires at once");
		if (h) {
			h->cancel();
		}
	}

	// 3. a callback returning anything but Ok cancels the wait
	{
		uint32_t word = 0;
		uint32_t calls = 0;
		auto h = queue->waitOnAddress(&word, 0, [&](uint32_t) -> Status {
			++calls;
			return Status::Done;
		});
		queue->run(dispatch::TimeInterval::milliseconds(10));
		bump(&word);
		pumpUntil(queue, 1'000'000, [&] { return calls > 0; });
		bump(&word);
		queue->run(dispatch::TimeInterval::milliseconds(100));
		check(calls == 1 && h && h->getStatus() != Status::Ok, engine,
				": non-Ok result cancels the handle");
	}

	// 4. cancelling a blocked wait returns promptly
	{
		uint32_t word = 0;
		auto h = queue->waitOnAddress(&word, 0, [&](uint32_t) -> Status { return Status::Ok; });
		queue->run(dispatch::TimeInterval::milliseconds(30));
		auto start = nowUs();
		if (h) {
			h->cancel();
		}
		queue->run(dispatch::TimeInterval::milliseconds(10));
		auto elapsed = nowUs() - start;
		check(h && elapsed < 200'000, engine, ": cancel of a blocked wait is prompt (",
				elapsed / 1'000, "ms)");
	}

#if SPRT_LINUX
	// 5. a word in memory shared with a forked process
	{
		auto mem =
				::mmap(nullptr, 4'096, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
		if (mem == MAP_FAILED) {
			check(false, engine, ": shared mapping");
			return;
		}
		auto word = static_cast<uint32_t *>(mem);
		*word = 0;

		uint32_t last = 0;
		auto h = queue->waitOnAddress(word, 0, [&](uint32_t value) -> Status {
			last = value;
			return Status::Ok;
		});
		queue->run(dispatch::TimeInterval::milliseconds(10));

		// the child must not inherit unflushed output and write it a second time
		::fflush(nullptr);
		auto pid = ::fork();
		if (pid == 0) {
			for (uint32_t i = 0; i < 5; ++i) {
				::usleep(20'000);
				bump(word);
			}
			::_exit(0);
		}

		pumpUntil(queue, 3'000'000, [&] { return last == 5; });
		int st = 0;
		if (pid > 0) {
			::syscall(__SPRT_SYSCALL_wait4, pid, &st, 0, nullptr);
		}

		check(pid > 0 && last == 5, engine,
				": changes from another process are delivered (last: ", last, ")");
		if (h) {
			h->cancel();
		}
		queue->run(dispatch::TimeInterval::milliseconds(10));
		::munmap(mem, 4'096);
	}
#endif
}

} // namespace

void performAddressWaitTests() {
	sprt::cout << "\n== runtime waitOnAddress tests ==\n";

	AddressWaitChecks check;

#if SPRT_LINUX
	struct EngineCase {
		StringView name;
		dispatch::QueueEngine engine;
	};

	static constexpr EngineCase s_engines[] = {
		{"uring", dispatch::QueueEngine::URing},
		{"epoll", dispatch::QueueEngine::EPoll},
	};

	for (auto &it : s_engines) {
		dispatch::QueueInfo info;
		info.engineMask = it.engine;
		auto queue = dispatch::Queue::create(move(info));
		if (!queue || queue->getEngine() != it.engine) {
			sprt::cout << "  SKIP: " << it.name << " engine is not available\n";
			continue;
		}
		runCases(queue.get(), it.name, check);
	}
#else
	auto looper = dispatch::Looper::acquire();
	if (!looper) {
		sprt::cout << "  FAIL: no looper\n";
		return;
	}
	runCases(looper, "default", check);
#endif

	sprt::cout << "waitOnAddress: " << (check.failures ? "FAILED" : "PASSED")
			   << ", failures: " << check.failures << "\n";
}

} // namespace sprt
