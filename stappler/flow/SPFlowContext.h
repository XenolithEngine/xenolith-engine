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

#ifndef STAPPLER_FLOW_SPFLOWCONTEXT_H_
#define STAPPLER_FLOW_SPFLOWCONTEXT_H_

#include "SPFlowLocal.h"

// What an operation is handed (the door), and the implementation of it over the two policies the
// machine runs on (SPFlowStatic.h): the graph's representation and the run's local store.
// `OpContext` is the seam where the arena kind and the graph's representation stop travelling: a
// virtual cannot be a template and an operation is a plain function pointer in a registry, so
// `ops/` and `ext/` are written against the abstract base and are not templates. One vtable, one
// set of function pointers, whatever kind of store the run is in and whatever the graph is made of.
namespace STAPPLER_VERSIONIZED stappler::flow {

// What an operation is handed. Everything it may reach, and nothing else: there is no back door to
// the interpreter's own bookkeeping, and none to the graph beyond this node's own signature. The
// cost is one indirect call per door; the bodies on the far side stay monomorphic on the concrete
// kind, so the erasure is one indirection at the boundary and not one per byte.
class SP_PUBLIC OpContext {
public:
	virtual ~OpContext() = default;

	const OpDesc &getOp() const { return *_op; }
	NodeId getNodeId() const { return _id; }
	uint32_t getNodeIndex() const { return _node; }
	uint32_t getActivation() const { return _activation; }

	// True when the input has a value: an edge that has delivered, a node parameter, or the pin's
	// default. An optional input with none of those is false, and reading it gives the type's zero.
	virtual bool hasInput(uint32_t pin) const = 0;

	// The value, with the edge's conversion already applied - the rule was decided by the build and
	// is read from the edge, never recomputed.
	virtual Status getInput(uint32_t pin, Var &out) const = 0;

	// The address of the container behind an input: the producer's own field, borrowed, not copied.
	// Nothing is allocated to read a String or an Array across an edge. Valid until the next
	// allocator call, exactly like every other address in this codebase.
	virtual Addr getInputAddr(uint32_t pin) const = 0;

	// A String input as a view. For a constant - which is what every `component` and `field` pin in
	// every graph anyone writes actually is - the view points into the graph's own constant table
	// and nothing is allocated; the ordinary path materialises the literal into the arena and frees
	// it again on every call, and an edge-fed name still goes through the arena. The view is
	// borrowed and dies at the next allocator call: turn it into a TypeId or a descriptor and keep
	// neither.
	virtual Status getInputName(uint32_t pin, StringView &out) const = 0;

	// An Array input, by length and by element, and the same argument as getInputName one type up:
	// an operation that walks an array wants how many and which one, and neither needs the array to
	// exist in the store. A constant array is read where it has been sitting since the build, in
	// the graph's constant table; an edge-fed array is a value somebody computed and still goes
	// through the arena. One narrowing worth naming: a malformed element of a constant fails on the
	// turn that reaches it, where materialising the whole array failed on the first - the same
	// refusal by the same decoder, at a different moment.
	virtual Status getInputCount(uint32_t pin, uint32_t &out) const = 0;
	virtual Status getInputElement(uint32_t pin, uint32_t index, Var &out) const = 0;

	// The address of an output field in this node's record, and the mark that says it was produced.
	// Marked at the moment the address is handed out rather than on write: an operation that claims
	// an output and writes nothing then has zeroes, which is defined, instead of a value nobody can
	// see because the mask stayed empty.
	virtual Addr claimOutput(uint32_t pin) = 0;

	// Inline values only. A container Var refers to a block somebody else owns, and storing the
	// handle would give one block two owners and a double free at the end of the run - so the two
	// ways to produce a container are claimOutput() plus the blob:: accessors, or the copy below.
	virtual Status setOutput(uint32_t pin, const Var &) = 0;

	// Deep copy, the honest way to pass a container through: the destination ends up owning its own
	// block.
	virtual Status copyInputToOutput(uint32_t inPin, uint32_t outPin) = 0;

	// The node's own memory: the fields the operation declared in OpDef::locals, indexed from zero
	// among those rather than within the record. A loop's cursor is exactly this, and it has to
	// survive from one turn to the next. A local is not an output: nothing marks it produced and no
	// consumer can read it.
	virtual Status getLocal(uint32_t index, Var &out) const = 0;
	virtual Status setLocal(uint32_t index, const Var &) = 0;
	virtual Addr localAddr(uint32_t index) const = 0;

