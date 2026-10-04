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

// Linking: the operations an interface spells, the hash a call node stores, where each node of a
// linked graph came from, and every refusal a document with functions can earn before anything
// runs.

#include "SPCommon.h"
#include "SPMemory.h"
#include "SPData.h"

#include "SPFlowOps.h"
#include "SPFlowRuntime.h"

#include "../tests.h"
#include "../check/flow_check.h"

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;

namespace {

using namespace flow;

using stappler::test::hasDiag;

struct Library : FunctionHost {
	mem_std::Vector<GraphAsset *> docs;

	~Library() {
		for (auto it : docs) { delete it; }
	}

	bool add(StringView json) {
		auto asset = new GraphAsset();
		asset->init();
		if (asset->load(data::read<mem_std::Interface>(json)) != Status::Ok) {
			delete asset;
			return false;
		}
		docs.emplace_back(asset);
		return true;
	}

	const GraphAsset *findFunction(StringView name) const override {
		for (auto it : docs) {
			if (it->getName() == name) {
				return it;
			}
		}
		return nullptr;
	}
};

struct Linked {
	OpRegistry ops;
	GraphAsset asset;
	RuntimeGraph graph;
	mem_std::Value report;
	Status status = Status::ErrorInvalidArguemnt;

	Linked(StringView json, const FunctionHost *host = nullptr) {
		ops.init();
		flow::ops::registerCoreOps(ops);
		asset.init();
		if (asset.load(data::read<mem_std::Interface>(json), &report) != Status::Ok) {
			sprt::cout << "       asset: " << data::toString<mem_std::Interface>(report, true)
					   << "\n";
			return;
		}
		graph.init();
		graph.setFunctionHost(host);
		status = graph.build(asset, ops, &report);
	}

	bool ok() const { return status == Status::Ok; }
	bool has(StringView code) const { return hasDiag(report, code); }
	void dump() const {
		sprt::cout << "       report: " << data::toString<mem_std::Interface>(report, true) << "\n";
	}
};

#define DOC(BODY) R"json({"__meta": {"kind": "graph", "version": 2}, "name": "main", )json" BODY "}"

// An exec function with one input, one output and one exit.
#define INC_FN(NAME, NODES, EDGES) R"json({"name": ")json" NAME R"json(",
		"interface": {"inputs": [{"name": "x", "type": "int"}],
			"outputs": [{"name": "y", "type": "int"}], "execIn": true, "execOut": ["done"]},
		"nodes": )json" NODES R"json(, "edges": )json" EDGES "}"

#define INC_BODY R"json([{"id": 10, "op": "fn.entry"}, {"id": 11, "op": "math.addInt", "params": {"rhs": 1}},
			{"id": 12, "op": "fn.return"}])json"
#define INC_EDGES R"json([{"kind": "exec", "from": 10, "fromPin": "start", "to": 12},
			{"kind": "data", "from": 10, "fromPin": "x", "to": 11, "toPin": "lhs"},
			{"kind": "data", "from": 11, "fromPin": "result", "to": 12, "toPin": "y"}])json"

constexpr StringView IncLibrary(R"json({"__meta": {"kind": "graph", "version": 2}, "name": "inc",
	"interface": {"inputs": [{"name": "x", "type": "int"}], "outputs": [{"name": "y", "type": "int"}],
		"execIn": true, "execOut": ["done"]},
	"scene": [{"component": "game.Unit", "fields": [{"name": "hp", "type": "int"}]}],
	"nodes": [{"id": 1, "op": "fn.entry"}, {"id": 2, "op": "math.addInt", "params": {"rhs": 1}},
		{"id": 3, "op": "fn.return"}],
	"edges": [{"kind": "exec", "from": 1, "fromPin": "start", "to": 3},
		{"kind": "data", "from": 1, "fromPin": "x", "to": 2, "toPin": "lhs"},
		{"kind": "data", "from": 2, "fromPin": "result", "to": 3, "toPin": "y"}]
})json");

} // namespace

