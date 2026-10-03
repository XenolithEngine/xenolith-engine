// Generated from the graph "<unnamed>". Do not edit: regenerate it.
//
// A graph as constants: the shape the build settled, the constant table, both
// contracts unbound, and the identity of what it was written against. Loaded through
// flow::StaticGraph<Tables>, which resolves the operations, checks the identity and binds the scene;
// run by Run<A>, the interpreter's own machine over the loaded rows, or held by a host as Engine<A>.
//
// The rows are half of it. The other half is in <name>.gen.cpp: one function per node, with the
// node index a compile-time constant, which is what the machine performs a node through.
#ifndef GEN_CORPUS_NUMERIC32_GEN_H_
#define GEN_CORPUS_NUMERIC32_GEN_H_

#include "SPFlowCompiled.h"
#include "SPFlowEnv.h"

namespace STAPPLER_VERSIONIZED stappler::flow::gen::corpus_numeric32 {

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
	static constexpr uint32_t NodeCount = 9;
	static constexpr uint32_t ScopeCount = 1;

	static constexpr uint32_t opNamesCount = 9;
	static constexpr StringView opNames[9] = {
		StringView("value.int32"),
		StringView("math.addInt32"),
		StringView("convert.int32ToFloat32"),
		StringView("math.mulFloat32"),
		StringView("value.vec3"),
		StringView("math.lengthVec3"),
		StringView("math.addFloat32"),
		StringView("convert.intToUInt32"),
		StringView("math.addUInt32"),
	};

