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

// An enum family a node names: PinRole::EnumFamily on a String input decides the family
// of the node's EnumFamily outputs - for the edges the build checks, for the value a consumer reads,
// for the scene it is bound to, and for the editor's connect rule.

#include "SPCommon.h"
#include "SPMemory.h"
#include "SPData.h"

#include "SPFlowOps.h"

#include "../tests.h"
#include "../check/flow_check.h"
#include "../interp/interp_fixture.h"

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;
using stappler::test::hasDiag;

namespace {

using namespace stappler::flow;
using flow::value::Var;
using flow::value::VarType;

constexpr StringView FamilyA("enum.Direction");
constexpr StringView FamilyB("enum.Colour");

Var s_captured;

Status captureInput(OpContext &ctx) { return ctx.getInput(0, s_captured); }

PinDesc pin(StringView name, VarType type, PinRole role = PinRole::None) {
	PinDesc p;
	p.name = name;
	p.type = type;
	p.role = role;
	return p;
}

bool registerProbes(OpRegistry &ops) {
	for (auto family : {FamilyA, FamilyB}) {
		PinDesc in[] = {pin("e", VarType::Enum)};
		in[0].subtypeId = flow::value::makeTypeId(family);
		OpDef def;
		def.name = family == FamilyA ? StringView("probe.inA") : StringView("probe.inB");
		def.dataIn = SpanView<PinDesc>(in, 1);
		def.invoke = &captureInput;
		if (!ops.createNative(def)) {
			return false;
		}
	}
	PinDesc any[] = {pin("e", VarType::Enum)};
	OpDef def;
	def.name = StringView("probe.inAny");
	def.dataIn = SpanView<PinDesc>(any, 1);
	def.invoke = &captureInput;
	return ops.createNative(def) != nullptr;
}

// value.int32 -> enum.fromInt32 -> a consumer; `family` is the node's parameter spelling.
mem_std::String chain(StringView consumer, StringView familyParam) {
	return mem_std::toString(R"({"formatVersion": 1, "nodes": [
		{"id": 1, "op": "value.int32", "params": {"value": 4}},
		{"id": 2, "op": "enum.fromInt32")",
			familyParam.empty() ? mem_std::String() : mem_std::toString(R"(, "params": {"family": )", familyParam, "}"),
			R"(},
		{"id": 3, "op": ")", consumer, R"("}
	], "edges": [
		{"kind": "data", "from": 1, "fromPin": "value", "to": 2, "toPin": "value"},
		{"kind": "data", "from": 2, "fromPin": "result", "to": 3, "toPin": "e"}
	]})");
}

struct Built {
	OpRegistry ops;
	GraphAsset asset;
	RuntimeGraph graph;
	mem_std::Value report;
	Status status = Status::ErrorInvalidArguemnt;

	bool init() {
		return ops.init() && stappler::flow::ops::registerCoreOps(ops) == Status::Ok
				&& registerProbes(ops);
	}

	Status build(StringView json, const flow::value::TypeRegistry *scene = nullptr) {
		asset.init();
		graph.init();
		if (asset.load(data::read<mem_std::Interface>(json)) != Status::Ok) {
			return Status::ErrorInvalidArguemnt;
		}
		status = scene ? graph.build(asset, ops, *scene, &report) : graph.build(asset, ops, &report);
		return status;
	}
};

bool registerFamily(flow::value::TypeRegistry &reg) {
	flow::value::EnumMemberDef members[] = {{StringView("North"), 0}, {StringView("South"), 4}};
	return reg.createEnum(FamilyA, SpanView<flow::value::EnumMemberDef>(members, 2)) != nullptr;
}

} // namespace

