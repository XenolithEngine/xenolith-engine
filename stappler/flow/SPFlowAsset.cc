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

// The graph asset: what a graph looks like on disk, and the canonical order it comes back in.
// Nothing here resolves anything. A node names its operation with a string and an edge names a pin
// with a string; whether those exist is validation's question and it needs a registry, which this
// file deliberately does not have. What this file owns is the format - and the fact that a file
// which differs from another only in the order of its records loads into an identical asset.

#include "SPFlowAsset.h"

#include "SPData.h"

#include <sprt/runtime/hash.h>

#include <sprt/c/__sprt_string.h>
#include <sprt/cxx/algorithm>

namespace STAPPLER_VERSIONIZED stappler::flow {

StringView getEdgeKindName(EdgeKind k) {
	switch (k) {
	case EdgeKind::Data: return StringView("data");
	case EdgeKind::Exec: return StringView("exec");
	}
	return StringView("?");
}

bool readEdgeKind(StringView name, EdgeKind &out) {
	if (name == "data") {
		out = EdgeKind::Data;
		return true;
	}
	if (name == "exec") {
		out = EdgeKind::Exec;
		return true;
	}
	return false;
}

// An unknown key is refused rather than dropped. Silently dropping it would make "save -> load ->
// save is byte-identical" a property of what happened to be in the file rather than of the format,
// and would swallow a typo in a hand-written asset without a word.
static bool checkKeys(const mem_std::Value &dict, SpanView<StringView> known, DiagPhrase what,
		DiagReport &report) {
	bool ok = true;
	for (auto &it : dict.asDict()) {
		bool found = false;
		for (auto &k : known) {
			if (StringView(it.first) == k) {
				found = true;
				break;
			}
		}
		if (!found) {
			ok = false;
			report.report(DiagSeverity::Error, DiagCode::AssetUnknownKey,
					DiagText(DiagDetail::AssetUnknownKey).phrase(what).name(it.first));
		}
	}
	return ok;
}

/* What the file says it is, and whether this build can read it. Three shapes arrive, and each is a
decision rather than a case to normalise away: an envelope naming this kind, the standard and the
only shape save() writes; an envelope naming another kind, refused outright, because a scene opened
as a graph would otherwise be reported as eleven unknown keys; and no envelope, a file older than
the standard, read by the `formatVersion` it was written under and answered through `legacy` rather
than through the report - the report is about the graph, and there is nothing wrong with this graph.
`formatVersion` beside an envelope is accepted only when the two agree: a hand-written file may
carry both, but two numbers that disagree is what the envelope exists to end. */
static void readEnvelope(const mem_std::Value &value, DiagReport &report, bool &legacy) {
	auto refuse = [&](const DiagText &text) {
		report.report(DiagSeverity::Error, DiagCode::AssetMalformed, text);
	};

	auto &meta = value.getValue(GraphAsset::MetaKey);
	if (meta.isNull()) {
		if (!value.isInteger("formatVersion")
				|| value.getInteger("formatVersion") != int64_t(GraphAsset::FormatVersion)) {
			refuse(DiagText(DiagDetail::NeedsMeta)
							.name(GraphAsset::MetaKey)
							.name(GraphAsset::Kind)
							.number(int64_t(GraphAsset::FormatVersion)));
		} else {
			legacy = true;
		}
		return;
	}

	// The envelope's own sentences, under the graph's code for a file it cannot read: what an asset
	// reports is what is wrong with an asset, and `asset-malformed` is the answer to "can this be
	// opened" whatever the envelope's reason was.
	if (!meta.isDictionary()) {
		refuse(DiagText(DiagDetail::FormatMetaMalformed));
		return;
	}
	auto &kind = meta.getValue("kind");
	if (!kind.isString() || kind.getString().empty()) {
		refuse(DiagText(DiagDetail::FormatMetaKindMissing));
		return;
	}
	auto &version = meta.getValue("version");
	if (!version.isInteger() || version.getInteger() <= 0
			|| version.getInteger() > int64_t(maxOf<uint32_t>())) {
		refuse(DiagText(DiagDetail::FormatMetaVersionInvalid));
		return;
	}

	if (StringView(kind.getString()) != GraphAsset::Kind) {
		refuse(DiagText(DiagDetail::NotAGraph).name(kind.getString()));
		return;
	}

	if (uint32_t(version.getInteger()) > GraphAsset::FormatVersion) {
		refuse(DiagText(DiagDetail::FormatVersionUnsupported)
						.name(GraphAsset::Kind)
						.number(version.getInteger())
						.number(int64_t(GraphAsset::FormatVersion)));
		return;
	}

	if (value.getValue("formatVersion").isInteger()
			&& value.getInteger("formatVersion") != version.getInteger()) {
		refuse(DiagText(DiagDetail::VersionDisagrees));
	}
}

GraphAsset::~GraphAsset() {
	_scene.clear();
	_nodes.clear();
	_edges.clear();
	if (_pool && _ownsPool) {
		memory::pool::destroy(_pool);
	}
	_pool = nullptr;
}

bool GraphAsset::init(memory::pool_t *parent) {
	if (_pool) {
		return false;
	}
	_pool = parent;
	_ownsPool = false;
	if (!_pool) {
		_pool = memory::pool::create();
		_ownsPool = true;
	}
	return _pool != nullptr;
}

uint64_t GraphAsset::getContentHash() const {
	if (!_pool) {
		return 0;
	}
	// The canonical form and nothing else: save() writes the nodes in ascending id order and the
	// edges in their endpoint order whatever the file had, so the bytes below are a property of
	// what the asset says and not of how it was spelled.
	mem_std::Value canonical;
	save(canonical);
	auto bytes = data::write<mem_std::Interface>(canonical, data::EncodeFormat::Cbor);
	return sprt::hash64(reinterpret_cast<const char *>(bytes.data()), bytes.size());
}

const GraphNode *GraphAsset::getNode(NodeId id) const {
	uint32_t lo = 0;
	uint32_t hi = uint32_t(_nodes.size());
	while (lo < hi) {
		auto mid = lo + (hi - lo) / 2;
		if (_nodes[mid].id < id) {
			lo = mid + 1;
		} else if (_nodes[mid].id > id) {
			hi = mid;
		} else {
			return &_nodes[mid];
		}
	}
	return nullptr;
}

// The canonical order of edges follows their identity: a data edge is identified by the
// input it feeds, an exec edge by the output it leaves. Sorting by that key puts the two edges of
// an arity violation next to each other, so validation finds them in one pass instead of a search.
static bool edgeLess(const GraphEdge &a, const GraphEdge &b) {
	if (a.kind != b.kind) {
		return a.kind < b.kind;
	}
	if (a.kind == EdgeKind::Data) {
		if (a.to != b.to) {
			return a.to < b.to;
		}
		if (a.toPin != b.toPin) {
			return a.toPin < b.toPin;
		}
		if (a.from != b.from) {
			return a.from < b.from;
		}
		return a.fromPin < b.fromPin;
	}
	if (a.from != b.from) {
		return a.from < b.from;
	}
	if (a.fromPin != b.fromPin) {
		return a.fromPin < b.fromPin;
	}
	return a.to < b.to;
}

Status GraphAsset::load(const mem_std::Value &value, DiagSink *diagnostic) {
	if (!_pool) {
		return Status::ErrorInvalidArguemnt;
	}

	DiagReport report(diagnostic);

	auto intern = [&](StringView s) -> StringView {
		if (s.empty()) {
			return StringView();
		}
		auto mem = reinterpret_cast<char *>(memory::pool::palloc(_pool, s.size() + 1, 1));
		if (!mem) {
			return StringView();
		}
		__sprt_memcpy(mem, s.data(), s.size());
		mem[s.size()] = 0;
		return StringView(mem, s.size());
	};

	if (!value.isDictionary()) {
		report.report(DiagSeverity::Error, DiagCode::AssetMalformed,
				DiagText(DiagDetail::GraphDict));
		return report.getStatus();
	}

	static const StringView rootKeys[] = {GraphAsset::MetaKey, StringView("formatVersion"),
		StringView("name"), StringView("meta"), StringView("scene"), StringView("extensions"),
		StringView("nodes"), StringView("edges")};
	checkKeys(value, SpanView<StringView>(rootKeys, 8), DiagPhrase::SectionGraph, report);

	bool legacyFormat = false;
	readEnvelope(value, report, legacyFormat);

	// Built beside the current content and swapped in at the end, so a rejected file leaves the
	// asset exactly as it was.
	StringView name;
	StringView generator;
	if (auto &meta = value.getValue(GraphAsset::MetaKey); meta.isString("generator")) {
		generator = intern(StringView(meta.getString("generator")));
	}
	mem_std::Value meta;
	mem_std::Vector<SceneDecl> scene;
	mem_std::Vector<ExtensionDecl> extensions;
	mem_std::Vector<GraphNode> nodes;
	mem_std::Vector<GraphEdge> edges;

	if (value.isString("name")) {
		name = intern(StringView(value.getString("name")));
	} else if (!value.getValue("name").isNull()) {
		report.report(DiagSeverity::Error, DiagCode::AssetMalformed,
				DiagText(DiagDetail::NameString));
	}

	auto &metaValue = value.getValue("meta");
	if (metaValue.isDictionary()) {
		meta = metaValue;
	} else if (!metaValue.isNull()) {
		report.report(DiagSeverity::Error, DiagCode::AssetMalformed,
				DiagText(DiagDetail::MetaDict));
	}

	// The graph's side of the scene contract, when the file states it. Structure only: whether the
	// scene actually has these components is the build's question, and it needs a registry this
	// class deliberately does not hold - the same line the operation registry sits on.
	auto &sceneValue = value.getValue("scene");
	if (sceneValue.isArray()) {
		static const StringView declKeys[] = {StringView("component"), StringView("fields")};
		static const StringView fieldKeys[] = {StringView("name"), StringView("type")};

		scene.reserve(sceneValue.size());
		for (auto &entry : sceneValue.asArray()) {
			if (!entry.isDictionary()) {
				report.report(DiagSeverity::Error, DiagCode::AssetMalformed,
						DiagText(DiagDetail::SceneDeclDict));
				continue;
			}
			checkKeys(entry, SpanView<StringView>(declKeys, 2), DiagPhrase::SectionScene, report);

			SceneDecl decl;
			decl.component = intern(StringView(entry.getString("component")));
			if (decl.component.empty()) {
				report.report(DiagSeverity::Error, DiagCode::AssetMalformed,
						DiagText(DiagDetail::SceneDeclComponent));
				continue;
			}

			bool duplicate = false;
			for (auto &existing : scene) {
				if (existing.component == decl.component) {
					duplicate = true;
					break;
				}
			}
			if (duplicate) {
				report.report(DiagSeverity::Error, DiagCode::AssetMalformed,
						DiagText(DiagDetail::SceneDeclTwice).name(decl.component));
				continue;
			}

			auto &fieldArray = entry.getValue("fields");
			if (fieldArray.isArray()) {
				for (auto &f : fieldArray.asArray()) {
					if (!f.isDictionary()) {
						report.report(DiagSeverity::Error, DiagCode::AssetMalformed,
								DiagText(DiagDetail::SceneFieldDict));
						continue;
					}
					checkKeys(f, SpanView<StringView>(fieldKeys, 2), DiagPhrase::SectionSceneField,
							report);

					SceneFieldDecl field;
					field.name = intern(StringView(f.getString("name")));
					if (field.name.empty()) {
						report.report(DiagSeverity::Error, DiagCode::AssetMalformed,
								DiagText(DiagDetail::SceneFieldName).name(decl.component));
						continue;
					}
					// By name, through the same reader the schema layer uses on a data-driven
					// component - so a declaration and the type it declares are spelled alike.
					if (!value::readVarType(StringView(f.getString("type")), field.type)) {
						report.report(DiagSeverity::Error, DiagCode::AssetMalformed,
								DiagText(DiagDetail::SceneFieldTypeUnknown)
										.name(decl.component)
										.name(field.name));
						continue;
					}

					bool dupField = false;
					for (auto &existing : decl.fields) {
						if (existing.name == field.name) {
							dupField = true;
							break;
						}
					}
					if (dupField) {
						report.report(DiagSeverity::Error, DiagCode::AssetMalformed,
								DiagText(DiagDetail::SceneFieldTwice)
										.name(decl.component)
										.name(field.name));
						continue;
					}

					decl.fields.emplace_back(sprt::move(field));
				}
			} else if (!fieldArray.isNull()) {
				report.report(DiagSeverity::Error, DiagCode::AssetMalformed,
						DiagText(DiagDetail::SceneFieldsArray));
			}

			scene.emplace_back(sprt::move(decl));
		}
	} else if (!sceneValue.isNull()) {
		report.report(DiagSeverity::Error, DiagCode::AssetMalformed,
				DiagText(DiagDetail::SceneArray));
	}

	// The graph's side of the extension contract. Structure only, as above: whether the process has
	// such an extension and whether the scene carries such an instance both need things this class
	// deliberately does not hold.
	auto &extValue = value.getValue("extensions");
	if (extValue.isArray()) {
		static const StringView extKeys[] = {StringView("name"), StringView("id"),
			StringView("params")};

		extensions.reserve(extValue.size());
		for (auto &entry : extValue.asArray()) {
			if (!entry.isDictionary()) {
				report.report(DiagSeverity::Error, DiagCode::AssetMalformed,
						DiagText(DiagDetail::ExtDeclDict));
				continue;
			}
			checkKeys(entry, SpanView<StringView>(extKeys, 3), DiagPhrase::SectionExtensions,
					report);

			ExtensionDecl decl;
			decl.name = intern(StringView(entry.getString("name")));
			decl.id = intern(StringView(entry.getString("id")));
			if (decl.name.empty() || decl.id.empty()) {
				report.report(DiagSeverity::Error, DiagCode::AssetMalformed,
						DiagText(DiagDetail::ExtDeclNames));
				continue;
			}

			// By id, because the id is what a node writes. Two instances of one extension are
			// ordinary; two declarations of one id are a file that cannot say what "board" means.
			bool duplicate = false;
			for (auto &existing : extensions) {
				if (existing.id == decl.id) {
					duplicate = true;
					break;
				}
			}
			if (duplicate) {
				report.report(DiagSeverity::Error, DiagCode::AssetMalformed,
						DiagText(DiagDetail::ExtDeclTwice).name(decl.id));
				continue;
			}

			auto &params = entry.getValue("params");
			if (params.isDictionary()) {
				decl.params = params;
			} else if (!params.isNull()) {
				report.report(DiagSeverity::Error, DiagCode::AssetMalformed,
						DiagText(DiagDetail::ExtParamsDict).name(decl.id));
				continue;
			}

			extensions.emplace_back(sprt::move(decl));
		}
	} else if (!extValue.isNull()) {
		report.report(DiagSeverity::Error, DiagCode::AssetMalformed,
				DiagText(DiagDetail::ExtArray));
	}

	if (!value.isArray("nodes")) {
		report.report(DiagSeverity::Error, DiagCode::AssetMalformed,
				DiagText(DiagDetail::NodesArray));
		return report.getStatus();
	}
	if (!value.isArray("edges")) {
		report.report(DiagSeverity::Error, DiagCode::AssetMalformed,
				DiagText(DiagDetail::EdgesArray));
		return report.getStatus();
	}

	auto &nodeArray = value.getArray("nodes");
	auto &edgeArray = value.getArray("edges");

	if (nodeArray.size() > MaxNodesPerGraph) {
		report.report(DiagSeverity::Error, DiagCode::GraphTooLarge,
				DiagText(DiagDetail::TooManyNodes).number(int64_t(MaxNodesPerGraph)));
		return report.getStatus();
	}
	if (edgeArray.size() > MaxEdgesPerGraph) {
		report.report(DiagSeverity::Error, DiagCode::GraphTooLarge,
				DiagText(DiagDetail::TooManyEdges).number(int64_t(MaxEdgesPerGraph)));
		return report.getStatus();
	}

	static const StringView nodeKeys[] = {StringView("id"), StringView("op"), StringView("opHash"),
		StringView("params"), StringView("settings"), StringView("meta")};

	nodes.reserve(nodeArray.size());

	// The ids accepted so far, so that "two nodes share this id" costs a lookup rather than a walk.
	// Local to this loop and says nothing about the order of anything: `nodes` is sorted by id
	// below, and the report is emitted at the node that repeats rather than at the one it repeats.
	mem_std::Set<NodeId> seenIds;

	for (auto &entry : nodeArray) {
		if (!entry.isDictionary()) {
			report.report(DiagSeverity::Error, DiagCode::AssetMalformed,
					DiagText(DiagDetail::NodeDict));
			continue;
		}
		checkKeys(entry, SpanView<StringView>(nodeKeys, 6), DiagPhrase::SectionNode, report);

		auto rawId = entry.getInteger("id");
		if (!entry.isInteger("id") || rawId <= 0 || rawId > int64_t(0xffff'ffffll)) {
			report.report(DiagSeverity::Error, DiagCode::NodeIdInvalid,
					DiagText(DiagDetail::NodeIdInvalid).number(int64_t(rawId)));
			continue;
		}

		GraphNode node;
		node.id = NodeId(rawId);

		if (seenIds.find(node.id) != seenIds.end()) {
			report.reportNode(DiagSeverity::Error, DiagCode::NodeIdDuplicate, node.id,
					DiagText(DiagDetail::NodeIdDuplicate));
			continue;
		}

		StringView op(entry.getString("op"));
		if (op.empty()) {
			report.reportNode(DiagSeverity::Error, DiagCode::AssetMalformed, node.id,
					DiagText(DiagDetail::NodeNoOp));
			continue;
		}
		node.op = intern(op);
		node.opHash = uint64_t(entry.getInteger("opHash"));

		auto &params = entry.getValue("params");
		if (params.isDictionary()) {
			node.params = params;
		} else if (!params.isNull()) {
			report.reportNode(DiagSeverity::Error, DiagCode::AssetMalformed, node.id,
					DiagText(DiagDetail::NodeParamsDict));
			continue;
		}

		auto &settings = entry.getValue("settings");
		if (settings.isDictionary()) {
			node.settings = settings;
		} else if (!settings.isNull()) {
			report.reportNode(DiagSeverity::Error, DiagCode::AssetMalformed, node.id,
					DiagText(DiagDetail::NodeSettingsDict));
			continue;
		}

		auto &nodeMeta = entry.getValue("meta");
		if (nodeMeta.isDictionary()) {
			node.meta = nodeMeta;
		} else if (!nodeMeta.isNull()) {
			report.reportNode(DiagSeverity::Error, DiagCode::AssetMalformed, node.id,
					DiagText(DiagDetail::NodeMetaDict));
			continue;
		}

		// Recorded where the node is accepted. Recording it at the id check instead would make a
		// second node with the same id a duplicate even when the first one was thrown out for
		// something else - a different set of findings for the same malformed file.
		seenIds.emplace(node.id);
		nodes.emplace_back(sprt::move(node));
	}

	static const StringView edgeKeys[] = {StringView("kind"), StringView("from"),
		StringView("fromPin"), StringView("to"), StringView("toPin")};

	edges.reserve(edgeArray.size());
	for (auto &entry : edgeArray) {
		if (!entry.isDictionary()) {
			report.report(DiagSeverity::Error, DiagCode::AssetMalformed,
					DiagText(DiagDetail::EdgeDict));
			continue;
		}
		checkKeys(entry, SpanView<StringView>(edgeKeys, 5), DiagPhrase::SectionEdge, report);

		GraphEdge edge;
		if (!readEdgeKind(StringView(entry.getString("kind")), edge.kind)) {
			report.report(DiagSeverity::Error, DiagCode::AssetMalformed,
					DiagText(DiagDetail::EdgeKind));
			continue;
		}

		auto from = entry.getInteger("from");
		auto to = entry.getInteger("to");
		if (from <= 0 || from > int64_t(0xffff'ffffll) || to <= 0 || to > int64_t(0xffff'ffffll)) {
			report.report(DiagSeverity::Error, DiagCode::NodeIdInvalid,
					DiagText(DiagDetail::EdgeIds));
			continue;
		}
		edge.from = NodeId(from);
		edge.to = NodeId(to);

		StringView fromPin(entry.getString("fromPin"));
		if (fromPin.empty()) {
			report.reportEdge(DiagSeverity::Error, DiagCode::AssetMalformed, edge.from,
					StringView(), edge.to, StringView(), DiagText(DiagDetail::EdgeFromPin));
			continue;
		}
		edge.fromPin = intern(fromPin);

		StringView toPin(entry.getString("toPin"));
		if (edge.kind == EdgeKind::Data) {
			if (toPin.empty()) {
				report.reportEdge(DiagSeverity::Error, DiagCode::AssetMalformed, edge.from,
						edge.fromPin, edge.to, StringView(), DiagText(DiagDetail::EdgeToPin));
				continue;
			}
			edge.toPin = intern(toPin);
		} else if (!toPin.empty()) {
			// An operation has at most one exec input, so naming it would be a field with exactly
			// one legal value - and a second spelling of the same edge.
			report.reportEdge(DiagSeverity::Error, DiagCode::AssetMalformed, edge.from,
					edge.fromPin, edge.to, toPin, DiagText(DiagDetail::EdgeExecNoPin));
			continue;
		}

		edges.emplace_back(sprt::move(edge));
	}

	if (report.hasErrors()) {
		return report.getStatus();
	}

	sprt::sort(nodes.begin(), nodes.end(),
			[](const GraphNode &a, const GraphNode &b) { return a.id < b.id; });
	sprt::sort(edges.begin(), edges.end(), edgeLess);

	// By name, both levels: a declaration has no order of its own the way a node has an id, so the
	// only order that two files agreeing on the contract can agree on is the alphabet.
	sprt::sort(scene.begin(), scene.end(),
			[](const SceneDecl &a, const SceneDecl &b) { return a.component < b.component; });
	for (auto &decl : scene) {
		sprt::sort(decl.fields.begin(), decl.fields.end(),
				[](const SceneFieldDecl &a, const SceneFieldDecl &b) { return a.name < b.name; });
	}

	// By id for the same reason, and by id rather than by name because the id is what is unique:
	// two instances of one extension are ordinary, and sorting by name would leave their order to
	// the file.
	sprt::sort(extensions.begin(), extensions.end(),
			[](const ExtensionDecl &a, const ExtensionDecl &b) { return a.id < b.id; });

	// With the rest, and only now: an asset that refused this file must not be left claiming the
	// file's format either.
	_legacyFormat = legacyFormat;
	_name = name;
	_generator = generator;
	_meta = sprt::move(meta);
	_scene = sprt::move(scene);
	_extensions = sprt::move(extensions);
	_nodes = sprt::move(nodes);
	_edges = sprt::move(edges);
	return Status::Ok;
}

void GraphAsset::setGenerator(StringView generator) {
	if (!_pool || generator.empty()) {
		_generator = StringView();
		return;
	}
	auto mem = reinterpret_cast<char *>(memory::pool::palloc(_pool, generator.size() + 1, 1));
	if (!mem) {
		return;
	}
	__sprt_memcpy(mem, generator.data(), generator.size());
	mem[generator.size()] = 0;
	_generator = StringView(mem, generator.size());
}

void GraphAsset::save(mem_std::Value &out) const {
	out = mem_std::Value(mem_std::Value::Type::DICTIONARY);

	// The envelope and not `formatVersion`: two version numbers in one file are two places that can
	// disagree, and the one in __meta is what every reader of every kind already looks at. A file
	// written here therefore always announces itself.
	mem_std::Value envelope(mem_std::Value::Type::DICTIONARY);
	envelope.setString(Kind, "kind");
	envelope.setInteger(int64_t(FormatVersion), "version");
	if (!_generator.empty()) {
		envelope.setString(_generator, "generator");
	}
	out.setValue(sprt::move(envelope), MetaKey);
	if (!_name.empty()) {
		out.setString(_name, "name");
	}
	if (_meta.isDictionary() && _meta.size() > 0) {
		out.setValue(_meta, "meta");
	}

	// Omitted when the graph declares nothing, like every other empty field here, so that a file
	// which never had a contract does not grow one by being saved.
	if (!_scene.empty()) {
		auto &scene = out.newArray("scene");
		for (auto &decl : _scene) {
			mem_std::Value entry(mem_std::Value::Type::DICTIONARY);
			entry.setString(decl.component, "component");
			if (!decl.fields.empty()) {
				auto &fields = entry.newArray("fields");
				for (auto &f : decl.fields) {
					mem_std::Value fieldEntry(mem_std::Value::Type::DICTIONARY);
					fieldEntry.setString(f.name, "name");
					fieldEntry.setString(value::getVarTypeName(f.type), "type");
					fields.addValue(sprt::move(fieldEntry));
				}
			}
			scene.addValue(sprt::move(entry));
		}
	}

	if (!_extensions.empty()) {
		auto &extensions = out.newArray("extensions");
		for (auto &decl : _extensions) {
			mem_std::Value entry(mem_std::Value::Type::DICTIONARY);
			entry.setString(decl.name, "name");
			entry.setString(decl.id, "id");
			if (decl.params.isDictionary() && decl.params.size() > 0) {
				entry.setValue(decl.params, "params");
			}
			extensions.addValue(sprt::move(entry));
		}
	}

	auto &nodes = out.newArray("nodes");
	for (auto &n : _nodes) {
		mem_std::Value entry(mem_std::Value::Type::DICTIONARY);
		entry.setInteger(int64_t(n.id), "id");
		entry.setString(n.op, "op");
		if (n.opHash != 0) {
			entry.setInteger(int64_t(n.opHash), "opHash");
		}
		if (n.params.isDictionary() && n.params.size() > 0) {
			entry.setValue(n.params, "params");
		}
		if (n.settings.isDictionary() && n.settings.size() > 0) {
			entry.setValue(n.settings, "settings");
		}
		if (n.meta.isDictionary() && n.meta.size() > 0) {
			entry.setValue(n.meta, "meta");
		}
		nodes.addValue(sprt::move(entry));
	}

	auto &edges = out.newArray("edges");
	for (auto &e : _edges) {
		mem_std::Value entry(mem_std::Value::Type::DICTIONARY);
		entry.setString(getEdgeKindName(e.kind), "kind");
		entry.setInteger(int64_t(e.from), "from");
		entry.setString(e.fromPin, "fromPin");
		entry.setInteger(int64_t(e.to), "to");
		if (!e.toPin.empty()) {
			entry.setString(e.toPin, "toPin");
		}
		edges.addValue(sprt::move(entry));
	}
}

} // namespace stappler::flow
