// Generated from the graph "recursive". Do not edit: regenerate it.
//
// A graph as constants: the shape the build settled, the constant table, both
// contracts unbound, and the identity of what it was written against. Loaded through
// flow::StaticGraph<Tables>, which resolves the operations, checks the identity and binds the scene;
// run by Run<A>, the interpreter's own machine over the loaded rows, or held by a host as Engine<A>.
//
// The rows are half of it. The other half is in <name>.gen.cpp: one function per node, with the
// node index a compile-time constant, which is what the machine performs a node through.
#ifndef GEN_CORPUS_CALL_RECURSIVE_GEN_H_
#define GEN_CORPUS_CALL_RECURSIVE_GEN_H_

#include "SPFlowCompiled.h"
#include "SPFlowEnv.h"

namespace STAPPLER_VERSIONIZED stappler::flow::gen::corpus_call_recursive {

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
	static constexpr uint32_t NodeCount = 11;
	static constexpr uint32_t ScopeCount = 2;

	static constexpr uint32_t opNamesCount = 11;
	static constexpr StringView opNames[11] = {
		StringView("flow.event"),
		StringView("fn.recursive:fact"),
		StringView("debug.trace"),
		StringView("fn.recursive:fact#entry"),
		StringView("compare.lessInt"),
		StringView("flow.branch"),
		StringView("fn.recursive:fact#return:0"),
		StringView("math.subInt"),
		StringView("fn.recursive:fact"),
		StringView("math.mulInt"),
		StringView("fn.recursive:fact#return:0"),
	};

