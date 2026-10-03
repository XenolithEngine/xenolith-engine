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

#ifndef TESTS_FLOW_GRAPH_GRAPH_FIXTURE_H_
#define TESTS_FLOW_GRAPH_GRAPH_FIXTURE_H_

#include "SPCommon.h"
#include "SPMemory.h"
#include "SPData.h"

#include "SPFlowRuntime.h"

#include "../check/flow_check.h"

// The operation set the validation and build sections are written against. Shared by both because a
// diagnostic and the graph it refuses have to be about the same signatures - and because the set is
// chosen to reach every rule exactly once: a Widen edge, a Narrow one, an element mismatch, an enum
// family, a container transfer, a loop.
//
// None of these operations does anything. The build never calls one, and the day it needs to in order
// to be tested is the day it has grown a responsibility it should not have.
namespace STAPPLER_VERSIONIZED stappler::test::graphfx {

using namespace stappler::flow;
using stappler::flow::value::VarType;
using stappler::flow::value::makeTypeId;

inline Status noopInvoke(OpContext &) { return Status::Ok; }

inline PinDesc pin(StringView name, VarType type, ElementChain element = 0,
		flow::value::TypeId subtype = flow::value::NullTypeId) {
	PinDesc p;
	p.name = name;
	p.type = type;
	p.element = element;
	p.subtypeId = subtype;
	return p;
}

inline PinDesc pinRequired(StringView name, VarType type) {
	auto p = pin(name, type);
	p.flags = PinFlags::Required;
	return p;
}

inline PinDesc pinDefault(StringView name, VarType type, mem_std::Value &&def) {
	auto p = pin(name, type);
	p.def = sprt::move(def);
	return p;
}

struct OpBuilder {
	OpRegistry &reg;
	bool ok = true;

