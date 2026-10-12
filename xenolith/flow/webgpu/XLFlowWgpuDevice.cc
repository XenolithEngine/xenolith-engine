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
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.
 **/

#include "XLFlowWgpuDevice.h"
#include "SPFilepath.h"
#include "XLWgpuPlatform.h"
#include "XLWgpuInstance.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::flowwebgpu {

static uint64_t nowMs() {
	return uint64_t(sprt::platform::nanoclock(ClockType::Monotonic) / 1'000'000ull);
}

WgpuGpuDevice::WgpuGpuDevice(Rc<wg::Loop> &&loop) : WgpuGpuDevice(sp::move(loop), Options()) { }

WgpuGpuDevice::WgpuGpuDevice(Rc<wg::Loop> &&loop, const Options &opts)
: _loop(sp::move(loop)), _options(opts) {
	if (_loop) {
		if (auto dev = _loop->getDevice()) {
			_deviceName = dev->getAdapterData().description;
		}
	}
}

WgpuGpuDevice::~WgpuGpuDevice() {
	for (auto &it : _sets) {
		if (it->stageBuffer) {
			wgpuBufferRelease(it->stageBuffer);
			it->stageBuffer = nullptr;
		}
		if (it->reduceStageBuffer) {
			wgpuBufferRelease(it->reduceStageBuffer);
			it->reduceStageBuffer = nullptr;
		}
	}
	_sets.clear();
	_tables.clear();
	if (_blockLayout) {
		wgpuBindGroupLayoutRelease(_blockLayout);
		_blockLayout = nullptr;
	}
	if (_reduceLayout) {
		wgpuBindGroupLayoutRelease(_reduceLayout);
		_reduceLayout = nullptr;
	}
	if (_blockPipelineLayout) {
		wgpuPipelineLayoutRelease(_blockPipelineLayout);
		_blockPipelineLayout = nullptr;
	}
	if (_reducePipelineLayout) {
		wgpuPipelineLayoutRelease(_reducePipelineLayout);
		_reducePipelineLayout = nullptr;
	}
}

bool WgpuGpuDevice::isAvailable() const {
	return _loop && _loop->isRunning();
}

bool WgpuGpuDevice::isLost() const {
	// The backend does not surface a device-lost signal to a plain device user yet: a device that
	// dies mid-run shows up as flights that never answer, which the executor's silence deadline
	// breaks off exactly as a stopped loop does.
	return false;
}

auto WgpuGpuDevice::findTable(uint64_t id) const -> Table * {
	for (auto &it : _tables) {
		if (it->id == id) {
			return it.get();
		}
	}
	return nullptr;
}

bool WgpuGpuDevice::isPrepared(uint64_t id) const {
	auto t = findTable(id);
	return t && t->compiled && !t->failed;
}

void WgpuGpuDevice::prepare(uint64_t id, SpanView<flow::GpuPipelineSpec> specs) {
	if (!isAvailable() || findTable(id)) {
		return;
	}

	auto table = Rc<Table>::alloc();
	table->id = id;
	for (auto &it : specs) {
		// The words are the executor's to free once this call returns: kept here, so the compile
		// that runs on the loop's thread reads a copy.
		auto words = mem_std::Vector<uint32_t>(it.spirv.begin(), it.spirv.end());
		Pipeline pipe;
		pipe.words = sp::move(words);
		if (it.kind == flow::GpuPipelineSpec::Kind::Block) {
			if (table->findBlock(it.hash)) {
				return; // one hash twice in one table: not this lowering's shape
			}
			table->blocks.emplace_back(it.hash, sp::move(pipe));
		} else {
			if (table->findReducer(it.name)) {
				return;
			}
			table->reducers.emplace_back(it.name.str<memory::StandartInterface>(), sp::move(pipe));
		}
	}
	_tables.emplace_back(table);
	_loop->performOnThread([this, table]() mutable { compileTable(sp::move(table)); }, this, true);
}

