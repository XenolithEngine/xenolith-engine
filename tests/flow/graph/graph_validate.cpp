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

// Subtask E3: what a graph is refused for, and what it is only warned about.
//
// Every case below states the whole expected report - code, node and pin - not merely "refused".
// "It failed" is not a test of a diagnostic: an author reads the code and the locus, and a rule that
// fires with the wrong node named is as broken as one that does not fire.
//
// The line that will surprise someone: an Int output does NOT reach a Float input. int64 does not
// fit in a double without loss, so the conversion matrix calls that cell Narrow, and only Same and
// Widen cross an edge implicitly. Whether that stays this way is a decision to make against real
// graphs in group F; that it is a DECISION and not an accident is what this section records.

#include "graph_fixture.h"

#include "../tests.h"
#include "../check/flow_check.h"

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;

namespace {

using namespace stappler::test::graphfx;

struct Case {
	StringView what;
	StringView json;
	StringView expect; // the whole report, in order
};

bool runCase(const OpRegistry &ops, const Case &c) {
	GraphAsset asset;
	asset.init();

	mem_std::Value loadDiag;
	if (asset.load(data::read<mem_std::Interface>(c.json), &loadDiag) != Status::Ok) {
		sprt::cout << "       " << c.what << ": the fixture itself does not load: "
				   << data::toString<mem_std::Interface>(loadDiag, true) << "\n";
		return false;
	}

	RuntimeGraph graph;
	graph.init();

	mem_std::Value report;
	auto st = graph.build(asset, ops, &report);
	auto lines = diagLines(report);

	if (StringView(lines) != c.expect) {
		sprt::cout << "       " << c.what << ":\n         got      \"" << lines
				   << "\"\n         expected \"" << c.expect << "\"\n";
		return false;
	}

	// A graph with an error must not be left half-built, and one with only warnings must be usable.
	bool hasError = false;
	if (report.isArray()) {
		for (auto &it : report.asArray()) {
			if (test::getDiagSeverityName(it) == "error") {
				hasError = true;
			}
		}
	}
	if (hasError && (st == Status::Ok || graph.isValid())) {
		sprt::cout << "       " << c.what << ": an error left the graph built\n";
		return false;
	}
	if (!hasError && (st != Status::Ok || !graph.isValid())) {
		sprt::cout << "       " << c.what << ": warnings alone refused the graph\n";
		return false;
	}
	return true;
}

} // namespace

