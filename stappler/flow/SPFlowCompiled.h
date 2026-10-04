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

#ifndef STAPPLER_FLOW_SPFLOWCOMPILED_H_
#define STAPPLER_FLOW_SPFLOWCOMPILED_H_

#include "SPFlowEngine.h"
#include "SPFlowFast.h"
#include "SPFlowStatic.h"

// A graph carried as constants: what a generated unit is, and how it is loaded. The generator
// (stappler_flow_codegen) takes a built graph and writes its shape out as data - the
// nodes with their slices and masks, the edges with their casts, the scopes, the constant table,
// both contracts unbound - together with an identity block saying which operations, which schemas
// and which frame layout the shape was settled against. Nothing in a unit is a decision: every
// number in it was made by RuntimeGraph::build and is copied, so a unit loaded here answers the
// graph policy (SPFlowStatic.h) with the same rows the built graph would. What a unit cannot carry
// is a pointer - an OpDesc, a ComponentType, an extension instance - so it cannot know whether the
// process it is loaded into still has the shapes it was written against. Loading is where that is
// asked: every operation is resolved by name and its signature hash compared, every local schema
// and the store's own bookkeeping type are compared by hash, and the frame layout the unit carries
// is recomputed by the one function the store uses and compared row for row. A unit that has
// drifted is refused with a code that names which of the three files to open, never loaded halfway.
namespace STAPPLER_VERSIONIZED stappler::flow {

class CompiledGraph;

// The two stores a run over a loaded unit can live in, and the site of one unit of work in either.
// Named this early because the tables carry a pointer to code written against them.
template <typename A, typename Env = NoEnv>
using CompiledLocalT = LocalStoreT<A, CompiledGraph, Env>;

template <typename A, typename Env = NoEnv>
using CompiledFastLocalT = FastLocalT<A, CompiledGraph, Env>;

template <typename Local>
using CompiledStepSiteT = StepSite<CompiledGraph, Local>;

// A unit's own way of performing one unit of work: `step<N>` with the node index a constant, the
// door built over the unit's tables (StaticContext below) and the operation called directly where
// it has an inline form. Written by stappler_flow_codegen.
template <typename Local>
using CompiledStepFnT = Status (*)(CompiledStepSiteT<Local> &);

// The unit's compiled steps, one per store a run may live in - because the door reaches the store,
// and another store is another type. Six, and exactly six: the arena store and the fast store, each
// over the three arena kinds a scene is ever in (XSGraph.scu.cpp). In a release build ShadowArena
// is TrackedArena, so the unit writes the same function into two slots of each pair. Compiled for
// one environment (SPFlowEnv.h), whose scene the door reaches: a unit names it, and a run in
// another environment takes none of these and performs its nodes through the registry.
template <typename Env>
struct CompiledStepsT {
	CompiledStepFnT<CompiledLocalT<value::PlainArena, Env>> plain = nullptr;
	CompiledStepFnT<CompiledLocalT<value::TrackedArena, Env>> tracked = nullptr;
	CompiledStepFnT<CompiledLocalT<value::ShadowArena, Env>> shadow = nullptr;

	CompiledStepFnT<CompiledFastLocalT<value::PlainArena, Env>> fastPlain = nullptr;
	CompiledStepFnT<CompiledFastLocalT<value::TrackedArena, Env>> fastTracked = nullptr;
	CompiledStepFnT<CompiledFastLocalT<value::ShadowArena, Env>> fastShadow = nullptr;

	// By the store, which carries the arena kind in it. `Local::ArenaType` is the scene's kind
	// under both stores - the fast one keeps its frames in a PlainArena of its own whatever the
	// scene is - so the two halves are told apart by the store type and not by the arena.
	template <typename Local>
	CompiledStepFnT<Local> get() const {
		using A = typename Local::ArenaType;
		if constexpr (std::is_same_v<Local, CompiledLocalT<A, Env>>) {
			if constexpr (std::is_same_v<A, value::PlainArena>) {
				return plain;
			} else if constexpr (std::is_same_v<A, value::TrackedArena>) {
				return tracked;
			} else {
				return shadow;
			}
		} else {
			if constexpr (std::is_same_v<A, value::PlainArena>) {
				return fastPlain;
			} else if constexpr (std::is_same_v<A, value::TrackedArena>) {
				return fastTracked;
			} else {
				return fastShadow;
			}
		}
	}
};

// One operation the unit was written against: its name, its signature, and the hash of the record
// schema derived from it (0 when it declares none). The operation registry answers the same three
// at load time, and a difference in any of them is `CodegenOpDrift` or `CodegenSchemaDrift`.
struct CompiledOpIdentity {
	StringView name;
	uint64_t signatureHash = 0;
	uint64_t localSchemaHash = 0;

