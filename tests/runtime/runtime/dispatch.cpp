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

#include <sprt/cxx/atomic>
#include <sprt/cxx/thread>
#include <sprt/c/__sprt_unistd.h>
#include "../tests.h"

namespace sprt {

#if !SPRT_WASM

/* A graceful wakeup suspends every handle and waits for the ones that promised to report. A
handle that cannot report must say it is suspended already, or the looper waits for it forever:
here a listening socket, a running child and a polled pipe are all armed when another thread
stops the looper. */
static bool testGracefulWakeup(dispatch::QueueEngine engine, StringView name) {
	sprt::atomic<bool> ready = false;
	sprt::atomic<bool> done = false;
	sprt::atomic<dispatch::Looper *> target = nullptr;
	Status result = Status::Pending;

	int fds[2] = {-1, -1};
	if (::__sprt_pipe(fds) != 0) {
		sprt::cout << sprt::test::failed("FAIL  ") << name << ": pipe\n";
		return false;
	}

	sprt::thread th([&] {
		auto looper = dispatch::Looper::acquire(dispatch::LooperInfo{
			.name = name,
			.workersCount = 0,
			.engineMask = engine,
		});

		auto listener = looper->listenSocket(dispatch::SocketAddress::parse(":0"),
				[](Rc<dispatch::StreamHandle> &&) { });
		auto process = looper->spawnProcess("sleep 30", [](StringView) { }, [](int, Status) { });
		auto poll = looper->listenPollableHandle(fds[0], dispatch::PollFlags::In,
				[](dispatch::NativeHandle, dispatch::PollFlags) { return Status::Ok; });

		target = looper;
		ready = true;

		result = looper->run();
		done = true;

		if (process) {
			process->cancel();
		}
		if (listener) {
			listener->cancel();
		}
		if (poll) {
			poll->cancel();
		}
		looper->poll();
	});

	while (!ready) { sprt::this_thread::sleep_for(1'000'000); }
	sprt::this_thread::sleep_for(100'000'000);
	target.load()->wakeup(dispatch::WakeupFlags::Graceful);

	for (uint32_t i = 0; i < 300 && !done; ++i) { sprt::this_thread::sleep_for(10'000'000); }

	bool ok = done;
	sprt::cout << (ok ? "PASS  " : sprt::test::failed("FAIL  ")) << name
			   << ": a graceful wakeup returns with a socket, a child and a pipe armed ("
			   << (ok ? status::getStatusName(result) : StringView("still running")) << ")\n";
	if (ok) {
		th.join();
		::__sprt_close(fds[0]);
		::__sprt_close(fds[1]);
	} else {
		// The looper never returns: leave its thread to the process exit.
		th.detach();
	}
	return ok;
}

#endif

void performDispatchTests() {
	sprt::cout << "\n== runtime dispatch tests ==\n";

	auto looper = dispatch::Looper::acquire();
	if (!looper) {
		return;
	}

	sprt::cout << "Typename: " << typeid(looper) << "\n";

	auto c = platform::clock(platform::ClockType::Realtime);

	auto handle = looper->schedule(dispatch::TimeInterval::seconds(2),
			[c](dispatch::Handle *, bool success) {
		auto t = platform::clock(platform::ClockType::Realtime) - c;
		sprt::cout << "Fn timer: " << success << ": "
				   << platform::clock(platform::ClockType::Realtime) - c << " " << t / 1'000'000
				   << "\n";
	});

	bool wakeup1Perfromed = false;
	(void)looper->scheduleTimer(dispatch::TimerInfo{
		.completion = dispatch::TimerInfo::Completion::create<bool>(&wakeup1Perfromed,
				[](bool *data, dispatch::TimerHandle *self, uint32_t value, Status status) {
		if (status != Status::Ok) {
			sprt::cout << self << " Timer1 ended: " << value << " " << status << "\n";
		} else {
			sprt::cout << self << " Timer1: " << value << " " << status << "\n";
		}

		if (value >= 5) {
			sprt::cout << self << " Timer1: cancelling;\n";
			self->cancel();
		}

		if (value > 2 && !*data) {
			*data = true;
			sprt::cout << self << " Timer1: wakeup;\n";
			dispatch::Looper::acquire()->wakeup(dispatch::WakeupFlags::Graceful);
		}
	}),
		.interval = dispatch::TimeInterval::milliseconds(500),
		.count = 100,
	});

	bool wakeup2Perfromed = false;
	(void)looper->scheduleTimer(dispatch::TimerInfo{
		.completion = dispatch::TimerInfo::Completion::create<bool>(&wakeup2Perfromed,
				[](bool *data, dispatch::TimerHandle *self, uint32_t value, Status status) {
		sprt::cout << self << " Timer2: " << value << " " << status << "\n";

		if (value >= 10) {
			sprt::cout << self << " Timer2: resetting;\n";
			self->reset(dispatch::TimerInfo{
				.interval = dispatch::TimeInterval::milliseconds(500),
				.count = 5,
			});
			return;
		}

		if (status == Status::Done) {
			sprt::cout << self << " Timer2: complete and wakeup;\n";
			dispatch::Looper::acquire()->wakeup(dispatch::WakeupFlags::Graceful);
		}
	}),
		.timeout = dispatch::TimeInterval::milliseconds(2'000),
		.interval = dispatch::TimeInterval::milliseconds(250),
		.count = 50,
	});

	sprt::thread thread([](dispatch::Looper *looper) {
		sprt::this_thread::sleep_for(100'000'000); // 100ms in nanoseconds
		looper->performOnThread([] {
			sprt::cout << "From thread1\n"; //
		}, nullptr);
		sprt::this_thread::sleep_for(500'000'000); // 500ms in nanoseconds
		looper->performOnThread([] {
			sprt::cout << "From thread1\n"; //
		}, nullptr);
	}, looper);
	thread.detach();

	sprt::thread thread2([](dispatch::Looper *looper) {
		sprt::this_thread::sleep_for(100'000'000); // 100ms in nanoseconds
		looper->performOnThread([] {
			sprt::cout << "From thread2\n"; //
		}, nullptr);
		sprt::this_thread::sleep_for(500'000'000); // 500ms in nanoseconds
		looper->performOnThread([] {
			sprt::cout << "From thread2\n"; //
		}, nullptr);
	}, looper);
	thread2.detach();

	auto status = looper->run();

	sprt::cout << "Wakeup: " << status << "\n";

	status = looper->run();

	sprt::cout << "Wakeup 2: " << status << "\n";

	status = looper->run(dispatch::TimeInterval::milliseconds(500));

	sprt::cout << "Complete: " << status << "\n";

	handle = nullptr;

	thread.join();
	thread2.join();

#if !SPRT_WASM
	testGracefulWakeup(dispatch::QueueEngine::Any, "GracefulDefault");
#if SPRT_LINUX
	testGracefulWakeup(dispatch::QueueEngine::EPoll, "GracefulEpoll");
#endif
#endif
}

} // namespace sprt
