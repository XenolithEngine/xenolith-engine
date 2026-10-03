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

#ifndef STAPPLER_FLOW_SPFLOWGPU_HPP_
#define STAPPLER_FLOW_SPFLOWGPU_HPP_

// The lowering of a parallel body (SPFlowGpu.h), over any graph policy: the built graph and a
// unit's static tables answer the same questions (SPFlowStatic.h), and a loaded unit re-lowers its
// blocks to check that the shader it was built with still describes them.

#include "SPFlowGpu.h"
#include "SPFlowLocal.h"

namespace STAPPLER_VERSIONIZED stappler::flow {

namespace gpu {

struct SimNode {
	uint32_t flags = 0;
	uint32_t inputs = 0;
	uint32_t produced = 0;
};

struct SimState {
	mem_std::Vector<uint32_t> stack; // body indices, top last
	mem_std::Vector<SimNode> nodes;
	uint32_t steps = 0;

	mem_std::String key() const {
		mem_std::String out;
		out.reserve(8 + stack.size() * 4 + nodes.size() * 12);
		auto put = [&](uint32_t v) {
			out.push_back(char(v & 0xFF));
			out.push_back(char((v >> 8) & 0xFF));
			out.push_back(char((v >> 16) & 0xFF));
			out.push_back(char((v >> 24) & 0xFF));
		};
		put(steps);
		put(uint32_t(stack.size()));
		for (auto s : stack) {
			put(s);
		}
		for (auto &n : nodes) {
			put(n.flags);
			put(n.inputs);
			put(n.produced);
		}
		return out;
	}
};

static constexpr uint32_t FlagToken = uint32_t(NodeFlags::Token);
static constexpr uint32_t FlagRan = uint32_t(NodeFlags::Ran);
static constexpr uint32_t FlagQueued = uint32_t(NodeFlags::Queued);

// The front's rules, restated over body indices: isReadyIn, propagate and the pushes of the machine
// (SPFlowMachine.hpp). A body in a GPU block has no nested scope and no loop, and every value it
// reads from outside was produced before its fan-out ran, so readiness is the node's own record.
template <typename Graph>
struct Simulator {
	const Graph &g;
	const GpuProgram &p;
	const mem_std::Vector<uint32_t> &bodyOf;

	bool isReady(const SimState &s, uint32_t i) const {
		auto &rt = g.getNodeAt(p.bodyNodes[i]);
		if (!rt.op) {
			return false;
		}
		auto &n = s.nodes[i];
		if ((n.flags & (FlagRan | FlagQueued)) != 0) {
			return false;
		}
		if (rt.op->hasExecIn() && (n.flags & FlagToken) == 0) {
			return false;
		}
		return (n.inputs & rt.sameScopeInputs) == rt.sameScopeInputs;
	}

	void push(SimState &s, uint32_t i) const {
		s.stack.emplace_back(i);
		s.nodes[i].flags |= FlagQueued;
	}

	void open(SimState &s, uint32_t first) const {
		s.stack.clear();
		s.nodes.assign(p.bodyNodes.size(), SimNode());
		s.steps = 0;
		if (first != InvalidIndex) {
			s.nodes[first].flags |= FlagToken;
		}
		mem_std::Vector<uint32_t> ready;
		for (uint32_t i = 0; i < uint32_t(p.bodyNodes.size()); ++i) {
			if (isReady(s, i)) {
				ready.emplace_back(i);
			}
		}
		for (uint32_t k = uint32_t(ready.size()); k > 0; --k) {
			push(s, ready[k - 1]);
		}
	}