void WgpuGpuDevice::ensureLayouts() {
	if (_layoutsReady || !_loop) {
		return;
	}

	auto dev = _loop->getDevice();
	if (!dev) {
		return;
	}

	{
		WGPUBindGroupLayoutEntry entries[3] = {WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT,
				WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT, WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT};
		entries[0].binding = 0;
		entries[0].visibility = WGPUShaderStage_Compute;
		entries[0].buffer.type = WGPUBufferBindingType_ReadOnlyStorage;
		entries[1].binding = 1;
		entries[1].visibility = WGPUShaderStage_Compute;
		entries[1].buffer.type = WGPUBufferBindingType_ReadOnlyStorage;
		entries[2].binding = 2;
		entries[2].visibility = WGPUShaderStage_Compute;
		entries[2].buffer.type = WGPUBufferBindingType_Storage;

		WGPUBindGroupLayoutDescriptor desc = WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;
		desc.label = WGPUStringView{"flowBlockLayout", WGPU_STRLEN};
		desc.entryCount = 3;
		desc.entries = entries;
		_blockLayout = wgpuDeviceCreateBindGroupLayout(dev->getDevice(), &desc);

		WGPUPipelineLayoutDescriptor pipeline = WGPU_PIPELINE_LAYOUT_DESCRIPTOR_INIT;
		pipeline.label = WGPUStringView{"flowBlockPipeline", WGPU_STRLEN};
		pipeline.bindGroupLayoutCount = 1;
		pipeline.bindGroupLayouts = &_blockLayout;
		_blockPipelineLayout = wgpuDeviceCreatePipelineLayout(dev->getDevice(), &pipeline);
	}

	{
		WGPUBindGroupLayoutEntry entries[3] = {WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT,
				WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT, WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT};
		entries[0].binding = 0;
		entries[0].visibility = WGPUShaderStage_Compute;
		entries[0].buffer.type = WGPUBufferBindingType_Storage;
		entries[1].binding = 1;
		entries[1].visibility = WGPUShaderStage_Compute;
		entries[1].buffer.type = WGPUBufferBindingType_Storage;
		entries[2].binding = 2;
		entries[2].visibility = WGPUShaderStage_Compute;
		entries[2].buffer.type = WGPUBufferBindingType_Uniform;

		WGPUBindGroupLayoutDescriptor desc = WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;
		desc.label = WGPUStringView{"flowReduceLayout", WGPU_STRLEN};
		desc.entryCount = 3;
		desc.entries = entries;
		_reduceLayout = wgpuDeviceCreateBindGroupLayout(dev->getDevice(), &desc);

		WGPUPipelineLayoutDescriptor pipeline = WGPU_PIPELINE_LAYOUT_DESCRIPTOR_INIT;
		pipeline.label = WGPUStringView{"flowReducePipeline", WGPU_STRLEN};
		pipeline.bindGroupLayoutCount = 1;
		pipeline.bindGroupLayouts = &_reduceLayout;
		_reducePipelineLayout = wgpuDeviceCreatePipelineLayout(dev->getDevice(), &pipeline);
	}

	_layoutsReady = _blockLayout && _reduceLayout && _blockPipelineLayout && _reducePipelineLayout;
}