void performGraphEnumFamilyTests() {
	sprt::cout << "\n== flow graph: a node's own enum family ==\n";

	// ---- the role at registration -------------------------------------------------------------------

	{
		auto refused = [](const OpDef &def) {
			OpRegistry reg;
			reg.init();
			mem_std::Value diag;
			return reg.createNative(def, &diag) == nullptr && hasDiag(diag, "op-pin-role-invalid");
		};

		PinDesc out[] = {pin("result", VarType::Enum, PinRole::EnumFamily)};
		OpDef orphan;
		orphan.name = StringView("probe.orphan");
		orphan.dataOut = SpanView<PinDesc>(out, 1);
		check(refused(orphan), "graph-enum-family: a family output with no family input is refused");

		PinDesc intName[] = {pin("family", VarType::Int, PinRole::EnumFamily)};
		OpDef wrongName;
		wrongName.name = StringView("probe.intFamily");
		wrongName.dataIn = SpanView<PinDesc>(intName, 1);
		check(refused(wrongName), "graph-enum-family: a family name that is not a String is refused");

		PinDesc named[] = {pin("family", VarType::String, PinRole::EnumFamily)};
		PinDesc fixedOut[] = {pin("result", VarType::Enum, PinRole::EnumFamily)};
		fixedOut[0].subtypeId = flow::value::makeTypeId(FamilyB);
		OpDef fixed;
		fixed.name = StringView("probe.fixed");
		fixed.dataIn = SpanView<PinDesc>(named, 1);
		fixed.dataOut = SpanView<PinDesc>(fixedOut, 1);
		check(refused(fixed),
				"graph-enum-family: a family output that already names a family is refused");

		PinDesc twice[] = {pin("a", VarType::String, PinRole::EnumFamily),
			pin("b", VarType::String, PinRole::EnumFamily)};
		OpDef two;
		two.name = StringView("probe.twoFamilies");
		two.dataIn = SpanView<PinDesc>(twice, 2);
		check(refused(two), "graph-enum-family: a second family input is refused");
	}

	// ---- the build ----------------------------------------------------------------------------------

	{
		Built b;
		check(b.init(), "graph-enum-family: the registry prepares");

		check(b.build(chain("probe.inA", "\"enum.Direction\"")) == Status::Ok,
				"graph-enum-family: a family-A value reaches a family-A input");
		auto &node = b.graph.getNodeAt(b.graph.findNode(2));
		check(node.family == flow::value::makeTypeId(FamilyA) && node.familyName == FamilyA,
				"graph-enum-family: the node carries its family");
		check(DynamicSite::fieldAt(b.graph, b.graph.findNode(2), 0).subtypeId
						== flow::value::makeTypeId(FamilyA),
				"graph-enum-family: and its output field reads back with it");

		mem_std::Value described;
		describeGraph(b.graph, described);
		check(described.getValue("nodes").getValue(1).getString("family") == FamilyA,
				"graph-enum-family: the dump names the family");

		b.report = mem_std::Value();
		check(b.build(chain("probe.inB", "\"enum.Direction\"")) != Status::Ok
						&& hasDiag(b.report, "subtype-mismatch"),
				"graph-enum-family: a family-A value does not reach a family-B input");

		b.report = mem_std::Value();
		check(b.build(chain("probe.inAny", "\"enum.Direction\"")) == Status::Ok,
				"graph-enum-family: an input with no family takes it");

		b.report = mem_std::Value();
		check(b.build(chain("probe.inAny", StringView())) != Status::Ok
						&& hasDiag(b.report, "enum-family-missing"),
				"graph-enum-family: a node that names no family is refused");

		b.report = mem_std::Value();
		auto dynamic = R"({"formatVersion": 1, "nodes": [
			{"id": 1, "op": "value.int32", "params": {"value": 4}},
			{"id": 2, "op": "enum.fromInt32", "params": {"family": "enum.Direction"}},
			{"id": 3, "op": "value.string", "params": {"value": "enum.Direction"}},
			{"id": 4, "op": "probe.inAny"}
		], "edges": [
			{"kind": "data", "from": 1, "fromPin": "value", "to": 2, "toPin": "value"},
			{"kind": "data", "from": 3, "fromPin": "value", "to": 2, "toPin": "family"},
			{"kind": "data", "from": 2, "fromPin": "result", "to": 4, "toPin": "e"}
		]})";
		check(b.build(dynamic) != Status::Ok && hasDiag(b.report, "enum-family-dynamic"),
				"graph-enum-family: a family name arriving on an edge is refused");
	}

	// ---- the run ------------------------------------------------------------------------------------

	{
		stappler::test::interpfx::Fixture fx;
		fx.extraOps = &registerProbes;
		s_captured = Var();
		check(fx.prepare(chain("probe.inA", "\"enum.Direction\"")),
				"graph-enum-family: the running graph prepares");
		fx.run();
		check(fx.report.outcome == RunOutcome::Completed && s_captured.type == VarType::Enum
						&& s_captured.e.value == 4
						&& s_captured.e.type == flow::value::makeTypeId(FamilyA),
				"graph-enum-family: the consumer reads the value with the node's family");
	}

	// ---- the scene ----------------------------------------------------------------------------------

	{
		Built b;
		b.init();

		flow::value::TypeRegistry empty;
		empty.init();
		check(b.build(chain("probe.inAny", "\"enum.Direction\""), &empty) != Status::Ok
						&& hasDiag(b.report, "enum-family-unknown"),
				"graph-enum-family: a scene with no such family refuses the graph");

		flow::value::TypeRegistry declared;
		declared.init();
		registerFamily(declared);
		b.report = mem_std::Value();
		check(b.build(chain("probe.inAny", "\"enum.Direction\""), &declared) == Status::Ok,
				"graph-enum-family: a scene that declares it binds");

		flow::value::TypeRegistry aliased;
		aliased.init();
		registerFamily(aliased);
		flow::value::AliasSpelling alias;
		alias.name = StringView("enum.Heading");
		alias.target = FamilyA;
		check(aliased.createAlias(alias) != nullptr, "graph-enum-family: an alias registers");
		b.report = mem_std::Value();
		check(b.build(chain("probe.inAny", "\"enum.Heading\""), &aliased) != Status::Ok
						&& hasDiag(b.report, "enum-family-alias"),
				"graph-enum-family: a node naming the family through an alias is refused");
	}

}

} // namespace STAPPLER_VERSIONIZED stappler