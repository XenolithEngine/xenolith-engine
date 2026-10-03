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

// Subtask E4: the runtime graph.
//
// The build resolves names to indices, computes the adjacency and the entry set, settles the
// conversion of every data edge, and converts every literal - so that the interpreter, when it
// arrives, reads decisions rather than making them.
//
// The second half of this section is about ORDER, which is the property that is easy to lose and
// impossible to notice later: two files holding the same records in a different order must build
// into the same graph, or the execution log of a system would depend on how its file happened to be
// written, and group F's "two identical runs produce identical local stores" would be untestable.

#include "graph_fixture.h"

#include "../tests.h"
#include "../check/flow_check.h"

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;

namespace {

using namespace stappler::test::graphfx;

// A loop: start -> repeat, body -> tick -> back to repeat, done -> print. Everything the dump has to
// show is in here - an entry, a terminal, a back edge, a literal, a derived local schema.
StringView loopGraph() {
	return StringView(R"json({
		"formatVersion": 1,
		"name": "loop",
		"nodes": [
			{"id": 1, "op": "flow.start"},
			{"id": 2, "op": "flow.repeat", "params": {"count": 3}},
			{"id": 3, "op": "flow.tick"},
			{"id": 4, "op": "sink.print"}
		],
		"edges": [
			{"kind": "exec", "from": 1, "fromPin": "then", "to": 2},
			{"kind": "exec", "from": 2, "fromPin": "body", "to": 3},
			{"kind": "exec", "from": 2, "fromPin": "done", "to": 4},
			{"kind": "exec", "from": 3, "fromPin": "then", "to": 2},
			{"kind": "data", "from": 1, "fromPin": "value", "to": 4, "toPin": "value"}
		]
	})json");
}

bool load(GraphAsset &asset, StringView json, StringView label) {
	asset.init();
	mem_std::Value diag;
	if (asset.load(data::read<mem_std::Interface>(json), &diag) != Status::Ok) {
		sprt::cout << "       " << label << ": " << data::toString<mem_std::Interface>(diag, true)
				   << "\n";
		return false;
	}
	return true;
}

bool build(RuntimeGraph &graph, const GraphAsset &asset, const OpRegistry &ops, StringView label) {
	graph.init();
	mem_std::Value report;
	if (graph.build(asset, ops, &report) != Status::Ok) {
		sprt::cout << "       " << label << ": " << data::toString<mem_std::Interface>(report, true)
				   << "\n";
		return false;
	}
	return true;
}

} // namespace

