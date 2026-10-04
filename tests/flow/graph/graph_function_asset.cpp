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

// The function sections of the graph asset: a document that is a function (`interface`) and a
// document that defines functions of its own (`functions`). The same three properties as the root
// body - transactional load, canonical order, a byte-identical round-trip - plus the two this format
// adds: one id numbering across every body of the file, and a version that rises only for a file
// that uses either section.

#include "SPCommon.h"
#include "SPMemory.h"

#include "SPFlowAsset.h"

#include "../tests.h"
#include "../check/flow_check.h"

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;

namespace {

using namespace flow;

using stappler::test::hasDiag;

// A library function with every interface field filled in, and a local function listed out of
// name order with its nodes and edges out of canonical order.
StringView functionGraph() {
	return StringView(R"json({
		"__meta": {"kind": "graph", "version": 2},
		"name": "lib.clamp",
		"interface": {
			"inputs": [
				{"name": "value", "type": "float", "required": true},
				{"name": "limits", "type": "array", "element": ["float"], "default": [0.0, 1.0]},
				{"name": "kind", "type": "enum", "subtype": "game.Kind"}
			],
			"outputs": [{"name": "result", "type": "float"}],
			"execIn": true,
			"execOut": ["clamped", "inside"],
			"mode": "inline"
		},
		"nodes": [
			{"id": 2, "op": "fn.return", "settings": {"exit": "inside"}},
			{"id": 1, "op": "fn.entry"}
		],
		"edges": [
			{"kind": "exec", "from": 1, "fromPin": "start", "to": 2},
			{"kind": "data", "from": 1, "fromPin": "value", "to": 2, "toPin": "result"}
		],
		"functions": [
			{"name": "zeta", "interface": {}, "nodes": [{"id": 9, "op": "fn.entry"}], "edges": []},
			{"name": "alpha",
				"interface": {"inputs": [{"name": "x", "type": "int"}],
					"outputs": [{"name": "y", "type": "int"}]},
				"meta": {"editor": {"viewport": [0, 0, 1]}},
				"nodes": [
					{"id": 7, "op": "fn.return"},
					{"id": 5, "op": "fn.entry"},
					{"id": 6, "op": "math.addInt", "params": {"b": 1}}
				],
				"edges": [
					{"kind": "data", "from": 6, "fromPin": "result", "to": 7, "toPin": "y"},
					{"kind": "data", "from": 5, "fromPin": "x", "to": 6, "toPin": "a"}
				]}
		]
	})json");
}

bool roundTrip(StringView json, StringView label) {
	GraphAsset first;
	first.init();

	mem_std::Value diag;
	if (first.load(data::read<mem_std::Interface>(json), &diag) != Status::Ok) {
		sprt::cout << "       " << label
				   << ": load refused: " << data::toString<mem_std::Interface>(diag, true) << "\n";
		return false;
	}

	mem_std::Value once;
	first.save(once);

	GraphAsset second;
	second.init();
	if (second.load(once, &diag) != Status::Ok) {
		sprt::cout << "       " << label
				   << ": reload refused: " << data::toString<mem_std::Interface>(diag, true)
				   << "\n";
		return false;
	}

	mem_std::Value twice;
	second.save(twice);

	auto cborA = data::write<mem_std::Interface>(once, data::EncodeFormat::Cbor);
	auto cborB = data::write<mem_std::Interface>(twice, data::EncodeFormat::Cbor);
	auto jsonA = data::toString<mem_std::Interface>(once, false);
	auto jsonB = data::toString<mem_std::Interface>(twice, false);

	bool ok = test::compareBytes(BytesView(cborA.data(), cborA.size()),
			BytesView(cborB.data(), cborB.size()), label);
	if (StringView(jsonA) != StringView(jsonB)) {
		sprt::cout << "       " << label << ": JSON differs\n  " << jsonA << "\n  " << jsonB
				   << "\n";
		ok = false;
	}
	return ok;
}

} // namespace