	// Fires an exec output. The token is delivered after the operation returns, so an operation
	// that fires and then fails changes nothing.
	virtual Status fire(uint32_t execOut) = 0;
	virtual Status fire(StringView execOut) = 0;

	// The scene, through a narrow door. A node that needs long-lived state - a Wait that keeps its
	// remaining time, a watch that removes the component it saw so the same answer cannot fire a
	// branch twice - keeps it in the scene, because the local store dies with the run. Deliberately
	// absent: general queries (a parallel fan-out's query below is the block's own, run before its
	// branches), creating or destroying entities, a deferred-effect buffer - without a command
	// buffer a node reaching them could move a pool's rows out from under a walk in progress.
	// Access is gated by the operation's own declaration: reading without OpFlags::ReadsScene and
	// writing without OpFlags::WritesScene are refused with ErrorNotPermitted.
	virtual value::EntityId getGlobalEntity() const = 0;

	virtual bool sceneHas(value::EntityId, TypeId) const = 0;
	virtual Status sceneGet(value::EntityId, TypeId, StringView field, Var &out) const = 0;
	virtual Status sceneSet(value::EntityId, TypeId, StringView field, const Var &) = 0;

	// The one lookup the door offers: the first entity whose component field equals `value`, in the
	// pool's row order. ErrorNotFound when nothing matches, which is an answer rather than a
	// failure. It walks a pool and mutates nothing, so the rule that structural mutation
	// invalidates a walk is respected by construction - the walk begins and ends inside this call.
	virtual Status sceneFind(TypeId, StringView field, const Var &value,
			value::EntityId &out) const = 0;

	// Idempotent, exactly like EntityStore::addComponent: an entity that already has the component
	// keeps the record it had. "Make sure this entity has a timer" must not reset the timer.
	virtual Status sceneAdd(value::EntityId, TypeId) = 0;
	virtual Status sceneRemove(value::EntityId, TypeId) = 0;

	// The same six doors with the names already resolved: this node's scene bindings, one per group
	// its signature declares, in that order, so an operation reads its own group as
	// `getSceneBindings()[0]` and not by searching for anything. Empty when the graph was never
	// bound to a scene, and then an operation takes the TypeId doors above. A bound door does no
	// lookup at all - no makeTypeId, no walk of the registry, no field name compared byte by byte -
	// because all three were answered when the graph was built, once, for its whole life.
	SpanView<SceneBinding> getSceneBindings() const { return _sceneBind; }

	// The scene's extensions, and the door is narrow on purpose: an index into this node's own
	// bindings, and a pointer or nothing. No lookup by name, no registry walk, no "which extension
	// was that" - all three were answered when the graph was built. Null when the graph was never
	// bound to a holder, when the id arrives on an edge, or when the binding failed; unlike a scene
	// group there is no older path behind it, because an extension is a host object and cannot be
	// worked out from a string, so an operation that gets null refuses. That is why the build has
	// six diagnostics about extensions: the failure has to happen where an author can read it.
	// Gated by the operation's own ReadsScene / WritesScene, because an extension's state lives in
	// the scene's store and needs no permission of its own.
	SpanView<ExtensionBinding> getExtensionBindings() const { return _extBind; }
	virtual value::ExtensionInstance *getExtension(uint32_t group) const = 0;

	// The named entities this node names, one per EntityName pin, resolved to their ids when the
	// graph was bound to the project's table; empty when it was not.
	SpanView<NamedBinding> getNamedBindings() const { return _namedBind; }

	virtual bool sceneHas(value::EntityId, const SceneBinding &) const = 0;
	virtual Status sceneGet(value::EntityId, const SceneBinding &, Var &out) const = 0;
	virtual Status sceneSet(value::EntityId, const SceneBinding &, const Var &) = 0;
	virtual Status sceneFind(const SceneBinding &, const Var &value,
			value::EntityId &out) const = 0;
	virtual Status sceneAdd(value::EntityId, const SceneBinding &) = 0;
	virtual Status sceneRemove(value::EntityId, const SceneBinding &) = 0;

	// Parallel blocks. Inside a branch the scene doors above serve the branch's own entity from its
	// copies: a write of a field the block writes goes to the copy, a read of it comes from the
	// copy once written, and the copies are committed when the block is delivered. A fan-out's
	// query appends every entity of the block's `with`/`without` to the Array local `local`, in the
	// row order of the first `with` component.
	virtual Status queryEntities(uint32_t local) = 0;

