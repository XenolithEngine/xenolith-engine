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

// Subtask E1: the graph asset and its round-trip.
//
// The property this section exists for is `save -> load -> save is byte-identical`, and it is worth
// being precise about why it is not trivially true. Two things could break it:
//
//   * a key the format does not know. Dropping it silently would make the property hold for files
//     this code wrote and fail for files anyone edited by hand, so an unknown key is an error.
//   * the order of the records. A file may list its nodes in any order, and loading CANONICALIZES
//     them - ascending id for nodes, endpoint order for edges. That is what makes two files
//     differing only in order build into the same graph and execute identically, and the
//     round-trip is the cheapest place to notice if it ever stops happening.
//
// The editor's `meta` is the deliberate exception: an opaque dictionary carried through untouched.
// Nothing reads it - not validation, not the build, not the interpreter.

#include "SPCommon.h"
#include "SPMemory.h"

#include "SPFlowAsset.h"

#include "../tests.h"
#include "../check/flow_check.h"

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;

namespace {

using namespace flow;

// The one reader of a report entry's code, for every section (check/studio_check.h).
using stappler::test::hasDiag;

// A graph with every optional part filled in: a name, graph meta, node meta, parameters of several
// value types, node settings, an operation hash, both kinds of edge, and a node with no incoming
// anything.
StringView sampleGraph() {
	return StringView(R"json({
		"formatVersion": 1,
		"name": "system.movement",
		"meta": {"author": "test", "grid": [16, 16]},
		"nodes": [
			{"id": 3, "op": "math.add", "opHash": 12345,
				"params": {"rhs": 2.5, "label": "speed", "on": true},
				"meta": {"x": 40, "y": 120}},
			{"id": 1, "op": "flow.start", "meta": {"x": 0, "y": 0}},
			{"id": 2, "op": "flow.branch", "params": {"condition": false},
				"settings": {"mode": "exact", "with": ["Unit"]}}
		],
		"edges": [
			{"kind": "exec", "from": 2, "fromPin": "false", "to": 3},
			{"kind": "data", "from": 3, "fromPin": "result", "to": 2, "toPin": "condition"},
			{"kind": "exec", "from": 1, "fromPin": "then", "to": 2},
			{"kind": "exec", "from": 2, "fromPin": "true", "to": 3}
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

	// Both encodings, because they fail differently: CBOR is where a double or an integer width
	// would drift, JSON is where key order and number formatting would.
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

void performGraphAssetTests() {
	sprt::cout << "\n== flow graph: the graph asset ==\n";

	// ---- the round-trip ---------------------------------------------------------------------------

	check(roundTrip(sampleGraph(), StringView("sample")),
			"graph-asset: save -> load -> save is byte-identical");

	check(roundTrip(StringView(R"json({"formatVersion": 1, "nodes": [], "edges": []})json"),
				  StringView("empty")),
			"graph-asset: an empty graph round-trips");

	check(roundTrip(StringView(R"json({"formatVersion": 1, "name": "one",
				"nodes": [{"id": 4294967295, "op": "op.only"}], "edges": []})json"),
				  StringView("max-id")),
			"graph-asset: the largest node id round-trips");

	// ---- the envelope -----------------------------------------------------------------------------
	//
	// A graph announces itself in `__meta`. What matters here is that the ASSET acts on it: it refuses a file
	// that says it is something else, it still reads a file written before the standard, and what it
	// writes is always the standard.

	{
		auto envelope = StringView(R"json({"__meta": {"kind": "graph", "version": 1},
				"nodes": [{"id": 1, "op": "op.only"}], "edges": []})json");

		GraphAsset asset;
		asset.init();
		mem_std::Value diag;
		check(asset.load(data::read<mem_std::Interface>(envelope), &diag) == Status::Ok
						&& !asset.isLegacyFormat(),
				"graph-asset: a file with the envelope loads and is not legacy");

		mem_std::Value saved;
		asset.save(saved);
		check(saved.getValue(GraphAsset::MetaKey).getString("kind") == GraphAsset::Kind
						&& saved.getValue(GraphAsset::MetaKey).getInteger("version") == 1
						&& saved.getValue("formatVersion").isNull(),
				"graph-asset: save writes the envelope and not formatVersion");

		check(roundTrip(envelope, StringView("envelope")),
				"graph-asset: a file with the envelope round-trips");
	}

	{
		// Rule 4: the file predates the standard. It loads, it says so, and its save is the standard
		// - which is the whole of the migration.
		GraphAsset asset;
		asset.init();
		check(asset.load(data::read<mem_std::Interface>(
					  StringView(R"json({"formatVersion": 1, "nodes": [], "edges": []})json")))
								== Status::Ok
						&& asset.isLegacyFormat(),
				"graph-asset: a file with no envelope loads and reports itself legacy");

		mem_std::Value saved;
		asset.save(saved);
		check(!saved.getValue(GraphAsset::MetaKey).isNull(),
				"graph-asset: saving a legacy file upgrades it");

		// And the flag belongs to the file that was loaded, not to the asset for ever.
		check(asset.load(data::read<mem_std::Interface>(StringView(
					  R"json({"__meta": {"kind": "graph", "version": 1},)json" R"json( "nodes": [], "edges": []})json")))
								== Status::Ok
						&& !asset.isLegacyFormat(),
				"graph-asset: loading a standard file clears the legacy flag");
	}

	{
		struct Case {
			StringView json;
			StringView code;
			StringView name;
		};

		const Case cases[] = {
			{StringView(
					 R"json({"__meta": {"kind": "scene", "version": 2},)json" R"json( "nodes": [], "edges": []})json"),
				StringView("asset-malformed"),
				StringView("graph-asset: a file that says it is a scene is refused as a graph")},
			{StringView(
					 R"json({"__meta": {"kind": "graph", "version": 3},)json" R"json( "nodes": [], "edges": []})json"),
				StringView("asset-malformed"),
				StringView("graph-asset: a graph from a newer studio is refused")},
			{StringView(
					 R"json({"__meta": {"kind": "graph", "version": 1}, "formatVersion": 2,)json" R"json( "nodes": [], "edges": []})json"),
				StringView("asset-malformed"),
				StringView("graph-asset: formatVersion disagreeing with the envelope is refused")},
			{StringView(R"json({"__meta": {"version": 1}, "nodes": [], "edges": []})json"),
				StringView("asset-malformed"),
				StringView("graph-asset: an envelope naming no kind is refused")},
		};

		for (auto &c : cases) {
			GraphAsset asset;
			asset.init();
			mem_std::Value diag;
			check(asset.load(data::read<mem_std::Interface>(c.json), &diag) != Status::Ok
							&& hasDiag(diag, c.code),
					c.name);
		}
	}

	{
		// The envelope agreeing with a legacy key is an ordinary file, not a conflict: a hand-written
		// asset may carry both while a project is being migrated.
		GraphAsset asset;
		asset.init();
		check(asset.load(data::read<mem_std::Interface>(StringView(
					  R"json({"__meta": {"kind": "graph", "version": 1},)json" R"json( "formatVersion": 1, "nodes": [], "edges": []})json")))
						== Status::Ok,
				"graph-asset: an envelope beside an agreeing formatVersion is accepted");
	}

	// ---- canonicalization -------------------------------------------------------------------------

	{
		GraphAsset asset;
		asset.init();
		check(asset.load(data::read<mem_std::Interface>(sampleGraph())) == Status::Ok,
				"graph-asset: the sample loads");

		auto nodes = asset.getNodes();
		check(nodes.size() == 3 && nodes[0].id == 1 && nodes[1].id == 2 && nodes[2].id == 3,
				"graph-asset: nodes come back in ascending id order, not file order");

		auto edges = asset.getEdges();
		check(edges.size() == 4, "graph-asset: every edge is kept");
		check(edges[0].kind == EdgeKind::Data && edges[1].kind == EdgeKind::Exec
						&& edges[2].kind == EdgeKind::Exec && edges[3].kind == EdgeKind::Exec,
				"graph-asset: data edges sort before exec edges");
		// Exec edges are keyed by the output they leave: (1,"then"), (2,"false"), (2,"true").
		check(edges[1].from == 1 && edges[2].from == 2 && edges[2].fromPin == "false"
						&& edges[3].fromPin == "true",
				"graph-asset: exec edges sort by the pin they leave");

		check(asset.getNode(2) != nullptr && asset.getNode(2)->op == "flow.branch",
				"graph-asset: a node is found by id");
		check(asset.getNode(99) == nullptr, "graph-asset: an absent id is not found");

		// The editor's notes survive exactly, nested values included.
		check(asset.getMeta().getValue("grid").size() == 2
						&& StringView(asset.getMeta().getString("author")) == "test",
				"graph-asset: graph meta is carried through verbatim");
		check(asset.getNode(3)->meta.getInteger("x") == 40,
				"graph-asset: node meta is carried through verbatim");
		check(asset.getNode(3)->opHash == 12'345, "graph-asset: the operation hash is kept");
		check(asset.getNode(3)->params.getDouble("rhs") == 2.5
						&& StringView(asset.getNode(3)->params.getString("label")) == "speed"
						&& asset.getNode(3)->params.getBool("on"),
				"graph-asset: parameters keep their values and their types");
		check(StringView(asset.getNode(2)->settings.getString("mode")) == "exact"
						&& asset.getNode(2)->settings.getValue("with").size() == 1,
				"graph-asset: settings are carried as written, for the build to check");
	}

	{
		// The same graph, written in a different order. Not a rearrangement of the text: the same
		// records, listed differently.
		StringView shuffled(R"json({
			"formatVersion": 1,
			"name": "system.movement",
			"meta": {"author": "test", "grid": [16, 16]},
			"nodes": [
				{"id": 2, "op": "flow.branch", "settings": {"with": ["Unit"], "mode": "exact"},
					"params": {"condition": false}},
				{"id": 3, "op": "math.add", "opHash": 12345,
					"params": {"rhs": 2.5, "label": "speed", "on": true},
					"meta": {"x": 40, "y": 120}},
				{"id": 1, "op": "flow.start", "meta": {"x": 0, "y": 0}}
			],
			"edges": [
				{"kind": "exec", "from": 2, "fromPin": "true", "to": 3},
				{"kind": "exec", "from": 1, "fromPin": "then", "to": 2},
				{"kind": "data", "from": 3, "fromPin": "result", "to": 2, "toPin": "condition"},
				{"kind": "exec", "from": 2, "fromPin": "false", "to": 3}
			]
		})json");

		GraphAsset a;
		GraphAsset b;
		a.init();
		b.init();
		check(a.load(data::read<mem_std::Interface>(sampleGraph())) == Status::Ok
						&& b.load(data::read<mem_std::Interface>(shuffled)) == Status::Ok,
				"graph-asset: both orderings load");

		mem_std::Value dumpA;
		mem_std::Value dumpB;
		a.save(dumpA);
		b.save(dumpB);
		check(test::compareValues(dumpA, dumpB, StringView("shuffled")),
				"graph-asset: a file reordered record by record loads into the same asset");
	}

	// ---- what the format refuses ------------------------------------------------------------------

	{
		GraphAsset asset;
		asset.init();
		check(asset.load(data::read<mem_std::Interface>(sampleGraph())) == Status::Ok,
				"graph-asset: a good graph loads first");

		struct BadCase {
			StringView json;
			StringView code;
			StringView what;
		};

		const BadCase cases[] = {
			{StringView(
					 R"json({"formatVersion": 1, "nodes": [], "edges": [], "colour": "red"})json"),
				StringView("asset-unknown-key"), StringView("an unknown top-level key")},
			{StringView(R"json({"formatVersion": 1, "nodes": [{"id": 1, "op": "a", "x": 3}],
					"edges": []})json"),
				StringView("asset-unknown-key"), StringView("an unknown node key")},
			{StringView(R"json({"formatVersion": 2, "nodes": [], "edges": []})json"),
				StringView("asset-malformed"), StringView("the wrong format version")},
			{StringView(R"json({"nodes": [], "edges": []})json"), StringView("asset-malformed"),
				StringView("a missing format version")},
			{StringView(R"json({"formatVersion": 1, "edges": []})json"),
				StringView("asset-malformed"), StringView("a missing nodes array")},
			{StringView(R"json({"formatVersion": 1, "nodes": [{"id": 0, "op": "a"}],
					"edges": []})json"),
				StringView("node-id-invalid"), StringView("the null node id")},
			{StringView(R"json({"formatVersion": 1, "nodes": [{"id": -3, "op": "a"}],
					"edges": []})json"),
				StringView("node-id-invalid"), StringView("a negative node id")},
			{StringView(R"json({"formatVersion": 1, "nodes": [{"id": 1, "op": "a"},
					{"id": 1, "op": "b"}], "edges": []})json"),
				StringView("node-id-duplicate"), StringView("two nodes with one id")},
			{StringView(R"json({"formatVersion": 1, "nodes": [{"id": 1}], "edges": []})json"),
				StringView("asset-malformed"), StringView("a node with no operation")},
			{StringView(R"json({"formatVersion": 1, "nodes": [{"id": 1, "op": "a", "params": 7}],
					"edges": []})json"),
				StringView("asset-malformed"), StringView("params that are not a dictionary")},
			{StringView(R"json({"formatVersion": 1, "nodes": [{"id": 1, "op": "a", "settings": 7}],
					"edges": []})json"),
				StringView("asset-malformed"), StringView("settings that are not a dictionary")},
			{StringView(R"json({"formatVersion": 1, "nodes": [],
					"edges": [{"kind": "flow", "from": 1, "fromPin": "a", "to": 2}]})json"),
				StringView("asset-malformed"), StringView("an unknown edge kind")},
			{StringView(R"json({"formatVersion": 1, "nodes": [],
					"edges": [{"kind": "data", "from": 1, "fromPin": "a", "to": 2}]})json"),
				StringView("asset-malformed"), StringView("a data edge with no target pin")},
			{StringView(R"json({"formatVersion": 1, "nodes": [],
					"edges": [{"kind": "exec", "from": 1, "fromPin": "a", "to": 2,
						"toPin": "in"}]})json"),
				StringView("asset-malformed"), StringView("an exec edge that names a target pin")},
			{StringView(R"json({"formatVersion": 1, "nodes": [],
					"edges": [{"kind": "data", "from": 1, "to": 2, "toPin": "b"}]})json"),
				StringView("asset-malformed"), StringView("an edge with no source pin")},
			{StringView(R"json([])json"), StringView("asset-malformed"),
				StringView("a graph that is not a dictionary")},
		};

		for (auto &c : cases) {
			mem_std::Value diag;
			auto st = asset.load(data::read<mem_std::Interface>(c.json), &diag);
			check(st != Status::Ok && hasDiag(diag, c.code),
					mem_std::toString("graph-asset: ", c.what, " is refused"));
		}

		// ... and every one of those refusals left the previously loaded graph untouched. A partly
		// applied load would be worse than a rejected one: the editor would be holding a graph that
		// was never written down anywhere.
		check(asset.getNodes().size() == 3 && asset.getNode(3) != nullptr
						&& StringView(asset.getName()) == "system.movement",
				"graph-asset: a rejected load leaves the previous graph exactly as it was");
	}

	// ---- the limits -------------------------------------------------------------------------------

	{
		mem_std::Value graph(mem_std::Value::Type::DICTIONARY);
		graph.setInteger(1, "formatVersion");
		auto &nodes = graph.newArray("nodes");
		for (uint32_t i = 0; i <= MaxNodesPerGraph; ++i) {
			mem_std::Value node(mem_std::Value::Type::DICTIONARY);
			node.setInteger(int64_t(i + 1), "id");
			node.setString(StringView("test.op"), "op");
			nodes.addValue(sprt::move(node));
		}
		graph.newArray("edges");

		GraphAsset asset;
		asset.init();
		mem_std::Value diag;
		check(asset.load(graph, &diag) != Status::Ok
						&& hasDiag(diag, StringView("graph-too-large")),
				"graph-asset: a graph past MaxNodesPerGraph is refused");
	}

	// ---- the scene contract -----------------------------------------------------------------------
	//
	// The format half only: WHAT a graph declares it needs from the scene, not whether the scene has
	// it. That question needs a registry, and this class holds no more of one than it holds an
	// operation registry.

	{
		StringView declared = StringView(R"json({
			"formatVersion": 1,
			"scene": [
				{"component": "probe.Zeta", "fields": [{"name": "b", "type": "int"},
					{"name": "a", "type": "float"}]},
				{"component": "probe.Alpha"}
			],
			"nodes": [{"id": 1, "op": "flow.start"}],
			"edges": []
		})json");

		GraphAsset asset;
		asset.init();
		mem_std::Value diag;
		check(asset.load(data::read<mem_std::Interface>(declared), &diag) == Status::Ok,
				"graph-asset: a declared scene contract loads");

		auto scene = asset.getScene();
		check(scene.size() == 2 && StringView(scene[0].component) == "probe.Alpha"
						&& StringView(scene[1].component) == "probe.Zeta",
				"graph-asset: components come back in name order, whatever the file's was");
		check(scene[0].fields.empty(), "graph-asset: a component may name no field at all");
		check(scene[1].fields.size() == 2 && StringView(scene[1].fields[0].name) == "a"
						&& scene[1].fields[0].type == VarType::Float
						&& StringView(scene[1].fields[1].name) == "b"
						&& scene[1].fields[1].type == VarType::Int,
				"graph-asset: and so do fields, with the type each one declares");

		check(roundTrip(declared, StringView("scene contract")),
				"graph-asset: a contract survives save -> load -> save byte for byte");

		mem_std::Value saved;
		asset.save(saved);
		check(saved.isArray("scene"), "graph-asset: a graph that declares a contract saves it");
	}

	{
		// Omitted when empty, like every other optional field: a graph that never had a contract must
		// not grow one by being saved, or the round trip would stop being a fixed point.
		GraphAsset asset;
		asset.init();
		mem_std::Value diag;
		check(asset.load(data::read<mem_std::Interface>(sampleGraph()), &diag) == Status::Ok
						&& asset.getScene().empty(),
				"graph-asset: a graph that declares nothing has an empty contract");

		mem_std::Value saved;
		asset.save(saved);
		check(saved.getValue("scene").isNull(),
				"graph-asset: and saving it does not invent a \"scene\" key");
	}

	{
		struct Case {
			StringView json;
			StringView code;
			StringView name;
		};
		const Case cases[] = {
			{StringView(R"json({"formatVersion": 1, "scene": {}, "nodes": [], "edges": []})json"),
				StringView("asset-malformed"), StringView("graph-asset: scene is an array")},
			{StringView(
					 R"json({"formatVersion": 1, "scene": [{"comp": "x"}], "nodes": [], "edges": []})json"),
				StringView("asset-unknown-key"),
				StringView("graph-asset: an unknown key in a declaration is refused")},
			{StringView(R"json({"formatVersion": 1, "scene": [{"component": ""}],
					"nodes": [], "edges": []})json"),
				StringView("asset-malformed"),
				StringView("graph-asset: a declaration names its component")},
			{StringView(R"json({"formatVersion": 1,
					"scene": [{"component": "a.B"}, {"component": "a.B"}],
					"nodes": [], "edges": []})json"),
				StringView("asset-malformed"),
				StringView("graph-asset: one component declared twice is refused")},
			{StringView(R"json({"formatVersion": 1,
					"scene": [{"component": "a.B", "fields": [{"name": "f", "type": "quaternion"}]}],
					"nodes": [], "edges": []})json"),
				StringView("asset-malformed"),
				StringView("graph-asset: a field type this layer does not know is refused")},
			{StringView(R"json({"formatVersion": 1,
					"scene": [{"component": "a.B", "fields": [{"name": "f", "type": "int"},
						{"name": "f", "type": "int"}]}],
					"nodes": [], "edges": []})json"),
				StringView("asset-malformed"),
				StringView("graph-asset: one field declared twice is refused")},
		};

		for (auto &c : cases) {
			GraphAsset asset;
			asset.init();
			mem_std::Value diag;
			check(asset.load(data::read<mem_std::Interface>(c.json), &diag) != Status::Ok
							&& hasDiag(diag, c.code),
					c.name);
		}
	}
}

} // namespace STAPPLER_VERSIONIZED stappler