	// What the operation may do in a parallel block. Part of the signature hash as well; stated
	// here so a unit says what class it ran its blocks under.
	OpParallel parallel = OpParallel::Serial;
};

// A parallel block's policy as the unit was written with it: the fan-out's `onFailure` and
// `timeout`, the branch budgets, and the query. Checked on load against the block rows, which the
// machine reads.
struct CompiledBlockIdentity {
	NodeId fanOut = NullNodeId;
	ParallelFailure onFailure = ParallelFailure::CancelFrame;
	uint32_t timeoutMs = 0;
	uint32_t maxSteps = 0;
	uint32_t maxActivations = 0;
	uint64_t withHash = 0;
	uint64_t withoutHash = 0;

	// For a block its author gave to the GPU: the hash of the shader its body lowers to, zero
	// otherwise. Re-lowered on load; a body, a layout or an operation's GLSL that moved since the
	// unit was written is `CodegenLayoutDrift`.
	uint64_t gpuHash = 0;
};

// An enum family a node narrows, widens or names, and the hash its members had when the unit was
// written. A family's members are in no schema hash, so a family changed after the build is caught
// here, when the unit is bound to a scene whose registry has it.
struct CompiledFamilyIdentity {
	NodeId node = NullNodeId;
	StringView name;
	uint64_t hash = 0;
};

// The hash a block's query is identified by: its component ids, in order.
SP_PUBLIC uint64_t hashBlockQuery(SpanView<TypeId>);

// A document the unit's graph took a function's body from (LinkCallee), and the content hash of
// what it said then.
struct CompiledCalleeIdentity {
	StringView name;
	uint64_t contentHash = 0;
};

// The identity block: what makes this unit this graph against these shapes. `assetHash` is
// GraphAsset::getContentHash() of the file the unit was written from, for a host that holds both
// and wants to know whether they are the same graph. `textHash` is the hash of the generated
// header's own text with this field blanked, which is how a committed unit is proved fresh without
// reading it back from disk: regenerate, compare the number. `stateSchemaHash` is
// interp.NodeState's, because the frame layout is a function of it.
struct CompiledIdentity {
	StringView name;
	uint64_t assetHash = 0;
	uint64_t textHash = 0;
	uint64_t stateSchemaHash = 0;
	SpanView<CompiledOpIdentity> ops;
	SpanView<uint32_t> frameBytes; // per scope
	SpanView<FrameSlot> slots; // per node
	SpanView<CompiledBlockIdentity> blocks; // per block
	SpanView<CompiledFamilyIdentity> families;

	// The library documents the graph was linked with, by name.
	SpanView<CompiledCalleeIdentity> callees;
};

// Whether the documents a host links the unit's graph against are the ones the unit was written
// from: CodegenCalleeDrift for every callee that moved, vanished or appeared. A unit of a graph
// with no library functions has none, and checks against a null link.
SP_PUBLIC Status checkUnitCallees(const CompiledIdentity &, const GraphLink *,
		DiagSink *report = nullptr);

template <DiagContainer Out>
Status checkUnitCallees(const CompiledIdentity &identity, const GraphLink *link, Out *report) {
	DiagSinkFor<Out> sink(report);
	return checkUnitCallees(identity, link, sink.get());
}

// One declaration of the asset's `extensions` section, as the unit carries it: the parameters are
// the declaration's `params` value, encoded as CBOR, because a data::Value is not a constant.
struct CompiledExtensionDecl {
	StringView name;
	StringView id;
	BytesView params;
};

// Everything a unit hands the loader: views into the unit's constant storage, which outlives every
// graph loaded from it (it is the program's own text and data). The row types are the built graph's
// own - a generated unit is a RuntimeGraph spelled as constants, and a second set of row types
// would be a second set of fields to keep in step. The pointers in them (`op`, `localSchema`,
// `source`; a binding's `type`, `desc`, `def`, `instance`) are null in the unit and filled in by
// the loader, or never: `source` stays null, because there is no asset.
struct CompiledTables {
	SpanView<RuntimeNode> nodes;
	SpanView<StringView> opNames; // per node
	SpanView<RuntimeDataEdge> dataEdges;
	SpanView<RuntimeExecEdge> execEdges;

