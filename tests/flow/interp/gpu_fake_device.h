/**
Copyright (c) 2026 Stappler Team <admin@stappler.org>

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

// A device made of the CPU evaluator (gpu_eval.h), so the GPU executor can be proved without one.
// What a device does to a run is not only arithmetic: it answers late, or never, or it is lost
// halfway, and each of those has to reach the machine as the same break every time, in a test that
// links no graphics API. Here they are modes, not weather.
//
// What this device does NOT prove is what a GLSL or WGSL body computes on hardware; that is a
// device harness's to hold.

#ifndef TESTS_FLOW_INTERP_GPU_FAKE_DEVICE_H_
#define TESTS_FLOW_INTERP_GPU_FAKE_DEVICE_H_

#include "SPFlowExecutorGpu.h"

#include "gpu_eval.h"

namespace STAPPLER_VERSIONIZED stappler::test::gpufake {

enum class Mode {
	Immediate, // answers inside update()
	Hold, // answers when release() is called
	HoldReduce, // the dispatch answers, the fold waits for release()
	Silent, // never answers
	LoseNext, // the next request is lost, and the device with it
	Refuse, // dispatch() returns null: the block falls back
	Stopped, // the device is not available: a loop that was stopped
};

template <typename Graph>
class CpuGpuDevice final : public flow::GpuDevice {
public:
	CpuGpuDevice(const Graph &graph) : _graph(&graph) { }

	StringView getName() const override { return StringView("cpu"); }
	bool isAvailable() const override { return _mode != Mode::Stopped && !_lost; }
	bool isLost() const override { return _lost; }

	bool isPrepared(uint64_t table) const override {
		return _prepared != 0 && _prepared == table && _prepareDelay == 0;
	}

	void prepare(uint64_t table, SpanView<flow::GpuPipelineSpec> specs) override {
		_prepared = table;
		_pipelines.clear();
		for (auto &it : specs) {
			_pipelines.emplace_back(it.hash);
		}
		++_prepareCount;
	}

	Rc<flow::GpuRequest> dispatch(const flow::GpuDispatchCall &call) override {
		if (_mode == Mode::Refuse || !isAvailable()) {
			return nullptr;
		}
		bool known = false;
		for (auto h : _pipelines) {
			known = known || h == call.pipeline;
		}
		if (!known) {
			return nullptr; // a block whose shader was never prepared does not run here
		}
		// The buffers a device would keep per block, with the same growth policy.
		auto &set = slotOf(call.table, call.slot);
		set.block.reserve(call.block.size());
		set.ins.reserve(call.ins.size());
		set.outs.reserve(size_t(call.outStride) * call.count);
		set.live = true;

		auto request = Rc<flow::GpuRequest>::alloc();
		Pending pending;
		pending.request = request;
		pending.kind = Pending::Kind::Dispatch;
		pending.outs = evaluate(call);
		++_dispatches;
		_pending.emplace_back(sprt::move(pending));
		return request;
	}

	Rc<flow::GpuRequest> reduce(const flow::GpuReduceCall &call) override {
		if (!isAvailable()) {
			return nullptr;
		}
		auto request = Rc<flow::GpuRequest>::alloc();
		Pending pending;
		pending.request = request;
		pending.kind = Pending::Kind::Reduce;
		pending.outs = fold(call);
		++_reduces;
		_pending.emplace_back(sprt::move(pending));
		return request;
	}

	void update() override {
		for (auto it = _pending.begin(); it != _pending.end();) {
			if (!answer(*it)) {
				++it;
				continue;
			}
			it = _pending.erase(it);
		}
	}

	bool waitCompletion(TimeInterval) override {
		auto before = _pending.size();
		update();
		return _pending.size() != before;
	}

	void releaseBuffers(uint64_t table = 0) override {
		for (auto &it : _sets) {
			if (table == 0 || it.table == table) {
				it.block.release();
				it.ins.release();
				it.outs.release();
				it.live = false;
			}
		}
	}

	uint32_t getBufferAllocations() const override {
		uint32_t total = 0;
		for (auto &it : _sets) {
			total += it.block.allocations + it.ins.allocations + it.outs.allocations;
		}
		return total;
	}

	uint32_t getLiveBufferSets() const override {
		uint32_t total = 0;
		for (auto &it : _sets) {
			total += it.live ? 1 : 0;
		}
		return total;
	}

	// ---- what a test drives ---------------------------------------------------------------------

	void setMode(Mode mode) { _mode = mode; }
	Mode getMode() const { return _mode; }
	void release() { _released = true; }
	void setPrepareDelay(uint32_t frames) { _prepareDelay = frames; }
	void tickPrepare() {
		if (_prepareDelay > 0) {
			--_prepareDelay;
		}
	}

	uint32_t getDispatchCount() const { return _dispatches; }
	uint32_t getReduceCount() const { return _reduces; }
	uint32_t getPrepareCount() const { return _prepareCount; }
	uint32_t getPendingCount() const { return uint32_t(_pending.size()); }

private:
	struct Pending {
		enum class Kind {
			Dispatch,
			Reduce,
		};

		Rc<flow::GpuRequest> request;
		Kind kind = Kind::Dispatch;
		mem_std::Vector<uint8_t> outs;
	};

	struct BufferSet {
		uint64_t table = 0;
		uint32_t slot = 0;
		bool live = false;
		flow::GpuBufferSlot block;
		flow::GpuBufferSlot ins;
		flow::GpuBufferSlot outs;
	};

	BufferSet &slotOf(uint64_t table, uint32_t slot) {
		for (auto &it : _sets) {
			if (it.table == table && it.slot == slot) {
				return it;
			}
		}
		_sets.emplace_back(BufferSet{table, slot});
		return _sets.back();
	}

	// The dispatch itself: the lowered program over the C++ bodies, the same bytes in and out.
	mem_std::Vector<uint8_t> evaluate(const flow::GpuDispatchCall &call) {
		const flow::GpuBlockShaders *shaders = nullptr;
		if (auto table = _graph->getGpuShaders()) {
			for (auto &it : table->getBlocks()) {
				if (it.textHash == call.pipeline) {
					shaders = &it;
				}
			}
		}
		if (!shaders) {
			return mem_std::Vector<uint8_t>();
		}
		flow::GpuLoaded loaded;
		loaded.branches = call.count;
		loaded.block = mem_std::Vector<uint8_t>(call.block.data(), call.block.data() + call.block.size());
		loaded.ins = mem_std::Vector<uint8_t>(call.ins.data(), call.ins.data() + call.ins.size());
		gpueval::Evaluator<Graph> evaluator{*_graph, shaders->program, loaded};
		return evaluator.runAll();
	}

	// The tree fold, in the order the reducer shader folds in: value then presence, per input.
	mem_std::Vector<uint8_t> fold(const flow::GpuReduceCall &call) {
		mem_std::Vector<uint8_t> out;
		for (auto &in : call.inputs) {
			mem_std::Vector<flow::value::Var> values;
			mem_std::Vector<uint8_t> present;
			for (uint32_t i = 0; i < in.count; ++i) {
				values.emplace_back(flow::readGpuValue(in.values.data() + size_t(in.stride) * i, in.type));
				uint32_t flag = 0;
				sprt::memcpy(&flag, in.present.data() + size_t(4) * i, 4);
				present.emplace_back(flag != 0 ? 1 : 0);
			}
			flow::value::Var folded;
			bool any = false;
			for (auto it : present) {
				any = any || it != 0;
			}
			flow::treeFoldReference(in.kind, in.type, values, present, folded);
			auto at = out.size();
			out.resize(at + in.stride + 4, 0);
			flow::writeGpuValue(out.data() + at, in.type, folded);
			uint32_t flag = any ? 1 : 0;
			sprt::memcpy(out.data() + at + in.stride, &flag, 4);
		}
		return out;
	}

	bool answer(Pending &pending) {
		switch (_mode) {
		case Mode::Silent:
		case Mode::Stopped: return false;
		case Mode::Hold:
			if (!_released) {
				return false;
			}
			break;
		case Mode::HoldReduce:
			if (pending.kind == Pending::Kind::Reduce && !_released) {
				return false;
			}
			break;
		case Mode::LoseNext:
			_lost = true;
			pending.request->complete(flow::GpuRequest::State::Lost, BytesView());
			return true;
		default: break;
		}
		pending.request->complete(flow::GpuRequest::State::Done,
				BytesView(pending.outs.data(), pending.outs.size()));
		return true;
	}

	const Graph *_graph = nullptr;
	Mode _mode = Mode::Immediate;
	bool _released = false;
	bool _lost = false;
	uint64_t _prepared = 0;
	uint32_t _prepareDelay = 0;
	uint32_t _prepareCount = 0;
	uint32_t _dispatches = 0;
	uint32_t _reduces = 0;
	mem_std::Vector<uint64_t> _pipelines;
	mem_std::Vector<Pending> _pending;
	mem_std::Vector<BufferSet> _sets;
};

} // namespace stappler::test::gpufake

#endif /* TESTS_FLOW_INTERP_GPU_FAKE_DEVICE_H_ */
