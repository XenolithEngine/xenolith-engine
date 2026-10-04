// Generated from the graph "pure". Do not edit: regenerate it.
//
// A graph as constants: the shape the build settled, the constant table, both
// contracts unbound, and the identity of what it was written against. Loaded through
// flow::StaticGraph<Tables>, which resolves the operations, checks the identity and binds the scene;
// run by Run<A>, the interpreter's own machine over the loaded rows, or held by a host as Engine<A>.
//
// The rows are half of it. The other half is in <name>.gen.cpp: one function per node, with the
// node index a compile-time constant, which is what the machine performs a node through.
#ifndef GEN_CORPUS_CALL_PURE_GEN_H_
#define GEN_CORPUS_CALL_PURE_GEN_H_

#include "SPFlowCompiled.h"
#include "SPFlowEnv.h"

namespace STAPPLER_VERSIONIZED stappler::flow::gen::corpus_call_pure {

// This unit's own way of performing a node: `step<N>` with the index a constant, the door over
// the tables below and the operation called by name where it has one. Written in
// <name>.gen.cpp, beside the bodies it calls, once per store a run
// may live in - the arena one and the fast one.
const flow::CompiledStepsT<flow::NoEnv> *steps();

// The code is written across 2 sources, each performing a range of nodes: the graph is
// large enough that one translation unit would be compiled alone while the other cores waited.
// The first source dispatches by range and holds the table above; these are the entry points it
// calls, instantiated where they are defined.
template <typename Local>
Status stepPart0(flow::CompiledStepSiteT<Local> &site);
template <typename Local>
Status stepPart1(flow::CompiledStepSiteT<Local> &site);

struct Tables {
	static constexpr uint32_t NodeCount = 5;
	static constexpr uint32_t ScopeCount = 2;

	static constexpr uint32_t opNamesCount = 5;
	static constexpr StringView opNames[5] = {
		StringView("fn.pure:twice"),
		StringView("math.addInt"),
		StringView("fn.pure:twice#entry"),
		StringView("math.addInt"),
		StringView("fn.pure:twice#return:0"),
	};

	static constexpr uint32_t nodesCount = 5;
	static constexpr flow::RuntimeNode nodes[5] = {
		{.id = 1, .dataInBegin = 0, .dataInCount = 0, .dataOutBegin = 0, .dataOutCount = 1, .execInBegin = 0, .execInCount = 0, .execOutBegin = 0, .execOutCount = 0, .constantBegin = 0, .sceneBegin = 0, .sceneCount = 0, .extBegin = 0, .extCount = 0, .scope = 0, .opensScope = 1, .slotInScope = 0, .sameScopeInputs = 0u, .allInputs = 0u, .crossScopeInBegin = 0, .crossScopeInCount = 0, .isEntry = true, .isTerminal = false, .settingBegin = 0},
		{.id = 2, .dataInBegin = 0, .dataInCount = 1, .dataOutBegin = 1, .dataOutCount = 0, .execInBegin = 0, .execInCount = 0, .execOutBegin = 0, .execOutCount = 0, .constantBegin = 1, .sceneBegin = 0, .sceneCount = 0, .extBegin = 0, .extCount = 0, .scope = 0, .opensScope = flow::InvalidIndex, .slotInScope = 1, .sameScopeInputs = 1u, .allInputs = 1u, .crossScopeInBegin = 0, .crossScopeInCount = 0, .isEntry = false, .isTerminal = true},
		{.id = 10, .dataInBegin = 1, .dataInCount = 0, .dataOutBegin = 1, .dataOutCount = 2, .execInBegin = 0, .execInCount = 0, .execOutBegin = 0, .execOutCount = 0, .constantBegin = 3, .sceneBegin = 0, .sceneCount = 0, .extBegin = 0, .extCount = 0, .scope = 1, .opensScope = flow::InvalidIndex, .slotInScope = 0, .sameScopeInputs = 0u, .allInputs = 0u, .crossScopeInBegin = 0, .crossScopeInCount = 0, .isEntry = false, .isTerminal = false},
		{.id = 11, .dataInBegin = 1, .dataInCount = 2, .dataOutBegin = 3, .dataOutCount = 1, .execInBegin = 0, .execInCount = 0, .execOutBegin = 0, .execOutCount = 0, .constantBegin = 4, .sceneBegin = 0, .sceneCount = 0, .extBegin = 0, .extCount = 0, .scope = 1, .opensScope = flow::InvalidIndex, .slotInScope = 1, .sameScopeInputs = 3u, .allInputs = 3u, .crossScopeInBegin = 0, .crossScopeInCount = 0, .isEntry = false, .isTerminal = false},
		{.id = 12, .dataInBegin = 3, .dataInCount = 1, .dataOutBegin = 4, .dataOutCount = 0, .execInBegin = 0, .execInCount = 0, .execOutBegin = 0, .execOutCount = 0, .constantBegin = 6, .sceneBegin = 0, .sceneCount = 0, .extBegin = 0, .extCount = 0, .scope = 1, .opensScope = flow::InvalidIndex, .slotInScope = 2, .sameScopeInputs = 1u, .allInputs = 1u, .crossScopeInBegin = 0, .crossScopeInCount = 0, .isEntry = false, .isTerminal = true},
	};

