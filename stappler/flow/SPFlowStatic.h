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

#ifndef STAPPLER_FLOW_SPFLOWSTATIC_H_
#define STAPPLER_FLOW_SPFLOWSTATIC_H_

#include "SPFlowLocal.h"

#include <concepts>

// The two policies the machine (SPFlowMachine.h) runs on, stated as what it asks of them. A policy
// is a type that answers a fixed list of questions, and the list is the contract: the machine is
// written once against it, the built graph and the arena store answer it, and so do the static
// tables a generated unit carries. The requirements are `requires`-expressions checked by
// static_assert rather than concepts - a policy that stops answering then fails to compile with the
// question named, instead of failing to link with a mangled name. Nothing here names a field of a
// node or an edge: the machine reads `rt.scope`, `edge.srcNode` and the rest directly, and a row
// type that lacks one fails on the line that reads it. What the requirements pin is the shape of
// the answers - which methods exist, what they take, and that the row types are named so a second
// representation can carry rows of its own. One question is optional and is in neither list below:
// `compiledStep<Local>()`, which a graph may offer to say it carries code that performs a node
// itself. MachineT asks for it under `if constexpr (requires ...)`, so a graph that has none -
// RuntimeGraph, the interpreter's whole world - compiles to the code that was there before it.
namespace STAPPLER_VERSIONIZED stappler::flow {

// How the graph is represented: everything the build settled - the canonical order, the resolved
// operations, the edges with their casts, the scopes, the constants, the bindings - reached through
// one set of names, whether it lives in a RuntimeGraph's vectors or in a generated unit's constexpr
// arrays. The row types are the policy's own (`Node`, `DataEdge`, `ExecEdge`, `Scope`), and every
// method that hands one out hands it out by reference into storage that outlives the run.

template <typename G>
constexpr bool IsGraphPolicy = requires(const G &g, uint32_t i, NodeId id) {
	typename G::Node;
	typename G::DataEdge;
	typename G::ExecEdge;
	typename G::Scope;

	{ g.isValid() } -> std::convertible_to<bool>;
	{ g.getNodeCount() } -> std::convertible_to<uint32_t>;
	{ g.getNodeAt(i) } -> std::convertible_to<const typename G::Node &>;
	{ g.findNode(id) } -> std::convertible_to<uint32_t>;

	{ g.getDataEdges() } -> std::convertible_to<SpanView<typename G::DataEdge>>;
	{ g.getExecEdges() } -> std::convertible_to<SpanView<typename G::ExecEdge>>;
	{ g.getDataInEdges(i) } -> std::convertible_to<SpanView<uint32_t>>;
	{ g.getDataOutEdges(i) } -> std::convertible_to<SpanView<uint32_t>>;
	{ g.getExecOutEdges(i) } -> std::convertible_to<SpanView<uint32_t>>;
	{ g.getCrossScopeInEdges(i) } -> std::convertible_to<SpanView<uint32_t>>;
	{ g.getEntryNodes() } -> std::convertible_to<SpanView<uint32_t>>;
	{ g.getTerminalNodes() } -> std::convertible_to<SpanView<uint32_t>>;

	{ g.getScopeCount() } -> std::convertible_to<uint32_t>;
	{ g.getScopeAt(i) } -> std::convertible_to<const typename G::Scope &>;
	{ g.getScopeNodes(i) } -> std::convertible_to<SpanView<uint32_t>>;

	// The literal an unconnected input takes, or null: read by the door on every getInput of a
	// constant, and by the two array readers that never touch the arena.
	{ g.getConstant(i, i) } -> std::convertible_to<const mem_std::Value *>;

	// This node's bindings, empty unless bound - "the scene does not have this" and "no scene was
	// consulted" must not look alike to an operation.
	{ g.getSceneBindings(i) } -> std::convertible_to<SpanView<SceneBinding>>;
	{ g.getExtensionBindings(i) } -> std::convertible_to<SpanView<ExtensionBinding>>;

	// The same two whether or not bound - what a reader of the contract, and its description, want.
	{ g.getSceneContract(i) } -> std::convertible_to<SpanView<SceneBinding>>;
	{ g.getExtensionContract(i) } -> std::convertible_to<SpanView<ExtensionBinding>>;
	{ g.getNamedBindings(i) } -> std::convertible_to<SpanView<NamedBinding>>;
	{ g.getNamedContract(i) } -> std::convertible_to<SpanView<NamedBinding>>;

	// What the graph is bound to, so that a run against something else is refused rather than run
	// against descriptors that have stopped being true.
	{ g.getSceneRegistry() } -> std::convertible_to<const value::TypeRegistry *>;
	{ g.getSceneRegistryCount() } -> std::convertible_to<uint32_t>;
	{ g.getExtensions() } -> std::convertible_to<const value::ExtensionHost *>;
	{ g.getExtensionsEpoch() } -> std::convertible_to<uint32_t>;

	// Parallel blocks; a generated unit carries none.
	{ g.getBlockCount() } -> std::convertible_to<uint32_t>;
	{ g.getBlockAt(i) } -> std::convertible_to<const RuntimeBlock &>;
	{ g.getBlockWrites(i) } -> std::convertible_to<SpanView<RuntimeBlockWrite>>;
	{ g.getBlockCollectors(i) } -> std::convertible_to<SpanView<uint32_t>>;
	{ g.getBlockQuery(i, true) } -> std::convertible_to<SpanView<TypeId>>;

	// The GPU shaders the host attached, or null. A graph that carries none says so, and every
	// block of it runs on the CPU.
	{ g.getGpuShaders() } -> std::convertible_to<const GpuShaderTable *>;
	{ g.getGpuBlock(i) } -> std::convertible_to<const GpuBlockShaders *>;
};

static_assert(IsGraphPolicy<RuntimeGraph>, "RuntimeGraph is the graph policy the machine was written against");

// Where the run's records live, and the run's own bookkeeping with them: the frames, the activation
// tree, the front, the stalled list, the counters. `ArenaType` is the kind of arena the scene is in
// - a run reads and writes one scene, and the machine names its store through this - whether or not
// the local records are in an arena at all. Addresses are `Addr`, and a record is `frame + offset`
// under every policy: the machine holds an address it was handed and asks the policy to read
// through it. What an address means is the policy's - an arena offset today, and whatever the fast
// store makes of it.

template <typename L>
constexpr bool IsLocalPolicy = requires(L &l, const L &c, uint32_t i, Addr a, int64_t bits,
		uint64_t key, const Callback<bool(uint32_t, uint32_t, Addr)> &each) {
	typename L::ArenaType;

	// Whether a unit of work in this store can be undone. The arena store says yes - its whole
	// state is bytes a version covers; the fast store says no, because half of the run is host
	// memory. The machine reads it to refuse stepBack and to refuse a journal at a quantum finer
	// than `Run`.
	{ L::Rollbackable } -> std::convertible_to<bool>;

	{ c.isValid() } -> std::convertible_to<bool>;
	{ c.getArena() };

	// Frames and records.
	{ l.materializeScope(i, i) } -> std::convertible_to<Status>;
	{ c.activationFrame(i) } -> std::convertible_to<Addr>;
	{ c.frameFor(i, i) } -> std::convertible_to<Addr>;
	{ c.activationScope(i) } -> std::convertible_to<uint32_t>;
	{ c.getRecord(i, i) } -> std::convertible_to<Addr>;
	{ c.getState(i, i) } -> std::convertible_to<Addr>;
	{ c.getRecordIn(a, i) } -> std::convertible_to<Addr>;
	{ c.getStateIn(a, i) } -> std::convertible_to<Addr>;
	{ c.getStateType() } -> std::convertible_to<const value::ComponentType *>;

	// Activations.
	{ l.openActivation(i, i, i, key, i) } -> std::convertible_to<Status>;
	{ c.getActivationCount() } -> std::convertible_to<uint32_t>;
	{ c.readActivation(i) } -> std::convertible_to<ActivationData>;
	{ c.resolveScope(i, i) } -> std::convertible_to<uint32_t>;
	{ l.pushOpen(i) } -> std::convertible_to<Status>;
	{ l.popOpen(i) } -> std::convertible_to<bool>;
	{ c.getOpenCount() } -> std::convertible_to<uint32_t>;
	{ c.getOpenAt(i) } -> std::convertible_to<uint32_t>;
	{ c.forEachRecord(each) };

	// Node state.
	{ c.readState(a) } -> std::convertible_to<NodeStateData>;
	{ l.markInput(a, i) } -> std::convertible_to<Status>;
	{ l.markProduced(a, i) } -> std::convertible_to<Status>;
	{ l.addFlags(a, bits) } -> std::convertible_to<Status>;
	{ l.removeFlags(a, bits) } -> std::convertible_to<Status>;
	{ l.setStall(a, bits, i) } -> std::convertible_to<Status>;
	{ l.clearStall(a) } -> std::convertible_to<Status>;
	{ l.setTurns(a, i) } -> std::convertible_to<Status>;

	// The front, the stalled list, the counters.
	{ l.pushReady(i, i) } -> std::convertible_to<Status>;
	{ l.setWeighted(true) };
	{ l.popReady(i, i) } -> std::convertible_to<bool>;
	{ c.peekReady(i, i) } -> std::convertible_to<bool>;
	{ c.getReadyCount() } -> std::convertible_to<uint32_t>;
	{ l.pushStalled(i, i) } -> std::convertible_to<Status>;
	{ c.getStalledCount() } -> std::convertible_to<uint32_t>;
	{ c.getStalledAt(i) } -> std::convertible_to<uint64_t>;
	{ l.clearStalled() } -> std::convertible_to<Status>;
	{ c.getStep() } -> std::convertible_to<uint32_t>;
	{ l.setStep(i) } -> std::convertible_to<Status>;
	{ c.getPass() } -> std::convertible_to<uint32_t>;
	{ l.setPass(i) } -> std::convertible_to<Status>;

	{ l.destroy() };
};

} // namespace stappler::flow

#endif /* STAPPLER_FLOW_SPFLOWSTATIC_H_ */