	// The index arrays the nodes' slices point into, in the build's order.
	SpanView<uint32_t> dataIn;
	SpanView<uint32_t> dataOut;
	SpanView<uint32_t> execIn;
	SpanView<uint32_t> execOut;
	SpanView<uint32_t> crossScopeIn;
	SpanView<uint32_t> entries;
	SpanView<uint32_t> terminals;

	SpanView<RuntimeScope> scopes;
	SpanView<uint32_t> scopeNodes;

	// The parallel blocks and the slices their rows point into.
	SpanView<RuntimeBlock> blocks;
	SpanView<RuntimeBlockWrite> blockWrites;
	SpanView<uint32_t> blockCollectors;
	SpanView<TypeId> blockQuery;

	// The settings table as one CBOR array, as `constants`: an entry per setting every node's
	// operation declares, resolved, null where it is the type's zero.
	BytesView settings;

	// The constant table as one CBOR array: an entry per constant slot of the build, null where the
	// build stored none. Decoded on load into the very data::Values the built graph held, so that a
	// door reading a literal reads what it read before - the same decoder, the same scratch, the
	// same bytes.
	BytesView constants;

	// Both contracts, unbound.
	SpanView<SceneBinding> sceneContract;
	SpanView<ExtensionBinding> extensionContract;
	SpanView<CompiledExtensionDecl> extensionDecls;

	// The named entities' contract, unbound; empty for a unit that names none.
	SpanView<NamedBinding> namedContract;

	/* The asset the unit was written from, as canonical CBOR, or empty (`--embed-asset`). The
	machine never reads it: it is for tools that need a `RuntimeGraph` beside the static one - the
	narrative, an overlay, a debugger - and those bytes are the ones `GraphAsset::getContentHash` is
	taken over, so a unit that carries an asset carries the asset its identity names. */
	BytesView asset;

	// The unit's compiled steps, or null. Not part of the identity and not hashed: it is code, and
	// what makes code the right code is that it was generated from these tables and compiled beside
	// them. A unit whose steps are null runs through the registry, and `codegen-exact-arena` holds
	// both paths to the same bytes.
	const void *steps = nullptr;

	// The environment the steps were compiled for (Env::Tag).
	uint64_t stepsEnv = 0;

	CompiledIdentity identity;
};

// A graph loaded from a unit: the graph policy, over tables that do not belong to it. Not a
// template - everything a loaded unit does, resolve, check, decode, bind, answer, is the same for
// every unit, and a body per unit would be a body per graph in the program. What is per unit is the
// tables, and StaticGraph<Tables> below is the thin type that names them, so that a later stage can
// read `Tables::nodes[N]` as a compile-time constant where this class reads a row.
class SP_PUBLIC CompiledGraph {
public:
	using Node = RuntimeNode;
	using DataEdge = RuntimeDataEdge;
	using ExecEdge = RuntimeExecEdge;
	using Scope = RuntimeScope;

	virtual ~CompiledGraph();

	CompiledGraph() = default;
	CompiledGraph(const CompiledGraph &) = delete;
	CompiledGraph &operator=(const CompiledGraph &) = delete;

	/* Loads the unit against the registry it will run with: resolves every operation by name,
	checks the identity block, decodes the constants, lays both contracts out unbound. Reports
	through the graph's diagnostic vocabulary; on any error nothing is loaded - isValid() stays
	false - rather than a graph that is right in the parts that happened to check. The registry
	must already hold the interpreter's core types (registerCoreTypes), because the frame layout
	is a function of interp.NodeState and the check recomputes it, and it must outlive the graph
	exactly as it must outlive a built one. The two longer forms bind as well, which is what a
	host loading a unit to run it wants: the scene's registry and, when the graph names
	extensions, the holder. A unit that names an extension and is loaded without a holder stays
	unbound there and refuses to run such a node, exactly as a built graph does. */
	Status load(const CompiledTables &, const OpRegistry &, DiagSink *report = nullptr);
	Status load(const CompiledTables &, const OpRegistry &, const value::TypeRegistry &scene,
			DiagSink *report = nullptr);
	Status load(const CompiledTables &, const OpRegistry &, const value::TypeRegistry &scene,
			const value::ExtensionHost &, DiagSink *report = nullptr);

