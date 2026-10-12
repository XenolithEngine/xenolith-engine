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

// A GPU body run on the CPU: the lowered program executed the way its shader executes it - the same
// buffers in, the same Out record back - with each operation's body the C++ one, called through a
// door that holds nothing but values. What this proves is the lowering: the order, the paths, the
// copies, the tags and the prefix. What a GLSL or WGSL body computes on a device is a device
// harness's to hold.

#ifndef TESTS_FLOW_INTERP_GPU_EVAL_H_
#define TESTS_FLOW_INTERP_GPU_EVAL_H_

#include "SPFlowGpu.h"
#include "SPFlowContext.h"

namespace STAPPLER_VERSIONIZED stappler::test::gpueval {

using flow::GpuForm;
using flow::GpuInput;
using flow::GpuProgram;
using flow::GpuRep;
using flow::GpuSlot;
using flow::GpuSource;
using flow::GpuTag;
using flow::value::Var;
using flow::value::VarType;

// A door with values only: what a pure operation reads and writes, and nothing it could not.
class ValueContext final : public flow::OpContext {
public:
	mem_std::Vector<Var> inputs;
	mem_std::Vector<Var> outputs;

	void bind(const flow::OpDesc &op) {
		_op = &op;
		inputs.assign(op.getDataIn().size(), Var());
		outputs.assign(op.getDataOut().size(), Var());
	}

	bool hasInput(uint32_t pin) const override { return pin < inputs.size(); }
	Status getInput(uint32_t pin, Var &out) const override {
		if (pin >= inputs.size()) {
			return Status::ErrorInvalidArguemnt;
		}
		out = inputs[pin];
		return Status::Ok;
	}
	flow::Addr getInputAddr(uint32_t) const override { return flow::NullAddr; }
	Status getInputName(uint32_t, StringView &) const override { return Status::ErrorNotImplemented; }
	Status getInputCount(uint32_t, uint32_t &) const override { return Status::ErrorNotImplemented; }
	Status getInputElement(uint32_t, uint32_t, Var &) const override {
		return Status::ErrorNotImplemented;
	}
	flow::Addr claimOutput(uint32_t) override { return flow::NullAddr; }
	Status setOutput(uint32_t pin, const Var &v) override {
		if (pin >= outputs.size()) {
			return Status::ErrorInvalidArguemnt;
		}
		outputs[pin] = v;
		return Status::Ok;
	}
	Status copyInputToOutput(uint32_t, uint32_t) override { return Status::ErrorNotImplemented; }
	Status getLocal(uint32_t, Var &) const override { return Status::ErrorNotImplemented; }
	Status setLocal(uint32_t, const Var &) override { return Status::ErrorNotImplemented; }
	flow::Addr localAddr(uint32_t) const override { return flow::NullAddr; }
	Status fire(uint32_t) override { return Status::Ok; }
	Status fire(StringView) override { return Status::Ok; }
	flow::value::EntityId getGlobalEntity() const override { return flow::value::EntityId(); }
	bool sceneHas(flow::value::EntityId, flow::TypeId) const override { return false; }
	Status sceneGet(flow::value::EntityId, flow::TypeId, StringView, Var &) const override {
		return Status::ErrorNotImplemented;
	}
	Status sceneSet(flow::value::EntityId, flow::TypeId, StringView, const Var &) override {
		return Status::ErrorNotImplemented;
	}
	Status sceneFind(flow::TypeId, StringView, const Var &, flow::value::EntityId &) const override {
		return Status::ErrorNotImplemented;
	}
	Status sceneAdd(flow::value::EntityId, flow::TypeId) override {
		return Status::ErrorNotImplemented;
	}
	Status sceneRemove(flow::value::EntityId, flow::TypeId) override {
		return Status::ErrorNotImplemented;
	}
	flow::value::ExtensionInstance *getExtension(uint32_t) const override { return nullptr; }
	bool sceneHas(flow::value::EntityId, const flow::SceneBinding &) const override { return false; }
	Status sceneGet(flow::value::EntityId, const flow::SceneBinding &, Var &) const override {
		return Status::ErrorNotImplemented;
	}
	Status sceneSet(flow::value::EntityId, const flow::SceneBinding &, const Var &) override {
		return Status::ErrorNotImplemented;
	}
	Status sceneFind(const flow::SceneBinding &, const Var &,
			flow::value::EntityId &) const override {
		return Status::ErrorNotImplemented;
	}
	Status sceneAdd(flow::value::EntityId, const flow::SceneBinding &) override {
		return Status::ErrorNotImplemented;
	}
	Status sceneRemove(flow::value::EntityId, const flow::SceneBinding &) override {
		return Status::ErrorNotImplemented;
	}
	Status queryEntities(uint32_t) override { return Status::ErrorNotImplemented; }
	Status getBranchCount(uint32_t, uint32_t &) const override { return Status::ErrorNotImplemented; }
	bool isBranchPresent(uint32_t, uint32_t) const override { return false; }
	Status getBranchValue(uint32_t, uint32_t, Var &) const override {
		return Status::ErrorNotImplemented;
	}
	Status getBarrierCount(uint32_t &) const override { return Status::ErrorNotImplemented; }
	Status getBarrierBranch(uint32_t, bool &, bool &) const override {
		return Status::ErrorNotImplemented;
	}
	Status call() override { return Status::ErrorNotImplemented; }
	Status copyArgument(uint32_t, uint32_t) override { return Status::ErrorNotImplemented; }
	Status copyResult(uint32_t) override { return Status::ErrorNotImplemented; }
	Status setResultExit(uint32_t) override { return Status::ErrorNotImplemented; }
	flow::value::ArenaRef getArena() const override { return flow::value::ArenaRef(); }
	uint64_t getEnvTag() const override { return 0; }
	const void *getSceneRef() const override { return nullptr; }
};

struct Tagged {
	uint32_t tag = 0;
	uint32_t bits = 0;
};

inline const GpuSlot *slotOf(const flow::GpuLayout &l, GpuSlot::Kind kind, uint32_t index = 0) {
	for (auto &s : l.slots) {
		if (s.kind == kind && s.index == index) {
			return &s;
		}
	}
	return nullptr;
}

inline uint32_t getU(const uint8_t *base, const GpuSlot *slot, uint32_t word = 0) {
	uint32_t v = 0;
	if (slot) {
		sprt::memcpy(&v, base + slot->offset + 4 * word, 4);
	}
	return v;
}

inline void putU(uint8_t *base, const GpuSlot *slot, uint32_t v, uint32_t word = 0) {
	if (slot) {
		sprt::memcpy(base + slot->offset + 4 * word, &v, 4);
	}
}

inline Var zeroOf(VarType type) {
	Var v;
	flow::value::decodeVar(mem_std::Value(), type, v);
	return v;
}

inline uint32_t widenTag(VarType type) {
	switch (type) {
	case VarType::UInt32: return GpuTag::UInt32;
	case VarType::Float32: return GpuTag::Float32;
	default: return GpuTag::Int32;
	}
}

inline uint32_t bitsOf(const Var &v, VarType type) {
	uint32_t bits = 0;
	if (type == VarType::Float32) {
		sprt::memcpy(&bits, &v.v[0], 4);
	} else {
		bits = uint32_t(v.i);
	}
	return bits;
}

template <typename Graph>
struct Evaluator {
	const Graph &g;
	const GpuProgram &p;
	const flow::GpuLoaded &loaded;