	void add(StringView name, SpanView<PinDesc> in, SpanView<PinDesc> out, bool execIn,
			SpanView<StringView> execOut, uint32_t scopeExecOut = 0) {
		OpDef def;
		def.name = name;
		def.dataIn = in;
		def.dataOut = out;
		def.hasExecIn = execIn;
		def.execOut = execOut;
		def.scopeExecOut = scopeExecOut;
		def.invoke = &noopInvoke;
		if (!reg.createNative(def)) {
			ok = false;
		}
	}
};

// Two enum families, so "same tag, different family" is reachable.
inline flow::value::TypeId familyA() { return makeTypeId(StringView("enum.Direction")); }
inline flow::value::TypeId familyB() { return makeTypeId(StringView("enum.Colour")); }

inline bool buildFixtureOps(OpRegistry &reg) {
	OpBuilder b{reg};

	const StringView then[] = {StringView("then")};
	const StringView branches[] = {StringView("true"), StringView("false")};
	const StringView loop[] = {StringView("body"), StringView("done")};

	// An entry point: nothing comes in, execution starts here.
	{
		PinDesc out[] = {pin("value", VarType::Float)};
		b.add(StringView("flow.start"), SpanView<PinDesc>(), SpanView<PinDesc>(out, 1), false,
				SpanView<StringView>(then, 1));
	}

	// Float arithmetic, with a default on the second input so "unconnected but fine" is reachable.
	{
		PinDesc in[] = {pin("lhs", VarType::Float),
			pinDefault("rhs", VarType::Float, mem_std::Value(1.0))};
		PinDesc out[] = {pin("result", VarType::Float)};
		b.add(StringView("math.addf"), SpanView<PinDesc>(in, 2), SpanView<PinDesc>(out, 1), false,
				SpanView<StringView>());
	}

	// Integer arithmetic: its output into a Float input is the Narrow cell, and therefore refused.
	{
		PinDesc in[] = {pin("lhs", VarType::Int), pin("rhs", VarType::Int)};
		PinDesc out[] = {pin("result", VarType::Int)};
		b.add(StringView("math.addi"), SpanView<PinDesc>(in, 2), SpanView<PinDesc>(out, 1), false,
				SpanView<StringView>());
	}

	// A required input with no default: the "missing input" case.
	{
		PinDesc in[] = {pinRequired("value", VarType::Float)};
		b.add(StringView("sink.print"), SpanView<PinDesc>(in, 1), SpanView<PinDesc>(), true,
				SpanView<StringView>());
	}

	// The same, taking an Int: needed wherever a consumer has to be told to run AND has to accept a
	// value from integer arithmetic, since Int reaching a Float input is the Narrow cell.
	{
		PinDesc in[] = {pin("n", VarType::Int)};
		b.add(StringView("sink.count"), SpanView<PinDesc>(in, 1), SpanView<PinDesc>(), true,
				SpanView<StringView>());
	}

	// Branching and sequencing.
	{
		PinDesc in[] = {pinRequired("condition", VarType::Bool)};
		b.add(StringView("flow.branch"), SpanView<PinDesc>(in, 1), SpanView<PinDesc>(), true,
				SpanView<StringView>(branches, 2));
	}
	{
		PinDesc in[] = {pin("count", VarType::Int)};
		b.add(StringView("flow.repeat"), SpanView<PinDesc>(in, 1), SpanView<PinDesc>(), true,
				SpanView<StringView>(loop, 2));
	}
	{
		b.add(StringView("flow.tick"), SpanView<PinDesc>(), SpanView<PinDesc>(), true,
				SpanView<StringView>(then, 1));
	}

	// The one operation here whose two exec outputs do not mean the same kind of thing: `body` opens
	// a SCOPE - everything it reaches is a loop body, re-run per iteration - and `done` continues
	// after the loop. `item` is handed out once per turn, which is why anything computed from it
	// belongs to the body as well.
	{
		PinDesc in[] = {pin("items", VarType::Array, flow::value::makeChain(VarType::Int))};
		PinDesc out[] = {pin("item", VarType::Int)};
		b.add(StringView("flow.each"), SpanView<PinDesc>(in, 1), SpanView<PinDesc>(out, 1), true,
				SpanView<StringView>(loop, 2), 1u << 0);
	}

	// Widening: Bool reaches Int and Vec2 reaches Vec3, both without losing anything.
	{
		PinDesc out[] = {pin("flag", VarType::Bool)};
		b.add(StringView("bool.out"), SpanView<PinDesc>(), SpanView<PinDesc>(out, 1), false,
				SpanView<StringView>());
	}
	{
		PinDesc in[] = {pin("n", VarType::Int)};
		b.add(StringView("int.in"), SpanView<PinDesc>(in, 1), SpanView<PinDesc>(), false,
				SpanView<StringView>());
	}
	{
		PinDesc out[] = {pin("v", VarType::Vec2)};
		b.add(StringView("vec.out2"), SpanView<PinDesc>(), SpanView<PinDesc>(out, 1), false,
				SpanView<StringView>());
	}
	{
		PinDesc in[] = {pin("v", VarType::Vec3)};
		b.add(StringView("vec.in3"), SpanView<PinDesc>(in, 1), SpanView<PinDesc>(), false,
				SpanView<StringView>());
	}

	// Containers: a String reaching a Bytes input is Same and still a deep copy.
	{
		PinDesc out[] = {pin("text", VarType::String)};
		b.add(StringView("str.out"), SpanView<PinDesc>(), SpanView<PinDesc>(out, 1), false,
				SpanView<StringView>());
	}
	{
		PinDesc in[] = {pin("blob", VarType::Bytes)};
		b.add(StringView("bytes.in"), SpanView<PinDesc>(in, 1), SpanView<PinDesc>(), false,
				SpanView<StringView>());
	}
	{
		PinDesc out[] = {pin("items", VarType::Array, flow::value::makeChain(VarType::Int))};
		b.add(StringView("arr.ints"), SpanView<PinDesc>(), SpanView<PinDesc>(out, 1), false,
				SpanView<StringView>());
	}
	{
		PinDesc in[] = {pin("items", VarType::Array, flow::value::makeChain(VarType::Float))};
		b.add(StringView("arr.floats"), SpanView<PinDesc>(in, 1), SpanView<PinDesc>(), false,
				SpanView<StringView>());
	}
	{
		PinDesc in[] = {pin("items", VarType::Array, flow::value::makeChain(VarType::Int))};
		b.add(StringView("arr.intsIn"), SpanView<PinDesc>(in, 1), SpanView<PinDesc>(), false,
				SpanView<StringView>());
	}

	// Enum families: the same tag, and not the same type.
	{
		PinDesc out[] = {pin("e", VarType::Enum, 0, familyA())};
		b.add(StringView("enum.outA"), SpanView<PinDesc>(), SpanView<PinDesc>(out, 1), false,
				SpanView<StringView>());
	}
	{
		PinDesc in[] = {pin("e", VarType::Enum, 0, familyB())};
		b.add(StringView("enum.inB"), SpanView<PinDesc>(in, 1), SpanView<PinDesc>(), false,
				SpanView<StringView>());
	}
	{
		PinDesc in[] = {pin("e", VarType::Enum)};
		b.add(StringView("enum.inAny"), SpanView<PinDesc>(in, 1), SpanView<PinDesc>(), false,
				SpanView<StringView>());
	}

	return b.ok;
}

// The short form of a diagnostic: enough to pin down what was reported and where, and nothing of the
// prose. `code@node.pin`, `code@node#setting`, `code@from.pin->to.pin`, or bare `code` - read off the
// numbers the kernel wrote (flow::writeDiagNumbers): the locus kind says which of `at` and `names`
// mean what.
inline mem_std::String diagLine(const mem_std::Value &entry) {
	mem_std::String out(test::getDiagCodeName(entry).str<mem_std::Interface>());
	auto &at = entry.getValue("at");
	auto &names = entry.getValue("names");
	auto name = [&](size_t i) { return StringView(names.getString(i)); };
	if (entry.getInteger("domain") != int64_t(flow::value::DiagDomain::Graph)) {
		return out;
	}
	switch (DiagLocus(entry.getInteger("locus"))) {
	case DiagLocus::Edge:
		out.append("@");
		out.append(mem_std::toString(at.getInteger(0)));
		if (!name(0).empty()) {
			out.append(".");
			out.append(name(0).data(), name(0).size());
		}
		out.append("->");
		out.append(mem_std::toString(at.getInteger(1)));
		if (!name(1).empty()) {
			out.append(".");
			out.append(name(1).data(), name(1).size());
		}
		break;
	case DiagLocus::Node:
	case DiagLocus::NodeOp:
	case DiagLocus::NodeIteration:
	case DiagLocus::Branch:
	case DiagLocus::Timeout:
		out.append("@");
		out.append(mem_std::toString(at.getInteger(0)));
		break;
	case DiagLocus::Pin:
		out.append("@");
		out.append(mem_std::toString(at.getInteger(0)));
		if (!name(0).empty()) {
			out.append(".");
			out.append(name(0).data(), name(0).size());
		}
		break;
	case DiagLocus::Setting:
		out.append("@");
		out.append(mem_std::toString(at.getInteger(0)));
		out.append("#");
		out.append(name(0).data(), name(0).size());
		break;
	default: break;
	}
	return out;
}

inline mem_std::String diagLines(const mem_std::Value &report) {
	mem_std::String out;
	if (!report.isArray()) {
		return out;
	}
	for (auto &it : report.asArray()) {
		if (!out.empty()) {
			out.append(" ");
		}
		out.append(diagLine(it));
	}
	return out;
}

} // namespace stappler::test::graphfx

#endif /* TESTS_FLOW_GRAPH_GRAPH_FIXTURE_H_ */