	template <DiagContainer Out>
	Status load(const CompiledTables &tables, const OpRegistry &ops, Out *report) {
		DiagSinkFor<Out> sink(report);
		return load(tables, ops, sink.get());
	}
	template <DiagContainer Out>
	Status load(const CompiledTables &tables, const OpRegistry &ops,
			const value::TypeRegistry &scene, Out *report) {
		DiagSinkFor<Out> sink(report);
		return load(tables, ops, scene, sink.get());
	}
	template <DiagContainer Out>
	Status load(const CompiledTables &tables, const OpRegistry &ops,
			const value::TypeRegistry &scene, const value::ExtensionHost &extensions, Out *report) {
		DiagSinkFor<Out> sink(report);
		return load(tables, ops, scene, extensions, sink.get());
	}

	// The same two gates a built graph has, through the same binders (bindSceneBindings,
	// bindExtensionBindings), with the same rule: both halves or neither.
	Status bindScene(const value::TypeRegistry &, DiagSink *report = nullptr);
	Status bindScene(const value::TypeRegistry &, const value::ExtensionHost &,
			DiagSink *report = nullptr);

	template <DiagContainer Out>
	Status bindScene(const value::TypeRegistry &scene, Out *report) {
		DiagSinkFor<Out> sink(report);
		return bindScene(scene, sink.get());
	}
	template <DiagContainer Out>
	Status bindScene(const value::TypeRegistry &scene, const value::ExtensionHost &extensions,
			Out *report) {
		DiagSinkFor<Out> sink(report);
		return bindScene(scene, extensions, sink.get());
	}

	// The named entities, bound against the project's table through the same binder a built graph
	// uses (bindNamedBindings).
	Status bindNamed(const value::NamedHost &, DiagSink *report = nullptr);

	template <DiagContainer Out>
	Status bindNamed(const value::NamedHost &table, Out *report) {
		DiagSinkFor<Out> sink(report);
		return bindNamed(table, sink.get());
	}
	bool isNamedBound() const { return _namedBound; }

	// Forgets the load. The tables are the unit's and are not touched.
	void reset();

	bool isValid() const { return _loaded; }

	uint32_t getNodeCount() const { return uint32_t(_nodes.size()); }
	const RuntimeNode &getNodeAt(uint32_t index) const { return _nodes[index]; }
	uint32_t findNode(NodeId) const;

	SpanView<RuntimeDataEdge> getDataEdges() const { return _tables.dataEdges; }
	SpanView<RuntimeExecEdge> getExecEdges() const { return _tables.execEdges; }

	SpanView<uint32_t> getCrossScopeInEdges(uint32_t node) const;
	SpanView<uint32_t> getDataInEdges(uint32_t node) const;
	SpanView<uint32_t> getDataOutEdges(uint32_t node) const;
	SpanView<uint32_t> getExecInEdges(uint32_t node) const;
	SpanView<uint32_t> getExecOutEdges(uint32_t node) const;

	SpanView<uint32_t> getEntryNodes() const { return _tables.entries; }
	SpanView<uint32_t> getTerminalNodes() const { return _tables.terminals; }

	uint32_t getScopeCount() const { return uint32_t(_tables.scopes.size()); }
	const RuntimeScope &getScopeAt(uint32_t index) const { return _tables.scopes[index]; }
	SpanView<uint32_t> getScopeNodes(uint32_t scope) const;
	bool isScopeWithin(uint32_t scope, uint32_t ancestor) const;

	const mem_std::Value *getConstant(uint32_t node, uint32_t pin) const;

	const mem_std::Value *getSetting(uint32_t node, uint32_t index) const;

	uint32_t getBlockCount() const { return uint32_t(_tables.blocks.size()); }
	const RuntimeBlock &getBlockAt(uint32_t index) const { return _tables.blocks[index]; }
	SpanView<RuntimeBlockWrite> getBlockWrites(uint32_t block) const;
	SpanView<uint32_t> getBlockCollectors(uint32_t block) const;
	SpanView<TypeId> getBlockQuery(uint32_t block, bool without) const;

