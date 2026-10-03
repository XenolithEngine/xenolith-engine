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

#ifndef STAPPLER_FLOW_SPFLOWRUNTIME_H_
#define STAPPLER_FLOW_SPFLOWRUNTIME_H_

#include "SPFlowAsset.h"
#include "SPFlowOp.h"

namespace STAPPLER_VERSIONIZED stappler::flow {

static constexpr uint32_t InvalidIndex = 0xffff'ffffu;

// One thing a node names in the scene, resolved: the build read the names off the node's constants
// and off the asset's declaration, the bind resolved them against the scene's registry, and from
// there the interpreter holds a descriptor rather than a string. The type decision was made
// once, here. `type` and `desc` are null in two different situations and
// the flags say which: `dynamic`, meaning the name arrives on an edge, so there was never anything
// to resolve and the operation keeps the old path; or `optional` and the scene simply does not have
// the type, which is a state and not a failure - `scene.has` answers no about it and
// `scene.removeComponent` has nothing to remove, both without touching the registry. A binding with
// neither flag and no `type` cannot happen: the build refuses such a graph.
struct SceneBinding {
	// Views into the asset's pool, which outlives the graph. Diagnostics and the dump read them;
	// nothing on the run path does.
	StringView component;
	StringView field;

	TypeId componentId = value::NullTypeId;

	// What the graph says the field's type is - from the asset's declaration, or failing that from
	// the value pin. Checked against the scene's own answer at bind time.
	VarType declared = VarType::Nil;

	const value::ComponentType *type = nullptr;
	const value::FieldDesc *desc = nullptr;

	bool optional = false;
	bool dynamic = false;
};

// One extension instance a node names, resolved. The same shape as SceneBinding and for the same
// reason: the build read the id off the node's constants, checked it against the asset's
// declaration, and the bind resolved it against the live instances, so the interpreter holds a
// pointer rather than a name. `instance` is null in three situations and the
// flags say which: `dynamic`, the id arrives on an edge, and an operation that sees one has no
// fallback, because what stands behind an id is a host object and not a number; the graph was never
// bound to a scene, and then the whole slice is empty rather than holding unresolved entries; or
// the bind failed, and then there is no graph to run - `bindScene` leaves a graph unbound rather
// than half-bound.
struct ExtensionBinding {
	// Views into the asset's pool, which outlives the graph. Diagnostics and the dump read them.
	StringView id;
	StringView name;

	value::ExtensionInstance *instance = nullptr;

	bool dynamic = false;
};

// One named entity a node names (PinRole::EntityName), resolved when the graph is bound to the
// project's table: the entity's id is a constant of the project, so a bound node holds it and looks
// nothing up. `dynamic` is a name that arrives on an edge, which nothing can resolve.
struct NamedBinding {
	StringView name; // a view into the asset's pool
	value::EntityId entity;
	bool dynamic = false;
};

// A node with its operation resolved. Everything addressed by index from here on: the id survives
// for diagnostics and for the local store's keying, but nothing walks the graph by it.
struct RuntimeNode {
	NodeId id = NullNodeId;
	const GraphNode *source = nullptr; // the asset record, for params and the editor's meta
	const OpDesc *op = nullptr;
	const value::ComponentType *localSchema = nullptr;

	// Slices into the graph's shared index arrays.
	uint32_t dataInBegin = 0, dataInCount = 0;
	uint32_t dataOutBegin = 0, dataOutCount = 0;
	uint32_t execInBegin = 0, execInCount = 0;
	uint32_t execOutBegin = 0, execOutCount = 0;

	// Where this node's constants start in the constant array; one entry per data input of the
	// operation, null for an input that an edge feeds.
	uint32_t constantBegin = 0;

	// This node's slice of the graph's scene bindings: one per scene group of its operation, in the
	// signature's group order, always - a group whose name comes on an edge is a binding marked
	// `dynamic`, never a gap. That is what lets an operation read its own group by an index it
	// writes as a literal, and it is why the bindings are not deduplicated across nodes: three
	// nodes naming game.Board.row are three entries, because a shared table has no contiguous
	// per-node slice. Empty for a node whose operation names nothing in the scene, and for every
	// node of a graph that was built without one.
	uint32_t sceneBegin = 0, sceneCount = 0;

	// The same, for the extension instances the operation names - one per extension group, in the
	// signature's group order. A separate slice from the scene's because the two lists are indexed
	// separately by the operation.
	uint32_t extBegin = 0, extCount = 0;

	// The named entities it names, one per EntityName pin, in pin order.
	uint32_t namedBegin = 0, namedCount = 0;

	// Which loop body this node belongs to; 0 is the graph itself. Decided by the build, so that
	// the interpreter never works out at run time which nodes an iteration re-runs.
	uint32_t scope = 0;

	// The scope this node opens, or InvalidIndex. At most one, because the value a loop hands its
	// body is per-iteration and could not belong to two bodies at once.
	uint32_t opensScope = InvalidIndex;

