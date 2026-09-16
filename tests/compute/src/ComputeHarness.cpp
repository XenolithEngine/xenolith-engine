/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
SPDX-License-Identifier: MIT
**/

#include "ComputeHarness.h"

#include "SPPlatform.h"
#include "XLCoreAttachment.h"
#include "XLCoreFrameQueue.h"
#include "XLCoreFrameRequest.h"
#include "XLCoreQueueData.h"
#include "XLVkAllocator.h"
#include "XLVkAttachment.h"
#include "XLVkDeviceQueue.h"
#include "XLVkRenderPass.h"
#include "XLVkInstance.h"
#include "XLVkPlatform.h"
#include "XLVkQueuePass.h"

#include <sprt/c/__sprt_stdio.h>
#include <sprt/c/__sprt_stdlib.h>
#include <sprt/cxx/atomic>
#include <sprt/cxx/mutex>
#include <sprt/cxx/thread>
#include <sprt/runtime/dispatch/looper.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::compute {

namespace {

#include "records.comp.h"

SpanView<uint32_t> RecordsComp(reinterpret_cast<const uint32_t *>(records_comp),
		sizeof(records_comp) / sizeof(uint32_t));

constexpr StringView RecordsPipelineName = "RecordsPipeline";

struct RecordsInput : public core::AttachmentInputData {
	Rc<vk::Buffer> target;
	Rc<vk::Buffer> staging;
	uint32_t count = 0;
};

sprt::dispatch::Looper *s_looper = nullptr;
Rc<core::Instance> s_instance;
uint32_t s_pumpQuantum = 100;

uint64_t now() { return sp::platform::clock(ClockType::Monotonic); }

} // namespace