	// The host attaches the SPIR-V it built from the text this graph's GPU blocks lower to; without
	// it a block runs on the CPU, because a run carries no GLSL compiler. Attach while no run holds
	// this graph: a launch reads the table. SPFlowGpu.cc.
	uint32_t attachGpuShaders(SpanView<GpuShaderBinary>);
	const GpuShaderTable *getGpuShaders() const { return _gpuShaders; }
	const GpuBlockShaders *getGpuBlock(uint32_t block) const;

	SpanView<SceneBinding> getSceneBindings(uint32_t node) const;
	SpanView<SceneBinding> getSceneContract(uint32_t node) const;
	SpanView<ExtensionBinding> getExtensionBindings(uint32_t node) const;
	SpanView<ExtensionBinding> getExtensionContract(uint32_t node) const;
	SpanView<const ExtensionDecl *> getExtensionDecls() const { return _extDecls; }
	SpanView<NamedBinding> getNamedBindings(uint32_t node) const;
	SpanView<NamedBinding> getNamedContract(uint32_t node) const;

	const value::ExtensionHost *getExtensions() const { return _extensions; }
	uint32_t getExtensionsEpoch() const { return _extensionsEpoch; }
	const value::TypeRegistry *getSceneRegistry() const { return _sceneRegistry; }
	uint32_t getSceneRegistryCount() const { return _sceneRegistryCount; }

	const CompiledIdentity &getIdentity() const { return _tables.identity; }
	const CompiledTables &getTables() const { return _tables; }

	// The asset the unit was written from, as canonical CBOR, or empty. `data::read` turns it back
	// into the value a GraphAsset loads; see CompiledTables::asset.
	BytesView getEmbeddedAsset() const { return _tables.asset; }

	// The unit's compiled steps, and the one of them a run in this store would take. Null when the
	// unit carries none, which is the machine's cue to perform the node itself
	// (MachineT::invokeAt). A graph that was never loaded answers null: a step function without the
	// rows it was written for has nothing to read.
	template <typename Env>
	const CompiledStepsT<Env> *getSteps() const {
		return (_loaded && _tables.stepsEnv == Env::Tag)
				? static_cast<const CompiledStepsT<Env> *>(_tables.steps)
				: nullptr;
	}

	template <typename Local>
	CompiledStepFnT<Local> compiledStep() const;

	// The same descriptions a built graph gives, through the same describers - so that "the unit
	// describes the graph it was written from" compares two tables and not two describers.
	void describe(mem_std::Value &) const;
	void describeSceneContract(mem_std::Value &) const;
	void describeExtensionContract(mem_std::Value &) const;

private:
	GpuShaderTable *_gpuShaders = nullptr;

	// The one body every load() overload runs. Null scene and holder mean "not bound".
	Status loadImpl(const CompiledTables &, const OpRegistry &, const value::TypeRegistry *,
			const value::ExtensionHost *, DiagSink *);

	// The identity block against the live registry: three checks, three codes.
	void verify(const OpRegistry &, DiagReport &);

	// The block policies of the identity against the block rows.
	void verifyBlocks(DiagReport &);

	// The families of the identity against a scene's registry.
	void verifyFamilies(const value::TypeRegistry &, DiagReport &);

	// Whether every slice and every index in the tables lands inside them. A unit is program data
	// and a wrong one is a bug in the generator, not a hostile file - but a bug that reads past an
	// array is the worst kind, so the shape is checked once before anything follows a slice.
	bool checkShape(DiagReport &) const;

	void unbindAll();

	CompiledTables _tables;
	bool _loaded = false;

	mem_std::Vector<RuntimeNode> _nodes;
	mem_std::Vector<mem_std::Value> _constants;
	mem_std::Vector<mem_std::Value> _settings;
	mem_std::Vector<SceneBinding> _sceneBindings;
	mem_std::Vector<ExtensionBinding> _extBindings;
	mem_std::Vector<NamedBinding> _namedBindings;
	bool _namedBound = false;

	// The declarations, decoded, and the pointer list the shared binder takes.
	mem_std::Vector<ExtensionDecl> _extDeclStorage;
	mem_std::Vector<const ExtensionDecl *> _extDecls;