void WgpuGpuDevice::compileTable(Rc<Table> &&table) {
	if (!_loop || !_loop->isRunning()) {
		return;
	}
	auto dev = _loop->getDevice();
	if (!dev) {
		return;
	}
	ensureLayouts();
	if (!_layoutsReady) {
		table->failed = true;
		return;
	}

#if XL_WGPU_NATIVE_API
	// A pipeline that does not compile comes back as an error object rather than a failed call, so
	// the whole build runs inside one error scope and the pop's answer decides the table.
	wgpuDevicePushErrorScope(dev->getDevice(), WGPUErrorFilter_Validation);
#endif

	bool ok = true;
	auto buildPipeline = [&](Pipeline &pipe, StringView name, WGPUPipelineLayout layout) {
		if (!ok) {
			return;
		}
		core::ProgramData program;
		program.key = name.str<memory::PoolInterface>();
		program.stage = core::ProgramStage::Compute;
		program.data = SpanView<uint32_t>(pipe.words.data(), pipe.words.size());

		if (auto dump = __sprt_getenv("XL_FLOW_WGSL_DUMP")) {
			auto path = mem_std::toString(dump, "/", name, ".wgsl");
			filesystem::write(FileInfo{path},
					(const uint8_t *)pipe.words.data(), pipe.words.size() * sizeof(uint32_t));
		}

		pipe.shader = Rc<wg::Shader>::create(*dev, program);
		if (!pipe.shader || !pipe.shader->getModule()) {
			log::source().error("xenolith::flow::webgpu", "shader module failed: ", name);
			ok = false;
			return;
		}

		WGPUComputePipelineDescriptor desc = WGPU_COMPUTE_PIPELINE_DESCRIPTOR_INIT;
		desc.label = WGPUStringView{name.data(), name.size()};
		desc.layout = layout;
		desc.compute.module = pipe.shader->getModule();
		desc.compute.entryPoint = WGPUStringView{"main", WGPU_STRLEN};
		pipe.pipeline = wgpuDeviceCreateComputePipeline(dev->getDevice(), &desc);
		if (!pipe.pipeline) {
			log::source().error("xenolith::flow::webgpu", "compute pipeline failed: ", name);
			ok = false;
		}
	};

	for (auto &it : table->blocks) {
		buildPipeline(it.second, mem_std::toString("b", it.first), _blockPipelineLayout);
	}
	for (auto &it : table->reducers) {
		buildPipeline(it.second, it.first, _reducePipelineLayout);
	}

	// Warm every pipeline through one no-op dispatch before the table answers prepared: a driver
	// compiles a pipeline at its first use, and a 200 ms bill in the middle of a block is a
	// timeout the block never voted for. The dispatches read a zero Block (count 0: every
	// invocation returns at the guard) and a zero Params (count 0: same), so nothing touches a
	// buffer but the driver's compiler.
	{
		WGPUBufferDescriptor dummy = WGPU_BUFFER_DESCRIPTOR_INIT;
		dummy.usage = WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform;
		dummy.size = 4096; // every record shape this lowering writes fits in one page
		// Three buffers, one per binding: two read-only and a read-write cannot share one, and
		// neither can two read-write.
		WGPUBuffer buffers[3] = {wgpuDeviceCreateBuffer(dev->getDevice(), &dummy),
				wgpuDeviceCreateBuffer(dev->getDevice(), &dummy),
				wgpuDeviceCreateBuffer(dev->getDevice(), &dummy)};

		auto encoder = wgpuDeviceCreateCommandEncoder(dev->getDevice(), nullptr);
		for (auto &it : table->blocks) {
			WGPUBindGroupEntry entries[3] = {WGPU_BIND_GROUP_ENTRY_INIT, WGPU_BIND_GROUP_ENTRY_INIT,
					WGPU_BIND_GROUP_ENTRY_INIT};
			entries[0].binding = 0;
			entries[0].buffer = buffers[0];
			entries[1].binding = 1;
			entries[1].buffer = buffers[1];
			entries[2].binding = 2;
			entries[2].buffer = buffers[2];
			WGPUBindGroupDescriptor group = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
			group.layout = _blockLayout;
			group.entryCount = 3;
			group.entries = entries;
			auto bindGroup = wgpuDeviceCreateBindGroup(dev->getDevice(), &group);
			auto pass = wgpuCommandEncoderBeginComputePass(encoder, nullptr);
			wgpuComputePassEncoderSetPipeline(pass, it.second.pipeline);
			wgpuComputePassEncoderSetBindGroup(pass, 0, bindGroup, 0, nullptr);
			wgpuComputePassEncoderDispatchWorkgroups(pass, 1, 1, 1);
			wgpuComputePassEncoderEnd(pass);
			wgpuComputePassEncoderRelease(pass);
			wgpuBindGroupRelease(bindGroup);
		}
		for (auto &it : table->reducers) {
			WGPUBindGroupEntry entries[3] = {WGPU_BIND_GROUP_ENTRY_INIT, WGPU_BIND_GROUP_ENTRY_INIT,
					WGPU_BIND_GROUP_ENTRY_INIT};
			entries[0].binding = 0;
			entries[0].buffer = buffers[0];
			entries[1].binding = 1;
			entries[1].buffer = buffers[1];
			entries[2].binding = 2;
			entries[2].buffer = buffers[2];
			WGPUBindGroupDescriptor group = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
			group.layout = _reduceLayout;
			group.entryCount = 3;
			group.entries = entries;
			auto bindGroup = wgpuDeviceCreateBindGroup(dev->getDevice(), &group);
			auto pass = wgpuCommandEncoderBeginComputePass(encoder, nullptr);
			wgpuComputePassEncoderSetPipeline(pass, it.second.pipeline);
			wgpuComputePassEncoderSetBindGroup(pass, 0, bindGroup, 0, nullptr);
			wgpuComputePassEncoderDispatchWorkgroups(pass, 1, 1, 1);
			wgpuComputePassEncoderEnd(pass);
			wgpuComputePassEncoderRelease(pass);
			wgpuBindGroupRelease(bindGroup);
		}
		auto commands = wgpuCommandEncoderFinish(encoder, nullptr);
		wgpuCommandEncoderRelease(encoder);
		wgpuQueueSubmit(dev->getQueue(), 1, &commands);
		wgpuCommandBufferRelease(commands);
		wgpuBufferRelease(buffers[0]);
		wgpuBufferRelease(buffers[1]);
		wgpuBufferRelease(buffers[2]);
#if XL_WGPU_NATIVE_API
		// The native device can wait synchronously; a browser build's compilers already ran at
		// pipeline creation, and the warm submission only rides the queue.
		dev->waitIdle();
#endif
	}

#if XL_WGPU_NATIVE_API
	struct ScopeContext {
		Rc<WgpuGpuDevice> self;
		Rc<Table> table;
		bool buildOk = false;
	};
	auto scope = new ScopeContext{this, table, ok};

	WGPUPopErrorScopeCallbackInfo pop = WGPU_POP_ERROR_SCOPE_CALLBACK_INFO_INIT;
	pop.mode = WGPUCallbackMode_AllowProcessEvents;
	pop.callback = [](WGPUPopErrorScopeStatus status, WGPUErrorType type, WGPUStringView message,
			void *userdata1, void *) {
		auto ctx = reinterpret_cast<ScopeContext *>(userdata1);
		ctx->self->_loop->performOnThread([ctx, status, type, message] {
			if (!ctx->buildOk || (status == WGPUPopErrorScopeStatus_Success
					&& type != WGPUErrorType_NoError)) {
				ctx->table->failed = true;
				if (type != WGPUErrorType_NoError) {
					log::source().error("xenolith::flow::webgpu",
							"pipeline compilation was refused: ",
							StringView(message.data, message.length));
				}
			} else {
				ctx->table->compiled = true;
			}
			delete ctx;
		}, ctx->self.get(), true);
	};
	pop.userdata1 = scope;
	wgpuDevicePopErrorScope(dev->getDevice(), pop);
#else
	// The browser broker answers a module it could not build with a null handle, which the build
	// above already turned into a refusal.
	table->compiled = ok;
	table->failed = !ok;
#endif
}

auto WgpuGpuDevice::takeSet(uint64_t table, uint32_t slot) -> Rc<BufferSet> {
	for (auto &it : _sets) {
		if (it->table == table && it->slot == slot && !it->busy.load()) {
			it->busy = true;
			return it;
		}
	}
	auto set = Rc<BufferSet>::alloc();
	set->table = table;
	set->slot = slot;
	set->busy = true;
	_sets.emplace_back(set);
	return set;
}