	// The branch in progress.
	uint32_t branch = 0;
	const uint8_t *in = nullptr;
	mem_std::Vector<Var> cells;
	mem_std::Vector<Tagged> tags;
	mem_std::Vector<uint8_t> written;
	mem_std::Vector<Var> copies;
	mem_std::Vector<Tagged> copyTags;
	uint32_t status = 0, failNode = 0, steps = 0, budget = 0;
	int32_t failCode = 0;
	mem_std::Vector<uint32_t> decisions;

	const uint8_t *blk() const { return loaded.block.data(); }

	Var blockValue(uint32_t k) const {
		auto slot = slotOf(p.blockLayout, GpuSlot::Kind::BlockValue, k);
		return slot ? flow::readGpuValue(blk() + slot->offset, p.blockValues[k].type) : Var();
	}

	Var direct(const GpuInput &input, VarType pinType) const {
		Var raw;
		VarType from = pinType;
		switch (input.source) {
		case GpuSource::Cell:
			raw = cells[input.index];
			from = p.cells[input.index].type;
			break;
		case GpuSource::Literal: {
			auto &lit = p.literals[input.index];
			if (auto c = g.getConstant(lit.node, lit.pin)) {
				flow::value::decodeVar(*c, pinType, raw);
			} else {
				raw = zeroOf(pinType);
			}
			return raw;
		}
		case GpuSource::Block:
			raw = blockValue(input.index);
			from = p.blockValues[input.index].type;
			break;
		case GpuSource::Index: {
			int32_t index = 0;
			sprt::memcpy(&index, in + slotOf(p.inLayout, GpuSlot::Kind::Index)->offset, 4);
			raw = flow::value::makeInt32(index);
			from = VarType::Int32;
			break;
		}
		default: return zeroOf(pinType);
		}
		if (from == pinType) {
			return raw;
		}
		Var out;
		if (flow::value::castVar(raw, pinType, flow::value::CastPolicy::Lossless, out) != Status::Ok) {
			return zeroOf(pinType);
		}
		return out;
	}

