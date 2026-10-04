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

// A graph loaded from a generated unit: resolve, check, decode, bind, answer.

#include "SPFlowCompiled.h"

#include "SPData.h"

// NodeStateTypeName is SPFlowLocal.hpp's, which this subunit follows in the compile unit: the
// store registers the type and the loader checks it, and they must spell one name.
namespace STAPPLER_VERSIONIZED stappler::flow {

CompiledGraph::~CompiledGraph() { reset(); }

void CompiledGraph::reset() {
	delete _gpuShaders;
	_gpuShaders = nullptr;
	_loaded = false;
	_tables = CompiledTables();
	_nodes.clear();
	_constants.clear();
	_settings.clear();
	_sceneBindings.clear();
	_extBindings.clear();
	_namedBindings.clear();
	_namedBound = false;
	_extDeclStorage.clear();
	_extDecls.clear();
	_sceneRegistry = nullptr;
	_sceneRegistryCount = 0;
	_extensions = nullptr;
	_extensionsEpoch = 0;
}

Status CompiledGraph::load(const CompiledTables &tables, const OpRegistry &ops, DiagSink *report) {
	return loadImpl(tables, ops, nullptr, nullptr, report);
}

Status CompiledGraph::load(const CompiledTables &tables, const OpRegistry &ops,
		const value::TypeRegistry &scene, DiagSink *report) {
	return loadImpl(tables, ops, &scene, nullptr, report);
}

Status CompiledGraph::load(const CompiledTables &tables, const OpRegistry &ops,
		const value::TypeRegistry &scene, const value::ExtensionHost &extensions,
		DiagSink *report) {
	return loadImpl(tables, ops, &scene, &extensions, report);
}

// The operation, or the scope, a check is about.
static void reportOp(DiagReport &report, DiagCode code, const DiagText &text, StringView op) {
	StringView names[] = {op};
	report.reportAt(DiagSeverity::Error, code, text, DiagLocus::Op, SpanView<int64_t>(),
			SpanView<StringView>(names, 1));
}

Status checkUnitCallees(const CompiledIdentity &identity, const GraphLink *link, DiagSink *sink) {
	DiagReport report(sink);
	auto linked = link ? link->getCallees() : SpanView<LinkCallee>();
	for (auto &it : identity.callees) {
		const LinkCallee *found = nullptr;
		for (auto &c : linked) {
			if (c.name == it.name) {
				found = &c;
			}
		}
		if (!found || found->contentHash != it.contentHash) {
			reportOp(report, DiagCode::CodegenCalleeDrift,
					DiagText(DiagDetail::UnitCalleeDrift).name(it.name), it.name);
		}
	}
	for (auto &c : linked) {
		bool found = false;
		for (auto &it : identity.callees) {
			if (c.name == it.name) {
				found = true;
			}
		}
		if (!found) {
			reportOp(report, DiagCode::CodegenCalleeDrift,
					DiagText(DiagDetail::UnitCalleeDrift).name(c.name), c.name);
		}
	}
	return report.getStatus();
}

uint64_t hashBlockQuery(SpanView<TypeId> ids) {
	return sprt::hash64(reinterpret_cast<const char *>(ids.data()), ids.size() * sizeof(TypeId));
}

static void reportScope(DiagReport &report, DiagCode code, const DiagText &text, uint32_t scope) {
	int64_t values[] = {int64_t(scope)};
	report.reportAt(DiagSeverity::Error, code, text, DiagLocus::Scope,
			SpanView<int64_t>(values, 1));
}

bool CompiledGraph::checkShape(DiagReport &report) const {
	auto &t = _tables;
	auto nodeCount = uint32_t(t.nodes.size());
	auto scopeCount = uint32_t(t.scopes.size());
	bool ok = true;

	auto refuse = [&](const DiagText &what) {
		report.report(DiagSeverity::Error, DiagCode::CodegenMalformed, what);
		ok = false;
	};

	if (t.opNames.size() != nodeCount) {
		refuse(DiagText(DiagDetail::UnitOpCount));
	}
	if (scopeCount == 0) {
		refuse(DiagText(DiagDetail::UnitNoRootScope));
	}
	if (t.identity.slots.size() != nodeCount || t.identity.frameBytes.size() != scopeCount) {
		refuse(DiagText(DiagDetail::UnitLayoutCover));
	}

	auto slice = [&](uint32_t begin, uint32_t count, size_t size, DiagPhrase what) {
		if (uint64_t(begin) + count > size) {
			refuse(DiagText(DiagDetail::UnitSlicePastEnd).phrase(what));
		}
	};
	auto indexInto = [&](SpanView<uint32_t> indices, size_t bound, DiagPhrase what) {
		for (auto i : indices) {
			if (i >= bound) {
				refuse(DiagText(DiagDetail::UnitIndexPastEnd).phrase(what));
				return;
			}
		}
	};

	for (auto &n : t.nodes) {
		slice(n.dataInBegin, n.dataInCount, t.dataIn.size(), DiagPhrase::TableDataIn);
		slice(n.dataOutBegin, n.dataOutCount, t.dataOut.size(), DiagPhrase::TableDataOut);
		slice(n.execInBegin, n.execInCount, t.execIn.size(), DiagPhrase::TableExecIn);
		slice(n.execOutBegin, n.execOutCount, t.execOut.size(), DiagPhrase::TableExecOut);
		slice(n.crossScopeInBegin, n.crossScopeInCount, t.crossScopeIn.size(),
				DiagPhrase::TableCrossScope);
		slice(n.sceneBegin, n.sceneCount, t.sceneContract.size(), DiagPhrase::TableSceneContract);
		slice(n.extBegin, n.extCount, t.extensionContract.size(),
				DiagPhrase::TableExtensionContract);
		slice(n.namedBegin, n.namedCount, t.namedContract.size(), DiagPhrase::TableNamedContract);
		if (n.scope >= scopeCount || (n.opensScope != InvalidIndex && n.opensScope >= scopeCount)) {
			refuse(DiagText(DiagDetail::UnitScopeUnknown));
		}
	}
	for (auto &s : t.scopes) {
		slice(s.nodeBegin, s.nodeCount, t.scopeNodes.size(), DiagPhrase::TableScope);
		if (s.opener != InvalidIndex && s.opener >= nodeCount) {
			refuse(DiagText(DiagDetail::UnitOpenerUnknown));
		}
		if (s.parent != InvalidIndex && s.parent >= scopeCount) {
			refuse(DiagText(DiagDetail::UnitParentUnknown));
		}
		if (s.barrier != InvalidIndex && s.barrier >= nodeCount) {
			refuse(DiagText(DiagDetail::UnitOpenerUnknown));
		}
		if ((s.branchScope != InvalidIndex && s.branchScope >= scopeCount)
				|| (s.block != InvalidIndex && s.block >= t.blocks.size())) {
			refuse(DiagText(DiagDetail::UnitScopeUnknown));
		}
	}
	for (auto &b : t.blocks) {
		if (b.scope >= scopeCount || b.fanOut >= nodeCount || b.barrier >= nodeCount) {
			refuse(DiagText(DiagDetail::UnitScopeUnknown));
		}
		slice(b.writeBegin, b.writeCount, t.blockWrites.size(), DiagPhrase::TableBlockWrites);
		slice(b.collectorBegin, b.collectorCount, t.blockCollectors.size(),
				DiagPhrase::TableBlockCollectors);
		slice(b.withBegin, b.withCount, t.blockQuery.size(), DiagPhrase::TableBlockQuery);
		slice(b.withoutBegin, b.withoutCount, t.blockQuery.size(), DiagPhrase::TableBlockQuery);
	}
	indexInto(t.blockCollectors, nodeCount, DiagPhrase::TableBlockCollector);
	for (auto &e : t.dataEdges) {
		if (e.srcNode >= nodeCount || e.dstNode >= nodeCount) {
			refuse(DiagText(DiagDetail::UnitDataEdgeNode));
			break;
		}
	}
	for (auto &e : t.execEdges) {
		if (e.srcNode >= nodeCount || e.dstNode >= nodeCount) {
			refuse(DiagText(DiagDetail::UnitExecEdgeNode));
			break;
		}
	}
	indexInto(t.dataIn, t.dataEdges.size(), DiagPhrase::TableDataIn);
	indexInto(t.dataOut, t.dataEdges.size(), DiagPhrase::TableDataOut);
	indexInto(t.crossScopeIn, t.dataEdges.size(), DiagPhrase::TableCrossScope);
	indexInto(t.execIn, t.execEdges.size(), DiagPhrase::TableExecIn);
	indexInto(t.execOut, t.execEdges.size(), DiagPhrase::TableExecOut);
	indexInto(t.entries, nodeCount, DiagPhrase::TableEntry);
	indexInto(t.terminals, nodeCount, DiagPhrase::TableTerminal);
	indexInto(t.scopeNodes, nodeCount, DiagPhrase::TableScopeNode);
	return ok;
}

void CompiledGraph::verify(const OpRegistry &ops, DiagReport &report) {
	auto &identity = _tables.identity;

	// The operations, by name: present, of the signature the unit was written against, deriving
	// the record schema it laid its frames out with.
	for (auto &it : identity.ops) {
		auto op = ops.get(it.name);
		if (!op) {
			reportOp(report, DiagCode::CodegenOpDrift,
					DiagText(DiagDetail::UnitOpDrift).name(it.name), it.name);
			continue;
		}
		if (op->getSignatureHash() != it.signatureHash) {
			reportOp(report, DiagCode::CodegenOpDrift,
					DiagText(DiagDetail::OpSignatureDrift).name(it.name), it.name);
			continue;
		}
		if (op->getParallel() != it.parallel) {
			reportOp(report, DiagCode::CodegenOpDrift,
					DiagText(DiagDetail::OpParallelDrift)
							.name(it.name)
							.name(getOpParallelName(it.parallel))
							.name(getOpParallelName(op->getParallel())),
					it.name);
			continue;
		}
		auto schema = op->getLocalSchema();
		auto hash = schema ? schema->getSchemaHash() : uint64_t(0);
		if (hash != it.localSchemaHash) {
			reportOp(report, DiagCode::CodegenSchemaDrift,
					DiagText(DiagDetail::OpSchemaDrift).name(it.name), it.name);
		}
	}

	// The store's own bookkeeping type, which every frame begins with.
	auto stateType = ops.getCoreTypes().get(NodeStateTypeName);
	if (!stateType) {
		reportOp(report, DiagCode::CodegenSchemaDrift, DiagText(DiagDetail::NoNodeStateType),
				NodeStateTypeName);
		return;
	}
	if (stateType->getSchemaHash() != identity.stateSchemaHash) {
		reportOp(report, DiagCode::CodegenSchemaDrift, DiagText(DiagDetail::NodeStateTypeDrift),
				NodeStateTypeName);
		return;
	}

	// The layout, recomputed by the store's own rule over the resolved nodes, and compared row for
	// row: a unit whose offsets are wrong would address every record of a run one field over.
	if (report.hasErrors()) {
		return; // an unresolved operation has no schema to lay out
	}
	mem_std::Vector<FrameSlot> slots;
	mem_std::Vector<uint32_t> frameBytes;
	if (computeFrameLayout(*this, *stateType, slots, frameBytes) != Status::Ok) {
		report.report(DiagSeverity::Error, DiagCode::CodegenLayoutDrift,
				DiagText(DiagDetail::LayoutFailed));
		return;
	}
	for (uint32_t s = 0; s < uint32_t(frameBytes.size()); ++s) {
		if (frameBytes[s] != identity.frameBytes[s]) {
			reportScope(report, DiagCode::CodegenLayoutDrift,
					DiagText(DiagDetail::ScopeFrameBytesDrift)
							.number(int64_t(s))
							.number(int64_t(frameBytes[s]))
							.number(int64_t(identity.frameBytes[s])),
					s);
		}
	}
	for (uint32_t n = 0; n < uint32_t(slots.size()); ++n) {
		auto &here = slots[n];
		auto &there = identity.slots[n];
		if (here.stateOffset != there.stateOffset || here.recordOffset != there.recordOffset) {
			report.reportNode(DiagSeverity::Error, DiagCode::CodegenLayoutDrift, _nodes[n].id,
					DiagText(DiagDetail::LayoutDrift));
		}
	}
}

void CompiledGraph::verifyBlocks(DiagReport &report) {
	auto &identity = _tables.identity;
	if (identity.blocks.size() != _tables.blocks.size()) {
		report.report(DiagSeverity::Error, DiagCode::CodegenMalformed,
				DiagText(DiagDetail::UnitBlockCount)
						.number(int64_t(identity.blocks.size()))
						.number(int64_t(_tables.blocks.size())));
		return;
	}
	for (uint32_t b = 0; b < uint32_t(_tables.blocks.size()); ++b) {
		auto &row = _tables.blocks[b];
		auto &said = identity.blocks[b];
		auto fanOut = _nodes[row.fanOut].id;
		if (said.fanOut != fanOut || said.onFailure != row.onFailure || said.timeoutMs != row.timeoutMs
				|| said.maxSteps != row.maxSteps || said.maxActivations != row.maxActivations
				|| said.withHash != hashBlockQuery(getBlockQuery(b, false))
				|| said.withoutHash != hashBlockQuery(getBlockQuery(b, true))) {
			report.reportNode(DiagSeverity::Error, DiagCode::CodegenMalformed, fanOut,
					DiagText(DiagDetail::UnitBlockPolicy));
		}
	}
}

void CompiledGraph::verifyFamilies(const value::TypeRegistry &registry, DiagReport &report) {
	for (auto &it : _tables.identity.families) {
		// A family the registry does not have is the binder's to report (enum-family-unknown).
		auto type = registry.getEnum(it.name);
		if (type && type->getHash() != it.hash) {
			report.reportNode(DiagSeverity::Error, DiagCode::CodegenSchemaDrift, it.node,
					DiagText(DiagDetail::UnitFamilyDrift).name(it.name));
		}
	}
}

Status CompiledGraph::loadImpl(const CompiledTables &tables, const OpRegistry &ops,
		const value::TypeRegistry *scene, const value::ExtensionHost *extensions, DiagSink *sink) {
	reset();
	DiagReport report(sink);
	_tables = tables;

	if (!checkShape(report)) {
		reset();
		return report.getStatus();
	}

	// The nodes, with their operations resolved by name. A missing operation is the unit's
	// `CodegenOpDrift` rather than the build's `UnknownOp`: the build was asking whether a file
	// names something that exists, and this asks whether a program still has what it was compiled
	// against. The identity check below says it by name; here the node is simply left unresolved so
	// that nothing that follows dereferences it.
	_nodes.assign(tables.nodes.begin(), tables.nodes.end());
	uint32_t constantCount = 0;
	uint32_t settingCount = 0;
	for (uint32_t i = 0; i < uint32_t(_nodes.size()); ++i) {
		auto &node = _nodes[i];
		node.source = nullptr;
		node.op = ops.get(tables.opNames[i]);
		node.localSchema = node.op ? node.op->getLocalSchema() : nullptr;
		if (node.op) {
			if (node.constantBegin != constantCount) {
				report.reportNode(DiagSeverity::Error, DiagCode::CodegenMalformed, node.id,
						DiagText(DiagDetail::ConstDrift));
			}
			constantCount += uint32_t(node.op->getDataIn().size());
			if (!node.op->getSettings().empty()) {
				if (node.settingBegin != settingCount) {
					report.reportNode(DiagSeverity::Error, DiagCode::CodegenMalformed, node.id,
							DiagText(DiagDetail::ConstDrift));
				}
				settingCount += uint32_t(node.op->getSettings().size());
			}
		}
	}

	verify(ops, report);
	if (!report.hasErrors()) {
		verifyBlocks(report);
	}
	if (report.hasErrors()) {
		reset();
		return report.getStatus();
	}

	// The constants, decoded into the values the build held.
	{
		BytesView bytes(tables.constants);
		auto decoded = bytes.empty() ? mem_std::Value(mem_std::Value::Type::ARRAY)
									 : data::read<mem_std::Interface>(bytes);
		if (!decoded.isArray() || decoded.size() != constantCount) {
			report.report(DiagSeverity::Error, DiagCode::CodegenMalformed,
					DiagText(DiagDetail::ConstCount)
							.number(int64_t(decoded.isArray() ? decoded.size() : 0))
							.number(int64_t(constantCount)));
			reset();
			return report.getStatus();
		}
		_constants.reserve(constantCount);
		for (auto &it : decoded.asArray()) { _constants.emplace_back(it); }
	}
	{
		BytesView bytes(tables.settings);
		auto decoded = bytes.empty() ? mem_std::Value(mem_std::Value::Type::ARRAY)
									 : data::read<mem_std::Interface>(bytes);
		if (!decoded.isArray() || decoded.size() != settingCount) {
			report.report(DiagSeverity::Error, DiagCode::CodegenMalformed,
					DiagText(DiagDetail::ConstCount)
							.number(int64_t(decoded.isArray() ? decoded.size() : 0))
							.number(int64_t(settingCount)));
			reset();
			return report.getStatus();
		}
		_settings.reserve(settingCount);
		for (auto &it : decoded.asArray()) { _settings.emplace_back(it); }
	}

	// Both contracts, unbound; the declarations, decoded.
	_sceneBindings.assign(tables.sceneContract.begin(), tables.sceneContract.end());
	for (auto &b : _sceneBindings) {
		b.type = nullptr;
		b.desc = nullptr;
	}
	_extBindings.assign(tables.extensionContract.begin(), tables.extensionContract.end());
	for (auto &b : _extBindings) { b.instance = nullptr; }
	_namedBindings.assign(tables.namedContract.begin(), tables.namedContract.end());
	for (auto &b : _namedBindings) { b.entity = value::EntityId(); }
	_extDeclStorage.reserve(tables.extensionDecls.size());
	for (auto &it : tables.extensionDecls) {
		ExtensionDecl decl;
		decl.name = it.name;
		decl.id = it.id;
		if (!it.params.empty()) {
			BytesView bytes(it.params);
			decl.params = data::read<mem_std::Interface>(bytes);
		}
		_extDeclStorage.emplace_back(sprt::move(decl));
	}
	_extDecls.reserve(_extDeclStorage.size());
	for (auto &it : _extDeclStorage) { _extDecls.emplace_back(&it); }

	// A GPU block's shader, re-lowered from the tables now that they are whole.
	for (uint32_t b = 0; b < uint32_t(tables.identity.blocks.size()); ++b) {
		auto &said = tables.identity.blocks[b];
		if (said.gpuHash != 0 && hashGpuBlock(*this, b) != said.gpuHash) {
			report.reportNode(DiagSeverity::Error, DiagCode::CodegenLayoutDrift, said.fanOut,
					DiagText(DiagDetail::UnitGpuDrift));
		}
	}
	if (report.hasErrors()) {
		reset();
		return report.getStatus();
	}

	_loaded = true;

	// And the scene's side, when the caller brought one. After `_loaded`, because the binders are
	// the shared ones and they read the graph through its accessors; a bind that fails leaves the
	// graph loaded and unbound, exactly as bindScene does on a built graph. The load itself does
	// not fail for it: the unit is the right graph, the scene is the one that does not fit, and the
	// caller reads that off the report.
	if (scene && extensions) {
		bindScene(*scene, *extensions, sink);
	} else if (scene) {
		bindScene(*scene, sink);
	}
	return Status::Ok;
}

void CompiledGraph::unbindAll() {
	for (auto &binding : _sceneBindings) {
		binding.type = nullptr;
		binding.desc = nullptr;
	}
	for (auto &binding : _extBindings) { binding.instance = nullptr; }
	_sceneRegistry = nullptr;
	_sceneRegistryCount = 0;
	_extensions = nullptr;
	_extensionsEpoch = 0;
}

Status CompiledGraph::bindScene(const value::TypeRegistry &registry, DiagSink *sink) {
	if (!_loaded) {
		return Status::ErrorInvalidArguemnt;
	}
	DiagReport report(sink);
	bindSceneBindings(_nodes, _sceneBindings, registry, report);
	verifyFamilies(registry, report);
	if (report.hasErrors()) {
		unbindAll();
		return report.getStatus();
	}
	if (anyResolvable(_sceneBindings)) {
		_sceneRegistry = &registry;
		_sceneRegistryCount = registry.getCount();
	}
	return Status::Ok;
}

Status CompiledGraph::bindScene(const value::TypeRegistry &registry,
		const value::ExtensionHost &extensions, DiagSink *sink) {
	if (!_loaded) {
		return Status::ErrorInvalidArguemnt;
	}
	DiagReport report(sink);
	bindSceneBindings(_nodes, _sceneBindings, registry, report);
	bindExtensionBindings(_nodes, _extBindings, _extDecls, extensions, report);
	verifyFamilies(registry, report);
	// Both halves or neither - the rule RuntimeGraph::bindScene states, over the same two tables.
	if (report.hasErrors()) {
		unbindAll();
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

uint32_t CompiledGraph::findNode(NodeId id) const {
	// Ascending ids, as the build laid them out: a bisection, like RuntimeGraph's.
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

SpanView<uint32_t> CompiledGraph::getCrossScopeInEdges(uint32_t node) const {
	auto &n = _nodes[node];
	return SpanView<uint32_t>(_tables.crossScopeIn.data() + n.crossScopeInBegin,
			n.crossScopeInCount);
}

SpanView<uint32_t> CompiledGraph::getDataInEdges(uint32_t node) const {
	auto &n = _nodes[node];
	return SpanView<uint32_t>(_tables.dataIn.data() + n.dataInBegin, n.dataInCount);
}

SpanView<uint32_t> CompiledGraph::getDataOutEdges(uint32_t node) const {
	auto &n = _nodes[node];
	return SpanView<uint32_t>(_tables.dataOut.data() + n.dataOutBegin, n.dataOutCount);
}

SpanView<uint32_t> CompiledGraph::getExecInEdges(uint32_t node) const {
	auto &n = _nodes[node];
	return SpanView<uint32_t>(_tables.execIn.data() + n.execInBegin, n.execInCount);
}

SpanView<uint32_t> CompiledGraph::getExecOutEdges(uint32_t node) const {
	auto &n = _nodes[node];
	return SpanView<uint32_t>(_tables.execOut.data() + n.execOutBegin, n.execOutCount);
}

SpanView<uint32_t> CompiledGraph::getScopeNodes(uint32_t scope) const {
	if (scope >= _tables.scopes.size()) {
		return SpanView<uint32_t>();
	}
	auto &s = _tables.scopes[scope];
	return SpanView<uint32_t>(_tables.scopeNodes.data() + s.nodeBegin, s.nodeCount);
}

bool CompiledGraph::isScopeWithin(uint32_t scope, uint32_t ancestor) const {
	while (scope != InvalidIndex) {
		if (scope == ancestor) {
			return true;
		}
		if (scope >= _tables.scopes.size()) {
			return false;
		}
		scope = _tables.scopes[scope].parent;
	}
	return false;
}

const mem_std::Value *CompiledGraph::getSetting(uint32_t node, uint32_t index) const {
	auto &n = _nodes[node];
	if (!n.op || index >= n.op->getSettings().size() || n.settingBegin + index >= _settings.size()) {
		return nullptr;
	}
	auto &value = _settings[n.settingBegin + index];
	return value.isNull() ? nullptr : &value;
}

SpanView<RuntimeBlockWrite> CompiledGraph::getBlockWrites(uint32_t block) const {
	auto &b = _tables.blocks[block];
	return SpanView<RuntimeBlockWrite>(_tables.blockWrites.data() + b.writeBegin, b.writeCount);
}

SpanView<uint32_t> CompiledGraph::getBlockCollectors(uint32_t block) const {
	auto &b = _tables.blocks[block];
	return SpanView<uint32_t>(_tables.blockCollectors.data() + b.collectorBegin, b.collectorCount);
}

SpanView<TypeId> CompiledGraph::getBlockQuery(uint32_t block, bool without) const {
	auto &b = _tables.blocks[block];
	return without ? SpanView<TypeId>(_tables.blockQuery.data() + b.withoutBegin, b.withoutCount)
				   : SpanView<TypeId>(_tables.blockQuery.data() + b.withBegin, b.withCount);
}

const mem_std::Value *CompiledGraph::getConstant(uint32_t node, uint32_t pin) const {
	auto &n = _nodes[node];
	if (!n.op || pin >= n.op->getDataIn().size()) {
		return nullptr;
	}
	auto &value = _constants[n.constantBegin + pin];
	return value.isNull() ? nullptr : &value;
}

SpanView<SceneBinding> CompiledGraph::getSceneBindings(uint32_t node) const {
	auto &n = _nodes[node];
	if (!_sceneRegistry || n.sceneCount == 0) {
		return SpanView<SceneBinding>();
	}
	return SpanView<SceneBinding>(_sceneBindings.data() + n.sceneBegin, n.sceneCount);
}

SpanView<SceneBinding> CompiledGraph::getSceneContract(uint32_t node) const {
	auto &n = _nodes[node];
	if (n.sceneCount == 0) {
		return SpanView<SceneBinding>();
	}
	return SpanView<SceneBinding>(_sceneBindings.data() + n.sceneBegin, n.sceneCount);
}

SpanView<ExtensionBinding> CompiledGraph::getExtensionBindings(uint32_t node) const {
	auto &n = _nodes[node];
	if (!_extensions || n.extCount == 0) {
		return SpanView<ExtensionBinding>();
	}
	return SpanView<ExtensionBinding>(_extBindings.data() + n.extBegin, n.extCount);
}

SpanView<ExtensionBinding> CompiledGraph::getExtensionContract(uint32_t node) const {
	auto &n = _nodes[node];
	if (n.extCount == 0) {
		return SpanView<ExtensionBinding>();
	}
	return SpanView<ExtensionBinding>(_extBindings.data() + n.extBegin, n.extCount);
}

SpanView<NamedBinding> CompiledGraph::getNamedContract(uint32_t node) const {
	auto &n = _nodes[node];
	if (n.namedCount == 0) {
		return SpanView<NamedBinding>();
	}
	return SpanView<NamedBinding>(_namedBindings.data() + n.namedBegin, n.namedCount);
}

SpanView<NamedBinding> CompiledGraph::getNamedBindings(uint32_t node) const {
	return _namedBound ? getNamedContract(node) : SpanView<NamedBinding>();
}

Status CompiledGraph::bindNamed(const value::NamedHost &table, DiagSink *sink) {
	if (!_loaded) {
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

void CompiledGraph::describe(mem_std::Value &out) const { describeGraph(*this, out); }

void CompiledGraph::describeSceneContract(mem_std::Value &out) const {
	describeSceneContractOf(*this, out);
}

void CompiledGraph::describeExtensionContract(mem_std::Value &out) const {
	describeExtensionContractOf(*this, out);
}

} // namespace stappler::flow