// The staging of a set, made anew when it has to grow: the old buffer's last reference dies with
// the flights that borrowed it, so a growth never pulls the memory out from under a pending map.
static WGPUBuffer takeStaging(wg::Device &dev, flow::GpuBufferSlot &slot, WGPUBuffer &owned,
		uint64_t bytes, StringView label) {
	if (!slot.reserve(bytes)) {
		return owned;
	}
	if (owned) {
		wgpuBufferRelease(owned);
		owned = nullptr;
	}
	WGPUBufferDescriptor desc = WGPU_BUFFER_DESCRIPTOR_INIT;
	desc.label = WGPUStringView{label.data(), label.size()};
	desc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead;
	desc.size = slot.capacity;
	owned = wgpuDeviceCreateBuffer(dev.getDevice(), &desc);
	return owned;
}

bool WgpuGpuDevice::buildBlock(const Rc<DispatchContext> &ctx, const flow::GpuDispatchCall &call) {
	if (!_loop || !_loop->isRunning()) {
		return false;
	}
	auto dev = _loop->getDevice();
	auto table = findTable(call.table);
	if (!dev || !table) {
		return false;
	}
	auto pipeline = table->findBlock(call.pipeline);
	if (!pipeline) {
		return false;
	}

	auto &set = ctx->set;
	const uint64_t blockBytes = ctx->block.size();
	const uint64_t inBytes = ctx->ins.size();
	const uint64_t outBytes = uint64_t(call.count) * call.outStride;
	ctx->outBytes = outBytes;

	auto reserveBuffer = [&](flow::GpuBufferSlot &slot, Rc<wg::Buffer> &buffer, uint64_t bytes,
			core::BufferUsage extra) -> wg::Buffer * {
		if (slot.reserve(bytes)) {
			buffer = Rc<wg::Buffer>::create(*dev,
					core::BufferInfo(core::BufferUsage::StorageBuffer | extra,
							core::PassType::Compute, slot.capacity),
					BytesView());
			++_allocations;
		}
		return buffer.get();
	};

	auto block = reserveBuffer(set->blockSlot, set->block, blockBytes, core::BufferUsage::TransferDst);
	auto ins = reserveBuffer(set->inSlot, set->ins, inBytes, core::BufferUsage::TransferDst);
	auto outs = reserveBuffer(set->outSlot, set->outs, outBytes, core::BufferUsage::TransferSrc);
	if (!block || !ins || !outs) {
		return false;
	}

	ctx->staging = takeStaging(*dev, set->stageSlot, set->stageBuffer, outBytes,
			StringView("flowOutStaging"));
	if (!ctx->staging) {
		return false;
	}

	auto queue = dev->getQueue();
	if (blockBytes > 0) {
		wgpuQueueWriteBuffer(queue, block->getBuffer(), 0, ctx->block.data(), size_t(blockBytes));
	}
	if (inBytes > 0) {
		wgpuQueueWriteBuffer(queue, ins->getBuffer(), 0, ctx->ins.data(), size_t(inBytes));
	}

	WGPUBindGroupEntry entries[3] = {WGPU_BIND_GROUP_ENTRY_INIT, WGPU_BIND_GROUP_ENTRY_INIT,
			WGPU_BIND_GROUP_ENTRY_INIT};
	entries[0].binding = 0;
	entries[0].buffer = block->getBuffer();
	entries[1].binding = 1;
	entries[1].buffer = ins->getBuffer();
	entries[2].binding = 2;
	entries[2].buffer = outs->getBuffer();

	WGPUBindGroupDescriptor group = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
	group.label = WGPUStringView{"flowBlockGroup", WGPU_STRLEN};
	group.layout = _blockLayout;
	group.entryCount = 3;
	group.entries = entries;
	ctx->bindGroup = wgpuDeviceCreateBindGroup(dev->getDevice(), &group);
	if (!ctx->bindGroup) {
		return false;
	}

	auto encoder = wgpuDeviceCreateCommandEncoder(dev->getDevice(), nullptr);
	if (!encoder) {
		return false;
	}

	auto pass = wgpuCommandEncoderBeginComputePass(encoder, nullptr);
	wgpuComputePassEncoderSetPipeline(pass, pipeline->pipeline);
	wgpuComputePassEncoderSetBindGroup(pass, 0, ctx->bindGroup, 0, nullptr);
	// Every pipeline this device runs - a block body, a reducer, a conformance shader - declares
	// a 256-wide workgroup; the lowering writes it so.
	wgpuComputePassEncoderDispatchWorkgroups(pass, (call.count + 255) / 256, 1, 1);
	wgpuComputePassEncoderEnd(pass);
	wgpuComputePassEncoderRelease(pass);

	wgpuCommandEncoderCopyBufferToBuffer(encoder, outs->getBuffer(), 0, ctx->staging, 0, outBytes);

	auto commands = wgpuCommandEncoderFinish(encoder, nullptr);
	wgpuCommandEncoderRelease(encoder);
	wgpuQueueSubmit(queue, 1, &commands);
	wgpuCommandBufferRelease(commands);

	// The answer arrives when the loop is polled: the map callback runs there, copies the bytes
	// out and completes the request.
	struct MapContext {
		Rc<DispatchContext> ctx;
	};
	auto map = new MapContext{ctx};

	WGPUBufferMapCallbackInfo mapCallback = WGPU_BUFFER_MAP_CALLBACK_INFO_INIT;
	mapCallback.mode = WGPUCallbackMode_AllowProcessEvents;
	mapCallback.callback = [](WGPUMapAsyncStatus status, WGPUStringView, void *userdata1, void *) {
		auto map = reinterpret_cast<MapContext *>(userdata1);
		map->ctx->device->_loop->performOnThread([map, status] {
			auto &ctx = *map->ctx;
			if (status == WGPUMapAsyncStatus_Success) {
				auto mapped = reinterpret_cast<const uint8_t *>(
						wgpuBufferGetConstMappedRange(ctx.staging, 0, size_t(ctx.outBytes)));
				if (mapped) {
					Bytes data;
					data.resize(size_t(ctx.outBytes));
					sprt::memcpy(data.data(), mapped, size_t(ctx.outBytes));
					wgpuBufferUnmap(ctx.staging);
					ctx.device->finishContext(ctx, flow::GpuRequest::State::Done, data);
				} else {
					wgpuBufferUnmap(ctx.staging);
					ctx.device->finishContext(ctx, flow::GpuRequest::State::Failed, BytesView());
				}
			} else {
				ctx.device->finishContext(ctx, flow::GpuRequest::State::Failed, BytesView());
			}
			delete map;
		}, map->ctx->device.get(), true);
	};
	mapCallback.userdata1 = map;

	wgpuBufferMapAsync(ctx->staging, WGPUMapMode_Read, 0, outBytes, mapCallback);
	return true;
}

