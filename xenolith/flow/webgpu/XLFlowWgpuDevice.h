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

#ifndef XENOLITH_FLOW_WEBGPU_XLFLOWWGPUDEVICE_H_
#define XENOLITH_FLOW_WEBGPU_XLFLOWWGPUDEVICE_H_

#include "XLWgpu.h"
#include "XLWgpuObject.h"
#include "XLWgpuPipeline.h"
#include "XLWgpuLoop.h"

#include "SPFlowExecutorGpu.h"

/* The WebGPU side of the flow GPU executor: what `flow::GpuDevice` asks for, done over the engine's
webgpu backend. A headless loop, one compute pipeline per shader the table carries, three storage
bindings for a block body and two storage plus a uniform one for a reducer - the layout the WGSL
lowering writes - and none of a window or a presentation.

The shaders arrive as the WGSL text the flow lowering writes, packed into words exactly the way
`webgpu::Shader` expects (anything without the SPIR-V magic word is WGSL), keyed by the hash of the
GLSL text as the shader table keys everything; a device that runs SPIR-V-capable wgpu-native could
take those words instead, and nothing here would notice.

A block's buffers belong to the block and grow by powers of two (`flow::GpuBufferSlot`), so a frame
series of one size allocates nothing after the first. What Vulkan did through staging and barriers,
WebGPU does through `wgpuQueueWriteBuffer` (no staging for the way in) and its own submission
ordering: dispatches that depend on each other go in separate compute passes of one encoder, because
WebGPU orders between passes and not within one. The way out is a persistent MapRead staging buffer
per slot - `captureBuffer` would allocate one per call - mapped after the copy answers, on whatever
thread pumps the loop.

`dispatch` and `reduce` are called on the machine's thread and return at once; the frame is built
and submitted on the loop's thread, and the answer is written into the request there. A request the
executor abandoned is dropped, whatever the loop does with the frame afterwards. */
namespace STAPPLER_VERSIONIZED stappler::xenolith::flowwebgpu {

namespace wg = stappler::xenolith::webgpu;

class SP_PUBLIC WgpuGpuDevice final : public flow::GpuDevice {
public:
	struct Options {
		// A looper to poll while waiting, for a host with no loop thread of its own. Null means the
		// loop runs elsewhere and answers on its own.
		sprt::dispatch::Looper *pump = nullptr;
		uint64_t compileTimeoutMs = 20'000;
	};

	explicit WgpuGpuDevice(Rc<wg::Loop> &&);
	WgpuGpuDevice(Rc<wg::Loop> &&, const Options &);
	virtual ~WgpuGpuDevice();

	StringView getName() const override { return StringView("webgpu"); }
	bool isAvailable() const override;
	bool isLost() const override;

	bool isPrepared(uint64_t table) const override;
	void prepare(uint64_t table, SpanView<flow::GpuPipelineSpec>) override;

	Rc<flow::GpuRequest> dispatch(const flow::GpuDispatchCall &) override;
	Rc<flow::GpuRequest> reduce(const flow::GpuReduceCall &) override;

	void update() override;
	bool waitCompletion(TimeInterval) override;

	void releaseBuffers(uint64_t table = 0) override;
	uint32_t getBufferAllocations() const override { return _allocations; }
	uint32_t getLiveBufferSets() const override;

	wg::Loop *getLoop() const { return _loop; }
	StringView getDeviceName() const { return _deviceName; }

protected:
	// One compiled program of a table: the module wrapper that owns it, the pipeline over the
	// fixed layout of its kind, and the words it was built from (kept: the executor frees its own
	// copy when the prepare call returns).
	struct Pipeline {
		Rc<wg::Shader> shader;
		WGPUComputePipeline pipeline = nullptr;
		mem_std::Vector<uint32_t> words;
	};

	// One table's pipelines, keyed by the block text hash the shader table keys everything by,
	// and by the reducer's own name.
	struct Table : public Ref {
		uint64_t id = 0;
		bool compiled = false;
		bool failed = false;
		mem_std::Vector<Pair<uint64_t, Pipeline>> blocks;
		mem_std::Vector<Pair<mem_std::String, Pipeline>> reducers;

