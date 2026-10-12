/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
SPDX-License-Identifier: MIT
**/

/* WebGPU compute with readback and no window, the webgpu twin of tests/compute: a batch of
records goes up as a storage buffer with its input bytes, one compute pass transforms it, and
`Loop::captureBuffer` brings it back through a MapRead staging buffer.

    wgpucompute                       every check (the same as `wgpucompute check`)

Natively this runs over wgpu-native (Vulkan underneath); the same binary is the wasm browser
test built by `runtime/wasm-js/xwasm.sh wgpu`, where the readback path exercises the browser
mapAsync bridge (a broker-side promise, bytes into shared memory, a completion slot pumped by
the device poll — see runtime/wasm-js/webgpu.mjs).

With no adapter the checks report SKIP and exit 0. XL_COMPUTE_DEVICE picks a device index,
XL_COMPUTE_VALIDATION=1 turns wgpu-native validation on. */

#include "SPCommon.h"
#include "SPPlatform.h"

#include "XLCoreAttachment.h"
#include "XLCoreFrameQueue.h"
#include "XLCoreFrameRequest.h"
#include "XLCoreInstance.h"
#include "XLCoreQueue.h"
#include "XLCoreQueueData.h"
#include "XLWgpuDevice.h"
#include "XLWgpuLoop.h"
#include "XLWgpuObject.h"
#include "XLWgpuPlatform.h"
#include "XLWgpuQueuePass.h"

#include <sprt/c/__sprt_stdio.h>
#include <sprt/c/__sprt_stdlib.h>
#include <sprt/cxx/atomic>
#include <sprt/cxx/mutex>
#include <sprt/cxx/thread>
#include <sprt/runtime/dispatch/looper.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::wgpucompute {

namespace wg = STAPPLER_VERSIONIZED stappler::xenolith::webgpu;

namespace {

// One record of the batch: a WGSL vec4<f32>, 16 bytes, no padding.
struct Record {
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
	float w = 0.0f;
};

// The records.comp twin of tests/compute in WGSL — the same transform, so the CPU-side
// expectations are shared shape. Bound as the pipeline's single storage buffer.
constexpr StringView RecordsWgslSrc = R"WGSL(
// One record per invocation; the bound buffer is the whole batch, so its length is the count.
@group(0) @binding(0) var<storage, read_write> records: array<vec4<f32>>;

@compute @workgroup_size(256)
fn main(@builtin(global_invocation_id) id: vec3<u32>) {
	let i = id.x;
	if (i >= arrayLength(&records)) {
		return;
	}
	records[i] = records[i] * vec4<f32>(2.0, 3.0, -1.0, 0.5) + vec4<f32>(1.0, 0.0, 7.0, 0.0);
}
)WGSL";

constexpr StringView RecordsPipelineName = "RecordsPipeline";

SpanView<uint32_t> recordsProgram() {
	// WGSL text padded into u32 words — the form the wgpu Shader module expects
	// (anything without the SPIR-V magic is WGSL, see wg::Shader::setup).
	static Vector<uint32_t> words = [] {
		Vector<uint32_t> ret;
		ret.resize((RecordsWgslSrc.size() + sizeof(uint32_t) - 1) / sizeof(uint32_t), 0);
		sprt::memcpy(ret.data(), RecordsWgslSrc.data(), RecordsWgslSrc.size());
		return ret;
	}();
	return SpanView<uint32_t>(words);
}

struct RecordsInput : public core::AttachmentInputData {
	Rc<wg::Buffer> target;
	uint32_t count = 0;
};

enum class SetupResult {
	Ok,
	Skip, // no adapter at all
	Fail, // an adapter exists and the loop or the queue did not come up
};

// What one asynchronous callback delivered. Held by reference from the callback, so a callback
// that comes late — or twice — is still counted.
struct CallState : public Ref {
	uint32_t calls = 0;
	bool success = false;
	Bytes data;
	uint64_t time = 0; // monotonic, microseconds, of the first call
};