	const value::TypeRegistry *_sceneRegistry = nullptr;
	uint32_t _sceneRegistryCount = 0;
	const value::ExtensionHost *_extensions = nullptr;
	uint32_t _extensionsEpoch = 0;
};

static_assert(IsGraphPolicy<CompiledGraph>, "a loaded unit answers what the machine asks");

// The type a generated unit names: the loaded graph over its own tables. `Tables` is the unit's own
// struct (`gen::<name>::Tables`), whose static members are the constant arrays and whose `tables()`
// hands them to the loader. Everything this adds is the name - which is exactly what a later stage
// needs, because a template over the tables can read a row as a compile-time constant where
// CompiledGraph reads it through a span.
template <typename Tables>
class StaticGraph final : public CompiledGraph {
public:
	using TablesType = Tables;

	Status load(const OpRegistry &ops, DiagSink *report = nullptr) {
		return CompiledGraph::load(Tables::tables(), ops, report);
	}
	Status load(const OpRegistry &ops, const value::TypeRegistry &scene,
			DiagSink *report = nullptr) {
		return CompiledGraph::load(Tables::tables(), ops, scene, report);
	}
	Status load(const OpRegistry &ops, const value::TypeRegistry &scene,
			const value::ExtensionHost &extensions, DiagSink *report = nullptr) {
		return CompiledGraph::load(Tables::tables(), ops, scene, extensions, report);
	}

	template <DiagContainer Out>
	Status load(const OpRegistry &ops, Out *report) {
		DiagSinkFor<Out> sink(report);
		return load(ops, sink.get());
	}
	template <DiagContainer Out>
	Status load(const OpRegistry &ops, const value::TypeRegistry &scene, Out *report) {
		DiagSinkFor<Out> sink(report);
		return load(ops, scene, sink.get());
	}
	template <DiagContainer Out>
	Status load(const OpRegistry &ops, const value::TypeRegistry &scene,
			const value::ExtensionHost &extensions, Out *report) {
		DiagSinkFor<Out> sink(report);
		return load(ops, scene, extensions, sink.get());
	}
};

// Running a unit in the arena mode: the same machine the interpreter is, over the loaded unit and
// the arena store. Not a second machine and not a per-unit one - MachineT is instantiated once per
// arena kind over CompiledGraph (XSGraph.scu.cpp), and a StaticGraph<Tables> is passed to it as the
// CompiledGraph it is. That is what makes the arena mode exact by construction - one propagate, one
// close of a turn, one door, one store, and only the rows come from somewhere else - and what
// `codegen-exact-arena` measures against the interpreter at level B: both arenas byte for byte
// after every unit, under every quantum, across a cut in either direction. The door is ContextT
// over the same pair, and which of its two sites a node is performed through - the machine's own,
// or the unit's compiled step below - changes nothing else, which is why the two are byte for byte
// the same run.

template <typename A, typename Env = NoEnv>
using CompiledContextT = ContextT<CompiledGraph, CompiledLocalT<A, Env>>;

template <typename Local>
CompiledStepFnT<Local> CompiledGraph::compiledStep() const {
	auto steps = getSteps<typename Local::EnvType>();
	return steps ? steps->template get<Local>() : nullptr;
}

// The compiled step: a door that knows which node it is. The same door with one thing decided
// earlier - `N` is the node's index as a constant and `Tables` is the unit's own struct, so the
// questions a door asks about the node it is performing (which edge feeds this pin, where this
// node's state sits in the frame, where its record sits, what shape a field of the record has) are
// answered by the compiler off constant arrays rather than by walking rows at run time. Every rule
// that uses those answers is unchanged and unduplicated: it is ContextT's, written once
//. What is still not folded and does not pretend to be: an operation's
// descriptor, a scene binding and the decoded constants, all resolved when the unit is loaded, and
// the door reads them exactly as the interpreter's does.
template <typename Tables, uint32_t N>
struct StaticSite {
	template <typename Graph>
	static const typename Graph::DataEdge *incoming(const Graph &, uint32_t, uint32_t pin) {
		// The same scan in the same order as DynamicSite's, over the same rows - a loaded unit's
		// getDataEdges() is this array - so the edge found is the edge found. What changes is that
		// the slice is a constant and, where an operation spells the pin as a literal, so is the
		// answer.
		constexpr auto &node = Tables::nodes[N];
		for (uint32_t i = 0; i < node.dataInCount; ++i) {
			auto &edge = Tables::dataEdges[Tables::dataIn[node.dataInBegin + i]];
			if (edge.dstPin == pin) {
				return &edge;
			}
		}
		return nullptr;
	}