	// Where this node sits in its own scope's node list. The interpreter addresses a record by
	// (activation, this) - the activation says where its block of records begins and this says
	// which one inside it - so a record is found by arithmetic rather than by searching for a key.
	// A build-time fact for the same reason `scope` is: which nodes an iteration consists of is a
	// property of the graph, and the run makes no structural decisions.
	uint32_t slotInScope = 0;

	// Readiness, as far as it can be decided statically. `sameScopeInputs` is the set of input pins
	// fed from this node's own scope; those are marked on the node's record by the producer when it
	// runs, so "are they all there" is one AND against the record's `inputs` mask, with no walk
	// over the edges. The rest is `crossScopeIn`: inputs fed from an enclosing scope, which cannot
	// be pushed - the consumer may not exist when the producer runs, and later there may be many of
	// it - and so are read off the producer where it stands; that slice is empty for almost every
	// node. `allInputs` is the union, which is what a stall report wants: every pin an edge feeds.
	// 32 bits is enough by construction, not by luck: MaxDataPins is 32 and validation refuses a
	// wider signature at registration.
	uint32_t sameScopeInputs = 0;
	uint32_t allInputs = 0;
	uint32_t crossScopeInBegin = 0, crossScopeInCount = 0;

	// No incoming edges and no exec input: the interpreter starts here. A node that has an exec
	// input and no incoming exec edge is not an entry - it can never fire, and validation says so.
	bool isEntry = false;
	bool isTerminal = false;

	// The enum family this node's EnumFamily outputs carry (makeTypeId of the name), or NullTypeId.
	value::TypeId family = value::NullTypeId;
	StringView familyName;

	// Where this node's settings start in the graph's setting table: one entry per setting its
	// operation declares, resolved (the default when the node names none).
	uint32_t settingBegin = 0;

	// The level of the front this node is pushed on: 1 for a node that leads to a parallel fan-out
	// in its own activation, when lifting it changes no result; 0 otherwise.
	uint8_t weight = 0;
};

// How many levels the front has: weight 0 and weight 1.
static constexpr uint32_t MaxFrontLevels = 2;

// The subtype a node's record field reads back with: an EnumFamily output takes the node's family.
inline value::TypeId nodeFieldSubtype(const RuntimeNode &node, uint32_t index,
		value::TypeId schemaSubtype) {
	if (node.family != value::NullTypeId && node.op && index < node.op->getDataOut().size()
			&& carriesNodeFamily(*node.op, node.op->getDataOut()[index])) {
		return node.family;
	}
	return schemaSubtype;
}

// A loop body, statically. Scope 0 is the graph itself and has no opener; every other one is the
// set of nodes reached through an exec output its operation declared as opening a scope. Static,
// and dynamic is the activation: this says which nodes an iteration consists of, the run says how
// many iterations there were. Keeping the two apart is what lets a node's record be addressed by
// (node, activation) without anybody searching for anything.
struct RuntimeScope {
	uint32_t opener = InvalidIndex; // node index; InvalidIndex for the root scope
	uint32_t execPin = InvalidIndex;
	uint32_t parent = InvalidIndex;
	uint32_t depth = 0;

	// Slice into the graph's scope-node array, in ascending node index.
	uint32_t nodeBegin = 0, nodeCount = 0;

	// A loop's turns or a parallel block's branches (the opener's OpDef::scopeKind).
	ScopeKind kind = ScopeKind::Loop;

	// For a parallel scope: the barrier node in the parent scope the body closes into, or
	// InvalidIndex.
	uint32_t barrier = InvalidIndex;

	// The parallel scope this one is or lies within, or InvalidIndex: which branch a step belongs
	// to.
	uint32_t branchScope = InvalidIndex;

	// For a parallel scope: its block (RuntimeGraph::getBlockAt), and the bytes of the branch
	// header at the start of every branch frame. Zero for every other scope, so their frames do not
	// move.
	uint32_t block = InvalidIndex;
	uint32_t headerBytes = 0;
};

// What a parallel block does with its branches when one fails (the fan-out's `onFailure`).
enum class ParallelFailure : uint8_t {
	CancelFrame,
	Partial,
	Nothing,
};

SP_PUBLIC StringView getParallelFailureName(ParallelFailure);

// The header every branch frame starts with. Offsets are fixed; the row copies follow at the
// offsets RuntimeBlockWrite names.
struct BranchHeader {
	static constexpr uint32_t StatusOffset = 0; // Int32: BranchFailed | BranchClosed
	static constexpr uint32_t StepsOffset = 4; // Int32: units of work this branch took
	static constexpr uint32_t ActivationsOffset = 8; // Int32: activations its loops opened
	static constexpr uint32_t EntityOffset = 16; // UInt64: EntityId::pack() of the branch's entity
	static constexpr uint32_t CopiesOffset = 24;

	static constexpr int32_t Failed = 1 << 0;
	static constexpr int32_t Closed = 1 << 1;
};

// The locals a fan-out keeps (OpDef::locals of `par.forEach` and `par.forEachWith`): the set it
// runs over, the first branch activation, how many branches have closed, and where the block is.
struct FanOutLocal {
	static constexpr uint32_t Entities = 0; // Array<EntityRef>
	static constexpr uint32_t First = 1; // Int
	static constexpr uint32_t Closed = 2; // Int
	static constexpr uint32_t Phase = 3; // Int: FanOutPhase

