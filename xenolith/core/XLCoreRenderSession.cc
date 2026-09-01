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

#include "XLCoreRenderSession.h"

#include <sprt/cxx/atomic>

namespace STAPPLER_VERSIONIZED stappler::xenolith::core {

#if XL_FRAME_ACCOUNT
namespace {

struct AccountClock {
	bool useRealtime = false;
	uint64_t resolution = 0; // nanoseconds, measured

	// Reads until the clock moves and returns the step. Bounded: a clock that never advances must
	// not hang the first frame that asks for the time. 200k reads is a few milliseconds even on a
	// slow board, well past any plausible tick.
	static uint64_t probe(ClockType type) {
		const auto start = sp::platform::nanoclock(type);
		for (uint32_t i = 0; i < 200'000; ++i) {
			auto now = sp::platform::nanoclock(type);
			if (now > start) {
				return now - start;
			}
		}
		return 0;
	}

	AccountClock() {
		auto mono = probe(ClockType::Monotonic);
		auto real = probe(ClockType::Realtime);

		// Zero means the probe never saw it move: the worst case, not the best, or a dead clock
		// would win the comparison below.
		if (mono == 0) {
			mono = maxOf<uint64_t>();
		}
		if (real == 0) {
			real = maxOf<uint64_t>();
		}

		// Monotonic wins ties: it is the correct clock for a duration, and realtime is taken only
		// when it is measurably better. A wall clock stepped mid-run would corrupt a sample; the
		// boards where realtime wins have no RTC and no time sync, so there is nothing to step it.
		useRealtime = real < mono;
		resolution = useRealtime ? real : mono;
	}
};

static const AccountClock &getClock() {
	static const AccountClock clock;
	return clock;
}

} // namespace

uint64_t getAccountClock() {
	return sp::platform::nanoclock(
			getClock().useRealtime ? ClockType::Realtime : ClockType::Monotonic);
}

uint64_t getAccountClockResolution() { return getClock().resolution; }

StringView getAccountClockName() { return getClock().useRealtime ? "realtime" : "monotonic"; }

namespace {

// Same grammar as the other instruments: N = report every N frames, unset or 0 = off, anything
// unparseable = 60.
static uint64_t timelineInterval() {
	static const uint64_t value = [] () -> uint64_t {
		auto env = ::getenv("XL_FRAME_TIMELINE");
		if (!env) {
			return 0;
		}
		auto str = StringView(env);
		if (str == "0") {
			return 0;
		}
		auto n = str.readInteger(10).get(0);
		return n > 0 ? uint64_t(n) : 60;
	}();
	return value;
}

static const char *s_markNames[toInt(FrameMark::Count)] = {
	"render",
	"postPresent",
	"toApp",
	"update",
	"visit",
	"toLoop",
};

// Relaxed atomics: the marks are made from three threads but strictly in sequence, so there is no
// race to lose - only a publication to make. One increment per mark is nothing beside a frame.
static sprt::atomic<uint64_t> s_bucket[toInt(FrameMark::Count)] = {};
static sprt::atomic<uint64_t> s_prevMark{0};
static sprt::atomic<uint64_t> s_closed{0};

} // namespace

bool isFrameTimelineEnabled() { return timelineInterval() != 0; }

void markFrame(FrameMark mark) {
	auto interval = timelineInterval();
	if (interval == 0 || mark >= FrameMark::Count) {
		return;
	}

	auto now = getAccountClock();
	auto prev = s_prevMark.exchange(now);

	// The first mark of a run has nothing to measure from and opens the account instead of
	// contributing to it.
	if (prev != 0) {
		s_bucket[toInt(mark)].fetch_add(now - prev);
	}

	// The timeline closes at Presented: that is the mark the period is counted in, and reporting
	// anywhere else would divide sums that cover a different number of frames.
	if (mark != FrameMark::Presented) {
		return;
	}

	auto frames = s_closed.fetch_add(1) + 1;
	if (frames % interval != 0) {
		return;
	}

	uint64_t total = 0;
	uint64_t bucket[toInt(FrameMark::Count)];
	for (uint32_t i = 0; i < toInt(FrameMark::Count); ++i) {
		bucket[i] = s_bucket[i].load();
		total += bucket[i];
	}

	// Nanoseconds in, microseconds out - the other instruments print microseconds and the whole
	// point of this one is to be read beside them.
	auto per = [&] (uint64_t v) { return double(v) / double(frames) / 1'000.0; };
	auto pct = [&] (uint64_t v) { return total ? double(v) * 100.0 / double(total) : 0.0; };

	log::source().debug("frame::timeline", "frames=", frames, " period=", per(total),
			"us/frame (", total ? 1'000'000'000.0 * double(frames) / double(total) : 0.0, " fps)",
			" clock=", getAccountClockName(), " res=",
			double(getAccountClockResolution()) / 1'000.0, "us");

	for (uint32_t i = 0; i < toInt(FrameMark::Count); ++i) {
		log::source().debug("frame::timeline", "  ", s_markNames[i], "=", per(bucket[i]), "us ",
				pct(bucket[i]), "%");
	}
}
#endif

// Out-of-line virtual destructors: each is the vtable key function (anchoring the vtable and
// typeinfo in this single TU). They are defaulted, so the deleting destructor variant calls
// operator delete -- safe in this freestanding build with exceptions disabled, so the warning is
// suppressed here.
__SPRT_PUSH_ALLOW_CXXABI_ALLOC

RenderClientChannel::~RenderClientChannel() = default;

RenderServerChannel::~RenderServerChannel() = default;

__SPRT_POP_ALLOW_CXXABI_ALLOC

void RenderServerChannel::setWindowExtent(Extent2, Function<void(Status)> &&cb, Ref *) {
	if (cb) {
		cb(Status::ErrorNotSupported);
	}
}

Status RenderServerChannel::openDialog(NotNull<sprt::window::DialogRequest> req) {
	// No OS behind this channel (RemoteWindow). Answer rather than drop, so the caller does not
	// wait forever for a callback that will never come.
	if (req->callback) {
		sprt::window::DialogResult result;
		result.status = Status::Declined;
		result.type = req->type;
		req->callback(result);
	}
	return Status::Declined;
}

Status RenderServerChannel::cancelDialog(NotNull<sprt::window::DialogRequest>) {
	return Status::ErrorNotFound;
}

void RenderServerChannel::performTextInput(TextInputCommand &&) {
	// No native window behind this channel (RemoteWindow), so there is no processor to drive.
}

void RenderServerChannel::handleNativeInputEvents(Vector<InputEventData> &&events) {
	// No native window to route through: deliver straight to the client, which is what
	// handleInputEvents does anyway.
	handleInputEvents(sp::move(events));
}

void RenderServerChannel::setRenderClient(core::RenderClientChannel *c) {
	_clientRef = nullptr;
	_client = c;

	if (auto ref = dynamic_cast<Ref *>(c)) {
		_clientRef = ref;
	}
}

} // namespace stappler::xenolith::core
