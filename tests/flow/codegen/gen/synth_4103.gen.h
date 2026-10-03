// Generated from the graph "synth". Do not edit: regenerate it.
//
// A graph as constants: the shape the build settled, the constant table, both
// contracts unbound, and the identity of what it was written against. Loaded through
// flow::StaticGraph<Tables>, which resolves the operations, checks the identity and binds the scene;
// run by Run<A>, the interpreter's own machine over the loaded rows, or held by a host as Engine<A>.
//
// The rows are half of it. The other half is in <name>.gen.cpp: one function per node, with the
// node index a compile-time constant, which is what the machine performs a node through.
#ifndef GEN_SYNTH_4103_GEN_H_
#define GEN_SYNTH_4103_GEN_H_

#include "SPFlowCompiled.h"
#include "SPFlowEnv.h"

namespace STAPPLER_VERSIONIZED stappler::flow::gen::synth_4103 {

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
	static constexpr uint32_t NodeCount = 17;
	static constexpr uint32_t ScopeCount = 3;

	static constexpr uint32_t opNamesCount = 17;
	static constexpr StringView opNames[17] = {
		StringView("flow.event"),
		StringView("value.int"),
		StringView("flow.sequence"),
		StringView("debug.trace"),
		StringView("debug.trace"),
		StringView("debug.trace"),
		StringView("math.addInt"),
		StringView("compare.equalInt"),
		StringView("flow.branch"),
		StringView("debug.trace"),
		StringView("math.addInt"),
		StringView("compare.equalInt"),
		StringView("flow.branch"),
		StringView("flow.forEach"),
		StringView("flow.forEach"),
		StringView("flow.sequence"),
		StringView("flow.forEach"),
	};