	static constexpr int64_t Idle = 0;
	static constexpr int64_t InFlight = 1;
	static constexpr int64_t Ready = 2; // every branch closed, not delivered
	static constexpr int64_t Delivered = 3;
	static constexpr int64_t Launched = 4; // the branches are with an executor that runs them
};

// One field a block's branches write on their own entity: the copy every branch keeps of it, in its
// header, and what a delivery commits.
struct RuntimeBlockWrite {
	TypeId componentId = value::NullTypeId;
	StringView component;
	StringView field;
	VarType type = VarType::Nil;
	TypeId subtypeId = value::NullTypeId;
	uint32_t flagOffset = 0; // a byte: the branch wrote it
	uint32_t valueOffset = 0; // the value, encoded as a field of `type`
};

// A parallel block, statically: its scope, its three kinds of node, its policy and its budgets, and
// the slices of what its branches write, which collectors read it, and what its query names.
struct RuntimeBlock {
	uint32_t scope = InvalidIndex;
	uint32_t fanOut = InvalidIndex;
	uint32_t barrier = InvalidIndex;
	uint32_t entityPin = InvalidIndex;
	uint32_t indexPin = InvalidIndex;
	ParallelFailure onFailure = ParallelFailure::CancelFrame;

	// A branch's own limits, derived from the body as the run's are from the graph.
	uint32_t maxSteps = 0;
	uint32_t maxActivations = 0;

	// The fan-out's `timeout`, in milliseconds; 0 is none. Read only under an executor.
	uint32_t timeoutMs = 0;

	uint32_t writeBegin = 0, writeCount = 0;
	uint32_t collectorBegin = 0, collectorCount = 0;
	uint32_t withBegin = 0, withCount = 0;
	uint32_t withoutBegin = 0, withoutCount = 0;
};

struct RuntimeDataEdge {
	uint32_t srcNode = InvalidIndex, srcPin = InvalidIndex;
	uint32_t dstNode = InvalidIndex, dstPin = InvalidIndex;

	// Decided here, once, and never reconsidered: the interpreter carries the value across this
	// edge by applying this rule and never consults the conversion matrix. Same means the
	// bytes are already what the destination wants.
	value::CastRule cast = value::CastRule::Same;

	// A container's transfer is a deep copy of its block, so it needs the local store's arena even
	// when the rule is Same. Precomputed for the same reason as the rule.
	bool needsArena = false;
};

struct RuntimeExecEdge {
	uint32_t srcNode = InvalidIndex, srcPin = InvalidIndex;
	uint32_t dstNode = InvalidIndex;

	// The edge closes a loop: its target was on the stack of the walk that reached it. Layer L3
	// opens a loop activation here. Computed during the build because that is where the traversal
	// order is canonical, and a back edge found in a different order would be a different edge.
	bool backEdge = false;
};

// What a build found the regions of a graph to be, in the asset's node ids - for an editor, which
// draws a block's frame from it. Taken right after the scopes are assigned, before a build with
// errors throws its scopes away, so a graph that fails for a reason past that point (a conflict in
// a block, say) still has a shape. `produced` is false when the build did not get that far.
struct GraphShape {
	struct Region {
		NodeId opener = 0;
		NodeId barrier = 0; // 0 for a loop, and for a block whose barrier was not found
		ScopeKind kind = ScopeKind::Loop;
		mem_std::Vector<NodeId> nodes; // every node inside, nested regions included, in node order
	};

	bool produced = false;
	mem_std::Vector<Region> regions;
};

// The executable form of a graph: names resolved, order canonical, types settled. Immutable after
// build(): editing a graph means editing the asset and building again, and hot swapping one means
// replacing the whole object - there is no incremental path on purpose.
class SP_PUBLIC RuntimeGraph final {
public:
	// The names the machine reads a graph through (SPFlowStatic.h): a policy names its own row
	// types, so that a second representation of a graph - the static tables a generated unit
	// carries - can answer the same questions with rows of its own shape.
	using Node = RuntimeNode;
	using DataEdge = RuntimeDataEdge;
	using ExecEdge = RuntimeExecEdge;
	using Scope = RuntimeScope;

	~RuntimeGraph();

	RuntimeGraph() = default;
	RuntimeGraph(const RuntimeGraph &) = delete;
	RuntimeGraph &operator=(const RuntimeGraph &) = delete;

	bool init(memory::pool_t *parent = nullptr);

	// Validates and builds. After this returns Ok nothing else can refuse the graph for a reason of
	// substance - only for want of memory. The report carries warnings even on success, and
	// a caller that wants them must pass a sink. The asset must outlive the graph, because a
	// RuntimeNode points at its GraphNode for the parameters and the editor's meta; so must the
	// operation registry.
	Status build(const GraphAsset &, const OpRegistry &, DiagSink *report = nullptr);

	template <DiagContainer Out>
	Status build(const GraphAsset &asset, const OpRegistry &ops, Out *report) {
		DiagSinkFor<Out> sink(report);
		return build(asset, ops, sink.get());
	}