	static constexpr uint32_t nodesCount = 11;
	static constexpr flow::RuntimeNode nodes[11] = {
		{.id = 1, .dataInBegin = 0, .dataInCount = 0, .dataOutBegin = 0, .dataOutCount = 0, .execInBegin = 0, .execInCount = 0, .execOutBegin = 0, .execOutCount = 1, .constantBegin = 0, .sceneBegin = 0, .sceneCount = 0, .extBegin = 0, .extCount = 0, .scope = 0, .opensScope = flow::InvalidIndex, .slotInScope = 0, .sameScopeInputs = 0u, .allInputs = 0u, .crossScopeInBegin = 0, .crossScopeInCount = 0, .isEntry = true, .isTerminal = false},
		{.id = 2, .dataInBegin = 0, .dataInCount = 0, .dataOutBegin = 0, .dataOutCount = 0, .execInBegin = 0, .execInCount = 1, .execOutBegin = 1, .execOutCount = 1, .constantBegin = 0, .sceneBegin = 0, .sceneCount = 0, .extBegin = 0, .extCount = 0, .scope = 0, .opensScope = 1, .slotInScope = 1, .sameScopeInputs = 0u, .allInputs = 0u, .crossScopeInBegin = 0, .crossScopeInCount = 0, .isEntry = false, .isTerminal = false, .settingBegin = 0},
		{.id = 3, .dataInBegin = 0, .dataInCount = 0, .dataOutBegin = 0, .dataOutCount = 0, .execInBegin = 1, .execInCount = 1, .execOutBegin = 2, .execOutCount = 0, .constantBegin = 1, .sceneBegin = 0, .sceneCount = 0, .extBegin = 0, .extCount = 0, .scope = 0, .opensScope = flow::InvalidIndex, .slotInScope = 2, .sameScopeInputs = 0u, .allInputs = 0u, .crossScopeInBegin = 0, .crossScopeInCount = 0, .isEntry = false, .isTerminal = true},
		{.id = 10, .dataInBegin = 0, .dataInCount = 0, .dataOutBegin = 0, .dataOutCount = 3, .execInBegin = 2, .execInCount = 0, .execOutBegin = 2, .execOutCount = 1, .constantBegin = 2, .sceneBegin = 0, .sceneCount = 0, .extBegin = 0, .extCount = 0, .scope = 1, .opensScope = flow::InvalidIndex, .slotInScope = 0, .sameScopeInputs = 0u, .allInputs = 0u, .crossScopeInBegin = 0, .crossScopeInCount = 0, .isEntry = false, .isTerminal = false},
		{.id = 11, .dataInBegin = 0, .dataInCount = 1, .dataOutBegin = 3, .dataOutCount = 1, .execInBegin = 2, .execInCount = 0, .execOutBegin = 3, .execOutCount = 0, .constantBegin = 3, .sceneBegin = 0, .sceneCount = 0, .extBegin = 0, .extCount = 0, .scope = 1, .opensScope = flow::InvalidIndex, .slotInScope = 1, .sameScopeInputs = 1u, .allInputs = 1u, .crossScopeInBegin = 0, .crossScopeInCount = 0, .isEntry = false, .isTerminal = false},
		{.id = 12, .dataInBegin = 1, .dataInCount = 1, .dataOutBegin = 4, .dataOutCount = 0, .execInBegin = 2, .execInCount = 1, .execOutBegin = 3, .execOutCount = 2, .constantBegin = 5, .sceneBegin = 0, .sceneCount = 0, .extBegin = 0, .extCount = 0, .scope = 1, .opensScope = flow::InvalidIndex, .slotInScope = 2, .sameScopeInputs = 1u, .allInputs = 1u, .crossScopeInBegin = 0, .crossScopeInCount = 0, .isEntry = false, .isTerminal = false},
		{.id = 13, .dataInBegin = 2, .dataInCount = 0, .dataOutBegin = 4, .dataOutCount = 0, .execInBegin = 3, .execInCount = 1, .execOutBegin = 5, .execOutCount = 0, .constantBegin = 6, .sceneBegin = 0, .sceneCount = 0, .extBegin = 0, .extCount = 0, .scope = 1, .opensScope = flow::InvalidIndex, .slotInScope = 3, .sameScopeInputs = 0u, .allInputs = 0u, .crossScopeInBegin = 0, .crossScopeInCount = 0, .isEntry = false, .isTerminal = true},
		{.id = 14, .dataInBegin = 2, .dataInCount = 1, .dataOutBegin = 4, .dataOutCount = 1, .execInBegin = 4, .execInCount = 0, .execOutBegin = 5, .execOutCount = 0, .constantBegin = 7, .sceneBegin = 0, .sceneCount = 0, .extBegin = 0, .extCount = 0, .scope = 1, .opensScope = flow::InvalidIndex, .slotInScope = 4, .sameScopeInputs = 1u, .allInputs = 1u, .crossScopeInBegin = 0, .crossScopeInCount = 0, .isEntry = false, .isTerminal = false},
		{.id = 15, .dataInBegin = 3, .dataInCount = 1, .dataOutBegin = 5, .dataOutCount = 1, .execInBegin = 4, .execInCount = 1, .execOutBegin = 5, .execOutCount = 1, .constantBegin = 9, .sceneBegin = 0, .sceneCount = 0, .extBegin = 0, .extCount = 0, .scope = 1, .opensScope = 1, .slotInScope = 5, .sameScopeInputs = 1u, .allInputs = 1u, .crossScopeInBegin = 0, .crossScopeInCount = 0, .isEntry = false, .isTerminal = false, .settingBegin = 1},
		{.id = 16, .dataInBegin = 4, .dataInCount = 2, .dataOutBegin = 6, .dataOutCount = 1, .execInBegin = 5, .execInCount = 0, .execOutBegin = 6, .execOutCount = 0, .constantBegin = 10, .sceneBegin = 0, .sceneCount = 0, .extBegin = 0, .extCount = 0, .scope = 1, .opensScope = flow::InvalidIndex, .slotInScope = 6, .sameScopeInputs = 3u, .allInputs = 3u, .crossScopeInBegin = 0, .crossScopeInCount = 0, .isEntry = false, .isTerminal = false},
		{.id = 17, .dataInBegin = 6, .dataInCount = 1, .dataOutBegin = 7, .dataOutCount = 0, .execInBegin = 5, .execInCount = 1, .execOutBegin = 6, .execOutCount = 0, .constantBegin = 12, .sceneBegin = 0, .sceneCount = 0, .extBegin = 0, .extCount = 0, .scope = 1, .opensScope = flow::InvalidIndex, .slotInScope = 7, .sameScopeInputs = 1u, .allInputs = 1u, .crossScopeInBegin = 0, .crossScopeInCount = 0, .isEntry = false, .isTerminal = true},
	};

	static constexpr uint32_t dataEdgesCount = 7;
	static constexpr flow::RuntimeDataEdge dataEdges[7] = {
		{.srcNode = 3, .srcPin = 0, .dstNode = 4, .dstPin = 0, .cast = flow::value::CastRule(1) /* same */, .needsArena = false},
		{.srcNode = 4, .srcPin = 0, .dstNode = 5, .dstPin = 0, .cast = flow::value::CastRule(1) /* same */, .needsArena = false},
		{.srcNode = 3, .srcPin = 0, .dstNode = 7, .dstPin = 0, .cast = flow::value::CastRule(1) /* same */, .needsArena = false},
		{.srcNode = 7, .srcPin = 0, .dstNode = 8, .dstPin = 0, .cast = flow::value::CastRule(1) /* same */, .needsArena = false},
		{.srcNode = 3, .srcPin = 0, .dstNode = 9, .dstPin = 0, .cast = flow::value::CastRule(1) /* same */, .needsArena = false},
		{.srcNode = 8, .srcPin = 0, .dstNode = 9, .dstPin = 1, .cast = flow::value::CastRule(1) /* same */, .needsArena = false},
		{.srcNode = 9, .srcPin = 0, .dstNode = 10, .dstPin = 0, .cast = flow::value::CastRule(1) /* same */, .needsArena = false},
	};
	static constexpr uint32_t execEdgesCount = 6;
	static constexpr flow::RuntimeExecEdge execEdges[6] = {
		{.srcNode = 0, .srcPin = 0, .dstNode = 1, .backEdge = false},
		{.srcNode = 1, .srcPin = 0, .dstNode = 2, .backEdge = false},
		{.srcNode = 3, .srcPin = 0, .dstNode = 5, .backEdge = false},
		{.srcNode = 5, .srcPin = 1, .dstNode = 8, .backEdge = false},
		{.srcNode = 5, .srcPin = 0, .dstNode = 6, .backEdge = false},
		{.srcNode = 8, .srcPin = 0, .dstNode = 10, .backEdge = false},
	};

