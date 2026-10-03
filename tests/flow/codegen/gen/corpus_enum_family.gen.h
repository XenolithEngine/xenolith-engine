// Generated from the graph "<unnamed>". Do not edit: regenerate it.
//
// A graph as constants: the shape the build settled, the constant table, both
// contracts unbound, and the identity of what it was written against. Loaded through
// flow::StaticGraph<Tables>, which resolves the operations, checks the identity and binds the scene;
// run by Run<A>, the interpreter's own machine over the loaded rows, or held by a host as Engine<A>.
//
// The rows are half of it. The other half is in <name>.gen.cpp: one function per node, with the
// node index a compile-time constant, which is what the machine performs a node through.
#ifndef GEN_CORPUS_ENUM_FAMILY_GEN_H_
#define GEN_CORPUS_ENUM_FAMILY_GEN_H_

#include "SPFlowCompiled.h"
#include "SPFlowEnv.h"

namespace STAPPLER_VERSIONIZED stappler::flow::gen::corpus_enum_family {

// This unit's own way of performing a node: `step<N>` with the index a constant, the door over
// the tables below and the operation called by name where it has one. Written in
// <name>.gen.cpp, beside the bodies it calls, once per store a run
// may live in - the arena one and the fast one.
const flow::CompiledStepsT<flow::NoEnv> *steps();

struct Tables {
	static constexpr uint32_t NodeCount = 4;
	static constexpr uint32_t ScopeCount = 1;

	static constexpr uint32_t opNamesCount = 4;
	static constexpr StringView opNames[4] = {
		StringView("value.int32"),
		StringView("enum.fromInt32"),
		StringView("enum.toInt32"),
		StringView("math.mulInt32"),
	};

	static constexpr uint32_t nodesCount = 4;
	static constexpr flow::RuntimeNode nodes[4] = {
		{.id = 1, .dataInBegin = 0, .dataInCount = 0, .dataOutBegin = 0, .dataOutCount = 1, .execInBegin = 0, .execInCount = 0, .execOutBegin = 0, .execOutCount = 0, .constantBegin = 0, .sceneBegin = 0, .sceneCount = 0, .extBegin = 0, .extCount = 0, .scope = 0, .opensScope = flow::InvalidIndex, .slotInScope = 0, .sameScopeInputs = 0u, .allInputs = 0u, .crossScopeInBegin = 0, .crossScopeInCount = 0, .isEntry = true, .isTerminal = false},
		{.id = 2, .dataInBegin = 0, .dataInCount = 1, .dataOutBegin = 1, .dataOutCount = 1, .execInBegin = 0, .execInCount = 0, .execOutBegin = 0, .execOutCount = 0, .constantBegin = 1, .sceneBegin = 0, .sceneCount = 0, .extBegin = 0, .extCount = 0, .scope = 0, .opensScope = flow::InvalidIndex, .slotInScope = 1, .sameScopeInputs = 1u, .allInputs = 1u, .crossScopeInBegin = 0, .crossScopeInCount = 0, .isEntry = false, .isTerminal = false, .family = 0x52402d76586b9181ull, .familyName = StringView("enum.Direction")},
		{.id = 3, .dataInBegin = 1, .dataInCount = 1, .dataOutBegin = 2, .dataOutCount = 1, .execInBegin = 0, .execInCount = 0, .execOutBegin = 0, .execOutCount = 0, .constantBegin = 3, .sceneBegin = 0, .sceneCount = 0, .extBegin = 0, .extCount = 0, .scope = 0, .opensScope = flow::InvalidIndex, .slotInScope = 2, .sameScopeInputs = 1u, .allInputs = 1u, .crossScopeInBegin = 0, .crossScopeInCount = 0, .isEntry = false, .isTerminal = false},
		{.id = 4, .dataInBegin = 2, .dataInCount = 1, .dataOutBegin = 3, .dataOutCount = 0, .execInBegin = 0, .execInCount = 0, .execOutBegin = 0, .execOutCount = 0, .constantBegin = 4, .sceneBegin = 0, .sceneCount = 0, .extBegin = 0, .extCount = 0, .scope = 0, .opensScope = flow::InvalidIndex, .slotInScope = 3, .sameScopeInputs = 1u, .allInputs = 1u, .crossScopeInBegin = 0, .crossScopeInCount = 0, .isEntry = false, .isTerminal = true},
	};