	static constexpr uint32_t nodesCount = 17;
	static constexpr flow::RuntimeNode nodes[17] = {
		{.id = 1, .dataInBegin = 0, .dataInCount = 0, .dataOutBegin = 0, .dataOutCount = 0, .execInBegin = 0, .execInCount = 0, .execOutBegin = 0, .execOutCount = 1, .constantBegin = 0, .sceneBegin = 0, .sceneCount = 0, .extBegin = 0, .extCount = 0, .scope = 0, .opensScope = flow::InvalidIndex, .slotInScope = 0, .sameScopeInputs = 0u, .allInputs = 0u, .crossScopeInBegin = 0, .crossScopeInCount = 0, .isEntry = true, .isTerminal = false},
		{.id = 2, .dataInBegin = 0, .dataInCount = 0, .dataOutBegin = 0, .dataOutCount = 4, .execInBegin = 0, .execInCount = 0, .execOutBegin = 1, .execOutCount = 0, .constantBegin = 0, .sceneBegin = 0, .sceneCount = 0, .extBegin = 0, .extCount = 0, .scope = 0, .opensScope = flow::InvalidIndex, .slotInScope = 1, .sameScopeInputs = 0u, .allInputs = 0u, .crossScopeInBegin = 0, .crossScopeInCount = 0, .isEntry = true, .isTerminal = false},
		{.id = 3, .dataInBegin = 0, .dataInCount = 0, .dataOutBegin = 4, .dataOutCount = 0, .execInBegin = 0, .execInCount = 1, .execOutBegin = 1, .execOutCount = 1, .constantBegin = 1, .sceneBegin = 0, .sceneCount = 0, .extBegin = 0, .extCount = 0, .scope = 0, .opensScope = flow::InvalidIndex, .slotInScope = 2, .sameScopeInputs = 0u, .allInputs = 0u, .crossScopeInBegin = 0, .crossScopeInCount = 0, .isEntry = false, .isTerminal = false},
		{.id = 4, .dataInBegin = 0, .dataInCount = 0, .dataOutBegin = 4, .dataOutCount = 0, .execInBegin = 1, .execInCount = 1, .execOutBegin = 2, .execOutCount = 1, .constantBegin = 1, .sceneBegin = 0, .sceneCount = 0, .extBegin = 0, .extCount = 0, .scope = 0, .opensScope = flow::InvalidIndex, .slotInScope = 3, .sameScopeInputs = 0u, .allInputs = 0u, .crossScopeInBegin = 0, .crossScopeInCount = 0, .isEntry = false, .isTerminal = false},
		{.id = 5, .dataInBegin = 0, .dataInCount = 0, .dataOutBegin = 4, .dataOutCount = 0, .execInBegin = 2, .execInCount = 1, .execOutBegin = 3, .execOutCount = 1, .constantBegin = 2, .sceneBegin = 0, .sceneCount = 0, .extBegin = 0, .extCount = 0, .scope = 0, .opensScope = flow::InvalidIndex, .slotInScope = 4, .sameScopeInputs = 0u, .allInputs = 0u, .crossScopeInBegin = 0, .crossScopeInCount = 0, .isEntry = false, .isTerminal = false},
		{.id = 6, .dataInBegin = 0, .dataInCount = 0, .dataOutBegin = 4, .dataOutCount = 0, .execInBegin = 3, .execInCount = 1, .execOutBegin = 4, .execOutCount = 1, .constantBegin = 3, .sceneBegin = 0, .sceneCount = 0, .extBegin = 0, .extCount = 0, .scope = 0, .opensScope = flow::InvalidIndex, .slotInScope = 5, .sameScopeInputs = 0u, .allInputs = 0u, .crossScopeInBegin = 0, .crossScopeInCount = 0, .isEntry = false, .isTerminal = false},
		{.id = 7, .dataInBegin = 0, .dataInCount = 1, .dataOutBegin = 4, .dataOutCount = 0, .execInBegin = 4, .execInCount = 0, .execOutBegin = 5, .execOutCount = 0, .constantBegin = 4, .sceneBegin = 0, .sceneCount = 0, .extBegin = 0, .extCount = 0, .scope = 0, .opensScope = flow::InvalidIndex, .slotInScope = 6, .sameScopeInputs = 1u, .allInputs = 1u, .crossScopeInBegin = 0, .crossScopeInCount = 0, .isEntry = false, .isTerminal = true},
		{.id = 8, .dataInBegin = 1, .dataInCount = 1, .dataOutBegin = 4, .dataOutCount = 1, .execInBegin = 4, .execInCount = 0, .execOutBegin = 5, .execOutCount = 0, .constantBegin = 6, .sceneBegin = 0, .sceneCount = 0, .extBegin = 0, .extCount = 0, .scope = 0, .opensScope = flow::InvalidIndex, .slotInScope = 7, .sameScopeInputs = 1u, .allInputs = 1u, .crossScopeInBegin = 0, .crossScopeInCount = 0, .isEntry = false, .isTerminal = false},
		{.id = 9, .dataInBegin = 2, .dataInCount = 1, .dataOutBegin = 5, .dataOutCount = 0, .execInBegin = 4, .execInCount = 1, .execOutBegin = 5, .execOutCount = 1, .constantBegin = 8, .sceneBegin = 0, .sceneCount = 0, .extBegin = 0, .extCount = 0, .scope = 0, .opensScope = flow::InvalidIndex, .slotInScope = 8, .sameScopeInputs = 1u, .allInputs = 1u, .crossScopeInBegin = 0, .crossScopeInCount = 0, .isEntry = false, .isTerminal = false},
		{.id = 10, .dataInBegin = 3, .dataInCount = 0, .dataOutBegin = 5, .dataOutCount = 0, .execInBegin = 5, .execInCount = 1, .execOutBegin = 6, .execOutCount = 1, .constantBegin = 9, .sceneBegin = 0, .sceneCount = 0, .extBegin = 0, .extCount = 0, .scope = 0, .opensScope = flow::InvalidIndex, .slotInScope = 9, .sameScopeInputs = 0u, .allInputs = 0u, .crossScopeInBegin = 0, .crossScopeInCount = 0, .isEntry = false, .isTerminal = false},
		{.id = 11, .dataInBegin = 3, .dataInCount = 1, .dataOutBegin = 5, .dataOutCount = 0, .execInBegin = 6, .execInCount = 0, .execOutBegin = 7, .execOutCount = 0, .constantBegin = 10, .sceneBegin = 0, .sceneCount = 0, .extBegin = 0, .extCount = 0, .scope = 0, .opensScope = flow::InvalidIndex, .slotInScope = 10, .sameScopeInputs = 1u, .allInputs = 1u, .crossScopeInBegin = 0, .crossScopeInCount = 0, .isEntry = false, .isTerminal = true},
		{.id = 12, .dataInBegin = 4, .dataInCount = 1, .dataOutBegin = 5, .dataOutCount = 1, .execInBegin = 6, .execInCount = 0, .execOutBegin = 7, .execOutCount = 0, .constantBegin = 12, .sceneBegin = 0, .sceneCount = 0, .extBegin = 0, .extCount = 0, .scope = 0, .opensScope = flow::InvalidIndex, .slotInScope = 11, .sameScopeInputs = 1u, .allInputs = 1u, .crossScopeInBegin = 0, .crossScopeInCount = 0, .isEntry = false, .isTerminal = false},
		{.id = 13, .dataInBegin = 5, .dataInCount = 1, .dataOutBegin = 6, .dataOutCount = 0, .execInBegin = 6, .execInCount = 1, .execOutBegin = 7, .execOutCount = 1, .constantBegin = 14, .sceneBegin = 0, .sceneCount = 0, .extBegin = 0, .extCount = 0, .scope = 0, .opensScope = flow::InvalidIndex, .slotInScope = 12, .sameScopeInputs = 1u, .allInputs = 1u, .crossScopeInBegin = 0, .crossScopeInCount = 0, .isEntry = false, .isTerminal = false},
		{.id = 14, .dataInBegin = 6, .dataInCount = 0, .dataOutBegin = 6, .dataOutCount = 0, .execInBegin = 7, .execInCount = 1, .execOutBegin = 8, .execOutCount = 2, .constantBegin = 15, .sceneBegin = 0, .sceneCount = 0, .extBegin = 0, .extCount = 0, .scope = 0, .opensScope = 1, .slotInScope = 13, .sameScopeInputs = 0u, .allInputs = 0u, .crossScopeInBegin = 0, .crossScopeInCount = 0, .isEntry = false, .isTerminal = false},
		{.id = 15, .dataInBegin = 6, .dataInCount = 0, .dataOutBegin = 6, .dataOutCount = 0, .execInBegin = 8, .execInCount = 1, .execOutBegin = 10, .execOutCount = 1, .constantBegin = 16, .sceneBegin = 0, .sceneCount = 0, .extBegin = 0, .extCount = 0, .scope = 1, .opensScope = 2, .slotInScope = 0, .sameScopeInputs = 0u, .allInputs = 0u, .crossScopeInBegin = 0, .crossScopeInCount = 0, .isEntry = false, .isTerminal = false},
		{.id = 16, .dataInBegin = 6, .dataInCount = 0, .dataOutBegin = 6, .dataOutCount = 0, .execInBegin = 9, .execInCount = 1, .execOutBegin = 11, .execOutCount = 0, .constantBegin = 17, .sceneBegin = 0, .sceneCount = 0, .extBegin = 0, .extCount = 0, .scope = 2, .opensScope = flow::InvalidIndex, .slotInScope = 0, .sameScopeInputs = 0u, .allInputs = 0u, .crossScopeInBegin = 0, .crossScopeInCount = 0, .isEntry = false, .isTerminal = true},
		{.id = 17, .dataInBegin = 6, .dataInCount = 0, .dataOutBegin = 6, .dataOutCount = 0, .execInBegin = 10, .execInCount = 1, .execOutBegin = 11, .execOutCount = 0, .constantBegin = 17, .sceneBegin = 0, .sceneCount = 0, .extBegin = 0, .extCount = 0, .scope = 0, .opensScope = flow::InvalidIndex, .slotInScope = 14, .sameScopeInputs = 0u, .allInputs = 0u, .crossScopeInBegin = 0, .crossScopeInCount = 0, .isEntry = false, .isTerminal = true},
	};