	Tagged tagged(const GpuInput &input) const {
		switch (input.source) {
		case GpuSource::Cell: {
			auto &cell = p.cells[input.index];
			if (cell.rep == GpuRep::Tagged) {
				return tags[input.index];
			}
			return Tagged{widenTag(cell.type), bitsOf(cells[input.index], cell.type)};
		}
		case GpuSource::Literal: return Tagged{GpuTag::Literal, input.index};
		case GpuSource::Block: {
			auto &value = p.blockValues[input.index];
			if (flow::isGpuValueType(value.type)) {
				return Tagged{widenTag(value.type), bitsOf(blockValue(input.index), value.type)};
			}
			return Tagged{GpuTag::Block, input.index};
		}
		case GpuSource::Index: {
			int32_t index = 0;
			sprt::memcpy(&index, in + slotOf(p.inLayout, GpuSlot::Kind::Index)->offset, 4);
			return Tagged{GpuTag::Int32, uint32_t(index)};
		}
		default: return Tagged();
		}
	}

	bool fail(uint32_t node, Status st) {
		status = 1;
		failNode = node;
		failCode = toInt(st);
		return false;
	}

	// A tag and its bits narrowed to `target`, refusing as the shader does.
	bool narrowTagged(uint32_t node, Tagged t, VarType target, Var &out) {
		Var host;
		switch (t.tag) {
		case GpuTag::Zero: out = zeroOf(target); return true;
		case GpuTag::Int32: host = flow::value::makeInt(int32_t(t.bits)); break;
		case GpuTag::UInt32: host = flow::value::makeInt(int64_t(t.bits)); break;
		case GpuTag::Float32: {
			float f = 0.0f;
			sprt::memcpy(&f, &t.bits, 4);
			host = flow::value::makeFloat(double(f));
			break;
		}
		default: return fail(node, Status::ErrorInvalidArguemnt);
		}
		if (flow::value::castVar(host, target, flow::value::CastPolicy::Lossy, out) != Status::Ok) {
			return fail(node, Status::ErrorInvalidArguemnt);
		}
		return true;
	}