void performGraphValidateTests() {
	sprt::cout << "\n== flow graph: validation ==\n";

	OpRegistry ops;
	ops.init();
	check(buildFixtureOps(ops), "graph-validate: the fixture operations register");

	const Case cases[] = {
		{StringView("a well-formed graph"), StringView(R"json({"formatVersion": 1,
			"nodes": [{"id": 1, "op": "flow.start"}, {"id": 2, "op": "sink.print"}],
			"edges": [{"kind": "exec", "from": 1, "fromPin": "then", "to": 2},
				{"kind": "data", "from": 1, "fromPin": "value", "to": 2, "toPin": "value"}]})json"),
			StringView("")},

		{StringView("an unregistered operation"), StringView(R"json({"formatVersion": 1,
			"nodes": [{"id": 1, "op": "flow.start"}, {"id": 2, "op": "math.imaginary"}],
			"edges": []})json"),
			StringView("unknown-op@2")},

		{StringView("an edge to a node that does not exist"), StringView(R"json({"formatVersion": 1,
			"nodes": [{"id": 1, "op": "flow.start"}],
			"edges": [{"kind": "data", "from": 1, "fromPin": "value", "to": 9,
				"toPin": "value"}]})json"),
			StringView("unknown-node@1.value->9.value")},

		{StringView("an edge from a pin that does not exist"),
			StringView(R"json({"formatVersion": 1,
			"nodes": [{"id": 1, "op": "flow.start"}, {"id": 2, "op": "sink.print"}],
			"edges": [{"kind": "data", "from": 1, "fromPin": "amount", "to": 2,
				"toPin": "value"}]})json"),
			StringView("unknown-pin@1.amount->2.value missing-input@2.value unreachable@2")},

		{StringView("a data edge leaving an input"), StringView(R"json({"formatVersion": 1,
			"nodes": [{"id": 1, "op": "math.addf"}, {"id": 2, "op": "sink.print"}],
			"edges": [{"kind": "data", "from": 1, "fromPin": "lhs", "to": 2,
				"toPin": "value"}]})json"),
			StringView("pin-kind-mismatch@1.lhs->2.value missing-input@2.value unreachable@2")},

		{StringView("a data edge entering an output"), StringView(R"json({"formatVersion": 1,
			"nodes": [{"id": 1, "op": "flow.start"}, {"id": 2, "op": "math.addf"}],
			"edges": [{"kind": "data", "from": 1, "fromPin": "value", "to": 2,
				"toPin": "result"}]})json"),
			StringView("pin-kind-mismatch@1.value->2.result")},

		{StringView("an exec edge into a node with no exec input"), StringView(R"json({
			"formatVersion": 1,
			"nodes": [{"id": 1, "op": "flow.start"}, {"id": 2, "op": "math.addf"}],
			"edges": [{"kind": "exec", "from": 1, "fromPin": "then", "to": 2}]})json"),
			StringView("pin-kind-mismatch@1.then->2")},

		{StringView("an exec edge leaving a data output"), StringView(R"json({"formatVersion": 1,
			"nodes": [{"id": 1, "op": "flow.start"}, {"id": 2, "op": "sink.print"}],
			"edges": [{"kind": "exec", "from": 1, "fromPin": "value", "to": 2}]})json"),
			StringView("pin-kind-mismatch@1.value->2 missing-input@2.value unreachable@2")},

		// The documented consequence of "Same and Widen only".
		{StringView("Int does not reach Float"), StringView(R"json({"formatVersion": 1,
			"nodes": [{"id": 1, "op": "math.addi", "params": {"lhs": 1, "rhs": 2}},
				{"id": 2, "op": "sink.print"}],
			"edges": [{"kind": "data", "from": 1, "fromPin": "result", "to": 2,
				"toPin": "value"}]})json"),
			StringView("type-mismatch@1.result->2.value missing-input@2.value unreachable@2")},

		{StringView("Bool reaches Int, and Vec2 reaches Vec3"), StringView(R"json({
			"formatVersion": 1,
			"nodes": [{"id": 1, "op": "bool.out"}, {"id": 2, "op": "int.in"},
				{"id": 3, "op": "vec.out2"}, {"id": 4, "op": "vec.in3"}],
			"edges": [{"kind": "data", "from": 1, "fromPin": "flag", "to": 2, "toPin": "n"},
				{"kind": "data", "from": 3, "fromPin": "v", "to": 4, "toPin": "v"}]})json"),
			StringView("eager-speculative@1 eager-unused@2 eager-speculative@3 eager-unused@4")},

		{StringView("a String reaches a Bytes input"), StringView(R"json({"formatVersion": 1,
			"nodes": [{"id": 1, "op": "str.out"}, {"id": 2, "op": "bytes.in"}],
			"edges": [{"kind": "data", "from": 1, "fromPin": "text", "to": 2,
				"toPin": "blob"}]})json"),
			StringView("eager-speculative@1 eager-unused@2")},

		{StringView("Array<Int> does not reach Array<Float>"), StringView(R"json({
			"formatVersion": 1,
			"nodes": [{"id": 1, "op": "arr.ints"}, {"id": 2, "op": "arr.floats"}],
			"edges": [{"kind": "data", "from": 1, "fromPin": "items", "to": 2,
				"toPin": "items"}]})json"),
			StringView("element-mismatch@1.items->2.items")},

		{StringView("Array<Int> reaches Array<Int>"), StringView(R"json({"formatVersion": 1,
			"nodes": [{"id": 1, "op": "arr.ints"}, {"id": 2, "op": "arr.intsIn"}],
			"edges": [{"kind": "data", "from": 1, "fromPin": "items", "to": 2,
				"toPin": "items"}]})json"),
			StringView("eager-speculative@1 eager-unused@2")},

		{StringView("one enum family does not reach another"), StringView(R"json({
			"formatVersion": 1,
			"nodes": [{"id": 1, "op": "enum.outA"}, {"id": 2, "op": "enum.inB"}],
			"edges": [{"kind": "data", "from": 1, "fromPin": "e", "to": 2, "toPin": "e"}]})json"),
			StringView("subtype-mismatch@1.e->2.e")},

		{StringView("an unconstrained enum input takes any family"), StringView(R"json({
			"formatVersion": 1,
			"nodes": [{"id": 1, "op": "enum.outA"}, {"id": 2, "op": "enum.inAny"}],
			"edges": [{"kind": "data", "from": 1, "fromPin": "e", "to": 2, "toPin": "e"}]})json"),
			StringView("eager-speculative@1 eager-unused@2")},

		{StringView("two edges into one data input"), StringView(R"json({"formatVersion": 1,
			"nodes": [{"id": 1, "op": "flow.start"}, {"id": 2, "op": "flow.start"},
				{"id": 3, "op": "sink.print"}],
			"edges": [{"kind": "data", "from": 1, "fromPin": "value", "to": 3, "toPin": "value"},
				{"kind": "data", "from": 2, "fromPin": "value", "to": 3, "toPin": "value"}]})json"),
			StringView("pin-arity@2.value->3.value unreachable@3")},

		{StringView("two edges out of one exec output"), StringView(R"json({"formatVersion": 1,
			"nodes": [{"id": 1, "op": "flow.tick"}, {"id": 2, "op": "flow.branch",
					"params": {"condition": true}},
				{"id": 3, "op": "flow.branch", "params": {"condition": true}}],
			"edges": [{"kind": "exec", "from": 1, "fromPin": "then", "to": 2},
				{"kind": "exec", "from": 1, "fromPin": "then", "to": 3}]})json"),
			StringView("pin-arity@1.then->3 unreachable@1 unreachable@3")},

		{StringView("a cycle over data edges"), StringView(R"json({"formatVersion": 1,
			"nodes": [{"id": 1, "op": "math.addf"}, {"id": 2, "op": "math.addf"},
				{"id": 3, "op": "math.addf"}],
			"edges": [{"kind": "data", "from": 1, "fromPin": "result", "to": 2, "toPin": "lhs"},
				{"kind": "data", "from": 2, "fromPin": "result", "to": 3, "toPin": "lhs"},
				{"kind": "data", "from": 3, "fromPin": "result", "to": 1,
					"toPin": "lhs"}]})json"),
			StringView("data-cycle@1")},

		// The same shape over exec edges is a loop, and legal. The node with the exec input that
		// nothing reaches is warned about, because nothing can ever start it.
		{StringView("a cycle over exec edges"), StringView(R"json({"formatVersion": 1,
			"nodes": [{"id": 1, "op": "flow.repeat", "params": {"count": 3}},
				{"id": 2, "op": "flow.tick"}],
			"edges": [{"kind": "exec", "from": 1, "fromPin": "body", "to": 2},
				{"kind": "exec", "from": 2, "fromPin": "then", "to": 1}]})json"),
			StringView("")},

		{StringView("a required input with nothing to give it"), StringView(R"json({
			"formatVersion": 1,
			"nodes": [{"id": 1, "op": "flow.tick"}, {"id": 2, "op": "sink.print"}],
			"edges": [{"kind": "exec", "from": 1, "fromPin": "then", "to": 2}]})json"),
			StringView("missing-input@2.value unreachable@1")},

		{StringView("an optional input with a default"), StringView(R"json({"formatVersion": 1,
			"nodes": [{"id": 1, "op": "flow.start"}, {"id": 2, "op": "math.addf"}],
			"edges": [{"kind": "data", "from": 1, "fromPin": "value", "to": 2,
				"toPin": "lhs"}]})json"),
			StringView("eager-speculative@1 eager-unused@2")},

		{StringView("a literal that does not fit its pin"), StringView(R"json({"formatVersion": 1,
			"nodes": [{"id": 1, "op": "math.addi", "params": {"lhs": 3.5, "rhs": 1}}],
			"edges": []})json"),
			StringView("constant-invalid@1.lhs")},

		{StringView("a parameter naming no pin at all"), StringView(R"json({"formatVersion": 1,
			"nodes": [{"id": 1, "op": "math.addf", "params": {"lsh": 1.0}}],
			"edges": []})json"),
			StringView("unknown-pin@1.lsh")},

		{StringView("an operation whose signature moved"), StringView(R"json({"formatVersion": 1,
			"nodes": [{"id": 1, "op": "flow.start", "opHash": 4242}], "edges": []})json"),
			StringView("signature-drift@1 eager-unused@1")},

		{StringView("a node nothing can ever start"), StringView(R"json({"formatVersion": 1,
			"nodes": [{"id": 1, "op": "flow.start"}, {"id": 2, "op": "sink.print"}],
			"edges": [{"kind": "data", "from": 1, "fromPin": "value", "to": 2,
				"toPin": "value"}]})json"),
			StringView("unreachable@2 eager-speculative@1")},

		// Several problems at once: an author gets the list, not the first one.
		{StringView("more than one problem"), StringView(R"json({"formatVersion": 1,
			"nodes": [{"id": 1, "op": "math.addi", "params": {"lhs": 1, "rhs": 2}},
				{"id": 2, "op": "sink.print"}, {"id": 3, "op": "nope"}],
			"edges": [{"kind": "data", "from": 1, "fromPin": "result", "to": 2,
				"toPin": "value"}]})json"),
			StringView("unknown-op@3 type-mismatch@1.result->2.value" " missing-input@2.value "
																	  "unreachable@2")},

		// ---- what eagerness costs ------------------------------------------------------------------
		//
		// The interpreter runs a node with no exec input as soon as its inputs exist. That is a
		// strategy an author cannot see in the picture, so the build says where it does work that may
		// not be needed. Advice, never a refusal.

		{StringView("a value only a conditional branch reads"), StringView(R"json({
			"formatVersion": 1,
			"nodes": [{"id": 1, "op": "flow.start"},
				{"id": 2, "op": "flow.branch", "params": {"condition": true}},
				{"id": 3, "op": "sink.print"}],
			"edges": [{"kind": "exec", "from": 1, "fromPin": "then", "to": 2},
				{"kind": "exec", "from": 2, "fromPin": "true", "to": 3},
				{"kind": "data", "from": 1, "fromPin": "value", "to": 3,
					"toPin": "value"}]})json"),
			StringView("eager-speculative@1")},

		// The same graph with the branch taken out: the consumer now certainly runs, the value is
		// certainly needed, and there is nothing to say.
		{StringView("the same value on an unconditional path"), StringView(R"json({
			"formatVersion": 1,
			"nodes": [{"id": 1, "op": "flow.start"}, {"id": 2, "op": "flow.tick"},
				{"id": 3, "op": "sink.print"}],
			"edges": [{"kind": "exec", "from": 1, "fromPin": "then", "to": 2},
				{"kind": "exec", "from": 2, "fromPin": "then", "to": 3},
				{"kind": "data", "from": 1, "fromPin": "value", "to": 3,
					"toPin": "value"}]})json"),
			StringView("")},
	};

	for (auto &c : cases) { check(runCase(ops, c), mem_std::toString("graph-validate: ", c.what)); }

	// ---- advice is not a verdict ---------------------------------------------------------------------

	{
		GraphAsset asset;
		asset.init();
		check(asset.load(data::read<mem_std::Interface>(StringView(R"json({"formatVersion": 1,
			"nodes": [{"id": 1, "op": "bool.out"}, {"id": 2, "op": "int.in"}],
			"edges": [{"kind": "data", "from": 1, "fromPin": "flag", "to": 2,
				"toPin": "n"}]})json")))
						== Status::Ok,
				"graph-validate: the advisory fixture loads");

		RuntimeGraph graph;
		graph.init();
		mem_std::Value report;
		check(graph.build(asset, ops, &report) == Status::Ok && graph.isValid(),
				"graph-validate: a graph with advice builds and is usable");
		check(report.isArray() && report.size() == 2,
				"graph-validate: and the advice is still reported");

		// A graph with errors gets no advice at all: it would be about wiring that does not exist.
		GraphAsset broken;
		broken.init();
		broken.load(data::read<mem_std::Interface>(StringView(R"json({"formatVersion": 1,
			"nodes": [{"id": 1, "op": "bool.out"}, {"id": 2, "op": "does.not.exist"}],
			"edges": []})json")));

		mem_std::Value brokenReport;
		RuntimeGraph brokenGraph;
		brokenGraph.init();
		check(brokenGraph.build(broken, ops, &brokenReport) != Status::Ok,
				"graph-validate: the broken fixture is refused");
		check(StringView(diagLines(brokenReport)) == "unknown-op@2",
				"graph-validate: a refused graph gets no advice about the wiring it does not have");
	}

	// ---- validate() and build() are the same function -----------------------------------------------

	{
		StringView json(R"json({"formatVersion": 1,
			"nodes": [{"id": 1, "op": "math.addi", "params": {"lhs": 1, "rhs": 2}},
				{"id": 2, "op": "sink.print"}],
			"edges": [{"kind": "data", "from": 1, "fromPin": "result", "to": 2,
				"toPin": "value"}]})json");

		GraphAsset asset;
		asset.init();
		check(asset.load(data::read<mem_std::Interface>(json)) == Status::Ok,
				"graph-validate: the comparison fixture loads");

		mem_std::Value fromBuild;
		mem_std::Value fromValidate;

		RuntimeGraph graph;
		graph.init();
		graph.build(asset, ops, &fromBuild);
		RuntimeGraph::validate(asset, ops, &fromValidate);

		check(test::compareValues(fromValidate, fromBuild, StringView("validate vs build")),
				"graph-validate: validate() reports exactly what build() reports");
	}

	// ---- a rejected build leaves nothing behind -----------------------------------------------------

	{
		GraphAsset good;
		GraphAsset bad;
		good.init();
		bad.init();

		good.load(data::read<mem_std::Interface>(StringView(R"json({"formatVersion": 1,
			"nodes": [{"id": 1, "op": "flow.start"}, {"id": 2, "op": "sink.print"}],
			"edges": [{"kind": "exec", "from": 1, "fromPin": "then", "to": 2},
				{"kind": "data", "from": 1, "fromPin": "value", "to": 2,
					"toPin": "value"}]})json")));
		bad.load(data::read<mem_std::Interface>(StringView(R"json({"formatVersion": 1,
			"nodes": [{"id": 1, "op": "does.not.exist"}], "edges": []})json")));

		RuntimeGraph graph;
		graph.init();
		check(graph.build(good, ops) == Status::Ok && graph.isValid() && graph.getNodeCount() == 2,
				"graph-validate: the good graph builds");
		check(graph.build(bad, ops) != Status::Ok && !graph.isValid() && graph.getNodeCount() == 0,
				"graph-validate: a refused build leaves no graph, not a partial one");
	}

	// ---- the identity a literal carries -------------------------------------------------------------
	//
	// An enum's family lives in the VALUE, not in the tag, and decodeVar reads it out of the literal -
	// it is handed a VarType and never sees the pin. So the only place the pin's declared family can be
	// enforced is resolveConstant, and these four cases are the whole of that rule.
	//
	// Built as Values rather than written as JSON on purpose: a TypeId is a hash64 of the family name,
	// and a golden file with one typed out in decimal would be a number nobody could check by reading.

	{
		auto enumLiteral = [](int64_t v, flow::value::TypeId family) {
			mem_std::Value out(mem_std::Value::Type::DICTIONARY);
			out.setInteger(v, "value");
			if (family != flow::value::NullTypeId) {
				out.setInteger(int64_t(family), "type");
			}
			return out;
		};

		auto lines = [&ops](StringView op, mem_std::Value &&param) {
			mem_std::Value params(mem_std::Value::Type::DICTIONARY);
			params.setValue(sprt::move(param), "e");

			mem_std::Value node(mem_std::Value::Type::DICTIONARY);
			node.setInteger(1, "id");
			node.setString(op, "op");
			node.setValue(sprt::move(params), "params");

			mem_std::Value nodes(mem_std::Value::Type::ARRAY);
			nodes.addValue(sprt::move(node));

			mem_std::Value doc(mem_std::Value::Type::DICTIONARY);
			doc.setInteger(1, "formatVersion");
			doc.setValue(sprt::move(nodes), "nodes");
			doc.setValue(mem_std::Value(mem_std::Value::Type::ARRAY), "edges");

			GraphAsset asset;
			asset.init();
			mem_std::String out;
			if (asset.load(doc) != Status::Ok) {
				return mem_std::String("<the fixture does not load>");
			}
			RuntimeGraph graph;
			graph.init();
			mem_std::Value report;
			graph.build(asset, ops, &report);
			return diagLines(report);
		};

		// enum.inB declares family B. A literal of family A on it is the mistake this rule exists for.
		check(StringView(lines(StringView("enum.inB"), enumLiteral(2, familyA())))
						== "constant-invalid@1.e",
				"graph-validate: a literal of another enum family is refused");
		// "eager-unused" is the advice every one of these single-node fixtures draws - the node has no
		// exec input, so it runs on every tick, and nothing reads it. It is here because the expectation
		// is the WHOLE report; what matters is that it is advice and not `constant-invalid`.
		check(StringView(lines(StringView("enum.inB"), enumLiteral(2, familyB()))) == "eager-unused@1",
				"graph-validate: and one of the pin's own family is accepted");

		// Naming no family is not a mistake: the pin's is the only answer there could be, so the
		// literal takes it. That the value comes BACK carrying it is asserted in edit-widgets, which
		// can read the converted value; here the point is only that it is not refused.
		check(StringView(lines(StringView("enum.inB"), enumLiteral(2, flow::value::NullTypeId)))
						== "eager-unused@1",
				"graph-validate: a literal that names no family takes the pin's");

		// enum.inAny declares none, so nothing here has an identity to disagree with.
		check(StringView(lines(StringView("enum.inAny"), enumLiteral(2, familyA()))) == "eager-unused@1",
				"graph-validate: a pin that declares no family accepts any");
	}
}

} // namespace STAPPLER_VERSIONIZED stappler