	static constexpr uint32_t dataEdgesCount = 6;
	static constexpr flow::RuntimeDataEdge dataEdges[6] = {
		{.srcNode = 1, .srcPin = 0, .dstNode = 6, .dstPin = 0, .cast = flow::value::CastRule(1) /* same */, .needsArena = false},
		{.srcNode = 1, .srcPin = 0, .dstNode = 7, .dstPin = 0, .cast = flow::value::CastRule(1) /* same */, .needsArena = false},
		{.srcNode = 7, .srcPin = 0, .dstNode = 8, .dstPin = 0, .cast = flow::value::CastRule(1) /* same */, .needsArena = false},
		{.srcNode = 1, .srcPin = 0, .dstNode = 10, .dstPin = 0, .cast = flow::value::CastRule(1) /* same */, .needsArena = false},
		{.srcNode = 1, .srcPin = 0, .dstNode = 11, .dstPin = 0, .cast = flow::value::CastRule(1) /* same */, .needsArena = false},
		{.srcNode = 11, .srcPin = 0, .dstNode = 12, .dstPin = 0, .cast = flow::value::CastRule(1) /* same */, .needsArena = false},
	};
	static constexpr uint32_t execEdgesCount = 11;
	static constexpr flow::RuntimeExecEdge execEdges[11] = {
		{.srcNode = 0, .srcPin = 0, .dstNode = 2, .backEdge = false},
		{.srcNode = 2, .srcPin = 0, .dstNode = 3, .backEdge = false},
		{.srcNode = 3, .srcPin = 0, .dstNode = 4, .backEdge = false},
		{.srcNode = 4, .srcPin = 0, .dstNode = 5, .backEdge = false},
		{.srcNode = 5, .srcPin = 0, .dstNode = 8, .backEdge = false},
		{.srcNode = 8, .srcPin = 0, .dstNode = 9, .backEdge = false},
		{.srcNode = 9, .srcPin = 0, .dstNode = 12, .backEdge = false},
		{.srcNode = 12, .srcPin = 0, .dstNode = 13, .backEdge = false},
		{.srcNode = 13, .srcPin = 0, .dstNode = 14, .backEdge = false},
		{.srcNode = 13, .srcPin = 1, .dstNode = 16, .backEdge = false},
		{.srcNode = 14, .srcPin = 0, .dstNode = 15, .backEdge = false},
	};