	// Builds, and checks the graph against the scene it will run on. The contract is declared by
	// the signatures (which pin names what) and by the asset (which component, which field), both
	// already in the file; it is checked here, the one moment both halves are in the same room. A
	// graph that names a component the scene does not have, or a field of the wrong type, does not
	// build. The scene registry must outlive the graph, exactly as the operation registry must.
	Status build(const GraphAsset &, const OpRegistry &, const value::TypeRegistry &scene,
			DiagSink *report = nullptr);

	template <DiagContainer Out>
	Status build(const GraphAsset &asset, const OpRegistry &ops, const value::TypeRegistry &scene,
			Out *report) {
		DiagSinkFor<Out> sink(report);
		return build(asset, ops, scene, sink.get());
	}

	// The same, plus the scene's extensions. A separate overload rather than a defaulted parameter
	// on the one above, so that "this host has no extensions" and "this host forgot to pass them"
	// cannot be spelled alike: a graph that names an extension and is built without a holder is
	// refused.
	Status build(const GraphAsset &, const OpRegistry &, const value::TypeRegistry &scene,
			const value::ExtensionHost &, DiagSink *report = nullptr);

	template <DiagContainer Out>
	Status build(const GraphAsset &asset, const OpRegistry &ops, const value::TypeRegistry &scene,
			const value::ExtensionHost &extensions, Out *report) {
		DiagSinkFor<Out> sink(report);
		return build(asset, ops, scene, extensions, sink.get());
	}

	// The same rules with nothing built - what an editor calls on every keystroke. It is the same
	// function underneath, not a second implementation that would drift.
	static Status validate(const GraphAsset &, const OpRegistry &, DiagSink *report = nullptr);
	static Status validate(const GraphAsset &, const OpRegistry &, const value::TypeRegistry &scene,
			DiagSink *report = nullptr);
	static Status validate(const GraphAsset &, const OpRegistry &, const value::TypeRegistry &scene,
			const value::ExtensionHost &, DiagSink *report = nullptr);

	// The same, and the regions the build assigned (GraphShape) - what the graph editor calls.
	static Status validate(const GraphAsset &, const OpRegistry &, DiagSink *report,
			GraphShape *shape);

	template <DiagContainer Out>
	static Status validate(const GraphAsset &asset, const OpRegistry &ops, Out *report) {
		DiagSinkFor<Out> sink(report);
		return validate(asset, ops, sink.get());
	}
	template <DiagContainer Out>
	static Status validate(const GraphAsset &asset, const OpRegistry &ops,
			const value::TypeRegistry &scene, Out *report) {
		DiagSinkFor<Out> sink(report);
		return validate(asset, ops, scene, sink.get());
	}
	template <DiagContainer Out>
	static Status validate(const GraphAsset &asset, const OpRegistry &ops,
			const value::TypeRegistry &scene, const value::ExtensionHost &extensions, Out *report) {
		DiagSinkFor<Out> sink(report);
		return validate(asset, ops, scene, extensions, sink.get());
	}
	template <DiagContainer Out>
	static Status validate(const GraphAsset &asset, const OpRegistry &ops, Out *report,
			GraphShape *shape) {
		DiagSinkFor<Out> sink(report);
		return validate(asset, ops, sink.get(), shape);
	}

	// Re-checks an already built graph against a scene - what a host does after loading a scene
	// that brought types of its own. Repeatable, and it never un-builds the graph: a graph is code
	// and a scene is state, so a scene that does not fit is the scene's problem and the graph is
	// still the graph. A failed bind leaves the graph unbound rather than half-bound, which is the
	// same rule the build follows over the larger thing it owns.
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

	bool isValid() const { return _built; }

	uint32_t getNodeCount() const { return uint32_t(_nodes.size()); }
	const RuntimeNode &getNodeAt(uint32_t index) const { return _nodes[index]; }
	uint32_t findNode(NodeId) const; // InvalidIndex when absent

	SpanView<RuntimeDataEdge> getDataEdges() const { return _dataEdges; }
	SpanView<RuntimeExecEdge> getExecEdges() const { return _execEdges; }

	// Data-in edges whose producer is in an enclosing scope - the ones readiness cannot decide from
	// the node's own record. Indices into the edge arrays above, in canonical order, and almost
	// always empty.
	SpanView<uint32_t> getCrossScopeInEdges(uint32_t node) const;

	SpanView<uint32_t> getDataInEdges(uint32_t node) const;
	SpanView<uint32_t> getDataOutEdges(uint32_t node) const;
	SpanView<uint32_t> getExecInEdges(uint32_t node) const;
	SpanView<uint32_t> getExecOutEdges(uint32_t node) const;

	SpanView<uint32_t> getEntryNodes() const { return _entryNodes; }
	SpanView<uint32_t> getTerminalNodes() const { return _terminalNodes; }

	// Scopes.

	uint32_t getScopeCount() const { return uint32_t(_scopes.size()); }
	const RuntimeScope &getScopeAt(uint32_t index) const { return _scopes[index]; }
	SpanView<uint32_t> getScopeNodes(uint32_t scope) const;