void performGraphBuildTests() {
	sprt::cout << "\n== flow graph: the runtime graph ==\n";

	OpRegistry ops;
	ops.init();
	check(buildFixtureOps(ops), "graph-build: the fixture operations register");

	// ---- the whole shape, in one golden dump --------------------------------------------------------

	{
		GraphAsset asset;
		RuntimeGraph graph;
		check(load(asset, loopGraph(), StringView("loop")), "graph-build: the loop graph loads");
		check(build(graph, asset, ops, StringView("loop")), "graph-build: the loop graph builds");

		mem_std::Value dump;
		graph.describe(dump);

		mem_std::Value expect = data::read<mem_std::Interface>(StringView(R"json({
			"nodes": [
				{"id": 1, "op": "flow.start", "entry": true, "dataIn": 0, "dataOut": 1,
					"execIn": 0, "execOut": 1, "locals": "op.flow.start.locals"},
				{"id": 2, "op": "flow.repeat", "dataIn": 0, "dataOut": 0, "execIn": 2, "execOut": 2,
					"constants": {"count": 3}},
				{"id": 3, "op": "flow.tick", "dataIn": 0, "dataOut": 0, "execIn": 1, "execOut": 1},
				{"id": 4, "op": "sink.print", "terminal": true, "dataIn": 1, "dataOut": 0,
					"execIn": 1, "execOut": 0}
			],
			"dataEdges": [
				{"from": 1, "fromPin": "value", "to": 4, "toPin": "value", "cast": "same"}
			],
			"execEdges": [
				{"from": 1, "fromPin": "then", "to": 2},
				{"from": 2, "fromPin": "body", "to": 3},
				{"from": 2, "fromPin": "done", "to": 4},
				{"from": 3, "fromPin": "then", "to": 2, "back": true}
			],
			"entry": [1],
			"terminal": [4]
		})json"));

		check(test::compareValues(dump, expect, StringView("loop graph")),
				"graph-build: the built graph matches the golden dump");

		// The accessors, since the dump is a projection of them and not the thing itself.
		check(graph.getNodeCount() == 4 && graph.findNode(3) == 2
						&& graph.findNode(7) == InvalidIndex,
				"graph-build: nodes are indexed in ascending id order");
		check(graph.getEntryNodes().size() == 1 && graph.getEntryNodes()[0] == 0,
				"graph-build: the entry set holds the node with no incoming anything");
		check(graph.getExecOutEdges(1).size() == 2 && graph.getExecInEdges(1).size() == 2,
				"graph-build: the loop node has two exec outputs and two incoming exec edges");
		check(graph.getDataInEdges(3).size() == 1 && graph.getDataOutEdges(0).size() == 1,
				"graph-build: the data edge appears in both adjacency lists");

		auto count = graph.getConstant(1, 0);
		check(count != nullptr && count->getInteger() == 3,
				"graph-build: an unconnected input carries its literal");
		check(graph.getConstant(3, 0) == nullptr,
				"graph-build: an input an edge feeds carries no literal");
	}

	// ---- conversions are decided here, once ---------------------------------------------------------

	{
		GraphAsset asset;
		RuntimeGraph graph;
		check(load(asset, StringView(R"json({"formatVersion": 1,
			"nodes": [{"id": 1, "op": "bool.out"}, {"id": 2, "op": "int.in"},
				{"id": 3, "op": "str.out"}, {"id": 4, "op": "bytes.in"},
				{"id": 5, "op": "vec.out2"}, {"id": 6, "op": "vec.in3"}],
			"edges": [{"kind": "data", "from": 1, "fromPin": "flag", "to": 2, "toPin": "n"},
				{"kind": "data", "from": 3, "fromPin": "text", "to": 4, "toPin": "blob"},
				{"kind": "data", "from": 5, "fromPin": "v", "to": 6, "toPin": "v"}]})json"),
					  StringView("casts")),
				"graph-build: the conversion fixture loads");
		check(build(graph, asset, ops, StringView("casts")),
				"graph-build: the conversion fixture builds");

		mem_std::Value dump;
		graph.describe(dump);

		mem_std::Value expectEdges = data::read<mem_std::Interface>(StringView(R"json([
			{"from": 1, "fromPin": "flag", "to": 2, "toPin": "n", "cast": "widen"},
			{"from": 3, "fromPin": "text", "to": 4, "toPin": "blob", "cast": "same",
				"needsArena": true},
			{"from": 5, "fromPin": "v", "to": 6, "toPin": "v", "cast": "widen"}
		])json"));

		check(test::compareValues(dump.getValue("dataEdges"), expectEdges, StringView("casts")),
				"graph-build: every data edge carries its resolved conversion");

		// A String reaching a Bytes input is Same by the table and still a deep copy of a block, which
		// is why the flag exists separately from the rule.
		check(graph.getDataEdges()[1].needsArena && !graph.getDataEdges()[0].needsArena,
				"graph-build: a container transfer is marked as needing an arena");
	}

	// ---- a literal is converted to the pin's type, once -----------------------------------------------

	{
		GraphAsset asset;
		RuntimeGraph graph;
		check(load(asset, StringView(R"json({"formatVersion": 1,
			"nodes": [{"id": 1, "op": "math.addf", "params": {"lhs": 3}}], "edges": []})json"),
					  StringView("literal")),
				"graph-build: the literal fixture loads");
		check(build(graph, asset, ops, StringView("literal")),
				"graph-build: the literal fixture builds");

		auto lhs = graph.getConstant(0, 0);
		check(lhs != nullptr && lhs->isDouble() && lhs->getDouble() == 3.0,
				"graph-build: the integer literal 3 on a Float input is stored as 3.0");

		// The pin's own default, also already converted, for the input the file says nothing about.
		auto rhs = graph.getConstant(0, 1);
		check(rhs != nullptr && rhs->isDouble() && rhs->getDouble() == 1.0,
				"graph-build: an input with no literal falls back to the pin's default");
	}

	// ---- order: the same records, written differently ------------------------------------------------

	{
		StringView shuffled(R"json({
			"formatVersion": 1,
			"name": "loop",
			"nodes": [
				{"id": 3, "op": "flow.tick"},
				{"id": 1, "op": "flow.start"},
				{"id": 4, "op": "sink.print"},
				{"id": 2, "op": "flow.repeat", "params": {"count": 3}}
			],
			"edges": [
				{"kind": "data", "from": 1, "fromPin": "value", "to": 4, "toPin": "value"},
				{"kind": "exec", "from": 3, "fromPin": "then", "to": 2},
				{"kind": "exec", "from": 2, "fromPin": "done", "to": 4},
				{"kind": "exec", "from": 1, "fromPin": "then", "to": 2},
				{"kind": "exec", "from": 2, "fromPin": "body", "to": 3}
			]
		})json");

		GraphAsset a;
		GraphAsset b;
		RuntimeGraph ga;
		RuntimeGraph gb;
		check(load(a, loopGraph(), StringView("order-a"))
						&& load(b, shuffled, StringView("order-b")),
				"graph-build: both orderings load");
		check(build(ga, a, ops, StringView("order-a")) && build(gb, b, ops, StringView("order-b")),
				"graph-build: both orderings build");

		mem_std::Value dumpA;
		mem_std::Value dumpB;
		ga.describe(dumpA);
		gb.describe(dumpB);
		check(test::compareValues(dumpA, dumpB, StringView("shuffled build")),
				"graph-build: record order in the file does not reach the built graph");

		// Building the same asset again gives the same graph: the build is a function of its input,
		// with nothing carried over from the previous one.
		RuntimeGraph again;
		check(build(again, a, ops, StringView("rebuild")), "graph-build: the asset builds again");
		mem_std::Value dumpAgain;
		again.describe(dumpAgain);
		check(test::compareValues(dumpAgain, dumpA, StringView("rebuild")),
				"graph-build: building the same asset twice gives the same graph");
	}

	// ---- back edges ------------------------------------------------------------------------------------

	{
		// No loop at all: every exec edge is a forward edge.
		GraphAsset asset;
		RuntimeGraph graph;
		check(load(asset, StringView(R"json({"formatVersion": 1,
			"nodes": [{"id": 1, "op": "flow.start"}, {"id": 2, "op": "flow.tick"},
				{"id": 3, "op": "sink.print", "params": {"value": 0.0}}],
			"edges": [{"kind": "exec", "from": 1, "fromPin": "then", "to": 2},
				{"kind": "exec", "from": 2, "fromPin": "then", "to": 3}]})json"),
					  StringView("acyclic")),
				"graph-build: the acyclic fixture loads");
		check(build(graph, asset, ops, StringView("acyclic")),
				"graph-build: the acyclic fixture builds");

		uint32_t backEdges = 0;
		for (auto &e : graph.getExecEdges()) {
			if (e.backEdge) {
				++backEdges;
			}
		}
		check(backEdges == 0, "graph-build: an exec graph with no loop has no back edge");
	}

	{
		// Nested loops: an outer repeat whose body holds an inner one. Two back edges, one per loop,
		// and neither is the other's.
		GraphAsset asset;
		RuntimeGraph graph;
		check(load(asset, StringView(R"json({"formatVersion": 1,
			"nodes": [{"id": 1, "op": "flow.start"},
				{"id": 2, "op": "flow.repeat", "params": {"count": 2}},
				{"id": 3, "op": "flow.repeat", "params": {"count": 3}},
				{"id": 4, "op": "flow.tick"},
				{"id": 5, "op": "flow.tick"},
				{"id": 6, "op": "sink.print", "params": {"value": 0.0}}],
			"edges": [{"kind": "exec", "from": 1, "fromPin": "then", "to": 2},
				{"kind": "exec", "from": 2, "fromPin": "body", "to": 3},
				{"kind": "exec", "from": 3, "fromPin": "body", "to": 4},
				{"kind": "exec", "from": 4, "fromPin": "then", "to": 3},
				{"kind": "exec", "from": 3, "fromPin": "done", "to": 5},
				{"kind": "exec", "from": 5, "fromPin": "then", "to": 2},
				{"kind": "exec", "from": 2, "fromPin": "done", "to": 6}]})json"),
					  StringView("nested")),
				"graph-build: the nested-loop fixture loads");
		check(build(graph, asset, ops, StringView("nested")),
				"graph-build: the nested-loop fixture builds");

		mem_std::Vector<mem_std::String> back;
		for (auto &e : graph.getExecEdges()) {
			if (e.backEdge) {
				back.emplace_back(mem_std::toString(graph.getNodeAt(e.srcNode).id, "->",
						graph.getNodeAt(e.dstNode).id));
			}
		}
		check(back.size() == 2 && back[0] == "4->3" && back[1] == "5->2",
				"graph-build: each loop contributes exactly one back edge");
	}

	{
		// A loop in a component no entry point reaches. Its back edge is still marked: the marking is
		// a property of the graph's shape, not of what happens to be reachable from a start node.
		GraphAsset asset;
		RuntimeGraph graph;
		check(load(asset, StringView(R"json({"formatVersion": 1,
			"nodes": [{"id": 1, "op": "flow.tick"}, {"id": 2, "op": "flow.tick"}],
			"edges": [{"kind": "exec", "from": 1, "fromPin": "then", "to": 2},
				{"kind": "exec", "from": 2, "fromPin": "then", "to": 1}]})json"),
					  StringView("orphan")),
				"graph-build: the unreachable-loop fixture loads");
		check(build(graph, asset, ops, StringView("orphan")),
				"graph-build: the unreachable-loop fixture builds, with warnings");
		check(graph.getEntryNodes().size() == 0,
				"graph-build: a component with no start node has no entry point");

		uint32_t backEdges = 0;
		for (auto &e : graph.getExecEdges()) {
			if (e.backEdge) {
				++backEdges;
			}
		}
		check(backEdges == 1, "graph-build: a loop no entry point reaches is still marked");
	}

	// ---- the empty graph -------------------------------------------------------------------------------

	{
		GraphAsset asset;
		RuntimeGraph graph;
		check(load(asset, StringView(R"json({"formatVersion": 1, "nodes": [], "edges": []})json"),
					  StringView("empty")),
				"graph-build: the empty graph loads");
		check(build(graph, asset, ops, StringView("empty")), "graph-build: the empty graph builds");
		check(graph.getNodeCount() == 0 && graph.getEntryNodes().size() == 0
						&& graph.getTerminalNodes().size() == 0,
				"graph-build: the empty graph is empty in every projection");
	}
}

} // namespace STAPPLER_VERSIONIZED stappler