sprt::dispatch::Looper *s_looper = nullptr;
Rc<core::Instance> s_instance;
uint32_t s_pumpQuantum = 100;

uint64_t now() { return sp::platform::clock(ClockType::Monotonic); }

uint32_t s_checks = 0;
uint32_t s_failures = 0;

// Output is a pipe under the runner: flushed per line, so a killed run still shows what passed.
void flushOutputImpl() { __sprt_fflush(__sprt_stdout_impl()); }

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
	flushOutputImpl();
}

} // namespace

void flushOutput() { flushOutputImpl(); }

// The batch the shader transforms. Inputs are small integers, so the transform is exact in float32.
Record makeInput(uint32_t i) {
	return Record{float(i % 4'096), float((i * 7) % 4'096), float((i * 13) % 4'096),
		float(((i * 3) % 4'096) * 2)};
}

Record expectOutput(const Record &r) {
	return Record{r.x * 2.0f + 1.0f, r.y * 3.0f, -r.z + 7.0f, r.w * 0.5f};
}

// Index of the first record that differs, or `count` when all match. A view of the wrong size is
// a mismatch at 0.
uint32_t verifyOutput(BytesView view, uint32_t count) {
	if (view.size() != count * sizeof(Record)) {
		return 0;
	}
	for (uint32_t i = 0; i < count; ++i) {
		Record got;
		sprt::memcpy(&got, view.data() + i * sizeof(Record), sizeof(Record));
		auto want = expectOutput(makeInput(i));
		if (got.x != want.x || got.y != want.y || got.z != want.z || got.w != want.w) {
			return i;
		}
	}
	return count;
}

// The whole engine side of the test: one instance shared by every rig, one loop with its own
// device, and one compiled compute queue.
class Rig {
public:
	struct Batch {
		Rc<wg::Buffer> target;
		uint32_t count = 0;
	};

	static SetupResult prepareInstance(StringView &reason);
	static void releaseInstance();

	SetupResult init();
	void finalize(uint32_t millis = 5'000);

	wg::Loop *getLoop() const { return _loop; }
	wg::Device *getDevice() const { return _loop ? _loop->getDevice() : nullptr; }

	StringView getDeviceName() const {
		if (auto dev = getDevice()) {
			const auto &adapter = dev->getAdapterData();
			if (!adapter.description.empty()) {
				return adapter.description;
			}
			if (!adapter.device.empty()) {
				return adapter.device;
			}
			return adapter.vendor;
		}
		return StringView("(none)");
	}

	// The storage buffer holding the batch, uploaded with its input bytes.
	bool upload(Batch &, uint32_t count);

	Rc<CallState> runFrame(const Batch &);
	Rc<CallState> capture(const Batch &);

	// Polls the looper until `ready` holds or the time is out. The looper drives the loop's
	// device-poll timer, which is what delivers map callbacks and fences.
	bool pump(const Callback<bool()> &ready, uint32_t millis);
	void pumpFor(uint32_t millis) { pump([] { return false; }, millis); }

protected:
	Rc<core::Queue> makeQueue();

	Rc<wg::Loop> _loop;
	Rc<core::Queue> _queue;
	const core::AttachmentData *_records = nullptr;
};

SetupResult Rig::prepareInstance(StringView &reason) {
	if (s_instance) {
		return SetupResult::Ok;
	}

	s_looper = sprt::dispatch::Looper::acquire();
	if (!s_looper) {
		reason = "no looper";
		return SetupResult::Fail;
	}

	auto info = Rc<core::InstanceInfo>::alloc();
	info->api = core::InstanceApi::WebGPU;
	if (auto env = __sprt_getenv("XL_COMPUTE_VALIDATION")) {
		if (StringView(env) == "1") {
			info->flags |= core::InstanceFlags::Validation;
		}
	}

	s_instance = wg::platform::createInstance(move(info));
	if (!s_instance) {
		reason = "no WebGPU instance";
		return SetupResult::Skip;
	}
	if (s_instance->getDeviceCount() == 0) {
		reason = "no adapter";
		s_instance = nullptr;
		return SetupResult::Skip;
	}
	return SetupResult::Ok;
}

void Rig::releaseInstance() {
	s_instance = nullptr;
	s_looper = nullptr;
}

SetupResult Rig::init() {
	auto loopInfo = Rc<core::LoopInfo>::alloc();
	if (auto env = __sprt_getenv("XL_COMPUTE_DEVICE")) {
		loopInfo->deviceIdx = uint32_t(StringView(env).readInteger(10).get(0));
	}

	_loop = s_instance->makeLoop(s_looper, move(loopInfo)).cast<wg::Loop>();
	if (!_loop || !getDevice()) {
		sprt::cout << "[ FAIL ] setup: the loop has no device\n";
		return SetupResult::Fail;
	}

	// A loop that is not running drops every frame and every compilation without a word.
	_loop->run();

	_queue = makeQueue();
	if (!_queue) {
		sprt::cout << "[ FAIL ] setup: the queue was not built\n";
		return SetupResult::Fail;
	}

	auto state = Rc<CallState>::alloc();
	_loop->compileQueue(_queue, [state](bool success) {
		++state->calls;
		state->success = success;
	});
	if (!pump([&] { return state->calls > 0; }, 20'000) || !state->success) {
		sprt::cout << "[ FAIL ] setup: the queue did not compile\n";
		return SetupResult::Fail;
	}

	_records = _queue->getAttachment("Records");
	return _records ? SetupResult::Ok : SetupResult::Fail;
}

void Rig::finalize(uint32_t millis) {
	if (!_loop) {
		return;
	}
	if (_loop->isRunning()) {
		_loop->stop();
		pump([&] { return !_loop->isRunning(); }, millis);
	}
	_queue = nullptr;
	_records = nullptr;
	_loop = nullptr;
	pumpFor(50);
}

bool Rig::upload(Batch &batch, uint32_t count) {
	auto dev = getDevice();
	if (!dev) {
		return false;
	}

	Bytes input;
	input.resize(count * sizeof(Record));
	for (uint32_t i = 0; i < count; ++i) {
		auto r = makeInput(i);
		sprt::memcpy(input.data() + i * sizeof(Record), &r, sizeof(Record));
	}

	batch.count = count;
	batch.target = Rc<wg::Buffer>::create(*dev,
			core::BufferInfo(core::BufferUsage::StorageBuffer | core::BufferUsage::TransferSrc,
					core::PassType::Compute, uint64_t(input.size())),
			BytesView(input));
	return batch.target != nullptr;
}

Rc<CallState> Rig::runFrame(const Batch &batch) {
	auto state = Rc<CallState>::alloc();

	auto req = Rc<core::FrameRequest>::create(_queue);
	auto input = Rc<RecordsInput>::alloc();
	input->target = batch.target;
	input->count = batch.count;
	req->addInput(_records, move(input));

	_loop->runRenderQueue(move(req), 0, [state](bool success) {
		if (state->calls++ == 0) {
			state->time = now();
		}
		state->success = success;
	});
	return state;
}

Rc<CallState> Rig::capture(const Batch &batch) {
	auto state = Rc<CallState>::alloc();
	_loop->captureBuffer([state](const core::BufferInfo &, BytesView view) {
		if (state->calls++ == 0) {
			state->time = now();
		}
		state->success = !view.empty();
		state->data = view.bytes<memory::StandartInterface>();
	}, batch.target);
	return state;
}

bool Rig::pump(const Callback<bool()> &ready, uint32_t millis) {
	auto deadline = now() + uint64_t(millis) * 1'000;
	while (now() < deadline) {
		if (ready()) {
			return true;
		}
		if (s_looper->poll() == 0) {
			sp::platform::sleep(s_pumpQuantum);
		}
	}
	return ready();
}

Rc<core::Queue> Rig::makeQueue() {
	core::Queue::Builder builder("ComputeRecords");

	// WGSL has no reflection here: the stage the pipeline checks and the storage binding's
	// read-write mode both come from what we declare (the vk twin's SPIR-V carried them in
	// the module; a compute-stage storage descriptor becomes read_write storage in wg).
	core::ProgramInfo recordsInfo;
	recordsInfo.stage = core::ProgramStage::Compute;
	recordsInfo.bindings.emplace_back(core::ProgramDescriptorBinding{0, 0,
			core::DescriptorType::StorageBuffer, 1});
	auto program = builder.addProgramByRef("RecordsComp", recordsProgram(), &recordsInfo);

	auto records = builder.addAttachemnt("Records",
			[&](core::AttachmentBuilder &attachmentBuilder) -> Rc<core::Attachment> {
		attachmentBuilder.defineAsInput();
		attachmentBuilder.setInputValidationCallback(
				[](const core::AttachmentInputData *data) { return data != nullptr; });
		// The batch lives in the input, so one compiled queue serves any size.
		attachmentBuilder.setInputSubmissionCallback(
				[](core::FrameQueue &, core::AttachmentHandle &handle,
						core::AttachmentInputData *data, Function<void(bool)> &&cb) {
			auto input = static_cast<RecordsInput *>(data);
			auto &buffers = static_cast<wg::BufferAttachmentHandle &>(handle);
			buffers.clearBufferViews();
			if (!input || !input->target) {
				cb(false);
				return;
			}
			buffers.addBufferView(Rc<wg::Buffer>(input->target.get()));
			cb(true);
		});
		return Rc<wg::BufferAttachment>::create(attachmentBuilder,
				core::BufferInfo(core::BufferUsage::StorageBuffer, core::PassType::Compute));
	});

	builder.addPass("RecordsPass", core::PassType::Compute, core::RenderOrdering(0),
			[&](core::QueuePassBuilder &passBuilder) -> Rc<core::QueuePass> {
		auto recordsPass = passBuilder.addAttachment(records,
				core::AttachmentDependencyInfo::make(
						core::PipelineStage::ComputeShader,
						core::AccessType::ShaderRead | core::AccessType::ShaderWrite));

		auto layout = passBuilder.addDescriptorLayout("RecordsLayout",
				[&](core::PipelineLayoutBuilder &layoutBuilder) {
			layoutBuilder.addSet([&](core::DescriptorSetBuilder &setBuilder) {
				setBuilder.addDescriptor(recordsPass, core::DescriptorType::StorageBuffer);
			});
		});

		passBuilder.addSubpass([&](core::SubpassBuilder &subpassBuilder) {
			subpassBuilder.addComputePipeline(RecordsPipelineName, layout->defaultFamily,
					core::SpecializationInfo(program));

			subpassBuilder.setCommandsCallback([records](core::FrameQueue &frame,
													   const core::SubpassData &subpass,
													   core::CommandBuffer &commands) {
				auto &buf = static_cast<wg::CommandBuffer &>(commands);
				auto pipeline = subpass.computePipelines.get(RecordsPipelineName);
				auto attachment = frame.getAttachment(records);
				if (!pipeline || !attachment) {
					return;
				}
				auto input = static_cast<RecordsInput *>(attachment->handle->getInput());
				if (!input) {
					return;
				}

				// WebGPU orders buffer usage transitions itself; one compute pass over
				// ceil(count / 256) workgroups, the shader's declared workgroup size.
				buf.cmdBindPipeline(pipeline);
				buf.cmdDispatch((input->count + 255) / 256, 1, 1);
			});
		});

		return Rc<wg::QueuePass>::create(passBuilder);
	});

	return Rc<core::Queue>::create(move(builder));
}

namespace {

// Kills the process when a section outlives its deadline. The loop thread is the one that hangs,
// so the check cannot live on it.
struct Watchdog {
	sprt::mutex mutex;
	String section;
	sprt::atomic<uint64_t> deadline = 0;
	sprt::thread thread;
};

Watchdog *s_watchdog = nullptr;

void watchdogStart() {
	if (s_watchdog) {
		return;
	}
	s_watchdog = new Watchdog;
	s_watchdog->thread = sprt::thread([](Watchdog *w) {
		while (true) {
			sp::platform::sleep(50'000);
			auto deadline = w->deadline.load();
			if (deadline != 0 && now() > deadline) {
				sprt::unique_lock<sprt::mutex> lock(w->mutex);
				sprt::cout << "[ FAIL ] watchdog: " << w->section << " hung\n";
				sprt::cout << "wgpucompute: watchdog, the loop stopped answering\n";
				flushOutputImpl();
				__sprt__Exit(3);
			}
		}
	}, s_watchdog);
	s_watchdog->thread.detach();
}

void watchdogArm(StringView section, uint32_t seconds) {
	sprt::unique_lock<sprt::mutex> lock(s_watchdog->mutex);
	s_watchdog->section = section.str<memory::StandartInterface>();
	s_watchdog->deadline = now() + uint64_t(seconds) * 1'000'000;
}

void watchdogDisarm() { s_watchdog->deadline = 0; }

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

int performChecks() {
	StringView reason;
	switch (Rig::prepareInstance(reason)) {
	case SetupResult::Ok: break;
	case SetupResult::Skip:
		sprt::cout << "wgpucompute: SKIP no WebGPU adapter (" << reason << ")\n";
		sprt::cout << "wgpucompute: 0 checks, 0 failures\n";
		return 0;
	case SetupResult::Fail: sprt::cout << "[ FAIL ] setup: " << reason << "\n"; return 2;
	}

	watchdogStart();

	{
		Rig rig;
		watchdogArm("setup", 60);
		auto result = rig.init();
		watchdogDisarm();
		if (result != SetupResult::Ok) {
			return 2;
		}

		sprt::cout << "wgpucompute: device \"" << rig.getDeviceName() << "\"\n";
		flushOutput();

		watchdogArm("roundtrip", 60);
		for (uint32_t count : {1u, 255u, 257u, 1'000u, 10'000u, 100'000u}) {
			auto err = roundTrip(rig, count);
			check(err.empty(), toString("roundtrip/", count), err);
		}
		watchdogDisarm();

		// Fresh buffers every trip: creation, upload, dispatch and readback recycled 20 times.
		watchdogArm("repeat", 60);
		const uint32_t sizes[] = {100'000, 1'000, 10'000, 257};
		String firstError;
		for (uint32_t i = 0; i < 20; ++i) {
			auto err = roundTrip(rig, sizes[i % 4]);
			if (!err.empty() && firstError.empty()) {
				firstError = toString("frame ", i, ": ", err);
			}
		}
		check(firstError.empty(), "repeat", firstError);
		watchdogDisarm();

		watchdogArm("teardown", 30);
		rig.finalize();
		watchdogDisarm();
	}

	Rig::releaseInstance();

	sprt::cout << "wgpucompute: " << s_checks << " checks, " << s_failures << " failures\n";
	return s_failures == 0 ? 0 : 1;
}

int run(int argc, const char *argv[]) {
	StringView cmd = argc > 1 ? StringView(argv[1]) : StringView("check");
	if (cmd == "check") {
		return performChecks();
	}

	sprt::cout << "wgpucompute [check]\n";
	return 1;
}

} // namespace

} // namespace stappler::xenolith::wgpucompute

int main(int argc, const char *argv[]) {
	return stappler::perform_main(argc, argv,
			[&] { return stappler::xenolith::wgpucompute::run(argc, argv); });
}