void performGraphFunctionAssetTests() {
	sprt::cout << "\n== flow graph: functions in the graph asset ==\n";

	check(roundTrip(functionGraph(), StringView("functions")),
			"graph-function-asset: save -> load -> save is byte-identical with both sections");

	{
		GraphAsset asset;
		asset.init();
		mem_std::Value diag;
		auto loaded = asset.load(data::read<mem_std::Interface>(functionGraph()), &diag) == Status::Ok;
		check(loaded, "graph-function-asset: the sample loads");
		if (!loaded) {
			sprt::cout << "       " << data::toString<mem_std::Interface>(diag, true) << "\n";
			return;
		}

		check(asset.hasInterface(), "graph-function-asset: a document with an interface is a function");
		auto &iface = asset.getInterface();
		check(iface.inputs.size() == 3 && StringView(iface.inputs[0].name) == "value"
						&& StringView(iface.inputs[1].name) == "limits"
						&& StringView(iface.inputs[2].name) == "kind",
				"graph-function-asset: interface pins keep the order they were written in");
		check(iface.inputs[0].type == VarType::Float && iface.inputs[0].required,
				"graph-function-asset: a pin keeps its type and its required flag");
		check(iface.inputs[1].type == VarType::Array
						&& iface.inputs[1].element == value::makeChain(VarType::Float)
						&& iface.inputs[1].def.isArray() && iface.inputs[1].def.size() == 2,
				"graph-function-asset: a container pin keeps its element chain and its default");
		check(StringView(iface.inputs[2].subtype) == "game.Kind",
				"graph-function-asset: a subtype is kept by name");
		check(iface.execIn && iface.execOut.size() == 2 && StringView(iface.execOut[0]) == "clamped",
				"graph-function-asset: exec pins keep their order");
		check(iface.mode == FunctionMode::Inline, "graph-function-asset: the default mode is read");

		auto fns = asset.getFunctions();
		check(fns.size() == 2 && StringView(fns[0].name) == "alpha"
						&& StringView(fns[1].name) == "zeta",
				"graph-function-asset: functions come back in name order");
		check(fns[0].nodes.size() == 3 && fns[0].nodes[0].id == 5 && fns[0].nodes[2].id == 7,
				"graph-function-asset: a function's nodes come back in id order");
		check(fns[0].edges.size() == 2 && fns[0].edges[0].to == 6 && fns[0].edges[1].to == 7,
				"graph-function-asset: a function's edges come back in canonical order");
		check(fns[1].iface.inputs.empty() && !fns[1].iface.execIn
						&& fns[1].iface.mode == FunctionMode::Call,
				"graph-function-asset: an empty interface is a function with no pins");
		check(asset.getFunction(StringView("alpha")) == &fns[0]
						&& asset.getFunction(StringView("beta")) == nullptr,
				"graph-function-asset: a function is found by name");

		const GraphFunction *body = nullptr;
		check(asset.findNode(6, &body) != nullptr && body == &fns[0],
				"graph-function-asset: a node of a function is found with its body");
		check(asset.findNode(2, &body) != nullptr && body == nullptr,
				"graph-function-asset: a node of the root body is found with no body");
		check(asset.getNode(6) == nullptr,
				"graph-function-asset: getNode answers for the root body only");

		mem_std::Value saved;
		asset.save(saved);
		check(saved.getValue(GraphAsset::MetaKey).getInteger("version")
						== int64_t(GraphAsset::FunctionFormatVersion),
				"graph-function-asset: a document with functions is written as version 2");
		check(saved.getValue("interface").getString("mode") == "inline"
						&& saved.getValue("interface").getValue("inputs").getValue(0).isNull("default"),
				"graph-function-asset: an absent default stays absent");
		check(saved.getValue("functions").getValue(1).getValue("interface").isDictionary()
						&& saved.getValue("functions").getValue(1).getValue("interface").size() == 0,
				"graph-function-asset: an empty interface is still written");
	}

	{
		// A plain graph is still written as version 1: the version rises only with the sections.
		GraphAsset asset;
		asset.init();
		check(asset.load(data::read<mem_std::Interface>(StringView(
					R"json({"__meta": {"kind": "graph", "version": 1}, "nodes": [], "edges": []})json")))
						== Status::Ok,
				"graph-function-asset: a plain graph loads");
		mem_std::Value saved;
		asset.save(saved);
		check(saved.getValue(GraphAsset::MetaKey).getInteger("version") == 1
						&& saved.isNull("interface") && saved.isNull("functions"),
				"graph-function-asset: a plain graph is written as version 1, with neither section");
		check(!asset.hasInterface() && asset.getFunctions().empty(),
				"graph-function-asset: a plain graph is no function and defines none");
	}

	{
		GraphAsset asset;
		asset.init();
		check(asset.load(data::read<mem_std::Interface>(functionGraph())) == Status::Ok,
				"graph-function-asset: a good document loads first");

		struct BadCase {
			StringView json;
			StringView code;
			StringView what;
		};

#define FN_DOC(BODY) R"json({"__meta": {"kind": "graph", "version": 2}, "nodes": [], "edges": [], )json" BODY "}"

		const BadCase cases[] = {
			{StringView(FN_DOC(R"json("interface": 3)json")), StringView("asset-malformed"),
				StringView("an interface that is not a dictionary")},
			{StringView(FN_DOC(R"json("interface": {"inputs": {}})json")),
				StringView("asset-malformed"), StringView("inputs that are not an array")},
			{StringView(FN_DOC(R"json("interface": {"inputs": [{"type": "int"}]})json")),
				StringView("asset-malformed"), StringView("a pin with no name")},
			{StringView(FN_DOC(
					 R"json("interface": {"outputs": [{"name": "a", "type": "int"}, {"name": "a", "type": "float"}]})json")),
				StringView("asset-malformed"), StringView("two pins of one name")},
			{StringView(FN_DOC(R"json("interface": {"inputs": [{"name": "a", "type": "integer"}]})json")),
				StringView("asset-malformed"), StringView("a pin of an unknown type")},
			{StringView(FN_DOC(
					 R"json("interface": {"inputs": [{"name": "a", "type": "array", "element": ["Thing"]}]})json")),
				StringView("asset-malformed"), StringView("a pin with an unknown element type")},
			{StringView(FN_DOC(
					 R"json("interface": {"inputs": [{"name": "a", "type": "int", "width": 4}]})json")),
				StringView("asset-unknown-key"), StringView("an unknown pin key")},
			{StringView(FN_DOC(R"json("interface": {"execOut": ["a", "a"]})json")),
				StringView("asset-malformed"), StringView("two exec outputs of one name")},
			{StringView(FN_DOC(R"json("interface": {"mode": "macro"})json")),
				StringView("asset-malformed"), StringView("an unknown mode")},
			{StringView(FN_DOC(R"json("interface": {"outlets": []})json")),
				StringView("asset-unknown-key"), StringView("an unknown interface key")},
			{StringView(FN_DOC(R"json("functions": {})json")), StringView("asset-malformed"),
				StringView("functions that are not an array")},
			{StringView(FN_DOC(R"json("functions": [3])json")), StringView("asset-malformed"),
				StringView("a function that is not a dictionary")},
			{StringView(FN_DOC(R"json("functions": [{"nodes": [], "edges": []}])json")),
				StringView("asset-malformed"), StringView("a function with no name")},
			{StringView(FN_DOC(
					 R"json("functions": [{"name": "f", "nodes": [], "edges": []}, {"name": "f", "nodes": [], "edges": []}])json")),
				StringView("asset-malformed"), StringView("two functions of one name")},
			{StringView(FN_DOC(R"json("functions": [{"name": "f", "edges": []}])json")),
				StringView("asset-malformed"), StringView("a function with no nodes")},
			{StringView(FN_DOC(
					 R"json("functions": [{"name": "f", "nodes": [], "edges": [], "owner": "x"}])json")),
				StringView("asset-unknown-key"), StringView("an unknown function key")},
			{StringView(R"json({"__meta": {"kind": "graph", "version": 2},
					"nodes": [{"id": 4, "op": "a"}], "edges": [],
					"functions": [{"name": "f", "nodes": [{"id": 4, "op": "b"}], "edges": []}]})json"),
				StringView("node-id-duplicate"),
				StringView("a function node with the id of a root node")},
			{StringView(R"json({"__meta": {"kind": "graph", "version": 2},
					"nodes": [], "edges": [],
					"functions": [{"name": "f", "nodes": [{"id": 4, "op": "b"}], "edges": []},
						{"name": "g", "nodes": [{"id": 4, "op": "b"}], "edges": []}]})json"),
				StringView("node-id-duplicate"), StringView("two functions sharing a node id")},
			{StringView(R"json({"__meta": {"kind": "graph", "version": 2},
					"nodes": [], "edges": [],
					"functions": [{"name": "f", "nodes": [{"id": 4}], "edges": []}]})json"),
				StringView("asset-malformed"), StringView("a function node with no operation")},
		};

#undef FN_DOC

		for (auto &c : cases) {
			mem_std::Value diag;
			auto st = asset.load(data::read<mem_std::Interface>(c.json), &diag);
			check(st != Status::Ok && hasDiag(diag, c.code),
					mem_std::toString("graph-function-asset: ", c.what, " is refused"));
		}

		check(asset.hasInterface() && asset.getFunctions().size() == 2
						&& StringView(asset.getName()) == "lib.clamp",
				"graph-function-asset: a rejected load leaves the previous document exactly as it was");
	}
}

} // namespace stappler
