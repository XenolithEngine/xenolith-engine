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

// What the GPU lowering needs that is not a template: the std430 rules, the tolerance of level C,
// the CPU's statement of the reducers' tree order. The lowering itself is SPFlowGpu.hpp.

#include "SPFlowGpu.hpp"
#include "SPFlowGpuLoad.hpp"

namespace STAPPLER_VERSIONIZED stappler::flow {

bool isGpuValueType(VarType t) {
	switch (t) {
	case VarType::Bool:
	case VarType::Int32:
	case VarType::UInt32:
	case VarType::Float32:
	case VarType::Vec2:
	case VarType::Vec3:
	case VarType::Vec4:
	case VarType::Color: return true;
	default: return false;
	}
}

StringView getGpuGlslType(VarType t) {
	switch (t) {
	case VarType::Bool: return StringView("bool");
	case VarType::Int32: return StringView("int");
	case VarType::UInt32: return StringView("uint");
	case VarType::Float32: return StringView("float");
	case VarType::Vec2: return StringView("vec2");
	case VarType::Vec3: return StringView("vec3");
	case VarType::Vec4:
	case VarType::Color: return StringView("vec4");
	default: return StringView("uint");
	}
}

uint32_t getGpuTypeSize(VarType t) {
	switch (t) {
	case VarType::Vec2: return 8;
	case VarType::Vec3: return 12;
	case VarType::Vec4:
	case VarType::Color: return 16;
	default: return 4; // every scalar, a Bool as a uint, a plain uint
	}
}

uint32_t getGpuTypeAlign(VarType t) {
	switch (t) {
	case VarType::Vec2: return 8;
	case VarType::Vec3:
	case VarType::Vec4:
	case VarType::Color: return 16;
	default: return 4;
	}
}

// Wider alignment first, declaration order among equals: a vec3 then leaves its tail to a scalar,
// and the order never depends on anything but the program.
void layoutStd430(GpuLayout &l) {
	mem_std::Vector<uint32_t> order(l.slots.size());
	for (uint32_t k = 0; k < uint32_t(order.size()); ++k) {
		order[k] = k;
		auto &s = l.slots[k];
		const bool array = s.count > 1 || s.kind == GpuSlot::Kind::Decisions
				|| s.kind == GpuSlot::Kind::CopyMask;
		s.align = getGpuTypeAlign(s.type);
		s.size = array ? 4 * s.count : getGpuTypeSize(s.type);
	}
	sprt::stable_sort(order.begin(), order.end(),
			[&](uint32_t a, uint32_t b) { return l.slots[a].align > l.slots[b].align; });
	uint32_t cursor = 0;
	uint32_t align = 4;
	for (auto k : order) {
		auto &s = l.slots[k];
		cursor = value::alignUp(cursor, s.align);
		s.offset = cursor;
		cursor += s.size;
		align = sprt::max(align, s.align);
	}
	l.align = align;
	l.size = value::alignUp(cursor, align);
}

const GpuNode *GpuProgram::findNode(uint32_t graphIndex) const {
	for (auto &n : nodes) {
		if (n.node == graphIndex) {
			return &n;
		}
	}
	return nullptr;
}

GpuTolerance getGpuTolerance(VarType t) {
	switch (t) {
	case VarType::Float:
	case VarType::Float32:
	case VarType::Vec2:
	case VarType::Vec3:
	case VarType::Vec4:
	case VarType::Color: return GpuTolerance{1e-6, 1e-5, false};
	default: return GpuTolerance{0.0, 0.0, true};
	}
}

static bool nearlyReal(double a, double b, const GpuTolerance &tol) {
	if (a != a || b != b) {
		return (a != a) && (b != b);
	}
	if (a == b) {
		return true;
	}
	if ((a - a) != (a - a) || (b - b) != (b - b)) {
		return false; // an infinity equals only itself, which the test above already asked
	}
	auto diff = sprt::abs(a - b);
	auto scale = sprt::max(sprt::abs(a), sprt::abs(b));
	return diff <= sprt::max(tol.abs, tol.rel * scale);
}

bool nearlyEqual(const Var &cpu, const Var &gpu) {
	if (cpu.type != gpu.type) {
		return false;
	}
	auto tol = getGpuTolerance(cpu.type);
	if (tol.exact) {
		return value::varBytesEqual(cpu, gpu);
	}
	switch (cpu.type) {
	case VarType::Float: return nearlyReal(cpu.f, gpu.f, tol);
	case VarType::Float32: return nearlyReal(cpu.v[0], gpu.v[0], tol);
	case VarType::Vec2:
	case VarType::Vec3:
	case VarType::Vec4:
	case VarType::Color: {
		uint32_t n = cpu.type == VarType::Vec2 ? 2 : (cpu.type == VarType::Vec3 ? 3 : 4);
		for (uint32_t k = 0; k < n; ++k) {
			if (!nearlyReal(cpu.v[k], gpu.v[k], tol)) {
				return false;
			}
		}
		return true;
	}
	default: return value::varBytesEqual(cpu, gpu);
	}
}

// One step of a reducer on two present values, the left one earlier in branch order.
static Var combine(StringView kind, VarType type, const Var &l, const Var &r) {
	auto n = type == VarType::Vec2 ? 2 : (type == VarType::Vec3 ? 3 : 4);
	if (kind == StringView("sum")) {
		switch (type) {
		case VarType::Int32: return value::makeInt32(int32_t(uint32_t(l.i) + uint32_t(r.i)));
		case VarType::UInt32: return value::makeUInt32(uint32_t(l.i) + uint32_t(r.i));
		case VarType::Float32: return value::makeFloat32(l.v[0] + r.v[0]);
		case VarType::Vec2:
		case VarType::Vec3:
		case VarType::Vec4: {
			Var out = l;
			for (int k = 0; k < n; ++k) {
				out.v[k] = l.v[k] + r.v[k];
			}
			return out;
		}
		default: return l;
		}
	}
	if (kind == StringView("count")) {
		return value::makeInt32(int32_t(uint32_t(l.i) + uint32_t(r.i)));
	}
	if (kind == StringView("any")) {
		return value::makeBool(l.i != 0 || r.i != 0);
	}
	if (kind == StringView("all")) {
		return value::makeBool(l.i != 0 && r.i != 0);
	}
	auto less = [&](const Var &a, const Var &b) {
		switch (type) {
		case VarType::Int32: return int32_t(a.i) < int32_t(b.i);
		case VarType::UInt32: return uint32_t(a.i) < uint32_t(b.i);
		default: return a.v[0] < b.v[0];
		}
	};
	if (kind == StringView("min")) {
		return less(r, l) ? r : l;
	}
	if (kind == StringView("max")) {
		return less(l, r) ? r : l;
	}
	return l;
}

bool treeFoldReference(StringView kind, VarType type, SpanView<Var> values, SpanView<uint8_t> present,
		Var &out) {
	if (values.size() != present.size()) {
		return false;
	}
	auto count = uint32_t(values.size());
	mem_std::Vector<Var> v(values.begin(), values.end());
	mem_std::Vector<uint8_t> has(present.begin(), present.end());
	// `count` folds the ones, not the values.
	if (kind == StringView("count")) {
		for (uint32_t i = 0; i < count; ++i) { v[i] = value::makeInt32(v[i].i != 0 ? 1 : 0); }
	}
	for (uint32_t stride = 1; stride < count; stride *= 2) {
		for (uint32_t i = 0; i + stride < count; i += 2 * stride) {
			auto j = i + stride;
			if (has[i] && has[j]) {
				v[i] = combine(kind, type, v[i], v[j]);
			} else if (has[j]) {
				v[i] = v[j];
				has[i] = 1;
			}
		}
	}
	if (count > 0 && has[0]) {
		out = v[0];
		return true;
	}
	// No branch at all: the zero of the result, as parFold gives it.
	if (kind == StringView("count")) {
		out = value::makeInt32(0);
	} else if (kind == StringView("any")) {
		out = value::makeBool(false);
	} else if (kind == StringView("all")) {
		out = value::makeBool(true);
	} else {
		value::decodeVar(mem_std::Value(), type, out);
	}
	return true;
}

void writeGpuValue(uint8_t *at, VarType type, const Var &v) {
	switch (type) {
	case VarType::Bool: {
		uint32_t b = v.i != 0 ? 1 : 0;
		sprt::memcpy(at, &b, 4);
		break;
	}
	case VarType::Int32: {
		auto i = int32_t(v.i);
		sprt::memcpy(at, &i, 4);
		break;
	}
	case VarType::UInt32: {
		auto u = uint32_t(v.i);
		sprt::memcpy(at, &u, 4);
		break;
	}
	case VarType::Float32: sprt::memcpy(at, &v.v[0], 4); break;
	case VarType::Vec2: sprt::memcpy(at, v.v, 8); break;
	case VarType::Vec3: sprt::memcpy(at, v.v, 12); break;
	case VarType::Vec4:
	case VarType::Color: sprt::memcpy(at, v.v, 16); break;
	default: break;
	}
}

Var readGpuValue(const uint8_t *at, VarType type) {
	switch (type) {
	case VarType::Bool: {
		uint32_t b = 0;
		sprt::memcpy(&b, at, 4);
		return value::makeBool(b != 0);
	}
	case VarType::Int32: {
		int32_t i = 0;
		sprt::memcpy(&i, at, 4);
		return value::makeInt32(i);
	}
	case VarType::UInt32: {
		uint32_t u = 0;
		sprt::memcpy(&u, at, 4);
		return value::makeUInt32(u);
	}
	case VarType::Float32: {
		float f = 0.0f;
		sprt::memcpy(&f, at, 4);
		return value::makeFloat32(f);
	}
	case VarType::Vec2: {
		float f[2];
		sprt::memcpy(f, at, 8);
		return value::makeVec2(f[0], f[1]);
	}
	case VarType::Vec3: {
		float f[3];
		sprt::memcpy(f, at, 12);
		return value::makeVec3(f[0], f[1], f[2]);
	}
	case VarType::Vec4: {
		float f[4];
		sprt::memcpy(f, at, 16);
		return value::makeVec4(f[0], f[1], f[2], f[3]);
	}
	case VarType::Color: {
		float f[4];
		sprt::memcpy(f, at, 16);
		return value::makeColor(f[0], f[1], f[2], f[3]);
	}
	default: return Var();
	}
}

void RuntimeGraph::checkGpuLowering(DiagReport &report) {
	for (uint32_t b = 0; b < getBlockCount(); ++b) {
		if (isGpuBlock(*this, b)) {
			// The author demanded the GPU: a body that does not lower is an error with a name.
			GpuProgram program;
			lowerGpuBlock(*this, b, program, &report);
		} else if (isGpuCandidate(*this, b)) {
			// The author left it open: the body is lowered so the heuristic has something to
			// choose, and a refusal is not reported - the heuristic silently excludes the GPU.
			GpuProgram program;
			lowerGpuBlock(*this, b, program, nullptr);
		}
	}
}

SpanView<Pair<StringView, VarType>> getGpuReducers() {
	static const Pair<StringView, VarType> reducers[] = {
		{StringView("sum"), VarType::Int32},
		{StringView("sum"), VarType::UInt32},
		{StringView("sum"), VarType::Float32},
		{StringView("sum"), VarType::Vec2},
		{StringView("sum"), VarType::Vec3},
		{StringView("sum"), VarType::Vec4},
		{StringView("min"), VarType::Int32},
		{StringView("min"), VarType::UInt32},
		{StringView("min"), VarType::Float32},
		{StringView("max"), VarType::Int32},
		{StringView("max"), VarType::UInt32},
		{StringView("max"), VarType::Float32},
		{StringView("count"), VarType::Bool},
		{StringView("any"), VarType::Bool},
		{StringView("all"), VarType::Bool},
	};
	return SpanView<Pair<StringView, VarType>>(reducers, sizeof(reducers) / sizeof(reducers[0]));
}

static StringView reduceTypeTag(VarType t) {
	switch (t) {
	case VarType::Bool: return StringView("b");
	case VarType::Int32: return StringView("i32");
	case VarType::UInt32: return StringView("u32");
	case VarType::Float32: return StringView("f32");
	case VarType::Vec2: return StringView("v2");
	case VarType::Vec3: return StringView("v3");
	case VarType::Vec4: return StringView("v4");
	default: return StringView("x");
	}
}

mem_std::String getGpuReduceName(StringView kind, VarType type) {
	return mem_std::toString("par_reduce_", kind, "_", reduceTypeTag(type));
}

mem_std::String writeGpuReduceGlsl(StringView kind, VarType type) {
	// A Bool is a uint in a buffer, and `count` folds those as numbers.
	auto element = type == VarType::Bool ? StringView("uint") : getGpuGlslType(type);
	mem_std::String combine;
	if (kind == StringView("sum")) {
		combine = type == VarType::Int32 ? mem_std::String("int(uint(a) + uint(b))") : mem_std::String("a + b");
	} else if (kind == StringView("count")) {
		combine = "a + b";
	} else if (kind == StringView("any")) {
		combine = "a | b";
	} else if (kind == StringView("all")) {
		combine = "a & b";
	} else if (kind == StringView("min")) {
		combine = "(b < a) ? b : a";
	} else if (kind == StringView("max")) {
		combine = "(a < b) ? b : a";
	}
	const bool real = type == VarType::Float32 || type == VarType::Vec2 || type == VarType::Vec3
			|| type == VarType::Vec4;
	mem_std::String out;
	auto line = [&](StringView text) {
		out.append(text.data(), text.size());
		out.push_back('\n');
	};
	line("#version 450");
	line(mem_std::toString("// The ", kind, " of ", value::getVarTypeName(type),
			" branch values, one pass of a tree. Generated: do not edit."));
	line("//");
	line("// Pass `stride` = 1, 2, 4...: values[i] takes op(values[i], values[i + stride]) for every i that is a");
	line("// multiple of 2 * stride; an absent side takes the other. The earlier branch stays on the left, so a");
	line("// tie keeps it, as the CPU's fold in branch order does. For `count` the loader writes each");
	line("// branch's Bool as 0 or 1.");
	line("//");
	line("// The pass is a push constant, so every stride of every reducer of a block is one frame over one");
	line("// pair of buffers: `valueBase` and `presentBase` say where this reducer's run begins.");
	line("");
	line("layout (local_size_x = 256) in;");
	line("");
	line("layout (push_constant) uniform Params {");
	line("\tuint count;");
	line("\tuint stride;");
	line("\tuint valueBase;");
	line("\tuint presentBase;");
	line("} prm;");
	line("");
	line(mem_std::toString("layout (std430, set = 0, binding = 0) buffer Values { ", element, " values[]; };"));
	line("layout (std430, set = 0, binding = 1) buffer Present { uint present[]; };");
	line("");
	line("void main() {");
	line("\tuint k = gl_GlobalInvocationID.x * (2u * prm.stride);");
	line("\tuint l = k + prm.stride;");
	line("\tif (l >= prm.count || present[prm.presentBase + l] == 0u) {");
	line("\t\treturn;");
	line("\t}");
	line("\tuint i = prm.valueBase + k;");
	line("\tuint j = prm.valueBase + l;");
	line("\tif (present[prm.presentBase + k] == 0u) {");
	line("\t\tvalues[i] = values[j];");
	line("\t\tpresent[prm.presentBase + k] = 1u;");
	line("\t\treturn;");
	line("\t}");
	line(mem_std::toString("\t", real ? "precise " : "", element, " a = values[i];"));
	line(mem_std::toString("\t", element, " b = values[j];"));
	line(mem_std::toString("\tvalues[i] = ", combine, ";"));
	line("}");
	return out;
}

template Status lowerGpuBlock(const RuntimeGraph &, uint32_t, GpuProgram &, DiagReport *,
		const GpuLowerLimits &);
template mem_std::String writeGpuGlsl(const RuntimeGraph &, const GpuProgram &);
template bool replayGpuPath(const RuntimeGraph &, const GpuProgram &, SpanView<uint32_t>, bool, bool,
		uint32_t, Status, uint32_t, GpuPath &);
template Status loadGpuBlock(const RuntimeGraph &, const GpuProgram &, const GpuLoadSource &,
		SpanView<value::EntityId>, GpuLoaded &);
template bool decodeGpuBranch(const RuntimeGraph &, const GpuProgram &, const GpuLoaded &, BytesView,
		uint32_t, GpuBranchResult &);
template void collectGpuReducers(const RuntimeGraph &, const GpuProgram &, mem_std::Vector<GpuReducer> &);
template bool isGpuBlock(const RuntimeGraph &, uint32_t);
template bool isGpuCandidate(const RuntimeGraph &, uint32_t);
template uint64_t hashGpuBlock(const RuntimeGraph &, uint32_t, mem_std::String *);

const GpuBlockShaders *GpuShaderTable::get(uint32_t block) const {
	for (auto &it : _blocks) {
		if (it.block == block) {
			return &it;
		}
	}
	return nullptr;
}

void GpuShaderTable::clear() {
	_blocks.clear();
	_identity = 0;
}

mem_std::String getGpuConformanceName(const OpDesc &op) {
	return mem_std::toString("conf_", op.getShaderName());
}

// One case per invocation: In carries the operation's inputs, Out its outputs and the status the
// body returned. The three bindings are a block shader's, so a device runs this through the same
// path.
mem_std::String writeGpuConformanceGlsl(const OpDesc &op, GpuLayout &in, GpuLayout &out) {
	in = GpuLayout();
	out = GpuLayout();
	auto ins = op.getDataIn();
	auto outs = op.getDataOut();
	for (uint32_t i = 0; i < uint32_t(ins.size()); ++i) {
		GpuSlot slot;
		slot.kind = GpuSlot::Kind::In;
		slot.index = i;
		slot.name = mem_std::toString("a", i);
		slot.type = ins[i].type;
		in.slots.emplace_back(sprt::move(slot));
	}
	layoutStd430(in);
	for (uint32_t i = 0; i < uint32_t(outs.size()); ++i) {
		GpuSlot slot;
		slot.kind = GpuSlot::Kind::Cell;
		slot.index = i;
		slot.name = mem_std::toString("r", i);
		slot.type = outs[i].type;
		out.slots.emplace_back(sprt::move(slot));
	}
	{
		GpuSlot slot;
		slot.kind = GpuSlot::Kind::Status;
		slot.name = mem_std::String("status");
		slot.type = VarType::Int32;
		out.slots.emplace_back(sprt::move(slot));
	}
	layoutStd430(out);

	mem_std::String text;
	auto line = [&](StringView what) {
		text.append(what.data(), what.size());
		text.push_back('\n');
	};
	auto member = [&](const GpuLayout &l, StringView what) {
		// In declaration order of the layout, so the text is the offsets.
		mem_std::Vector<uint32_t> order(l.slots.size());
		for (uint32_t k = 0; k < uint32_t(order.size()); ++k) {
			order[k] = k;
		}
		sprt::stable_sort(order.begin(), order.end(),
				[&](uint32_t a, uint32_t b) { return l.slots[a].offset < l.slots[b].offset; });
		line(mem_std::toString("struct ", what, " {"));
		for (auto k : order) {
			auto &slot = l.slots[k];
			auto type = slot.type == VarType::Bool ? StringView("uint") : getGpuGlslType(slot.type);
			line(mem_std::toString("\t", type, " ", slot.name, ";"));
		}
		line("};");
		line("");
	};

	line("#version 450");
	line(mem_std::toString("// The body of ", op.getName(),
			" on a device, against the C++ body of the same operation."));
	line("// Generated: do not edit.");
	line("");
	line("layout (local_size_x = 64) in;");
	line("");
	member(in, StringView("In"));
	member(out, StringView("Out"));
	line("layout (std430, set = 0, binding = 0) readonly buffer Block { uint count; } blk;");
	line("layout (std430, set = 0, binding = 1) readonly buffer Ins { In ins[]; };");
	line("layout (std430, set = 0, binding = 2) buffer Outs { Out outs[]; };");
	line("");
	text.append(op.getShaderSource().data(), op.getShaderSource().size());
	line("");
	line("void main() {");
	line("\tuint i = gl_GlobalInvocationID.x;");
	line("\tif (i >= blk.count) {");
	line("\t\treturn;");
	line("\t}");
	for (uint32_t k = 0; k < uint32_t(outs.size()); ++k) {
		auto type = outs[k].type == VarType::Bool ? StringView("bool") : getGpuGlslType(outs[k].type);
		line(mem_std::toString("\t", type, " o", k, " = ", type, "(0);"));
	}
	mem_std::String args;
	for (uint32_t k = 0; k < uint32_t(ins.size()); ++k) {
		if (!args.empty()) {
			args.append(", ");
		}
		auto text2 = ins[k].type == VarType::Bool
				? mem_std::toString("(ins[i].a", k, " != 0u)")
				: mem_std::toString("ins[i].a", k);
		args.append(text2.data(), text2.size());
	}
	for (uint32_t k = 0; k < uint32_t(outs.size()); ++k) {
		if (!args.empty()) {
			args.append(", ");
		}
		auto text2 = mem_std::toString("o", k);
		args.append(text2.data(), text2.size());
	}
	line(mem_std::toString("\touts[i].status = ", op.getShaderName(), "(", args, ");"));
	for (uint32_t k = 0; k < uint32_t(outs.size()); ++k) {
		auto value = outs[k].type == VarType::Bool ? mem_std::toString("uint(o", k, ")")
												  : mem_std::toString("o", k);
		line(mem_std::toString("\touts[i].r", k, " = ", value, ";"));
	}
	line("}");
	return text;
}

// Both graphs own their table and answer the same two questions: the machine's launch asks them
// through the graph policy.
uint32_t RuntimeGraph::attachGpuShaders(SpanView<GpuShaderBinary> shaders) {
	if (!_gpuShaders) {
		_gpuShaders = new GpuShaderTable();
	}
	return _gpuShaders->attach(*this, shaders);
}

const GpuBlockShaders *RuntimeGraph::getGpuBlock(uint32_t block) const {
	return _gpuShaders ? _gpuShaders->get(block) : nullptr;
}

uint32_t CompiledGraph::attachGpuShaders(SpanView<GpuShaderBinary> shaders) {
	if (!_gpuShaders) {
		_gpuShaders = new GpuShaderTable();
	}
	return _gpuShaders->attach(*this, shaders);
}

const GpuBlockShaders *CompiledGraph::getGpuBlock(uint32_t block) const {
	return _gpuShaders ? _gpuShaders->get(block) : nullptr;
}

template uint32_t GpuShaderTable::attach(const RuntimeGraph &, SpanView<GpuShaderBinary>);
template uint32_t GpuShaderTable::attach(const CompiledGraph &, SpanView<GpuShaderBinary>);
template void collectGpuReducers(const CompiledGraph &, const GpuProgram &, mem_std::Vector<GpuReducer> &);

template Status lowerGpuBlock(const CompiledGraph &, uint32_t, GpuProgram &, DiagReport *,
		const GpuLowerLimits &);
template mem_std::String writeGpuGlsl(const CompiledGraph &, const GpuProgram &);
template bool isGpuBlock(const CompiledGraph &, uint32_t);
template bool isGpuCandidate(const CompiledGraph &, uint32_t);
template uint64_t hashGpuBlock(const CompiledGraph &, uint32_t, mem_std::String *);
template bool replayGpuPath(const CompiledGraph &, const GpuProgram &, SpanView<uint32_t>, bool, bool,
		uint32_t, Status, uint32_t, GpuPath &);
template Status loadGpuBlock(const CompiledGraph &, const GpuProgram &, const GpuLoadSource &,
		SpanView<value::EntityId>, GpuLoaded &);
template bool decodeGpuBranch(const CompiledGraph &, const GpuProgram &, const GpuLoaded &, BytesView,
		uint32_t, GpuBranchResult &);

} // namespace stappler::flow
