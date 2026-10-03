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

// The contract between a graph and the scene it runs on: what a graph declares it needs, and what
// happens when the scene does not have it.
//
// Two gates, and they are checked separately because they answer at different moments. The FIRST
// needs no scene at all: a node that names a component the graph's own "scene" section does not
// declare, or reads an Int out of a field the section calls a Float, is refused by the build - which
// is what an editor with nothing loaded can still say on a keystroke. The SECOND needs a registry:
// a component the scene has never heard of, a field it does not have, a type that is not the one
// declared. That one is bindScene, and build() runs it when a caller brings a scene.
//
// The thing all of this replaces: finding out on the fortieth step of a tick, as ErrorNotFound.

#include "SPCommon.h"
#include "SPMemory.h"
#include "SPData.h"

#include "../tests.h"
#include "../check/flow_check.h"
#include "graph_fixture.h"

#include "SPFlowOps.h"

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;
using namespace flow;
using namespace stappler::test::graphfx;

namespace {

namespace vstore = flow::value;

constexpr StringView ThingTypeName("probe.Thing");
constexpr StringView TagTypeName("probe.Tag");

// A component with one field of each of the two types the checks below need to tell apart, and a
// tag with no fields at all - which is what an optional group is usually asked about.
bool registerSceneTypes(flow::value::TypeRegistry &reg) {
	flow::value::FieldDef thing[] = {
		flow::value::FieldDef{.name = StringView("count"), .type = VarType::Int},
		flow::value::FieldDef{.name = StringView("size"), .type = VarType::Float},
	};
	return reg.createNative(ThingTypeName, SpanView<flow::value::FieldDef>(thing, 2)) != nullptr
			&& reg.createNative(TagTypeName, SpanView<flow::value::FieldDef>()) != nullptr;
}

// Everything a graph needs to be built: the core node library, an asset, and a runtime graph. The
// scene registry is deliberately NOT part of it - a graph is built with one or without one, and both
// have to keep working.
struct Fx {
	OpRegistry ops;
	GraphAsset asset;
	RuntimeGraph graph;
	flow::value::TypeRegistry types;
	mem_std::Value report;

	bool prepare(StringView json) {
		if (!ops.init() || flow::ops::registerCoreOps(ops) != Status::Ok) {
			return false;
		}
		if (!types.init() || !registerSceneTypes(types)) {
			return false;
		}
		asset.init();
		if (asset.load(data::read<mem_std::Interface>(json), &report) != Status::Ok) {
			return false;
		}
		return graph.init();
	}

	Status build() { return graph.build(asset, ops, &report); }
	Status buildBound() { return graph.build(asset, ops, types, &report); }

	// The bindings of the node with this id, read straight off the graph. Empty when the graph is
	// not bound, which is the accessor's contract and one of the things checked below.
	SpanView<SceneBinding> bindingsOf(NodeId id) {
		auto index = graph.findNode(id);
		return index == InvalidIndex ? SpanView<SceneBinding>() : graph.getSceneBindings(index);
	}
};

// The one reader of a report entry's code, for every section (check/studio_check.h).
using stappler::test::hasDiag;

// An entity a node parameter can name. Nothing reads it here - no graph below is executed - but
// `target` is a required input and a graph missing one is refused for that reason instead of the
// one under test.
constexpr StringView SomeTarget(R"json({"index": 1, "generation": 1})json");

// A one-node graph reading `field` off `component`. The scene section is written by the caller, so
// that "declared" and "what the node names" can be made to disagree.
mem_std::String getIntGraph(StringView scene, StringView component, StringView field) {
	return mem_std::toString(R"json({"formatVersion": 1, )json", scene,
			R"json("nodes": [{"id": 1, "op": "scene.getInt", "params": {"target": )json",
			SomeTarget, R"json(, "component": ")json", component, R"json(", "field": ")json", field,
			R"json("}}], "edges": []})json");
}

mem_std::String declaring(StringView component, StringView field, StringView type) {
	return mem_std::toString(R"json("scene": [{"component": ")json", component,
			R"json(", "fields": [{"name": ")json", field, R"json(", "type": ")json", type,
			R"json("}]}], )json");
}

} // namespace