	// True when `scope` is `ancestor` or lies inside it. The interpreter walks the activation tree
	// with this: a consumer in a body reads a producer in an enclosing scope, never the other way.
	bool isScopeWithin(uint32_t scope, uint32_t ancestor) const;

	// The literal an unconnected data input takes, already converted to the pin's type. Null for an
	// input an edge feeds, and for one whose value is the type's zero.
	const mem_std::Value *getConstant(uint32_t node, uint32_t pin) const;

	// A node's setting, resolved by the build; null when the operation declares no such setting or
	// its value is the type's zero.
	const mem_std::Value *getSetting(uint32_t node, uint32_t index) const;

	// Parallel blocks.

	uint32_t getBlockCount() const { return uint32_t(_blocks.size()); }
	const RuntimeBlock &getBlockAt(uint32_t index) const { return _blocks[index]; }
	SpanView<RuntimeBlockWrite> getBlockWrites(uint32_t block) const;
	SpanView<uint32_t> getBlockCollectors(uint32_t block) const;
	SpanView<TypeId> getBlockQuery(uint32_t block, bool without) const;

	// The host attaches the SPIR-V it built from the text this graph's GPU blocks lower to; without
	// it a block runs on the CPU, because a run carries no GLSL compiler. Attach while no run holds
	// this graph: a launch reads the table. SPFlowGpu.cc.
	uint32_t attachGpuShaders(SpanView<GpuShaderBinary>);
	const GpuShaderTable *getGpuShaders() const { return _gpuShaders; }
	const GpuBlockShaders *getGpuBlock(uint32_t block) const;

	// The scene contract. This node's bindings, and empty unless the graph is bound: the contract
	// is derived with no scene in reach, so an unbound graph holds bindings that resolved to
	// nothing - and "the scene does not have this component" and "no scene was ever consulted" must
	// not look alike to an operation. Empty is what tells it to work the name out for itself, as it
	// always did.
	SpanView<SceneBinding> getSceneBindings(uint32_t node) const;

	/* The same slice, whether or not the graph was ever bound - for analysis rather than execution.
	What separates it from getSceneBindings is not the data but the caller. An operation is handed
	nothing until a scene has answered, because for it "the scene does not have this component" and
	"no scene was ever consulted" must not look alike. A pass that is reading the graph to say what
	the project uses (dispatch::DispatchSchema) is in the opposite position: the names are what it
	wants, they are a build-time fact, and a `desc` is exactly what it never touches. Hiding them
	from it would not make it careful, it would make it wrong - an unbound graph would contribute
	neither reads nor writes, its producers would vanish, and every input they produce would be
	reported as produced by nobody. So the rule is stated where it belongs, read the names and never
	the descriptors, rather than enforced by handing back an empty span. */
	SpanView<SceneBinding> getSceneContract(uint32_t node) const;

	// This node's extension bindings, and empty unless the graph is bound - the same distinction
	// the scene's makes, and a sharper one here: an unresolved extension is not something an
	// operation can work out for itself, so an empty slice is the only thing a node can be handed
	// when nobody resolved anything.
	SpanView<ExtensionBinding> getExtensionBindings(uint32_t node) const;

	// The same slice whether or not the graph was bound - the extension half of getSceneContract,
	// for the same reader: a pass that wants the names a node declares, and a generated unit that
	// carries them.
	SpanView<ExtensionBinding> getExtensionContract(uint32_t node) const;

	// The asset's declarations the extension contract was derived against, for whoever has to carry
	// them somewhere the asset is not - a generated unit binds against the same declarations.
	SpanView<const ExtensionDecl *> getExtensionDecls() const { return _extDecls; }

	// The holder this graph was bound to and its epoch at that moment. Null and zero for a graph
	// that binds no extension. The epoch only ever goes up and moves on every create and drop, so a
	// run against a holder that has changed is refused rather than run against a resolution that
	// has stopped being true - the same argument as getSceneRegistryCount(), over a set that can
	// shrink.
	const value::ExtensionHost *getExtensions() const { return _extensions; }
	uint32_t getExtensionsEpoch() const { return _extensionsEpoch; }

	/* The named entities (Named-4). Bound once, against the project's table, since the table is a
	fact of the project and not of any one scene: a named entity has the same id in every scene of
	its arena. `getNamedBindings` is empty until then, `getNamedContract` is the names either way.
	A failed bind leaves nothing bound. */
	Status bindNamed(const value::NamedHost &, DiagSink *report = nullptr);

	template <DiagContainer Out>
	Status bindNamed(const value::NamedHost &table, Out *report) {
		DiagSinkFor<Out> sink(report);
		return bindNamed(table, sink.get());
	}
	bool isNamedBound() const { return _namedBound; }
	SpanView<NamedBinding> getNamedBindings(uint32_t node) const;
	SpanView<NamedBinding> getNamedContract(uint32_t node) const;