	static constexpr uint32_t dataInCount = 6;
	static constexpr uint32_t dataIn[6] = {
		0,
		1,
		2,
		3,
		4,
		5,
	};
	static constexpr uint32_t dataOutCount = 6;
	static constexpr uint32_t dataOut[6] = {
		0,
		1,
		3,
		4,
		2,
		5,
	};
	static constexpr uint32_t execInCount = 11;
	static constexpr uint32_t execIn[11] = {
		0,
		1,
		2,
		3,
		4,
		5,
		6,
		7,
		8,
		10,
		9,
	};
	static constexpr uint32_t execOutCount = 11;
	static constexpr uint32_t execOut[11] = {
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
	static constexpr uint32_t crossScopeInCount = 0;
	static constexpr uint32_t crossScopeIn[1] = {
		{},
	};
	static constexpr uint32_t entriesCount = 2;
	static constexpr uint32_t entries[2] = {
		0,
		1,
	};
	static constexpr uint32_t terminalsCount = 4;
	static constexpr uint32_t terminals[4] = {
		6,
		10,
		15,
		16,
	};

	static constexpr uint32_t scopesCount = 3;
	static constexpr flow::RuntimeScope scopes[3] = {
		{.opener = flow::InvalidIndex, .execPin = flow::InvalidIndex, .parent = flow::InvalidIndex, .depth = 0, .nodeBegin = 0, .nodeCount = 15},
		{.opener = 13, .execPin = 0, .parent = 0, .depth = 1, .nodeBegin = 15, .nodeCount = 1},
		{.opener = 14, .execPin = 0, .parent = 1, .depth = 2, .nodeBegin = 16, .nodeCount = 1},
	};
	static constexpr uint32_t scopeNodesCount = 17;
	static constexpr uint32_t scopeNodes[17] = {
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
		11,
		12,
		13,
		16,
		14,
		15,
	};

	// The constant table, one CBOR array: an entry per input of every node, null where the build
	// stored none.
	static constexpr uint32_t constantsSize = 25;
	static constexpr uint8_t constants[25] = {
		0xd9, 0xd9, 0xf7, 0x92, 0x01, 0xf6, 0xf6, 0xf6, 0xf6, 0x02, 0xf6, 0x02, 0xf6, 0xf6, 0xf6, 0x05,
		0xf6, 0x00, 0xf6, 0x81, 0x0a, 0x81, 0x0a, 0x81, 0x0a,
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
	static constexpr uint32_t opsCount = 8;
	static constexpr flow::CompiledOpIdentity ops[8] = {
		{.name = StringView("compare.equalInt"), .signatureHash = 0x78c712f545aff7a7ull, .localSchemaHash = 0x3a025ce96859274full, .parallel = flow::OpParallel(1) /* pure */},
		{.name = StringView("debug.trace"), .signatureHash = 0xf67a9b88fff342d5ull, .localSchemaHash = 0x0000000000000000ull, .parallel = flow::OpParallel(3) /* flow */},
		{.name = StringView("flow.branch"), .signatureHash = 0x075162dbc56d858aull, .localSchemaHash = 0x0000000000000000ull, .parallel = flow::OpParallel(3) /* flow */},
		{.name = StringView("flow.event"), .signatureHash = 0x20ae4b5e702837f5ull, .localSchemaHash = 0x0000000000000000ull, .parallel = flow::OpParallel(3) /* flow */},
		{.name = StringView("flow.forEach"), .signatureHash = 0x5b8463243ba6a5efull, .localSchemaHash = 0x656d70e76ea467a7ull, .parallel = flow::OpParallel(3) /* flow */},
		{.name = StringView("flow.sequence"), .signatureHash = 0x8e7f2924917a20daull, .localSchemaHash = 0x0000000000000000ull, .parallel = flow::OpParallel(3) /* flow */},
		{.name = StringView("math.addInt"), .signatureHash = 0xb5fdb033feaaf02eull, .localSchemaHash = 0x12043720acfa39b9ull, .parallel = flow::OpParallel(1) /* pure */},
		{.name = StringView("value.int"), .signatureHash = 0x0338a64f47cb2499ull, .localSchemaHash = 0x813de10b1456ef71ull, .parallel = flow::OpParallel(1) /* pure */},
	};
	static constexpr uint32_t frameBytesCount = 3;
	static constexpr uint32_t frameBytes[3] = {
		688,
		64,
		40,
	};
	static constexpr uint32_t slotsCount = 17;
	static constexpr flow::FrameSlot slots[17] = {
		{.stateOffset = 0, .recordOffset = flow::InvalidIndex},
		{.stateOffset = 40, .recordOffset = 80},
		{.stateOffset = 88, .recordOffset = flow::InvalidIndex},
		{.stateOffset = 128, .recordOffset = flow::InvalidIndex},
		{.stateOffset = 168, .recordOffset = flow::InvalidIndex},
		{.stateOffset = 208, .recordOffset = flow::InvalidIndex},
		{.stateOffset = 248, .recordOffset = 288},
		{.stateOffset = 296, .recordOffset = 336},
		{.stateOffset = 344, .recordOffset = flow::InvalidIndex},
		{.stateOffset = 384, .recordOffset = flow::InvalidIndex},
		{.stateOffset = 424, .recordOffset = 464},
		{.stateOffset = 472, .recordOffset = 512},
		{.stateOffset = 520, .recordOffset = flow::InvalidIndex},
		{.stateOffset = 560, .recordOffset = 600},
		{.stateOffset = 0, .recordOffset = 40},
		{.stateOffset = 0, .recordOffset = flow::InvalidIndex},
		{.stateOffset = 624, .recordOffset = 664},
	};

	// The record of each node, field by field: what the door reads and writes.
	static constexpr uint32_t recordFieldsCount = 14;
	static constexpr flow::FieldShape recordFields[14] = {
		{.offset = 0, .size = 8, .type = flow::value::VarType(2) /* int */, .element = 0, .subtypeId = 0x0000000000000000ull, .valid = true},
		{.offset = 0, .size = 8, .type = flow::value::VarType(2) /* int */, .element = 0, .subtypeId = 0x0000000000000000ull, .valid = true},
		{.offset = 0, .size = 1, .type = flow::value::VarType(1) /* bool */, .element = 0, .subtypeId = 0x0000000000000000ull, .valid = true},
		{.offset = 0, .size = 8, .type = flow::value::VarType(2) /* int */, .element = 0, .subtypeId = 0x0000000000000000ull, .valid = true},
		{.offset = 0, .size = 1, .type = flow::value::VarType(1) /* bool */, .element = 0, .subtypeId = 0x0000000000000000ull, .valid = true},
		{.offset = 0, .size = 8, .type = flow::value::VarType(2) /* int */, .element = 0, .subtypeId = 0x0000000000000000ull, .valid = true},
		{.offset = 8, .size = 8, .type = flow::value::VarType(2) /* int */, .element = 0, .subtypeId = 0x0000000000000000ull, .valid = true},
		{.offset = 16, .size = 8, .type = flow::value::VarType(2) /* int */, .element = 0, .subtypeId = 0x0000000000000000ull, .valid = true},
		{.offset = 0, .size = 8, .type = flow::value::VarType(2) /* int */, .element = 0, .subtypeId = 0x0000000000000000ull, .valid = true},
		{.offset = 8, .size = 8, .type = flow::value::VarType(2) /* int */, .element = 0, .subtypeId = 0x0000000000000000ull, .valid = true},
		{.offset = 16, .size = 8, .type = flow::value::VarType(2) /* int */, .element = 0, .subtypeId = 0x0000000000000000ull, .valid = true},
		{.offset = 0, .size = 8, .type = flow::value::VarType(2) /* int */, .element = 0, .subtypeId = 0x0000000000000000ull, .valid = true},
		{.offset = 8, .size = 8, .type = flow::value::VarType(2) /* int */, .element = 0, .subtypeId = 0x0000000000000000ull, .valid = true},
		{.offset = 16, .size = 8, .type = flow::value::VarType(2) /* int */, .element = 0, .subtypeId = 0x0000000000000000ull, .valid = true},
	};
	static constexpr uint32_t recordFieldBeginCount = 17;
	static constexpr uint32_t recordFieldBegin[17] = {
		0,
		0,
		1,
		1,
		1,
		1,
		1,
		2,
		3,
		3,
		3,
		4,
		5,
		5,
		8,
		11,
		11,
	};
	static constexpr uint32_t recordFieldCountCount = 17;
	static constexpr uint32_t recordFieldCount[17] = {
		0,
		1,
		0,
		0,
		0,
		0,
		1,
		1,
		0,
		0,
		1,
		1,
		0,
		3,
		3,
		0,
		3,
	};

	static constexpr flow::CompiledIdentity identity = {
		.name = StringView("synth"),
		.assetHash = 0xe2c8ba2715f61fcfull,
		.textHash = 0x10411c760ac057d4ull,
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

} // namespace stappler::flow::gen::synth_4103

#endif /* GEN_SYNTH_4103_GEN_H_ */