	// One unit; false when the branch failed on it.
	bool run(uint32_t i) {
		auto &n = p.nodes[i];
		auto &rt = g.getNodeAt(n.node);
		++steps;
		if (n.staticFailure != Status::Ok) {
			return fail(i, n.staticFailure);
		}
		auto ins = rt.op->getDataIn();
		auto rows = getU(in, slotOf(p.inLayout, GpuSlot::Kind::RowMask));
		auto input = [&](uint32_t pin) -> const GpuInput & { return p.inputs[n.inputBegin + pin]; };
		switch (n.form) {
		case GpuForm::Body: {
			ValueContext ctx;
			ctx.bind(*rt.op);
			for (uint32_t pin = 0; pin < n.inputCount; ++pin) {
				if (input(pin).source != GpuSource::Name) {
					ctx.inputs[pin] = direct(input(pin), ins[pin].type);
				}
			}
			auto st = rt.op->getInvoke()(ctx);
			if (st != Status::Ok) {
				return fail(i, st);
			}
			for (uint32_t c = 0; c < n.cellCount; ++c) {
				cells[n.cellBegin + c] = ctx.outputs[c];
			}
			return true;
		}
		case GpuForm::Passthrough: cells[n.cellBegin] = direct(input(0), ins[0].type); return true;
		case GpuForm::Widen:
			for (uint32_t pin = 0; pin < n.inputCount; ++pin) {
				if (input(pin).source != GpuSource::Name) {
					auto v = direct(input(pin), ins[pin].type);
					tags[n.cellBegin] = Tagged{widenTag(ins[pin].type), bitsOf(v, ins[pin].type)};
					break;
				}
			}
			return true;
		case GpuForm::Narrow: {
			auto target = p.cells[n.cellBegin].type;
			const GpuInput *value = nullptr;
			for (uint32_t pin = 0; pin < n.inputCount; ++pin) {
				if (input(pin).source != GpuSource::Name) {
					value = &input(pin);
				}
			}
			if (n.guard != flow::InvalidIndex) {
				auto &gd = p.guards[n.guard];
				if (gd.source == flow::GpuGuard::Source::Column) {
					if ((getU(in, slotOf(p.inLayout, GpuSlot::Kind::GuardMask)) >> gd.bit) & 1) {
						return fail(i, Status::ErrorInvalidArguemnt);
					}
					auto slot = slotOf(p.inLayout, GpuSlot::Kind::Column, n.guard);
					cells[n.cellBegin] = flow::readGpuValue(in + slot->offset, target);
				} else {
					if ((getU(blk(), slotOf(p.blockLayout, GpuSlot::Kind::BlockGuardMask)) >> gd.bit)
							& 1) {
						return fail(i, Status::ErrorInvalidArguemnt);
					}
					auto slot = slotOf(p.blockLayout, GpuSlot::Kind::BlockNarrowed, n.guard);
					cells[n.cellBegin] = flow::readGpuValue(blk() + slot->offset, target);
				}
				return true;
			}
			if (value && value->source == GpuSource::Literal) {
				auto &lit = p.literals[value->index];
				Var decoded, narrowed;
				if (auto c = g.getConstant(lit.node, lit.pin)) {
					flow::value::decodeVar(*c, lit.type, decoded);
				}
				flow::value::castVar(decoded, target, flow::value::CastPolicy::Lossy, narrowed);
				cells[n.cellBegin] = narrowed;
				return true;
			}
			if (!value || value->source == GpuSource::Zero) {
				cells[n.cellBegin] = zeroOf(target);
				return true;
			}
			return narrowTagged(i, tagged(*value), target, cells[n.cellBegin]);
		}
		case GpuForm::SceneHas:
			if (n.own) {
				cells[n.cellBegin] = flow::value::makeBool(((rows >> n.component) & 1) != 0);
			} else if (n.foreign != flow::InvalidIndex) {
				auto slot = slotOf(p.blockLayout, GpuSlot::Kind::ForeignValue, n.foreign);
				cells[n.cellBegin] = flow::readGpuValue(blk() + slot->offset, VarType::Bool);
			}
			return true;
		case GpuForm::SceneGet: {
			auto &cell = p.cells[n.cellBegin];
			if (n.own) {
				if (((rows >> n.component) & 1) == 0) {
					return fail(i, Status::ErrorNotFound);
				}
				if (cell.rep == GpuRep::Direct) {
					if (n.write != flow::InvalidIndex && written[n.write]) {
						cells[n.cellBegin] = copies[n.write];
					} else {
						auto slot = slotOf(p.inLayout, GpuSlot::Kind::In, n.in);
						cells[n.cellBegin] = flow::readGpuValue(in + slot->offset, cell.type);
					}
				} else if (n.write != flow::InvalidIndex && written[n.write]) {
					tags[n.cellBegin] = copyTags[n.write];
				} else {
					tags[n.cellBegin] = Tagged{GpuTag::Column, n.column};
				}
			} else if (n.foreign != flow::InvalidIndex) {
				if (((getU(blk(), slotOf(p.blockLayout, GpuSlot::Kind::ForeignRowMask)) >> n.foreign)
							& 1)
						== 0) {
					return fail(i, Status::ErrorNotFound);
				}
				if (cell.rep == GpuRep::Direct) {
					auto slot = slotOf(p.blockLayout, GpuSlot::Kind::ForeignValue, n.foreign);
					cells[n.cellBegin] = flow::readGpuValue(blk() + slot->offset, cell.type);
				} else {
					tags[n.cellBegin] = Tagged{GpuTag::Foreign, n.foreign};
				}
			}
			return true;
		}
		case GpuForm::SceneSet: {
			if (((rows >> n.component) & 1) == 0) {
				return fail(i, Status::ErrorNotFound);
			}
			auto &write = g.getBlockWrites(p.block)[n.write];
			const GpuInput *value = nullptr;
			for (auto &group : rt.op->getSceneGroups()) {
				if (!group.valueIsOutput && group.valuePin != flow::NullPin) {
					value = &input(group.valuePin);
				}
			}
			if (flow::isGpuValueType(write.type)) {
				copies[n.write] = value ? direct(*value, write.type) : zeroOf(write.type);
			} else {
				copyTags[n.write] = value ? tagged(*value) : Tagged();
			}
			written[n.write] = 1;
			return true;
		}
		default: return true;
		}
	}

