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

// Subtask F7, layer L2's half: which nodes make up a loop body.
//
// A SCOPE is static and an ACTIVATION is dynamic. This section is about the static half only - the
// interpreter is not involved and nothing here runs. Deciding it at build time is GR-3 applied to
// loops: the run must not have to work out which nodes an iteration consists of, because it would
// have to work it out again on every turn.
//
// The rule that is easy to get wrong, and is checked hardest below: a node that reads the value the
// loop hands out belongs to the LOOP, even though the node handing it out sits outside. Anything
// else gives that reader one record for the whole loop, so it runs once, on the first item, and
// never again - which looks like a graph bug and is not.
//
// The last block is about what is refused. A value produced inside a body cannot be read outside
// it: the reader would have to name one iteration of many and there is no honest answer. That is a
// real limitation of this stage and it is stated in the diagnostic itself, because an author who
// meets it needs to be told where to put the accumulator instead.

#include "graph_fixture.h"

#include "../tests.h"
#include "../check/flow_check.h"

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;
using stappler::test::checkEq;

namespace {

using namespace stappler::test::graphfx;

struct Built {
	OpRegistry ops;
	GraphAsset asset;
	RuntimeGraph graph;
	mem_std::Value report;

	bool build(StringView json) {
		if (!ops.init() || !buildFixtureOps(ops)) {
			return false;
		}
		asset.init();
		if (asset.load(data::read<mem_std::Interface>(json), &report) != Status::Ok) {
			return false;
		}
		graph.init();
		return graph.build(asset, ops, &report) == Status::Ok;
	}

	// The scope tree as the dump sees it, or an empty value for a graph with no loops at all.
	mem_std::Value scopes() {
		mem_std::Value dump;
		graph.describe(dump);
		return dump.getValue("scopes");
	}

	uint32_t scopeOf(NodeId id) {
		auto index = graph.findNode(id);
		return index == InvalidIndex ? 0xffff'ffffu : graph.getNodeAt(index).scope;
	}
};

// 1 start -> 2 each; body -> 3 tick -> back to 2; done -> 4 tick. 5 feeds the sequence.
constexpr StringView OneLoop(R"json({"formatVersion": 1,
	"nodes": [
		{"id": 1, "op": "flow.start"},
		{"id": 2, "op": "flow.each"},
		{"id": 3, "op": "flow.tick"},
		{"id": 4, "op": "flow.tick"},
		{"id": 5, "op": "arr.ints"}
	],
	"edges": [
		{"kind": "exec", "from": 1, "fromPin": "then", "to": 2},
		{"kind": "exec", "from": 2, "fromPin": "body", "to": 3},
		{"kind": "exec", "from": 3, "fromPin": "then", "to": 2},
		{"kind": "exec", "from": 2, "fromPin": "done", "to": 4},
		{"kind": "data", "from": 5, "fromPin": "items", "to": 2, "toPin": "items"}
	]})json");

} // namespace