void performGraphSceneTests() {
	sprt::cout << "\n== flow graph: the contract with the scene ==\n";

	// ---- what a node names, resolved ---------------------------------------------------------

	{
		Fx fx;
		check(fx.prepare(getIntGraph(declaring(ThingTypeName, StringView("count"),
									 StringView("int")),
					 ThingTypeName, StringView("count"))),
				"graph-scene: a graph declaring what it uses loads");
		check(fx.buildBound() == Status::Ok, "graph-scene: and builds against a scene that has it");

		auto bindings = fx.bindingsOf(1);
		auto field = fx.types.get(ThingTypeName)->getField(StringView("count"));
		check(bindings.size() == 1, "graph-scene: one group of the signature, one binding");
		check(bindings.size() == 1 && bindings[0].type == fx.types.get(ThingTypeName)
						&& bindings[0].desc == field,
				"graph-scene: bound to the registry's own descriptors, not to copies of them");
		check(bindings.size() == 1 && !bindings[0].dynamic && !bindings[0].optional,
				"graph-scene: a required name, known at build time");
		check(fx.graph.getSceneRegistry() == &fx.types
						&& fx.graph.getSceneRegistryCount() == fx.types.getCount(),
				"graph-scene: the graph remembers which registry it was bound to, and how big it was");
	}

	{
		// No dedup: two nodes naming the same component and field are two entries. A shared table
		// would save a few dozen bytes and cost the one property the interpreter relies on - that a
		// node's groups are a contiguous slice it can index by a constant.
		Fx fx;
		check(fx.prepare(mem_std::toString(R"json({"formatVersion": 1, )json",
					 declaring(ThingTypeName, StringView("count"), StringView("int")),
					 R"json("nodes": [
					{"id": 1, "op": "scene.getInt", "params": {"target": )json",
					 SomeTarget, R"json(, "component": ")json", ThingTypeName,
					 R"json(", "field": "count"}},
					{"id": 2, "op": "scene.getInt", "params": {"target": )json",
					 SomeTarget, R"json(, "component": ")json", ThingTypeName,
					 R"json(", "field": "count"}}
				], "edges": []})json")),
				"graph-scene: two nodes naming the same field load");
		check(fx.buildBound() == Status::Ok, "graph-scene: and build");

		auto first = fx.bindingsOf(1);
		auto second = fx.bindingsOf(2);
		check(first.size() == 1 && second.size() == 1 && first.data() != second.data(),
				"graph-scene: each node has a slice of its own - the bindings are not deduplicated");
	}

	// ---- the first gate: no scene needed -------------------------------------------------------

	{
		Fx fx;
		check(fx.prepare(getIntGraph(declaring(ThingTypeName, StringView("count"),
									 StringView("int")),
					 StringView("probe.Thin"), StringView("count"))),
				"graph-scene: a graph naming something it did not declare loads");
		check(fx.build() != Status::Ok && hasDiag(fx.report, StringView("scene-undeclared")),
				"graph-scene: and is refused WITHOUT A SCENE - which is what the editor can say");
		check(!fx.graph.isValid() && fx.bindingsOf(1).empty(),
				"graph-scene: a refused build leaves no graph and no bindings");
	}

	{
		Fx fx;
		check(fx.prepare(getIntGraph(declaring(ThingTypeName, StringView("count"),
									 StringView("int")),
					 ThingTypeName, StringView("weight"))),
				"graph-scene: a graph naming an undeclared FIELD loads");
		check(fx.build() != Status::Ok && hasDiag(fx.report, StringView("scene-undeclared")),
				"graph-scene: and is refused, also with no scene");
	}

	{
		// The one that used to be a step failing on the frame it first ran: scene.getInt reads an Int
		// or nothing, and the declaration says the field is a Float.
		Fx fx;
		check(fx.prepare(getIntGraph(declaring(ThingTypeName, StringView("size"),
									 StringView("float")),
					 ThingTypeName, StringView("size"))),
				"graph-scene: a graph reading a declared Float as an Int loads");
		check(fx.build() != Status::Ok && hasDiag(fx.report, StringView("scene-field-type")),
				"graph-scene: and is refused by the declaration alone");
	}

	{
		// A declaration nobody uses is not an error - it is almost always a node that was deleted and
		// a contract that was not.
		Fx fx;
		check(fx.prepare(mem_std::toString(R"json({"formatVersion": 1,
				"scene": [{"component": ")json",
					 ThingTypeName, R"json(", "fields": [{"name": "count", "type": "int"},
						{"name": "size", "type": "float"}]}],
				"nodes": [{"id": 1, "op": "scene.getInt",
					"params": {"target": )json",
					 SomeTarget, R"json(, "component": ")json", ThingTypeName,
					 R"json(", "field": "count"}}],
				"edges": []})json")),
				"graph-scene: a contract with a spare field loads");
		check(fx.buildBound() == Status::Ok
						&& hasDiag(fx.report, StringView("scene-unused")),
				"graph-scene: an unused declaration is an advice, and the graph still builds");
	}

	// ---- the second gate: the scene itself -----------------------------------------------------

	{
		Fx fx;
		check(fx.prepare(getIntGraph(StringView(), StringView("probe.Absent"),
					 StringView("count"))),
				"graph-scene: an undeclared graph naming an unknown component loads");
		check(fx.build() == Status::Ok,
				"graph-scene: with no contract and no scene, nothing can refuse it");

		Fx bound;
		check(bound.prepare(getIntGraph(StringView(), StringView("probe.Absent"),
					  StringView("count"))),
				"graph-scene: the same graph loads again");
		check(bound.buildBound() != Status::Ok
						&& hasDiag(bound.report, StringView("scene-unknown-component")),
				"graph-scene: against a scene, the component that is not there is an error");
	}

	{
		Fx fx;
		check(fx.prepare(getIntGraph(StringView(), ThingTypeName, StringView("weight"))),
				"graph-scene: a graph naming a field the scene lacks loads");
		check(fx.buildBound() != Status::Ok
						&& hasDiag(fx.report, StringView("scene-unknown-field")),
				"graph-scene: and binding says which field, at build time");
	}

	{
		Fx fx;
		check(fx.prepare(getIntGraph(StringView(), ThingTypeName, StringView("size"))),
				"graph-scene: a graph reading the scene's Float as an Int loads");
		check(fx.buildBound() != Status::Ok && hasDiag(fx.report, StringView("scene-field-type")),
				"graph-scene: and binding refuses it, naming both types");
	}

	{
		// Absence of an OPTIONAL type is an answer, not a mistake: scene.has exists to say no about
		// it and await.component exists to wait for it. Said as an advice all the same, because it is
		// also what a misspelt name looks like and only the author can tell the two apart.
		Fx fx;
		check(fx.prepare(mem_std::toString(R"json({"formatVersion": 1, "nodes": [
					{"id": 1, "op": "scene.has", "params": {"target": )json",
					 SomeTarget, R"json(, "component": "probe.Absent"}}
				], "edges": []})json")),
				"graph-scene: a graph asking about a component nothing registered loads");
		check(fx.buildBound() == Status::Ok, "graph-scene: and builds - absence is its answer");
		check(hasDiag(fx.report, StringView("scene-unknown-component")),
				"graph-scene: with an advice, since a typo looks exactly like this");

		auto bindings = fx.bindingsOf(1);
		check(bindings.size() == 1 && bindings[0].optional && bindings[0].type == nullptr,
				"graph-scene: the binding exists and holds nothing, which is the answer itself");
	}

	// ---- names an edge computes ----------------------------------------------------------------

	{
		// Nothing here can be resolved, and the operation must keep the path it always had. The
		// build says so once rather than leaving it to a profiler.
		Fx fx;
		check(fx.prepare(mem_std::toString(R"json({"formatVersion": 1, "nodes": [
					{"id": 1, "op": "value.string", "params": {"value": "probe.Tag"}},
					{"id": 2, "op": "scene.has", "params": {"target": )json",
					 SomeTarget, R"json(}}
				], "edges": [
					{"kind": "data", "from": 1, "fromPin": "value", "to": 2, "toPin": "component"}
				]})json")),
				"graph-scene: a graph computing a component name loads");
		check(fx.buildBound() == Status::Ok,
				"graph-scene: and builds - a name from an edge refuses nothing");
		check(hasDiag(fx.report, StringView("scene-name-dynamic")),
				"graph-scene: with an advice naming the pin that cannot be hoisted");

		auto bindings = fx.bindingsOf(2);
		check(bindings.empty(),
				"graph-scene: and the node gets no bindings - it never resolved anything");
	}

	// ---- binding as a step of its own ------------------------------------------------------------

	{
		// GR-5's rule one level down: build-with-a-scene and build-then-bind must not be two
		// implementations that agree today and drift tomorrow. They are one, and this compares what
		// they produce rather than trusting the arrangement.
		auto json = getIntGraph(StringView(), ThingTypeName, StringView("count"));

		Fx together;
		Fx apart;
		check(together.prepare(json) && apart.prepare(json), "graph-scene: two copies of one graph");
		check(together.buildBound() == Status::Ok, "graph-scene: one built against the scene");
		check(apart.build() == Status::Ok && apart.graph.bindScene(apart.types) == Status::Ok,
				"graph-scene: the other built alone and bound afterwards");

		mem_std::Value a;
		mem_std::Value b;
		together.graph.describeSceneContract(a);
		apart.graph.describeSceneContract(b);
		check(test::compareValues(a, b, StringView("scene contract")),
				"graph-scene: and the two contracts are identical");

		check(apart.graph.bindScene(apart.types) == Status::Ok,
				"graph-scene: binding twice is binding once");
	}

	{
		Fx fx;
		check(fx.prepare(getIntGraph(StringView(), ThingTypeName, StringView("count"))),
				"graph-scene: a graph to bind badly");
		check(fx.build() == Status::Ok, "graph-scene: built unbound");
		check(!fx.graph.getSceneRegistry() && fx.bindingsOf(1).empty(),
				"graph-scene: an unbound graph hands out no bindings, whatever it derived");

		// A registry that has the component under another name only. The bind fails, and what matters
		// is what it leaves behind: not half a binding.
		flow::value::TypeRegistry other;
		check(other.init(), "graph-scene: another registry");
		check(fx.graph.bindScene(other) != Status::Ok,
				"graph-scene: binding to a scene without the component is refused");
		check(!fx.graph.getSceneRegistry() && fx.graph.isValid(),
				"graph-scene: a failed bind leaves the graph unbound - and still a graph");
	}
}

} // namespace STAPPLER_VERSIONIZED stappler
