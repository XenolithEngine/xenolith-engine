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

// Validation and the runtime graph. Validation is not a separate pass with its own rulebook: it is
// the first half of the build, and RuntimeGraph::validate() builds into a graph it then discards.
// One implementation, so an editor's verdict and the loader's cannot drift apart - and after
// build() returns Ok nothing may refuse the graph for a reason of substance any more. Two
// decisions show up all over this file. Indices rather than ids, and canonical order everywhere:
// the node index is the position in ascending id order, adjacency lists are built by counting sort
// so they keep the asset's canonical edge order, and the traversals start from the entry nodes in
// index order, which is what makes two files differing only in record order produce identical
// graphs. And everything a run-time decision would need is decided here: the conversion rule
// of every edge, whether that conversion touches the arena, and the already-converted value of
// every unconnected input.

#include "SPFlowRuntime.h"
#include "SPFlowGpu.h"

namespace STAPPLER_VERSIONIZED stappler::flow {

RuntimeGraph::~RuntimeGraph() {
	delete _gpuShaders;
	_gpuShaders = nullptr;
	delete _link;
	_link = nullptr;
	_nodes.clear();
	_constants.clear();
	_settings.clear();
	_sceneBindings.clear();
	if (_pool && _ownsPool) {
		memory::pool::destroy(_pool);
	}
	_pool = nullptr;
}

bool RuntimeGraph::init(memory::pool_t *parent) {
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

uint32_t RuntimeGraph::findNode(NodeId id) const {
	uint32_t lo = 0;
	uint32_t hi = uint32_t(_nodes.size());
	while (lo < hi) {
		auto mid = lo + (hi - lo) / 2;
		if (_nodes[mid].id < id) {
			lo = mid + 1;
		} else if (_nodes[mid].id > id) {
			hi = mid;
		} else {
			return mid;
		}
	}
	return InvalidIndex;
}

SpanView<uint32_t> RuntimeGraph::getCrossScopeInEdges(uint32_t node) const {
	auto &n = _nodes[node];
	return SpanView<uint32_t>(_crossScopeInIndex.data() + n.crossScopeInBegin, n.crossScopeInCount);
}

SpanView<uint32_t> RuntimeGraph::getDataInEdges(uint32_t node) const {
	auto &n = _nodes[node];
	return SpanView<uint32_t>(_dataInIndex.data() + n.dataInBegin, n.dataInCount);
}

SpanView<uint32_t> RuntimeGraph::getDataOutEdges(uint32_t node) const {
	auto &n = _nodes[node];
	return SpanView<uint32_t>(_dataOutIndex.data() + n.dataOutBegin, n.dataOutCount);
}

SpanView<uint32_t> RuntimeGraph::getExecInEdges(uint32_t node) const {
	auto &n = _nodes[node];
	return SpanView<uint32_t>(_execInIndex.data() + n.execInBegin, n.execInCount);
}

SpanView<uint32_t> RuntimeGraph::getExecOutEdges(uint32_t node) const {
	auto &n = _nodes[node];
	return SpanView<uint32_t>(_execOutIndex.data() + n.execOutBegin, n.execOutCount);
}

const mem_std::Value *RuntimeGraph::getConstant(uint32_t node, uint32_t pin) const {
	auto &n = _nodes[node];
	if (!n.op || pin >= n.op->getDataIn().size()) {
		return nullptr;
	}
	auto &value = _constants[n.constantBegin + pin];
	return value.isNull() ? nullptr : &value;
}

const mem_std::Value *RuntimeGraph::getSetting(uint32_t node, uint32_t index) const {
	auto &n = _nodes[node];
	if (!n.op || index >= n.op->getSettings().size()
			|| n.settingBegin + index >= _settings.size()) {
		return nullptr;
	}
	auto &value = _settings[n.settingBegin + index];
	return value.isNull() ? nullptr : &value;
}

// edgeTypesMeet lives in SPFlowOp.cc, beside resolveConstant: an edge is checked by type and a
// literal by value, the two halves of the same question, and the editor calls the first one
// directly. Only the naming of its three refusals is there - the rule underneath is
// value::valueTypesMeet, because the screen asks it too and must not link this module.

// Whether this graph holds anything a registry could have answered. A graph that names nothing in
// the scene, and one whose every name is computed at run time, hold no descriptor that could go
// stale - so they are not bound to anything even when they were checked against something, and the
// interpreter must let them run against whatever scene they are handed.
bool anyResolvable(SpanView<SceneBinding> bindings) {
	for (auto &binding : bindings) {
		if (!binding.dynamic) {
			return true;
		}
	}
	return false;
}

// The same question one level along, and it needs no `dynamic` clause of its own for a different
// reason: a dynamic extension binding is unusable rather than merely unresolved, so a graph holding
// only those is not bound to anything either.
bool anyResolvable(SpanView<ExtensionBinding> bindings) {
	for (auto &binding : bindings) {
		if (!binding.dynamic) {
			return true;
		}
	}
	return false;
}

Status RuntimeGraph::build(const GraphAsset &asset, const OpRegistry &ops, DiagSink *reportSink) {
	return buildImpl(asset, ops, nullptr, nullptr, reportSink);
}

Status RuntimeGraph::build(const GraphAsset &asset, const OpRegistry &ops,
		const value::TypeRegistry &scene, DiagSink *reportSink) {
	return buildImpl(asset, ops, &scene, nullptr, reportSink);
}

Status RuntimeGraph::build(const GraphAsset &asset, const OpRegistry &ops,
		const value::TypeRegistry &scene, const value::ExtensionHost &extensions,
		DiagSink *reportSink) {
	return buildImpl(asset, ops, &scene, &extensions, reportSink);
}

// Everything a build derives, emptied: what a refused build leaves behind.
void RuntimeGraph::clearBuilt() {
	_nodes.clear();
	_dataEdges.clear();
	_execEdges.clear();
	_dataInIndex.clear();
	_dataOutIndex.clear();
	_execInIndex.clear();
	_execOutIndex.clear();
	_crossScopeInIndex.clear();
	_entryNodes.clear();
	_terminalNodes.clear();
	_constants.clear();
	_settings.clear();
	_sceneBindings.clear();
	_sceneRegistry = nullptr;
	_sceneRegistryCount = 0;
	_extBindings.clear();
	_extDecls.clear();
	_extensions = nullptr;
	_extensionsEpoch = 0;
	_namedBindings.clear();
	_namedBound = false;
	_scopes.clear();
	_scopeNodes.clear();
	_blocks.clear();
	_blockWrites.clear();
	_blockCollectors.clear();
	_blockQuery.clear();
	_bodyOf.clear();
}

Status RuntimeGraph::buildImpl(const GraphAsset &asset, const OpRegistry &ops,
		const value::TypeRegistry *scene, const value::ExtensionHost *extensions,
		DiagSink *reportSink) {
	if (!_pool) {
		return Status::ErrorInvalidArguemnt;
	}

	_built = false;
	_nodes.clear();
	_dataEdges.clear();
	_execEdges.clear();
	_dataInIndex.clear();
	_dataOutIndex.clear();
	_execInIndex.clear();
	_execOutIndex.clear();
	_crossScopeInIndex.clear();
	_entryNodes.clear();
	_terminalNodes.clear();
	_constants.clear();
	_settings.clear();
	_sceneBindings.clear();
	_sceneRegistry = nullptr;
	_sceneRegistryCount = 0;
	_extBindings.clear();
	_extensions = nullptr;
	_extensionsEpoch = 0;
	_namedBindings.clear();
	_namedBound = false;
	_builtAsset = nullptr;
	_bodyOf.clear();
	_functionCount = 0;

	// A document that is a function, defines one or calls one is linked first, and what is built is
	// the linked document over the operations its functions contribute. Every other document is
	// built as it stands.
	if (GraphLink::needsLink(asset)) {
		if (!_link) {
			_link = new GraphLink();
			_link->init(_pool);
		}
		if (_link->link(asset, ops, _functionHost, reportSink) != Status::Ok) {
			clearBuilt();
			return Status::ErrorInvalidArguemnt;
		}
		return buildLinked(_link->getAsset(), _link->getOps(), scene, extensions, reportSink);
	}
	if (_link) {
		delete _link;
		_link = nullptr;
	}
	return buildLinked(asset, ops, scene, extensions, reportSink);
}

Status RuntimeGraph::buildLinked(const GraphAsset &asset, const OpRegistry &ops,
		const value::TypeRegistry *scene, const value::ExtensionHost *extensions,
		DiagSink *reportSink) {
	DiagReport report(reportSink);
	_builtAsset = &asset;

	// Nodes and their operations: every body's, in one id order. A document without functions has
	// one body, and this is its node list as written.

	mem_std::Vector<sprt::pair<const GraphNode *, uint32_t>> assetNodes;
	{
		size_t total = asset.getNodes().size();
		for (auto &f : asset.getFunctions()) { total += f.nodes.size(); }
		assetNodes.reserve(total);
		for (auto &n : asset.getNodes()) { assetNodes.emplace_back(&n, 0); }
		uint32_t body = 1;
		for (auto &f : asset.getFunctions()) {
			for (auto &n : f.nodes) { assetNodes.emplace_back(&n, body); }
			++body;
		}
		_functionCount = body - 1;
		if (_functionCount > 0) {
			sprt::sort(assetNodes.begin(), assetNodes.end(),
					[](const sprt::pair<const GraphNode *, uint32_t> &a,
							const sprt::pair<const GraphNode *, uint32_t> &b) {
				return a.first->id < b.first->id;
			});
		}
	}
	_nodes.reserve(assetNodes.size());
	_bodyOf.reserve(assetNodes.size());

	uint32_t constantCount = 0;
	uint32_t settingCount = 0;
	uint32_t execOutSlots = 0;
	for (auto &it : assetNodes) {
		auto &n = *it.first;
		_bodyOf.emplace_back(it.second);
		RuntimeNode node;
		node.id = n.id;
		node.source = &n;
		node.op = ops.get(n.op);

		if (!node.op) {
			report.reportNode(DiagSeverity::Error, DiagCode::UnknownOp, n.id,
					DiagText(DiagDetail::UnknownOp).name(n.op));
		} else {
			node.localSchema = node.op->getLocalSchema();
			node.constantBegin = constantCount;
			constantCount += uint32_t(node.op->getDataIn().size());
			node.settingBegin = settingCount;
			settingCount += uint32_t(node.op->getSettings().size());
			execOutSlots += uint32_t(node.op->getExecOut().size());

			if (node.op->getFamilyPin() != NullPin) {
				node.familyName = readNodeFamilyName(*node.op, n.params);
				if (node.familyName.empty()) {
					report.reportPin(DiagSeverity::Error, DiagCode::EnumFamilyMissing, n.id,
							node.op->getDataIn()[node.op->getFamilyPin()].name,
							DiagText(DiagDetail::EnumFamilyMissing));
				} else {
					node.family = value::makeTypeId(node.familyName);
				}
			}

			// The asset was written against another shape of this operation. Not a refusal: the
			// edges that reference pins which no longer exist will say precisely what moved, and
			// this only names the cause once instead of leaving the author to infer it.
			if (n.opHash != 0 && n.opHash != node.op->getSignatureHash()) {
				report.reportNode(DiagSeverity::Warning, DiagCode::SignatureDrift, n.id,
						DiagText(DiagDetail::SignatureDrift).name(n.op));
			}
		}

		_nodes.emplace_back(sprt::move(node));
	}

	_constants.resize(constantCount);
	_settings.resize(settingCount);

	// One slot per data input and per exec output, for the arity check. A flat vector rather than a
	// per-node structure: the graph is bounded and this is one allocation.
	mem_std::Vector<uint8_t> inputTaken(constantCount, uint8_t(0));
	mem_std::Vector<uint8_t> execTaken(execOutSlots, uint8_t(0));
	mem_std::Vector<uint32_t> execSlotBegin(_nodes.size(), 0);
	{
		uint32_t cursor = 0;
		for (uint32_t i = 0; i < uint32_t(_nodes.size()); ++i) {
			execSlotBegin[i] = cursor;
			if (_nodes[i].op) {
				cursor += uint32_t(_nodes[i].op->getExecOut().size());
			}
		}
	}

	// Edges: the root body's, then each function's, every one in its canonical order.

	mem_std::Vector<const GraphEdge *> assetEdges;
	for (auto &e : asset.getEdges()) { assetEdges.emplace_back(&e); }
	for (auto &f : asset.getFunctions()) {
		for (auto &e : f.edges) { assetEdges.emplace_back(&e); }
	}

	for (auto edgePtr : assetEdges) {
		auto &e = *edgePtr;
		auto srcIndex = findNode(e.from);
		auto dstIndex = findNode(e.to);
		if (srcIndex == InvalidIndex || dstIndex == InvalidIndex) {
			report.reportEdge(DiagSeverity::Error, DiagCode::UnknownNode, e.from, e.fromPin, e.to,
					e.toPin, DiagText(DiagDetail::EdgeUnknownNode));
			continue;
		}

		auto &src = _nodes[srcIndex];
		auto &dst = _nodes[dstIndex];
		if (!src.op || !dst.op) {
			continue; // the missing operation is already reported; anything else would be noise
		}

		if (e.kind == EdgeKind::Data) {
			uint32_t srcPin = 0;
			uint32_t dstPin = 0;
			uint32_t other = 0;

			if (!src.op->findDataOut(e.fromPin, srcPin)) {
				auto code = src.op->findDataIn(e.fromPin, other) ? DiagCode::PinKindMismatch
																 : DiagCode::UnknownPin;
				report.reportEdge(DiagSeverity::Error, code, e.from, e.fromPin, e.to, e.toPin,
						DiagText(DiagDetail::NoDataOut).name(src.op->getName()).name(e.fromPin));
				continue;
			}
			if (!dst.op->findDataIn(e.toPin, dstPin)) {
				auto code = dst.op->findDataOut(e.toPin, other) ? DiagCode::PinKindMismatch
																: DiagCode::UnknownPin;
				report.reportEdge(DiagSeverity::Error, code, e.from, e.fromPin, e.to, e.toPin,
						DiagText(DiagDetail::NoDataIn).name(dst.op->getName()).name(e.toPin));
				continue;
			}

			auto slot = dst.constantBegin + dstPin;
			if (inputTaken[slot]) {
				report.reportEdge(DiagSeverity::Error, DiagCode::PinArity, e.from, e.fromPin, e.to,
						e.toPin, DiagText(DiagDetail::DataInArity));
				continue;
			}

			if (dstPin == dst.op->getFamilyPin()) {
				report.reportEdge(DiagSeverity::Error, DiagCode::EnumFamilyDynamic, e.from,
						e.fromPin, e.to, e.toPin, DiagText(DiagDetail::EnumFamilyDynamic));
				continue;
			}

			auto fromPin = nodeDataOut(*src.op, srcPin, src.family);
			auto &toPin = dst.op->getDataIn()[dstPin];

			value::CastRule rule = value::CastRule::Same;
			DiagCode failure = DiagCode::TypeMismatch;
			if (!edgeTypesMeet(fromPin, toPin, rule, failure)) {
				report.reportEdge(DiagSeverity::Error, failure, e.from, e.fromPin, e.to, e.toPin,
						DiagText(DiagDetail::TypeNoReach).type(fromPin.type).type(toPin.type));
				continue;
			}

			inputTaken[slot] = 1;

			RuntimeDataEdge edge;
			edge.srcNode = srcIndex;
			edge.srcPin = srcPin;
			edge.dstNode = dstIndex;
			edge.dstPin = dstPin;
			edge.cast = rule;
			edge.needsArena = value::castNeedsArena(fromPin.type, toPin.type);
			_dataEdges.emplace_back(edge);
		} else {
			uint32_t srcPin = 0;
			uint32_t other = 0;

			if (!src.op->findExecOut(e.fromPin, srcPin)) {
				auto code = src.op->findDataOut(e.fromPin, other) ? DiagCode::PinKindMismatch
																  : DiagCode::UnknownPin;
				report.reportEdge(DiagSeverity::Error, code, e.from, e.fromPin, e.to, e.toPin,
						DiagText(DiagDetail::NoExecOut).name(src.op->getName()).name(e.fromPin));
				continue;
			}
			if (!dst.op->hasExecIn()) {
				report.reportEdge(DiagSeverity::Error, DiagCode::PinKindMismatch, e.from, e.fromPin,
						e.to, e.toPin, DiagText(DiagDetail::NoExecIn).name(dst.op->getName()));
				continue;
			}

			auto slot = execSlotBegin[srcIndex] + srcPin;
			if (execTaken[slot]) {
				report.reportEdge(DiagSeverity::Error, DiagCode::PinArity, e.from, e.fromPin, e.to,
						e.toPin, DiagText(DiagDetail::ExecOutArity));
				continue;
			}
			execTaken[slot] = 1;

			RuntimeExecEdge edge;
			edge.srcNode = srcIndex;
			edge.srcPin = srcPin;
			edge.dstNode = dstIndex;
			_execEdges.emplace_back(edge);
		}
	}

	// The inputs no edge feeds.

	for (uint32_t i = 0; i < uint32_t(_nodes.size()); ++i) {
		auto &node = _nodes[i];
		if (!node.op) {
			continue;
		}

		auto &params = node.source->params;
		auto pins = node.op->getDataIn();

		for (uint32_t p = 0; p < uint32_t(pins.size()); ++p) {
			auto &pin = pins[p];
			auto &param = params.getValue(pin.name);

			// A collector's input is the values of a block's branches; nothing else can stand for
			// it.
			if (pin.role == PinRole::BranchValue) {
				if (!inputTaken[node.constantBegin + p]) {
					report.reportPin(DiagSeverity::Error, DiagCode::ParallelUnpaired, node.id,
							pin.name, DiagText(DiagDetail::ParallelCollectorNoBlock));
				}
				continue;
			}

			// A parameter is checked whether or not an edge makes it moot: an author who connects
			// an input keeps its literal for when the edge goes away, and a literal that would be
			// refused then should be refused now.
			mem_std::Value resolved;
			if (!param.isNull()) {
				if (resolveConstant(param, pin, node.id, DiagCode::ConstantInvalid, report,
							resolved)
						!= Status::Ok) {
					continue;
				}
			}

			if (inputTaken[node.constantBegin + p]) {
				continue;
			}

			if (resolved.isNull() && !pin.def.isNull()) {
				resolved = pin.def; // already converted, at registration
			}
			if (resolved.isNull() && (pin.flags & PinFlags::Required) != PinFlags::None) {
				report.reportPin(DiagSeverity::Error, DiagCode::MissingInput, node.id, pin.name,
						DiagText(DiagDetail::MissingInput));
				continue;
			}
			_constants[node.constantBegin + p] = sprt::move(resolved);
		}

		// Settings: the same two checks as params, against the declarations instead of the pins.
		auto &settings = node.source->settings;
		auto decls = node.op->getSettings();
		for (uint32_t k = 0; k < uint32_t(decls.size()); ++k) {
			DiagPhrase reason = DiagPhrase::None;
			mem_std::Value resolved;
			if (!resolveSetting(decls[k], settings.getValue(decls[k].name), resolved, reason)) {
				report.reportSetting(DiagSeverity::Error, DiagCode::SettingInvalid, node.id,
						decls[k].name,
						DiagText(DiagDetail::SettingInvalid).name(decls[k].name).phrase(reason));
				continue;
			}
			_settings[node.settingBegin + k] = sprt::move(resolved);
		}
		if (settings.isDictionary()) {
			for (auto &it : settings.asDict()) {
				uint32_t index = 0;
				if (!node.op->findSetting(StringView(it.first), index)) {
					report.reportSetting(DiagSeverity::Error, DiagCode::SettingUnknown, node.id,
							StringView(it.first),
							DiagText(DiagDetail::SettingUnknown).name(node.op->getName()));
				}
			}
		}

		// A parameter that names nothing is a typo, and a typo that is silently ignored is a value
		// the author believes they set.
		if (params.isDictionary()) {
			for (auto &it : params.asDict()) {
				uint32_t index = 0;
				if (!node.op->findDataIn(StringView(it.first), index)) {
					report.reportPin(DiagSeverity::Error, DiagCode::UnknownPin, node.id,
							StringView(it.first),
							DiagText(DiagDetail::UnknownPin).name(node.op->getName()));
				}
			}
		}
	}

	// The graph's side of the scene contract. Here rather than in phase 8 because the names are
	// constants, which phase 3 has only just settled, and because a name that is not a string has
	// to be reported together with everything else rather than after the graph is already built.
	// Here rather than after phase 4 because `inputTaken` is the answer to "a literal, or something
	// an edge computes", and it dies below.
	deriveSceneContract(SpanView<uint8_t>(inputTaken.data(), inputTaken.size()), asset, report);

	// And the scene's side, when the caller brought one. Its errors reach the same bail-out as
	// everything else below, so a graph that does not fit its scene leaves no graph at all.
	if (scene) {
		bindSceneContract(*scene, report);
		if (!report.hasErrors() && anyResolvable(_sceneBindings)) {
			_sceneRegistry = scene;
			_sceneRegistryCount = scene->getCount();
		}
	}

	// The same two gates for the scene's extensions.

	deriveExtensionContract(SpanView<uint8_t>(inputTaken.data(), inputTaken.size()), asset, report);

	if (extensions) {
		bindExtensionContract(*extensions, report);
		if (!report.hasErrors() && anyResolvable(_extBindings)) {
			_extensions = extensions;
			_extensionsEpoch = extensions->getEpoch();
		}
	} else if (!_extBindings.empty() && anyResolvable(_extBindings)) {
		// A graph that names an extension and was built without a holder. Not an error - that is
		// exactly what an editor with no scene open does, and what every graph test does - but it
		// leaves the bindings unresolved, and the interpreter refuses to run such a node rather
		// than falling back to something. There is nothing to fall back to: an extension is a host
		// object, not a name that can be hashed.
		for (auto &binding : _extBindings) { binding.instance = nullptr; }
	}

	// And the named entities: names now, ids when the graph is bound to the project's table.
	deriveNamedContract(SpanView<uint8_t>(inputTaken.data(), inputTaken.size()), report);

	// Adjacency, by counting sort so the canonical edge order survives.

	auto buildIndex = [](uint32_t nodeCount, uint32_t edgeCount,
							  const Callback<uint32_t(uint32_t)> &nodeOf,
							  mem_std::Vector<uint32_t> &index, mem_std::Vector<uint32_t> &begin,
							  mem_std::Vector<uint32_t> &count) {
		begin.clear();
		begin.resize(nodeCount, 0);
		count.clear();
		count.resize(nodeCount, 0);
		for (uint32_t e = 0; e < edgeCount; ++e) { ++count[nodeOf(e)]; }
		uint32_t cursor = 0;
		for (uint32_t n = 0; n < nodeCount; ++n) {
			begin[n] = cursor;
			cursor += count[n];
		}
		mem_std::Vector<uint32_t> fill(begin);
		index.clear();
		index.resize(edgeCount, 0);
		for (uint32_t e = 0; e < edgeCount; ++e) { index[fill[nodeOf(e)]++] = e; }
	};

	auto nodeCount = uint32_t(_nodes.size());
	mem_std::Vector<uint32_t> begin;
	mem_std::Vector<uint32_t> count;

	buildIndex(nodeCount, uint32_t(_dataEdges.size()),
			[&](uint32_t e) { return _dataEdges[e].dstNode; }, _dataInIndex, begin, count);
	for (uint32_t n = 0; n < nodeCount; ++n) {
		_nodes[n].dataInBegin = begin[n];
		_nodes[n].dataInCount = count[n];
	}

	buildIndex(nodeCount, uint32_t(_dataEdges.size()),
			[&](uint32_t e) { return _dataEdges[e].srcNode; }, _dataOutIndex, begin, count);
	for (uint32_t n = 0; n < nodeCount; ++n) {
		_nodes[n].dataOutBegin = begin[n];
		_nodes[n].dataOutCount = count[n];
	}

	buildIndex(nodeCount, uint32_t(_execEdges.size()),
			[&](uint32_t e) { return _execEdges[e].dstNode; }, _execInIndex, begin, count);
	for (uint32_t n = 0; n < nodeCount; ++n) {
		_nodes[n].execInBegin = begin[n];
		_nodes[n].execInCount = count[n];
	}

	buildIndex(nodeCount, uint32_t(_execEdges.size()),
			[&](uint32_t e) { return _execEdges[e].srcNode; }, _execOutIndex, begin, count);
	for (uint32_t n = 0; n < nodeCount; ++n) {
		_nodes[n].execOutBegin = begin[n];
		_nodes[n].execOutCount = count[n];
	}

	// Entry points, terminals, and the node that can never fire.

	for (uint32_t n = 0; n < nodeCount; ++n) {
		auto &node = _nodes[n];
		if (!node.op) {
			continue;
		}

		// A function's body starts when a call opens it, never with the run.
		node.isEntry = node.dataInCount == 0 && node.execInCount == 0 && !node.op->hasExecIn()
				&& _bodyOf[n] == 0;
		node.isTerminal = node.dataOutCount == 0 && node.execOutCount == 0;

		if (node.isEntry) {
			_entryNodes.emplace_back(n);
		}
		if (node.isTerminal) {
			_terminalNodes.emplace_back(n);
		}

		// Not the same thing as a branch that was not taken this frame (which is a normal end of a
		// path): this node has nowhere to be told to run from, ever. Still a valid graph - a
		// half-wired node is the normal state of one being edited - but never silently.
		if (node.op->hasExecIn() && node.execInCount == 0) {
			report.reportNode(DiagSeverity::Warning, DiagCode::Unreachable, node.id,
					DiagText(DiagDetail::Unreachable));
		}
	}

	// Cycles. Over data edges a cycle is an error: a value would be its own input. Over exec edges
	// it is a loop, which is the point of having exec edges at all - and its back edges are marked
	// here, where the traversal order is canonical, for the interpreter to open an activation on.

	enum class Colour : uint8_t {
		White,
		Grey,
		Black
	};

	{
		mem_std::Vector<Colour> colour(nodeCount, Colour::White);
		mem_std::Vector<uint32_t> stack;
		mem_std::Vector<uint32_t> path;

		// Iterative, because a graph is author data and a deep chain must not be a stack overflow.
		// Each frame is (node, next adjacency index) packed into the two vectors below.
		mem_std::Vector<uint32_t> frameNode;
		mem_std::Vector<uint32_t> frameEdge;

		for (uint32_t root = 0; root < nodeCount; ++root) {
			if (colour[root] != Colour::White) {
				continue;
			}
			frameNode.emplace_back(root);
			frameEdge.emplace_back(0);
			colour[root] = Colour::Grey;
			path.emplace_back(root);

			while (!frameNode.empty()) {
				auto n = frameNode.back();
				auto edges = getDataOutEdges(n);
				if (frameEdge.back() < edges.size()) {
					auto edgeIndex = edges[frameEdge.back()];
					++frameEdge.back();
					auto next = _dataEdges[edgeIndex].dstNode;

					if (colour[next] == Colour::Grey) {
						// The cycle is the tail of the current path starting at `next`. Reported by
						// ascending id rather than in discovery order, so the diagnostic is a
						// property of the graph and not of where the walk happened to start.
						mem_std::Vector<NodeId> cycle;
						bool collecting = false;
						for (auto p : path) {
							if (p == next) {
								collecting = true;
							}
							if (collecting) {
								cycle.emplace_back(_nodes[p].id);
							}
						}
						sprt::sort(cycle.begin(), cycle.end());

						mem_std::String list;
						for (uint32_t i = 0; i < uint32_t(cycle.size()); ++i) {
							if (i > 0) {
								list.append(", ");
							}
							list.append(mem_std::toString(cycle[i]));
						}
						report.reportNode(DiagSeverity::Error, DiagCode::DataCycle, cycle.front(),
								DiagText(DiagDetail::DataCycle).name(list));
					} else if (colour[next] == Colour::White) {
						colour[next] = Colour::Grey;
						path.emplace_back(next);
						frameNode.emplace_back(next);
						frameEdge.emplace_back(0);
					}
				} else {
					colour[n] = Colour::Black;
					path.pop_back();
					frameNode.pop_back();
					frameEdge.pop_back();
				}
			}
		}
	}

	{
		mem_std::Vector<Colour> colour(nodeCount, Colour::White);
		mem_std::Vector<uint32_t> frameNode;
		mem_std::Vector<uint32_t> frameEdge;

		// From the entry points first, in index order, then from whatever the entries could not
		// reach - a component with no entry still has loops, and leaving its edges unclassified
		// would make the marking depend on how the graph was wired rather than on its shape.
		mem_std::Vector<uint32_t> roots(_entryNodes);
		for (uint32_t n = 0; n < nodeCount; ++n) { roots.emplace_back(n); }

		for (auto root : roots) {
			if (colour[root] != Colour::White) {
				continue;
			}
			colour[root] = Colour::Grey;
			frameNode.emplace_back(root);
			frameEdge.emplace_back(0);

			while (!frameNode.empty()) {
				auto n = frameNode.back();
				auto edges = getExecOutEdges(n);
				if (frameEdge.back() < edges.size()) {
					auto edgeIndex = edges[frameEdge.back()];
					++frameEdge.back();
					auto next = _execEdges[edgeIndex].dstNode;
					if (colour[next] == Colour::Grey) {
						_execEdges[edgeIndex].backEdge = true;
					} else if (colour[next] == Colour::White) {
						colour[next] = Colour::Grey;
						frameNode.emplace_back(next);
						frameEdge.emplace_back(0);
					}
				} else {
					colour[n] = Colour::Black;
					frameNode.pop_back();
					frameEdge.pop_back();
				}
			}
		}
	}

	// Loop bodies, after the cycle pass, because a back edge is what a loop closes with and the
	// walk here has to see the same graph the walk there did. Skipped when the graph is already
	// broken: scopes over wiring that does not resolve would produce diagnostics about a shape
	// nobody wrote.

	if (!report.hasErrors()) {
		assignScopes(report);
		if (_shapeSink) {
			takeShape();
		}
	}

	// Parallel blocks, over the scopes just assigned (SPFlowParallel.cc): what the machine needs
	// to run them, then what the build refuses about them.
	if (!report.hasErrors()) {
		deriveBlocks(report);
	}
	if (!report.hasErrors()) {
		analyzeParallel(report, scene);
	}
	if (!report.hasErrors()) {
		checkGpuLowering(report);
	}

	if (report.hasErrors()) {
		clearBuilt();
		return report.getStatus();
	}

	// Derived data: everything below is a function of what the phases above settled, cached here so
	// that the interpreter reads it instead of recomputing it on every step. It runs last because
	// it depends on scopes, and only on success, because a graph about to be discarded is not worth
	// deriving anything from.
	deriveNodeData();

	_built = true;
	adviseEagerCost(report);
	return Status::Ok;
}

SpanView<SceneBinding> RuntimeGraph::getSceneBindings(uint32_t node) const {
	auto &n = _nodes[node];
	// Nothing at all unless the graph was actually bound, and that distinction is load-bearing. The
	// contract is derived without a scene, so an unbound graph holds a full set of bindings with no
	// descriptors in them - and an operation handed one of those cannot tell "the scene does not
	// have this component", which is an answer, from "nobody ever asked a scene", which is not.
	// Handing it nothing is what sends it down the path it took before bindings existed.
	if (!_sceneRegistry || n.sceneCount == 0) {
		return SpanView<SceneBinding>();
	}
	return SpanView<SceneBinding>(_sceneBindings.data() + n.sceneBegin, n.sceneCount);
}

SpanView<SceneBinding> RuntimeGraph::getSceneContract(uint32_t node) const {
	auto &n = _nodes[node];
	// No _sceneRegistry test, and that is the whole difference: the names are what a reader of the
	// contract wants and the contract was derived with no scene in reach, so withholding them until
	// a scene has answered would answer a question nobody asked. See the header for who may call
	// this and what they may read out of it.
	if (n.sceneCount == 0) {
		return SpanView<SceneBinding>();
	}
	return SpanView<SceneBinding>(_sceneBindings.data() + n.sceneBegin, n.sceneCount);
}

SpanView<NamedBinding> RuntimeGraph::getNamedContract(uint32_t node) const {
	auto &n = _nodes[node];
	if (n.namedCount == 0) {
		return SpanView<NamedBinding>();
	}
	return SpanView<NamedBinding>(_namedBindings.data() + n.namedBegin, n.namedCount);
}

SpanView<NamedBinding> RuntimeGraph::getNamedBindings(uint32_t node) const {
	return _namedBound ? getNamedContract(node) : SpanView<NamedBinding>();
}

SpanView<ExtensionBinding> RuntimeGraph::getExtensionContract(uint32_t node) const {
	auto &n = _nodes[node];
	if (n.extCount == 0) {
		return SpanView<ExtensionBinding>();
	}
	return SpanView<ExtensionBinding>(_extBindings.data() + n.extBegin, n.extCount);
}

SpanView<ExtensionBinding> RuntimeGraph::getExtensionBindings(uint32_t node) const {
	auto &n = _nodes[node];
	// The same rule, with one difference in what it means: an operation handed nothing here has no
	// older path to fall back to, so it refuses. That is not a regression - a graph that names an
	// extension nobody resolved has nothing to run against.
	if (!_extensions || n.extCount == 0) {
		return SpanView<ExtensionBinding>();
	}
	return SpanView<ExtensionBinding>(_extBindings.data() + n.extBegin, n.extCount);
}

// The graph's side of the scene contract. Everything decided here is decidable without a scene, and
// that is the point: an editor with no scene loaded still refuses a node naming a component this
// graph's own declaration does not have, and still refuses reading an Int out of a field the
// declaration calls a Float. Resolving those names against a real registry is a second gate,
// bindScene, and only that one needs a scene.
void RuntimeGraph::deriveSceneContract(SpanView<uint8_t> inputTaken, const GraphAsset &asset,
		DiagReport &report) {
	_sceneBindings.clear();

	auto declared = asset.getScene();

	// One flag per declared component and one per declared field, so that "declared and named by
	// nobody" falls out of the same walk instead of costing a second one.
	mem_std::Vector<uint32_t> usedBegin(declared.size(), 0);
	uint32_t usedCount = 0;
	for (uint32_t d = 0; d < uint32_t(declared.size()); ++d) {
		usedBegin[d] = usedCount;
		usedCount += 1 + uint32_t(declared[d].fields.size());
	}
	mem_std::Vector<uint8_t> used(usedCount, uint8_t(0));

	auto findDecl = [&](StringView component) -> uint32_t {
		for (uint32_t d = 0; d < uint32_t(declared.size()); ++d) {
			if (declared[d].component == component) {
				return d;
			}
		}
		return InvalidIndex;
	};

	for (uint32_t i = 0; i < uint32_t(_nodes.size()); ++i) {
		auto &node = _nodes[i];
		node.sceneBegin = uint32_t(_sceneBindings.size());
		node.sceneCount = 0;

		if (!node.op) {
			continue;
		}
		auto groups = node.op->getSceneGroups();
		if (groups.empty()) {
			continue;
		}

		auto refs = node.op->getSceneRefs();
		auto pinGroups = uint32_t(groups.size() - refs.size());

		// A literal, or nothing at all. An input an edge feeds is a name nobody can know here; an
		// input the node simply did not fill in was already reported by phase 3, and reporting it
		// again in another shape would only make the list longer.
		auto literal = [&](uint32_t pin, StringView &out) -> bool {
			if (inputTaken[node.constantBegin + pin]) {
				return false;
			}
			auto value = getConstant(i, pin);
			if (!value || !value->isString()) {
				return false;
			}
			// Into the constant table, which the build sizes once in phase 1 and never resizes, so
			// this view is good for as long as the graph is.
			out = StringView(value->getString());
			return !out.empty();
		};

		for (uint32_t g = 0; g < uint32_t(groups.size()); ++g) {
			auto &grp = groups[g];
			SceneBinding binding;

			if (g >= pinGroups) {
				// What the operation names itself. Not checked against the asset's declaration:
				// that section says what the graph wants of the scene, and this is the node
				// library's own business, which no author wrote down and none should have to.
				auto &ref = refs[g - pinGroups];
				binding.component = ref.component;
				binding.field = ref.field;
				binding.componentId = makeTypeId(ref.component);
				binding.declared = ref.type;
				binding.optional = ref.optional;
				_sceneBindings.emplace_back(sprt::move(binding));
				++node.sceneCount;
				continue;
			}

			binding.optional = grp.optional;

			// One name on an edge makes the whole group dynamic. The operation checks one flag and
			// takes the old path for all of it, rather than holding half a resolution.
			if (!literal(grp.componentPin, binding.component)
					|| (grp.fieldPin != NullPin && !literal(grp.fieldPin, binding.field))) {
				binding.component = StringView();
				binding.field = StringView();
				binding.dynamic = true;
				report.reportPin(DiagSeverity::Advice, DiagCode::SceneNameDynamic, node.id,
						node.op->getDataIn()[grp.componentPin].name,
						DiagText(DiagDetail::SceneNameDynamic));
				_sceneBindings.emplace_back(sprt::move(binding));
				++node.sceneCount;
				continue;
			}

			binding.componentId = makeTypeId(binding.component);

			// What the graph says the field's type is. The value pin's own type to begin with; the
			// declaration overrides it below, because the declaration is what the author wrote and
			// the pin is what the operation happens to be spelled as.
			if (grp.valuePin != NullPin) {
				binding.declared = grp.valueIsOutput ? node.op->getDataOut()[grp.valuePin].type
													 : node.op->getDataIn()[grp.valuePin].type;
			}

			if (!declared.empty()) {
				auto d = findDecl(binding.component);
				if (d == InvalidIndex) {
					report.reportNode(DiagSeverity::Error, DiagCode::SceneUndeclared, node.id,
							DiagText(DiagDetail::SceneUndeclared).name(binding.component));
				} else {
					used[usedBegin[d]] = 1;
					if (!binding.field.empty()) {
						uint32_t f = InvalidIndex;
						for (uint32_t k = 0; k < uint32_t(declared[d].fields.size()); ++k) {
							if (declared[d].fields[k].name == binding.field) {
								f = k;
								break;
							}
						}
						if (f == InvalidIndex) {
							report.reportNode(DiagSeverity::Error, DiagCode::SceneUndeclared,
									node.id,
									DiagText(DiagDetail::SceneNoField)
											.name(binding.component)
											.name(binding.field));
						} else {
							used[usedBegin[d] + 1 + f] = 1;
							auto declaredType = declared[d].fields[f].type;
							// Exactly, not compatibly: `scene.getInt` refuses a Var that is not an
							// Int, so a widening read is a step that fails, and the whole point of
							// saying it here is not to find that out on the frame it first runs.
							if (binding.declared != VarType::Nil
									&& declaredType != binding.declared) {
								report.reportNode(DiagSeverity::Error, DiagCode::SceneFieldType,
										node.id,
										DiagText(DiagDetail::SceneTypeDeclared)
												.name(binding.component)
												.name(binding.field)
												.type(declaredType)
												.type(binding.declared));
							}
							binding.declared = declaredType;
						}
					}
				}
			}

			_sceneBindings.emplace_back(sprt::move(binding));
			++node.sceneCount;
		}
	}

	// Component names a node lists in its settings (a query's `with`/`without`): declared like the
	// ones its pins name, and used by that.
	for (uint32_t i = 0; i < uint32_t(_nodes.size()); ++i) {
		auto &node = _nodes[i];
		if (!node.op || declared.empty()) {
			continue;
		}
		auto decls = node.op->getSettings();
		for (uint32_t k = 0; k < uint32_t(decls.size()); ++k) {
			auto value = getSetting(i, k);
			if (decls[k].role != SettingRole::ComponentNames || !value || !value->isArray()) {
				continue;
			}
			for (auto &item : value->asArray()) {
				auto name = StringView(item.getString());
				auto d = findDecl(name);
				if (d == InvalidIndex) {
					report.reportSetting(DiagSeverity::Error, DiagCode::SceneUndeclared, node.id,
							decls[k].name, DiagText(DiagDetail::SceneUndeclared).name(name));
				} else {
					used[usedBegin[d]] = 1;
				}
			}
		}
	}

	// A declaration nobody uses is not wrong, but it is almost always a leftover: the node that
	// named it was deleted and the contract was not.
	for (uint32_t d = 0; d < uint32_t(declared.size()); ++d) {
		if (!used[usedBegin[d]]) {
			report.report(DiagSeverity::Advice, DiagCode::SceneUnused,
					DiagText(DiagDetail::SceneUnusedComponent).name(declared[d].component));
			continue;
		}
		for (uint32_t k = 0; k < uint32_t(declared[d].fields.size()); ++k) {
			if (!used[usedBegin[d] + 1 + k]) {
				report.report(DiagSeverity::Advice, DiagCode::SceneUnused,
						DiagText(DiagDetail::SceneUnusedField)
								.name(declared[d].component)
								.name(declared[d].fields[k].name));
			}
		}
	}
}

// The scene's side of the contract: the one place a name becomes a descriptor. After this every
// bound group holds the pointers the interpreter used to fetch by scanning the registry and
// comparing field names on every call: the decisions are
// made once, at registration and here, so that this layer makes none.
void bindSceneBindings(SpanView<RuntimeNode> nodes, mem_std::Vector<SceneBinding> &bindings,
		const value::TypeRegistry &registry, DiagReport &report) {
	for (uint32_t i = 0; i < uint32_t(nodes.size()); ++i) {
		auto &node = nodes[i];
		if (!node.familyName.empty()) {
			auto family = registry.getEnum(node.familyName);
			if (!family) {
				report.reportNode(DiagSeverity::Error, DiagCode::EnumFamilyUnknown, node.id,
						DiagText(DiagDetail::EnumFamilyUnknown).name(node.familyName));
			} else if (family->getId() != node.family) {
				report.reportNode(DiagSeverity::Error, DiagCode::EnumFamilyAlias, node.id,
						DiagText(DiagDetail::EnumFamilyAlias)
								.name(node.familyName)
								.name(family->getName()));
			}
		}
		for (uint32_t k = 0; k < node.sceneCount; ++k) {
			auto &binding = bindings[node.sceneBegin + k];
			binding.type = nullptr;
			binding.desc = nullptr;

			if (binding.dynamic) {
				continue;
			}

			auto type = registry.get(binding.componentId);
			if (!type) {
				// An optional group is asking whether the scene has this, and "no" is an answer.
				// Said as an advice all the same: it is also what a misspelt component name looks
				// like, and the author is the only one who can tell the two apart.
				report.reportNode(binding.optional ? DiagSeverity::Advice : DiagSeverity::Error,
						DiagCode::SceneUnknownComponent, node.id,
						DiagText(DiagDetail::SceneNoComponent).name(binding.component));
				continue;
			}
			binding.type = type;

			if (binding.field.empty()) {
				continue;
			}

			auto desc = type->getField(binding.field);
			if (!desc) {
				report.reportNode(DiagSeverity::Error, DiagCode::SceneUnknownField, node.id,
						DiagText(DiagDetail::SceneNoFieldNamed)
								.name(binding.component)
								.name(binding.field));
				continue;
			}
			if (binding.declared != VarType::Nil && desc->type != binding.declared) {
				report.reportNode(DiagSeverity::Error, DiagCode::SceneFieldType, node.id,
						DiagText(DiagDetail::SceneTypeHere)
								.name(binding.component)
								.name(binding.field)
								.type(desc->type)
								.type(binding.declared));
				continue;
			}
			if (desc->type == VarType::Enum && node.family != value::NullTypeId
					&& desc->subtypeId != node.family) {
				auto fieldFamily = registry.getEnum(desc->subtypeId);
				report.reportNode(DiagSeverity::Error, DiagCode::SceneFieldType, node.id,
						DiagText(DiagDetail::SceneEnumFamilyHere)
								.name(binding.component)
								.name(binding.field)
								.name(fieldFamily ? fieldFamily->getName() : StringView("?"))
								.name(node.familyName));
				continue;
			}
			binding.desc = desc;
		}
	}
}

void RuntimeGraph::bindSceneContract(const value::TypeRegistry &registry, DiagReport &report) {
	bindSceneBindings(_nodes, _sceneBindings, registry, report);

	for (uint32_t i = 0; i < uint32_t(_nodes.size()); ++i) {
		auto &node = _nodes[i];
		if (!node.op) {
			continue;
		}
		auto decls = node.op->getSettings();
		for (uint32_t k = 0; k < uint32_t(decls.size()); ++k) {
			auto value = getSetting(i, k);
			if (decls[k].role != SettingRole::ComponentNames || !value || !value->isArray()) {
				continue;
			}
			for (auto &item : value->asArray()) {
				auto name = StringView(item.getString());
				if (!registry.get(makeTypeId(name))) {
					report.reportSetting(DiagSeverity::Error, DiagCode::SceneUnknownComponent,
							node.id, decls[k].name,
							DiagText(DiagDetail::SceneNoComponent).name(name));
				}
			}
		}
	}
}

// What a node says about an extension, against what the file declares. No scene, and that is the
// whole reason the "extensions" section exists - a typo in an instance id is refused on the
// keystroke that made it rather than on the frame that reaches the node. Sharper than the scene's
// equivalent in one respect: a component name has a fallback, since makeTypeId is a pure function
// and an operation handed no binding can still work the name out, while an extension id has none -
// what stands behind it is a host object - so a graph with no declaration to check against gets
// errors here rather than a contract derived from the node parameters.
void RuntimeGraph::deriveExtensionContract(SpanView<uint8_t> inputTaken, const GraphAsset &asset,
		DiagReport &report) {
	_extBindings.clear();
	_extDecls.clear();

	auto declared = asset.getExtensions();
	mem_std::Vector<uint8_t> used(declared.size(), uint8_t(0));

	// Pointers into the asset, which outlives the graph - the same lifetime a RuntimeNode's
	// `source` already depends on. Kept so that the parameter check has something to compare
	// against without the graph holding a second copy of what the file said.
	_extDecls.reserve(declared.size());
	for (auto &decl : declared) { _extDecls.emplace_back(&decl); }

	auto findDecl = [&](StringView id) -> uint32_t {
		for (uint32_t d = 0; d < uint32_t(declared.size()); ++d) {
			if (declared[d].id == id) {
				return d;
			}
		}
		return InvalidIndex;
	};

	for (uint32_t i = 0; i < uint32_t(_nodes.size()); ++i) {
		auto &node = _nodes[i];
		node.extBegin = uint32_t(_extBindings.size());
		node.extCount = 0;

		if (!node.op) {
			continue;
		}
		auto groups = node.op->getExtensionGroups();
		if (groups.empty()) {
			continue;
		}

		for (auto &grp : groups) {
			ExtensionBinding binding;

			StringView id;
			auto value = inputTaken[node.constantBegin + grp.namePin] ? nullptr
																	  : getConstant(i, grp.namePin);
			if (value && value->isString()) {
				id = StringView(value->getString());
			}

			if (id.empty()) {
				binding.dynamic = true;
				report.reportPin(DiagSeverity::Advice, DiagCode::ExtNameDynamic, node.id,
						node.op->getDataIn()[grp.namePin].name,
						DiagText(DiagDetail::ExtNameDynamic));
				_extBindings.emplace_back(sprt::move(binding));
				++node.extCount;
				continue;
			}

			binding.id = id;

			auto d = findDecl(id);
			if (d == InvalidIndex) {
				report.reportNode(DiagSeverity::Error, DiagCode::ExtUndeclared, node.id,
						DiagText(DiagDetail::ExtUndeclared).name(id));
			} else {
				used[d] = 1;
				binding.name = declared[d].name;
			}

			_extBindings.emplace_back(sprt::move(binding));
			++node.extCount;
		}
	}

	// Same advice as the scene's, and the same cause: the node that named it was deleted and the
	// declaration was not. Louder here in one respect - a declared instance is something a host
	// will go and create, so an unused one is a block in an arena nobody reads.
	for (uint32_t d = 0; d < uint32_t(declared.size()); ++d) {
		if (!used[d]) {
			report.report(DiagSeverity::Advice, DiagCode::ExtUnused,
					DiagText(DiagDetail::ExtUnused).name(declared[d].id).name(declared[d].name));
		}
	}
}

// The scene's side: a declaration against the live instances, and the one place an id becomes a
// pointer. Three ways it can fail, kept apart because they are three different mistakes: the
// process was built without the extension, the scene was built without the instance, and the
// scene's instance is not what the graph thinks it is. Collapsing them into one "cannot bind" would
// leave the author guessing which of their three files to open.
void bindExtensionBindings(SpanView<RuntimeNode> nodes, mem_std::Vector<ExtensionBinding> &bindings,
		SpanView<const ExtensionDecl *> decls, const value::ExtensionHost &extensions,
		DiagReport &report) {
	// Once per declaration, before the per-node walk. A parameter mismatch is a fact about the file
	// and the scene, not about any one node, and reporting it from inside the node loop would name
	// three nodes for one mistake.
	for (auto decl : decls) {
		auto instance = extensions.findExtensionInstance(decl->id);
		if (!instance || instance->getExtensionName() != decl->name) {
			continue; // said below, where there is a node to name
		}
		if (!instance->matchExtensionParams(decl->params)) {
			report.report(DiagSeverity::Error, DiagCode::ExtParamMismatch,
					DiagText(DiagDetail::ExtSceneMismatch).name(decl->id).name(decl->name));
		}
	}

	for (uint32_t i = 0; i < uint32_t(nodes.size()); ++i) {
		auto &node = nodes[i];
		for (uint32_t k = 0; k < node.extCount; ++k) {
			auto &binding = bindings[node.extBegin + k];
			binding.instance = nullptr;

			if (binding.dynamic || binding.name.empty()) {
				// Undeclared was already reported by 3c; saying it again from here would name the
				// same node twice for one mistake.
				continue;
			}

			if (!extensions.hasExtensionDefinition(binding.name)) {
				report.reportNode(DiagSeverity::Error, DiagCode::ExtUnknown, node.id,
						DiagText(DiagDetail::ExtUnknown).name(binding.name));
				continue;
			}
			auto instance = extensions.findExtensionInstance(binding.id);
			if (!instance) {
				report.reportNode(DiagSeverity::Error, DiagCode::ExtInstanceMissing, node.id,
						DiagText(DiagDetail::ExtInstanceMissing)
								.name(binding.name)
								.name(binding.id));
				continue;
			}
			if (instance->getExtensionName() != binding.name) {
				report.reportNode(DiagSeverity::Error, DiagCode::ExtParamMismatch, node.id,
						DiagText(DiagDetail::ExtInstanceKind)
								.name(binding.id)
								.name(instance->getExtensionName())
								.name(binding.name));
				continue;
			}
			binding.instance = instance;
		}
	}
}

void RuntimeGraph::bindExtensionContract(const value::ExtensionHost &extensions,
		DiagReport &report) {
	bindExtensionBindings(_nodes, _extBindings, _extDecls, extensions, report);
}

// One binding per EntityName pin, in pin order. A literal is a name; a name on an edge is an advice,
// and the operation refuses at run time rather than guessing.
void RuntimeGraph::deriveNamedContract(SpanView<uint8_t> inputTaken, DiagReport &report) {
	_namedBindings.clear();
	for (uint32_t i = 0; i < uint32_t(_nodes.size()); ++i) {
		auto &node = _nodes[i];
		node.namedBegin = uint32_t(_namedBindings.size());
		node.namedCount = 0;
		if (!node.op) {
			continue;
		}
		auto pins = node.op->getDataIn();
		for (uint32_t p = 0; p < uint32_t(pins.size()); ++p) {
			if (pins[p].role != PinRole::EntityName) {
				continue;
			}
			NamedBinding binding;
			auto value = inputTaken[node.constantBegin + p] ? nullptr : getConstant(i, p);
			if (value && value->isString() && !value->getString().empty()) {
				binding.name = StringView(value->getString());
			} else {
				binding.dynamic = true;
				report.reportPin(DiagSeverity::Advice, DiagCode::NamedDynamic, node.id,
						pins[p].name, DiagText(DiagDetail::NamedDynamic));
			}
			_namedBindings.emplace_back(sprt::move(binding));
			++node.namedCount;
		}
	}
}

void bindNamedBindings(SpanView<RuntimeNode> nodes, mem_std::Vector<NamedBinding> &bindings,
		const value::NamedHost &table, DiagReport &report) {
	for (uint32_t i = 0; i < uint32_t(nodes.size()); ++i) {
		auto &node = nodes[i];
		for (uint32_t k = 0; k < node.namedCount; ++k) {
			auto &binding = bindings[node.namedBegin + k];
			binding.entity = value::EntityId();
			if (binding.dynamic) {
				continue;
			}
			value::EntityId entity;
			StringView store;
			auto st = table.findNamedEntity(binding.name, entity, store);
			if (st == Status::ErrorNotFound) {
				report.reportNode(DiagSeverity::Error, DiagCode::NamedUnknown, node.id,
						DiagText(DiagDetail::NamedUnknown).name(binding.name));
				continue;
			}
			// The graph runs over the scene and reaches no other store.
			if (st != Status::Ok) {
				report.reportNode(DiagSeverity::Error, DiagCode::NamedArena, node.id,
						DiagText(DiagDetail::NamedArena).name(binding.name).name(store));
				continue;
			}
			binding.entity = entity;
		}
	}
}

Status RuntimeGraph::bindNamed(const value::NamedHost &table, DiagSink *sink) {
	if (!_built) {
		return Status::ErrorInvalidArguemnt;
	}
	DiagReport report(sink);
	bindNamedBindings(_nodes, _namedBindings, table, report);
	if (report.hasErrors()) {
		for (auto &binding : _namedBindings) { binding.entity = value::EntityId(); }
		_namedBound = false;
		return report.getStatus();
	}
	_namedBound = !_namedBindings.empty();
	return Status::Ok;
}

Status RuntimeGraph::bindScene(const value::TypeRegistry &registry, DiagSink *sink) {
	if (!_built) {
		return Status::ErrorInvalidArguemnt;
	}

	DiagReport report(sink);
	bindSceneContract(registry, report);

	if (report.hasErrors()) {
		// Unbound, not half-bound: a graph carrying some resolved descriptors and some not would
		// run half on each path, and which half would depend on the order the errors happened to be
		// in.
		for (auto &binding : _sceneBindings) {
			binding.type = nullptr;
			binding.desc = nullptr;
		}
		_sceneRegistry = nullptr;
		_sceneRegistryCount = 0;
		return report.getStatus();
	}

	if (anyResolvable(_sceneBindings)) {
		_sceneRegistry = &registry;
		_sceneRegistryCount = registry.getCount();
	}
	return Status::Ok;
}

Status RuntimeGraph::bindScene(const value::TypeRegistry &registry,
		const value::ExtensionHost &extensions, DiagSink *sink) {
	if (!_built) {
		return Status::ErrorInvalidArguemnt;
	}

	DiagReport report(sink);
	bindSceneContract(registry, report);
	bindExtensionContract(extensions, report);

	// Both halves or neither. A graph half-bound to a scene and fully bound to its extensions would
	// run one path resolved and the other not, and which is which would depend on the order the
	// errors happened to arrive in - the same rule the scene-only overload states one paragraph up,
	// over one more thing.
	if (report.hasErrors()) {
		for (auto &binding : _sceneBindings) {
			binding.type = nullptr;
			binding.desc = nullptr;
		}
		for (auto &binding : _extBindings) { binding.instance = nullptr; }
		_sceneRegistry = nullptr;
		_sceneRegistryCount = 0;
		_extensions = nullptr;
		_extensionsEpoch = 0;
		return report.getStatus();
	}

	if (anyResolvable(_sceneBindings)) {
		_sceneRegistry = &registry;
		_sceneRegistryCount = registry.getCount();
	}
	if (anyResolvable(_extBindings)) {
		_extensions = &extensions;
		_extensionsEpoch = extensions.getEpoch();
	}
	return Status::Ok;
}

// Readiness, split into the part a node can answer from its own record and the part it cannot. An
// input fed from the node's own scope is marked on its record when the producer runs, so the whole
// question is one mask compare. An input fed from an enclosing scope cannot be marked - when the
// producer runs, the consumer's iteration may not exist yet, and later there may be several of it -
// so it is read off the producer, and those edges are collected here because for almost every node
// there are none.
void RuntimeGraph::deriveNodeData() {
	_crossScopeInIndex.clear();
	_crossScopeInIndex.reserve(_dataEdges.size());

	for (uint32_t n = 0; n < uint32_t(_nodes.size()); ++n) {
		auto &node = _nodes[n];
		node.sameScopeInputs = 0;
		node.allInputs = 0;
		node.crossScopeInBegin = uint32_t(_crossScopeInIndex.size());

		for (auto e : getDataInEdges(n)) {
			auto &edge = _dataEdges[e];
			node.allInputs |= uint32_t(1) << edge.dstPin;
			if (edge.dstPin == node.op->getBranchPin()) {
				continue; // the values of all branches, delivered with the block
			}
			if (_nodes[edge.srcNode].scope == node.scope) {
				node.sameScopeInputs |= uint32_t(1) << edge.dstPin;
			} else {
				_crossScopeInIndex.emplace_back(e);
			}
		}

		// A fan-out waits for every value its body reads from outside it: the branches read them
		// all at once, and none may be produced while the block is in flight.
		if (node.opensScope != InvalidIndex
				&& _scopes[node.opensScope].kind == ScopeKind::Parallel) {
			for (uint32_t b = 0; b < uint32_t(_nodes.size()); ++b) {
				if (!isScopeWithin(_nodes[b].scope, node.opensScope)) {
					continue;
				}
				for (auto e : getDataInEdges(b)) {
					auto src = _dataEdges[e].srcNode;
					if (src != n && !isScopeWithin(_nodes[src].scope, node.opensScope)) {
						_crossScopeInIndex.emplace_back(e);
					}
				}
			}
		}

		node.crossScopeInCount = uint32_t(_crossScopeInIndex.size()) - node.crossScopeInBegin;
	}
}

// A node without an exec input runs whenever its inputs exist - that is what eager means. Two
// shapes are worth telling an author about: it produces nothing anybody reads (eager-unused), so
// the work is wasted on every run; or everything that reads it may not run (eager-speculative), so
// the work is done every time and used sometimes. "May not run" is decided from the signatures, not
// from a list of known operations: a node is unconditional when it can be reached from an entry
// point over exec edges without ever leaving through an output of a node that has more than one -
// which is what a branch is, whatever it is called.
void RuntimeGraph::adviseEagerCost(DiagReport &report) const {
	auto count = uint32_t(_nodes.size());
	if (count == 0) {
		return;
	}

	mem_std::Vector<uint8_t> unconditional(count, uint8_t(0));
	mem_std::Vector<uint32_t> stack;

	for (auto entry : _entryNodes) {
		if (!unconditional[entry]) {
			unconditional[entry] = 1;
			stack.emplace_back(entry);
		}
	}
	// A function's body starts at its own entries whenever it is called.
	for (uint32_t n = 0; n < count && n < _bodyOf.size(); ++n) {
		auto &node = _nodes[n];
		if (_bodyOf[n] != 0 && node.op && node.dataInCount == 0 && node.execInCount == 0
				&& !node.op->hasExecIn() && !unconditional[n]) {
			unconditional[n] = 1;
			stack.emplace_back(n);
		}
	}
	while (!stack.empty()) {
		auto n = stack.back();
		stack.pop_back();
		auto &node = _nodes[n];
		if (!node.op || node.op->getExecOut().size() > 1) {
			continue; // past a branch, nothing is certain any more
		}
		for (auto e : getExecOutEdges(n)) {
			auto next = _execEdges[e].dstNode;
			if (!unconditional[next]) {
				unconditional[next] = 1;
				stack.emplace_back(next);
			}
		}
	}

	// An eager node is worth its cost if something that certainly runs reads it - directly, or
	// through other eager nodes. Iterated to a fixed point rather than sorted topologically: data
	// edges are acyclic (a cycle is an error), so this converges in at most one pass per node.
	mem_std::Vector<uint8_t> useful(count, uint8_t(0));
	bool changed = true;
	while (changed) {
		changed = false;
		for (uint32_t n = 0; n < count; ++n) {
			if (useful[n] || !_nodes[n].op) {
				continue;
			}
			for (auto e : getDataOutEdges(n)) {
				auto consumer = _dataEdges[e].dstNode;
				auto &target = _nodes[consumer];
				if (!target.op) {
					continue;
				}
				// A function's return is read by its call, which is outside the body.
				const bool returns = target.op->getFunctionRole() == FunctionRole::Return;
				bool certain = target.op->hasExecIn() ? unconditional[consumer] != 0
													  : (useful[consumer] != 0 || returns);
				if (certain) {
					useful[n] = 1;
					changed = true;
					break;
				}
			}
		}
	}

	for (uint32_t n = 0; n < count; ++n) {
		auto &node = _nodes[n];
		if (!node.op || node.op->hasExecIn()) {
			continue; // it waits for a token; it costs nothing until it is asked
		}

		if (node.dataOutCount == 0 && node.execOutCount == 0
				&& node.op->getFunctionRole() != FunctionRole::Return) {
			report.reportNode(DiagSeverity::Advice, DiagCode::EagerUnused, node.id,
					DiagText(DiagDetail::EagerUnused));
			continue;
		}

		if (node.dataOutCount > 0 && !useful[n]) {
			report.reportNode(DiagSeverity::Advice, DiagCode::EagerSpeculative, node.id,
					DiagText(DiagDetail::EagerSpeculative));
		}
	}
}

Status RuntimeGraph::validate(const GraphAsset &asset, const OpRegistry &ops, DiagSink *report) {
	// Builds and throws the result away. Deliberately not a separate traversal that answers the
	// same questions: two implementations of "is this graph well-formed" would agree on the day
	// they were written and on no day after that. The cost is one pool and a few vectors, which is
	// nothing next to being wrong.
	RuntimeGraph scratch;
	if (!scratch.init()) {
		return Status::ErrorOutOfHostMemory;
	}
	return scratch.build(asset, ops, report);
}

Status RuntimeGraph::validate(const GraphAsset &asset, const OpRegistry &ops,
		const value::TypeRegistry &scene, DiagSink *report) {
	// The same, one gate wider. An editor with a scene open gets the scene's answers on the same
	// keystroke as everything else, and gets them from the same function that will build the graph.
	RuntimeGraph scratch;
	if (!scratch.init()) {
		return Status::ErrorOutOfHostMemory;
	}
	return scratch.build(asset, ops, scene, report);
}

Status RuntimeGraph::validate(const GraphAsset &asset, const OpRegistry &ops,
		const value::TypeRegistry &scene, const value::ExtensionHost &extensions,
		DiagSink *report) {
	RuntimeGraph scratch;
	if (!scratch.init()) {
		return Status::ErrorOutOfHostMemory;
	}
	return scratch.build(asset, ops, scene, extensions, report);
}

Status RuntimeGraph::validate(const GraphAsset &asset, const OpRegistry &ops, DiagSink *report,
		GraphShape *shape) {
	return validate(asset, ops, report, shape, nullptr);
}

Status RuntimeGraph::validate(const GraphAsset &asset, const OpRegistry &ops, DiagSink *report,
		GraphShape *shape, const FunctionHost *host) {
	RuntimeGraph scratch;
	if (!scratch.init()) {
		return Status::ErrorOutOfHostMemory;
	}
	scratch._shapeSink = shape;
	scratch._functionHost = host;
	auto st = scratch.build(asset, ops, report);

	// Where the linked ids came from, whether or not the build got as far as the regions: a refused
	// link names its nodes by linked id too.
	if (shape && scratch._link) {
		shape->origins.clear();
		auto ids = scratch._link->getOriginIds();
		auto origins = scratch._link->getOrigins();
		for (uint32_t i = 0; i < uint32_t(ids.size()); ++i) {
			auto &origin = origins[i];
			if (origin.source == ids[i] && origin.callSite == NullNodeId) {
				continue;
			}
			GraphShape::Origin o;
			o.linked = ids[i];
			o.source = origin.source;
			o.callSite = origin.callSite;
			o.document = origin.document.str<mem_std::Interface>();
			o.function = origin.function.str<mem_std::Interface>();
			shape->origins.emplace_back(sprt::move(o));
		}
	}
	return st;
}

void RuntimeGraph::takeShape() {
	auto &shape = *_shapeSink;
	shape.produced = true;
	shape.regions.clear();

	for (uint32_t s = 1; s < _scopes.size(); ++s) {
		auto &scope = _scopes[s];
		if (scope.opener >= _nodes.size()) {
			continue;
		}
		GraphShape::Region region;
		region.opener = _nodes[scope.opener].id;
		region.kind = scope.kind;
		if (scope.barrier < _nodes.size()) {
			region.barrier = _nodes[scope.barrier].id;
		}
		for (auto &node : _nodes) {
			if (node.scope != InvalidIndex && isScopeWithin(node.scope, s)) {
				region.nodes.emplace_back(node.id);
			}
		}
		shape.regions.emplace_back(sprt::move(region));
	}
}

void RuntimeGraph::describeSceneContract(mem_std::Value &out) const {
	describeSceneContractOf(*this, out);
}

void RuntimeGraph::describeExtensionContract(mem_std::Value &out) const {
	describeExtensionContractOf(*this, out);
}

void RuntimeGraph::describe(mem_std::Value &out) const { describeGraph(*this, out); }

} // namespace stappler::flow
