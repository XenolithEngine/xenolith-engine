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

namespace sprt {

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
}

} // namespace sprt