		const Pipeline *findBlock(uint64_t hash) const {
			for (auto &it : blocks) {
				if (it.first == hash) {
					return &it.second;
				}
			}
			return nullptr;
		}
		const Pipeline *findReducer(StringView name) const {
			for (auto &it : reducers) {
				if (StringView(it.first) == name) {
					return &it.second;
				}
			}
			return nullptr;
		}
	};

	// The buffers of one block, kept between frames: the three a dispatch binds, the staging the
	// answer is read through, and the pair a reduce folds over with the uniform of its parameters.
	// The two staging buffers are raw WGPU handles - MapRead is no core usage - owned here and
	// released with the set.
	struct BufferSet : public Ref {
		uint64_t table = 0;
		uint32_t slot = 0;
		sprt::atomic<bool> busy = false;
		flow::GpuBufferSlot blockSlot;
		flow::GpuBufferSlot inSlot;
		flow::GpuBufferSlot outSlot;
		flow::GpuBufferSlot stageSlot;
		flow::GpuBufferSlot reduceValueSlot;
		flow::GpuBufferSlot reducePresentSlot;
		flow::GpuBufferSlot reduceParamsSlot;
		flow::GpuBufferSlot reduceStageSlot;
		Rc<wg::Buffer> block;
		Rc<wg::Buffer> ins;
		Rc<wg::Buffer> outs;
		Rc<wg::Buffer> reduceValues;
		Rc<wg::Buffer> reducePresent;
		Rc<wg::Buffer> reduceParams;
		WGPUBuffer stageBuffer = nullptr;
		WGPUBuffer reduceStageBuffer = nullptr;
	};

	// What one flight keeps alive until its map callback has answered: the bytes it was given on
	// the machine's thread (an abandoned job may not wait for a pump), the request to complete and
	// the set to hand back.
	struct DispatchContext : public Ref {
		Rc<WgpuGpuDevice> device;
		Rc<flow::GpuRequest> request;
		Rc<BufferSet> set;
		mem_std::Vector<uint8_t> block;
		mem_std::Vector<uint8_t> ins;
		mem_std::Vector<WGPUBindGroup> reduceGroups;
		WGPUBuffer staging = nullptr;
		WGPUBindGroup bindGroup = nullptr;
		uint64_t outBytes = 0;
	};

	Table *findTable(uint64_t) const;
	Rc<BufferSet> takeSet(uint64_t table, uint32_t slot);
	void compileTable(Rc<Table> &&);
	void ensureLayouts();
	bool buildBlock(const Rc<DispatchContext> &, const flow::GpuDispatchCall &);
	bool buildReduce(const Rc<DispatchContext> &, const flow::GpuReduceCall &);
	void finishContext(DispatchContext &, flow::GpuRequest::State, BytesView);
	void pumpOnce();

	Rc<wg::Loop> _loop;
	Options _options;
	String _deviceName;

	// the fixed layouts of the two shader kinds, shared by every table
	WGPUBindGroupLayout _blockLayout = nullptr;
	WGPUBindGroupLayout _reduceLayout = nullptr;
	WGPUPipelineLayout _blockPipelineLayout = nullptr;
	WGPUPipelineLayout _reducePipelineLayout = nullptr;

	uint32_t _allocations = 0;
	mem_std::Vector<Rc<Table>> _tables;
	mem_std::Vector<Rc<BufferSet>> _sets;
	sprt::atomic<uint32_t> _inFlight = 0;
	sprt::atomic<uint32_t> _answered = 0;
	bool _layoutsReady = false;
};

// A loop for a device: the instance acquisition of tests/wgpu (a WebGPU instance over the
// configured adapter, a headless loop on the given looper), for a host that has none of its own.
// Null with `reason` when there is no adapter - the caller's skip path.
SP_PUBLIC Rc<wg::Loop> makeWgpuLoop(NotNull<sprt::dispatch::Looper>, String *reason);

} // namespace stappler::xenolith::flowwebgpu

#endif /* XENOLITH_FLOW_WEBGPU_XLFLOWWGPUDEVICE_H_ */