	// A collector's `BranchValue` input, branch by branch: how many branches the block had, whether
	// branch `i` has a value (not failed, the producer produced, the policy commits), and the
	// value.
	virtual Status getBranchCount(uint32_t pin, uint32_t &out) const = 0;
	virtual bool isBranchPresent(uint32_t pin, uint32_t branch) const = 0;
	virtual Status getBranchValue(uint32_t pin, uint32_t branch, Var &out) const = 0;

	// A collector whose fold the device already did takes it here instead of walking the branches;
	// ErrorNotFound means there is none and the fold happens on the CPU. Not const: it takes the
	// value, so a re-run of the collector folds rather than repeating a stale one.
	virtual Status takeBranchFold(uint32_t pin, Var &out) { return Status::ErrorNotFound; }

	// A barrier's own block: how many branches, and for each whether it is present and whether it
	// failed.
	virtual Status getBarrierCount(uint32_t &out) const = 0;
	virtual Status getBarrierBranch(uint32_t branch, bool &present, bool &failed) const = 0;

	// Functions (SPFlowFunction.h). A call node asks for its function to be opened, the way an
	// operation fires an exec output: the machine opens it when the step returns. A body's entry
	// copies the call's input `pin` to its own output `out`; a return copies its own input `pin` to
	// the call's output of that index, and names the exit the call leaves by. The last three answer
	// ErrorNotFound outside a called body - a function's document run as a graph of its own.
	virtual Status call() = 0;
	virtual Status copyArgument(uint32_t pin, uint32_t out) = 0;
	virtual Status copyResult(uint32_t pin) = 0;
	virtual Status setResultExit(uint32_t exit) = 0;

	// By value and kind-erased. An operation that reaches the arena does so to call blob::, which
	// is instantiated for ArenaRef exactly as for a real kind - so `ops/` is written once and reads
	// a debugger's store and a release one with the same code.
	virtual value::ArenaRef getArena() const = 0;

	// The run's scene as its environment erases it (SPFlowEnv.h), and which environment that is:
	// for an operation that belongs to the environment and knows its type. Null with no scene.
	virtual uint64_t getEnvTag() const = 0;
	virtual const void *getSceneRef() const = 0;

protected:
	// State no policy touches, held here so that the accessors above stay inline and non-virtual.
	// Everything that needs a store or a graph is a virtual and lives in ContextT below.
	const OpDesc *_op = nullptr;
	uint32_t _node = InvalidIndex;
	NodeId _id = NullNodeId;
	uint32_t _activation = RootActivation;

	SpanView<SceneBinding> _sceneBind;
	SpanView<ExtensionBinding> _extBind;
	SpanView<NamedBinding> _namedBind;
};

// One unit of work, as the machine resolved it: which node, in which activation, with its frame
// already found and both contracts already sliced. Everything a door needs and nothing a door
// decides. It exists because there are two ways to perform a unit of work and they must be handed
// the same thing: the machine builds one of these and then either fills its own door with it (the
// interpreter, and a unit run through the registry's OpFn) or hands it to the unit's compiled step,
// which fills a door of its own with the node index a constant. `fired` travels
// back - an operation fires exec outputs into its door, and this is where the door leaves them.
template <typename Graph, typename Local>
struct StepSite {
	const Graph *graph = nullptr;
	Local *local = nullptr;
	typename Local::SceneType *scene = nullptr;

	const OpDesc *op = nullptr;
	uint32_t node = InvalidIndex;
	NodeId id = NullNodeId;
	uint32_t activation = RootActivation;

	// The activation's frame. The node's own state and record are an addition away from it, and
	// which addition is what the two sites below disagree about.
	Addr frame = NullAddr;

	SpanView<SceneBinding> sceneBind;
	SpanView<ExtensionBinding> extBind;
	SpanView<NamedBinding> namedBind;

	// The parallel branch this step is in, when it is in one: its block, its activation, its frame
	// (the header is at the start of it) and its entity.
	uint32_t block = InvalidIndex;
	uint32_t branchActivation = InvalidIndex;
	Addr branchFrame = NullAddr;
	value::EntityId branchEntity;

	uint32_t fired = 0;