	static constexpr uint32_t dataInCount = 7;
	static constexpr uint32_t dataIn[7] = {
		0,
		1,
		2,
		3,
		4,
		5,
		6,
	};
	static constexpr uint32_t dataOutCount = 7;
	static constexpr uint32_t dataOut[7] = {
		0,
		2,
		4,
		1,
		3,
		5,
		6,
	};
	static constexpr uint32_t execInCount = 6;
	static constexpr uint32_t execIn[6] = {
		0,
		1,
		2,
		4,
		3,
		5,
	};
	static constexpr uint32_t execOutCount = 6;
	static constexpr uint32_t execOut[6] = {
		0,
		1,
		2,
		3,
		4,
		5,
	};
	static constexpr uint32_t crossScopeInCount = 0;
	static constexpr uint32_t crossScopeIn[1] = {
		{},
	};
	static constexpr uint32_t entriesCount = 1;
	static constexpr uint32_t entries[1] = {
		0,
	};
	static constexpr uint32_t terminalsCount = 3;
	static constexpr uint32_t terminals[3] = {
		2,
		6,
		10,
	};

	static constexpr uint32_t scopesCount = 2;
	static constexpr flow::RuntimeScope scopes[2] = {
		{.opener = flow::InvalidIndex, .execPin = flow::InvalidIndex, .parent = flow::InvalidIndex, .depth = 0, .nodeBegin = 0, .nodeCount = 3},
		{.opener = flow::InvalidIndex, .execPin = flow::InvalidIndex, .parent = flow::InvalidIndex, .depth = 0, .nodeBegin = 3, .nodeCount = 8, .kind = flow::ScopeKind(2) /* function */, .barrier = flow::InvalidIndex, .branchScope = flow::InvalidIndex, .block = flow::InvalidIndex, .headerBytes = 0},
	};
	static constexpr uint32_t scopeNodesCount = 11;
	static constexpr uint32_t scopeNodes[11] = {
		0,
		1,
		2,
		3,
		4,
		5,
		6,
		7,
		8,
		9,
		10,
	};

	// The settings table, one CBOR array: an entry per setting every node's operation declares.
	static constexpr uint32_t settingsSize = 20;
	static constexpr uint8_t settings[20] = {
		0xd9, 0xd9, 0xf7, 0x82, 0x67, 0x64, 0x65, 0x66, 0x61, 0x75, 0x6c, 0x74, 0x67, 0x64, 0x65, 0x66,
		0x61, 0x75, 0x6c, 0x74,
	};

