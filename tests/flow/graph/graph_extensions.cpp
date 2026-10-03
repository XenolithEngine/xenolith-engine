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

// The graph's side of the extension contract: a section in the asset, a pin role, and two
// gates.
//
// The shape is the one the scene contract already has, and the argument for it is the same: a node
// names an INSTANCE ("board"), the asset declares what that instance is, and the build checks the
// first against the second WITH NO SCENE IN REACH. That is what lets an editor refuse a typo on the
// keystroke that made it instead of on the frame that reaches the node.
//
// One thing is sharper here than for a component. A component name has a fallback - makeTypeId is a
// pure function, so an operation handed no binding can still work the name out and ask the scene
// itself. An extension id has none: what stands behind it is a host object. So an unresolved
// extension is not a slower path, it is no path, and that is why this file spends most of its length
// on the six ways the build says no.

#include "SPCommon.h"
#include "SPMemory.h"
#include "SPData.h"

#include "SPFlowOps.h"
#include "SPFlowValueHost.h"

#include "../tests.h"
#include "../check/flow_check.h"

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;

namespace {

using namespace flow;
using namespace flow::value;

// The probe: it names an extension on a pin. The sections here only build it - what a live instance
// answers is a host's test - so its body only says whether the binding resolved.
Status opExtRead(OpContext &ctx) {
	if (!ctx.getExtension(0)) {
		return Status::ErrorNotFound;
	}
	return ctx.fire(uint32_t(0));
}

bool registerProbe(OpRegistry &reg) {
	const PinDesc in[] = {PinDesc{.name = StringView("extension"),
		.type = VarType::String,
		.role = PinRole::ExtensionName}};
	const PinDesc out[] = {PinDesc{.name = StringView("mark"), .type = VarType::Int}};
	const StringView then[] = {StringView("then")};

	OpDef def;
	def.name = StringView("test.extRead");
	def.dataIn = SpanView<PinDesc>(in, 1);
	def.dataOut = SpanView<PinDesc>(out, 1);
	def.hasExecIn = true;
	def.execOut = SpanView<StringView>(then, 1);
	def.flags = OpFlags::ReadsScene;
	def.invoke = &opExtRead;
	return reg.createNative(def) != nullptr;
}

// A graph naming one extension instance, parameterised by what the declaration and the node say so
// that every refusal below is one string apart from the accepted case.
mem_std::String graphJson(StringView declaredName, StringView declaredId, uint32_t declaredMark,
		StringView namedId) {
	return mem_std::toString(R"json({
	"formatVersion": 1,
	"name": "ext-probe",
	"extensions": [{"name": ")json",
			declaredName, R"json(", "id": ")json", declaredId, R"json(", "params": {"mark": )json",
			declaredMark, R"json(}}],
	"nodes": [
		{"id": 1, "op": "flow.event"},
		{"id": 2, "op": "test.extRead", "params": {"extension": ")json",
			namedId, R"json("}}
	],
	"edges": [
		{"kind": "exec", "from": 1, "fromPin": "then", "to": 2}
	]
})json");
}

// The one reader of a report entry's code, for every section (check/flow_check.h).
using stappler::test::hasDiag;

// One place that builds the operation registry, so every case below starts from the same one.
struct Probe {
	OpRegistry ops;
	GraphAsset asset;
	RuntimeGraph graph;

	bool prepare(StringView json, mem_std::Value *diag = nullptr) {
		return ops.init() && flow::ops::registerCoreOps(ops) == Status::Ok
				&& Interpreter::registerCoreTypes(ops.getLocalTypes()) == Status::Ok
				&& registerProbe(ops) && asset.init()
				&& asset.load(data::read<mem_std::Interface>(json), diag) == Status::Ok
				&& graph.init();
	}
};

} // namespace

void performGraphExtensionsTests() {
	sprt::cout << "\n== flow graph: the extension contract ==\n";

	// 1. The section is part of the FORMAT: it canonicalizes, it round-trips, and a file that cannot
	//    say what an id means is refused by structure alone.
	{
		GraphAsset asset;
		check(asset.init(), "graph-extensions: an asset");

		constexpr StringView json(R"json({
			"formatVersion": 1,
			"extensions": [
				{"name": "xs.grid", "id": "fog", "params": {"dims": 3}},
				{"name": "xs.grid", "id": "board", "params": {"dims": 2}}
			],
			"nodes": [{"id": 1, "op": "flow.event"}],
			"edges": []
		})json");
		check(asset.load(data::read<mem_std::Interface>(json)) == Status::Ok,
				"graph-extensions: the section loads");
		check(asset.getExtensions().size() == 2, "graph-extensions: both declarations survive");
		check(asset.getExtensions()[0].id == StringView("board")
						&& asset.getExtensions()[1].id == StringView("fog"),
				"graph-extensions: in id order, whatever order the file used");
		check(asset.getExtensions()[0].name == StringView("xs.grid")
						&& asset.getExtensions()[0].params.getInteger("dims") == 2,
				"graph-extensions: carrying the extension and its parameters");

		mem_std::Value saved;
		asset.save(saved);
		GraphAsset reloaded;
		mem_std::Value again;
		check(reloaded.init() && reloaded.load(saved) == Status::Ok,
				"graph-extensions: it reloads");
		reloaded.save(again);
		check(data::toString<mem_std::Interface>(again)
						== data::toString<mem_std::Interface>(saved),
				"graph-extensions: save -> load -> save is byte for byte the same");

		// Two instances of one extension are ordinary; two declarations of one id are a file that
		// cannot say what "board" means.
		constexpr StringView dup(R"json({
			"formatVersion": 1,
			"extensions": [
				{"name": "xs.grid", "id": "board"},
				{"name": "xs.other", "id": "board"}
			],
			"nodes": [{"id": 1, "op": "flow.event"}],
			"edges": []
		})json");
		GraphAsset bad;
		check(bad.init() && bad.load(data::read<mem_std::Interface>(dup)) != Status::Ok,
				"graph-extensions: one id declared twice is refused");

