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

#include <sprt/cxx/chrono>

namespace sprt {

void performDispatchTests() {
	sprt::cout << "\n== runtime Ref/Rc tests ==\n";

	auto looper = dispatch::Looper::acquire();
	if (!looper) {
		return;
	}

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
			sprt::cout << "Timer1 ended: " << value << " " << status << "\n";
		} else {
			sprt::cout << "Timer1: " << value << " " << status << "\n";
		}

		if (value >= 5) {
			sprt::cout << "Timer1: cancelling;\n";
			self->cancel();
		}

		if (value > 2 && !*data) {
			*data = true;
			sprt::cout << "Timer1: wakeup;\n";
			dispatch::Looper::acquire()->wakeup(dispatch::WakeupFlags::Graceful);
		}
	}),
		.interval = dispatch::TimeInterval::milliseconds(250),
		.count = 100,
	});

	bool wakeup2Perfromed = false;
	(void)looper->scheduleTimer(dispatch::TimerInfo{
		.completion = dispatch::TimerInfo::Completion::create<bool>(&wakeup2Perfromed,
				[](bool *data, dispatch::TimerHandle *self, uint32_t value, Status status) {
		sprt::cout << "Timer2: " << value << " " << status << "\n";

		if (value >= 10) {
			sprt::cout << "Timer2: resetting;\n";
			self->reset(dispatch::TimerInfo{
				.interval = dispatch::TimeInterval::milliseconds(500),
				.count = 5,
			});
			return;
		}

		if (status == Status::Done) {
			sprt::cout << "Timer2: complete and wakeup;\n";
			dispatch::Looper::acquire()->wakeup(dispatch::WakeupFlags::Graceful);
		}
	}),
		.interval = dispatch::TimeInterval::milliseconds(150),
		.count = 50,
	});

	sprt::thread thread([](dispatch::Looper *looper) {
		sprt::this_thread::sleep_for(sprt::chrono::milliseconds(100));
		looper->performOnThread([] {
			sprt::cout << "From thread1\n"; //
		}, nullptr);
		sprt::this_thread::sleep_for(sprt::chrono::milliseconds(500));
		looper->performOnThread([] {
			sprt::cout << "From thread1\n"; //
		}, nullptr);
	}, looper);

	sprt::thread thread2([](dispatch::Looper *looper) {
		sprt::this_thread::sleep_for(sprt::chrono::milliseconds(100));
		looper->performOnThread([] {
			sprt::cout << "From thread2\n"; //
		}, nullptr);
		sprt::this_thread::sleep_for(sprt::chrono::milliseconds(500));
		looper->performOnThread([] {
			sprt::cout << "From thread2\n"; //
		}, nullptr);
	}, looper);

	auto status = looper->run();

	sprt::cout << "Wakeup: " << status << "\n";

	status = looper->run();

	sprt::cout << "Wakeup 2: " << status << "\n";

	status = looper->run(dispatch::TimeInterval::milliseconds(500));

	sprt::cout << "Complete: " << status << "\n";

	handle = nullptr;

	thread.join();
	thread2.join();

	/*struct AppData {
		uint32_t timerTicks = 0;
		Rc<event::TimerHandle> timer1;
		Rc<event::TimerHandle> timer2;
		Rc<event::QueueRef> queue;
		Rc<event::ThreadHandle> thread;
	};

	AppData data;

	//data.queue = Rc<event::QueueRef>::create(event::QueueInfo(), event::QueueFlags::Protected);
*/
	/*data.timer2 = data.queue->scheduleTimer(event::TimerInfo{
			.completion = event::TimerInfo::Completion::create<AppData>(&data,
					[] (AppData *data, event::TimerHandle *self, uint32_t value, event::Status status) {
				if (!event::isSuccessful(status)) {
					log::debug("App", "Error: ", status);
				} else {
					log::debug("App", "Timer2: ", value);
				}
			}),
			.timeout = TimeInterval::seconds(5),
			.count = uint32_t(10),
		});*/


	/*data.dir = data.queue->openDir(event::OpenDirInfo{
			.completion = event::OpenDirInfo::Completion::create<AppData>(&data,
					[] (AppData *data, event::DirHandle *self, uint32_t value, event::Status err) {
				log::debug("App", "OpenDir: ", self->getPath(), ": ", err);
				self->scan([] (event::FileType type, StringView name) {
					log::debug("App", "scan: (", type, ") ", name);
				});
			}),
			.file = event::FileOpInfo{
				.path = StringView("/home/sbkarr")
			}
		});

		data.dir2 = data.queue->openDir(event::OpenDirInfo{
			.completion = event::OpenDirInfo::Completion::create<AppData>(&data,
					[] (AppData *data, event::DirHandle *self, uint32_t value, event::Status err) {
				log::debug("App", "OpenDir2: ", self->getPath(), ": ", err);
				self->scan([] (event::FileType type, StringView name) {
					log::debug("App", "scan2: (", type, ") ", name);
				});
			}),
			.file = event::FileOpInfo{
				.root = data.dir,
				.path = StringView("videos")
			}
		});

		data.stat = data.queue->stat(event::StatOpInfo{
			.completion = event::StatOpInfo::Completion::create<AppData>(&data,
					[] (AppData *data, event::StatHandle *self, uint32_t value, event::Status err) {
				log::debug("App", "Stat: ", self->getPath(), ": ", self->getStat());
			}),
			.file = event::FileOpInfo{
				.path = StringView("/home/sbkarr/image1076.png")
			}
		});*/

	//data.thread = data.queue->addThreadHandle();
}

} // namespace sprt
