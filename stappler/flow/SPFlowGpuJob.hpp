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

#ifndef STAPPLER_FLOW_SPFLOWGPUJOB_HPP_
#define STAPPLER_FLOW_SPFLOWGPUJOB_HPP_

#include "SPFlowGpuJob.h"
#include "SPFlowGpuLoad.hpp"
#include "SPFlowMachine.h"

// The machine's half of a block in flight (SPFlowGpuJob.h): it loads the block when it opens - the
// scene as it stands, and the values the body takes from outside it - it accepts the records the
// device returns, and the machine imports them into the branch frames at delivery
// (SPFlowMachine.hpp, importGpu).
namespace STAPPLER_VERSIONIZED stappler::flow {

template <typename Graph, typename Local, typename Trace>
class GpuBlockJobT final : public GpuBlockJob {
public:
	using Parent = MachineT<Graph, Local, Trace>;

	// The loader's view of the machine: the scene as the block sees it, and the records of the
	// nodes above the body. A value's producer is read in its activation, not in the root's.
	struct MachineSource : GpuLoadSource {
		const Parent &parent;
		uint32_t activation = RootActivation;

		MachineSource(const Parent &p, uint32_t act) : parent(p), activation(act) { }

		bool hasComponent(value::EntityId id, TypeId component) const override {
			auto scene = parent.getScene();
			if (!scene || !scene->isValid()) {
				return false;
			}
			return scene->getComponent(id, component) != NullAddr;
		}

		Status readField(value::EntityId id, TypeId component, StringView field,
				Var &out) const override {
			out = value::makeNil();
			auto scene = parent.getScene();
			if (!scene || !scene->isValid() || !scene->getRegistry()) {
				return Status::ErrorInvalidArguemnt;
			}
			auto ct = scene->getRegistry()->get(component);
			if (!ct) {
				return Status::ErrorNotFound;
			}
			auto record = scene->getComponent(id, *ct);
			if (record == NullAddr) {
				return Status::ErrorNotFound;
			}
			return ct->getField(*scene->getArena(), record, field, out);
		}

		Status readBlockValue(uint32_t srcNode, uint32_t srcPin, Var &out) const override {
			auto &local = parent.getLocal();
			auto &rt = parent.getGraph()->getNodeAt(srcNode);
			auto act = parent.producerActivation(srcNode, activation);
			auto record = local.getRecord(srcNode, act);
			if (!rt.localSchema || record == NullAddr) {
				return Status::ErrorNotFound;
			}
			return rt.localSchema->getField(*local.getArena(), record,
					rt.localSchema->getFields()[srcPin], out);
		}
	};

	// On the machine's thread, with the scene frozen: the block's bytes and one In record per
	// branch, with the prefix's verdict already in them (a missing row, a guard, a static refusal).
	Status prepare(Parent &parent, const BlockTicket &ticket, const GpuBlockShaders &shaders,
			SpanView<value::EntityId> entities) {
		_ticket = ticket;
		_shaders = &shaders;
		_table = parent.getGraph()->getGpuShaders() ? parent.getGraph()->getGpuShaders()->getIdentity() : 0;
		_branches = uint32_t(entities.size());
		auto &p = shaders.program;
		MachineSource source(parent, ticket.activation);
		if (loadGpuBlock(*parent.getGraph(), p, source, entities, _loaded) != Status::Ok) {
			return Status::ErrorInvalidArguemnt;
		}
		_block = _loaded.block;
		_ins = _loaded.ins;
		_inStride = p.inLayout.size;
		_outStride = p.outLayout.size;
		_graph = parent.getGraph();
		_batches = 1;
		_remaining.store(1);
		return Status::Ok;
	}

	Status acceptOuts(BytesView outs) override {
		if (outs.size() < size_t(_outStride) * _branches) {
			return Status::ErrorInvalidArguemnt;
		}
		auto &p = _shaders->program;
		_results.resize(_branches);
		for (uint32_t i = 0; i < _branches; ++i) {
			if (!decodeGpuBranch(*_graph, p, _loaded, outs, i, _results[i])) {
				return Status::ErrorInvalidArguemnt;
			}
		}
		_hasResults = true;
		buildReduceInputs();
		return Status::Ok;
	}

	const GpuBranchResult &getResult(uint32_t branch) const { return _results[branch]; }
	const GpuLoaded &getLoaded() const { return _loaded; }

	// The fold of a collector, when the device did it; otherwise the collector folds on the CPU.
	bool getReduced(uint32_t reducer, Var &out) const {
		if (reducer < _reduced.size() && _reducedSet[reducer]) {
			out = _reduced[reducer];
			return true;
		}
		return false;
	}

protected:
	// One run of values per reducer, in branch order: present is what parFold calls present - a
	// branch that did not fail and produced the cell - and the value is the cell under the edge's
	// cast.
	void buildReduceInputs() {
		_reduceInputs.clear();
		_reduced.clear();
		_reducedSet.clear();
		auto &p = _shaders->program;
		_reduced.resize(_shaders->reducers.size());
		_reducedSet.resize(_shaders->reducers.size(), 0);
		// Under `nothing` no branch is present: the collectors give the neutral element, and the
		// CPU's own fold over an empty set gives exactly that.
		if (_graph->getBlockAt(p.block).onFailure == ParallelFailure::Nothing) {
			return;
		}
		for (uint32_t r = 0; r < uint32_t(_shaders->reducers.size()); ++r) {
			auto &binding = _shaders->reducers[r];
			auto &reducer = binding.reducer;
			GpuReduceInput in;
			in.reducer = r;
			in.kind = reducer.kind;
			in.type = reducer.type;
			in.count = _branches;
			in.stride = value::alignUp(getGpuTypeSize(reducer.type), getGpuTypeAlign(reducer.type));
			in.values.resize(size_t(in.stride) * _branches, 0);
			in.present.resize(size_t(4) * _branches, 0);
			auto edgeType = reducer.type;
			for (uint32_t i = 0; i < _branches; ++i) {
				auto &result = _results[i];
				bool present = !result.failed;
				Var value;
				if (reducer.source == GpuSource::Index) {
					value = value::makeInt32(int32_t(i));
				} else if (reducer.cell < uint32_t(result.cells.size())) {
					auto &cell = p.cells[reducer.cell];
					present = present
							&& ((result.path.nodes[cell.node].produced >> cell.pin) & 1) != 0;
					value = result.cells[reducer.cell];
				} else {
					present = false;
				}
				if (!present) {
					continue;
				}
				Var cast = value;
				if (value.type != edgeType) {
					value::castVar(value, edgeType, value::CastPolicy::Lossless, cast);
				}
				// A `count` folds ones: its element is a Bool, and a Bool is written as 0 or 1.
				writeGpuValue(in.values.data() + size_t(in.stride) * i, reducer.type, cast);
				uint32_t one = 1;
				sprt::memcpy(in.present.data() + size_t(4) * i, &one, 4);
			}
			_reduceInputs.emplace_back(sprt::move(in));
		}
	}

private:
	const Graph *_graph = nullptr;
	GpuLoaded _loaded;
	mem_std::Vector<GpuBranchResult> _results;
};

} // namespace stappler::flow

#endif /* STAPPLER_FLOW_SPFLOWGPUJOB_HPP_ */