	static constexpr uint32_t dataEdgesCount = 4;
	static constexpr flow::RuntimeDataEdge dataEdges[4] = {
		{.srcNode = 0, .srcPin = 0, .dstNode = 1, .dstPin = 0, .cast = flow::value::CastRule(1) /* same */, .needsArena = false},
		{.srcNode = 2, .srcPin = 0, .dstNode = 3, .dstPin = 0, .cast = flow::value::CastRule(1) /* same */, .needsArena = false},
		{.srcNode = 2, .srcPin = 0, .dstNode = 3, .dstPin = 1, .cast = flow::value::CastRule(1) /* same */, .needsArena = false},
		{.srcNode = 3, .srcPin = 0, .dstNode = 4, .dstPin = 0, .cast = flow::value::CastRule(1) /* same */, .needsArena = false},
	};
	static constexpr uint32_t execEdgesCount = 0;
	static constexpr flow::RuntimeExecEdge execEdges[1] = {
		{},
	};

	static constexpr uint32_t dataInCount = 4;
	static constexpr uint32_t dataIn[4] = {
		0,
		1,
		2,
		3,
	};
	static constexpr uint32_t dataOutCount = 4;
	static constexpr uint32_t dataOut[4] = {
		0,
		1,
		2,
		3,
	};
	static constexpr uint32_t execInCount = 0;
	static constexpr uint32_t execIn[1] = {
		{},
	};
	static constexpr uint32_t execOutCount = 0;
	static constexpr uint32_t execOut[1] = {
		{},
	};
	static constexpr uint32_t crossScopeInCount = 0;
	static constexpr uint32_t crossScopeIn[1] = {
		{},
	};
	static constexpr uint32_t entriesCount = 1;
	static constexpr uint32_t entries[1] = {
		0,
	};
	static constexpr uint32_t terminalsCount = 2;
	static constexpr uint32_t terminals[2] = {
		1,
		4,
	};

	static constexpr uint32_t scopesCount = 2;
	static constexpr flow::RuntimeScope scopes[2] = {
		{.opener = flow::InvalidIndex, .execPin = flow::InvalidIndex, .parent = flow::InvalidIndex, .depth = 0, .nodeBegin = 0, .nodeCount = 2},
		{.opener = flow::InvalidIndex, .execPin = flow::InvalidIndex, .parent = flow::InvalidIndex, .depth = 0, .nodeBegin = 2, .nodeCount = 3, .kind = flow::ScopeKind(2) /* function */, .barrier = flow::InvalidIndex, .branchScope = flow::InvalidIndex, .block = flow::InvalidIndex, .headerBytes = 0},
	};
	static constexpr uint32_t scopeNodesCount = 5;
	static constexpr uint32_t scopeNodes[5] = {
		0,
		1,
		2,
		3,
		4,
	};

	// The settings table, one CBOR array: an entry per setting every node's operation declares.
	static constexpr uint32_t settingsSize = 12;
	static constexpr uint8_t settings[12] = {
		0xd9, 0xd9, 0xf7, 0x81, 0x67, 0x64, 0x65, 0x66, 0x61, 0x75, 0x6c, 0x74,
	};