	static constexpr uint32_t nodesCount = 9;
	static constexpr flow::RuntimeNode nodes[9] = {
		{.id = 1, .dataInBegin = 0, .dataInCount = 0, .dataOutBegin = 0, .dataOutCount = 1, .execInBegin = 0, .execInCount = 0, .execOutBegin = 0, .execOutCount = 0, .constantBegin = 0, .sceneBegin = 0, .sceneCount = 0, .extBegin = 0, .extCount = 0, .scope = 0, .opensScope = flow::InvalidIndex, .slotInScope = 0, .sameScopeInputs = 0u, .allInputs = 0u, .crossScopeInBegin = 0, .crossScopeInCount = 0, .isEntry = true, .isTerminal = false},
		{.id = 2, .dataInBegin = 0, .dataInCount = 1, .dataOutBegin = 1, .dataOutCount = 1, .execInBegin = 0, .execInCount = 0, .execOutBegin = 0, .execOutCount = 0, .constantBegin = 1, .sceneBegin = 0, .sceneCount = 0, .extBegin = 0, .extCount = 0, .scope = 0, .opensScope = flow::InvalidIndex, .slotInScope = 1, .sameScopeInputs = 1u, .allInputs = 1u, .crossScopeInBegin = 0, .crossScopeInCount = 0, .isEntry = false, .isTerminal = false},
		{.id = 3, .dataInBegin = 1, .dataInCount = 1, .dataOutBegin = 2, .dataOutCount = 1, .execInBegin = 0, .execInCount = 0, .execOutBegin = 0, .execOutCount = 0, .constantBegin = 3, .sceneBegin = 0, .sceneCount = 0, .extBegin = 0, .extCount = 0, .scope = 0, .opensScope = flow::InvalidIndex, .slotInScope = 2, .sameScopeInputs = 1u, .allInputs = 1u, .crossScopeInBegin = 0, .crossScopeInCount = 0, .isEntry = false, .isTerminal = false},
		{.id = 4, .dataInBegin = 2, .dataInCount = 1, .dataOutBegin = 3, .dataOutCount = 1, .execInBegin = 0, .execInCount = 0, .execOutBegin = 0, .execOutCount = 0, .constantBegin = 4, .sceneBegin = 0, .sceneCount = 0, .extBegin = 0, .extCount = 0, .scope = 0, .opensScope = flow::InvalidIndex, .slotInScope = 3, .sameScopeInputs = 1u, .allInputs = 1u, .crossScopeInBegin = 0, .crossScopeInCount = 0, .isEntry = false, .isTerminal = false},
		{.id = 5, .dataInBegin = 3, .dataInCount = 0, .dataOutBegin = 4, .dataOutCount = 1, .execInBegin = 0, .execInCount = 0, .execOutBegin = 0, .execOutCount = 0, .constantBegin = 6, .sceneBegin = 0, .sceneCount = 0, .extBegin = 0, .extCount = 0, .scope = 0, .opensScope = flow::InvalidIndex, .slotInScope = 4, .sameScopeInputs = 0u, .allInputs = 0u, .crossScopeInBegin = 0, .crossScopeInCount = 0, .isEntry = true, .isTerminal = false},
		{.id = 6, .dataInBegin = 3, .dataInCount = 1, .dataOutBegin = 5, .dataOutCount = 1, .execInBegin = 0, .execInCount = 0, .execOutBegin = 0, .execOutCount = 0, .constantBegin = 7, .sceneBegin = 0, .sceneCount = 0, .extBegin = 0, .extCount = 0, .scope = 0, .opensScope = flow::InvalidIndex, .slotInScope = 5, .sameScopeInputs = 1u, .allInputs = 1u, .crossScopeInBegin = 0, .crossScopeInCount = 0, .isEntry = false, .isTerminal = false},
		{.id = 7, .dataInBegin = 4, .dataInCount = 2, .dataOutBegin = 6, .dataOutCount = 0, .execInBegin = 0, .execInCount = 0, .execOutBegin = 0, .execOutCount = 0, .constantBegin = 8, .sceneBegin = 0, .sceneCount = 0, .extBegin = 0, .extCount = 0, .scope = 0, .opensScope = flow::InvalidIndex, .slotInScope = 6, .sameScopeInputs = 3u, .allInputs = 3u, .crossScopeInBegin = 0, .crossScopeInCount = 0, .isEntry = false, .isTerminal = true},
		{.id = 8, .dataInBegin = 6, .dataInCount = 0, .dataOutBegin = 6, .dataOutCount = 1, .execInBegin = 0, .execInCount = 0, .execOutBegin = 0, .execOutCount = 0, .constantBegin = 10, .sceneBegin = 0, .sceneCount = 0, .extBegin = 0, .extCount = 0, .scope = 0, .opensScope = flow::InvalidIndex, .slotInScope = 7, .sameScopeInputs = 0u, .allInputs = 0u, .crossScopeInBegin = 0, .crossScopeInCount = 0, .isEntry = true, .isTerminal = false},
		{.id = 9, .dataInBegin = 6, .dataInCount = 1, .dataOutBegin = 7, .dataOutCount = 0, .execInBegin = 0, .execInCount = 0, .execOutBegin = 0, .execOutCount = 0, .constantBegin = 11, .sceneBegin = 0, .sceneCount = 0, .extBegin = 0, .extCount = 0, .scope = 0, .opensScope = flow::InvalidIndex, .slotInScope = 8, .sameScopeInputs = 1u, .allInputs = 1u, .crossScopeInBegin = 0, .crossScopeInCount = 0, .isEntry = false, .isTerminal = true},
	};