		constexpr StringView nameless(R"json({
			"formatVersion": 1,
			"extensions": [{"id": "board"}],
			"nodes": [{"id": 1, "op": "flow.event"}],
			"edges": []
		})json");
		GraphAsset bad2;
		check(bad2.init() && bad2.load(data::read<mem_std::Interface>(nameless)) != Status::Ok,
				"graph-extensions: a declaration without an extension is refused");
	}

	// 2. The first gate: a node against the declaration, WITH NO SCENE. This is the case the section
	//    exists for - the editor has an asset and nothing else, and it can still say no.
	{
		Probe probe;
		mem_std::Value report;
		check(probe.prepare(graphJson(StringView("test.mark"), StringView("board"), 7,
					  StringView("bored"))),
				"graph-extensions: a graph naming an id it did not declare");
		check(probe.graph.build(probe.asset, probe.ops, &report) != Status::Ok,
				"graph-extensions: which does not build");
		check(hasDiag(report, StringView("ext-undeclared")),
				"graph-extensions: and says the id is not declared");
		check(!hasDiag(report, StringView("ext-unknown"))
						&& !hasDiag(report, StringView("ext-instance-missing")),
				"graph-extensions: without pretending to know anything about a scene");
	}

	// 3. A declaration nobody names is an advice, not an error - but it is worth saying, because a
	//    host will go and CREATE that instance, and an unread block in an arena is still a block.
	{
		Probe probe;
		mem_std::Value report;
		constexpr StringView json(R"json({
			"formatVersion": 1,
			"extensions": [{"name": "test.mark", "id": "board", "params": {"mark": 7}}],
			"nodes": [{"id": 1, "op": "flow.event"}],
			"edges": []
		})json");
		check(probe.prepare(json), "graph-extensions: a graph that declares and does not use");
		check(probe.graph.build(probe.asset, probe.ops, &report) == Status::Ok,
				"graph-extensions: which builds");
		check(hasDiag(report, StringView("ext-unused")),
				"graph-extensions: with an advice about it");
	}

	// 4. A name that arrives on an edge. Advised, bound to nothing - and, unlike a component name,
	//    with no path left for the node to take.
	{
		Probe probe;
		mem_std::Value report;
		constexpr StringView json(R"json({
			"formatVersion": 1,
			"extensions": [{"name": "test.mark", "id": "board", "params": {"mark": 7}}],
			"nodes": [
				{"id": 1, "op": "flow.event"},
				{"id": 2, "op": "value.string", "params": {"value": "board"}},
				{"id": 3, "op": "test.extRead"}
			],
			"edges": [
				{"kind": "exec", "from": 1, "fromPin": "then", "to": 3},
				{"kind": "data", "from": 2, "fromPin": "value", "to": 3, "toPin": "extension"}
			]
		})json");
		check(probe.prepare(json), "graph-extensions: a graph naming its extension on an edge");
		check(probe.graph.build(probe.asset, probe.ops, &report) == Status::Ok,
				"graph-extensions: which builds");
		check(hasDiag(report, StringView("ext-name-dynamic")),
				"graph-extensions: with an advice that nothing can be resolved for it");

		mem_std::Value contract;
		probe.graph.describeExtensionContract(contract);
		check(contract.isArray() && contract.size() == 1 && contract.getValue(0).getBool("dynamic"),
				"graph-extensions: and the binding says so");
	}

}

} // namespace STAPPLER_VERSIONIZED stappler