bool WgpuGpuDevice::buildReduce(const Rc<DispatchContext> &ctx, const flow::GpuReduceCall &call) {
	if (!_loop || !_loop->isRunning()) {
		return false;
	}
	auto dev = _loop->getDevice();
	auto table = findTable(call.table);
	if (!dev || !table) {
		return false;
	}

	// All of a block's folds in one frame: the values and the presence words of every input
	// concatenated, each run aligned to what a binding's offset must respect (the device's own
	// storage alignment, and the element size - the shader indexes its run from the binding), then
	// one compute pass per stride step - WebGPU orders between passes of one encoder and not within
	// one - with the pass parameters in one uniform buffer, one slot per pass, so a single write
	// and a single submission carry them all. Each pass binds its run's slice of the buffers and
	// its own slot of the parameters, so the shader's bases stay zero.
	const uint32_t storageAlign = sprt::max(uint32_t(4),
			dev->getLimits().minStorageBufferOffsetAlignment);
	struct Run {
		const flow::GpuReduceInput *input = nullptr;
		uint64_t valueBytes = 0; // byte offset of this input's values in the shared buffer
		uint32_t presentBytes = 0; // byte offset of its presence words
		const Pipeline *pipeline = nullptr;
	};
	mem_std::Vector<Run> runs;
	mem_std::Vector<uint8_t> values;
	mem_std::Vector<uint8_t> present;
	uint64_t valueCursor = 0;
	uint32_t presentCursor = 0;
	for (auto &input : call.inputs) {
		Run run;
		run.input = &input;
		auto name = flow::getGpuReduceName(input.kind, input.type);
		run.pipeline = table->findReducer(name);
		if (!run.pipeline) {
			return false; // the collector folds on the CPU instead
		}

		// the least common multiple of the device's alignment and this run's element size
		auto lcm = [](uint32_t a, uint32_t b) {
			uint32_t x = a, y = b;
			while (y != 0) {
				uint32_t t = x % y;
				x = y;
				y = t;
			}
			return (uint64_t(a) / x) * b;
		};
		uint32_t valueAlign = uint32_t(lcm(storageAlign, sprt::max(uint32_t(4), input.stride)));

		run.valueBytes = valueCursor;
		run.presentBytes = presentCursor;
		values.insert(values.end(), input.values.begin(), input.values.end());
		present.insert(present.end(), input.present.begin(), input.present.end());
		// The runs sit at their aligned offsets, and the bytes written have to sit there too:
		// what the alignment added between runs is padding, written as zeros.
		valueCursor = flow::value::alignUp(valueCursor + input.values.size(), uint64_t(valueAlign));
		values.resize(size_t(valueCursor), 0);
		presentCursor = uint32_t(flow::value::alignUp(uint64_t(presentCursor) + input.present.size(),
				uint64_t(storageAlign)));
		present.resize(presentCursor, 0);
		runs.emplace_back(run);
	}

	// The stride steps of every run, in order: a pass each.
	struct Step {
		uint32_t run = 0;
		uint32_t stride = 1; // elements
	};
	mem_std::Vector<Step> steps;
	for (uint32_t r = 0; r < runs.size(); ++r) {
		uint32_t count = runs[r].input->count;
		for (uint32_t s = 1; s < count; s *= 2) {
			steps.emplace_back(Step{r, s});
		}
	}

	auto &set = ctx->set;
	const uint64_t valueBytes = values.size();
	const uint64_t presentBytes = present.size();
	// The parameters travel one 16-byte struct per step, but a uniform binding's offset must
	// respect the device's own alignment for them - commonly 64 - so a step's slot is that wide.
	const uint32_t paramsAlign = sprt::max(uint32_t(16),
			dev->getLimits().minUniformBufferOffsetAlignment);
	const uint64_t paramsBytes = uint64_t(sprt::max(size_t(1), steps.size())) * paramsAlign;
	const uint64_t stagingBytes = valueBytes + presentBytes;
	ctx->outBytes = stagingBytes;

	auto reserveBuffer = [&](flow::GpuBufferSlot &slot, Rc<wg::Buffer> &buffer, uint64_t bytes,
			core::BufferUsage usage) -> wg::Buffer * {
		if (slot.reserve(bytes)) {
			buffer = Rc<wg::Buffer>::create(*dev,
					core::BufferInfo(usage, core::PassType::Compute, slot.capacity), BytesView());
			++_allocations;
		}
		return buffer.get();
	};

	auto reduceValues = reserveBuffer(set->reduceValueSlot, set->reduceValues, valueBytes,
			core::BufferUsage::StorageBuffer | core::BufferUsage::TransferSrc);
	auto reducePresent = reserveBuffer(set->reducePresentSlot, set->reducePresent, presentBytes,
			core::BufferUsage::StorageBuffer | core::BufferUsage::TransferSrc);
	auto reduceParams = reserveBuffer(set->reduceParamsSlot, set->reduceParams, paramsBytes,
			core::BufferUsage::UniformBuffer | core::BufferUsage::TransferDst);
	if (!reduceValues || !reducePresent || !reduceParams) {
		return false;
	}

	ctx->staging = takeStaging(*dev, set->reduceStageSlot, set->reduceStageBuffer, stagingBytes,
			StringView("flowReduceStaging"));
	if (!ctx->staging) {
		return false;
	}

	// the parameters of every step: this run's count and this step's stride, bases zero
	mem_std::Vector<uint8_t> params(size_t(paramsBytes), 0);
	for (uint32_t s = 0; s < steps.size(); ++s) {
		uint32_t words[4] = {runs[steps[s].run].input->count, steps[s].stride, 0, 0};
		sprt::memcpy(params.data() + size_t(s) * paramsAlign, words, sizeof(words));
	}

	auto queue = dev->getQueue();
	wgpuQueueWriteBuffer(queue, reduceValues->getBuffer(), 0, values.data(), size_t(valueBytes));
	if (presentBytes > 0) {
		wgpuQueueWriteBuffer(queue, reducePresent->getBuffer(), 0, present.data(),
				size_t(presentBytes));
	}
	wgpuQueueWriteBuffer(queue, reduceParams->getBuffer(), 0, params.data(), size_t(paramsBytes));

	// one bind group per step: the run's slice of the values and the presence words, and this
	// step's slot of the parameters
	ctx->reduceGroups.resize(steps.size(), nullptr);
	for (uint32_t s = 0; s < steps.size(); ++s) {
		auto &run = runs[steps[s].run];
		auto &input = *run.input;

		WGPUBindGroupEntry entries[3] = {WGPU_BIND_GROUP_ENTRY_INIT, WGPU_BIND_GROUP_ENTRY_INIT,
				WGPU_BIND_GROUP_ENTRY_INIT};
		entries[0].binding = 0;
		entries[0].buffer = reduceValues->getBuffer();
		entries[0].offset = run.valueBytes;
		entries[0].size = input.values.size();
		entries[1].binding = 1;
		entries[1].buffer = reducePresent->getBuffer();
		entries[1].offset = run.presentBytes;
		entries[1].size = input.present.size();
		entries[2].binding = 2;
		entries[2].buffer = reduceParams->getBuffer();
		entries[2].offset = uint64_t(s) * paramsAlign;
		entries[2].size = 16;

		WGPUBindGroupDescriptor group = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
		group.label = WGPUStringView{"flowReduceGroup", WGPU_STRLEN};
		group.layout = _reduceLayout;
		group.entryCount = 3;
		group.entries = entries;
		ctx->reduceGroups[s] = wgpuDeviceCreateBindGroup(dev->getDevice(), &group);
		if (!ctx->reduceGroups[s]) {
			return false;
		}
	}

	auto encoder = wgpuDeviceCreateCommandEncoder(dev->getDevice(), nullptr);
	if (!encoder) {
		return false;
	}

	for (uint32_t s = 0; s < steps.size(); ++s) {
		auto &run = runs[steps[s].run];
		auto &input = *run.input;
		auto pass = wgpuCommandEncoderBeginComputePass(encoder, nullptr);
		wgpuComputePassEncoderSetPipeline(pass, run.pipeline->pipeline);
		wgpuComputePassEncoderSetBindGroup(pass, 0, ctx->reduceGroups[s], 0, nullptr);
		wgpuComputePassEncoderDispatchWorkgroups(pass,
				(input.count + 2 * steps[s].stride - 1) / (2 * steps[s].stride), 1, 1);
		wgpuComputePassEncoderEnd(pass);
		wgpuComputePassEncoderRelease(pass);
	}

	wgpuCommandEncoderCopyBufferToBuffer(encoder, reduceValues->getBuffer(), 0, ctx->staging, 0,
			valueBytes);
	if (presentBytes > 0) {
		wgpuCommandEncoderCopyBufferToBuffer(encoder, reducePresent->getBuffer(), 0, ctx->staging,
				valueBytes, presentBytes);
	}

	auto commands = wgpuCommandEncoderFinish(encoder, nullptr);
	wgpuCommandEncoderRelease(encoder);
	wgpuQueueSubmit(queue, 1, &commands);
	wgpuCommandBufferRelease(commands);

	// What the answer needs per run, copied: the callback reads it after this function's callers
	// are gone, and a pointer into the reduce call's inputs would not survive that.
	struct AnswerRun {
		uint64_t valueBytes = 0;
		uint32_t presentBytes = 0;
		uint32_t valueSize = 0; // one value, in bytes
	};
	mem_std::Vector<AnswerRun> answers;
	for (auto &run : runs) {
		answers.emplace_back(AnswerRun{run.valueBytes, run.presentBytes, run.input->stride});
	}

	struct MapContext {
		Rc<DispatchContext> ctx;
		mem_std::Vector<AnswerRun> runs;
		uint64_t valueBytes = 0;
	};
	auto map = new MapContext{ctx, sp::move(answers), valueBytes};

	WGPUBufferMapCallbackInfo mapCallback = WGPU_BUFFER_MAP_CALLBACK_INFO_INIT;
	mapCallback.mode = WGPUCallbackMode_AllowProcessEvents;
	mapCallback.callback = [](WGPUMapAsyncStatus status, WGPUStringView, void *userdata1, void *) {
		auto map = reinterpret_cast<MapContext *>(userdata1);
		map->ctx->device->_loop->performOnThread([map, status] {
			auto &ctx = *map->ctx;
			if (status == WGPUMapAsyncStatus_Success) {
				auto mapped = reinterpret_cast<const uint8_t *>(
						wgpuBufferGetConstMappedRange(ctx.staging, 0, size_t(ctx.outBytes)));
				if (mapped) {
					// per input: its folded value (as many bytes as one of its values) then one
					// u32 presence flag - the answer layout the executor reads
					Bytes data;
					for (auto &run : map->runs) {
						auto value = mapped + run.valueBytes;
						data.insert(data.end(), value, value + run.valueSize);
						uint32_t present = 0;
						sprt::memcpy(&present, mapped + map->valueBytes + run.presentBytes, 4);
						uint8_t flag[4] = {uint8_t(present), uint8_t(present >> 8),
								uint8_t(present >> 16), uint8_t(present >> 24)};
						data.insert(data.end(), flag, flag + 4);
					}
					wgpuBufferUnmap(ctx.staging);
					ctx.device->finishContext(ctx, flow::GpuRequest::State::Done, data);
				} else {
					wgpuBufferUnmap(ctx.staging);
					ctx.device->finishContext(ctx, flow::GpuRequest::State::Failed, BytesView());
				}
			} else {
				ctx.device->finishContext(ctx, flow::GpuRequest::State::Failed, BytesView());
			}
			delete map;
		}, map->ctx->device.get(), true);
	};
	mapCallback.userdata1 = map;

	wgpuBufferMapAsync(ctx->staging, WGPUMapMode_Read, 0, stagingBytes, mapCallback);
	return true;
}