	// The constant table, one CBOR array: an entry per input of every node, null where the build
	// stored none.
	static constexpr uint32_t constantsSize = 11;
	static constexpr uint8_t constants[11] = {
		0xd9, 0xd9, 0xf7, 0x87, 0x15, 0xf6, 0x00, 0xf6, 0xf6, 0xf6, 0xf6,
	};

	// The asset this unit was written from, as canonical CBOR - empty unless --embed-asset. The
	// machine never reads it; it is here for tools that want a RuntimeGraph beside the tables.
	static constexpr uint32_t assetSize = 0;
	static constexpr uint8_t asset[1] = {
		0,
	};

	static constexpr uint32_t sceneContractCount = 0;
	static constexpr flow::SceneBinding sceneContract[1] = {
		{},
	};
	static constexpr uint32_t extensionContractCount = 0;
	static constexpr flow::ExtensionBinding extensionContract[1] = {
		{},
	};
	static constexpr uint32_t extensionDeclsCount = 0;
	static constexpr flow::CompiledExtensionDecl extensionDecls[1] = {
		{},
	};

	// The identity: what this unit was written against, checked on load.
	static constexpr uint32_t opsCount = 4;
	static constexpr flow::CompiledOpIdentity ops[4] = {
		{.name = StringView("fn.pure:twice"), .signatureHash = 0xd6a4637579558127ull, .localSchemaHash = 0x135d2f95a321382bull, .parallel = flow::OpParallel(0) /* serial */},
		{.name = StringView("fn.pure:twice#entry"), .signatureHash = 0x0199449d73739d1full, .localSchemaHash = 0x8240d8dcf8e2f1bcull, .parallel = flow::OpParallel(0) /* serial */},
		{.name = StringView("fn.pure:twice#return:0"), .signatureHash = 0x2c70a2ac6b40cbb1ull, .localSchemaHash = 0x0000000000000000ull, .parallel = flow::OpParallel(0) /* serial */},
		{.name = StringView("math.addInt"), .signatureHash = 0xb5fdb033feaaf02eull, .localSchemaHash = 0x12043720acfa39b9ull, .parallel = flow::OpParallel(1) /* pure */},
	};
	static constexpr uint32_t frameBytesCount = 2;
	static constexpr uint32_t frameBytes[2] = {
		112,
		136,
	};
	static constexpr uint32_t slotsCount = 5;
	static constexpr flow::FrameSlot slots[5] = {
		{.stateOffset = 0, .recordOffset = 40},
		{.stateOffset = 64, .recordOffset = 104},
		{.stateOffset = 0, .recordOffset = 40},
		{.stateOffset = 48, .recordOffset = 88},
		{.stateOffset = 96, .recordOffset = flow::InvalidIndex},
	};

	// The record of each node, field by field: what the door reads and writes.
	static constexpr uint32_t recordFieldsCount = 6;
	static constexpr flow::FieldShape recordFields[6] = {
		{.offset = 0, .size = 8, .type = flow::value::VarType(2) /* int */, .element = 0, .subtypeId = 0x0000000000000000ull, .valid = true},
		{.offset = 8, .size = 8, .type = flow::value::VarType(2) /* int */, .element = 0, .subtypeId = 0x0000000000000000ull, .valid = true},
		{.offset = 16, .size = 8, .type = flow::value::VarType(2) /* int */, .element = 0, .subtypeId = 0x0000000000000000ull, .valid = true},
		{.offset = 0, .size = 8, .type = flow::value::VarType(2) /* int */, .element = 0, .subtypeId = 0x0000000000000000ull, .valid = true},
		{.offset = 0, .size = 8, .type = flow::value::VarType(2) /* int */, .element = 0, .subtypeId = 0x0000000000000000ull, .valid = true},
		{.offset = 0, .size = 8, .type = flow::value::VarType(2) /* int */, .element = 0, .subtypeId = 0x0000000000000000ull, .valid = true},
	};
	static constexpr uint32_t recordFieldBeginCount = 5;
	static constexpr uint32_t recordFieldBegin[5] = {
		0,
		3,
		4,
		5,
		6,
	};
	static constexpr uint32_t recordFieldCountCount = 5;
	static constexpr uint32_t recordFieldCount[5] = {
		3,
		1,
		1,
		1,
		0,
	};