	// The constant table, one CBOR array: an entry per input of every node, null where the build
	// stored none.
	static constexpr uint32_t constantsSize = 17;
	static constexpr uint8_t constants[17] = {
		0xd9, 0xd9, 0xf7, 0x8d, 0x04, 0xf6, 0xf6, 0xf6, 0x02, 0xf6, 0x01, 0xf6, 0x01, 0xf6, 0xf6, 0xf6,
		0xf6,
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
	static constexpr uint32_t opsCount = 9;
	static constexpr flow::CompiledOpIdentity ops[9] = {
		{.name = StringView("compare.lessInt"), .signatureHash = 0x4fd8247d5edb0879ull, .localSchemaHash = 0x5f115ece5b7247e0ull, .parallel = flow::OpParallel(1) /* pure */},
		{.name = StringView("debug.trace"), .signatureHash = 0xf67a9b88fff342d5ull, .localSchemaHash = 0x0000000000000000ull, .parallel = flow::OpParallel(3) /* flow */},
		{.name = StringView("flow.branch"), .signatureHash = 0x075162dbc56d858aull, .localSchemaHash = 0x0000000000000000ull, .parallel = flow::OpParallel(3) /* flow */},
		{.name = StringView("flow.event"), .signatureHash = 0x20ae4b5e702837f5ull, .localSchemaHash = 0x0000000000000000ull, .parallel = flow::OpParallel(3) /* flow */},
		{.name = StringView("fn.recursive:fact"), .signatureHash = 0x2e557788130ae43cull, .localSchemaHash = 0x8c3f65ad3adf3592ull, .parallel = flow::OpParallel(0) /* serial */},
		{.name = StringView("fn.recursive:fact#entry"), .signatureHash = 0xc0955dff66f83472ull, .localSchemaHash = 0x50d5b01478543161ull, .parallel = flow::OpParallel(0) /* serial */},
		{.name = StringView("fn.recursive:fact#return:0"), .signatureHash = 0x460a3e07dc86acd0ull, .localSchemaHash = 0x0000000000000000ull, .parallel = flow::OpParallel(0) /* serial */},
		{.name = StringView("math.mulInt"), .signatureHash = 0x0b1ab5bd44d95b8full, .localSchemaHash = 0x7e3840a1b270bc06ull, .parallel = flow::OpParallel(1) /* pure */},
		{.name = StringView("math.subInt"), .signatureHash = 0x65d76f1caa829754ull, .localSchemaHash = 0xe57ef639e2264c85ull, .parallel = flow::OpParallel(1) /* pure */},
	};
	static constexpr uint32_t frameBytesCount = 2;
	static constexpr uint32_t frameBytes[2] = {
		144,
		376,
	};
	static constexpr uint32_t slotsCount = 11;
	static constexpr flow::FrameSlot slots[11] = {
		{.stateOffset = 0, .recordOffset = flow::InvalidIndex},
		{.stateOffset = 40, .recordOffset = 80},
		{.stateOffset = 104, .recordOffset = flow::InvalidIndex},
		{.stateOffset = 0, .recordOffset = 40},
		{.stateOffset = 48, .recordOffset = 88},
		{.stateOffset = 96, .recordOffset = flow::InvalidIndex},
		{.stateOffset = 136, .recordOffset = flow::InvalidIndex},
		{.stateOffset = 176, .recordOffset = 216},
		{.stateOffset = 224, .recordOffset = 264},
		{.stateOffset = 288, .recordOffset = 328},
		{.stateOffset = 336, .recordOffset = flow::InvalidIndex},
	};

	// The record of each node, field by field: what the door reads and writes.
	static constexpr uint32_t recordFieldsCount = 10;
	static constexpr flow::FieldShape recordFields[10] = {
		{.offset = 0, .size = 8, .type = flow::value::VarType(2) /* int */, .element = 0, .subtypeId = 0x0000000000000000ull, .valid = true},
		{.offset = 8, .size = 8, .type = flow::value::VarType(2) /* int */, .element = 0, .subtypeId = 0x0000000000000000ull, .valid = true},
		{.offset = 16, .size = 8, .type = flow::value::VarType(2) /* int */, .element = 0, .subtypeId = 0x0000000000000000ull, .valid = true},
		{.offset = 0, .size = 8, .type = flow::value::VarType(2) /* int */, .element = 0, .subtypeId = 0x0000000000000000ull, .valid = true},
		{.offset = 0, .size = 1, .type = flow::value::VarType(1) /* bool */, .element = 0, .subtypeId = 0x0000000000000000ull, .valid = true},
		{.offset = 0, .size = 8, .type = flow::value::VarType(2) /* int */, .element = 0, .subtypeId = 0x0000000000000000ull, .valid = true},
		{.offset = 0, .size = 8, .type = flow::value::VarType(2) /* int */, .element = 0, .subtypeId = 0x0000000000000000ull, .valid = true},
		{.offset = 8, .size = 8, .type = flow::value::VarType(2) /* int */, .element = 0, .subtypeId = 0x0000000000000000ull, .valid = true},
		{.offset = 16, .size = 8, .type = flow::value::VarType(2) /* int */, .element = 0, .subtypeId = 0x0000000000000000ull, .valid = true},
		{.offset = 0, .size = 8, .type = flow::value::VarType(2) /* int */, .element = 0, .subtypeId = 0x0000000000000000ull, .valid = true},
	};
	static constexpr uint32_t recordFieldBeginCount = 11;
	static constexpr uint32_t recordFieldBegin[11] = {
		0,
		0,
		3,
		3,
		4,
		5,
		5,
		5,
		6,
		9,
		10,
	};
	static constexpr uint32_t recordFieldCountCount = 11;
	static constexpr uint32_t recordFieldCount[11] = {
		0,
		3,
		0,
		1,
		1,
		0,
		0,
		1,
		3,
		1,
		0,
	};

	static constexpr flow::CompiledIdentity identity = {
		.name = StringView("recursive"),
		.assetHash = 0xe7781125ba7e5946ull,
		.textHash = 0x60b1b38bbd2b627dull,
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

} // namespace stappler::flow::gen::corpus_call_recursive

#endif /* GEN_CORPUS_CALL_RECURSIVE_GEN_H_ */