	// The operation asked for its function to be opened (OpContext::call).
	bool called = false;
};

// Where a node's own rows are, and the only thing the two doors spell differently. The interpreter
// runs a node whose index is a number it was handed, so it looks the answers up: the edge feeding a
// pin by walking the node's incoming edges, the offsets of its state and its record out of the
// store's layout table. A generated unit knows which node it is compiling, so the compiler answers
// the same three questions off the unit's constant tables (StaticSite, in SPFlowCompiled.h). Every
// rule that uses them is written once, in ContextT below: a site decides where to look and never
// what the answer means, which is why there is nothing else in one.
struct DynamicSite {
	template <typename Graph>
	static const typename Graph::DataEdge *incoming(const Graph &graph, uint32_t node, uint32_t pin) {
		// A node's incoming edges are few and in canonical order, so a scan is both cheap and
		// deterministic, and it walks memory the step has already touched.
		for (auto e : graph.getDataInEdges(node)) {
			auto &edge = graph.getDataEdges()[e];
			if (edge.dstPin == pin) {
				return &edge;
			}
		}
		return nullptr;
	}

	template <typename Local>
	static Addr recordIn(const Local &local, Addr frame, uint32_t node) {
		return local.getRecordIn(frame, node);
	}

	template <typename Local>
	static Addr stateIn(const Local &local, Addr frame, uint32_t node) {
		return local.getStateIn(frame, node);
	}

	template <typename Graph>
	static FieldShape fieldAt(const Graph &graph, uint32_t node, uint32_t index) {
		auto &rt = graph.getNodeAt(node);
		if (!rt.localSchema) {
			return FieldShape();
		}
		auto fields = rt.localSchema->getFields();
		if (index >= fields.size()) {
			return FieldShape();
		}
		auto shape = shapeOfField(fields[index]);
		shape.subtypeId = nodeFieldSubtype(rt, index, shape.subtypeId);
		return shape;
	}
};

// The implementation, one per pair of policies. `final`, so a call through a known ContextT &
// inside graph/ devirtualizes and the erasure costs nothing where the kind is known. `Graph` is
// what the node's edges, constants and bindings are read from; `Local` is where its record and its
// state live and whose arena a container is borrowed out of. The scene is a store of the local's
// arena kind - a run reads and writes one scene, and it is in the kind the run is in. `Site` is
// where this node's rows are found, and the default is "ask the graph"; a generated unit passes one
// that knows the answers.
template <typename Graph, typename Local, typename Site = DynamicSite>
class ContextT final : public OpContext {
public:
	using GraphType = Graph;
	using LocalType = Local;
	using SiteType = Site;
	using ArenaType = typename Local::ArenaType;
	using EnvType = typename Local::EnvType;
	using SceneStore = typename Local::SceneType;

	// Fills the door for one unit of work, and empties it again. Both halves are here rather than
	// at the caller because there are two callers - the machine and a unit's compiled step - and a
	// door filled two ways is two doors. `finish` releases the blocks a container constant was
	// materialised into and hands back what the operation fired; it runs whether the operation
	// succeeded or not, since a failed step still leaves its scratch behind.
	void bind(const StepSite<Graph, Local> &site);
	void finish(StepSite<Graph, Local> &site);

	bool hasInput(uint32_t pin) const override;
	Status getInput(uint32_t pin, Var &out) const override;
	Addr getInputAddr(uint32_t pin) const override;
	Status getInputName(uint32_t pin, StringView &out) const override;
	Status getInputCount(uint32_t pin, uint32_t &out) const override;
	Status getInputElement(uint32_t pin, uint32_t index, Var &out) const override;
	Addr claimOutput(uint32_t pin) override;
	Status setOutput(uint32_t pin, const Var &) override;
	Status copyInputToOutput(uint32_t inPin, uint32_t outPin) override;
	Status getLocal(uint32_t index, Var &out) const override;
	Status setLocal(uint32_t index, const Var &) override;
	Addr localAddr(uint32_t index) const override;
	Status fire(uint32_t execOut) override;
	Status fire(StringView execOut) override;
	value::EntityId getGlobalEntity() const override;
	bool sceneHas(value::EntityId, TypeId) const override;
	Status sceneGet(value::EntityId, TypeId, StringView field, Var &out) const override;
	Status sceneSet(value::EntityId, TypeId, StringView field, const Var &) override;
	Status sceneFind(TypeId, StringView field, const Var &value,
			value::EntityId &out) const override;
	Status sceneAdd(value::EntityId, TypeId) override;
	Status sceneRemove(value::EntityId, TypeId) override;
	value::ExtensionInstance *getExtension(uint32_t group) const override;
	bool sceneHas(value::EntityId, const SceneBinding &) const override;
	Status sceneGet(value::EntityId, const SceneBinding &, Var &out) const override;
	Status sceneSet(value::EntityId, const SceneBinding &, const Var &) override;
	Status sceneFind(const SceneBinding &, const Var &value, value::EntityId &out) const override;
	Status sceneAdd(value::EntityId, const SceneBinding &) override;
	Status sceneRemove(value::EntityId, const SceneBinding &) override;
	Status queryEntities(uint32_t local) override;
	Status getBranchCount(uint32_t pin, uint32_t &out) const override;
	bool isBranchPresent(uint32_t pin, uint32_t branch) const override;
	Status getBranchValue(uint32_t pin, uint32_t branch, Var &out) const override;
	Status takeBranchFold(uint32_t pin, Var &out) override;
	Status getBarrierCount(uint32_t &out) const override;
	Status getBarrierBranch(uint32_t branch, bool &present, bool &failed) const override;
	Status call() override;
	Status copyArgument(uint32_t pin, uint32_t out) override;
	Status copyResult(uint32_t pin) override;
	Status setResultExit(uint32_t exit) override;