	// What this graph is bound to, and how many types that registry held when it was bound. Null
	// and zero for a graph that binds nothing - one that was never checked against a scene, and
	// equally one that names nothing in a scene or names it only through edges. Such a graph holds
	// no descriptor that could go stale, so it runs against anything. The count is the stamp, and
	// it is sound for one reason: ComponentRegistry's order is append-only and nothing is ever
	// removed, so every binding that resolved is good for the registry's life and only the recorded
	// absences can go stale. A registry that has grown may have grown a type this graph gave up on,
	// so a run against it is refused rather than run against a "no" that has stopped being true.
	const value::TypeRegistry *getSceneRegistry() const { return _sceneRegistry; }
	uint32_t getSceneRegistryCount() const { return _sceneRegistryCount; }

	// The contract as data: what this graph names in the scene and what each name resolved to. A
	// dump and not a file section - the names are already in the asset, and a second copy of them
	// there would be two things to keep in step.
	void describeSceneContract(mem_std::Value &) const;

	// The same for the extensions: what this graph names, and whether it resolved.
	void describeExtensionContract(mem_std::Value &) const;

	void describe(mem_std::Value &) const;

	// Per parallel block, what the build worked out about it: the body, the concurrent region with
	// the edge that put each node there, and what certainly runs after. For tests and tools; the
	// verdicts themselves are the build's diagnostics. SPFlowParallel.cc.
	void describeParallel(mem_std::Value &) const;

	memory::pool_t *getPool() const { return _pool; }

private:
	// What eagerness costs on this graph. Run by build() once, and only when the graph is otherwise
	// sound - a graph full of errors would produce advice about wiring that does not exist. The
	// interpreter runs a node with no exec input as soon as its inputs exist, which is a strategy
	// the author cannot see in the picture, so the build says where that strategy does work nobody
	// asked for. It never refuses anything: an advice is about cost, not correctness.
	void adviseEagerCost(DiagReport &) const;

	// Works out which loop body every node belongs to, and refuses the shapes that have no answer:
	// a node reached as part of two different bodies, and an edge crossing out of one. Lives in
	// SPFlowScope.cc.
	void assignScopes(DiagReport &);

	GpuShaderTable *_gpuShaders = nullptr;

	// The blocks, their branch headers and budgets, over the scopes just assigned.
	// SPFlowParallel.cc.
	void deriveBlocks(DiagReport &);

	// A block its author gave to the GPU is lowered at the build (SPFlowGpu.h): a body too large,
	// or one that narrows a CPU value off its prefix, does not build. SPFlowGpu.cc.
	void checkGpuLowering(DiagReport &);

	// The static half of parallel blocks, over the scopes just assigned. `scene` adds what only a
	// registry knows: the members of an enum family. SPFlowParallel.cc.
	void analyzeParallel(DiagReport &, const value::TypeRegistry *scene);

	// Build phase 8: per-node facts derived from the finished graph, cached so that the interpreter
	// reads them instead of recomputing them on every step. Runs last and only on success.
	void deriveNodeData();

	// Build phase 3b: reads every scene group's names off the constants phase 3 has just settled,
	// one binding per group, unresolved. `inputTaken` is what separates a literal name from one an
	// edge computes, and it is alive only there.
	void deriveSceneContract(SpanView<uint8_t> inputTaken, const GraphAsset &, DiagReport &);

	// Resolves what phase 3b derived against a registry. Split from the derivation because an
	// editor validates on every keystroke with no scene in reach, and because a host binds again
	// after a scene load.
	void bindSceneContract(const value::TypeRegistry &, DiagReport &);

	// Build phase 3c, and its bind. Beside the two above and split the same way, along the same
	// line: a node against the asset's declaration needs no scene, a declaration against a live
	// instance needs one.
	void deriveExtensionContract(SpanView<uint8_t> inputTaken, const GraphAsset &, DiagReport &);
	void bindExtensionContract(const value::ExtensionHost &, DiagReport &);
	void deriveNamedContract(SpanView<uint8_t> inputTaken, DiagReport &);

	// The one body every build() overload runs. A null scene means "not checked against one", which
	// is what an editor and every graph test without a scene ask for; a null holder means the same
	// about the extensions.
	Status buildImpl(const GraphAsset &, const OpRegistry &, const value::TypeRegistry *scene,
			const value::ExtensionHost *, DiagSink *report);

	// Fills `_shapeSink` from the scopes just assigned.
	void takeShape();

	memory::pool_t *_pool = nullptr;
	bool _ownsPool = false;
	bool _built = false;

	// Where the next build writes its shape, or null (validate with a shape sets it for one build).
	GraphShape *_shapeSink = nullptr;

	mem_std::Vector<RuntimeNode> _nodes;
	mem_std::Vector<RuntimeDataEdge> _dataEdges;
	mem_std::Vector<RuntimeExecEdge> _execEdges;

	mem_std::Vector<uint32_t> _dataInIndex, _dataOutIndex, _execInIndex, _execOutIndex;
	mem_std::Vector<uint32_t> _crossScopeInIndex;
	mem_std::Vector<uint32_t> _entryNodes, _terminalNodes;
	mem_std::Vector<mem_std::Value> _constants;
	mem_std::Vector<mem_std::Value> _settings;
	mem_std::Vector<SceneBinding> _sceneBindings;
	const value::TypeRegistry *_sceneRegistry = nullptr;
	uint32_t _sceneRegistryCount = 0;