void performGraphFunctionLinkTests() {
	sprt::cout << "\n== flow graph: linking functions ==\n";

	// ---- the signatures ------------------------------------------------------------------------

	{
		FunctionInterface a;
		a.inputs.emplace_back(FunctionPin{.name = StringView("x"), .type = VarType::Int});
		a.outputs.emplace_back(FunctionPin{.name = StringView("y"), .type = VarType::Int});
		a.execIn = true;
		a.execOut.emplace_back(StringView("done"));

		FunctionInterface b;
		b.inputs.emplace_back(FunctionPin{.name = StringView("x"), .type = VarType::Int});
		b.outputs.emplace_back(FunctionPin{.name = StringView("y"), .type = VarType::Int});
		b.execIn = true;
		b.execOut.emplace_back(StringView("done"));
		b.mode = FunctionMode::Inline;

		auto ha = getFunctionCallHash(a, StringView("inc"));
		check(ha != 0 && ha == getFunctionCallHash(b, StringView("inc")),
				"graph-function-link: the mode is not part of a call's signature");
		check(ha != getFunctionCallHash(a, StringView("dec")),
				"graph-function-link: the name is");
		b.inputs[0].type = VarType::Float;
		check(ha != getFunctionCallHash(b, StringView("inc")),
				"graph-function-link: and so is the type of a pin");

		FunctionSignature ret;
		ret.init(a, StringView("fn.return"), StringView("inc"), FunctionOpKind::SourceReturn);
		check(ret.getDef().settings.size() == 1 && ret.getDef().settings[0].choices.size() == 1
						&& ret.getDef().dataIn.size() == 1 && ret.getDef().hasExecIn,
				"graph-function-link: a return as an editor shows it names its exit as a setting");

		FunctionSignature entry;
		entry.init(a, StringView("fn.entry"), StringView("inc"), FunctionOpKind::SourceEntry);
		check(entry.getDef().dataIn.size() == 1 && entry.getDef().dataOut.size() == 1
						&& !entry.getDef().hasExecIn && entry.getDef().execOut.size() == 1,
				"graph-function-link: an entry hands out the inputs and starts the body");
	}

	// ---- what needs a link at all ------------------------------------------------------------------

	{
		Linked plain(StringView(R"json({"formatVersion": 1, "nodes": [{"id": 1, "op": "flow.event"}],
			"edges": []})json"));
		check(plain.ok() && plain.graph.getLink() == nullptr
						&& plain.graph.getBuiltAsset() == &plain.asset,
				"graph-function-link: a graph without functions is built as it stands");
		check(!GraphLink::needsLink(plain.asset), "graph-function-link: and needs no link");
	}

	// ---- origins -------------------------------------------------------------------------------

	{
		Library lib;
		check(lib.add(IncLibrary), "graph-function-link: the library loads");
		Linked l(StringView(DOC(R"json("nodes": [{"id": 1, "op": "flow.event"},
				{"id": 2, "op": "fn.inc", "params": {"x": 1}},
				{"id": 3, "op": "fn.local", "params": {"x": 1}, "settings": {"mode": "inline"}}],
			"edges": [{"kind": "exec", "from": 1, "fromPin": "then", "to": 2},
				{"kind": "exec", "from": 2, "fromPin": "done", "to": 3}],
			"functions": [)json" INC_FN("local", INC_BODY, INC_EDGES) "]")),
				&lib);
		if (!l.ok()) {
			l.dump();
		}
		check(l.ok(), "graph-function-link: a library call and an inline local site build");

		auto link = l.graph.getLink();
		check(link && link->getCallees().size() == 1
						&& StringView(link->getCallees()[0].name) == "inc"
						&& link->getCallees()[0].contentHash == lib.docs[0]->getContentHash(),
				"graph-function-link: the library document is a callee, with its content hash");

		auto root = link ? link->getOrigin(2) : nullptr;
		check(root && StringView(root->document) == "main" && root->source == 2
						&& root->function.empty() && root->callSite == NullNodeId,
				"graph-function-link: a node of the document keeps its id");

		auto local = link ? link->getOrigin(11) : nullptr;
		check(local && StringView(local->function) == "local" && local->source == 11,
				"graph-function-link: so does a node of a function it defines");

		// The site's copy: ids 13.. after the document's 12, the call node it stands for named.
		bool copy = false;
		bool libBody = false;
		for (NodeId id = 13; id < 20; ++id) {
			if (auto o = link ? link->getOrigin(id) : nullptr) {
				if (StringView(o->function) == "local" && o->callSite == 3 && o->source == 11) {
					copy = true;
				}
				if (StringView(o->document) == "inc" && o->source == 2) {
					libBody = true;
				}
			}
		}
		check(copy, "graph-function-link: a substituted copy names the call it stands for");
		check(libBody, "graph-function-link: a library body takes fresh ids, traced to its document");
		GraphShape shape;
		RuntimeGraph::validate(l.asset, l.ops, nullptr, &shape, &lib);
		bool shaped = !shape.origins.empty();
		for (auto &o : shape.origins) {
			if (o.document == "main" && o.function.empty() && o.callSite == NullNodeId) {
				shaped = false; // a node of the document's own is not listed
			}
		}
		check(shaped, "graph-function-link: an editor's validation hands back the origins of the "
				"ids that are not the document's own");

		check(l.graph.findNode(3) == InvalidIndex,
				"graph-function-link: the inline call node itself is gone from the graph");

		check(l.graph.getScopeCount() == 3,
				"graph-function-link: the graph, plus a scope per called body");
		auto call = l.graph.findNode(2);
		check(call != InvalidIndex && l.graph.getNodeAt(call).opensScope != InvalidIndex
						&& l.graph.getScopeAt(l.graph.getNodeAt(call).opensScope).kind
								== ScopeKind::Function,
				"graph-function-link: a call node opens its function's scope");
	}

	// ---- refusals ------------------------------------------------------------------------------

	struct BadCase {
		StringView json;
		StringView code;
		StringView what;
		bool refused = true;
	};

	Library lib;
	lib.add(IncLibrary);
	lib.add(StringView(R"json({"__meta": {"kind": "graph", "version": 2}, "name": "other",
		"interface": {},
		"scene": [{"component": "game.Unit", "fields": [{"name": "hp", "type": "float"}]}],
		"nodes": [{"id": 1, "op": "fn.entry"}], "edges": []})json"));

	const BadCase cases[] = {
		{StringView(DOC(R"json("nodes": [{"id": 1, "op": "fn.nothing"}], "edges": [])json")),
			StringView("fn-unknown"), StringView("a call of a function nobody defines")},
		{StringView(DOC(R"json("nodes": [{"id": 1, "op": "fn.entry"}], "edges": [])json")),
			StringView("fn-boundary"), StringView("an entry in a graph that is no function")},
		{StringView(DOC(R"json("nodes": [], "edges": [],
			"functions": [)json" INC_FN("f", R"json([{"id": 11, "op": "math.addInt"},
				{"id": 12, "op": "fn.return"}])json", "[]") "]")),
			StringView("fn-boundary"), StringView("a function with no entry")},
		{StringView(DOC(R"json("nodes": [], "edges": [],
			"functions": [)json" INC_FN("f", R"json([{"id": 10, "op": "fn.entry"},
				{"id": 11, "op": "fn.entry"}, {"id": 12, "op": "fn.return"}])json", "[]") "]")),
			StringView("fn-boundary"), StringView("a function with two entries")},
		{StringView(DOC(R"json("nodes": [], "edges": [],
			"functions": [)json" INC_FN("f", R"json([{"id": 10, "op": "fn.entry"}])json", "[]") "]")),
			StringView("fn-boundary"), StringView("a function with an output and no return")},
		{StringView(DOC(R"json("nodes": [], "edges": [],
			"functions": [)json" INC_FN("f", INC_BODY, R"json([{"kind": "data", "from": 11,
				"fromPin": "result", "to": 10, "toPin": "x"}])json") "]")),
			StringView("fn-boundary"), StringView("an edge into an entry")},
		{StringView(DOC(R"json("nodes": [], "edges": [], "functions": [{"name": "p",
				"interface": {"outputs": [{"name": "y", "type": "int"}]},
				"nodes": [{"id": 10, "op": "fn.entry"}, {"id": 11, "op": "fn.return"},
					{"id": 12, "op": "fn.return"}], "edges": []}])json")),
			StringView("fn-boundary"), StringView("a pure function with two returns")},
		{StringView(DOC(R"json("nodes": [], "edges": [], "functions": [)json" INC_FN("f",
				R"json([{"id": 10, "op": "fn.entry"}, {"id": 12, "op": "fn.return",
				"settings": {"exit": "elsewhere"}}])json", "[]") "]")),
			StringView("setting-invalid"), StringView("a return through an exit the function lacks")},
		{StringView(DOC(R"json("nodes": [{"id": 1, "op": "fn.f", "settings": {"mode": "turbo"}}],
				"edges": [], "functions": [)json" INC_FN("f", INC_BODY, INC_EDGES) "]")),
			StringView("setting-invalid"), StringView("a call in a mode that does not exist")},
		{StringView(DOC(R"json("nodes": [{"id": 1, "op": "fn.f", "settings": {"mode": "inline"}}],
				"edges": [], "functions": [)json" INC_FN("f", R"json([{"id": 10, "op": "fn.entry"},
				{"id": 11, "op": "fn.f", "settings": {"mode": "inline"}},
				{"id": 12, "op": "fn.return"}])json", "[]") "]")),
			StringView("fn-inline-cycle"), StringView("a function substituting itself")},
		{StringView(DOC(R"json("nodes": [{"id": 1, "op": "fn.f", "settings": {"mode": "inline"}}],
				"edges": [], "functions": [)json" INC_FN("f", R"json([{"id": 10, "op": "fn.entry"},
				{"id": 11, "op": "fn.return"}, {"id": 12, "op": "fn.return"}])json", "[]") "]")),
			StringView("fn-inline-multi-return"),
			StringView("an inline site of a body with two returns and an output")},
		{StringView(DOC(R"json("nodes": [], "edges": [], "functions": [{"name": "d",
				"interface": {"inputs": [{"name": "x", "type": "int", "default": "many"}]},
				"nodes": [{"id": 10, "op": "fn.entry"}], "edges": []}])json")),
			StringView("fn-interface-invalid"), StringView("an interface default of the wrong type")},
		{StringView(DOC(R"json("nodes": [], "edges": [], "functions": [{"name": "entry",
				"interface": {}, "nodes": [], "edges": []}])json")),
			StringView("fn-interface-invalid"), StringView("a function named like a boundary node")},
		{StringView(DOC(R"json("nodes": [{"id": 1, "op": "flow.event"}], "edges": [],
				"functions": [)json" INC_FN("f", INC_BODY, R"json([{"kind": "exec", "from": 1,
				"fromPin": "then", "to": 12}])json") "]")),
			StringView("unknown-node"), StringView("an edge from one body into another")},
		{StringView(DOC(R"json("scene": [{"component": "game.Unit", "fields": [{"name": "hp",
				"type": "int"}]}], "nodes": [{"id": 1, "op": "fn.inc"}, {"id": 2, "op": "fn.other"}],
				"edges": [])json")),
			StringView("fn-decl-conflict"),
			StringView("two documents declaring one field with two types")},
	};

	for (auto &c : cases) {
		Linked l(c.json, &lib);
		auto refused = !l.ok() && l.has(c.code);
		if (!refused) {
			l.dump();
		}
		check(refused, mem_std::toString("graph-function-link: ", c.what, " is refused"));
		check(l.graph.getNodeCount() == 0 && !l.graph.isValid(),
				mem_std::toString("graph-function-link: ", c.what, " leaves nothing built"));
	}

	// ---- warnings ------------------------------------------------------------------------------

	{
		Linked l(StringView(DOC(R"json("nodes": [{"id": 1, "op": "fn.inc", "params": {"x": 1}}],
				"edges": [], "functions": [)json" INC_FN("inc", INC_BODY, INC_EDGES) "]")),
				&lib);
		check(l.ok() && l.has(StringView("fn-shadowed")),
				"graph-function-link: a function hiding a library one builds, with a warning");
	}
	{
		Linked l(StringView(DOC(R"json("nodes": [{"id": 1, "op": "fn.inc", "opHash": 12345,
				"params": {"x": 1}}], "edges": [])json")),
				&lib);
		check(l.ok() && l.has(StringView("signature-drift")),
				"graph-function-link: a call written against another interface drifts");
	}
	{
		auto hash = getFunctionCallHash(lib.docs[0]->getInterface(), StringView("inc"));
		auto json = mem_std::toString(R"json({"__meta": {"kind": "graph", "version": 1},
			"nodes": [{"id": 1, "op": "fn.inc", "opHash": )json", int64_t(hash), R"json(,
			"params": {"x": 1}}], "edges": []})json");
		Linked l(StringView(json), &lib);
		check(l.ok() && !l.has(StringView("signature-drift")),
				"graph-function-link: and one written against this one does not");
	}
}

} // namespace stappler