void WgpuGpuDevice::finishContext(DispatchContext &ctx, flow::GpuRequest::State state,
		BytesView data) {
	if (ctx.request) {
		ctx.request->complete(state, data);
	}
	if (ctx.bindGroup) {
		wgpuBindGroupRelease(ctx.bindGroup);
		ctx.bindGroup = nullptr;
	}
	for (auto &g : ctx.reduceGroups) {
		if (g) {
			wgpuBindGroupRelease(g);
		}
	}
	ctx.reduceGroups.clear();
	ctx.set->busy = false;
	--_inFlight;
	++_answered;
}

Rc<flow::GpuRequest> WgpuGpuDevice::dispatch(const flow::GpuDispatchCall &call) {
	auto table = findTable(call.table);
	if (!table || !table->compiled || table->failed || call.count == 0
			|| !table->findBlock(call.pipeline)) {
		if (__sprt_getenv("XL_FLOW_DEBUG")) {
			sprt::cout << "        flow dispatch REFUSED: table " << (table ? 1 : 0)
					<< " compiled " << (table ? int(table->compiled) : -1)
					<< " failed " << (table ? int(table->failed) : -1)
					<< " count " << call.count << " pipeline " << call.pipeline << " known "
					<< (table && table->findBlock(call.pipeline) ? 1 : 0) << "\n";
		}
		return nullptr;
	}

	auto request = Rc<flow::GpuRequest>::create();
	auto ctx = Rc<DispatchContext>::alloc();
	ctx->device = this;
	ctx->request = request;
	ctx->set = takeSet(call.table, call.slot);
	// The call's bytes are the job's, and an abandoned job may not wait for a pump: they travel
	// into the context here, on the machine's own thread.
	ctx->block.assign(call.block.data(), call.block.data() + call.block.size());
	ctx->ins.assign(call.ins.data(), call.ins.data() + call.ins.size());
	++_inFlight;

	_loop->performOnThread([this, ctx, count = call.count, outStride = call.outStride,
			pipeline = call.pipeline, tableId = call.table] {
		flow::GpuDispatchCall local;
		local.table = tableId;
		local.pipeline = pipeline;
		local.count = count;
		local.outStride = outStride;
		if (!buildBlock(ctx, local)) {
			finishContext(*ctx, flow::GpuRequest::State::Failed, BytesView());
		}
	}, this, true);

	return request;
}