	// The unit at the top of the front: popped, run, propagated. `fired` is the exec outputs it
	// fired.
	void step(SimState &s, uint32_t i, uint32_t fired) const {
		auto graphIndex = p.bodyNodes[i];
		auto &rt = g.getNodeAt(graphIndex);
		auto &n = s.nodes[i];
		n.flags |= FlagRan;
		auto outs = uint32_t(rt.op->getDataOut().size());
		n.produced |= outs >= 32 ? uint32_t(0xFFFF'FFFF) : ((uint32_t(1) << outs) - 1);

		mem_std::Vector<uint32_t> dataReady;
		mem_std::Vector<uint32_t> execReady;
		for (auto e : g.getDataOutEdges(graphIndex)) {
			auto &edge = g.getDataEdges()[e];
			if ((n.produced & (uint32_t(1) << edge.srcPin)) == 0) {
				continue;
			}
			auto &dst = g.getNodeAt(edge.dstNode);
			if (dst.op && edge.dstPin == dst.op->getBranchPin()) {
				continue;
			}
			auto d = bodyOf[edge.dstNode];
			if (d == InvalidIndex) {
				continue;
			}
			s.nodes[d].inputs |= uint32_t(1) << edge.dstPin;
			if (isReady(s, d)) {
				dataReady.emplace_back(d);
			}
		}
		for (auto e : g.getExecOutEdges(graphIndex)) {
			auto &edge = g.getExecEdges()[e];
			if ((fired & (uint32_t(1) << edge.srcPin)) == 0) {
				continue;
			}
			auto d = bodyOf[edge.dstNode];
			if (d == InvalidIndex) {
				continue; // leaving the body for the barrier: nothing to do
			}
			if ((s.nodes[d].flags & FlagRan) != 0) {
				s.nodes[d].flags &= ~(FlagRan | FlagQueued);
			}
			s.nodes[d].flags |= FlagToken;
			if (isReady(s, d)) {
				execReady.emplace_back(d);
			}
		}
		for (uint32_t k = uint32_t(execReady.size()); k > 0; --k) {
			push(s, execReady[k - 1]);
		}
		for (uint32_t k = uint32_t(dataReady.size()); k > 0; --k) {
			push(s, dataReady[k - 1]);
		}
	}

	uint32_t firedBy(uint32_t i) const {
		auto &node = p.nodes[i];
		auto &rt = g.getNodeAt(node.node);
		auto execs = uint32_t(rt.op->getExecOut().size());
		switch (node.form) {
		case GpuForm::FireAll:
		case GpuForm::SceneGet:
		case GpuForm::SceneSet:
		case GpuForm::SceneHas: return execs >= 32 ? uint32_t(0xFFFF'FFFF) : ((uint32_t(1) << execs) - 1);
		default: return 0;
		}
	}
};

inline bool isNameRole(PinRole role) {
	switch (role) {
	case PinRole::ComponentName:
	case PinRole::ComponentNameOptional:
	case PinRole::FieldName:
	case PinRole::EnumFamily:
	case PinRole::ExtensionName:
	case PinRole::EntityName: return true;
	default: return false;
	}
}

// Whether a widened value of `payload` narrows to `target` without a refusal (SPFlowValueVar.cc).
inline bool narrowsExactly(VarType payload, VarType target) {
	if (payload == target || payload == VarType::Bool) {
		return true;
	}
	if (target == VarType::Float32) {
		return payload == VarType::Int32 || payload == VarType::UInt32;
	}
	return false;
}

} // namespace gpu

template <typename Graph>
void buildGpuLayouts(const Graph &g, GpuProgram &p);

template <typename Graph>
Status lowerGpuBlock(const Graph &g, uint32_t blockIndex, GpuProgram &p, DiagReport *report,
		const GpuLowerLimits &limits) {
	p = GpuProgram();
	if (blockIndex >= g.getBlockCount()) {
		return Status::ErrorInvalidArguemnt;
	}
	auto &block = g.getBlockAt(blockIndex);
	p.block = blockIndex;
	p.maxSteps = block.maxSteps;

	mem_std::Vector<uint32_t> bodyOf(g.getNodeCount(), InvalidIndex);
	for (auto n : g.getScopeNodes(block.scope)) {
		bodyOf[n] = uint32_t(p.bodyNodes.size());
		p.bodyNodes.emplace_back(n);
	}

	// Cells: every output of every body node.

	for (uint32_t i = 0; i < uint32_t(p.bodyNodes.size()); ++i) {
		auto &rt = g.getNodeAt(p.bodyNodes[i]);
		GpuNode node;
		node.node = p.bodyNodes[i];
		node.form = rt.op ? classifyGpuOp(*rt.op) : GpuForm::None;
		node.cellBegin = uint32_t(p.cells.size());
		if (rt.op) {
			auto outs = rt.op->getDataOut();
			for (uint32_t pin = 0; pin < uint32_t(outs.size()); ++pin) {
				auto desc = nodeDataOut(*rt.op, pin, rt.family);
				GpuCell cell;
				cell.node = i;
				cell.pin = pin;
				cell.type = desc.type;
				cell.subtypeId = desc.subtypeId;
				cell.rep = isGpuValueType(desc.type) ? GpuRep::Direct : GpuRep::Tagged;
				p.cells.emplace_back(cell);
			}
		}
		node.cellCount = uint32_t(p.cells.size()) - node.cellBegin;
		p.nodes.emplace_back(node);
	}

	auto cellOf = [&](uint32_t graphIndex, uint32_t pin) {
		auto i = bodyOf[graphIndex];
		return i == InvalidIndex ? InvalidIndex : p.nodes[i].cellBegin + pin;
	};

	auto blockValueOf = [&](uint32_t srcNode, uint32_t srcPin) {
		for (uint32_t k = 0; k < uint32_t(p.blockValues.size()); ++k) {
			if (p.blockValues[k].srcNode == srcNode && p.blockValues[k].srcPin == srcPin) {
				return k;
			}
		}
		auto &src = g.getNodeAt(srcNode);
		GpuBlockValue v;
		v.srcNode = srcNode;
		v.srcPin = srcPin;
		v.type = nodeDataOut(*src.op, srcPin, src.family).type;
		p.blockValues.emplace_back(v);
		return uint32_t(p.blockValues.size() - 1);
	};

	auto componentOf = [&](TypeId id) {
		for (uint32_t k = 0; k < uint32_t(p.components.size()); ++k) {
			if (p.components[k] == id) {
				return k;
			}
		}
		p.components.emplace_back(id);
		return uint32_t(p.components.size() - 1);
	};

	auto fieldIn = [&](mem_std::Vector<GpuColumn> &list, uint32_t component, StringView field,
						   VarType type) {
		for (uint32_t k = 0; k < uint32_t(list.size()); ++k) {
			if (list[k].component == component && list[k].field == field && list[k].type == type) {
				return k;
			}
		}
		list.emplace_back(GpuColumn{component, field, type});
		return uint32_t(list.size() - 1);
	};

	// Inputs, scene forms.

	for (uint32_t i = 0; i < uint32_t(p.nodes.size()); ++i) {
		auto &node = p.nodes[i];
		auto &rt = g.getNodeAt(node.node);
		if (!rt.op) {
			continue;
		}
		auto ins = rt.op->getDataIn();
		node.inputBegin = uint32_t(p.inputs.size());
		for (uint32_t pin = 0; pin < uint32_t(ins.size()); ++pin) {
			GpuInput in;
			in.type = ins[pin].type;
			if (gpu::isNameRole(ins[pin].role)) {
				in.source = GpuSource::Name;
				p.inputs.emplace_back(in);
				continue;
			}
			bool fed = false;
			for (auto e : g.getDataInEdges(node.node)) {
				auto &edge = g.getDataEdges()[e];
				if (edge.dstPin != pin) {
					continue;
				}
				fed = true;
				in.cast = edge.cast;
				if (edge.srcNode == block.fanOut) {
					in.source = edge.srcPin == block.entityPin ? GpuSource::OwnEntity : GpuSource::Index;
				} else if (bodyOf[edge.srcNode] != InvalidIndex) {
					in.source = GpuSource::Cell;
					in.index = cellOf(edge.srcNode, edge.srcPin);
				} else {
					in.source = GpuSource::Block;
					in.index = blockValueOf(edge.srcNode, edge.srcPin);
				}
				break;
			}
			if (!fed) {
				if (g.getConstant(node.node, pin)) {
					in.source = GpuSource::Literal;
					in.index = uint32_t(p.literals.size());
					p.literals.emplace_back(GpuLiteral{node.node, pin, ins[pin].type});
				} else {
					in.source = GpuSource::Zero;
				}
			}
			p.inputs.emplace_back(in);
		}
		node.inputCount = uint32_t(p.inputs.size()) - node.inputBegin;

		switch (node.form) {
		case GpuForm::Body: node.fallible = (rt.op->getFlags() & OpFlags::Infallible) == OpFlags::None; break;
		case GpuForm::SceneGet:
		case GpuForm::SceneSet:
		case GpuForm::SceneHas: {
			auto contract = g.getSceneContract(node.node);
			auto targetPin = rt.op->getTargetPin();
			if (contract.empty() || targetPin == NullPin) {
				node.staticFailure = Status::ErrorInvalidArguemnt;
				break;
			}
			auto &binding = contract[0];
			auto &target = p.inputs[node.inputBegin + targetPin];
			if (target.source == GpuSource::OwnEntity) {
				node.own = true;
				node.component = componentOf(binding.componentId);
				if (node.form != GpuForm::SceneHas) {
					node.fallible = true;
					for (uint32_t w = 0; w < block.writeCount; ++w) {
						auto &write = g.getBlockWrites(blockIndex)[w];
						if (write.componentId == binding.componentId && write.field == binding.field) {
							node.write = w;
						}
					}
				}
				if (node.form == GpuForm::SceneGet) {
					auto type = p.cells[node.cellBegin].type;
					if (isGpuValueType(type)) {
						node.in = fieldIn(p.fields, node.component, binding.field, type);
					} else {
						node.column = fieldIn(p.columns, node.component, binding.field, type);
					}
				} else if (node.form == GpuForm::SceneSet && node.write == InvalidIndex) {
					node.staticFailure = Status::ErrorInvalidArguemnt;
				}
			} else if (target.source == GpuSource::Block || target.source == GpuSource::Literal) {
				GpuForeign f;
				f.entity = target.source;
				f.entityIndex = target.index;
				f.componentId = binding.componentId;
				f.component = binding.component;
				if (node.form == GpuForm::SceneGet) {
					f.field = binding.field;
					f.type = p.cells[node.cellBegin].type;
					node.fallible = true;
				} else if (node.form == GpuForm::SceneHas) {
					f.type = VarType::Bool;
				} else {
					node.staticFailure = Status::ErrorInvalidArguemnt; // refused by the build
				}
				node.foreign = uint32_t(p.foreigns.size());
				p.foreigns.emplace_back(f);
			} else {
				node.staticFailure = Status::ErrorInvalidArguemnt; // a null target
			}
			break;
		}
		default: break;
		}
	}

	// Narrowings: a CPU source gets a guard, a widened one a cast, a literal is narrowed now.

	auto guardOf = [&](GpuGuard::Source source, uint32_t index, VarType target) {
		uint32_t bits = 0;
		for (uint32_t k = 0; k < uint32_t(p.guards.size()); ++k) {
			auto &gd = p.guards[k];
			if (gd.source == source && gd.index == index && gd.target == target) {
				return k;
			}
			if ((gd.source == GpuGuard::Source::Column) == (source == GpuGuard::Source::Column)) {
				++bits;
			}
		}
		p.guards.emplace_back(GpuGuard{source, index, target, bits});
		return uint32_t(p.guards.size() - 1);
	};

	for (auto &node : p.nodes) {
		if (node.form != GpuForm::Narrow || node.inputCount == 0) {
			continue;
		}
		auto target = p.cells[node.cellBegin].type;
		GpuInput *value = nullptr;
		for (uint32_t k = 0; k < node.inputCount; ++k) {
			if (p.inputs[node.inputBegin + k].source != GpuSource::Name) {
				value = &p.inputs[node.inputBegin + k];
			}
		}
		if (!value) {
			continue;
		}
		switch (value->source) {
		case GpuSource::Cell: {
			auto &cell = p.cells[value->index];
			auto &src = p.nodes[cell.node];
			if (src.form == GpuForm::SceneGet && src.own && src.column != InvalidIndex) {
				node.guard = guardOf(GpuGuard::Source::Column, src.column, target);
			} else if (src.form == GpuForm::SceneGet && src.foreign != InvalidIndex
					&& !isGpuValueType(cell.type)) {
				node.guard = guardOf(GpuGuard::Source::Foreign, src.foreign, target);
			} else if (src.form == GpuForm::Widen) {
				auto &widenIn = p.inputs[src.inputBegin];
				node.fallible = !gpu::narrowsExactly(widenIn.type, target);
			} else if (isGpuValueType(cell.type)) {
				node.fallible = !gpu::narrowsExactly(cell.type, target); // widened by the edge
			} else {
				node.fallible = true;
			}
			break;
		}
		case GpuSource::Block:
			if (!isGpuValueType(p.blockValues[value->index].type)) {
				node.guard = guardOf(GpuGuard::Source::Block, value->index, target);
			} else {
				node.fallible = !gpu::narrowsExactly(p.blockValues[value->index].type, target);
			}
			break;
		case GpuSource::Index: node.fallible = !gpu::narrowsExactly(VarType::Int32, target); break;
		case GpuSource::Literal: {
			auto &lit = p.literals[value->index];
			Var decoded, narrowed;
			if (auto c = g.getConstant(lit.node, lit.pin)) {
				if (value::decodeVar(*c, lit.type, decoded) != Status::Ok
						|| value::castVar(decoded, target, value::CastPolicy::Lossy, narrowed)
								!= Status::Ok) {
					node.staticFailure = Status::ErrorInvalidArguemnt;
				}
			}
			break;
		}
		default: break;
		}
	}

	// The simulation.

	uint32_t first = InvalidIndex;
	{
		auto &fanOut = g.getNodeAt(block.fanOut);
		for (auto e : g.getExecOutEdges(block.fanOut)) {
			auto &edge = g.getExecEdges()[e];
			if (fanOut.op->opensScope(edge.srcPin) && bodyOf[edge.dstNode] != InvalidIndex) {
				first = bodyOf[edge.dstNode];
				break;
			}
		}
	}

	gpu::Simulator<Graph> sim{g, p, bodyOf};
	mem_std::Map<mem_std::String, uint32_t> memo;
	bool oversize = false;

	// A continuation from `state`; the index it has in p.conts.
	mem_std::Function<uint32_t(gpu::SimState &, uint32_t)> walk;
	walk = [&](gpu::SimState &s, uint32_t decisions) -> uint32_t {
		auto key = s.key();
		auto it = memo.find(key);
		if (it != memo.end()) {
			return it->second;
		}
		auto index = uint32_t(p.conts.size());
		memo.emplace(sprt::move(key), index);
		p.conts.emplace_back(GpuCont());
		p.maxDecisions = sprt::max(p.maxDecisions, decisions);
		if (p.conts.size() > limits.maxConts) {
			oversize = true;
			return index;
		}

		mem_std::Vector<uint32_t> runs;
		GpuCont cont;
		while (true) {
			if (s.stack.empty()) {
				cont.end = GpuCont::End::Close;
				break;
			}
			auto top = s.stack.back();
			if (s.steps >= p.maxSteps) {
				cont.end = GpuCont::End::Budget;
				cont.node = top;
				break;
			}
			++s.steps;
			s.stack.pop_back();
			s.nodes[top].flags &= ~gpu::FlagQueued;
			runs.emplace_back(top);
			if (p.runs.size() + runs.size() > limits.maxRuns) {
				oversize = true;
				cont.end = GpuCont::End::Close;
				break;
			}

			auto &node = p.nodes[top];
			if (node.staticFailure != Status::Ok) {
				cont.end = GpuCont::End::Close; // never reached past this unit
				break;
			}
			if (node.form == GpuForm::Branch) {
				cont.end = GpuCont::End::Branch;
				cont.node = top;
				break;
			}
			sim.step(s, top, sim.firedBy(top));
		}

		cont.runBegin = uint32_t(p.runs.size());
		cont.runCount = uint32_t(runs.size());
		for (auto r : runs) {
			p.runs.emplace_back(r);
		}

		if (cont.end == GpuCont::End::Branch && !oversize) {
			auto whenTrue = s;
			sim.step(whenTrue, cont.node, uint32_t(1) << 0);
			auto whenFalse = sprt::move(s);
			sim.step(whenFalse, cont.node, uint32_t(1) << 1);
			cont.next[0] = walk(whenTrue, decisions + 1);
			cont.next[1] = walk(whenFalse, decisions + 1);
		}
		p.conts[index] = cont;
		return index;
	};

	{
		gpu::SimState start;
		sim.open(start, first);
		walk(start, 0);
	}

	if (oversize) {
		if (report) {
			report->reportNode(DiagSeverity::Error, DiagCode::ParallelGpuShape,
					g.getNodeAt(block.fanOut).id,
					DiagText(DiagDetail::ParallelGpuSize).number(int64_t(limits.maxRuns)));
		}
		return Status::ErrorInvalidArguemnt;
	}

	// The prefix: what the loader decides before dispatch.

	bool guardFault = false;
	{
		auto &c0 = p.conts[0];
		mem_std::Vector<uint8_t> written(block.writeCount, 0);
		uint32_t steps = 0;
		uint32_t k = 0;
		for (; k < c0.runCount; ++k) {
			auto i = p.runs[c0.runBegin + k];
			auto &node = p.nodes[i];
			auto stepsHere = steps + 1;
			if (node.staticFailure != Status::Ok) {
				p.prefix.emplace_back(GpuPrefixCheck{GpuPrefixCheck::Kind::Static, InvalidIndex, i,
					stepsHere, node.staticFailure});
				++k;
				steps = stepsHere;
				break;
			}
			bool gpuFallible = false;
			switch (node.form) {
			case GpuForm::SceneGet:
			case GpuForm::SceneSet:
				if (node.own) {
					p.prefix.emplace_back(GpuPrefixCheck{GpuPrefixCheck::Kind::OwnRow, node.component, i,
						stepsHere, Status::ErrorNotFound});
					if (node.form == GpuForm::SceneSet && node.write != InvalidIndex) {
						written[node.write] = 1;
					}
					if (node.form == GpuForm::SceneGet && node.column != InvalidIndex
							&& node.write != InvalidIndex && written[node.write]) {
						// A CPU value read after the branch wrote it: the loader cannot know it.
						guardFault = true;
						if (report) {
							report->reportNode(DiagSeverity::Error, DiagCode::ParallelGpuGuard,
									g.getNodeAt(node.node).id,
									DiagText(DiagDetail::ParallelGpuGuardWritten));
						}
					}
				} else if (node.foreign != InvalidIndex) {
					p.prefix.emplace_back(GpuPrefixCheck{GpuPrefixCheck::Kind::ForeignRow, node.foreign, i,
						stepsHere, Status::ErrorNotFound});
				}
				break;
			case GpuForm::Narrow:
				if (node.guard != InvalidIndex) {
					p.prefix.emplace_back(GpuPrefixCheck{GpuPrefixCheck::Kind::Guard, node.guard, i,
						stepsHere, Status::ErrorInvalidArguemnt});
				} else {
					gpuFallible = node.fallible;
				}
				break;
			case GpuForm::Body: gpuFallible = node.fallible; break;
			default: break;
			}
			if (gpuFallible) {
				break;
			}
			steps = stepsHere;
		}
		p.prefixRuns = k;
		if (k == c0.runCount && c0.end == GpuCont::End::Budget) {
			p.prefix.emplace_back(GpuPrefixCheck{GpuPrefixCheck::Kind::Budget, InvalidIndex, c0.node,
				steps, Status::ErrorInvalidArguemnt});
		}
	}

	// A narrowing of a CPU value must stand on the prefix: that is what lets its guard fail the
	// branch before dispatch, exactly where serial fails it.
	{
		auto &c0 = p.conts[0];
		mem_std::Vector<uint8_t> reported(p.nodes.size(), 0);
		for (uint32_t c = 0; c < uint32_t(p.conts.size()); ++c) {
			auto &cont = p.conts[c];
			for (uint32_t k = 0; k < cont.runCount; ++k) {
				if (c == 0 && k < p.prefixRuns) {
					continue;
				}
				auto i = p.runs[cont.runBegin + k];
				if (p.nodes[i].form == GpuForm::Narrow && p.nodes[i].guard != InvalidIndex && !reported[i]) {
					reported[i] = 1;
					guardFault = true;
					if (report) {
						report->reportNode(DiagSeverity::Error, DiagCode::ParallelGpuGuard,
								g.getNodeAt(p.nodes[i].node).id,
								DiagText(DiagDetail::ParallelGpuGuard));
					}
				}
			}
		}
		(void)c0;
	}

	if (guardFault) {
		return Status::ErrorInvalidArguemnt;
	}

	// One mask word each: a body that needs more is a body too large for this lowering.
	uint32_t columnGuards = 0, blockGuards = 0;
	for (auto &gd : p.guards) {
		(gd.source == GpuGuard::Source::Column ? columnGuards : blockGuards) += 1;
	}
	if (p.components.size() > 32 || p.foreigns.size() > 32 || columnGuards > 32 || blockGuards > 32) {
		if (report) {
			report->reportNode(DiagSeverity::Error, DiagCode::ParallelGpuShape,
					g.getNodeAt(block.fanOut).id,
					DiagText(DiagDetail::ParallelGpuSize).number(int64_t(32)));
		}
		return Status::ErrorInvalidArguemnt;
	}

	buildGpuLayouts(g, p);
	return Status::Ok;
}

template <typename Graph>
bool replayGpuPath(const Graph &g, const GpuProgram &p, SpanView<uint32_t> decisions, bool failed,
		bool budget, uint32_t failNode, Status failStatus, uint32_t steps, GpuPath &out) {
	mem_std::Vector<uint32_t> bodyOf(g.getNodeCount(), InvalidIndex);
	for (uint32_t i = 0; i < uint32_t(p.bodyNodes.size()); ++i) {
		bodyOf[p.bodyNodes[i]] = i;
	}
	gpu::Simulator<Graph> sim{g, p, bodyOf};
	uint32_t first = InvalidIndex;
	{
		auto &block = g.getBlockAt(p.block);
		auto &fanOut = g.getNodeAt(block.fanOut);
		for (auto e : g.getExecOutEdges(block.fanOut)) {
			auto &edge = g.getExecEdges()[e];
			if (fanOut.op->opensScope(edge.srcPin) && bodyOf[edge.dstNode] != InvalidIndex) {
				first = bodyOf[edge.dstNode];
				break;
			}
		}
	}

	out = GpuPath();
	out.failed = failed;
	out.budget = budget;
	out.failNode = failed ? failNode : InvalidIndex;
	out.failStatus = failed ? failStatus : Status::Ok;

	gpu::SimState s;
	sim.open(s, first);

	// The pre-dispatch refusals stop inside the prefix without a decision: a budget one before its
	// pop, any other one at its own unit.
	uint32_t cont = 0;
	uint32_t decision = 0;
	bool stopped = false;
	while (!stopped) {
		if (cont >= p.conts.size()) {
			return false;
		}
		auto &c = p.conts[cont];
		for (uint32_t k = 0; k < c.runCount; ++k) {
			auto i = p.runs[c.runBegin + k];
			if (s.stack.empty() || s.stack.back() != i) {
				return false;
			}
			if (failed && budget && s.steps == steps && i == failNode) {
				s.stack.pop_back();
				s.nodes[i].flags &= ~gpu::FlagQueued;
				stopped = true;
				break;
			}
			++s.steps;
			s.stack.pop_back();
			s.nodes[i].flags &= ~gpu::FlagQueued;
			if (failed && !budget && s.steps == steps && i == failNode) {
				stopped = true; // a refused step leaves no unit in the log, only the failure
				break;
			}
			out.log.emplace_back(i);
			out.fired.emplace_back(sim.firedBy(i));
			if (p.nodes[i].form == GpuForm::Branch && k + 1 == c.runCount) {
				break; // its pin is the decision, filled in at End::Branch
			}
			sim.step(s, i, sim.firedBy(i));
		}
		if (stopped) {
			break;
		}
		switch (c.end) {
		case GpuCont::End::Close: stopped = true; break;
		case GpuCont::End::Budget:
			if (!failed || !budget) {
				return false;
			}
			s.stack.pop_back();
			s.nodes[c.node].flags &= ~gpu::FlagQueued;
			stopped = true;
			break;
		case GpuCont::End::Jump: cont = c.next[0]; break;
		case GpuCont::End::Branch: {
			if (decision >= decisions.size()) {
				return false;
			}
			auto taken = decisions[decision++] != 0;
			out.decisions.emplace_back(taken ? 1 : 0);
			auto fired = taken ? (uint32_t(1) << 0) : (uint32_t(1) << 1);
			if (!out.fired.empty() && !out.log.empty() && out.log.back() == c.node) {
				out.fired.back() = fired;
			}
			sim.step(s, c.node, fired);
			cont = c.next[taken ? 0 : 1];
			break;
		}
		}
	}

	out.steps = s.steps;
	out.nodes.resize(p.bodyNodes.size());
	// A failed branch drops the rest of its front; a closed one stalls whatever holds a token and
	// is not ready (closeActivation).
	if (failed) {
		for (auto i : s.stack) {
			s.nodes[i].flags &= ~gpu::FlagQueued;
		}
	}
	for (uint32_t i = 0; i < uint32_t(p.bodyNodes.size()); ++i) {
		auto &n = s.nodes[i];
		out.nodes[i].flags = n.flags;
		out.nodes[i].inputs = n.inputs;
		out.nodes[i].produced = n.produced;
		if (!failed && (n.flags & gpu::FlagRan) == 0 && (n.flags & gpu::FlagToken) != 0
				&& !sim.isReady(s, i)) {
			out.nodes[i].stalled = true;
		}
	}
	return true;
}

template <typename Graph>
void buildGpuLayouts(const Graph &g, GpuProgram &p) {
	auto &block = g.getBlockAt(p.block);
	auto slot = [](GpuLayout &l, GpuSlot::Kind kind, uint32_t index, mem_std::String name, VarType type,
						uint32_t count = 1) {
		GpuSlot s;
		s.kind = kind;
		s.index = index;
		s.name = sprt::move(name);
		s.type = type;
		s.count = count;
		l.slots.emplace_back(sprt::move(s));
	};

	// Block: one per dispatch.
	auto &b = p.blockLayout;
	slot(b, GpuSlot::Kind::Count, 0, "count", VarType::Nil);
	slot(b, GpuSlot::Kind::BlockGuardMask, 0, "guardMask", VarType::Nil);
	slot(b, GpuSlot::Kind::ForeignRowMask, 0, "foreignRowMask", VarType::Nil);
	for (uint32_t k = 0; k < uint32_t(p.blockValues.size()); ++k) {
		if (isGpuValueType(p.blockValues[k].type)) {
			slot(b, GpuSlot::Kind::BlockValue, k, mem_std::toString("bv", k), p.blockValues[k].type);
		}
	}
	for (uint32_t k = 0; k < uint32_t(p.guards.size()); ++k) {
		if (p.guards[k].source != GpuGuard::Source::Column) {
			slot(b, GpuSlot::Kind::BlockNarrowed, k, mem_std::toString("bg", k), p.guards[k].target);
		}
	}
	for (uint32_t k = 0; k < uint32_t(p.foreigns.size()); ++k) {
		if (isGpuValueType(p.foreigns[k].type)) {
			slot(b, GpuSlot::Kind::ForeignValue, k, mem_std::toString("fv", k), p.foreigns[k].type);
		}
	}

	// In: one per branch.
	auto &in = p.inLayout;
	slot(in, GpuSlot::Kind::Index, 0, "index", VarType::Int32);
	slot(in, GpuSlot::Kind::PreFailed, 0, "preFailed", VarType::Nil);
	slot(in, GpuSlot::Kind::PreNode, 0, "preNode", VarType::Nil);
	slot(in, GpuSlot::Kind::PreCode, 0, "preCode", VarType::Int32);
	slot(in, GpuSlot::Kind::PreSteps, 0, "preSteps", VarType::Nil);
	slot(in, GpuSlot::Kind::PreBudget, 0, "preBudget", VarType::Nil);
	slot(in, GpuSlot::Kind::RowMask, 0, "rowMask", VarType::Nil);
	slot(in, GpuSlot::Kind::GuardMask, 0, "guardMask", VarType::Nil);
	for (uint32_t k = 0; k < uint32_t(p.fields.size()); ++k) {
		slot(in, GpuSlot::Kind::In, k, mem_std::toString("in", k), p.fields[k].type);
	}
	for (uint32_t k = 0; k < uint32_t(p.guards.size()); ++k) {
		if (p.guards[k].source == GpuGuard::Source::Column) {
			slot(in, GpuSlot::Kind::Column, k, mem_std::toString("cg", k), p.guards[k].target);
		}
	}

	// Out: one per branch, and every cell of the body (the stage's decision: level C compares all).
	auto &out = p.outLayout;
	slot(out, GpuSlot::Kind::Status, 0, "status", VarType::Nil);
	slot(out, GpuSlot::Kind::FailNode, 0, "failNode", VarType::Nil);
	slot(out, GpuSlot::Kind::FailCode, 0, "failCode", VarType::Int32);
	slot(out, GpuSlot::Kind::Steps, 0, "steps", VarType::Nil);
	slot(out, GpuSlot::Kind::Budget, 0, "budget", VarType::Nil);
	slot(out, GpuSlot::Kind::DecisionCount, 0, "decisionCount", VarType::Nil);
	slot(out, GpuSlot::Kind::Decisions, 0, "decisions", VarType::Nil, sprt::max(uint32_t(1), (p.maxDecisions + 31) / 32));
	slot(out, GpuSlot::Kind::CopyMask, 0, "copyMask", VarType::Nil, sprt::max(uint32_t(1), (block.writeCount + 31) / 32));
	for (uint32_t w = 0; w < block.writeCount; ++w) {
		auto &write = g.getBlockWrites(p.block)[w];
		if (isGpuValueType(write.type)) {
			slot(out, GpuSlot::Kind::Copy, w, mem_std::toString("wv", w), write.type);
		} else {
			slot(out, GpuSlot::Kind::CopyTag, w, mem_std::toString("wt", w), VarType::Nil);
			slot(out, GpuSlot::Kind::Copy, w, mem_std::toString("wv", w), VarType::Nil);
		}
	}
	for (uint32_t c = 0; c < uint32_t(p.cells.size()); ++c) {
		if (p.cells[c].rep == GpuRep::Direct) {
			slot(out, GpuSlot::Kind::Cell, c, mem_std::toString("c", c), p.cells[c].type);
		} else {
			slot(out, GpuSlot::Kind::CellTag, c, mem_std::toString("ct", c), VarType::Nil);
			slot(out, GpuSlot::Kind::Cell, c, mem_std::toString("cb", c), VarType::Nil);
		}
	}

	layoutStd430(b);
	layoutStd430(in);
	layoutStd430(out);
}

namespace gpu {

inline mem_std::String uintText(uint64_t v) { return mem_std::toString(v, "u"); }

inline mem_std::String intText(int64_t v) {
	if (v == int64_t(-2'147'483'647) - 1) {
		return mem_std::String("(-2147483647 - 1)");
	}
	return mem_std::toString(v);
}

inline mem_std::String floatText(float f) {
	uint32_t bits = 0;
	sprt::memcpy(&bits, &f, sizeof(bits));
	char buf[16];
	static constexpr const char *hex = "0123456789abcdef";
	for (int k = 0; k < 8; ++k) {
		buf[k] = hex[(bits >> (28 - 4 * k)) & 0xF];
	}
	return mem_std::toString("uintBitsToFloat(0x", StringView(buf, 8), "u)");
}

// A Var of a GPU type as a GLSL constant of its type.
inline mem_std::String literalText(const Var &v, VarType type) {
	switch (type) {
	case VarType::Bool: return mem_std::String(v.i != 0 ? "true" : "false");
	case VarType::Int32: return intText(int32_t(v.i));
	case VarType::UInt32: return uintText(uint32_t(v.i));
	case VarType::Float32: return floatText(v.v[0]);
	case VarType::Vec2: return mem_std::toString("vec2(", floatText(v.v[0]), ", ", floatText(v.v[1]), ")");
	case VarType::Vec3:
		return mem_std::toString("vec3(", floatText(v.v[0]), ", ", floatText(v.v[1]), ", ",
				floatText(v.v[2]), ")");
	case VarType::Vec4:
	case VarType::Color:
		return mem_std::toString("vec4(", floatText(v.v[0]), ", ", floatText(v.v[1]), ", ",
				floatText(v.v[2]), ", ", floatText(v.v[3]), ")");
	default: return mem_std::String("0u");
	}
}

inline mem_std::String zeroText(VarType type) {
	switch (type) {
	case VarType::Bool: return mem_std::String("false");
	case VarType::Int32: return mem_std::String("0");
	case VarType::UInt32: return mem_std::String("0u");
	case VarType::Float32: return mem_std::String("0.0");
	case VarType::Vec2: return mem_std::String("vec2(0.0)");
	case VarType::Vec3: return mem_std::String("vec3(0.0)");
	case VarType::Vec4:
	case VarType::Color: return mem_std::String("vec4(0.0)");
	default: return mem_std::String("0u");
	}
}

// A GPU value of `from` read by a GPU pin of `to` through an implicit edge.
inline mem_std::String castText(const mem_std::String &expr, VarType from, VarType to) {
	if (from == to || (from == VarType::Vec4 && to == VarType::Color)
			|| (from == VarType::Color && to == VarType::Vec4)) {
		return expr;
	}
	if (from == VarType::Bool) {
		switch (to) {
		case VarType::Int32: return mem_std::toString("int(", expr, ")");
		case VarType::UInt32: return mem_std::toString("uint(", expr, ")");
		case VarType::Float32: return mem_std::toString("(", expr, " ? 1.0 : 0.0)");
		default: break;
		}
	}
	if (from == VarType::Vec2 && to == VarType::Vec3) {
		return mem_std::toString("vec3(", expr, ", 0.0)");
	}
	if (from == VarType::Vec2 && to == VarType::Vec4) {
		return mem_std::toString("vec4(", expr, ", 0.0, 0.0)");
	}
	if (from == VarType::Vec3 && to == VarType::Vec4) {
		return mem_std::toString("vec4(", expr, ", 0.0)");
	}
	return expr;
}

// The tag a GPU value of `type` travels under once widened out of the set.
inline uint32_t widenTag(VarType type) {
	switch (type) {
	case VarType::UInt32: return GpuTag::UInt32;
	case VarType::Float32: return GpuTag::Float32;
	default: return GpuTag::Int32;
	}
}

inline mem_std::String bitsText(const mem_std::String &expr, VarType type) {
	switch (type) {
	case VarType::Float32: return mem_std::toString("floatBitsToUint(", expr, ")");
	case VarType::UInt32: return expr;
	default: return mem_std::toString("uint(", expr, ")");
	}
}

template <typename Graph>
struct GlslWriter {
	const Graph &g;
	const GpuProgram &p;
	mem_std::String out;

	void line(StringView text) {
		out.append(text.data(), text.size());
		out.push_back('\n');
	}

	const GpuInput &input(const GpuNode &n, uint32_t pin) const { return p.inputs[n.inputBegin + pin]; }

	const GpuSlot *slotOf(const GpuLayout &l, GpuSlot::Kind kind, uint32_t index) const {
		for (auto &s : l.slots) {
			if (s.kind == kind && s.index == index) {
				return &s;
			}
		}
		return nullptr;
	}

	// A buffer member read as the GLSL type of `type` (a Bool is stored as a uint).
	mem_std::String member(const mem_std::String &prefix, const GpuSlot *slot) const {
		auto text = mem_std::toString(prefix, slot->name);
		if (slot->type == VarType::Bool) {
			return mem_std::toString("(", text, " != 0u)");
		}
		return text;
	}

	Var literal(uint32_t index) const {
		auto &lit = p.literals[index];
		Var v;
		if (auto c = g.getConstant(lit.node, lit.pin)) {
			value::decodeVar(*c, lit.type, v);
		}
		return v;
	}

	// An input as a value of the pin's GPU type.
	mem_std::String direct(const GpuInput &in, VarType pinType) const {
		switch (in.source) {
		case GpuSource::Cell: {
			auto &cell = p.cells[in.index];
			return castText(mem_std::toString("c", in.index), cell.type, pinType);
		}
		case GpuSource::Literal: {
			auto v = literal(in.index);
			return literalText(v, pinType);
		}
		case GpuSource::Block: {
			auto slot = slotOf(p.blockLayout, GpuSlot::Kind::BlockValue, in.index);
			if (!slot) {
				return zeroText(pinType);
			}
			return castText(member("blk.", slot), p.blockValues[in.index].type, pinType);
		}
		case GpuSource::Index: return castText("ins[g_i].index", VarType::Int32, pinType);
		default: return zeroText(pinType);
		}
	}

	// An input as a tag and 32 bits: a value outside the GPU set.
	void tagged(const GpuInput &in, mem_std::String &tag, mem_std::String &bits) const {
		switch (in.source) {
		case GpuSource::Cell: {
			auto &cell = p.cells[in.index];
			if (cell.rep == GpuRep::Tagged) {
				tag = mem_std::toString("ct", in.index);
				bits = mem_std::toString("cb", in.index);
			} else {
				tag = uintText(widenTag(cell.type));
				bits = bitsText(mem_std::toString("c", in.index), cell.type);
			}
			return;
		}
		case GpuSource::Literal:
			tag = uintText(GpuTag::Literal);
			bits = uintText(in.index);
			return;
		case GpuSource::Block: {
			auto &value = p.blockValues[in.index];
			if (isGpuValueType(value.type)) {
				auto slot = slotOf(p.blockLayout, GpuSlot::Kind::BlockValue, in.index);
				tag = uintText(widenTag(value.type));
				bits = bitsText(member("blk.", slot), value.type);
			} else {
				tag = uintText(GpuTag::Block);
				bits = uintText(in.index);
			}
			return;
		}
		case GpuSource::Index:
			tag = uintText(GpuTag::Int32);
			bits = "uint(ins[g_i].index)";
			return;
		default:
			tag = "0u";
			bits = "0u";
			return;
		}
	}

	mem_std::String fail(uint32_t node, Status st) const {
		return mem_std::toString("{ st_status = 1u; st_failNode = ", uintText(node), "; st_failCode = ",
				intText(toInt(st)), "; return; }");
	}

	void writeRun(uint32_t i, bool last, const GpuCont &cont) {
		auto &n = p.nodes[i];
		auto &rt = g.getNodeAt(n.node);
		line(mem_std::toString("\t// ", rt.op->getName(), " #", rt.id));
		line("\tst_steps += 1u;");
		if (n.staticFailure != Status::Ok) {
			line(mem_std::toString("\t", fail(i, n.staticFailure)));
			return;
		}
		auto ins = rt.op->getDataIn();
		const auto notFound = Status::ErrorNotFound;
		const auto invalid = Status::ErrorInvalidArguemnt;
		switch (n.form) {
		case GpuForm::Body: {
			mem_std::String args;
			for (uint32_t pin = 0; pin < n.inputCount; ++pin) {
				auto &in = input(n, pin);
				if (in.source == GpuSource::Name) {
					continue;
				}
				if (!args.empty()) {
					args.append(", ");
				}
				auto text = direct(in, ins[pin].type);
				args.append(text.data(), text.size());
			}
			for (uint32_t c = 0; c < n.cellCount; ++c) {
				if (!args.empty()) {
					args.append(", ");
				}
				auto text = mem_std::toString("c", n.cellBegin + c);
				args.append(text.data(), text.size());
			}
			if (n.fallible) {
				line(mem_std::toString("\t{ int s = ", rt.op->getShaderName(), "(", args, "); if (s != 0) { st_status = 1u; st_failNode = ",
						uintText(i), "; st_failCode = s; return; } }"));
			} else {
				line(mem_std::toString("\t", rt.op->getShaderName(), "(", args, ");"));
			}
			break;
		}
		case GpuForm::Passthrough:
			line(mem_std::toString("\tc", n.cellBegin, " = ", direct(input(n, 0), ins[0].type), ";"));
			break;
		case GpuForm::Widen: {
			for (uint32_t pin = 0; pin < n.inputCount; ++pin) {
				auto &in = input(n, pin);
				if (in.source == GpuSource::Name) {
					continue;
				}
				auto value = direct(in, ins[pin].type);
				line(mem_std::toString("\tct", n.cellBegin, " = ", uintText(widenTag(ins[pin].type)), "; cb",
						n.cellBegin, " = ", bitsText(value, ins[pin].type), ";"));
				break;
			}
			break;
		}
		case GpuForm::Narrow: {
			auto target = p.cells[n.cellBegin].type;
			auto cell = mem_std::toString("c", n.cellBegin);
			const GpuInput *value = nullptr;
			for (uint32_t pin = 0; pin < n.inputCount; ++pin) {
				if (input(n, pin).source != GpuSource::Name) {
					value = &input(n, pin);
				}
			}
			if (n.guard != InvalidIndex) {
				auto &gd = p.guards[n.guard];
				if (gd.source == GpuGuard::Source::Column) {
					line(mem_std::toString("\tif (((ins[g_i].guardMask >> ", uintText(gd.bit), ") & 1u) != 0u) ",
							fail(i, invalid)));
					line(mem_std::toString("\t", cell, " = ", member("ins[g_i].", slotOf(p.inLayout, GpuSlot::Kind::Column, n.guard)), ";"));
				} else {
					line(mem_std::toString("\tif (((blk.guardMask >> ", uintText(gd.bit), ") & 1u) != 0u) ",
							fail(i, invalid)));
					line(mem_std::toString("\t", cell, " = ", member("blk.", slotOf(p.blockLayout, GpuSlot::Kind::BlockNarrowed, n.guard)), ";"));
				}
				break;
			}
			if (value && value->source == GpuSource::Literal) {
				Var narrowed;
				value::castVar(literal(value->index), target, value::CastPolicy::Lossy, narrowed);
				line(mem_std::toString("\t", cell, " = ", literalText(narrowed, target), ";"));
				break;
			}
			if (!value || value->source == GpuSource::Zero) {
				line(mem_std::toString("\t", cell, " = ", zeroText(target), ";"));
				break;
			}
			mem_std::String tag, bits;
			tagged(*value, tag, bits);
			line(mem_std::toString("\t{ uint t = ", tag, "; uint b = ", bits, ";"));
			line(mem_std::toString("\t\tif (t == 0u) { ", cell, " = ", zeroText(target), "; }"));
			switch (target) {
			case VarType::Int32:
				line(mem_std::toString("\t\telse if (t == 1u) { ", cell, " = int(b); }"));
				line(mem_std::toString("\t\telse if (t == 2u) { if (b > 2147483647u) ", fail(i, invalid), " ", cell, " = int(b); }"));
				line(mem_std::toString("\t\telse if (t == 3u) { float f = uintBitsToFloat(b); if (isnan(f) || isinf(f) || !(f >= -2147483648.0 && f < 2147483648.0)) ",
						fail(i, invalid), " ", cell, " = int(f); }"));
				break;
			case VarType::UInt32:
				line(mem_std::toString("\t\telse if (t == 1u) { if (int(b) < 0) ", fail(i, invalid), " ", cell, " = b; }"));
				line(mem_std::toString("\t\telse if (t == 2u) { ", cell, " = b; }"));
				line(mem_std::toString("\t\telse if (t == 3u) { float f = uintBitsToFloat(b); if (isnan(f) || isinf(f) || !(f > -1.0 && f < 4294967296.0)) ",
						fail(i, invalid), " ", cell, " = uint(f); }"));
				break;
			case VarType::Float32:
				line(mem_std::toString("\t\telse if (t == 1u) { ", cell, " = float(int(b)); }"));
				line(mem_std::toString("\t\telse if (t == 2u) { ", cell, " = float(b); }"));
				line(mem_std::toString("\t\telse if (t == 3u) { ", cell, " = uintBitsToFloat(b); }"));
				break;
			default: break;
			}
			line(mem_std::toString("\t\telse ", fail(i, invalid)));
			line("\t}");
			break;
		}
		case GpuForm::SceneHas:
			if (n.own) {
				line(mem_std::toString("\tc", n.cellBegin, " = ((ins[g_i].rowMask >> ", uintText(n.component), ") & 1u) != 0u;"));
			} else if (n.foreign != InvalidIndex) {
				line(mem_std::toString("\tc", n.cellBegin, " = ", member("blk.", slotOf(p.blockLayout, GpuSlot::Kind::ForeignValue, n.foreign)), ";"));
			}
			break;
		case GpuForm::SceneGet: {
			auto &cell = p.cells[n.cellBegin];
			if (n.own) {
				line(mem_std::toString("\tif (((ins[g_i].rowMask >> ", uintText(n.component), ") & 1u) == 0u) ", fail(i, notFound)));
				if (cell.rep == GpuRep::Direct) {
					auto loaded = member("ins[g_i].", slotOf(p.inLayout, GpuSlot::Kind::In, n.in));
					if (n.write != InvalidIndex) {
						line(mem_std::toString("\tc", n.cellBegin, " = cw", n.write, " ? cv", n.write, " : ", loaded, ";"));
					} else {
						line(mem_std::toString("\tc", n.cellBegin, " = ", loaded, ";"));
					}
				} else {
					if (n.write != InvalidIndex) {
						line(mem_std::toString("\tif (cw", n.write, ") { ct", n.cellBegin, " = ct", n.write, "c; cb", n.cellBegin, " = cv", n.write, "; }"));
						line(mem_std::toString("\telse { ct", n.cellBegin, " = ", uintText(GpuTag::Column), "; cb", n.cellBegin, " = ", uintText(n.column), "; }"));
					} else {
						line(mem_std::toString("\tct", n.cellBegin, " = ", uintText(GpuTag::Column), "; cb", n.cellBegin, " = ", uintText(n.column), ";"));
					}
				}
			} else if (n.foreign != InvalidIndex) {
				line(mem_std::toString("\tif (((blk.foreignRowMask >> ", uintText(n.foreign), ") & 1u) == 0u) ", fail(i, notFound)));
				if (cell.rep == GpuRep::Direct) {
					line(mem_std::toString("\tc", n.cellBegin, " = ", member("blk.", slotOf(p.blockLayout, GpuSlot::Kind::ForeignValue, n.foreign)), ";"));
				} else {
					line(mem_std::toString("\tct", n.cellBegin, " = ", uintText(GpuTag::Foreign), "; cb", n.cellBegin, " = ", uintText(n.foreign), ";"));
				}
			}
			break;
		}
		case GpuForm::SceneSet: {
			line(mem_std::toString("\tif (((ins[g_i].rowMask >> ", uintText(n.component), ") & 1u) == 0u) ", fail(i, notFound)));
			auto &write = g.getBlockWrites(p.block)[n.write];
			const GpuInput *value = nullptr;
			uint32_t valuePin = InvalidIndex;
			for (auto &group : rt.op->getSceneGroups()) {
				if (!group.valueIsOutput && group.valuePin != NullPin) {
					valuePin = group.valuePin;
				}
			}
			if (valuePin != InvalidIndex) {
				value = &input(n, valuePin);
			}
			if (isGpuValueType(write.type)) {
				line(mem_std::toString("\tcv", n.write, " = ", value ? direct(*value, write.type) : zeroText(write.type), ";"));
			} else {
				mem_std::String tag("0u"), bits("0u");
				if (value) {
					tagged(*value, tag, bits);
				}
				line(mem_std::toString("\tct", n.write, "c = ", tag, "; cv", n.write, " = ", bits, ";"));
			}
			line(mem_std::toString("\tcw", n.write, " = true;"));
			break;
		}
		case GpuForm::Branch: break; // the continuation's end decides
		default: break;
		}
		(void)last;
		(void)cont;
	}

	void writeCont(uint32_t index) {
		auto &c = p.conts[index];
		line(mem_std::toString("void k", index, "() {"));
		for (uint32_t k = 0; k < c.runCount; ++k) {
			writeRun(p.runs[c.runBegin + k], k + 1 == c.runCount, c);
		}
		switch (c.end) {
		case GpuCont::End::Close: break;
		case GpuCont::End::Budget:
			line(mem_std::toString("\tst_status = 1u; st_budget = 1u; st_failNode = ", uintText(c.node),
					"; st_failCode = ", intText(toInt(Status::ErrorInvalidArguemnt)), ";"));
			break;
		case GpuCont::End::Jump: line(mem_std::toString("\tk", c.next[0], "();")); break;
		case GpuCont::End::Branch: {
			auto &n = p.nodes[c.node];
			auto &rt = g.getNodeAt(n.node);
			auto cond = direct(input(n, 0), rt.op->getDataIn()[0].type);
			line(mem_std::toString("\tbool d = ", cond, ";"));
			line("\tst_dec[st_decN >> 5u] |= uint(d) << (st_decN & 31u);");
			line("\tst_decN += 1u;");
			line(mem_std::toString("\tif (d) { k", c.next[0], "(); } else { k", c.next[1], "(); }"));
			break;
		}
		}
		line("}");
		line("");
	}

	void declareStruct(const GpuLayout &l, StringView indent) {
		mem_std::Vector<const GpuSlot *> order;
		for (auto &s : l.slots) {
			order.emplace_back(&s);
		}
		sprt::sort(order.begin(), order.end(), [](const GpuSlot *a, const GpuSlot *b) { return a->offset < b->offset; });
		for (auto s : order) {
			auto type = s->type == VarType::Bool || s->type == VarType::Nil ? StringView("uint") : getGpuGlslType(s->type);
			if (s->count > 1 || s->kind == GpuSlot::Kind::Decisions || s->kind == GpuSlot::Kind::CopyMask) {
				line(mem_std::toString(indent, type, " ", s->name, "[", s->count, "];"));
			} else {
				line(mem_std::toString(indent, type, " ", s->name, ";"));
			}
		}
	}

	void write() {
		auto &block = g.getBlockAt(p.block);
		line("#version 450");
		line(mem_std::toString("// The body of the parallel block opened by node #", g.getNodeAt(block.fanOut).id,
				", lowered from the graph. Generated: do not edit."));
		line("");
		line("layout (local_size_x = 256) in;");
		line("");
		line("struct In {");
		declareStruct(p.inLayout, "\t");
		line("};");
		line("");
		line("struct Out {");
		declareStruct(p.outLayout, "\t");
		line("};");
		line("");
		line("layout (std430, set = 0, binding = 0) readonly buffer Block {");
		declareStruct(p.blockLayout, "\t");
		line("} blk;");
		line("");
		line("layout (std430, set = 0, binding = 1) readonly buffer Ins { In ins[]; };");
		line("layout (std430, set = 0, binding = 2) buffer Outs { Out outs[]; };");
		line("");

		// The operation bodies, each once, in name order.
		mem_std::Vector<StringView> bodies;
		mem_std::Vector<StringView> sources;
		for (auto &n : p.nodes) {
			auto &rt = g.getNodeAt(n.node);
			if (n.form == GpuForm::Body && rt.op && !rt.op->getShaderSource().empty()) {
				bool seen = false;
				for (auto &b : bodies) {
					seen = seen || b == rt.op->getShaderName();
				}
				if (!seen) {
					bodies.emplace_back(rt.op->getShaderName());
					sources.emplace_back(rt.op->getShaderSource());
				}
			}
		}
		mem_std::Vector<uint32_t> order(bodies.size());
		for (uint32_t k = 0; k < uint32_t(order.size()); ++k) {
			order[k] = k;
		}
		sprt::sort(order.begin(), order.end(), [&](uint32_t a, uint32_t b) { return bodies[a] < bodies[b]; });
		for (auto k : order) {
			out.append(sources[k].data(), sources[k].size());
			line("");
		}

		// The branch's state: the path, and a global per cell and per copy.
		auto decisionWords = sprt::max(uint32_t(1), (p.maxDecisions + 31) / 32);
		line("uint g_i;");
		line("uint st_status;");
		line("uint st_failNode;");
		line("int st_failCode;");
		line("uint st_steps;");
		line("uint st_budget;");
		line("uint st_decN;");
		line(mem_std::toString("uint st_dec[", decisionWords, "];"));
		for (uint32_t w = 0; w < block.writeCount; ++w) {
			auto &write = g.getBlockWrites(p.block)[w];
			line(mem_std::toString("bool cw", w, ";"));
			if (isGpuValueType(write.type)) {
				line(mem_std::toString(getGpuGlslType(write.type), " cv", w, ";"));
			} else {
				line(mem_std::toString("uint ct", w, "c;"));
				line(mem_std::toString("uint cv", w, ";"));
			}
		}
		for (uint32_t c = 0; c < uint32_t(p.cells.size()); ++c) {
			if (p.cells[c].rep == GpuRep::Direct) {
				// Not `precise`: glslang writes a module whose global variable is used before it is
				// defined for one, and spirv-val refuses it. Reassociation between the CPU and a
				// device is what the level-C tolerance is for.
				line(mem_std::toString(getGpuGlslType(p.cells[c].type), " c", c, ";"));
			} else {
				line(mem_std::toString("uint ct", c, ";"));
				line(mem_std::toString("uint cb", c, ";"));
			}
		}
		line("");

		// Callees before callers: a post-order walk from the open.
		mem_std::Vector<uint8_t> done(p.conts.size(), 0);
		mem_std::Vector<uint32_t> emitOrder;
		mem_std::Function<void(uint32_t)> visit = [&](uint32_t c) {
			if (c >= p.conts.size() || done[c]) {
				return;
			}
			done[c] = 1;
			auto &cont = p.conts[c];
			if (cont.end == GpuCont::End::Branch || cont.end == GpuCont::End::Jump) {
				visit(cont.next[0]);
				if (cont.end == GpuCont::End::Branch) {
					visit(cont.next[1]);
				}
			}
			emitOrder.emplace_back(c);
		};
		visit(0);
		for (auto c : emitOrder) {
			writeCont(c);
		}

		line("void main() {");
		line("\tg_i = gl_GlobalInvocationID.x;");
		line("\tif (g_i >= blk.count) {");
		line("\t\treturn;");
		line("\t}");
		// A global of a compute shader starts undefined: every one of them starts at zero here, as
		// a record does on the CPU.
		line("\tst_status = 0u; st_failNode = 0u; st_failCode = 0; st_steps = 0u; st_budget = 0u; st_decN = 0u;");
		for (uint32_t k = 0; k < decisionWords; ++k) {
			line(mem_std::toString("\tst_dec[", k, "] = 0u;"));
		}
		for (uint32_t w = 0; w < block.writeCount; ++w) {
			auto &write = g.getBlockWrites(p.block)[w];
			if (isGpuValueType(write.type)) {
				line(mem_std::toString("\tcw", w, " = false; cv", w, " = ", zeroText(write.type), ";"));
			} else {
				line(mem_std::toString("\tcw", w, " = false; ct", w, "c = 0u; cv", w, " = 0u;"));
			}
		}
		for (uint32_t c = 0; c < uint32_t(p.cells.size()); ++c) {
			if (p.cells[c].rep == GpuRep::Direct) {
				line(mem_std::toString("\tc", c, " = ", zeroText(p.cells[c].type), ";"));
			} else {
				line(mem_std::toString("\tct", c, " = 0u; cb", c, " = 0u;"));
			}
		}
		line("\tif (ins[g_i].preFailed != 0u) {");
		line("\t\tst_status = 1u; st_failNode = ins[g_i].preNode; st_failCode = ins[g_i].preCode;");
		line("\t\tst_steps = ins[g_i].preSteps; st_budget = ins[g_i].preBudget;");
		line("\t} else {");
		line("\t\tk0();");
		line("\t}");
		line("\touts[g_i].status = st_status;");
		line("\touts[g_i].failNode = st_failNode;");
		line("\touts[g_i].failCode = st_failCode;");
		line("\touts[g_i].steps = st_steps;");
		line("\touts[g_i].budget = st_budget;");
		line("\touts[g_i].decisionCount = st_decN;");
		for (uint32_t k = 0; k < decisionWords; ++k) {
			line(mem_std::toString("\touts[g_i].decisions[", k, "] = st_dec[", k, "];"));
		}
		auto copyWords = sprt::max(uint32_t(1), (block.writeCount + 31) / 32);
		for (uint32_t k = 0; k < copyWords; ++k) {
			mem_std::String mask("0u");
			for (uint32_t w = k * 32; w < sprt::min(block.writeCount, (k + 1) * 32); ++w) {
				mask = mem_std::toString(mask, " | (uint(cw", w, ") << ", w - k * 32, "u)");
			}
			line(mem_std::toString("\touts[g_i].copyMask[", k, "] = ", mask, ";"));
		}
		for (uint32_t w = 0; w < block.writeCount; ++w) {
			auto &write = g.getBlockWrites(p.block)[w];
			if (isGpuValueType(write.type)) {
				if (write.type == VarType::Bool) {
					line(mem_std::toString("\touts[g_i].wv", w, " = uint(cv", w, ");"));
				} else {
					line(mem_std::toString("\touts[g_i].wv", w, " = cv", w, ";"));
				}
			} else {
				line(mem_std::toString("\touts[g_i].wt", w, " = ct", w, "c;"));
				line(mem_std::toString("\touts[g_i].wv", w, " = cv", w, ";"));
			}
		}
		for (uint32_t c = 0; c < uint32_t(p.cells.size()); ++c) {
			if (p.cells[c].rep == GpuRep::Direct) {
				if (p.cells[c].type == VarType::Bool) {
					line(mem_std::toString("\touts[g_i].c", c, " = uint(c", c, ");"));
				} else {
					line(mem_std::toString("\touts[g_i].c", c, " = c", c, ";"));
				}
			} else {
				line(mem_std::toString("\touts[g_i].ct", c, " = ct", c, ";"));
				line(mem_std::toString("\touts[g_i].cb", c, " = cb", c, ";"));
			}
		}
		line("}");
	}
};

} // namespace gpu

template <typename Graph>
bool isGpuBlock(const Graph &g, uint32_t blockIndex) {
	if (blockIndex >= g.getBlockCount()) {
		return false;
	}
	auto fanOut = g.getBlockAt(blockIndex).fanOut;
	auto &node = g.getNodeAt(fanOut);
	uint32_t index = 0;
	if (!node.op || !node.op->findSetting(StringView("executors"), index)) {
		return false;
	}
	if (auto value = g.getSetting(fanOut, index); value && value->isArray()) {
		for (auto &it : value->asArray()) {
			if (it.getString() == StringView("gpu")) {
				return true;
			}
		}
	}
	return false;
}

template <typename Graph>
bool isGpuCandidate(const Graph &g, uint32_t blockIndex) {
	if (blockIndex >= g.getBlockCount()) {
		return false;
	}
	if (isGpuBlock(g, blockIndex)) {
		return true;
	}
	// An author who named nothing left the choice to the heuristic, and the heuristic may only
	// choose what exists: the body is lowered, and if it does not lower, the block quietly has no
	// GPU arm.
	auto fanOut = g.getBlockAt(blockIndex).fanOut;
	auto &node = g.getNodeAt(fanOut);
	uint32_t index = 0;
	if (!node.op || !node.op->findSetting(StringView("executors"), index)) {
		return true;
	}
	auto value = g.getSetting(fanOut, index);
	return !value || !value->isArray() || value->asArray().empty();
}

template <typename Graph>
uint64_t hashGpuBlock(const Graph &g, uint32_t blockIndex, mem_std::String *text) {
	if (!isGpuCandidate(g, blockIndex)) {
		return 0;
	}
	GpuProgram program;
	if (lowerGpuBlock(g, blockIndex, program) != Status::Ok) {
		return 0;
	}
	auto glsl = writeGpuGlsl(g, program);
	auto hash = sprt::hash64(glsl.data(), glsl.size());
	if (text) {
		*text = sprt::move(glsl);
	}
	return hash;
}

template <typename Graph>
void collectGpuReducers(const Graph &g, const GpuProgram &p, mem_std::Vector<GpuReducer> &out) {
	out.clear();
	auto &block = g.getBlockAt(p.block);
	for (auto collector : g.getBlockCollectors(p.block)) {
		auto &rt = g.getNodeAt(collector);
		if (!rt.op || rt.op->getBranchPin() == NullPin) {
			continue;
		}
		auto name = rt.op->getInlineName();
		auto pin = rt.op->getBranchPin();
		auto type = rt.op->getDataIn()[pin].type;
		StringView kind;
		if (name.starts_with(StringView("flow::ops::inl::parSum"))) {
			kind = StringView("sum");
		} else if (name.starts_with(StringView("flow::ops::inl::parMin"))) {
			kind = StringView("min");
		} else if (name.starts_with(StringView("flow::ops::inl::parMax"))) {
			kind = StringView("max");
		} else if (name == StringView("flow::ops::inl::parCount")) {
			kind = StringView("count");
		} else if (name == StringView("flow::ops::inl::parAny")) {
			kind = StringView("any");
		} else if (name == StringView("flow::ops::inl::parAll")) {
			kind = StringView("all");
		} else {
			continue;
		}
		bool known = false;
		for (auto &it : getGpuReducers()) {
			known = known || (it.first == kind && it.second == type);
		}
		if (!known) {
			continue;
		}
		GpuReducer r;
		r.collector = collector;
		r.kind = kind;
		r.type = type;
		for (auto e : g.getDataInEdges(collector)) {
			auto &edge = g.getDataEdges()[e];
			if (edge.dstPin != pin) {
				continue;
			}
			if (edge.srcNode == block.fanOut) {
				r.source = GpuSource::Index;
			} else if (auto node = p.findNode(edge.srcNode)) {
				r.source = GpuSource::Cell;
				r.cell = node->cellBegin + edge.srcPin;
			}
		}
		if (r.source != GpuSource::Zero) {
			out.emplace_back(r);
		}
	}
}

template <typename Graph>
mem_std::String writeGpuGlsl(const Graph &g, const GpuProgram &p) {
	gpu::GlslWriter<Graph> w{g, p};
	w.write();
	return sprt::move(w.out);
}

template <typename Graph>
uint32_t GpuShaderTable::attach(const Graph &g, SpanView<GpuShaderBinary> shaders) {
	clear();
	for (uint32_t b = 0; b < g.getBlockCount(); ++b) {
		if (!isGpuCandidate(g, b)) {
			continue;
		}
		GpuBlockShaders entry;
		entry.block = b;
		mem_std::String text;
		entry.textHash = hashGpuBlock(g, b, &text);
		if (entry.textHash == 0
				|| lowerGpuBlock(g, b, entry.program, nullptr) != Status::Ok) {
			continue;
		}
		entry.program.hash = entry.textHash;
		for (auto &it : shaders) {
			if (it.textHash == entry.textHash && !it.spirv.empty()) {
				entry.spirv = mem_std::Vector<uint32_t>(it.spirv.begin(), it.spirv.end());
				break;
			}
		}
		if (entry.spirv.empty()) {
			continue; // the host has no shader of this text: the block stays on the CPU
		}
		// A collector folds on the device only when its own reducer came too, by name and by text.
		mem_std::Vector<GpuReducer> reducers;
		collectGpuReducers(g, entry.program, reducers);
		for (auto &r : reducers) {
			GpuReducerBinding binding;
			binding.reducer = r;
			binding.name = getGpuReduceName(r.kind, r.type);
			auto reduceText = writeGpuReduceGlsl(r.kind, r.type);
			binding.textHash = sprt::hash64(reduceText.data(), reduceText.size());
			for (auto &it : shaders) {
				if (it.name == binding.name && it.textHash == binding.textHash && !it.spirv.empty()) {
					binding.spirv = mem_std::Vector<uint32_t>(it.spirv.begin(), it.spirv.end());
					break;
				}
			}
			if (!binding.spirv.empty()) {
				entry.reducers.emplace_back(sprt::move(binding));
			}
		}
		_blocks.emplace_back(sprt::move(entry));
	}
	uint64_t identity = 0;
	auto mix = [&](uint64_t h) {
		identity = sprt::hash64(reinterpret_cast<const char *>(&h), sizeof(h), identity);
	};
	for (auto &it : _blocks) {
		mix(it.textHash);
		for (auto &r : it.reducers) {
			mix(r.textHash);
		}
	}
	_identity = identity;
	return uint32_t(_blocks.size());
}

} // namespace stappler::flow

#endif /* STAPPLER_FLOW_SPFLOWGPU_HPP_ */