	static constexpr uint32_t dataEdgesCount = 7;
	static constexpr flow::RuntimeDataEdge dataEdges[7] = {
		{.srcNode = 0, .srcPin = 0, .dstNode = 1, .dstPin = 0, .cast = flow::value::CastRule(1) /* same */, .needsArena = false},
		{.srcNode = 1, .srcPin = 0, .dstNode = 2, .dstPin = 0, .cast = flow::value::CastRule(1) /* same */, .needsArena = false},
		{.srcNode = 2, .srcPin = 0, .dstNode = 3, .dstPin = 0, .cast = flow::value::CastRule(1) /* same */, .needsArena = false},
		{.srcNode = 4, .srcPin = 0, .dstNode = 5, .dstPin = 0, .cast = flow::value::CastRule(1) /* same */, .needsArena = false},
		{.srcNode = 3, .srcPin = 0, .dstNode = 6, .dstPin = 0, .cast = flow::value::CastRule(1) /* same */, .needsArena = false},
		{.srcNode = 5, .srcPin = 0, .dstNode = 6, .dstPin = 1, .cast = flow::value::CastRule(1) /* same */, .needsArena = false},
		{.srcNode = 7, .srcPin = 0, .dstNode = 8, .dstPin = 0, .cast = flow::value::CastRule(1) /* same */, .needsArena = false},
	};
	static constexpr uint32_t execEdgesCount = 0;
	static constexpr flow::RuntimeExecEdge execEdges[1] = {
		{},
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
		1,
		2,
		4,
		3,
		5,
		6,
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
	static constexpr uint32_t entriesCount = 3;
	static constexpr uint32_t entries[3] = {
		0,
		4,
		7,
	};
	static constexpr uint32_t terminalsCount = 2;
	static constexpr uint32_t terminals[2] = {
		6,
		8,
	};

	static constexpr uint32_t scopesCount = 1;
	static constexpr flow::RuntimeScope scopes[1] = {
		{.opener = flow::InvalidIndex, .execPin = flow::InvalidIndex, .parent = flow::InvalidIndex, .depth = 0, .nodeBegin = 0, .nodeCount = 9},
	};
	static constexpr uint32_t scopeNodesCount = 9;
	static constexpr uint32_t scopeNodes[9] = {
		0,
		1,
		2,
		3,
		4,
		5,
		6,
		7,
		8,
	};

	// The constant table, one CBOR array: an entry per input of every node, null where the build
	// stored none.
	static constexpr uint32_t constantsSize = 36;
	static constexpr uint8_t constants[36] = {
		0xd9, 0xd9, 0xf7, 0x8d, 0x1a, 0x7f, 0xff, 0xff, 0xff, 0xf6, 0x02, 0xf6, 0xf6, 0xf9, 0x38, 0x00,
		0x83, 0xf9, 0x42, 0x00, 0xf9, 0x44, 0x00, 0xf9, 0x4a, 0x00, 0xf6, 0xf6, 0xf6, 0x1a, 0xff, 0xff,
		0xff, 0xff, 0xf6, 0x01,
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
		{.name = StringView("convert.int32ToFloat32"), .signatureHash = 0xc11fc29de1963debull, .localSchemaHash = 0x0dde39e0cced3c61ull, .parallel = flow::OpParallel(1) /* pure */},
		{.name = StringView("convert.intToUInt32"), .signatureHash = 0x00e104609f454641ull, .localSchemaHash = 0x5186f400c91edf0full, .parallel = flow::OpParallel(1) /* pure */},
		{.name = StringView("math.addFloat32"), .signatureHash = 0x7f2bfbac4fefc3a3ull, .localSchemaHash = 0x184d0e2aa94b33f0ull, .parallel = flow::OpParallel(1) /* pure */},
		{.name = StringView("math.addInt32"), .signatureHash = 0xe1b7a9c60389fc93ull, .localSchemaHash = 0x3363469e3048199cull, .parallel = flow::OpParallel(1) /* pure */},
		{.name = StringView("math.addUInt32"), .signatureHash = 0x28634b23620abc17ull, .localSchemaHash = 0xde349c46984a8766ull, .parallel = flow::OpParallel(1) /* pure */},
		{.name = StringView("math.lengthVec3"), .signatureHash = 0x3712662416275e56ull, .localSchemaHash = 0x8a7b4b473fee3d29ull, .parallel = flow::OpParallel(1) /* pure */},
		{.name = StringView("math.mulFloat32"), .signatureHash = 0x2fb9cded2be73869ull, .localSchemaHash = 0x0110dedd4f8bdbb6ull, .parallel = flow::OpParallel(1) /* pure */},
		{.name = StringView("value.int32"), .signatureHash = 0xaffecbd260fc0151ull, .localSchemaHash = 0x4b010d92836835b2ull, .parallel = flow::OpParallel(1) /* pure */},
		{.name = StringView("value.vec3"), .signatureHash = 0x504a4f6f9236a23dull, .localSchemaHash = 0x4dd044f76cd3877bull, .parallel = flow::OpParallel(1) /* pure */},
	};
	static constexpr uint32_t frameBytesCount = 1;
	static constexpr uint32_t frameBytes[1] = {
		440,
	};
	static constexpr uint32_t slotsCount = 9;
	static constexpr flow::FrameSlot slots[9] = {
		{.stateOffset = 0, .recordOffset = 40},
		{.stateOffset = 48, .recordOffset = 88},
		{.stateOffset = 96, .recordOffset = 136},
		{.stateOffset = 144, .recordOffset = 184},
		{.stateOffset = 192, .recordOffset = 232},
		{.stateOffset = 248, .recordOffset = 288},
		{.stateOffset = 296, .recordOffset = 336},
		{.stateOffset = 344, .recordOffset = 384},
		{.stateOffset = 392, .recordOffset = 432},
	};

	// The record of each node, field by field: what the door reads and writes.
	static constexpr uint32_t recordFieldsCount = 9;
	static constexpr flow::FieldShape recordFields[9] = {
		{.offset = 0, .size = 4, .type = flow::value::VarType(14) /* int32 */, .element = 0, .subtypeId = 0x0000000000000000ull, .valid = true},
		{.offset = 0, .size = 4, .type = flow::value::VarType(14) /* int32 */, .element = 0, .subtypeId = 0x0000000000000000ull, .valid = true},
		{.offset = 0, .size = 4, .type = flow::value::VarType(16) /* float32 */, .element = 0, .subtypeId = 0x0000000000000000ull, .valid = true},
		{.offset = 0, .size = 4, .type = flow::value::VarType(16) /* float32 */, .element = 0, .subtypeId = 0x0000000000000000ull, .valid = true},
		{.offset = 0, .size = 12, .type = flow::value::VarType(5) /* vec3 */, .element = 0, .subtypeId = 0x0000000000000000ull, .valid = true},
		{.offset = 0, .size = 4, .type = flow::value::VarType(16) /* float32 */, .element = 0, .subtypeId = 0x0000000000000000ull, .valid = true},
		{.offset = 0, .size = 4, .type = flow::value::VarType(16) /* float32 */, .element = 0, .subtypeId = 0x0000000000000000ull, .valid = true},
		{.offset = 0, .size = 4, .type = flow::value::VarType(15) /* uint32 */, .element = 0, .subtypeId = 0x0000000000000000ull, .valid = true},
		{.offset = 0, .size = 4, .type = flow::value::VarType(15) /* uint32 */, .element = 0, .subtypeId = 0x0000000000000000ull, .valid = true},
	};
	static constexpr uint32_t recordFieldBeginCount = 9;
	static constexpr uint32_t recordFieldBegin[9] = {
		0,
		1,
		2,
		3,
		4,
		5,
		6,
		7,
		8,
	};
	static constexpr uint32_t recordFieldCountCount = 9;
	static constexpr uint32_t recordFieldCount[9] = {
		1,
		1,
		1,
		1,
		1,
		1,
		1,
		1,
		1,
	};

	static constexpr flow::CompiledIdentity identity = {
		.name = StringView(),
		.assetHash = 0x7cf72f4abd386cf3ull,
		.textHash = 0x175784925b42ee2cull,
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

} // namespace stappler::flow::gen::corpus_numeric32

#endif /* GEN_CORPUS_NUMERIC32_GEN_H_ */