Rc<flow::GpuRequest> WgpuGpuDevice::reduce(const flow::GpuReduceCall &call) {
	auto table = findTable(call.table);
	if (!table || !table->compiled || table->failed || call.inputs.empty()) {
		return nullptr;
	}
	for (auto &input : call.inputs) {
		if (!table->findReducer(flow::getGpuReduceName(input.kind, input.type))) {
			return nullptr; // one of the folds has no shader: they all go back to the CPU
		}
	}

	auto request = Rc<flow::GpuRequest>::create();
	auto ctx = Rc<DispatchContext>::alloc();
	ctx->device = this;
	ctx->request = request;
	ctx->set = takeSet(call.table, call.slot);
	++_inFlight;

	// The inputs' bytes are the job's, like a dispatch's: copied on the machine's thread, and the
	// build reads the copy.
	mem_std::Vector<flow::GpuReduceInput> inputs;
	for (auto &input : call.inputs) {
		flow::GpuReduceInput copy;
		copy.kind = input.kind;
		copy.type = input.type;
		copy.count = input.count;
		copy.stride = input.stride;
		copy.values = input.values;
		copy.present = input.present;
		inputs.emplace_back(sp::move(copy));
	}

	_loop->performOnThread([this, ctx, tableId = call.table,
			inputs = sp::move(inputs)]() mutable {
		flow::GpuReduceCall local;
		local.table = tableId;
		local.inputs = SpanView<flow::GpuReduceInput>(inputs.data(), inputs.size());
		if (!buildReduce(ctx, local)) {
			finishContext(*ctx, flow::GpuRequest::State::Failed, BytesView());
		}
	}, this, true);

	return request;
}