void performGraphScopeTests() {
	sprt::cout << "\n== flow graph: loop bodies, statically (F7/L2) ==\n";

	// ---- a graph without loops has one scope and says nothing about it -------------------------

	{
		Built b;
		check(b.build(StringView(R"json({"formatVersion": 1,
			"nodes": [
				{"id": 1, "op": "flow.start"},
				{"id": 2, "op": "sink.print"}
			],
			"edges": [
				{"kind": "exec", "from": 1, "fromPin": "then", "to": 2},
				{"kind": "data", "from": 1, "fromPin": "value", "to": 2, "toPin": "value"}
			]})json")),
				"graph-scope: the flat graph builds");
		check(b.graph.getScopeCount() == 1,
				"graph-scope: a graph without loops is one scope - the graph");
		check(b.scopeOf(1) == 0 && b.scopeOf(2) == 0, "graph-scope: with everything in it");
		check(b.graph.getScopeNodes(0).size() == 2,
				"graph-scope: and the scope knows which nodes those are");
		check(!b.scopes().isArray(),
				"graph-scope: the dump of a graph without loops reads as it always did");
	}

	// ---- one loop ---------------------------------------------------------------------------------

	{
		Built b;
		check(b.build(OneLoop), "graph-scope: the one-loop graph builds");
		check(b.graph.getScopeCount() == 2, "graph-scope: a loop is a second scope");

		mem_std::Value expect = data::read<mem_std::Interface>(StringView(R"json([
			{"scope": 0, "depth": 0, "nodes": [1, 2, 4, 5]},
			{"scope": 1, "depth": 1, "opener": 2, "pin": "body", "parent": 0, "nodes": [3]}
		])json"));
		check(test::compareValues(b.scopes(), expect, StringView("one loop")),
				"graph-scope: the body holds what `body` reaches, and nothing else");
		check(b.scopeOf(4) == 0,
				"graph-scope: what `done` reaches is after the loop, not inside it");
		check(b.scopeOf(2) == 0,
				"graph-scope: and the node that opens a loop is outside the loop it opens");
	}

	// ---- what the loop hands out belongs to the loop --------------------------------------------

	{
		// 6 reads `item`. It has no exec input, so nothing tells it to run - but its value is a
		// per-iteration value, and a node computing from a per-iteration value is per-iteration too.
		Built b;
		check(b.build(StringView(R"json({"formatVersion": 1,
			"nodes": [
				{"id": 1, "op": "flow.start"},
				{"id": 2, "op": "flow.each"},
				{"id": 3, "op": "flow.tick"},
				{"id": 5, "op": "arr.ints"},
				{"id": 6, "op": "int.in"}
			],
			"edges": [
				{"kind": "exec", "from": 1, "fromPin": "then", "to": 2},
				{"kind": "exec", "from": 2, "fromPin": "body", "to": 3},
				{"kind": "exec", "from": 3, "fromPin": "then", "to": 2},
				{"kind": "data", "from": 5, "fromPin": "items", "to": 2, "toPin": "items"},
				{"kind": "data", "from": 2, "fromPin": "item", "to": 6, "toPin": "n"}
			]})json")),
				"graph-scope: the item-reader graph builds");
		check(b.scopeOf(6) == 1,
				"graph-scope: a node reading the loop's item belongs to the loop, not to its opener");
		check(b.scopeOf(5) == 0,
				"graph-scope: while what feeds the loop from outside stays outside, computed once");
	}

	{
		// And a pure node fed only from outside stays outside, even next to a loop.
		Built b;
		check(b.build(StringView(R"json({"formatVersion": 1,
			"nodes": [
				{"id": 1, "op": "flow.start"},
				{"id": 2, "op": "flow.each"},
				{"id": 3, "op": "flow.tick"},
				{"id": 5, "op": "arr.ints"},
				{"id": 6, "op": "math.addf"},
				{"id": 7, "op": "math.addf"}
			],
			"edges": [
				{"kind": "exec", "from": 1, "fromPin": "then", "to": 2},
				{"kind": "exec", "from": 2, "fromPin": "body", "to": 3},
				{"kind": "data", "from": 5, "fromPin": "items", "to": 2, "toPin": "items"},
				{"kind": "data", "from": 1, "fromPin": "value", "to": 6, "toPin": "lhs"},
				{"kind": "data", "from": 6, "fromPin": "result", "to": 7, "toPin": "lhs"}
			]})json")),
				"graph-scope: the outside-chain graph builds");
		check(b.scopeOf(6) == 0 && b.scopeOf(7) == 0,
				"graph-scope: a chain of eager nodes fed from outside is computed once, outside");
	}

	// ---- nested ------------------------------------------------------------------------------------

	{
		Built b;
		check(b.build(StringView(R"json({"formatVersion": 1,
			"nodes": [
				{"id": 1, "op": "flow.start"},
				{"id": 2, "op": "flow.each"},
				{"id": 3, "op": "flow.each"},
				{"id": 4, "op": "flow.tick"},
				{"id": 5, "op": "flow.tick"},
				{"id": 6, "op": "arr.ints"}
			],
			"edges": [
				{"kind": "exec", "from": 1, "fromPin": "then", "to": 2},
				{"kind": "exec", "from": 2, "fromPin": "body", "to": 3},
				{"kind": "exec", "from": 3, "fromPin": "body", "to": 4},
				{"kind": "exec", "from": 4, "fromPin": "then", "to": 3},
				{"kind": "exec", "from": 3, "fromPin": "done", "to": 2},
				{"kind": "exec", "from": 2, "fromPin": "done", "to": 5},
				{"kind": "data", "from": 6, "fromPin": "items", "to": 2, "toPin": "items"},
				{"kind": "data", "from": 6, "fromPin": "items", "to": 3, "toPin": "items"}
			]})json")),
				"graph-scope: the nested graph builds");

		mem_std::Value expect = data::read<mem_std::Interface>(StringView(R"json([
			{"scope": 0, "depth": 0, "nodes": [1, 2, 5, 6]},
			{"scope": 1, "depth": 1, "opener": 2, "pin": "body", "parent": 0, "nodes": [3]},
			{"scope": 2, "depth": 2, "opener": 3, "pin": "body", "parent": 1, "nodes": [4]}
		])json"));
		check(test::compareValues(b.scopes(), expect, StringView("nested loops")),
				"graph-scope: nested loops nest, with the inner one's parent the outer one");
		check(b.graph.getScopeAt(2).depth == 2 && b.graph.getScopeAt(2).parent == 1,
				"graph-scope: and the depth counts how deep the activations will be");
		check(b.graph.isScopeWithin(2, 0) && b.graph.isScopeWithin(2, 1)
						&& !b.graph.isScopeWithin(1, 2),
				"graph-scope: containment is asked in one direction and answers in one direction");
	}

	// ---- two loops side by side ----------------------------------------------------------------------

	{
		Built b;
		check(b.build(StringView(R"json({"formatVersion": 1,
			"nodes": [
				{"id": 1, "op": "flow.start"},
				{"id": 2, "op": "flow.each"},
				{"id": 3, "op": "flow.tick"},
				{"id": 4, "op": "flow.each"},
				{"id": 5, "op": "flow.tick"},
				{"id": 6, "op": "flow.tick"},
				{"id": 7, "op": "arr.ints"}
			],
			"edges": [
				{"kind": "exec", "from": 1, "fromPin": "then", "to": 2},
				{"kind": "exec", "from": 2, "fromPin": "body", "to": 3},
				{"kind": "exec", "from": 3, "fromPin": "then", "to": 2},
				{"kind": "exec", "from": 2, "fromPin": "done", "to": 4},
				{"kind": "exec", "from": 4, "fromPin": "body", "to": 5},
				{"kind": "exec", "from": 5, "fromPin": "then", "to": 4},
				{"kind": "exec", "from": 4, "fromPin": "done", "to": 6},
				{"kind": "data", "from": 7, "fromPin": "items", "to": 2, "toPin": "items"},
				{"kind": "data", "from": 7, "fromPin": "items", "to": 4, "toPin": "items"}
			]})json")),
				"graph-scope: the sibling graph builds");

		mem_std::Value expect = data::read<mem_std::Interface>(StringView(R"json([
			{"scope": 0, "depth": 0, "nodes": [1, 2, 4, 6, 7]},
			{"scope": 1, "depth": 1, "opener": 2, "pin": "body", "parent": 0, "nodes": [3]},
			{"scope": 2, "depth": 1, "opener": 4, "pin": "body", "parent": 0, "nodes": [5]}
		])json"));
		check(test::compareValues(b.scopes(), expect, StringView("sibling loops")),
				"graph-scope: two loops one after the other are two scopes at the same depth");
		check(!b.graph.isScopeWithin(1, 2) && !b.graph.isScopeWithin(2, 1),
				"graph-scope: and neither is inside the other");
	}

	// ---- what has no answer, and is refused ------------------------------------------------------------

	{
		// The accumulator: node 6 computes inside the body and node 7 reads it after the loop. Which
		// iteration's value? There is no answer, so there is no such edge.
		Built b;
		mem_std::Value report;
		OpRegistry ops;
		check(ops.init() && buildFixtureOps(ops), "graph-scope: the escape registry builds");

		GraphAsset asset;
		asset.init();
		check(asset.load(data::read<mem_std::Interface>(StringView(R"json({"formatVersion": 1,
			"nodes": [
				{"id": 1, "op": "flow.start"},
				{"id": 2, "op": "flow.each"},
				{"id": 3, "op": "flow.tick"},
				{"id": 5, "op": "arr.ints"},
				{"id": 6, "op": "math.addi"},
				{"id": 7, "op": "math.addi"}
			],
			"edges": [
				{"kind": "exec", "from": 1, "fromPin": "then", "to": 2},
				{"kind": "exec", "from": 2, "fromPin": "body", "to": 3},
				{"kind": "data", "from": 5, "fromPin": "items", "to": 2, "toPin": "items"},
				{"kind": "data", "from": 2, "fromPin": "item", "to": 6, "toPin": "lhs"},
				{"kind": "data", "from": 6, "fromPin": "result", "to": 7, "toPin": "lhs"}
			]})json")),
					   &report)
						== Status::Ok,
				"graph-scope: the escape asset loads");

		// Node 7 reads node 6, which is in the body. So 7 joins the body too - that is rule 2, and
		// it is why THIS graph is legal. The escape needs a reader nothing else pulls inward.
		RuntimeGraph inward;
		inward.init();
		check(inward.build(asset, ops, &report) == Status::Ok,
				"graph-scope: a chain of eager readers is pulled into the body whole");
		check(inward.getNodeAt(inward.findNode(7)).scope == 1,
				"graph-scope: including the last of them");
	}

	{
		// Now the reader is exec-gated and sits after the loop, so nothing can pull it inward.
		OpRegistry ops;
		mem_std::Value report;
		check(ops.init() && buildFixtureOps(ops), "graph-scope: the sink registry builds");

		GraphAsset asset;
		asset.init();
		check(asset.load(data::read<mem_std::Interface>(StringView(R"json({"formatVersion": 1,
			"nodes": [
				{"id": 1, "op": "flow.start"},
				{"id": 2, "op": "flow.each"},
				{"id": 3, "op": "flow.tick"},
				{"id": 4, "op": "sink.count"},
				{"id": 5, "op": "arr.ints"},
				{"id": 6, "op": "math.addi"}
			],
			"edges": [
				{"kind": "exec", "from": 1, "fromPin": "then", "to": 2},
				{"kind": "exec", "from": 2, "fromPin": "body", "to": 3},
				{"kind": "exec", "from": 2, "fromPin": "done", "to": 4},
				{"kind": "data", "from": 5, "fromPin": "items", "to": 2, "toPin": "items"},
				{"kind": "data", "from": 2, "fromPin": "item", "to": 6, "toPin": "lhs"},
				{"kind": "data", "from": 6, "fromPin": "result", "to": 4, "toPin": "n"}
			]})json")),
					   &report)
						== Status::Ok,
				"graph-scope: the sink asset loads");

		RuntimeGraph graph;
		graph.init();
		check(graph.build(asset, ops, &report) != Status::Ok,
				"graph-scope: a value computed in a body and read after the loop is refused");
		checkEq(StringView(diagLines(report)), StringView("scope-escape@6.result->4.n"),
				"graph-scope: and the diagnostic names the edge that cannot exist");
	}

	{
		// Execution leaving a body sideways: node 3 is in the body and jumps to node 4, which is not
		// the loop and not after it.
		OpRegistry ops;
		mem_std::Value report;
		check(ops.init() && buildFixtureOps(ops), "graph-scope: the jump registry builds");

		GraphAsset asset;
		asset.init();
		check(asset.load(data::read<mem_std::Interface>(StringView(R"json({"formatVersion": 1,
			"nodes": [
				{"id": 1, "op": "flow.start"},
				{"id": 2, "op": "flow.each"},
				{"id": 3, "op": "flow.tick"},
				{"id": 4, "op": "flow.tick"},
				{"id": 5, "op": "arr.ints"}
			],
			"edges": [
				{"kind": "exec", "from": 1, "fromPin": "then", "to": 2},
				{"kind": "exec", "from": 2, "fromPin": "body", "to": 3},
				{"kind": "exec", "from": 2, "fromPin": "done", "to": 4},
				{"kind": "exec", "from": 3, "fromPin": "then", "to": 4},
				{"kind": "data", "from": 5, "fromPin": "items", "to": 2, "toPin": "items"}
			]})json")),
					   &report)
						== Status::Ok,
				"graph-scope: the jump asset loads");

		RuntimeGraph graph;
		graph.init();
		check(graph.build(asset, ops, &report) != Status::Ok,
				"graph-scope: a node reached both inside a body and after it is refused");
		check(StringView(diagLines(report)).find("scope-") != maxOf<size_t>(),
				"graph-scope: with a scope diagnostic, not a pile of unrelated ones");
	}

	{
		// Closing the loop is not an escape. This is the shape every loop has, and it must build.
		Built b;
		check(b.build(OneLoop), "graph-scope: an exec edge back to the loop's own node is legal");
		check(b.graph.getScopeCount() == 2 && b.scopeOf(3) == 1,
				"graph-scope: and it does not drag the loop's node into its own body");
	}

	{
		// An operation cannot open two bodies: the value it hands out would belong to both.
		OpRegistry ops;
		check(ops.init(), "graph-scope: the two-scope registry builds");
		const StringView pins[] = {StringView("a"), StringView("b")};
		OpDef def;
		def.name = StringView("flow.twoBodies");
		def.hasExecIn = true;
		def.execOut = SpanView<StringView>(pins, 2);
		def.scopeExecOut = 0b11;
		def.invoke = &noopInvoke;
		check(ops.createNative(def) == nullptr,
				"graph-scope: an operation with two scope-opening outputs is refused at registration");

		OpDef bad;
		bad.name = StringView("flow.noSuchPin");
		bad.hasExecIn = true;
		bad.execOut = SpanView<StringView>(pins, 1);
		bad.scopeExecOut = 0b10;
		bad.invoke = &noopInvoke;
		check(ops.createNative(bad) == nullptr,
				"graph-scope: and so is a scope on an exec output that does not exist");
	}

	// ---- the advice of F-I still works over a graph with loops ---------------------------------------

	{
		Built b;
		check(b.build(OneLoop), "graph-scope: the advice graph builds");
		check(b.graph.isValid(),
				"graph-scope: scopes never change a verdict - they are structure, not an opinion");
	}
}

} // namespace STAPPLER_VERSIONIZED stappler