	static constexpr flow::CompiledIdentity identity = {
		.name = StringView("pure"),
		.assetHash = 0xa466a17a1eca6c8full,
		.textHash = 0x73ef706d7edaa12dull,
		.stateSchemaHash = 0x9b08f22c113ee2f8ull,
		.ops = SpanView<flow::CompiledOpIdentity>(ops, ops + opsCount),
		.frameBytes = SpanView<uint32_t>(frameBytes, frameBytes + frameBytesCount),
		.slots = SpanView<flow::FrameSlot>(slots, slots + slotsCount),
	};

	static flow::CompiledTables tables() {
		flow::CompiledTables t;
		t.nodes = SpanView<flow::RuntimeNode>(nodes, nodes + nodesCount);
		t.opNames = SpanView<StringView>(opNames, opNames + opNamesCount);
		t.dataEdges = SpanView<flow::RuntimeDataEdge>(dataEdges, dataEdges + dataEdgesCount);
		t.execEdges = SpanView<flow::RuntimeExecEdge>(execEdges, execEdges + execEdgesCount);
		t.dataIn = SpanView<uint32_t>(dataIn, dataIn + dataInCount);
		t.dataOut = SpanView<uint32_t>(dataOut, dataOut + dataOutCount);
		t.execIn = SpanView<uint32_t>(execIn, execIn + execInCount);
		t.execOut = SpanView<uint32_t>(execOut, execOut + execOutCount);
		t.crossScopeIn = SpanView<uint32_t>(crossScopeIn, crossScopeIn + crossScopeInCount);
		t.entries = SpanView<uint32_t>(entries, entries + entriesCount);
		t.terminals = SpanView<uint32_t>(terminals, terminals + terminalsCount);
		t.scopes = SpanView<flow::RuntimeScope>(scopes, scopes + scopesCount);
		t.scopeNodes = SpanView<uint32_t>(scopeNodes, scopeNodes + scopeNodesCount);
		t.settings = BytesView(settings, size_t(settingsSize));
		t.constants = BytesView(constants, size_t(constantsSize));
		t.asset = BytesView(asset, size_t(assetSize));
		t.sceneContract = SpanView<flow::SceneBinding>(sceneContract, sceneContract + sceneContractCount);
		t.extensionContract = SpanView<flow::ExtensionBinding>(extensionContract, extensionContract + extensionContractCount);
		t.extensionDecls = SpanView<flow::CompiledExtensionDecl>(extensionDecls, extensionDecls + extensionDeclsCount);
		t.steps = steps();
		t.stepsEnv = flow::NoEnv::Tag;
		t.identity = identity;
		return t;
	}
};

using Graph = flow::StaticGraph<Tables>;

// The run over this unit, per arena kind: the machine the
// interpreter is, over these rows and the arena store, and the same behind the executor interface.
// The rows are this unit's and so is the code that performs a node - `steps()` above, written
// beside them in <name>.gen.cpp, which is why a header without its source does not link.
template <typename A, typename Trace = flow::TraceLog>
using Run = flow::CompiledRunT<A, flow::NoEnv, Trace>;

template <typename A>
using Engine = flow::CompiledEngineT<A, flow::NoEnv>;

// And the fast mode: the same rows and the same machine over a store that keeps
// the run's bookkeeping in host memory. `QuietEngine` is that with no execution log at all -
// what a shipped game holds, and the only one of the four that promises nothing about order.
template <typename A, typename Trace = flow::TraceLog>
using FastRun = flow::CompiledFastRunT<A, flow::NoEnv, Trace>;

template <typename A>
using FastEngine = flow::CompiledFastEngineT<A, flow::NoEnv>;

template <typename A>
using QuietEngine = flow::CompiledQuietEngineT<A, flow::NoEnv>;

} // namespace stappler::flow::gen::corpus_call_pure

#endif /* GEN_CORPUS_CALL_PURE_GEN_H_ */