	value::ArenaRef getArena() const override { return value::ArenaRef::of(*_local->getArena()); }

	uint64_t getEnvTag() const override { return EnvType::Tag; }
	const void *getSceneRef() const override {
		if (!_scene) {
			return nullptr;
		}
		_sceneRef = EnvType::refOf(_scene);
		return &_sceneRef;
	}

	// The concrete ones, for graph/ itself, which knows the kind and should not pay to forget it.
	Local &getLocalStore() const { return *_local; }
	SceneStore *getSceneStore() const { return _scene; }

private:
	// Blocks allocated to hand a container constant to an operation. Freed when the step ends,
	// which is why they are host-side: they do not survive the step and so are not state.
	struct Scratch {
		Addr handle = NullAddr;
		VarType type = VarType::Nil;
		ElementChain element = 0;
	};

	uint32_t sourceActivation(uint32_t srcNode) const;

	// The call this step's body was opened by: the nearest function activation up the tree, and the
	// call node and activation that opened it. False outside a called body.
	bool callerOf(uint32_t &node, uint32_t &activation) const;
	Status readEdgeValue(uint32_t pin, const typename Graph::DataEdge &, Var &out) const;
	Addr materializeConstant(uint32_t pin) const;
	void releaseScratch();

	// The edge feeding this input, or null - through the site, which is where the two doors differ.
	const typename Graph::DataEdge *incoming(uint32_t pin) const {
		return Site::template incoming<Graph>(*_graph, _node, pin);
	}

	// Null when there is no scene, the type is unknown to it, or the operation did not declare the
	// access. One place to ask, so that the flag check cannot be forgotten in one of six doors.
	const value::ComponentType *sceneType(TypeId, bool write) const;

	// The copy of `field` of this branch's own entity, when this step is in a branch, `id` is that
	// entity and the block writes the field; null otherwise.
	const RuntimeBlockWrite *ownCopy(value::EntityId id, TypeId component, StringView field) const;
	Status readCopy(const RuntimeBlockWrite &, bool &written, Var &out) const;
	Status writeCopy(const RuntimeBlockWrite &, const Var &);

	// The block a collector's input comes from, with its fan-out's activation and branch range.
	struct BranchRange {
		uint32_t block = InvalidIndex;
		uint32_t fanOutActivation = InvalidIndex;
		uint32_t first = 0;
		uint32_t count = 0;
	};
	bool branchRange(uint32_t block, BranchRange &) const;
	const typename Graph::DataEdge *branchEdge(uint32_t pin, BranchRange &) const;

	// The three that carry a policy. Everything else - the op, the node, the activation and this
	// node's slice of the bindings - is in the base, where it stays inline.
	const Graph *_graph = nullptr;
	Local *_local = nullptr;
	SceneStore *_scene = nullptr;

	Addr _record = NullAddr;
	Addr _state = NullAddr;
	uint32_t _fired = 0;
	bool _called = false;

	uint32_t _block = InvalidIndex;
	uint32_t _branchActivation = InvalidIndex;
	Addr _branchFrame = NullAddr;
	value::EntityId _branchEntity;

	// Resolved once when the context is built, not tested on every call.
	bool _readsScene = false;
	bool _writesScene = false;

	mutable mem_std::Vector<Scratch> _scratch;
	mutable typename EnvType::SceneRef _sceneRef;
};

} // namespace stappler::flow

#endif /* STAPPLER_FLOW_SPFLOWCONTEXT_H_ */