void WgpuGpuDevice::pumpOnce() {
	// Poll the device here as well as on the loop's own timer: a map callback is delivered by
	// wgpuDevicePoll, and a host that pumps from its own thread should not wait for the loop's
	// timer to notice the work is done.
	if (_loop) {
		if (auto dev = _loop->getDevice()) {
			wgpuDevicePoll(dev->getDevice(), false, nullptr);
		}
	}
	if (_options.pump) {
		_options.pump->poll();
	}
}

void WgpuGpuDevice::update() {
	pumpOnce();
}

bool WgpuGpuDevice::waitCompletion(TimeInterval interval) {
	auto started = nowMs();
	auto answered = _answered.load();
	const uint64_t limit = sprt::max(uint64_t(1), uint64_t(interval.toMillis()));
#if XL_WGPU_NATIVE_API
	// The native device delivers a map callback through a wait-poll: a plain poll notices the
	// work only on its own clock, a hundred milliseconds late, and a block's timeout does not
	// wait for that. This runs on the machine's thread - the one thread that pumps this looper,
	// so nothing here races the loop's own timer.
	if (_loop) {
		if (auto dev = _loop->getDevice()) {
			while (nowMs() - started < limit) {
				wgpuDevicePoll(dev->getDevice(), true, nullptr);
				if (_answered.load() != answered) {
					return true;
				}
			}
			return _answered.load() != answered;
		}
	}
#endif
	while (nowMs() - started < limit) {
		pumpOnce();
		if (_answered.load() != answered) {
			return true;
		}
		sprt::platform::sleep(100'000ull); // 100 us
	}
	return _answered.load() != answered;
}

void WgpuGpuDevice::releaseBuffers(uint64_t table) {
	mem_std::Vector<Rc<BufferSet>> keep;
	for (auto &it : _sets) {
		// dropped here; a busy set dies with its flight's context, which is still holding it
		if ((table == 0 || it->table == table) && !it->busy.load()) {
			continue;
		}
		keep.emplace_back(sp::move(it));
	}
	_sets = sp::move(keep);
}

uint32_t WgpuGpuDevice::getLiveBufferSets() const {
	uint32_t live = 0;
	for (auto &it : _sets) {
		if (!it->busy.load()) {
			++live;
		}
	}
	return live;
}

Rc<wg::Loop> makeWgpuLoop(NotNull<sprt::dispatch::Looper> looper, String *reason) {
	auto info = Rc<core::InstanceInfo>::alloc();
	info->api = core::InstanceApi::WebGPU;
	if (auto env = __sprt_getenv("XL_COMPUTE_VALIDATION")) {
		if (StringView(env) == "1") {
			info->flags |= core::InstanceFlags::Validation;
		}
	}

	auto instance = wg::platform::createInstance(sp::move(info));
	if (!instance || instance->getDeviceCount() == 0) {
		if (reason) {
			*reason = "no WebGPU adapter";
		}
		return nullptr;
	}

	auto loopInfo = Rc<core::LoopInfo>::alloc();
	if (auto env = __sprt_getenv("XL_COMPUTE_DEVICE")) {
		loopInfo->deviceIdx = uint32_t(StringView(env).readInteger(10).get(0));
	}

	auto loop = instance->makeLoop(looper, sp::move(loopInfo)).cast<wg::Loop>();
	if (loop) {
		loop->run();
	} else if (reason) {
		*reason = "the loop has no device";
	}
	return loop;
}

} // namespace stappler::xenolith::flowwebgpu