	void runCont(uint32_t k) {
		auto &c = p.conts[k];
		for (uint32_t r = 0; r < c.runCount; ++r) {
			if (!run(p.runs[c.runBegin + r])) {
				return;
			}
		}
		switch (c.end) {
		case flow::GpuCont::End::Close: return;
		case flow::GpuCont::End::Budget:
			status = 1;
			budget = 1;
			failNode = c.node;
			failCode = toInt(Status::ErrorInvalidArguemnt);
			return;
		case flow::GpuCont::End::Jump: runCont(c.next[0]); return;
		case flow::GpuCont::End::Branch: {
			auto &n = p.nodes[c.node];
			auto &rt = g.getNodeAt(n.node);
			auto cond = direct(p.inputs[n.inputBegin], rt.op->getDataIn()[0].type);
			auto d = cond.i != 0;
			decisions.emplace_back(d ? 1 : 0);
			runCont(c.next[d ? 0 : 1]);
			return;
		}
		}
	}

	// Every branch; the Out records, as the shader would leave them.
	mem_std::Vector<uint8_t> runAll() {
		auto &block = g.getBlockAt(p.block);
		mem_std::Vector<uint8_t> outs(size_t(p.outLayout.size) * loaded.branches, 0);
		for (branch = 0; branch < loaded.branches; ++branch) {
			in = loaded.ins.data() + size_t(p.inLayout.size) * branch;
			cells.assign(p.cells.size(), Var());
			for (uint32_t c = 0; c < uint32_t(p.cells.size()); ++c) {
				cells[c] = zeroOf(p.cells[c].type);
			}
			tags.assign(p.cells.size(), Tagged());
			written.assign(block.writeCount, 0);
			copies.assign(block.writeCount, Var());
			copyTags.assign(block.writeCount, Tagged());
			for (uint32_t w = 0; w < block.writeCount; ++w) {
				copies[w] = zeroOf(g.getBlockWrites(p.block)[w].type);
			}
			status = failNode = steps = budget = 0;
			failCode = 0;
			decisions.clear();

			if (getU(in, slotOf(p.inLayout, GpuSlot::Kind::PreFailed)) != 0) {
				status = 1;
				failNode = getU(in, slotOf(p.inLayout, GpuSlot::Kind::PreNode));
				failCode = int32_t(getU(in, slotOf(p.inLayout, GpuSlot::Kind::PreCode)));
				steps = getU(in, slotOf(p.inLayout, GpuSlot::Kind::PreSteps));
				budget = getU(in, slotOf(p.inLayout, GpuSlot::Kind::PreBudget));
			} else {
				runCont(0);
			}

			auto o = outs.data() + size_t(p.outLayout.size) * branch;
			auto &l = p.outLayout;
			putU(o, slotOf(l, GpuSlot::Kind::Status), status);
			putU(o, slotOf(l, GpuSlot::Kind::FailNode), failNode);
			putU(o, slotOf(l, GpuSlot::Kind::FailCode), uint32_t(failCode));
			putU(o, slotOf(l, GpuSlot::Kind::Steps), steps);
			putU(o, slotOf(l, GpuSlot::Kind::Budget), budget);
			putU(o, slotOf(l, GpuSlot::Kind::DecisionCount), uint32_t(decisions.size()));
			for (uint32_t k = 0; k < uint32_t(decisions.size()); ++k) {
				auto slot = slotOf(l, GpuSlot::Kind::Decisions);
				auto word = getU(o, slot, k / 32) | (decisions[k] << (k % 32));
				putU(o, slot, word, k / 32);
			}
			for (uint32_t w = 0; w < block.writeCount; ++w) {
				auto mask = slotOf(l, GpuSlot::Kind::CopyMask);
				putU(o, mask, getU(o, mask, w / 32) | (uint32_t(written[w]) << (w % 32)), w / 32);
				auto &write = g.getBlockWrites(p.block)[w];
				auto value = slotOf(l, GpuSlot::Kind::Copy, w);
				if (flow::isGpuValueType(write.type)) {
					flow::writeGpuValue(o + value->offset, write.type, copies[w]);
				} else {
					putU(o, slotOf(l, GpuSlot::Kind::CopyTag, w), copyTags[w].tag);
					putU(o, value, copyTags[w].bits);
				}
			}
			for (uint32_t c = 0; c < uint32_t(p.cells.size()); ++c) {
				auto value = slotOf(l, GpuSlot::Kind::Cell, c);
				if (p.cells[c].rep == GpuRep::Direct) {
					flow::writeGpuValue(o + value->offset, p.cells[c].type, cells[c]);
				} else {
					putU(o, slotOf(l, GpuSlot::Kind::CellTag, c), tags[c].tag);
					putU(o, value, tags[c].bits);
				}
			}
		}
		return outs;
	}
};

} // namespace stappler::test::gpueval

#endif /* TESTS_FLOW_INTERP_GPU_EVAL_H_ */