	// The frame layout the unit carries, which the loader has already compared row for row against
	// the one this process computes: equal, or the unit did not load.
	template <typename Local>
	static Addr recordIn(const Local &, Addr frame, uint32_t) {
		if constexpr (Tables::slots[N].recordOffset == InvalidIndex) {
			return NullAddr;
		} else {
			return frame == NullAddr ? NullAddr : frame + Tables::slots[N].recordOffset;
		}
	}

	template <typename Local>
	static Addr stateIn(const Local &, Addr frame, uint32_t) {
		return frame == NullAddr ? NullAddr : frame + Tables::slots[N].stateOffset;
	}

	// The fourth question: the shape of a field of a node's record, off the unit's own table, so
	// the offset, the size and above all the type are constants - and with the type a constant,
	// `value::decodeField`'s switch folds to the one case that field can ever be. The node here is
	// not always N: a door reads its own record and also the record of whichever node feeds a pin,
	// but that node comes out of `incoming`, which is itself constant, so both indices reach the
	// tables as constants and the whole lookup goes away. Offsets are facts about the process and
	// not about the graph, and what makes folding them sound is that a schema whose fields have
	// moved has a different hash: `computeSchemaHash` hashes every field's offset, and
	// `CompiledGraph::verify` refuses such a unit at load with `CodegenSchemaDrift`.
	template <typename Graph>
	static FieldShape fieldAt(const Graph &, uint32_t node, uint32_t index) {
		if (node >= Tables::nodesCount || index >= Tables::recordFieldCount[node]) {
			return FieldShape();
		}
		return Tables::recordFields[Tables::recordFieldBegin[node] + index];
	}
};

// What a generated `step<N>` fills and hands to the operation. Still an OpContext, so an operation
// with no inline form is called through the registry against this door and not another one - which
// is what makes the two paths comparable at all. Over the store and not over the arena kind,
// because a unit's step serves both stores: the same site, the same rules, the same constants, and
// only where the run's bookkeeping lives differs.
template <typename Tables, uint32_t N, typename Local>
using StaticContext = ContextT<CompiledGraph, Local, StaticSite<Tables, N>>;

template <typename A, typename Env = NoEnv, typename Trace = TraceLog>
using CompiledRunT = MachineT<CompiledGraph, CompiledLocalT<A, Env>, Trace>;

// "arena" is what a run over a unit names itself, whichever trace policy it logs under.
template <typename A, typename Env, typename Trace>
struct RunEngineName<CompiledRunT<A, Env, Trace>> {
	static constexpr StringView Value = StringView("arena");
};

template <typename A, typename Env = NoEnv>
using CompiledEngineT = MachineEngineT<CompiledRunT<A, Env, TraceLog>>;

// Running a unit in the fast mode: the same machine again, over the same rows, with the store
// swapped - `FastLocalT` keeps the frames in a scratch arena of its own and the run's bookkeeping
// in host memory (SPFlowFast.h). What it gives up is what a shipped game does not use, resuming,
// an image, undoing a unit; what it keeps is everything a run computes, which is why the contract
// here is level A and not level B. `Trace` is the second half of the trade: `TraceLog` writes the
// execution log a test reads, `TraceNone` writes none at all. Diagnostics are written under both -
// a refusal without an explanation is not an economy.

template <typename A, typename Env = NoEnv, typename Trace = TraceLog>
using CompiledFastRunT = MachineT<CompiledGraph, CompiledFastLocalT<A, Env>, Trace>;

// "fast" is what a run over the fast store names itself, whichever trace policy it logs under.
template <typename A, typename Env, typename Trace>
struct RunEngineName<CompiledFastRunT<A, Env, Trace>> {
	static constexpr StringView Value = StringView("fast");
};

template <typename A, typename Env = NoEnv>
using CompiledFastEngineT = MachineEngineT<CompiledFastRunT<A, Env, TraceLog>>;

// The same engine with the log turned off: what a shipped game holds. Named here rather than left
// to a caller because the machine's bodies are instantiated once, in the module, and a host that
// spelled its own combination would compile a second machine into itself.
template <typename A, typename Env = NoEnv>
using CompiledQuietEngineT = MachineEngineT<CompiledFastRunT<A, Env, TraceNone>>;

} // namespace stappler::flow

#endif /* STAPPLER_FLOW_SPFLOWCOMPILED_H_ */