	mem_std::Vector<ExtensionBinding> _extBindings;
	mem_std::Vector<NamedBinding> _namedBindings;
	bool _namedBound = false;

	// Pointers into the asset's declarations - the parameter check needs them, and the asset
	// outlives the graph already (a RuntimeNode holds its GraphNode for the same reason).
	mem_std::Vector<const ExtensionDecl *> _extDecls;
	const value::ExtensionHost *_extensions = nullptr;
	uint32_t _extensionsEpoch = 0;

	mem_std::Vector<RuntimeScope> _scopes;
	mem_std::Vector<uint32_t> _scopeNodes;

	mem_std::Vector<RuntimeBlock> _blocks;
	mem_std::Vector<RuntimeBlockWrite> _blockWrites;
	mem_std::Vector<uint32_t> _blockCollectors;
	mem_std::Vector<TypeId> _blockQuery;
};

// The scene's side of the two contracts, over a table: the one place a name becomes a descriptor,
// and the one place an id becomes a pointer. Written over a table of bindings rather than as a step
// of the build, because two things hold such a table - the built graph, and a generated unit that
// carries the graph's contract as constants (SPFlowCompiled.h) - and one binding, shared, is what
// keeps "bound" meaning the same thing in both. Each walks the nodes' slices
// (`sceneBegin`/`sceneCount`, `extBegin`/`extCount`), clears every descriptor, and fills in what
// resolves; what does not is reported with the node as its locus.

SP_PUBLIC void bindSceneBindings(SpanView<RuntimeNode> nodes,
		mem_std::Vector<SceneBinding> &bindings, const value::TypeRegistry &, DiagReport &);

SP_PUBLIC void bindExtensionBindings(SpanView<RuntimeNode> nodes,
		mem_std::Vector<ExtensionBinding> &bindings, SpanView<const ExtensionDecl *> decls,
		const value::ExtensionHost &, DiagReport &);

SP_PUBLIC void bindNamedBindings(SpanView<RuntimeNode> nodes,
		mem_std::Vector<NamedBinding> &bindings, const value::NamedHost &, DiagReport &);

// Whether a table holds anything a registry could have answered: a graph that names nothing, or
// names it only through edges, holds no descriptor that could go stale and is bound to nothing.
SP_PUBLIC bool anyResolvable(SpanView<SceneBinding>);
SP_PUBLIC bool anyResolvable(SpanView<ExtensionBinding>);

// The description, over the graph policy: the golden dump `graph-build` compares, written once over
// what any representation of a graph answers (SPFlowStatic.h). The built graph and a generated
// unit describe themselves through the same function, so "the unit describes the graph it was
// written from" is a comparison of two tables and not of two describers.

template <typename G>
inline void describeGraph(const G &g, mem_std::Value &out) {
	out = mem_std::Value(mem_std::Value::Type::DICTIONARY);

	auto &nodes = out.newArray("nodes");
	for (uint32_t n = 0; n < g.getNodeCount(); ++n) {
		auto &node = g.getNodeAt(n);
		mem_std::Value entry(mem_std::Value::Type::DICTIONARY);
		entry.setInteger(int64_t(node.id), "id");
		entry.setString(node.op ? node.op->getName() : StringView("<unresolved>"), "op");
		if (node.isEntry) {
			entry.setBool(true, "entry");
		}
		if (node.isTerminal) {
			entry.setBool(true, "terminal");
		}
		entry.setInteger(node.dataInCount, "dataIn");
		entry.setInteger(node.dataOutCount, "dataOut");
		entry.setInteger(node.execInCount, "execIn");
		entry.setInteger(node.execOutCount, "execOut");
		if (node.localSchema) {
			entry.setString(node.localSchema->getName(), "locals");
		}
		if (!node.familyName.empty()) {
			entry.setString(node.familyName, "family");
		}
		if (node.weight != 0) {
			entry.setInteger(node.weight, "weight");
		}

		if (node.op) {
			mem_std::Value constants(mem_std::Value::Type::DICTIONARY);
			auto pins = node.op->getDataIn();
			for (uint32_t p = 0; p < uint32_t(pins.size()); ++p) {
				if (auto value = g.getConstant(n, p)) {
					constants.setValue(*value, pins[p].name);
				}
			}
			if (constants.size() > 0) {
				entry.setValue(sprt::move(constants), "constants");
			}

			mem_std::Value settings(mem_std::Value::Type::DICTIONARY);
			auto decls = node.op->getSettings();
			for (uint32_t k = 0; k < uint32_t(decls.size()); ++k) {
				if (auto value = g.getSetting(n, k)) {
					settings.setValue(*value, decls[k].name);
				}
			}
			if (settings.size() > 0) {
				entry.setValue(sprt::move(settings), "settings");
			}
		}

		nodes.addValue(sprt::move(entry));
	}

	auto &dataEdges = out.newArray("dataEdges");
	for (auto &e : g.getDataEdges()) {
		auto &src = g.getNodeAt(e.srcNode);
		auto &dst = g.getNodeAt(e.dstNode);
		mem_std::Value entry(mem_std::Value::Type::DICTIONARY);
		entry.setInteger(int64_t(src.id), "from");
		entry.setString(src.op->getDataOut()[e.srcPin].name, "fromPin");
		entry.setInteger(int64_t(dst.id), "to");
		entry.setString(dst.op->getDataIn()[e.dstPin].name, "toPin");
		entry.setString(value::getCastRuleName(e.cast), "cast");
		if (e.needsArena) {
			entry.setBool(true, "needsArena");
		}
		dataEdges.addValue(sprt::move(entry));
	}

	auto &execEdges = out.newArray("execEdges");
	for (auto &e : g.getExecEdges()) {
		auto &src = g.getNodeAt(e.srcNode);
		mem_std::Value entry(mem_std::Value::Type::DICTIONARY);
		entry.setInteger(int64_t(src.id), "from");
		entry.setString(src.op->getExecOut()[e.srcPin], "fromPin");
		entry.setInteger(int64_t(g.getNodeAt(e.dstNode).id), "to");
		if (e.backEdge) {
			entry.setBool(true, "back");
		}
		execEdges.addValue(sprt::move(entry));
	}

	// Only when there is more than the graph itself, so a dump of a graph without loops says
	// nothing about scopes.
	if (g.getScopeCount() > 1) {
		auto &scopes = out.newArray("scopes");
		for (uint32_t s = 0; s < g.getScopeCount(); ++s) {
			auto &scope = g.getScopeAt(s);
			mem_std::Value entry(mem_std::Value::Type::DICTIONARY);
			entry.setInteger(int64_t(s), "scope");
			entry.setInteger(int64_t(scope.depth), "depth");
			if (scope.opener != InvalidIndex) {
				auto &opener = g.getNodeAt(scope.opener);
				entry.setInteger(int64_t(opener.id), "opener");
				entry.setString(opener.op->getExecOut()[scope.execPin], "pin");
				entry.setInteger(int64_t(scope.parent), "parent");
			}
			if (scope.kind != ScopeKind::Loop) {
				entry.setString(getScopeKindName(scope.kind), "kind");
			}
			if (scope.barrier != InvalidIndex) {
				entry.setInteger(int64_t(g.getNodeAt(scope.barrier).id), "barrier");
			}
			auto &nodeIds = entry.newArray("nodes");
			for (auto n : g.getScopeNodes(s)) { nodeIds.addInteger(int64_t(g.getNodeAt(n).id)); }
			scopes.addValue(sprt::move(entry));
		}
	}

	auto &entries = out.newArray("entry");
	for (auto n : g.getEntryNodes()) { entries.addInteger(int64_t(g.getNodeAt(n).id)); }

	auto &terminals = out.newArray("terminal");
	for (auto n : g.getTerminalNodes()) { terminals.addInteger(int64_t(g.getNodeAt(n).id)); }
}

// The contract as data, whether or not the graph was bound - "bound": false is exactly what one
// wants to see of a graph that was built alone.
template <typename G>
inline void describeSceneContractOf(const G &g, mem_std::Value &out) {
	out = mem_std::Value(mem_std::Value::Type::ARRAY);
	for (uint32_t i = 0; i < g.getNodeCount(); ++i) {
		auto id = g.getNodeAt(i).id;
		for (auto &b : g.getSceneContract(i)) {
			mem_std::Value entry(mem_std::Value::Type::DICTIONARY);
			entry.setInteger(int64_t(id), "node");
			if (b.dynamic) {
				// There is nothing else true about it, and printing an empty name beside it would
				// read as a component called "".
				entry.setBool(true, "dynamic");
				out.addValue(sprt::move(entry));
				continue;
			}
			entry.setString(b.component, "component");
			if (!b.field.empty()) {
				entry.setString(b.field, "field");
			}
			if (b.declared != VarType::Nil) {
				entry.setString(value::getVarTypeName(b.declared), "type");
			}
			if (b.optional) {
				entry.setBool(true, "optional");
			}
			// Whether the name found anything, which is the whole difference between a built graph
			// and a bound one.
			entry.setBool(b.type != nullptr, "bound");
			out.addValue(sprt::move(entry));
		}
	}
}

template <typename G>
inline void describeExtensionContractOf(const G &g, mem_std::Value &out) {
	out = mem_std::Value(mem_std::Value::Type::ARRAY);
	for (uint32_t i = 0; i < g.getNodeCount(); ++i) {
		auto id = g.getNodeAt(i).id;
		for (auto &b : g.getExtensionContract(i)) {
			mem_std::Value entry(mem_std::Value::Type::DICTIONARY);
			entry.setInteger(int64_t(id), "node");
			if (b.dynamic) {
				entry.setBool(true, "dynamic");
				out.addValue(sprt::move(entry));
				continue;
			}
			entry.setString(b.id, "id");
			if (!b.name.empty()) {
				entry.setString(b.name, "extension");
			}
			entry.setBool(b.instance != nullptr, "bound");
			out.addValue(sprt::move(entry));
		}
	}
}

} // namespace stappler::flow

#endif /* STAPPLER_FLOW_SPFLOWRUNTIME_H_ */
