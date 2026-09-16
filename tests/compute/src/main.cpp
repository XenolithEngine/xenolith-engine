/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
SPDX-License-Identifier: MIT
**/

/* Vulkan compute with no window: a batch of records goes up in a staging buffer, one compute pass
copies it to the device and transforms it, and `Loop::captureBuffer` brings it back.

    computetest                       every check (the same as `computetest check`)
    computetest timings [--reps N] [--warmup N] [--pump-us N] [--fence export|polled]
                                      [--sizes 1000,10000,100000]

The checks cover the round trip on several sizes, a run of frames on one queue, and a lost device:
`vk::Device::setTestFault` makes the engine see VK_ERROR_DEVICE_LOST at submit or at the fence
check, and every request after that must be refused - once, and without hanging.

With no Vulkan loader or no physical device the checks report SKIP and exit 0. A device that exists
and does not come up is a failure. XL_COMPUTE_DEVICE picks a device index, XL_COMPUTE_VALIDATION=1
turns the validation layer on. */

#include "ComputeHarness.h"

#include "SPPlatform.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::compute {

namespace {

uint32_t s_checks = 0;
uint32_t s_failures = 0;

void check(bool ok, StringView name, StringView detail = StringView()) {
	++s_checks;
	if (ok) {
		sprt::cout << "[ OK ] " << name << "\n";
	} else {
		++s_failures;
		sprt::cout << "[ FAIL ] " << name;
		if (!detail.empty()) {
			sprt::cout << ": " << detail;
		}
		sprt::cout << "\n";
	}
	flushOutput();
}

uint64_t now() { return sp::platform::clock(ClockType::Monotonic); }

#if LINUX
uint32_t countOpenFds() {
	uint32_t ret = 0;
	sp::filesystem::ftw(FileInfo("/proc/self/fd"), [&](const FileInfo &, FileType type) {
		if (type != FileType::Dir) {
			++ret;
		}
		return true;
	}, 1);
	return ret;
}
#endif

// Upload, frame, capture, verify. Empty string on success.
String roundTrip(Rig &rig, uint32_t count) {
	Rig::Batch batch;
	if (!rig.upload(batch, count)) {
		return "upload failed";
	}
	auto frame = rig.runFrame(batch);
	if (!rig.pump([&] { return frame->calls > 0; }, 10'000)) {
		return "the frame did not complete";
	}
	if (!frame->success) {
		return "the frame failed";
	}
	auto read = rig.capture(batch);
	if (!rig.pump([&] { return read->calls > 0; }, 10'000)) {
		return "the capture did not complete";
	}
	if (!read->success) {
		return "the capture returned nothing";
	}
	auto bad = verifyOutput(read->data, count);
	if (bad != count) {
		return toString("record ", bad, " differs (", read->data.size(), " bytes for ", count,
				" records)");
	}
	return String();
}

bool bringUp(Rig &rig, StringView section, FencePath path = FencePath::Export) {
	watchdogArm(section, 60);
	auto result = rig.init(path);
	watchdogDisarm();
	return result == SetupResult::Ok;
}

void tearDown(Rig &rig, StringView section) {
	watchdogArm(section, 30);
	auto start = now();
	rig.finalize();
	watchdogDisarm();
	check(now() - start < 10'000'000, toString(section, "/teardown"));
}

// A lost device answers a request with a refusal, once. The pump runs past the first call so a
// second one is seen.
void expectOneRefusal(Rig &rig, const Rc<CallState> &state, StringView name, uint32_t millis) {
	auto start = now();
	bool arrived = rig.pump([&] { return state->calls > 0; }, millis);
	if (!arrived) {
		check(false, name, toString("no callback in ", millis, " ms"));
		return;
	}
	auto elapsed = (state->time - start) / 1'000;
	rig.pumpFor(250);
	check(state->calls == 1 && !state->success, name,
			toString("calls=", state->calls, " success=", state->success, " after ", elapsed,
					" ms"));
}

// Every section on one fence path; the names carry the path.
int performPath(FencePath path) {
	auto prefix = StringView(path == FencePath::Export ? "export" : "polled");
	auto name = [&](StringView section) { return toString(prefix, "/", section); };

	{
		Rig rig;
		if (!bringUp(rig, name("roundtrip"), path)) {
			return 2;
		}
		if (path == FencePath::Export && !rig.canExportFences()) {
			sprt::cout << "computetest: " << prefix << ": the device \"" << rig.getDeviceName()
					   << "\" cannot export fences here, the path is not run\n";
			flushOutput();
			tearDown(rig, name("roundtrip"));
			return 0;
		}
		sprt::cout << "computetest: " << prefix << ": device \"" << rig.getDeviceName() << "\"\n";
		flushOutput();

		watchdogArm(name("roundtrip"), 60);
		for (uint32_t count : {1u, 255u, 257u, 1'000u, 10'000u, 100'000u}) {
			auto err = roundTrip(rig, count);
			check(err.empty(), toString(prefix, "/roundtrip/", count), err);
		}
		watchdogDisarm();

		watchdogArm(name("repeat"), 60);
		const uint32_t sizes[] = {100'000, 1'000, 10'000, 257};
		String firstError;
		for (uint32_t i = 0; i < 20; ++i) {
			auto err = roundTrip(rig, sizes[i % 4]);
			if (!err.empty() && firstError.empty()) {
				firstError = toString("frame ", i, ": ", err);
			}
		}
		check(firstError.empty(), name("repeat"), firstError);
		watchdogDisarm();

#if LINUX
		// Every exported fence owns an fd until its handle ends.
		if (path == FencePath::Export) {
			watchdogArm(name("fd"), 120);
			auto before = countOpenFds();
			String fdError;
			for (uint32_t i = 0; i < 300 && fdError.empty(); ++i) {
				fdError = roundTrip(rig, 257);
			}
			rig.pumpFor(300);
			auto after = countOpenFds();
			sprt::cout << "computetest: " << prefix << ": open fds " << before << " -> " << after
					   << " over 300 frames\n";
			check(fdError.empty() && after <= before + 4, name("fd"),
					toString(fdError, " open fds: ", before, " before 300 frames, ", after, " after"));
			watchdogDisarm();
		}
#endif

		// The path is what was used, not what the device advertises.
		auto exported = rig.getDevice()->getExportedFenceCount();
		check(path == FencePath::Export ? exported > 0 : exported == 0, name("fences-used"),
				toString(exported, " fences exported"));

		tearDown(rig, name("roundtrip"));
	}

	// The loss cannot be undone, so each fault gets a loop and a device of its own.
	{
		Rig rig;
		if (!bringUp(rig, name("lost/submit"), path)) {
			return 2;
		}
		watchdogArm(name("lost/submit"), 30);
		Rig::Batch batch;
		auto err = roundTrip(rig, 1'000);
		check(err.empty(), name("lost/submit/before"), err);
		rig.upload(batch, 1'000);

		rig.getDevice()->setTestFault(vk::DeviceTestFault::LoseOnSubmit);

		expectOneRefusal(rig, rig.capture(batch), name("lost/submit/capture"), 5'000);

		auto frame = rig.runFrame(batch);
		expectOneRefusal(rig, frame, name("lost/submit/frame"), 5'000);
		check(rig.getDevice()->isDeviceLost(), name("lost/submit/marked"));

		auto start = now();
		auto again = rig.runFrame(batch);
		bool quick = rig.pump([&] { return again->calls > 0; }, 1'000);
		check(quick && !again->success && (again->time - start) < 100'000,
				name("lost/submit/frame-refused-early"),
				toString("calls=", again->calls, " success=", again->success, " after ",
						(again->time - start) / 1'000, " ms"));

		expectOneRefusal(rig, rig.compileAnother(), name("lost/submit/compile"), 5'000);
		watchdogDisarm();
		batch = Rig::Batch(); // buffers go before their device
		tearDown(rig, name("lost/submit"));
	}

	{
		Rig rig;
		if (!bringUp(rig, name("lost/fence-frame"), path)) {
			return 2;
		}
		watchdogArm(name("lost/fence-frame"), 30);
		Rig::Batch batch;
		rig.upload(batch, 10'000);

		rig.getDevice()->setTestFault(vk::DeviceTestFault::LoseOnFenceCheck);

		expectOneRefusal(rig, rig.runFrame(batch), name("lost/fence-frame/frame"), 5'000);
		check(rig.getDevice()->isDeviceLost(), name("lost/fence-frame/marked"));
		expectOneRefusal(rig, rig.capture(batch), name("lost/fence-frame/capture"), 5'000);
		watchdogDisarm();
		batch = Rig::Batch(); // buffers go before their device
		tearDown(rig, name("lost/fence-frame"));
	}

	{
		Rig rig;
		if (!bringUp(rig, name("lost/fence-task"), path)) {
			return 2;
		}
		watchdogArm(name("lost/fence-task"), 30);
		Rig::Batch batch;
		rig.upload(batch, 10'000);
		auto frame = rig.runFrame(batch);
		check(rig.pump([&] { return frame->calls > 0; }, 10'000) && frame->success,
				name("lost/fence-task/before"));

		rig.getDevice()->setTestFault(vk::DeviceTestFault::LoseOnFenceCheck);

		expectOneRefusal(rig, rig.capture(batch), name("lost/fence-task/capture"), 5'000);
		check(rig.getDevice()->isDeviceLost(), name("lost/fence-task/marked"));
		watchdogDisarm();
		batch = Rig::Batch(); // buffers go before their device
		tearDown(rig, name("lost/fence-task"));
	}

	// A lost device whose exported fd never fires: the loop's watch has to release the fence. On the
	// polled path the same fault is a fence check that says lost.
	{
		Rig rig;
		if (!bringUp(rig, name("lost/silent"), path)) {
			return 2;
		}
		watchdogArm(name("lost/silent"), 30);
		Rig::Batch batch;
		rig.upload(batch, 10'000);

		rig.getDevice()->setTestFault(vk::DeviceTestFault::LoseWithoutSignal);

		expectOneRefusal(rig, rig.runFrame(batch), name("lost/silent/frame"), 5'000);
		check(rig.getDevice()->isDeviceLost(), name("lost/silent/marked"));
		expectOneRefusal(rig, rig.capture(batch), name("lost/silent/capture"), 5'000);
		watchdogDisarm();
		batch = Rig::Batch(); // buffers go before their device
		tearDown(rig, name("lost/silent"));
	}

	return 0;
}

int performChecks() {
	StringView reason;
	switch (Rig::prepareInstance(reason)) {
	case SetupResult::Ok: break;
	case SetupResult::Skip:
		sprt::cout << "computetest: SKIP no Vulkan device (" << reason << ")\n";
		sprt::cout << "computetest: 0 checks, 0 failures\n";
		return 0;
	case SetupResult::Fail: sprt::cout << "[ FAIL ] setup: " << reason << "\n"; return 2;
	}

	watchdogStart();

	// Linux exports fences as sync_fd where the device can; everything else polls them on the loop's
	// timer. Both paths run where both exist.
	for (auto path : {FencePath::Export, FencePath::Polled}) {
		auto result = performPath(path);
		if (result != 0) {
			return result;
		}
	}

	Rig::releaseInstance();

	sprt::cout << "computetest: " << s_checks << " checks, " << s_failures << " failures\n";
	return s_failures == 0 ? 0 : 1;
}

struct Samples {
	Vector<uint64_t> values;

	void add(uint64_t v) { values.emplace_back(v); }

	uint64_t at(float q) {
		if (values.empty()) {
			return 0;
		}
		sprt::sort(values.begin(), values.end());
		auto idx = size_t(q * float(values.size() - 1) + 0.5f);
		return values[sprt::min(idx, values.size() - 1)];
	}

	String describe() { return toString(at(0.5f), "/", at(0.9f), "/", at(0.0f)); }
};

int performTimings(uint32_t reps, uint32_t warmup, const Vector<uint32_t> &sizes, FencePath path) {
	StringView reason;
	switch (Rig::prepareInstance(reason)) {
	case SetupResult::Ok: break;
	case SetupResult::Skip: sprt::cout << "computetest: SKIP no Vulkan device (" << reason << ")\n"; return 0;
	case SetupResult::Fail: sprt::cout << "[ FAIL ] setup: " << reason << "\n"; return 2;
	}
	watchdogStart();

	Rig rig;
	if (!bringUp(rig, "timings", path)) {
		return 2;
	}
	auto device = rig.getDeviceName().str<memory::StandartInterface>();

	int ret = 0;
	for (auto count : sizes) {
		Samples upload, dispatch, read, total;
		bool failed = false;
		for (uint32_t i = 0; i < warmup + reps && !failed; ++i) {
			watchdogArm("timings", 60);
			Rig::Batch batch;

			auto t0 = now();
			if (!rig.upload(batch, count)) {
				failed = true;
				break;
			}
			auto t1 = now();
			auto frame = rig.runFrame(batch);
			if (!rig.pump([&] { return frame->calls > 0; }, 10'000) || !frame->success) {
				failed = true;
				break;
			}
			auto t2 = frame->time;
			auto captureStart = now();
			auto state = rig.capture(batch);
			if (!rig.pump([&] { return state->calls > 0; }, 10'000) || !state->success) {
				failed = true;
				break;
			}
			auto t3 = state->time;
			watchdogDisarm();

			if (verifyOutput(state->data, count) != count) {
				failed = true;
				break;
			}
			if (i >= warmup) {
				upload.add(t1 - t0);
				dispatch.add(t2 - t1);
				read.add(t3 - captureStart);
				total.add((t1 - t0) + (t2 - t1) + (t3 - captureStart));
			}
		}
		watchdogDisarm();
		if (failed) {
			sprt::cout << "[ FAIL ] timings n=" << count << ": a repetition failed or differed\n";
			ret = 1;
			continue;
		}
		sprt::cout << "timings n=" << count << " reps=" << reps
				   << " med/p90/min us: upload=" << upload.describe()
				   << " dispatch=" << dispatch.describe() << " read=" << read.describe()
				   << " total=" << total.describe() << " device=\"" << device
				   << "\" fence="
				   << (rig.getDevice()->getExportedFenceCount() > 0 ? "export" : "polled") << "\n";
		flushOutput();
	}

	tearDown(rig, "timings");
	Rig::releaseInstance();
	return ret;
}

Vector<uint32_t> parseSizes(StringView str) {
	Vector<uint32_t> ret;
	str.split<StringView::Chars<','>>([&](StringView item) {
		auto v = item.readInteger(10).get(0);
		if (v > 0) {
			ret.emplace_back(uint32_t(v));
		}
	});
	return ret;
}

int run(int argc, const char *argv[]) {
	StringView cmd = argc > 1 ? StringView(argv[1]) : StringView("check");

	if (cmd == "check") {
		return performChecks();
	} else if (cmd == "timings") {
		uint32_t reps = 50;
		uint32_t warmup = 5;
		Vector<uint32_t> sizes{1'000, 10'000, 100'000};
		FencePath path = FencePath::Export;
		for (int i = 2; i + 1 < argc; i += 2) {
			StringView arg(argv[i]);
			StringView value(argv[i + 1]);
			if (arg == "--reps") {
				reps = uint32_t(value.readInteger(10).get(reps));
			} else if (arg == "--warmup") {
				warmup = uint32_t(value.readInteger(10).get(warmup));
			} else if (arg == "--pump-us") {
				Rig::setPumpQuantum(uint32_t(value.readInteger(10).get(100)));
			} else if (arg == "--fence") {
				path = value == "polled" ? FencePath::Polled : FencePath::Export;
			} else if (arg == "--sizes") {
				sizes = parseSizes(value);
			}
		}
		return performTimings(reps, warmup, sizes, path);
	}

	sprt::cout << "computetest [check] | timings [--reps N] [--warmup N] [--pump-us N] [--fence export|polled] [--sizes a,b,c]\n";
	return 1;
}

} // namespace

} // namespace stappler::xenolith::compute

int main(int argc, const char *argv[]) {
	return stappler::perform_main(argc, argv,
			[&] { return stappler::xenolith::compute::run(argc, argv); });
}