Record makeInput(uint32_t i) {
	return Record{float(i % 4'096), float((i * 7) % 4'096), float((i * 13) % 4'096),
		float(((i * 3) % 4'096) * 2)};
}

Record expectOutput(const Record &r) {
	return Record{r.x * 2.0f + 1.0f, r.y * 3.0f, -r.z + 7.0f, r.w * 0.5f};
}

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
	info->api = core::InstanceApi::Vulkan;
	if (auto env = __sprt_getenv("XL_COMPUTE_VALIDATION")) {
		if (StringView(env) == "1") {
			info->flags |= core::InstanceFlags::Validation;
		}
	}

	auto backend = Rc<vk::InstanceBackendInfo>::alloc();
	backend->setup = [](vk::InstanceData &data, const vk::InstanceInfo &) {
		data.applicationName = StringView("computetest");
		return true;
	};
	info->backend = move(backend);

	s_instance = vk::platform::createInstance(move(info));
	if (!s_instance) {
		reason = "no Vulkan instance";
		return SetupResult::Skip;
	}
	if (s_instance->getDeviceCount() == 0) {
		reason = "no physical device";
		s_instance = nullptr;
		return SetupResult::Skip;
	}
	return SetupResult::Ok;
}

void Rig::releaseInstance() {
	s_instance = nullptr;
	s_looper = nullptr;
}

SetupResult Rig::init(FencePath path) {
	auto loopInfo = Rc<core::LoopInfo>::alloc();
	if (auto env = __sprt_getenv("XL_COMPUTE_DEVICE")) {
		loopInfo->deviceIdx = uint32_t(StringView(env).readInteger(10).get(0));
	}

	auto backend = Rc<vk::LoopBackendInfo>::alloc();
	backend->deviceSupportCallback = [](const vk::DeviceInfo &) { return true; };
	backend->deviceExtensionsCallback = [](const vk::DeviceInfo &) { return Vector<StringView>(); };
	loopInfo->backend = move(backend);

	_loop = s_instance->makeLoop(s_looper, move(loopInfo)).cast<vk::Loop>();
	if (!_loop || !_loop->getDevice()) {
		sprt::cout << "[ FAIL ] setup: the loop has no device\n";
		return SetupResult::Fail;
	}

	_loop->getDevice()->setFenceExportEnabled(path == FencePath::Export);

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

StringView Rig::getDeviceName() const {
	if (auto dev = getDevice()) {
		return StringView(dev->getInfo().properties.device10.properties.deviceName);
	}
	return StringView("(none)");
}

bool Rig::canExportFences() const {
#if LINUX
	auto dev = getDevice();
	return dev && dev->hasExternalFences();
#else
	return false;
#endif
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

	auto alloc = dev->getAllocator();
	batch.count = count;
	batch.staging = alloc->spawnPersistent(vk::AllocationUsage::HostTransitionSource,
			core::BufferInfo(core::ForceBufferUsage(core::BufferUsage::TransferSrc),
					core::PassType::Compute, uint64_t(input.size())),
			input);
	batch.target = alloc->spawnPersistent(vk::AllocationUsage::DeviceLocal,
			core::BufferInfo(core::BufferUsage::StorageBuffer | core::BufferUsage::TransferDst
							| core::BufferUsage::TransferSrc,
					core::PassType::Compute, uint64_t(input.size())));
	return batch.staging && batch.target;
}

Rc<CallState> Rig::runFrame(const Batch &batch) {
	auto state = Rc<CallState>::alloc();

	auto req = Rc<core::FrameRequest>::create(_queue);
	auto input = Rc<RecordsInput>::alloc();
	input->target = batch.target;
	input->staging = batch.staging;
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

Rc<CallState> Rig::compileAnother() {
	auto state = Rc<CallState>::alloc();
	auto queue = makeQueue();
	if (!queue) {
		state->calls = 1;
		return state;
	}
	_loop->compileQueue(queue, [state, queue](bool success) {
		++state->calls;
		state->success = success;
	});
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

void Rig::setPumpQuantum(uint32_t micros) { s_pumpQuantum = micros; }

void Rig::pumpFor(uint32_t millis) {
	pump([] { return false; }, millis);
}

Rc<core::Queue> Rig::makeQueue() {
	core::Queue::Builder builder("ComputeRecords");

	auto program = builder.addProgramByRef("RecordsComp", RecordsComp);

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
			auto &buffers = static_cast<vk::BufferAttachmentHandle &>(handle);
			buffers.clearBufferViews();
			if (!input || !input->target || !input->staging) {
				cb(false);
				return;
			}
			buffers.addBufferView(input->target.get());
			cb(true);
		});
		return Rc<vk::BufferAttachment>::create(attachmentBuilder,
				core::BufferInfo(core::BufferUsage::StorageBuffer, core::PassType::Compute));
	});

	builder.addPass("RecordsPass", core::PassType::Compute, core::RenderOrdering(0),
			[&](core::QueuePassBuilder &passBuilder) -> Rc<core::QueuePass> {
		auto recordsPass = passBuilder.addAttachment(records,
				core::AttachmentDependencyInfo::make(
						core::PipelineStage::ComputeShader | core::PipelineStage::Transfer,
						core::AccessType::ShaderWrite | core::AccessType::TransferWrite));

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
				auto &buf = static_cast<vk::CommandBuffer &>(commands);
				auto pipeline = subpass.computePipelines.get(RecordsPipelineName);
				auto attachment = frame.getAttachment(records);
				if (!pipeline || !attachment) {
					return;
				}
				auto input = static_cast<RecordsInput *>(attachment->handle->getInput());
				if (!input) {
					return;
				}

				buf.cmdCopyBuffer(input->staging, input->target);

				vk::BufferMemoryBarrier barrier(input->target, VK_ACCESS_TRANSFER_WRITE_BIT,
						VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
				buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT,
						VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, makeSpanView(&barrier, 1));

				buf.cmdBindPipelineWithDescriptors(pipeline, 0);
				buf.cmdDispatchPipeline(pipeline, input->count);
			});
		});

		return Rc<vk::QueuePass>::create(passBuilder);
	});

	return Rc<core::Queue>::create(move(builder));
}

namespace {

struct Watchdog {
	sprt::mutex mutex;
	String section;
	sprt::atomic<uint64_t> deadline = 0;
	sprt::thread thread;
};

Watchdog *s_watchdog = nullptr;

} // namespace

void watchdogStart() {
	if (s_watchdog) {
		return;
	}
	s_watchdog = new Watchdog;
	s_watchdog->thread = sprt::thread([w = s_watchdog] {
		while (true) {
			sp::platform::sleep(50'000);
			auto deadline = w->deadline.load();
			if (deadline != 0 && now() > deadline) {
				sprt::unique_lock<sprt::mutex> lock(w->mutex);
				sprt::cout << "[ FAIL ] watchdog: " << w->section << " hung\n";
				sprt::cout << "computetest: watchdog, the loop stopped answering\n";
				flushOutput();
				__sprt__Exit(3);
			}
		}
	});
	s_watchdog->thread.detach();
}

void watchdogArm(StringView section, uint32_t seconds) {
	sprt::unique_lock<sprt::mutex> lock(s_watchdog->mutex);
	s_watchdog->section = section.str<memory::StandartInterface>();
	s_watchdog->deadline = now() + uint64_t(seconds) * 1'000'000;
}

void flushOutput() { __sprt_fflush(__sprt_stdout_impl()); }

void watchdogDisarm() { s_watchdog->deadline = 0; }

} // namespace stappler::xenolith::compute
