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

#include <sprt/runtime/dispatch/bus.h>
#include <sprt/runtime/platform.h>
#include <sprt/runtime/stream.h>

/* THE BUS: an event queued before a delegate was removed is not delivered to it.

`Bus::dispatchEvent` snapshots its listeners under the lock and posts the snapshot to each looper; a
delegate removed on that looper before the task runs is still in the snapshot. That used to be a call
after removal - and, for `xenolith::EventDelegate`, whose `disable` nulls the looper, a SIGSEGV in
`BusDelegate::handleEvent` (a `Label` leaving the scene while the font thread's `onFontSourceUpdated`
was in flight; seen as a rare crash of the studio's `demo-check`).

The race is made DETERMINISTIC here rather than hoped for: everything happens on the looper's own
thread, where `dispatchEvent` queues and nothing runs until the loop is polled. So "removed after the
event was queued" is two lines in a row, and a WITNESS delegate in the same snapshot says when the
task has actually run - which is what makes "was not called" mean something. */
namespace sprt {

namespace {

struct Owner : public Ref {
	virtual ~Owner() = default;
};

// What `xenolith::EventDelegate::disable` does after removing itself: it forgets its looper.
struct ForgetfulDelegate : public dispatch::BusDelegate {
	virtual ~ForgetfulDelegate() = default;
	void forgetLooper() { _looper = nullptr; }
};

int s_failed = 0;

void report(bool ok, StringView what) {
	if (!ok) {
		++s_failed;
	}
	sprt::cout << (ok ? "[ OK ] " : "[FAIL] ") << "runtime-bus: " << what << "\n";
}

// Turn the looper until `done` answers true, or give up after a while rather than hang the harness.
template <typename Fn>
bool pump(dispatch::Looper *looper, const Fn &done) {
	const auto deadline = platform::clock(platform::ClockType::Monotonic) + 5'000'000;
	while (!done()) {
		if (platform::clock(platform::ClockType::Monotonic) > deadline) {
			return false;
		}
		looper->poll();
		if (!done()) {
			looper->wait(dispatch::TimeInterval::milliseconds(5));
		}
	}
	return true;
}

} // namespace

void performBusTests() {
	sprt::cout << "\n== runtime dispatch bus tests ==\n";
	s_failed = 0;

	auto looper = dispatch::Looper::acquire();
	if (!looper) {
		report(false, "a looper for this thread");
		return;
	}

	auto bus = Rc<dispatch::Bus>::alloc();
	auto category = bus->allocateCategory(StringView("runtime-bus.event"));
	auto owner = Rc<Owner>::alloc();

	uint32_t witnessCalls = 0;
	uint32_t targetCalls = 0;

	auto witness = Rc<dispatch::BusDelegate>::create(looper, category, owner,
			[&](dispatch::Bus &, const dispatch::BusEvent &, dispatch::BusDelegate &) {
		++witnessCalls;
	});
	bus->addListener(witness);

	auto countTarget = [&](dispatch::Bus &, const dispatch::BusEvent &, dispatch::BusDelegate &) {
		++targetCalls;
	};

	// ---- delivered while attached --------------------------------------------------------------

	{
		auto target = Rc<dispatch::BusDelegate>::create(looper, category, owner,
				dispatch::BusDelegate::BusEventCallback(countTarget));
		bus->addListener(target);
		bus->dispatchEvent(Rc<dispatch::BusEvent>::alloc(category));
		report(witnessCalls == 0 && targetCalls == 0,
				"an event dispatched on the looper's own thread is queued, not delivered in place");
		report(pump(looper, [&] { return witnessCalls == 1; }) && targetCalls == 1,
				"... and reaches every attached delegate when the loop runs");
		bus->removeListener(target);
	}

	// ---- removed after it was queued --------------------------------------------------------------

	{
		targetCalls = 0;
		auto target = Rc<dispatch::BusDelegate>::create(looper, category, owner,
				dispatch::BusDelegate::BusEventCallback(countTarget));
		bus->addListener(target);
		const auto before = witnessCalls;
		bus->dispatchEvent(Rc<dispatch::BusEvent>::alloc(category));
		bus->removeListener(target);
		report(pump(looper, [&] { return witnessCalls == before + 1; }) && targetCalls == 0,
				"a delegate removed after the event was queued is not called with it");
	}

	// ---- removed, and its looper forgotten, the way xenolith::EventDelegate::disable does ----------

	{
		targetCalls = 0;
		auto target = Rc<ForgetfulDelegate>::create(looper, category, owner,
				dispatch::BusDelegate::BusEventCallback(countTarget));
		bus->addListener(target);
		const auto before = witnessCalls;
		bus->dispatchEvent(Rc<dispatch::BusEvent>::alloc(category));
		bus->removeListener(target);
		target->forgetLooper();
		// Before the fix this is the SIGSEGV: `_looper->isOnThisThread()` on a null looper.
		report(pump(looper, [&] { return witnessCalls == before + 1; }) && targetCalls == 0,
				"... nor one that has forgotten its looper as well, and nothing crashes");
	}

	// ---- removed and added again before the task ran ----------------------------------------------

	{
		targetCalls = 0;
		auto target = Rc<dispatch::BusDelegate>::create(looper, category, owner,
				dispatch::BusDelegate::BusEventCallback(countTarget));
		bus->addListener(target);
		const auto before = witnessCalls;
		bus->dispatchEvent(Rc<dispatch::BusEvent>::alloc(category));
		bus->removeListener(target);
		bus->addListener(target);
		report(pump(looper, [&] { return witnessCalls == before + 1; }) && targetCalls == 1,
				"a delegate added back before the task ran is listening again, and is called");
		bus->removeListener(target);
	}

	// The witness holds the bus and the bus holds the witness: taken apart here, or both leak.
	bus->removeListener(witness);

	sprt::cout << "bus tests: " << (s_failed == 0 ? "ALL PASS" : "FAILURES")
			   << " (failures=" << s_failed << ")\n";
}

} // namespace sprt