	static constexpr uint32_t dataEdgesCount = 3;
	static constexpr flow::RuntimeDataEdge dataEdges[3] = {
		{.srcNode = 0, .srcPin = 0, .dstNode = 1, .dstPin = 0, .cast = flow::value::CastRule(1) /* same */, .needsArena = false},
		{.srcNode = 1, .srcPin = 0, .dstNode = 2, .dstPin = 0, .cast = flow::value::CastRule(1) /* same */, .needsArena = false},
		{.srcNode = 2, .srcPin = 0, .dstNode = 3, .dstPin = 0, .cast = flow::value::CastRule(1) /* same */, .needsArena = false},
	};
	static constexpr uint32_t execEdgesCount = 0;
	static constexpr flow::RuntimeExecEdge execEdges[1] = {
		{},
	};

	static constexpr uint32_t dataInCount = 3;
	static constexpr uint32_t dataIn[3] = {
		0,
		1,
		2,
	};
	static constexpr uint32_t dataOutCount = 3;
	static constexpr uint32_t dataOut[3] = {
		0,
		1,
		2,
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
	static constexpr uint32_t terminalsCount = 1;
	static constexpr uint32_t terminals[1] = {
		3,
	};

	static constexpr uint32_t scopesCount = 1;
	static constexpr flow::RuntimeScope scopes[1] = {
		{.opener = flow::InvalidIndex, .execPin = flow::InvalidIndex, .parent = flow::InvalidIndex, .depth = 0, .nodeBegin = 0, .nodeCount = 4},
	};
	static constexpr uint32_t scopeNodesCount = 4;
	static constexpr uint32_t scopeNodes[4] = {
		0,
		1,
		2,
		3,
	};

	// The constant table, one CBOR array: an entry per input of every node, null where the build
	// stored none.
	static constexpr uint32_t constantsSize = 24;
	static constexpr uint8_t constants[24] = {
		0xd9, 0xd9, 0xf7, 0x86, 0x04, 0xf6, 0x6e, 0x65, 0x6e, 0x75, 0x6d, 0x2e, 0x44, 0x69, 0x72, 0x65,
		0x63, 0x74, 0x69, 0x6f, 0x6e, 0xf6, 0xf6, 0x03,
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
		{.name = StringView("enum.fromInt32"), .signatureHash = 0xee35294223b138acull, .localSchemaHash = 0xf67454f270878b98ull, .parallel = flow::OpParallel(1) /* pure */},
		{.name = StringView("enum.toInt32"), .signatureHash = 0xb304133c678d0606ull, .localSchemaHash = 0x76ea6828485b0fe7ull, .parallel = flow::OpParallel(1) /* pure */},
		{.name = StringView("math.mulInt32"), .signatureHash = 0xbfc83dfc609e075bull, .localSchemaHash = 0xae1f6d944dd3d4e7ull, .parallel = flow::OpParallel(1) /* pure */},
		{.name = StringView("value.int32"), .signatureHash = 0xaffecbd260fc0151ull, .localSchemaHash = 0x4b010d92836835b2ull, .parallel = flow::OpParallel(1) /* pure */},
	};
	static constexpr uint32_t frameBytesCount = 1;
	static constexpr uint32_t frameBytes[1] = {
		192,
	};
	static constexpr uint32_t slotsCount = 4;
	static constexpr flow::FrameSlot slots[4] = {
		{.stateOffset = 0, .recordOffset = 40},
		{.stateOffset = 48, .recordOffset = 88},
		{.stateOffset = 96, .recordOffset = 136},
		{.stateOffset = 144, .recordOffset = 184},
	};

	// The record of each node, field by field: what the door reads and writes.
	static constexpr uint32_t recordFieldsCount = 4;
	static constexpr flow::FieldShape recordFields[4] = {
		{.offset = 0, .size = 4, .type = flow::value::VarType(14) /* int32 */, .element = 0, .subtypeId = 0x0000000000000000ull, .valid = true},
		{.offset = 0, .size = 8, .type = flow::value::VarType(12) /* enum */, .element = 0, .subtypeId = 0x52402d76586b9181ull, .valid = true},
		{.offset = 0, .size = 4, .type = flow::value::VarType(14) /* int32 */, .element = 0, .subtypeId = 0x0000000000000000ull, .valid = true},
		{.offset = 0, .size = 4, .type = flow::value::VarType(14) /* int32 */, .element = 0, .subtypeId = 0x0000000000000000ull, .valid = true},
	};
	static constexpr uint32_t recordFieldBeginCount = 4;
	static constexpr uint32_t recordFieldBegin[4] = {
		0,
		1,
		2,
		3,
	};
	static constexpr uint32_t recordFieldCountCount = 4;
	static constexpr uint32_t recordFieldCount[4] = {
		1,
		1,
		1,
		1,
	};

	static constexpr flow::CompiledIdentity identity = {
		.name = StringView(),
		.assetHash = 0x77055935ed5a96cbull,
		.textHash = 0x1fa9fece9f078537ull,
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

} // namespace stappler::flow::gen::corpus_enum_family

#endif /* GEN_CORPUS_ENUM_FAMILY_GEN_H_ */
