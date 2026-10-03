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

#ifndef STAPPLER_FLOW_SPFLOWGPULOAD_HPP_
#define STAPPLER_FLOW_SPFLOWGPULOAD_HPP_

// The CPU side of a GPU dispatch (SPFlowGpu.h): the loader fills the Block and In buffers and
// decides the prefix's refusals before dispatch; the decoder reads an Out record back into host
// values and a replayed path. Neither touches a device - the executor puts them around one.

#include "SPFlowGpu.h"

namespace STAPPLER_VERSIONIZED stappler::flow {

namespace gpu {

inline const GpuSlot *findSlot(const GpuLayout &l, GpuSlot::Kind kind, uint32_t index = 0) {
	for (auto &s : l.slots) {
		if (s.kind == kind && s.index == index) {
			return &s;
		}
	}
	return nullptr;
}

inline void putUInt(uint8_t *base, const GpuSlot *slot, uint32_t v, uint32_t word = 0) {
	if (slot) {
		sprt::memcpy(base + slot->offset + 4 * word, &v, 4);
	}
}

inline void putInt(uint8_t *base, const GpuSlot *slot, int32_t v) {
	if (slot) {
		sprt::memcpy(base + slot->offset, &v, 4);
	}
}

inline uint32_t getUInt(const uint8_t *base, const GpuSlot *slot, uint32_t word = 0) {
	uint32_t v = 0;
	if (slot) {
		sprt::memcpy(&v, base + slot->offset + 4 * word, 4);
	}
	return v;
}

inline Status literalVar(const auto &g, const GpuLiteral &lit, Var &out) {
	if (auto c = g.getConstant(lit.node, lit.pin)) {
		return value::decodeVar(*c, lit.type, out);
	}
	return value::decodeVar(mem_std::Value(), lit.type, out);
}

// A tag and its bits as a value of `type`, a type outside the GPU set.
template <typename Graph>
Var widen(const Graph &g, const GpuProgram &p, const GpuLoaded &loaded, uint32_t branch, uint32_t tag,
		uint32_t bits, VarType type, TypeId subtype) {
	auto fromInt = [&](int64_t i) {
		switch (type) {
		case VarType::Enum: {
			auto v = value::makeEnum(i);
			v.e.type = subtype;
			return v;
		}
		case VarType::Float: return value::makeFloat(double(i));
		default: return value::makeInt(i);
		}
	};
	switch (tag) {
	case GpuTag::Int32: return fromInt(int32_t(bits));
	case GpuTag::UInt32: return fromInt(int64_t(bits));
	case GpuTag::Float32: {
		float f = 0.0f;
		sprt::memcpy(&f, &bits, 4);
		return value::makeFloat(double(f));
	}
	case GpuTag::Column:
		if (bits < p.columns.size()) {
			return loaded.columns[branch * p.columns.size() + bits];
		}
		break;
	case GpuTag::Literal:
		if (bits < p.literals.size()) {
			Var v;
			literalVar(g, p.literals[bits], v);
			Var out;
			if (value::castVar(v, type, value::CastPolicy::Lossy, out) == Status::Ok) {
				return out;
			}
			return v;
		}
		break;
	case GpuTag::Block:
		if (bits < loaded.blockValues.size()) {
			return loaded.blockValues[bits];
		}
		break;
	case GpuTag::Foreign:
		if (bits < loaded.foreignValues.size()) {
			return loaded.foreignValues[bits];
		}
		break;
	default: break;
	}
	Var zero;
	value::decodeVar(mem_std::Value(), type, zero);
	if (type == VarType::Enum) {
		zero.e.type = subtype;
	}
	return zero;
}

} // namespace gpu

template <typename Graph>
Status loadGpuBlock(const Graph &g, const GpuProgram &p, const GpuLoadSource &source,
		SpanView<value::EntityId> entities, GpuLoaded &out) {
	out = GpuLoaded();
	out.branches = uint32_t(entities.size());
	out.block.resize(p.blockLayout.size, 0);
	out.ins.resize(size_t(p.inLayout.size) * entities.size(), 0);
	out.blockValues.resize(p.blockValues.size());
	out.foreignValues.resize(p.foreigns.size());
	out.columns.resize(p.columns.size() * entities.size());
	out.preFailed.resize(entities.size(), 0);

	// The block.

	auto blk = out.block.data();
	gpu::putUInt(blk, gpu::findSlot(p.blockLayout, GpuSlot::Kind::Count), uint32_t(entities.size()));

	for (uint32_t k = 0; k < uint32_t(p.blockValues.size()); ++k) {
		auto &bv = p.blockValues[k];
		source.readBlockValue(bv.srcNode, bv.srcPin, out.blockValues[k]);
		if (isGpuValueType(bv.type)) {
			if (auto slot = gpu::findSlot(p.blockLayout, GpuSlot::Kind::BlockValue, k)) {
				writeGpuValue(blk + slot->offset, bv.type, out.blockValues[k]);
			}
		}
	}

	uint32_t foreignRows = 0;
	for (uint32_t k = 0; k < uint32_t(p.foreigns.size()); ++k) {
		auto &f = p.foreigns[k];
		Var entity;
		if (f.entity == GpuSource::Block) {
			entity = out.blockValues[f.entityIndex];
		} else if (f.entity == GpuSource::Literal) {
			gpu::literalVar(g, p.literals[f.entityIndex], entity);
		}
		auto id = value::EntityId::unpack(entity.ent.id);
		const bool has = entity.type == VarType::EntityRef && source.hasComponent(id, f.componentId);
		if (f.field.empty()) {
			out.foreignValues[k] = value::makeBool(has);
		} else if (has) {
			foreignRows |= uint32_t(1) << k;
			source.readField(id, f.componentId, f.field, out.foreignValues[k]);
		}
		if (isGpuValueType(f.type)) {
			if (auto slot = gpu::findSlot(p.blockLayout, GpuSlot::Kind::ForeignValue, k)) {
				writeGpuValue(blk + slot->offset, f.type, out.foreignValues[k]);
			}
		}
	}
	gpu::putUInt(blk, gpu::findSlot(p.blockLayout, GpuSlot::Kind::ForeignRowMask), foreignRows);

	uint32_t blockGuards = 0;
	for (uint32_t k = 0; k < uint32_t(p.guards.size()); ++k) {
		auto &gd = p.guards[k];
		if (gd.source == GpuGuard::Source::Column) {
			continue;
		}
		auto &value = gd.source == GpuGuard::Source::Block ? out.blockValues[gd.index] : out.foreignValues[gd.index];
		Var narrowed;
		if (value::castVar(value, gd.target, value::CastPolicy::Lossy, narrowed) != Status::Ok) {
			blockGuards |= uint32_t(1) << gd.bit;
			value::decodeVar(mem_std::Value(), gd.target, narrowed);
		}
		if (auto slot = gpu::findSlot(p.blockLayout, GpuSlot::Kind::BlockNarrowed, k)) {
			writeGpuValue(blk + slot->offset, gd.target, narrowed);
		}
	}
	gpu::putUInt(blk, gpu::findSlot(p.blockLayout, GpuSlot::Kind::BlockGuardMask), blockGuards);

	// The branches.

	for (uint32_t b = 0; b < uint32_t(entities.size()); ++b) {
		auto in = out.ins.data() + size_t(p.inLayout.size) * b;
		auto entity = entities[b];
		gpu::putInt(in, gpu::findSlot(p.inLayout, GpuSlot::Kind::Index), int32_t(b));

		uint32_t rows = 0;
		for (uint32_t c = 0; c < uint32_t(p.components.size()); ++c) {
			if (source.hasComponent(entity, p.components[c])) {
				rows |= uint32_t(1) << c;
			}
		}
		gpu::putUInt(in, gpu::findSlot(p.inLayout, GpuSlot::Kind::RowMask), rows);

		for (uint32_t k = 0; k < uint32_t(p.fields.size()); ++k) {
			auto &f = p.fields[k];
			Var v;
			value::decodeVar(mem_std::Value(), f.type, v);
			if ((rows & (uint32_t(1) << f.component)) != 0) {
				source.readField(entity, p.components[f.component], f.field, v);
			}
			if (auto slot = gpu::findSlot(p.inLayout, GpuSlot::Kind::In, k)) {
				writeGpuValue(in + slot->offset, f.type, v);
			}
		}
		for (uint32_t k = 0; k < uint32_t(p.columns.size()); ++k) {
			auto &col = p.columns[k];
			auto &v = out.columns[b * p.columns.size() + k];
			value::decodeVar(mem_std::Value(), col.type, v);
			if ((rows & (uint32_t(1) << col.component)) != 0) {
				source.readField(entity, p.components[col.component], col.field, v);
			}
		}

		uint32_t guards = 0;
		for (uint32_t k = 0; k < uint32_t(p.guards.size()); ++k) {
			auto &gd = p.guards[k];
			if (gd.source != GpuGuard::Source::Column) {
				continue;
			}
			Var narrowed;
			if (value::castVar(out.columns[b * p.columns.size() + gd.index], gd.target,
						value::CastPolicy::Lossy, narrowed)
					!= Status::Ok) {
				guards |= uint32_t(1) << gd.bit;
				value::decodeVar(mem_std::Value(), gd.target, narrowed);
			}
			if (auto slot = gpu::findSlot(p.inLayout, GpuSlot::Kind::Column, k)) {
				writeGpuValue(in + slot->offset, gd.target, narrowed);
			}
		}
		gpu::putUInt(in, gpu::findSlot(p.inLayout, GpuSlot::Kind::GuardMask), guards);

		// The prefix, in the order the machine meets it: the first refusal fails the branch before
		// dispatch, with the node, the status and the steps serial reports.
		for (auto &check : p.prefix) {
			bool refuses = false;
			switch (check.kind) {
			case GpuPrefixCheck::Kind::OwnRow: refuses = (rows & (uint32_t(1) << check.index)) == 0; break;
			case GpuPrefixCheck::Kind::ForeignRow: refuses = (foreignRows & (uint32_t(1) << check.index)) == 0; break;
			case GpuPrefixCheck::Kind::Guard: {
				auto &gd = p.guards[check.index];
				refuses = ((gd.source == GpuGuard::Source::Column ? guards : blockGuards) & (uint32_t(1) << gd.bit)) != 0;
				break;
			}
			case GpuPrefixCheck::Kind::Static:
			case GpuPrefixCheck::Kind::Budget: refuses = true; break;
			}
			if (refuses) {
				out.preFailed[b] = 1;
				gpu::putUInt(in, gpu::findSlot(p.inLayout, GpuSlot::Kind::PreFailed), 1);
				gpu::putUInt(in, gpu::findSlot(p.inLayout, GpuSlot::Kind::PreNode), check.node);
				gpu::putInt(in, gpu::findSlot(p.inLayout, GpuSlot::Kind::PreCode), toInt(check.status));
				gpu::putUInt(in, gpu::findSlot(p.inLayout, GpuSlot::Kind::PreSteps), check.steps);
				gpu::putUInt(in, gpu::findSlot(p.inLayout, GpuSlot::Kind::PreBudget),
						check.kind == GpuPrefixCheck::Kind::Budget ? 1 : 0);
				break;
			}
		}
	}
	return Status::Ok;
}

template <typename Graph>
bool decodeGpuBranch(const Graph &g, const GpuProgram &p, const GpuLoaded &loaded, BytesView outs,
		uint32_t branch, GpuBranchResult &res) {
	res = GpuBranchResult();
	if (size_t(p.outLayout.size) * (branch + 1) > outs.size()) {
		return false;
	}
	auto o = outs.data() + size_t(p.outLayout.size) * branch;
	auto &l = p.outLayout;
	res.failed = gpu::getUInt(o, gpu::findSlot(l, GpuSlot::Kind::Status)) != 0;
	res.budget = gpu::getUInt(o, gpu::findSlot(l, GpuSlot::Kind::Budget)) != 0;
	res.failNode = gpu::getUInt(o, gpu::findSlot(l, GpuSlot::Kind::FailNode));
	res.failStatus = Status(int32_t(gpu::getUInt(o, gpu::findSlot(l, GpuSlot::Kind::FailCode))));
	res.steps = gpu::getUInt(o, gpu::findSlot(l, GpuSlot::Kind::Steps));

	auto &block = g.getBlockAt(p.block);
	auto copyMask = gpu::findSlot(l, GpuSlot::Kind::CopyMask);
	res.written.resize(block.writeCount);
	res.copies.resize(block.writeCount);
	for (uint32_t w = 0; w < block.writeCount; ++w) {
		auto &write = g.getBlockWrites(p.block)[w];
		res.written[w] = (gpu::getUInt(o, copyMask, w / 32) >> (w % 32)) & 1;
		auto value = gpu::findSlot(l, GpuSlot::Kind::Copy, w);
		if (isGpuValueType(write.type)) {
			res.copies[w] = readGpuValue(o + value->offset, write.type);
		} else {
			auto tag = gpu::getUInt(o, gpu::findSlot(l, GpuSlot::Kind::CopyTag, w));
			res.copies[w] = gpu::widen(g, p, loaded, branch, tag, gpu::getUInt(o, value), write.type,
					write.subtypeId);
		}
		if (!res.written[w]) {
			Var zero;
			value::decodeVar(mem_std::Value(), write.type, zero);
			res.copies[w] = zero;
		}
	}

	res.cells.resize(p.cells.size());
	for (uint32_t c = 0; c < uint32_t(p.cells.size()); ++c) {
		auto &cell = p.cells[c];
		auto value = gpu::findSlot(l, GpuSlot::Kind::Cell, c);
		if (cell.rep == GpuRep::Direct) {
			res.cells[c] = readGpuValue(o + value->offset, cell.type);
		} else {
			auto tag = gpu::getUInt(o, gpu::findSlot(l, GpuSlot::Kind::CellTag, c));
			res.cells[c] = gpu::widen(g, p, loaded, branch, tag, gpu::getUInt(o, value), cell.type,
					cell.subtypeId);
		}
	}

	auto count = gpu::getUInt(o, gpu::findSlot(l, GpuSlot::Kind::DecisionCount));
	auto words = gpu::findSlot(l, GpuSlot::Kind::Decisions);
	mem_std::Vector<uint32_t> decisions;
	for (uint32_t k = 0; k < count; ++k) {
		decisions.emplace_back((gpu::getUInt(o, words, k / 32) >> (k % 32)) & 1);
	}
	return replayGpuPath(g, p, SpanView<uint32_t>(decisions.data(), decisions.size()), res.failed,
			res.budget, res.failNode, res.failStatus, res.steps, res.path);
}

template <typename Graph, typename Local>
Status writeGpuBranch(const Graph &g, Local &local, const GpuProgram &p, uint32_t activation,
		const GpuBranchResult &r) {
	auto arena = local.getArena();
	auto frame = local.activationFrame(activation);
	if (!arena || frame == NullAddr) {
		return Status::ErrorInvalidArguemnt;
	}
	auto &block = g.getBlockAt(p.block);
	auto status = r.failed ? (BranchHeader::Failed | BranchHeader::Closed) : BranchHeader::Closed;
	writeBranchInt(*arena, frame, BranchHeader::StatusOffset, status);
	writeBranchInt(*arena, frame, BranchHeader::StepsOffset, int32_t(r.steps));

	for (uint32_t w = 0; w < block.writeCount && w < r.written.size(); ++w) {
		auto &write = g.getBlockWrites(p.block)[w];
		if (r.written[w]) {
			auto st = writeRecordField(*arena, frame, shapeOfCopy(write), r.copies[w]);
			if (st != Status::Ok) {
				return st;
			}
		}
		writeBranchFlag(*arena, frame, write.flagOffset, r.written[w] != 0);
	}

	auto stateType = local.getStateType();
	auto flagsField = stateType ? stateType->getField(StringView("flags")) : nullptr;
	auto inputsField = stateType ? stateType->getField(StringView("inputs")) : nullptr;
	auto producedField = stateType ? stateType->getField(StringView("produced")) : nullptr;
	if (!flagsField || !inputsField || !producedField) {
		return Status::ErrorInvalidArguemnt;
	}
	for (uint32_t i = 0; i < uint32_t(p.bodyNodes.size()) && i < r.path.nodes.size(); ++i) {
		auto n = p.bodyNodes[i];
		auto stateAddr = local.getStateIn(frame, n);
		auto &state = r.path.nodes[i];
		auto flags = int64_t(state.flags) | (state.stalled ? NodeFlags::StallData : 0);
		stateType->setField(*arena, stateAddr, *flagsField, value::makeInt(flags));
		stateType->setField(*arena, stateAddr, *inputsField, value::makeInt(int64_t(state.inputs)));
		stateType->setField(*arena, stateAddr, *producedField,
				value::makeInt(int64_t(state.produced)));

		auto &rt = g.getNodeAt(n);
		auto &node = p.nodes[i];
		auto record = local.getRecordIn(frame, n);
		if (!rt.localSchema || record == NullAddr) {
			continue;
		}
		// Only a node that ran wrote its record: a branch that failed on its prefix, or before this
		// node's turn, leaves the record as the frame was opened - which is what serial leaves.
		if ((state.flags & uint32_t(NodeFlags::Ran)) == 0) {
			continue;
		}
		auto fields = rt.localSchema->getFields();
		for (uint32_t c = 0; c < node.cellCount && c < fields.size(); ++c) {
			rt.localSchema->setField(*arena, record, fields[c], r.cells[node.cellBegin + c]);
		}
	}
	return Status::Ok;
}

} // namespace stappler::flow

#endif /* STAPPLER_FLOW_SPFLOWGPULOAD_HPP_ */
