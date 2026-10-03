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

#ifndef STAPPLER_FLOW_SPFLOWMACHINE_HPP_
#define STAPPLER_FLOW_SPFLOWMACHINE_HPP_

// The machine: the bodies of MachineT<Graph, Local, Trace>, of resolveWatch, and of the two checks
// a run makes of its bindings before it starts. An .hpp rather than a .cc subunit, because they are
// instantiated in more than one compile unit: XSGraph.scu.cpp, for the built graph and the three
// arena kinds, and every generated unit, for the static graph it carries. The store's and the
// door's bodies are pulled in below for the same reason - a unit that instantiates the machine
// instantiates them. The algorithm is a fixed point implemented by push rather than by rescanning:
// a node is queued at the moment its last input arrives or its exec token does, so a pass over the
// stalled set never has anything to find. One sweep is kept at the end, and that sweep is where a
// run learns why it stopped - which nodes hold a token and lack data (a deadlock) and which never
// received a token at all (a branch that was not taken, and no error at all). The order is not an
// accident and it is observable, because the execution log is a golden in the tests. After a node
// runs, the exec targets it fired go onto the front in reverse declaration order, so the first
// declared exec output runs first and its whole subtree before the second; then the nodes that just
// became ready by data, in reverse index order, so they pop ascending; and the front is a stack, so
// what was pushed last runs first. Everything that became computable is computed before the chain
// of execution moves on, and the chain itself goes depth-first in the order the author sees on the
// canvas.

#include "SPFlowMachine.h"
#include "SPFlowContext.hpp"
#include "SPFlowLocal.hpp"
#include "SPFlowFast.hpp"
#include "SPFlowGpuJob.hpp"

#include <sprt/c/__sprt_string.h>

namespace STAPPLER_VERSIONIZED stappler::flow {

/* The one place a Watch becomes an address. The three cases differ in two things and agree on
everything else, which is the whole reason this is one function: which store the record lives in,
and how the record is found. What follows - the descriptor, the field, and the answer - is the same
sentence three times over. Nothing is written here and nothing is read: a target is where a value
is, and whether the caller then reads it or puts one there is the caller's business, which is what
makes it serve a breakpoint and an edit at once. */
template <typename Graph, typename Local>
WatchResolution resolveWatch(const Watch &watch, uint32_t activation, const Graph *graph,
		const Local *local, const typename Local::SceneType *scene, WatchTarget &out) {
	out = WatchTarget();
	if (watch.source == Watch::Source::None || watch.field.empty()) {
		return WatchResolution::NotAddressed;
	}

	if (watch.source == Watch::Source::Scene) {
		if (!scene || !scene->isValid()) {
			return WatchResolution::NoStore;
		}
		auto reg = scene->getRegistry();
		auto type = reg ? reg->get(watch.type) : nullptr;
		if (!type) {
			return WatchResolution::UnknownName;
		}
		// getComponent creates nothing, which is the difference between a watch and an input: the
		// environment leaves a component, and a debugger looks at one that is there.
		auto record = scene->getComponent(watch.entity, *type);
		if (record == NullAddr) {
			return WatchResolution::NoRecord;
		}
		auto field = type->getField(watch.field);
		if (!field) {
			return WatchResolution::UnknownField;
		}
		out = WatchTarget{WatchTarget::Store::Scene, record, type, field};
		return WatchResolution::Ok;
	}

	if (!graph) {
		return WatchResolution::NoStore;
	}
	auto index = graph->findNode(watch.node);
	if (index == InvalidIndex) {
		return WatchResolution::UnknownName;
	}
	// A local record exists only while a run does: begin() builds the store and reset() destroys
	// it, so "there is no run in progress" is a different answer from "that node has no record".
	if (!local || !local->isValid() || !local->getArena()) {
		return WatchResolution::NoStore;
	}

	const value::ComponentType *type = nullptr;
	Addr record = NullAddr;
	if (watch.source == Watch::Source::NodeRecord) {
		type = graph->getNodeAt(index).localSchema;
		record = local->getRecord(index, activation);
	} else {
		type = local->getStateType();
		record = local->getState(index, activation);
	}
	if (!type) {
		// The operation declares no record at all - a name that will never address anything here,
		// rather than one that does not address anything yet.
		return WatchResolution::UnknownName;
	}
	if (record == NullAddr) {
		return WatchResolution::NoRecord;
	}
	auto field = type->getField(watch.field);
	if (!field) {
		return WatchResolution::UnknownField;
	}
	out = WatchTarget{WatchTarget::Store::Local, record, type, field};
	return WatchResolution::Ok;
}

// A bound graph carries descriptors resolved against one registry, so it may only run against that
// one. The same argument that moved the pool geometry check into EntityStore::open: a descriptor
// that has drifted from the data it describes is a silent read at the wrong offset, and the one
// place to catch it cheaply is where the two first meet. The count is checked as well as the
// pointer, because the registry only ever grows - a registry that has grown may have grown a type
// this graph recorded as absent, and running against a "no" that has stopped being true is exactly
// the failure this prevents. Re-binding is the way through. A graph with no bindings runs against
// anything.
template <typename Graph, typename Scene>
inline bool sceneMatchesBinding(const Graph &graph, Scene *scene) {
	auto bound = graph.getSceneRegistry();
	if (!bound) {
		return true;
	}
	auto registry = scene && scene->isValid() ? scene->getRegistry() : nullptr;
	return registry == bound && registry->getCount() == graph.getSceneRegistryCount();
}

// The same, over the extensions, and the epoch is checked for a stronger reason than the registry's
// count: a holder can shrink. An instance that was dropped leaves a graph holding a pointer to an
// object that has been deleted, so this is not "a recorded absence may have stopped being true" but
// "a recorded presence may have". A graph bound to no holder runs against anything.
template <typename Graph>
inline bool extensionsMatchBinding(const Graph &graph, value::ExtensionHost *extensions) {
	auto bound = graph.getExtensions();
	if (!bound) {
		return true;
	}
	return extensions == bound && extensions->getEpoch() == graph.getExtensionsEpoch();
}

template <typename Graph, typename Local, typename Trace>
MachineT<Graph, Local, Trace>::~MachineT() {
	reset();
	clearJobPool();
}

template <typename Graph, typename Local, typename Trace>
void MachineT<Graph, Local, Trace>::reset() {
	// Workers read this run's scene and seeds; they are stopped before anything of it goes.
	abandonFlights();
	_branchMode = false;
	_seedActivation = RootActivation;
	_local.destroy();
	_graph = nullptr;
	_ops = nullptr;
	_scene = nullptr;
	_extensions = nullptr;
	_journal = nullptr;
	_state = RunState::Idle;
	_status = Status::Ok;
	_maxSteps = 0;
	_quantum = RollbackQuantum::Step;
	_quantumBase = 0;
	if constexpr (!Trace::Log) {
		_quantumStepCount = 0;
		_lastNode = InvalidIndex;
		_lastActivation = RootActivation;
	}

	/* The job pool stays too, and for the same kind of reason: a host calls reset() at the end of
	every tick, so a pool released here would be a pool that never survived a frame. What a
	pooled job holds after recycle() names nothing of the run that ended - its batch stores were
	destroyed, their graph pointers nulled, its view of the scene closed - and what it keeps is
	memory, which is exactly what it is for. The breakpoints stay for their own reason: they
	belong to whoever is debugging, not to the run. What goes below is the bookkeeping about the
	run that just ended. */
	_lastBreakpoint = 0;
	_stoppedBefore = false;
	for (auto &bp : _breakpoints) {
		bp.last = value::makeNil();
		bp.hasLast = false;
	}
}

template <typename Graph, typename Local, typename Trace>
uint32_t MachineT<Graph, Local, Trace>::requiredInputs(uint32_t node) const {
	return _graph->getNodeAt(node).allInputs;
}

// Where the value a consumer reads actually is. Same scope means the same activation; an enclosing
// scope means walking up the activation tree until the scopes match. Nothing is guessed and
// nothing is copied - the value lives where it was produced.
template <typename Graph, typename Local, typename Trace>
uint32_t MachineT<Graph, Local, Trace>::producerActivation(uint32_t srcNode, uint32_t consumerActivation) const {
	auto scope = _graph->getNodeAt(srcNode).scope;
	return _local.resolveScope(consumerActivation, scope);
}

template <typename Graph, typename Local, typename Trace>
bool MachineT<Graph, Local, Trace>::isReady(uint32_t node, uint32_t activation) const {
	return isReadyIn(_local.frameFor(node, activation), node, activation);
}

// With the activation's frame already in hand. Every caller that asks about several nodes of one
// activation - propagating to same-scope consumers, entering a body, sweeping one on the way out -
// takes the frame once and calls this, because finding it is two array reads and reading a record
// out of it is an addition.
template <typename Graph, typename Local, typename Trace>
bool MachineT<Graph, Local, Trace>::isReadyIn(Addr frame, uint32_t node, uint32_t activation) const {
	auto &rt = _graph->getNodeAt(node);
	if (!rt.op || frame == NullAddr) {
		return false;
	}
	auto state = _local.readState(_local.getStateIn(frame, node));
	if ((state.flags & (NodeFlags::Ran | NodeFlags::Queued)) != 0) {
		return false;
	}
	if (rt.op->hasExecIn() && (state.flags & NodeFlags::Token) == 0) {
		return false;
	}

	// An input from this scope is marked by the producer when it runs, so every one of them is a
	// single mask compare against a set the build worked out. An input from an enclosing scope
	// cannot be pushed - the consumer may not exist yet when the producer runs, and there may end
	// up being many of it - so it is read off the producer where it stands, and those edges are the
	// only ones worth walking. For almost every node there are none.
	if ((state.inputs & rt.sameScopeInputs) != rt.sameScopeInputs) {
		return false;
	}
	for (auto e : _graph->getCrossScopeInEdges(node)) {
		auto &edge = _graph->getDataEdges()[e];
		auto srcAct = producerActivation(edge.srcNode, activation);
		if (srcAct == NullActivation) {
			return false;
		}
		auto srcState = _local.readState(_local.getState(edge.srcNode, srcAct));
		if ((srcState.produced & (uint32_t(1) << edge.srcPin)) == 0) {
			return false;
		}
	}
	return true;
}

template <typename Graph, typename Local, typename Trace>
Status MachineT<Graph, Local, Trace>::enqueue(uint32_t node, uint32_t activation) {
	return enqueueIn(_local.frameFor(node, activation), node, activation);
}

template <typename Graph, typename Local, typename Trace>
Status MachineT<Graph, Local, Trace>::enqueueIn(Addr frame, uint32_t node, uint32_t activation) {
	auto st = _local.pushReady(node, activation);
	if (st != Status::Ok) {
		return st;
	}
	return _local.addFlags(_local.getStateIn(frame, node), NodeFlags::Queued);
}

// The two ways to perform one unit of work, and the choice between them. The interpreter has one:
// fill the door and call the operation through the registry's function pointer. A loaded unit may
// have a second - a compiled step, written by stappler_flow_codegen with this node's index a constant and
// the operation's body called directly rather than through the registry - and
// when the unit carries one, that is what runs. Both are handed the same site and both leave what
// the operation fired in it, so "the compiled run is the interpreted run" stays a property of the
// arrangement: one machine, one door (ContextT with two sites), one set of rules, and only the node
// index is known earlier. `if constexpr`, so a graph that has none - the built graph, which is the
// interpreter's whole world - compiles to exactly the code that was here before it.
template <typename Graph, typename Local, typename Trace>
Status MachineT<Graph, Local, Trace>::invokeAt(StepSite<Graph, Local> &site) {
	if constexpr (requires(const Graph &g) { g.template compiledStep<Local>(); }) {
		if (auto fn = _graph->template compiledStep<Local>()) {
			return fn(site);
		}
	}
	Context ctx;
	ctx.bind(site);
	auto invoke = site.op->getInvoke();
	auto st = invoke ? invoke(ctx) : Status::ErrorNotImplemented;
	ctx.finish(site);
	return st;
}

template <typename Graph, typename Local, typename Trace>
Status MachineT<Graph, Local, Trace>::step(uint32_t node, uint32_t activation, RunReport &report) {
	auto &rt = _graph->getNodeAt(node);

	StepSite<Graph, Local> site;
	site.graph = _graph;
	site.local = &_local;
	site.scene = _scene;
	site.op = rt.op;
	site.node = node;
	site.id = rt.id;
	site.activation = activation;
	// The frame once, then both records out of it by addition. This is the shape the frame layout
	// exists for.
	site.frame = _local.frameFor(node, activation);
	// Empty for an unbound graph and for a node that names nothing, which is what makes the bound
	// doors an addition rather than a replacement.
	site.sceneBind = _graph->getSceneBindings(node);
	site.extBind = _graph->getExtensionBindings(node);
	site.namedBind = _graph->getNamedBindings(node);
	uint32_t block = InvalidIndex;
	uint32_t branch = InvalidIndex;
	if (branchOf(node, activation, block, branch)) {
		site.block = block;
		site.branchActivation = branch;
		site.branchFrame = _local.activationFrame(branch);
		site.branchEntity = readBranchEntity(*_local.getArena(), site.branchFrame);
	}

	auto st = invokeAt(site);

	// The firing contract the build relies on: an exclusive operation fires at most one exec
	// output, and an output that opens a scope fires alone. A step that breaks it is the
	// operation's failure, the same under every representation.
	if (st == Status::Ok && site.fired != 0) {
		const bool several = (site.fired & (site.fired - 1)) != 0;
		if (several
				&& (rt.op->isExecExclusive() || (site.fired & rt.op->getScopeExecOut()) != 0)) {
			st = Status::ErrorInvalidArguemnt;
		}
	}

	// A step of a branch fails the branch, not the run: stepOnce says what that means.
	if (st != Status::Ok && site.block != InvalidIndex) {
		return st;
	}

	if (st != Status::Ok) {
		report.outcome = RunOutcome::OpError;
		// Through the one writer, with a code the reader knows: an entry written beside it carries
		// no severity, and an editor then shows it as an error of the graph rather than of the run.
		int64_t values[] = {int64_t(rt.id), int64_t(toInt(st))};
		StringView names[] = {rt.op->getName()};
		reportRun<EnvType>(report, DiagSeverity::Error, DiagCode::OpError,
				DiagText(DiagDetail::OpRefused).name(rt.op->getName()), DiagLocus::NodeOp, values,
				names);
		return st;
	}

	// The state address is re-read rather than reused: the operation may have grown a blob, and a
	// blob growing is an allocator call. Re-read from the frame, though, not by resolving
	// the activation again - a blob may move a record, and the frame it sits in it cannot.
	_local.addFlags(_local.getStateIn(site.frame, node), NodeFlags::Ran);

	if constexpr (Trace::Log) {
		RunStep record;
		record.step = report.stepCount;
		record.kind = RunStepKind::Node;
		record.node = node;
		record.activation = activation;
		record.id = rt.id;
		record.fired = site.fired;
		report.log.emplace_back(record);
	} else {
		_lastNode = node;
		_lastActivation = activation;
	}

	propagate(node, activation, site.fired, site.frame);
	return Status::Ok;
}

template <typename Graph, typename Local, typename Trace>
void MachineT<Graph, Local, Trace>::propagate(uint32_t node, uint32_t activation, uint32_t fired,
		Addr frame) {
	auto &rt = _graph->getNodeAt(node);

	// Every same-scope consumer of this node lives in this activation, so they all share one frame
	// - this node's. The unit that called us had already resolved it once, which is why it is
	// handed over rather than found again.
	auto state = _local.readState(_local.getStateIn(frame, node));

	// A pair rather than an index now: with loops the same node is on the front more than once, in
	// different iterations.
	mem_std::Vector<uint64_t> dataReady;
	mem_std::Vector<uint64_t> execReady;

	// Data first: every output that was actually produced marks its consumers' inputs.
	for (auto e : _graph->getDataOutEdges(node)) {
		auto &edge = _graph->getDataEdges()[e];
		if ((state.produced & (uint32_t(1) << edge.srcPin)) == 0) {
			continue; // the operation left this output unwritten; the consumer keeps waiting
		}

		auto &dst = _graph->getNodeAt(edge.dstNode);
		if (dst.op && edge.dstPin == dst.op->getBranchPin()) {
			continue; // a collector's input arrives with the block's delivery, whatever feeds it
		}
		auto dstScope = dst.scope;
		if (dstScope == rt.scope) {
			_local.markInput(_local.getStateIn(frame, edge.dstNode), edge.dstPin);
			if (isReadyIn(frame, edge.dstNode, activation)) {
				dataReady.emplace_back(makeRecordKey(edge.dstNode, activation));
			}
			continue;
		}

		// The consumer is in a parallel body whose fan-out sits here: the fan-out waits for every
		// value its body reads from outside, so this may be the one it was waiting for.
		auto branchScope = _graph->getScopeAt(dstScope).branchScope;
		if (branchScope != InvalidIndex) {
			auto fanOut = _graph->getScopeAt(branchScope).opener;
			auto fanOutScope = _graph->getNodeAt(fanOut).scope;
			if (fanOutScope == rt.scope) {
				if (isReadyIn(frame, fanOut, activation)) {
					dataReady.emplace_back(makeRecordKey(fanOut, activation));
				}
			} else {
				auto count = _local.getOpenCount();
				for (uint32_t i = 0; i < count; ++i) {
					auto open = _local.getOpenAt(i);
					if (_local.activationScope(open) == fanOutScope
							&& _local.resolveScope(open, rt.scope) == activation
							&& isReady(fanOut, open)) {
						dataReady.emplace_back(makeRecordKey(fanOut, open));
					}
				}
			}
		}

		// The consumer is deeper - inside a loop this node feeds. Its readiness is read off this
		// record rather than pushed into it (there may be no such activation yet, and later there
		// may be many), so all that is left to do is wake the iterations already running. The count
		// is read once, because the loop cannot change it, and the scope is asked for directly
		// rather than through readActivation, which fetches four fields to use one.
		auto openCount = _local.getOpenCount();
		for (uint32_t i = 0; i < openCount; ++i) {
			auto open = _local.getOpenAt(i);
			if (_local.activationScope(open) != dstScope) {
				continue;
			}
			if (_local.resolveScope(open, rt.scope) != activation) {
				continue; // an iteration of the same loop, but not one of ours
			}
			if (isReady(edge.dstNode, open)) {
				dataReady.emplace_back(makeRecordKey(edge.dstNode, open));
			}
		}
	}

	for (auto e : _graph->getExecOutEdges(node)) {
		auto &edge = _graph->getExecEdges()[e];
		if ((fired & (uint32_t(1) << edge.srcPin)) == 0) {
			continue; // an exec output that did not fire is a branch not taken, not a failure
		}

		if (rt.op && rt.op->opensScope(edge.srcPin)
				&& _graph->getScopeAt(rt.opensScope).kind == ScopeKind::Parallel) {
			openBlock(node, activation, rt.opensScope, edge.dstNode, execReady);
			continue;
		}

		if (rt.op && rt.op->opensScope(edge.srcPin)) {
			// A turn of a loop. The iteration is opened here rather than by the operation, because
			// the operation is not supposed to know that loops exist - it fires an output, exactly
			// as `branch` does.
			openIteration(node, activation, rt.opensScope, edge.dstNode, execReady);
			continue;
		}

		auto target = edge.dstNode;
		auto targetScope = _graph->getNodeAt(target).scope;
		auto targetAct = activation;
		if (targetScope != rt.scope) {
			// Leaving a body: the only legal destination is the node that opened it or one further
			// out, and the build has already refused everything else (ScopeEscape). Continuing the
			// loop is the closing sweep's job, not this token's - so the edge is allowed and does
			// nothing, and an author who wires the tail back by hand gets the same run as one who
			// does not.
			continue;
		}

		// A token is permission to run, and it is permission every time it arrives. That is what
		// makes a cycle over exec edges a repeat: the whole cycle re-runs, not only the one node
		// the back edge points at, and marking just the back edge's target would leave the rest of
		// the cycle stuck at "already ran" after one lap. It costs nothing in a graph without
		// cycles: a node gets its token once, and two tokens arriving in the same step find it
		// queued rather than run. Same scope, same activation, therefore the same frame this
		// function already holds.
		auto targetState = _local.getStateIn(frame, target);
		if ((_local.readState(targetState).flags & NodeFlags::Ran) != 0) {
			_local.removeFlags(targetState, NodeFlags::Ran | NodeFlags::Queued);
		}
		_local.addFlags(targetState, NodeFlags::Token);
		if (isReadyIn(frame, target, targetAct)) {
			execReady.emplace_back(makeRecordKey(target, targetAct));
		}
	}

	// Exec targets go in first and in reverse declaration order, data-ready nodes on top of them in
	// reverse index order: the stack then hands back the computable work first and the first
	// declared branch before the second. Enqueued into the frame in hand when the key names this
	// activation, and resolved when it does not. The activation is the whole of the test and
	// neither list is exempt from it: an exec target reached along an edge is same-scope and
	// same-activation, but `openIteration` fills the same list with the body nodes of a turn it
	// just opened, and those live in a frame of their own. Enqueuing one of those into this frame
	// writes its Queued flag at an address belonging to another node, which is a corrupt record
	// rather than a wrong answer.
	auto enqueueKey = [&](uint64_t key) {
		auto act = recordKeyActivation(key);
		if (act == activation) {
			enqueueIn(frame, recordKeyNode(key), act);
		} else {
			enqueue(recordKeyNode(key), act);
		}
	};
	for (uint32_t i = uint32_t(execReady.size()); i > 0; --i) { enqueueKey(execReady[i - 1]); }
	for (uint32_t i = uint32_t(dataReady.size()); i > 0; --i) { enqueueKey(dataReady[i - 1]); }
}

// Opens one turn of a loop: a fresh activation, a fresh record for every node of the body, and the
// token on the node the body starts at. Every node of the body that is already computable goes on
// the front with it - their values come from outside the loop and are read, never pushed.
template <typename Graph, typename Local, typename Trace>
void MachineT<Graph, Local, Trace>::openIteration(uint32_t opener, uint32_t activation, uint32_t scope,
		uint32_t firstNode, mem_std::Vector<uint64_t> &ready) {
	if (scope == InvalidIndex || _local.getActivationCount() >= _maxActivations) {
		_activationsExhausted = _activationsExhausted || scope != InvalidIndex;
		return;
	}

	// A loop inside a branch spends the branch's activations.
	uint32_t block = InvalidIndex;
	uint32_t branch = InvalidIndex;
	if (branchOf(opener, activation, block, branch)) {
		auto header = _local.activationFrame(branch);
		auto spent = readBranchInt(*_local.getArena(), header, BranchHeader::ActivationsOffset);
		if (uint32_t(spent) >= _graph->getBlockAt(block).maxActivations) {
			_budgetFailure = BudgetFailure{opener, activation, block, branch};
			return;
		}
		writeBranchInt(*_local.getArena(), header, BranchHeader::ActivationsOffset, spent + 1);
	}

	// The turn number: how many turns this record has already opened. Kept on the opener's record
	// rather than counted, because counting means walking every activation opened so far and asking
	// which named this one - a scan whose cost grows with the number of turns, on every turn. The
	// record is addressed by (opener, activation), so an opener re-entered from the next turn of an
	// enclosing loop is a different record and starts at zero.
	auto openerState = _local.getState(opener, activation);
	auto iteration = _local.readState(openerState).turns;

	uint32_t opened = 0;
	if (_local.openActivation(scope, activation, iteration, makeRecordKey(opener, activation),
				opened)
			!= Status::Ok) {
		return;
	}
	if (_local.materializeScope(scope, opened) != Status::Ok) {
		return;
	}
	_local.pushOpen(opened);

	// After everything that could allocate: opening an activation and materializing its records
	// both grow the arena, and a record address does not survive that.
	_local.setTurns(_local.getState(opener, activation), iteration + 1);

	// The new turn's frame, once. Every node of the body lives in it, so the readiness sweep below
	// is an addition per node instead of two array reads per node.
	auto frame = _local.activationFrame(opened);
	_local.addFlags(_local.getStateIn(frame, firstNode), NodeFlags::Token);
	for (auto n : _graph->getScopeNodes(scope)) {
		if (isReadyIn(frame, n, opened)) {
			ready.emplace_back(makeRecordKey(n, opened));
		}
	}
}

// The ready front drained and an iteration is still open. Close it and hand control back to the
// node that opened it, with its Ran flag cleared so that it can decide: another turn, or
// `completed`. This is why an author does not wire the body's tail back to the loop - a body that
// ends in a branch has two tails, and a branch that did not fire has none, so "the work of this
// iteration ran out" is the only signal that is always right.
template <typename Graph, typename Local, typename Trace>
bool MachineT<Graph, Local, Trace>::closeActivation(RunReport &report) {
	auto openCount = _local.getOpenCount();
	if (openCount == 0 || closeBlocked(_local.getOpenAt(openCount - 1))) {
		return false; // a turn holding a block in flight waits for its delivery
	}
	uint32_t activation = 0;
	if (!_local.popOpen(activation)) {
		return false;
	}

	auto data = _local.readActivation(activation);
	auto &closedScope = _graph->getScopeAt(data.scope);
	const bool branch = closedScope.kind == ScopeKind::Parallel;
	bool failed = false;
	if (branch) {
		auto header = _local.activationFrame(activation);
		auto status = readBranchInt(*_local.getArena(), header, BranchHeader::StatusOffset);
		failed = (status & BranchHeader::Failed) != 0;
		writeBranchInt(*_local.getArena(), header, BranchHeader::StatusOffset,
				status | BranchHeader::Closed);
	}

	// Anything in this iteration still waiting for a value will never get it: the iteration is
	// over. One frame for the whole sweep - every node of the scope is in the activation being
	// closed.
	auto frame = _local.activationFrame(activation);
	for (auto n : _graph->getScopeNodes(data.scope)) {
		auto stateAddr = _local.getStateIn(frame, n);
		auto state = _local.readState(stateAddr);
		if ((state.flags & NodeFlags::Ran) != 0 || (state.flags & NodeFlags::Token) == 0) {
			continue;
		}
		if (isReadyIn(frame, n, activation) || failed) {
			continue;
		}
		_local.setStall(stateAddr, NodeFlags::StallData, 0);
		_local.pushStalled(n, activation);
		int64_t values[] = {int64_t(_graph->getNodeAt(n).id), int64_t(data.iteration)};
		reportRun<EnvType>(report, DiagSeverity::Error, DiagCode::Deadlock,
				DiagText(DiagDetail::Deadlock), DiagLocus::NodeIteration, values);
	}

	auto opener = recordKeyNode(data.openerKey);
	auto openerAct = recordKeyActivation(data.openerKey);
	if (branch && _branchMode) {
		// A batch's branch: the machine that fired the block counts it when it takes the batch
		// back.
	} else if (branch) {
		// A branch hands nothing back to its fan-out: when the last one closes, the block is ready.
		auto closed = readFanOut(opener, openerAct, FanOutLocal::Closed) + 1;
		writeFanOut(opener, openerAct, FanOutLocal::Closed, closed);
		auto record = _local.getRecord(opener, openerAct);
		auto shape = fanOutLocalShape(_graph->getNodeAt(opener), FanOutLocal::Entities);
		auto count =
				value::blob::arrayCount(*_local.getArena(), record + shape.offset, shape.element);
		if (closed >= int64_t(count)) {
			writeFanOut(opener, openerAct, FanOutLocal::Phase, FanOutLocal::Ready);
			submitBlock(opener, openerAct);
		}
	} else {
		_local.removeFlags(_local.getState(opener, openerAct), NodeFlags::Ran | NodeFlags::Queued);
		enqueue(opener, openerAct);
	}

	if constexpr (Trace::Log) {
		RunStep record;
		record.step = report.stepCount;
		record.kind = RunStepKind::CloseActivation;
		record.node = opener;
		record.activation = activation;
		record.id = _graph->getNodeAt(opener).id;
		report.log.emplace_back(record);
	} else {
		_lastNode = opener;
		_lastActivation = activation;
	}
	return true;
}

template <typename Graph, typename Local, typename Trace>
void MachineT<Graph, Local, Trace>::collectBlocked(RunReport &report) {
	_local.clearStalled();

	// Over every record, not over the nodes: with loops a node has a record per iteration, and a
	// node that stalled in iteration three is not the same fact as the same node in iteration four.
	// The enumeration is the activations and their scopes, which is where the answer lives.
	_local.forEachRecord([&](uint32_t n, uint32_t activation, Addr frame) {
		if (n >= _graph->getNodeCount()) {
			return true;
		}
		auto &rt = _graph->getNodeAt(n);
		if (!rt.op) {
			return true;
		}
		auto stateAddr = _local.getStateIn(frame, n);
		auto state = _local.readState(stateAddr);
		if ((state.flags & NodeFlags::Ran) != 0) {
			return true;
		}

		// A failed branch stopped on purpose; what it left waiting is not a deadlock.
		uint32_t block = InvalidIndex;
		uint32_t branch = InvalidIndex;
		if (branchOf(n, activation, block, branch)
				&& (readBranchInt(*_local.getArena(), _local.activationFrame(branch),
							BranchHeader::StatusOffset)
						   & BranchHeader::Failed)
						!= 0) {
			return true;
		}

		// A barrier whose block was never delivered waits for a delivery, not for anything it could
		// be blamed for.
		if ((state.flags & NodeFlags::StallExternal) != 0) {
			return true;
		}

		// Never told to run. Legal and common: the branch was not taken, and the nodes behind it
		// simply end the path.
		if (rt.op->hasExecIn() && (state.flags & NodeFlags::Token) == 0) {
			_local.setStall(stateAddr, NodeFlags::StallExec, 0);
			return true;
		}

		auto required = requiredInputs(n);
		auto missing = required & ~state.inputs;
		if (missing == 0 && isReadyIn(frame, n, activation)) {
			return true; // ready and unrun can only mean the run stopped early
		}
		if (missing == 0) {
			// Everything from this scope arrived; what is missing comes from outside the loop.
			missing = required;
		}

		uint32_t pin = 0;
		while (pin < MaxDataPins && (missing & (uint32_t(1) << pin)) == 0) { ++pin; }
		_local.setStall(stateAddr, NodeFlags::StallData, pin);

		// A node holding a token and missing a value is the deadlock the document defines: it was
		// asked to run and never can. A node with no token is not - nobody asked.
		if (!rt.op->hasExecIn()) {
			return true;
		}

		_local.pushStalled(n, activation);
		int64_t values[] = {int64_t(rt.id)};
		StringView names[] = {rt.op->getDataIn()[pin].name};
		reportRun<EnvType>(report, DiagSeverity::Error, DiagCode::Deadlock,
				DiagText(DiagDetail::DeadlockInput), DiagLocus::Pin, values, names);
		return true;
	});
}

template <typename Graph, typename Local, typename Trace>
void MachineT<Graph, Local, Trace>::conclude(RunReport &report, RunOutcome outcome, Status status) {
	report.outcome = outcome;
	_status = status;
	_state = RunState::Finished;
}

template <typename Graph, typename Local, typename Trace>
Status MachineT<Graph, Local, Trace>::begin(const Graph &graph, const OpRegistry &ops, ArenaType &arena,
		const Config &config, RunReport &report) {
	report = RunReport();
	reset();

	if (!graph.isValid() || !sceneMatchesBinding(graph, config.scene)
			|| !extensionsMatchBinding(graph, config.extensions)) {
		report.outcome = RunOutcome::Invalid;
		return Status::ErrorInvalidArguemnt;
	}

	// A store that cannot undo a unit of work cannot be run under a quantum that promises one. The
	// journal is still accepted - it watches the scene, and a host whose undo is "the last frame"
	// wants exactly that - but only at `Run`, where the boundary is the frame and not the step.
	// Refused here rather than at the stepBack that would have failed later: a run that took three
	// hundred boundaries and then could not use one of them has paid for nothing.
	if constexpr (!Local::Rollbackable) {
		if (config.journal && config.quantum != RollbackQuantum::Run) {
			report.outcome = RunOutcome::Invalid;
			reportRun<EnvType>(report, DiagSeverity::Error, DiagCode::CodegenQuantumUnsupported,
					DiagText(DiagDetail::QuantumUnsupported)
							.name(getRollbackQuantumName(RollbackQuantum::Run))
							.name(getRollbackQuantumName(config.quantum)));
			return Status::ErrorNotSupported;
		}
	}

	auto st = _local.init(arena, graph, ops.getLocalTypes());
	if (st != Status::Ok) {
		report.outcome = RunOutcome::Invalid;
		return st;
	}

	_graph = &graph;
	_ops = &ops;
	_scene = config.scene;
	_extensions = config.extensions;
	// A journal with no store attached is not a journal: it would open and close versions that
	// close over nothing, at 1.4 microseconds a page for pages nobody is watching.
	_journal = config.journal && config.journal->getStoreCount() > 0 ? config.journal : nullptr;
	_maxSteps = config.maxSteps ? config.maxSteps : graph.getNodeCount() * 4 + 16;
	_maxActivations = config.maxActivations ? config.maxActivations : graph.getNodeCount() * 8 + 64;
	_quantum = config.quantum;
	_quantumBase = 0;
	_activationsExhausted = false;
	_status = Status::Ok;
	_state = RunState::Paused;
	_executor = config.executor;
	_clock = config.clock;
	_frontWeights = config.frontWeights;
	_local.setWeighted(config.frontWeights);
	_pending.clear();
	_flights.clear();
	_hasPolled = false;
	// A new run: tickets an executor still holds from the last one name activations this run
	// reuses.
	++_generation;
	_branchUnits = 0;
	_quantumBranchUnits = 0;
	prepareParallel();
	if constexpr (ArenaType::HasShadow) {
		// Under an executor the scene may be read on other threads; it is written on this one only.
		if (_executor && _scene && _scene->getArena()) {
			_scene->getArena()->setOwnerThread();
		}
	}

	// The entry points, pushed in reverse so the front hands them back in index order.
	auto entries = graph.getEntryNodes();
	for (uint32_t i = uint32_t(entries.size()); i > 0; --i) {
		enqueue(entries[i - 1], RootActivation);
	}

	return Status::Ok;
}

template <typename Graph, typename Local, typename Trace>
Status MachineT<Graph, Local, Trace>::attach(const Graph &graph, const OpRegistry &ops, ArenaType &arena,
		const Config &config, RunReport &report) {
	reset();

	if (!graph.isValid() || !sceneMatchesBinding(graph, config.scene)
			|| !extensionsMatchBinding(graph, config.extensions)) {
		report.outcome = RunOutcome::Invalid;
		return Status::ErrorInvalidArguemnt;
	}

	auto st = _local.open(arena, graph, ops.getLocalTypes());
	if (st != Status::Ok) {
		report.outcome = RunOutcome::Invalid;
		return st;
	}

	_graph = &graph;
	_ops = &ops;
	_scene = config.scene;
	_extensions = config.extensions;
	// A journal with no store attached is not a journal: it would open and close versions that
	// close over nothing, at 1.4 microseconds a page for pages nobody is watching.
	_journal = config.journal && config.journal->getStoreCount() > 0 ? config.journal : nullptr;
	_maxSteps = config.maxSteps ? config.maxSteps : graph.getNodeCount() * 4 + 16;
	_maxActivations = config.maxActivations ? config.maxActivations : graph.getNodeCount() * 8 + 64;
	_quantum = config.quantum;
	_quantumBase = 0;
	_activationsExhausted = false;
	_status = Status::Ok;
	_state = RunState::Paused;
	_executor = config.executor;
	_clock = config.clock;
	_frontWeights = config.frontWeights;
	_local.setWeighted(config.frontWeights);
	prepareParallel();
	if constexpr (ArenaType::HasShadow) {
		if (_executor && _scene && _scene->getArena()) {
			_scene->getArena()->setOwnerThread();
		}
	}
	_branchUnits = 0;
	if constexpr (Trace::Log) {
		// The units a branch took do not count against the run's ceiling, and which those were is
		// in the report the first half of the run left.
		for (auto &it : report.log) {
			_branchUnits += isBranchUnit(it) ? 1 : 0;
		}
	}
	_quantumBranchUnits = _branchUnits;
	refreshPending();

	// No entry points are seeded: whoever ran the first half of this run already did that, and the
	// front they left behind is in the arena.
	report.outcome = RunOutcome::Invalid;
	return Status::Ok;
}

// The run's very first unit always takes one: with no base there is nothing an undo could return
// to, and a version of zero is not a version.
template <typename Graph, typename Local, typename Trace>
bool MachineT<Graph, Local, Trace>::needsBoundary(uint32_t node) const {
	if (_quantumBase == 0) {
		return true;
	}
	switch (_quantum) {
	case RollbackQuantum::Step: return true;
	case RollbackQuantum::Run: return false;
	case RollbackQuantum::Failable: break;
	}

	// Closing a loop iteration runs no operation; it moves the front and cannot fail.
	if (node == InvalidIndex) {
		return false;
	}
	auto op = _graph->getNodeAt(node).op;
	return op == nullptr || (op->getFlags() & OpFlags::Infallible) == OpFlags::None;
}

template <typename Graph, typename Local, typename Trace>
uint64_t MachineT<Graph, Local, Trace>::peekNext() const {
	if (_state != RunState::Paused) {
		return NullRecordKey;
	}
	// A delivery the serial executor would make next is named by its barrier.
	if (!_executor && !_pending.empty()) {
		auto &fanOut = _graph->getNodeAt(_pending.front().node);
		return makeRecordKey(_graph->getScopeAt(fanOut.opensScope).barrier, _pending.front().activation);
	}
	// Looks and does nothing else: the boundary belongs to the step that decides it needs one, and
	// a query has no business taking one.

	uint32_t node = 0;
	uint32_t activation = RootActivation;
	if (!_local.peekReady(node, activation)) {
		return NullRecordKey;
	}
	return makeRecordKey(node, activation);
}

template <typename Graph, typename Local, typename Trace>
bool MachineT<Graph, Local, Trace>::stepOnce(RunReport &report) {
	if ((_state == RunState::Paused || _state == RunState::Suspended) && !checkTimeouts(report)) {
		return false;
	}

	// A suspended run is waiting for a delivery; it goes on the moment one is there.
	BlockTicket ticket;
	bool delivering = false;
	if (_state == RunState::Suspended) {
		if (!nextDelivery(ticket)) {
			return false;
		}
		delivering = true;
		_state = RunState::Paused;
	}
	if (_state != RunState::Paused) {
		return false;
	}

	// An explicit step is a step over: it settles whatever BeforeStep breakpoint finish() stopped
	// on, so the next finish() tests the position it has actually arrived at.
	_stoppedBefore = false;

	// A delivery comes before anything on the front: under `serial` it is the unit right after the
	// last branch closed.
	delivering = delivering || nextDelivery(ticket);

	// Which unit is next has to be known before the boundary can be decided, so the front is looked
	// at first. Looking writes nothing, so there is still nothing on this side of the boundary that
	// ought to be on the other.
	uint32_t node = 0;
	uint32_t activation = RootActivation;
	bool haveNode = !delivering && _local.peekReady(node, activation);

	// A unit that would change the scene's structure waits while branches read it on other threads.
	if (!delivering && haveNode && _threaded > 0 && node < _freezes.size() && _freezes[node]) {
		_state = RunState::Suspended;
		return false;
	}

	// Nothing to run, nothing to close, and a block still out: the run waits rather than ends.
	if (!delivering && !haveNode && (!_pending.empty() || _threaded > 0)) {
		auto openCount = _local.getOpenCount();
		if (openCount == 0 || closeBlocked(_local.getOpenAt(openCount - 1))) {
			_state = RunState::Suspended;
			return false;
		}
	}

	// A boundary is opened before anything happens, so that a whole unit of work - including
	// closing an iteration, which runs no node but moves the front - is inside one. Whether it is
	// opened at all is the quantum's business. A delivery writes the scene, so it takes one unless
	// the quantum is the whole run.
	bool boundary = hasJournal()
			&& (delivering ? (_quantumBase == 0 || _quantum != RollbackQuantum::Run)
						   : needsBoundary(haveNode ? node : InvalidIndex));
	if (boundary) {
		_quantumBase = openVersion();
		_quantumBranchUnits = _branchUnits;
		if constexpr (!Trace::Log) {
			_quantumStepCount = report.stepCount;
		}
	}
	value::Version base = _quantumBase;

	// The unit is over and succeeded: count it, stamp it, close its version, tell the executor.
	auto settle = [&](bool branchUnit) {
		drainFailed();
		if (branchUnit) {
			++_branchUnits;
		}
		++report.stepCount;
		_local.setStep(report.stepCount);
		if (hasJournal()) {
			if constexpr (Trace::Log) {
				report.log.back().baseVersion = base;
				if (boundary) {
					report.log.back().version = closeVersionAndNotify();
				}
			} else if (boundary) {
				closeVersionAndNotify();
			}
		}
		if (_executor) {
			_executor->tick();
		}
	};

	if (delivering) {
		_hasPolled = false;
		auto st = deliver(ticket, report);
		if (st != Status::Ok) {
			if (hasJournal()) {
				abandonFlights();
				undoToAndNotify(base);
				refreshPending();
			}
			conclude(report, RunOutcome::OpError, st);
			return false;
		}
		settle(false);
		return true;
	}

	if (!haveNode) {
		// The front drained. If an iteration is still running, that is the signal it has run out of
		// work: close it and hand its opener control back, which is one unit in its own right so a
		// debugger can stop on the boundary between two turns. Closing an activation inside a
		// branch spends the branch's work, not the run's.
		auto openCount = _local.getOpenCount();
		const bool branchClose = openCount > 0
				&& _graph->getScopeAt(_local.activationScope(_local.getOpenAt(openCount - 1))).branchScope
						!= InvalidIndex;
		if ((branchClose || report.stepCount - _branchUnits < _maxSteps) && closeActivation(report)) {
			settle(branchClose);
			return true;
		}

		// A batch of branches is over when its branches are: the sweep is the machine's that fired
		// them, over the records it takes back.
		if (_branchMode) {
			conclude(report, RunOutcome::Completed, Status::Ok);
			return false;
		}

		// The one sweep. Under the push discipline it finds nothing new to run - it is here to say
		// why the run stopped.
		report.sweepCount = 1;
		_local.setPass(report.sweepCount);
		collectBlocked(report);

		if (_activationsExhausted) {
			int64_t values[] = {int64_t(_maxActivations)};
			reportRun<EnvType>(report, DiagSeverity::Error, DiagCode::ActivationLimit,
					DiagText(DiagDetail::ActivationLimit).number(int64_t(_maxActivations)),
					DiagLocus::Limit, values);
			conclude(report, RunOutcome::ActivationLimit, Status::ErrorInvalidArguemnt);
			return false;
		}

		auto deadlocked = _local.getStalledCount() > 0;
		conclude(report, deadlocked ? RunOutcome::Deadlock : RunOutcome::Completed,
				deadlocked ? Status::ErrorInvalidArguemnt : Status::Ok);
		return false;
	}

	// A unit of a branch spends the branch's budget, not the run's.
	uint32_t block = InvalidIndex;
	uint32_t branch = InvalidIndex;
	const bool inBranch = branchOf(node, activation, block, branch);
	if (inBranch) {
		auto header = _local.activationFrame(branch);
		auto steps = readBranchInt(*_local.getArena(), header, BranchHeader::StepsOffset);
		if (uint32_t(steps) >= _graph->getBlockAt(block).maxSteps) {
			_local.popReady(node, activation);
			_local.removeFlags(_local.getState(node, activation), NodeFlags::Queued);
			failBranch(node, activation, block, branch, Status::ErrorInvalidArguemnt, true, true, report);
			settle(true);
			return true;
		}
		writeBranchInt(*_local.getArena(), header, BranchHeader::StepsOffset, steps + 1);
	} else if (report.stepCount - _branchUnits >= _maxSteps) {
		// The ceiling is tested before the node leaves the front, not after. A run stopped by its
		// own ceiling has to be a run somebody can look at and carry on from, and popping the node
		// and clearing its Queued flag first would leave the store in a shape that could neither be
		// resumed nor honestly rolled back.
		int64_t values[] = {int64_t(_maxSteps)};
		reportRun<EnvType>(report, DiagSeverity::Error, DiagCode::StepLimit,
				DiagText(DiagDetail::StepLimit).number(int64_t(_maxSteps)), DiagLocus::Limit,
				values);
		_local.setStep(report.stepCount);
		conclude(report, RunOutcome::StepLimit, Status::ErrorInvalidArguemnt);
		return false;
	}

	// A failing node rolls back to "before entering the node", and taking it off the front is part
	// of entering it - so the boundary opened above is what an undo returns to, and the node goes
	// back on the front.
	_local.popReady(node, activation);
	_local.removeFlags(_local.getState(node, activation), NodeFlags::Queued);

	_budgetFailure = BudgetFailure();
	auto st = step(node, activation, report);
	if (st == Status::Ok) {
		if (_budgetFailure.node != InvalidIndex) {
			auto failure = _budgetFailure;
			_budgetFailure = BudgetFailure();
			failBranch(failure.node, failure.activation, failure.block, failure.branch,
					Status::ErrorInvalidArguemnt, true, false, report);
		}
		settle(inBranch);
		return true;
	}

	// A failed step of a branch is not undone: the branch wrote only its own records and its
	// copies, which nobody commits or reads once it has failed.
	if (inBranch) {
		failBranch(node, activation, block, branch, st, false, true, report);
		settle(true);
		return true;
	}

	if (hasJournal()) {
		// The whole quantum, not the part that failed: the front, the record, the flags and
		// whatever the operation managed to write to the scene, all at once. Under Step that
		// is exactly the failing unit; under a coarser quantum it is every unit back to the last
		// boundary - a state the store really had - and the log is truncated to match, because a
		// log entry for work that has been undone is a lie about where the run is. rollback() turns
		// whatever is still only marked into records itself, so there is nothing to close first.
		abandonFlights();
		undoToAndNotify(base);
		if constexpr (Trace::Log) {
			while (!report.log.empty() && report.log.back().baseVersion == base) {
				report.log.pop_back();
			}
			report.stepCount = uint32_t(report.log.size());
		} else {
			// No log to truncate: the units since the boundary are exactly the ones counted
			// since it was opened, and that number was kept for this line.
			report.stepCount = _quantumStepCount;
		}
		_branchUnits = _quantumBranchUnits;
		_local.setStep(report.stepCount);
		resampleWatches();
		refreshPending();
	}

	// step() has already named the outcome and written the diagnostic; what it could not do is
	// hand back the operation's own status, and that is what a caller reads.
	conclude(report, RunOutcome::OpError, st);
	return false;
}

template <typename Graph, typename Local, typename Trace>
bool MachineT<Graph, Local, Trace>::isBranchUnit(const RunStep &record) const {
	switch (record.kind) {
	case RunStepKind::Deliver: return false;
	case RunStepKind::CloseActivation:
		return _graph->getScopeAt(_local.activationScope(record.activation)).branchScope != InvalidIndex;
	case RunStepKind::Node:
	case RunStepKind::BranchFailed: {
		uint32_t block = InvalidIndex;
		uint32_t branch = InvalidIndex;
		return branchOf(record.node, record.activation, block, branch);
	}
	}
	return false;
}

template <typename Graph, typename Local, typename Trace>
bool MachineT<Graph, Local, Trace>::branchOf(uint32_t node, uint32_t activation, uint32_t &block,
		uint32_t &branch) const {
	auto &scope = _graph->getScopeAt(_graph->getNodeAt(node).scope);
	if (scope.branchScope == InvalidIndex) {
		return false;
	}
	branch = _local.resolveScope(activation, scope.branchScope);
	block = _graph->getScopeAt(scope.branchScope).block;
	return branch != NullActivation && block != InvalidIndex;
}

template <typename Graph, typename Local, typename Trace>
int64_t MachineT<Graph, Local, Trace>::readFanOut(uint32_t fanOut, uint32_t activation,
		uint32_t local) const {
	auto shape = fanOutLocalShape(_graph->getNodeAt(fanOut), local);
	Var value;
	if (!shape.valid
			|| readRecordField(*_local.getArena(), _local.getRecord(fanOut, activation), shape, value)
					!= Status::Ok) {
		return 0;
	}
	return value.i;
}

template <typename Graph, typename Local, typename Trace>
void MachineT<Graph, Local, Trace>::writeFanOut(uint32_t fanOut, uint32_t activation, uint32_t local,
		int64_t value) {
	auto shape = fanOutLocalShape(_graph->getNodeAt(fanOut), local);
	if (shape.valid) {
		writeRecordField(*_local.getArena(), _local.getRecord(fanOut, activation), shape,
				value::makeInt(value));
	}
}

template <typename Graph, typename Local, typename Trace>
void MachineT<Graph, Local, Trace>::openBlock(uint32_t fanOut, uint32_t activation, uint32_t scope,
		uint32_t firstNode, mem_std::Vector<uint64_t> &ready) {
	auto &blockScope = _graph->getScopeAt(scope);
	auto barrier = blockScope.barrier;

	// The set, read before anything allocates: an allocation may move the record it lives in.
	mem_std::Vector<value::EntityId> entities;
	{
		auto record = _local.getRecord(fanOut, activation);
		auto shape = fanOutLocalShape(_graph->getNodeAt(fanOut), FanOutLocal::Entities);
		auto arena = _local.getArena();
		auto count = value::blob::arrayCount(*arena, record + shape.offset, shape.element);
		for (uint32_t i = 0; i < count; ++i) {
			Var item;
			value::blob::arrayGet(*arena, record + shape.offset, shape.element, i, item);
			entities.emplace_back(value::EntityId::unpack(item.ent.id));
		}
	}

	// The whole block against the limit, before the first branch opens: half a block is not a
	// block.
	if (_local.getActivationCount() + uint32_t(entities.size()) > _maxActivations) {
		_activationsExhausted = true;
		return;
	}

	// Whether an executor takes the branches is asked before they open: its branches are not this
	// machine's work, and do not go on its open stack or its front.
	BlockLaunch launch;
	if (_executor && !_branchMode && !entities.empty()) {
		launch = _executor->chooseLaunch(BlockTicket{fanOut, activation, _generation},
				makeLaunchInfo(blockScope.block, uint32_t(entities.size())));
	}
	const bool threaded = launch.kind != BlockLaunchKind::Machine;

	uint32_t first = _local.getActivationCount();
	for (uint32_t i = 0; i < uint32_t(entities.size()); ++i) {
		uint32_t opened = 0;
		if (_local.openActivation(scope, activation, i, makeRecordKey(fanOut, activation), opened)
						!= Status::Ok
				|| _local.materializeScope(scope, opened) != Status::Ok) {
			_activationsExhausted = true;
			return;
		}
		if (!threaded) {
			_local.pushOpen(opened);
		}
		writeBranchEntity(*_local.getArena(), _local.activationFrame(opened), entities[i]);
	}

	writeFanOut(fanOut, activation, FanOutLocal::First, first);
	writeFanOut(fanOut, activation, FanOutLocal::Closed, 0);
	writeFanOut(fanOut, activation, FanOutLocal::Phase,
			entities.empty() ? FanOutLocal::Ready
							 : (threaded ? FanOutLocal::Launched : FanOutLocal::InFlight));
	if (barrier != InvalidIndex) {
		_local.addFlags(_local.getState(barrier, activation), NodeFlags::StallExternal);
	}

	if (threaded) {
		for (uint32_t i = 0; i < uint32_t(entities.size()); ++i) {
			_local.addFlags(_local.getStateIn(_local.activationFrame(first + i), firstNode), NodeFlags::Token);
		}
		if (!launchBlock(fanOut, activation,
					SpanView<value::EntityId>(entities.data(), entities.size()), launch)) {
			// The executor took the branches and the job could not be prepared: run them here.
			writeFanOut(fanOut, activation, FanOutLocal::Phase, FanOutLocal::InFlight);
			for (uint32_t i = 0; i < uint32_t(entities.size()); ++i) {
				_local.pushOpen(first + i);
			}
		} else {
			return;
		}
	}

	// Branch 0's work first: the caller pushes this list in reverse.
	for (uint32_t i = 0; i < uint32_t(entities.size()); ++i) {
		auto act = first + i;
		auto frame = _local.activationFrame(act);
		_local.addFlags(_local.getStateIn(frame, firstNode), NodeFlags::Token);
		for (auto n : _graph->getScopeNodes(scope)) {
			if (isReadyIn(frame, n, act)) {
				ready.emplace_back(makeRecordKey(n, act));
			}
		}
	}

	if (_executor && _clock) {
		auto &blockScopeHere = _graph->getScopeAt(_graph->getNodeAt(fanOut).opensScope);
		_flights.emplace_back(Flight{fanOut, activation, _clock->nowMs(),
			_graph->getBlockAt(blockScopeHere.block).timeoutMs, false});
	}

	if (entities.empty()) {
		submitBlock(fanOut, activation);
	}
}

template <typename Graph, typename Local, typename Trace>
void MachineT<Graph, Local, Trace>::failBranch(uint32_t node, uint32_t activation, uint32_t blockIndex,
		uint32_t branch, Status status, bool budget, bool log, RunReport &report) {
	auto &block = _graph->getBlockAt(blockIndex);
	auto header = _local.activationFrame(branch);
	auto flags = readBranchInt(*_local.getArena(), header, BranchHeader::StatusOffset);
	writeBranchInt(*_local.getArena(), header, BranchHeader::StatusOffset,
			flags | BranchHeader::Failed);

	auto &rt = _graph->getNodeAt(node);
	const bool cancel = block.onFailure == ParallelFailure::CancelFrame;
	const auto iteration = int64_t(_local.readActivation(branch).iteration);
	int64_t values[] = {int64_t(rt.id), int64_t(toInt(status)), iteration};
	StringView names[] = {rt.op->getName()};
	reportRun<EnvType>(report, cancel ? DiagSeverity::Error : DiagSeverity::Warning,
			DiagCode::ParallelBranchFailed,
			DiagText(budget ? DiagDetail::ParallelBranchBudget : DiagDetail::ParallelBranchFailed)
					.name(rt.op->getName())
					.number(iteration),
			DiagLocus::Branch, values, names);

	if (log) {
		if constexpr (Trace::Log) {
			RunStep record;
			record.step = report.stepCount;
			record.kind = RunStepKind::BranchFailed;
			record.node = node;
			record.activation = activation;
			record.id = rt.id;
			report.log.emplace_back(record);
		} else {
			_lastNode = node;
			_lastActivation = activation;
		}
	}

	if (cancel) {
		// The frame is the host's to restore. The run ends with this unit, which still counts: the
		// caller settles it, and the next stepOnce answers false.
		conclude(report, RunOutcome::Cancelled, status);
	}
}

template <typename Graph, typename Local, typename Trace>
void MachineT<Graph, Local, Trace>::drainFailed() {
	if (_graph->getBlockCount() == 0) {
		return;
	}
	uint32_t node = 0;
	uint32_t activation = RootActivation;
	while (_local.peekReady(node, activation)) {
		uint32_t block = InvalidIndex;
		uint32_t branch = InvalidIndex;
		if (!branchOf(node, activation, block, branch)) {
			return;
		}
		auto status = readBranchInt(*_local.getArena(), _local.activationFrame(branch),
				BranchHeader::StatusOffset);
		if ((status & BranchHeader::Failed) == 0) {
			return;
		}
		_local.popReady(node, activation);
		_local.removeFlags(_local.getState(node, activation), NodeFlags::Queued);
	}
}

template <typename Graph, typename Local, typename Trace>
bool MachineT<Graph, Local, Trace>::closeBlocked(uint32_t activation) const {
	auto holds = [&](uint32_t walk) {
		while (walk != NullActivation) {
			if (walk == activation) {
				return true;
			}
			walk = _local.readActivation(walk).parent;
		}
		return false;
	};
	for (auto &ticket : _pending) {
		if (holds(ticket.activation)) {
			return true;
		}
	}
	for (auto &flight : _flights) {
		if (flight.job && holds(flight.activation)) {
			return true;
		}
	}
	return false;
}

template <typename Graph, typename Local, typename Trace>
void MachineT<Graph, Local, Trace>::submitBlock(uint32_t fanOut, uint32_t activation) {
	BlockTicket ticket{fanOut, activation, _generation};
	_pending.emplace_back(ticket);
	if (_executor) {
		_executor->submit(ticket);
	}
}

template <typename Graph, typename Local, typename Trace>
bool MachineT<Graph, Local, Trace>::checkTimeouts(RunReport &report) {
	if (!_executor || !_clock || _flights.empty()) {
		return true;
	}
	auto now = _clock->nowMs();
	for (auto &flight : _flights) {
		auto &fanOut = _graph->getNodeAt(flight.fanOut);
		auto blockIndex = _graph->getScopeAt(fanOut.opensScope).block;
		auto &block = _graph->getBlockAt(blockIndex);
		// The flight's own deadline: the author's `timeout`, or what the executor answered for the
		// kind of launch it took.
		if (flight.aborted || flight.timeoutMs == 0 || now - flight.start <= flight.timeoutMs) {
			continue;
		}
		flight.aborted = true;

		// Branches an executor runs are stopped between two units, and what did not close fails
		// when the job is taken back.
		if (flight.job) {
			flight.job->cancel();
		}

		// Every branch still running fails; what closed without failing stays as it is.
		auto first = uint32_t(readFanOut(flight.fanOut, flight.activation, FanOutLocal::First));
		auto record = _local.getRecord(flight.fanOut, flight.activation);
		auto shape = fanOutLocalShape(fanOut, FanOutLocal::Entities);
		auto count =
				value::blob::arrayCount(*_local.getArena(), record + shape.offset, shape.element);
		for (uint32_t i = 0; i < count && !flight.job; ++i) {
			auto header = _local.activationFrame(first + i);
			auto status = readBranchInt(*_local.getArena(), header, BranchHeader::StatusOffset);
			if ((status & BranchHeader::Closed) == 0) {
				writeBranchInt(*_local.getArena(), header, BranchHeader::StatusOffset,
						status | BranchHeader::Failed);
			}
		}

		const bool cancel = block.onFailure == ParallelFailure::CancelFrame;
		// Whose number it was: the author wrote one, or the executor's own default stood in.
		int64_t values[] = {int64_t(fanOut.id), int64_t(flight.timeoutMs),
			block.timeoutMs == 0 ? 1 : 0};
		reportRun<EnvType>(report, cancel ? DiagSeverity::Error : DiagSeverity::Warning,
				DiagCode::ParallelBlockTimeout,
				DiagText(DiagDetail::ParallelBlockTimeout).number(int64_t(flight.timeoutMs)),
				DiagLocus::Timeout, values);
		if (cancel) {
			conclude(report, RunOutcome::Cancelled, Status::ErrorCancelled);
			return false;
		}
	}
	drainFailed();
	if (_state == RunState::Suspended) {
		_state = RunState::Paused; // what the timeout changed may be runnable now
	}
	return true;
}

template <typename Graph, typename Local, typename Trace>
bool MachineT<Graph, Local, Trace>::nextDelivery(BlockTicket &out) {
	if (_hasPolled) {
		out = _polled;
		return true;
	}
	if (_pending.empty() && _threaded == 0) {
		return false;
	}
	if (!_executor) {
		out = _pending.front();
		return true;
	}
	// A block broken off by its timeout is delivered without asking the executor - its branches,
	// when an executor runs them, once they have stopped.
	for (auto &flight : _flights) {
		if (!flight.aborted) {
			continue;
		}
		if (flight.job) {
			flight.job->waitDone();
			out = flight.job->getTicket();
			return true;
		}
		for (auto &it : _pending) {
			if (it.node == flight.fanOut && it.activation == flight.activation) {
				out = it;
				return true;
			}
		}
	}
	BlockTicket ticket;
	while (_executor->poll(ticket)) {
		if (ticket.generation != _generation) {
			continue; // handed out before the arena moved back
		}
		bool known = false;
		for (auto &it : _pending) {
			known = known || (it.node == ticket.node && it.activation == ticket.activation);
		}
		for (auto &flight : _flights) {
			known = known
					|| (flight.job && flight.fanOut == ticket.node && flight.activation == ticket.activation
							&& flight.job->isDone());
		}
		if (known) {
			_polled = ticket;
			_hasPolled = true;
			out = ticket;
			return true;
		}
	}
	return false;
}

// The pending blocks are what the arena says: every fan-out in phase Ready. Renewed after anything
// that moves the arena back, with a new generation, so a ticket handed out before cannot deliver.
template <typename Graph, typename Local, typename Trace>
void MachineT<Graph, Local, Trace>::refreshPending() {
	abandonFlights();
	_pending.clear();
	_flights.clear();
	_hasPolled = false;
	++_generation;
	if (!_graph || _graph->getBlockCount() == 0) {
		return;
	}
	for (uint32_t b = 0; b < _graph->getBlockCount(); ++b) {
		auto fanOut = _graph->getBlockAt(b).fanOut;
		auto scope = _graph->getNodeAt(fanOut).scope;
		for (uint32_t act = 0; act < _local.getActivationCount(); ++act) {
			if (_local.activationScope(act) != scope || _local.getRecord(fanOut, act) == NullAddr) {
				continue;
			}
			auto phase = readFanOut(fanOut, act, FanOutLocal::Phase);
			if (phase == FanOutLocal::Ready) {
				submitBlock(fanOut, act);
			}
			// Branches that were with an executor are given to it again, or - when none takes them
			// - run here, from where the fan-out left them.
			if (phase == FanOutLocal::Launched) {
				auto first = uint32_t(readFanOut(fanOut, act, FanOutLocal::First));
				auto record = _local.getRecord(fanOut, act);
				auto shape = fanOutLocalShape(_graph->getNodeAt(fanOut), FanOutLocal::Entities);
				mem_std::Vector<value::EntityId> entities;
				auto count = value::blob::arrayCount(*_local.getArena(), record + shape.offset,
						shape.element);
				for (uint32_t i = 0; i < count; ++i) {
					Var item;
					value::blob::arrayGet(*_local.getArena(), record + shape.offset, shape.element,
							i, item);
					entities.emplace_back(value::EntityId::unpack(item.ent.id));
				}
				if (!launchBlock(fanOut, act,
							SpanView<value::EntityId>(entities.data(), entities.size()))) {
					writeFanOut(fanOut, act, FanOutLocal::Phase, FanOutLocal::InFlight);
					auto branchScope = _graph->getNodeAt(fanOut).opensScope;
					for (uint32_t i = 0; i < count; ++i) {
						_local.pushOpen(first + i);
					}
					for (uint32_t i = count; i > 0; --i) {
						auto branchAct = first + i - 1;
						auto frame = _local.activationFrame(branchAct);
						auto nodes = _graph->getScopeNodes(branchScope);
						for (uint32_t k = uint32_t(nodes.size()); k > 0; --k) {
							if (isReadyIn(frame, nodes[k - 1], branchAct)) {
								enqueueIn(frame, nodes[k - 1], branchAct);
							}
						}
					}
				}
				continue;
			}
			// A fan-out that ran and opened nothing was refused its activations: the run still ends
			// in ActivationLimit, and that is in the arena rather than in the host's memory.
			if (phase == FanOutLocal::Idle
					&& (_local.readState(_local.getState(fanOut, act)).flags & NodeFlags::Ran) != 0) {
				_activationsExhausted = true;
			}
			// The time a block was in flight before the arena moved back is not in the arena: it
			// starts again now.
			if ((phase == FanOutLocal::Ready || phase == FanOutLocal::InFlight) && _executor && _clock) {
				auto &armedScope = _graph->getScopeAt(_graph->getNodeAt(fanOut).opensScope);
				_flights.emplace_back(Flight{fanOut, act, _clock->nowMs(),
					_graph->getBlockAt(armedScope.block).timeoutMs, false});
			}
		}
	}
}

template <typename Graph, typename Local, typename Trace>
Status MachineT<Graph, Local, Trace>::deliver(const BlockTicket &ticket, RunReport &report) {
	auto &fanOutNode = _graph->getNodeAt(ticket.node);
	auto blockIndex = _graph->getScopeAt(fanOutNode.opensScope).block;
	auto &block = _graph->getBlockAt(blockIndex);
	auto activation = ticket.activation;

	for (auto it = _pending.begin(); it != _pending.end(); ++it) {
		if (it->node == ticket.node && it->activation == ticket.activation) {
			_pending.erase(it);
			break;
		}
	}
	auto first = uint32_t(readFanOut(ticket.node, activation, FanOutLocal::First));
	for (auto it = _flights.begin(); it != _flights.end(); ++it) {
		if (it->fanOut == ticket.node && it->activation == ticket.activation) {
			auto job = it->job;
			auto timeoutMs = it->timeoutMs; // the flight goes before the import reads it
			_flights.erase(it);
			if (job) {
				// What an executor ran comes back into this store first, as if run here: a batch of
				// branch runs, or the records of a dispatch.
				job->waitDone();
				bool cancelled = false;
				Status st = Status::Ok;
				if (job->getKind() == BranchJob::Kind::Gpu) {
					auto gpuJob = static_cast<GpuBlockJobT<Graph, Local, Trace> *>(job);
					st = importGpu(*gpuJob, first, timeoutMs, report, cancelled);
					if (_executor) {
						_executor->forgetGpu(*gpuJob);
					}
					delete job;
				} else {
					auto branchJob = static_cast<BranchJobT<Graph, Local, Trace> *>(job);
					st = importBranches(*branchJob, first, report, cancelled);
					if (_executor) {
						_executor->forget(*branchJob);
					}
					giveBranchJob(branchJob); // its batch stores stay warm for the next launch
				}
				noteThreaded(-1);
				if (st != Status::Ok) {
					return st;
				}
				auto shape = fanOutLocalShape(fanOutNode, FanOutLocal::Entities);
				auto record = _local.getRecord(ticket.node, activation);
				writeFanOut(ticket.node, activation, FanOutLocal::Closed,
						value::blob::arrayCount(*_local.getArena(), record + shape.offset,
								shape.element));
				writeFanOut(ticket.node, activation, FanOutLocal::Phase, FanOutLocal::Ready);
				if (cancelled) {
					conclude(report, RunOutcome::Cancelled, Status::ErrorCancelled);
					return Status::Ok;
				}
			}
			break;
		}
	}

	auto record = _local.getRecord(ticket.node, activation);
	auto shape = fanOutLocalShape(fanOutNode, FanOutLocal::Entities);
	auto count = value::blob::arrayCount(*_local.getArena(), record + shape.offset, shape.element);

	// The copies, in branch order, as the policy says.
	auto writes = _graph->getBlockWrites(blockIndex);
	if (block.onFailure != ParallelFailure::Nothing && !writes.empty() && _scene) {
		auto registry = _scene->getRegistry();
		for (uint32_t i = 0; i < count; ++i) {
			auto header = _local.activationFrame(first + i);
			auto status = readBranchInt(*_local.getArena(), header, BranchHeader::StatusOffset);
			if ((status & BranchHeader::Failed) != 0) {
				continue;
			}
			auto entity = readBranchEntity(*_local.getArena(), header);
			for (auto &write : writes) {
				if (!readBranchFlag(*_local.getArena(), header, write.flagOffset)) {
					continue;
				}
				Var value;
				auto st = readRecordField(*_local.getArena(), header, shapeOfCopy(write), value);
				auto type = registry ? registry->get(write.componentId) : nullptr;
				auto desc = type ? type->getField(write.field) : nullptr;
				auto row = type ? _scene->getComponent(entity, *type) : NullAddr;
				if (st != Status::Ok || !desc || row == NullAddr) {
					return st != Status::Ok ? st : Status::ErrorNotFound;
				}
				st = type->setField(*_scene->getArena(), row, *desc, value);
				if (st != Status::Ok) {
					return st;
				}
			}
		}
	}

	writeFanOut(ticket.node, activation, FanOutLocal::Phase, FanOutLocal::Delivered);

	// The barrier, in the fan-out's activation, and the collectors that read the branches.
	auto frame = _local.activationFrame(activation);
	auto barrierState = _local.getStateIn(frame, block.barrier);
	_local.removeFlags(barrierState, NodeFlags::StallExternal | NodeFlags::Ran | NodeFlags::Queued);
	_local.addFlags(barrierState, NodeFlags::Token);
	mem_std::Vector<uint32_t> ready;
	for (auto collector : _graph->getBlockCollectors(blockIndex)) {
		_local.markInput(_local.getStateIn(frame, collector),
				_graph->getNodeAt(collector).op->getBranchPin());
		if (isReadyIn(frame, collector, activation)) {
			ready.emplace_back(collector);
		}
	}
	if (isReadyIn(frame, block.barrier, activation)) {
		enqueueIn(frame, block.barrier, activation);
	}
	for (uint32_t i = uint32_t(ready.size()); i > 0; --i) {
		enqueueIn(frame, ready[i - 1], activation);
	}

	if constexpr (Trace::Log) {
		RunStep record;
		record.step = report.stepCount;
		record.kind = RunStepKind::Deliver;
		record.node = block.barrier;
		record.activation = activation;
		record.id = _graph->getNodeAt(block.barrier).id;
		report.log.emplace_back(record);
	} else {
		_lastNode = block.barrier;
		_lastActivation = activation;
	}
	return Status::Ok;
}

template <typename Graph, typename Local, typename Trace>
void MachineT<Graph, Local, Trace>::resampleWatches() {
	for (auto &bp : _breakpoints) {
		if (bp.watch.source == Watch::Source::None) {
			continue;
		}
		auto act = bp.activation != InvalidIndex ? bp.activation : RootActivation;
		Var current;
		bp.hasLast = readWatch(bp, act, current);
		bp.last = bp.hasLast ? current : value::makeNil();
	}
}

template <typename Graph, typename Local, typename Trace>
Status MachineT<Graph, Local, Trace>::stepBack(RunReport &report) {
	// A step back is a boundary back, and the boundary is read off the log's last entry: a machine
	// that writes no log has nothing to read it from, and says so rather than guessing.
	if constexpr (!Trace::Log) {
		return Status::ErrorNotImplemented;
	}
	// And a store whose run is half in host memory has nothing for a version to restore: rolling
	// the frames back would leave the front and the activation tree describing a run that stopped
	// existing. Refusing is the mode (SPFlowFast.h), not a gap.
	if constexpr (!Local::Rollbackable) {
		return Status::ErrorNotSupported;
	}
	if (!hasJournal()) {
		return Status::ErrorNotImplemented;
	}
	if (_state == RunState::Idle) {
		return Status::ErrorInvalidArguemnt;
	}
	if (report.log.empty()) {
		return Status::ErrorNotFound;
	}

	auto base = report.log.back().baseVersion;

	// Close what has accumulated since the last unit - the final sweep writes too - so that the
	// rollback undoes all of it and not merely the last operation. Deliberately not the notifying
	// form: this version exists for a moment and is undone on the next line, so telling an
	// extension it closed would ask it to act on a state that is about to stop having existed. The
	// rollback below is the notification that means something here.
	closeVersion();
	abandonFlights();
	auto st = undoToAndNotify(base);
	if (st != Status::Ok) {
		return st;
	}

	// One step back is one boundary back, and under Step those are the same thing. Under a coarser
	// quantum the units inside a boundary have no point of their own on the timeline, so they leave
	// together - the same rule, over data that says less.
	while (!report.log.empty() && report.log.back().baseVersion == base) { report.log.pop_back(); }
	_quantumBase = base;
	report.stepCount = uint32_t(report.log.size());
	report.sweepCount = 0;
	report.outcome = RunOutcome::Invalid; // the run is no longer over, whatever it was

	_state = RunState::Paused;
	_status = Status::Ok;
	_stoppedBefore = false;
	_lastBreakpoint = 0;

	// The bytes went back; the host's memory of what they held did not.
	resampleWatches();
	refreshPending();
	_branchUnits = 0;
	for (auto &it : report.log) {
		_branchUnits += isBranchUnit(it) ? 1 : 0;
	}
	return Status::Ok;
}

template <typename Graph, typename Local, typename Trace>
uint32_t MachineT<Graph, Local, Trace>::addBreakpoint(const Breakpoint &desc) {
	// Everything that can be decided now is decided now. The failure mode this guards against is
	// the expensive one: a breakpoint that never fires looks exactly like a graph that never
	// reaches the node, and the search goes to the wrong place.
	if (desc.watch.source == Watch::Source::None) {
		if (desc.node == NullNodeId) {
			return 0; // a breakpoint on nothing at all, which is what stepOnce() already is
		}
		if (desc.compare != BreakCompare::Any) {
			return 0; // nothing to compare
		}
	} else {
		if (desc.watch.field.empty()) {
			return 0;
		}
		if (desc.compare == BreakCompare::Any) {
			return 0; // a watch that watches for nothing is a positional breakpoint written oddly
		}
		if (desc.compare == BreakCompare::Changed) {
			if (desc.value.type != value::VarType::Nil) {
				return 0; // Changed compares against the past, not against a value
			}
		} else if (desc.value.type == value::VarType::Nil) {
			return 0;
		}
	}

	if (desc.watch.source == Watch::Source::Scene) {
		if (desc.watch.type == value::NullTypeId || desc.watch.entity == value::EntityId()) {
			return 0;
		}
	} else if (desc.watch.source != Watch::Source::None && desc.watch.node == NullNodeId) {
		return 0; // a record watch has to say whose record
	}

	// Any node named - as a position or as a watch target - needs a graph to name it in.
	for (auto id : {desc.node, desc.watch.node}) {
		if (id != NullNodeId && (!_graph || _graph->findNode(id) == InvalidIndex)) {
			return 0;
		}
	}
	if (desc.watch.source == Watch::Source::NodeRecord) {
		auto &rt = _graph->getNodeAt(_graph->findNode(desc.watch.node));
		if (!rt.localSchema || !rt.localSchema->getField(desc.watch.field)) {
			return 0;
		}
	}
	if (desc.watch.source == Watch::Source::NodeState) {
		auto type = _local.getStateType();
		if (!type || !type->getField(desc.watch.field)) {
			return 0;
		}
	}

	Breakpoint bp = desc;
	if (bp.when == BreakWhen::Default) {
		bp.when = bp.watch.source == Watch::Source::None ? BreakWhen::BeforeStep
														 : BreakWhen::AfterStep;
	}
	bp.id = _nextBreakpointId++;
	bp.hits = 0;
	bp.last = value::makeNil();
	bp.hasLast = false;
	_breakpoints.emplace_back(sprt::move(bp));
	return _breakpoints.back().id;
}

template <typename Graph, typename Local, typename Trace>
bool MachineT<Graph, Local, Trace>::removeBreakpoint(uint32_t id) {
	for (auto it = _breakpoints.begin(); it != _breakpoints.end(); ++it) {
		if (it->id == id) {
			_breakpoints.erase(it);
			return true;
		}
	}
	return false;
}

template <typename Graph, typename Local, typename Trace>
Status MachineT<Graph, Local, Trace>::setBreakpointEnabled(uint32_t id, bool enabled) {
	for (auto &it : _breakpoints) {
		if (it.id == id) {
			it.enabled = enabled;
			return Status::Ok;
		}
	}
	return Status::ErrorNotFound;
}

template <typename Graph, typename Local, typename Trace>
void MachineT<Graph, Local, Trace>::clearBreakpoints() {
	_breakpoints.clear();
	_lastBreakpoint = 0;
}

template <typename Graph, typename Local, typename Trace>
const Breakpoint *MachineT<Graph, Local, Trace>::getBreakpoint(uint32_t id) const {
	for (auto &it : _breakpoints) {
		if (it.id == id) {
			return &it;
		}
	}
	return nullptr;
}

template <typename Graph, typename Local, typename Trace>
bool MachineT<Graph, Local, Trace>::readWatch(const Breakpoint &bp, uint32_t activation, Var &out) const {
	out = value::makeNil();

	// The resolver's stores, exactly the two this object holds. No flag gate on the scene: a
	// breakpoint is not an operation and promised nothing.
	WatchTarget target;
	if (resolveWatch(bp.watch, activation, _graph, &_local, _scene, target)
			!= WatchResolution::Ok) {
		return false;
	}

	// Two branches and not a ternary: the two stores are two arena types - a fast run keeps its
	// frames in a scratch arena of its own kind - and one expression cannot name both.
	if (target.store == WatchTarget::Store::Local) {
		auto arena = _local.getArena();
		return arena && target.type->getField(*arena, target.record, *target.field, out) == Status::Ok;
	}
	auto arena = _scene ? _scene->getArena() : nullptr;
	return arena && target.type->getField(*arena, target.record, *target.field, out) == Status::Ok;
}

// A breakpoint's value is spelled as Bool, Int or Float; it is compared in the field's own type.
inline bool equalAsField(const Var &field, const Var &value) {
	if (field.type == value.type) {
		return field == value;
	}
	Var converted;
	return value::castVar(value, field.type, value::CastPolicy::Lossless, converted) == Status::Ok
			&& field == converted;
}

// Ordering across the numeric variants only. A breakpoint that asks whether a string is less than
// another gets "no" rather than an answer nobody agreed on.
inline bool orderVar(const Var &a, const Var &b, int &out) {
	auto numeric = [](const Var &v, double &d) {
		switch (v.type) {
		case value::VarType::Bool:
		case value::VarType::Int:
		case value::VarType::Int32:
		case value::VarType::UInt32:
		case value::VarType::Enum: d = double(v.i); return true;
		case value::VarType::Float: d = v.f; return true;
		case value::VarType::Float32: d = double(v.v[0]); return true;
		default: return false;
		}
	};
	double x = 0;
	double y = 0;
	if (!numeric(a, x) || !numeric(b, y)) {
		return false;
	}
	out = x < y ? -1 : (x > y ? 1 : 0);
	return true;
}

template <typename Graph, typename Local, typename Trace>
uint32_t MachineT<Graph, Local, Trace>::evaluateBreakpoints(BreakWhen when, uint32_t node, uint32_t activation) {
	if (_breakpoints.empty()) {
		return 0;
	}

	uint32_t hit = 0;
	for (auto &bp : _breakpoints) {
		if (bp.when != when) {
			continue;
		}

		// Sampled before the position is tested, and independently of where the run is: the watch
		// names its own node, so "since the previous unit" means the previous unit and not "the
		// previous unit that happened to be somewhere this breakpoint cared about".
		Var current;
		auto previous = bp.last;
		auto hadPrevious = bp.hasLast;
		bool read = false;
		if (bp.watch.source != Watch::Source::None) {
			auto act = bp.activation != InvalidIndex ? bp.activation : activation;
			read = readWatch(bp, act, current);
			if (read) {
				bp.last = current;
				bp.hasLast = true;
			}
		}

		if (!bp.enabled || hit != 0) {
			continue; // sampled anyway, so that enabling one later does not fire on ancient history
		}
		if (bp.node != NullNodeId) {
			if (node == InvalidIndex || !_graph || _graph->getNodeAt(node).id != bp.node) {
				continue;
			}
		}
		if (bp.activation != InvalidIndex && bp.activation != activation) {
			continue;
		}

		bool fires = false;
		if (bp.compare == BreakCompare::Any) {
			fires = true;
		} else if (bp.compare == BreakCompare::Changed) {
			fires = read && hadPrevious && previous != current;
		} else if (read) {
			auto holds = [&](const Var &v) {
				int order = 0;
				switch (bp.compare) {
				case BreakCompare::Equal: return equalAsField(v, bp.value);
				case BreakCompare::NotEqual: return !equalAsField(v, bp.value);
				case BreakCompare::Less: return orderVar(v, bp.value, order) && order < 0;
				case BreakCompare::Greater: return orderVar(v, bp.value, order) && order > 0;
				default: return false;
				}
			};
			// The transition, not the state. A condition that merely goes on holding would stop the
			// run on every unit and drown the moment it actually became true.
			fires = holds(current) && !(hadPrevious && holds(previous));
		}

		if (fires) {
			++bp.hits;
			hit = bp.id;
		}
	}
	return hit;
}

template <typename Graph, typename Local, typename Trace>
Status MachineT<Graph, Local, Trace>::finish(RunReport &report) {
	_lastBreakpoint = 0;

	while (true) {
		// Continuing from a BeforeStep breakpoint gets past it first. Otherwise "continue" would
		// stop at the position it is already standing on, and never move.
		if (!_stoppedBefore) {
			auto key = peekNext();
			if (key != NullRecordKey) {
				auto id = evaluateBreakpoints(BreakWhen::BeforeStep, recordKeyNode(key),
						recordKeyActivation(key));
				if (id != 0) {
					_lastBreakpoint = id;
					_stoppedBefore = true;
					return _status;
				}
			}
		}
		_stoppedBefore = false;

		if (!stepOnce(report)) {
			break;
		}

		uint32_t id = 0;
		if constexpr (Trace::Log) {
			auto &done = report.log.back();
			id = evaluateBreakpoints(BreakWhen::AfterStep, done.node, done.activation);
		} else {
			id = evaluateBreakpoints(BreakWhen::AfterStep, _lastNode, _lastActivation);
		}
		if (id != 0) {
			_lastBreakpoint = id;
			return _status;
		}
	}
	return _status;
}

template <typename Graph, typename Local, typename Trace>
Status MachineT<Graph, Local, Trace>::run(const Graph &graph, const OpRegistry &ops, ArenaType &arena,
		const Config &config, RunReport &report) {
	auto st = begin(graph, ops, arena, config, report);
	if (st != Status::Ok) {
		return st;
	}
	return finish(report);
}

// What the fan-out's `executors` setting permits, as BlockExecutorMask bits.
template <typename Graph>
uint32_t readBlockExecutors(const Graph &graph, uint32_t blockIndex) {
	auto fanOut = graph.getBlockAt(blockIndex).fanOut;
	auto &node = graph.getNodeAt(fanOut);
	uint32_t index = 0;
	if (!node.op || !node.op->findSetting(StringView("executors"), index)) {
		return 0;
	}
	uint32_t mask = 0;
	if (auto value = graph.getSetting(fanOut, index); value && value->isArray()) {
		for (auto &it : value->asArray()) {
			if (it.getString() == StringView("threads")) {
				mask |= uint32_t(BlockExecutorMask::Threads);
			} else if (it.getString() == StringView("gpu")) {
				mask |= uint32_t(BlockExecutorMask::Gpu);
			}
		}
	}
	return mask;
}

// Whether copying an instance of this schema is a copy of bytes and nothing else. A container field
// deep-copies - `blob::copy` frees the destination's block and allocates a new one - and the
// allocator is the one thing two threads may never be in at once. A null schema is flat: the
// operation declares no record, so there is nothing to copy.
inline bool isFlatSchema(const value::ComponentType *type) {
	if (!type) {
		return true;
	}
	for (auto &field : type->getFields()) {
		if (value::isContainerType(field.type)) {
			return false;
		}
	}
	return true;
}

/* The machine's store as a worker sees it while it copies a batch back: the frame layout, which is
derived from the graph and the same for everybody, and a write view of the arena, which is this
worker's alone. There is deliberately nothing else in it - every address it is asked about was
worked out before the phase began, so it answers with arithmetic: no activation table, no memo, no
allocator. */
template <typename Local>
struct ImportTargetT {
	const Local *local = nullptr;
	typename Local::FrameArena *arena = nullptr;

	typename Local::FrameArena *getArena() const { return arena; }
	Addr getStateIn(Addr frame, uint32_t node) const { return local->getStateIn(frame, node); }
	Addr getRecordIn(Addr frame, uint32_t node) const { return local->getRecordIn(frame, node); }
};

template <typename Dst, typename Src, typename Graph>
Status copyNodeRows(Dst &dst, Addr dstFrame, const Src &src, Addr srcFrame, const Graph &graph,
		uint32_t node) {
	auto stateType = src.getStateType();
	auto st = stateType->copyInstance(*dst.getArena(), dst.getStateIn(dstFrame, node), *src.getArena(),
			src.getStateIn(srcFrame, node));
	if (st != Status::Ok) {
		return st;
	}
	auto schema = graph.getNodeAt(node).localSchema;
	auto dstRecord = dst.getRecordIn(dstFrame, node);
	auto srcRecord = src.getRecordIn(srcFrame, node);
	if (!schema || dstRecord == NullAddr || srcRecord == NullAddr) {
		return Status::Ok;
	}
	schema->destroyInstance(*dst.getArena(), dstRecord);
	return schema->copyInstance(*dst.getArena(), dstRecord, *src.getArena(), srcRecord);
}

// The job a machine hands an executor: the batches, each a machine over a fast store, and the view
// of the scene they read through. Prepared on the machine's thread; a batch is then touched by
// exactly one thread at a time - its worker, and the machine again when the job is done.
template <typename Graph, typename Local, typename Trace>
class BranchJobT final : public BranchJob {
public:
	using Parent = MachineT<Graph, Local, Trace>;
	using ArenaType = typename Local::ArenaType;
	using Runner = MachineT<Graph, FastLocalT<ArenaType, Graph, typename Local::EnvType>, Trace>;

	struct Batch {
		Runner machine;
		RunReport report;
		uint32_t first = 0; // the global index of its first branch
		uint32_t count = 0;
		uint32_t rootFirst = 0; // the activation its first branch opened as, in its own store
		Status status = Status::Ok; // what went wrong in this batch's phase, if anything

		// Filled by the machine's reserving pass, read by whoever copies the batch back: which
		// branch owns each of the batch's activations, where each one lands here, its frame's
		// address here, and one plan per branch. `frame` is here rather than asked for at copy time
		// because asking goes through the parent store's `runRecord()` memo, which is `mutable` and
		// not atomic; resolved once, on the machine's thread, it is just a number.
		mem_std::Vector<uint32_t> owner;
		mem_std::Vector<uint32_t> map;
		mem_std::Vector<Addr> frame;
		mem_std::Vector<BranchPlan> plan;

		// This batch's own window into the machine's store while it copies. Its dirty map is its
		// own; the machine merges it when the phase is over.
		typename Local::FrameArena writeView;
	};

	virtual ~BranchJobT() {
		for (auto it : _list) {
			delete it;
		}
	}

	/* Everything about the job that only the machine's thread can decide: the view of the scene,
	the entities, and how the branches are cut into batches. It allocates no batch store - that is
	`prepareBatch`, which is a phase and may run anywhere. The split is the whole point: building a
	batch is the larger half of what a block costs before a single branch runs, and every one of
	those batches is independent of every other, while what is left here reads the parent and is a
	few dozen words. */
	Status plan(Parent &parent, const BlockTicket &ticket, uint32_t block,
			SpanView<value::EntityId> entities, uint32_t batchSize) {
		_parent = &parent;
		_ticket = ticket;
		_block = block;
		_entities.assign(entities.begin(), entities.end());
		_config = typename Runner::Config();
		if (parent._scene) {
			/* The view is taken afresh every launch, and it is the one thing here that is not
			reused. A view is a snapshot of the scene arena's chunk table, and that table grows
			silently: `getEpoch()` is bumped by a row that moved, not by a chunk that was added,
			so a kept snapshot would go on answering for a store that has since grown past it.
			Copying a few dozen pointers is the cheap half of what a launch builds; the batch
			stores are the expensive half, and those are kept. */
			if (_view.isInitialized()) {
				_view.closeView();
			}
			if (_view.initView(*parent._scene->getArena()) != Status::Ok
					|| _viewStore.open(_view, *parent._scene->getRegistry()) != Status::Ok) {
				return Status::ErrorInvalidArguemnt;
			}
			_config.scene = &_viewStore;
			_epoch = parent._scene->getArena()->getEpoch();
		}
		_config.extensions = parent._extensions;
		_config.maxSteps = parent._maxSteps;
		_config.maxActivations = maxOf<uint32_t>() / 2;
		_config.frontWeights = parent._frontWeights;

		auto n = uint32_t(_entities.size());
		uint32_t used = 0;
		for (uint32_t k = 0; k < n; k += batchSize) {
			// A recycled job keeps its batches, and with them their arenas and the pools those cut
			// from - which is the whole of what this pool is for.
			if (used == _list.size()) {
				_list.emplace_back(new Batch);
			}
			auto batch = _list[used++];
			batch->first = k;
			batch->count = sprt::min(batchSize, n - k);
			batch->status = Status::Ok;
		}
		_batches = used;
		_branches = n;
		beginPhase(Phase::Prepare);
		return Status::Ok;
	}

	/* Made ready to be used again. What goes is what named the last block; what stays is every
	batch, and with it the machine, the arena and the memory pool that arena cut from - the
	`FastLocalT` scratch store outlives its run by design, and this is the pool that lets it.
	Only ever called after `waitDone()`: a batch's arena belongs to its worker until then, and
	`_cancel` is set from that worker's thread. */
	void recycle() {
		if (_view.isInitialized()) {
			_view.closeView();
		}
		// The batches this job will not use again give their frames back now rather than holding
		// them until something else happens to want that many.
		for (uint32_t i = 0; i < uint32_t(_list.size()); ++i) {
			_list[i]->machine.reset();
			if (_list[i]->writeView.isInitialized()) {
				_list[i]->writeView.closeView();
			}
			_list[i]->owner.clear();
			_list[i]->map.clear();
			_list[i]->frame.clear();
			_list[i]->plan.clear();
			_list[i]->report = RunReport();
			_list[i]->status = Status::Ok;
		}
		_parent = nullptr;
		_ticket = BlockTicket();
		_block = InvalidIndex;
		_epoch = 0;
		_batches = 0;
		_branches = 0;
		_entities.clear();
		_report = nullptr;
		_logBase = 0;
		// The cancel is not cleared here. It is cleared where a job is armed to run again
		// (`beginPhase(Phase::Prepare)`), which is the one place every launch passes through -
		// including a job that is re-planned without ever having been recycled.
		_remaining.store(0);
	}

	// The first thing that went wrong in a phase, or Ok. A phase runs every batch whatever happens
	// - a batch that gave up still owes its `finish()`, or the machine would wait for it forever.
	Status getStatus() const {
		for (auto it : getBatchList()) {
			if (it->status != Status::Ok) {
				return it->status;
			}
		}
		return Status::Ok;
	}

	// Opens a window into the machine's store for every batch, and remembers where the copying is
	// to put its log. Called when the reserving pass is over, so a view's snapshot already covers
	// every frame the pass allocated.
	Status openImport(typename Local::FrameArena &arena, RunReport &report, uint32_t logBase) {
		_report = &report;
		_logBase = logBase;
		for (auto it : getBatchList()) {
			it->status = Status::Ok;
			if (it->writeView.isInitialized()) {
				it->writeView.closeView();
			}
			auto st = it->writeView.initWriteView(arena);
			if (st != Status::Ok) {
				return st;
			}
		}
		return Status::Ok;
	}

	// Takes the pages the workers marked, and the windows back.
	void closeImport(typename Local::FrameArena &arena) {
		for (auto it : getBatchList()) {
			if constexpr (Local::FrameArena::IsTracked) {
				arena.mergeDirtyFrom(it->writeView);
			}
			it->writeView.closeView();
		}
		_report = nullptr;
	}

	uint32_t getBlock() const { return _block; }
	uint32_t getEpoch() const { return _epoch; }

	// The batches this launch is using. A recycled job keeps more than it needs; the ones past the
	// count belong to a wider block it ran before.
	SpanView<Batch *> getBatchList() const { return SpanView<Batch *>(_list.data(), _batches); }

protected:
	virtual void performBatch(Phase phase, uint32_t index) override {
		switch (phase) {
		case Phase::Prepare: prepareBatch(*_list[index]); break;
		case Phase::Run: runBranches(*_list[index]); break;
		case Phase::Import: importRows(index); break;
		}
	}

	void importRows(uint32_t index) {
		auto &batch = *_list[index];
		ImportTargetT<Local> target{&_parent->_local, &batch.writeView};
		batch.status = _parent->importBatchRows(*this, target, index, *_report, _logBase);
	}

	void prepareBatch(Batch &batch) {
		auto &parent = *_parent;
		batch.status = batch.machine.beginBranches(*parent._graph, *parent._ops, _view, _config,
				batch.report);
		if (batch.status == Status::Ok) {
			batch.status = batch.machine.seedBranches(parent._local, _ticket.node, _ticket.activation);
		}
		if (batch.status == Status::Ok) {
			batch.rootFirst = batch.machine._local.getActivationCount();
			batch.status = batch.machine.openBranches(_ticket.node,
					SpanView<value::EntityId>(_entities.data() + batch.first, batch.count),
					batch.first);
		}
	}

	void runBranches(Batch &batch) {
		while (!isCancelled() && batch.machine.stepOnce(batch.report)) { }
		if (batch.report.outcome == RunOutcome::Cancelled) {
			cancel(); // the frame does not happen; the other batches need not finish it
		}
	}

private:
	Parent *_parent = nullptr;
	uint32_t _block = InvalidIndex;
	uint32_t _epoch = 0;
	ArenaType _view;
	typename Local::SceneType _viewStore;
	typename Runner::Config _config;
	mem_std::Vector<value::EntityId> _entities;
	mem_std::Vector<Batch *> _list;

	// The import phase's two parameters, held for the length of the phase only.
	RunReport *_report = nullptr;
	uint32_t _logBase = 0;
};

template <typename Graph, typename Local, typename Trace>
void MachineT<Graph, Local, Trace>::prepareParallel() {
	_blockExecutors.clear();
	_blockShapes.clear();
	_freezes.clear();
	_threaded = 0;
	if (!_graph || _graph->getBlockCount() == 0) {
		return;
	}
	for (uint32_t b = 0; b < _graph->getBlockCount(); ++b) {
		_blockExecutors.emplace_back(readBlockExecutors(*_graph, b));

		// The block's shape, worked out once. `bodyNodes` is not a table field: a scope says which
		// parallel scope it lies within, so the body's nodes are the scopes that answer this one -
		// the nested loop bodies among them.
		auto &block = _graph->getBlockAt(b);
		BlockShape shape;
		shape.block = b;
		shape.writeCount = block.writeCount;
		shape.collectorCount = block.collectorCount;
		shape.maxSteps = block.maxSteps;
		shape.flatFrames = isFlatSchema(_local.getStateType());
		for (uint32_t sc = 0; sc < _graph->getScopeCount(); ++sc) {
			auto &scope = _graph->getScopeAt(sc);
			if (scope.branchScope != block.scope) {
				continue;
			}
			shape.bodyNodes += scope.nodeCount;
			shape.nested = shape.nested || sc != block.scope;
			for (auto n : _graph->getScopeNodes(sc)) {
				shape.flatFrames = shape.flatFrames && isFlatSchema(_graph->getNodeAt(n).localSchema);
			}
		}
		for (auto &write : _graph->getBlockWrites(b)) {
			shape.rowBytes += value::getTypeSize(write.type) + 1; // the value and its flag
			shape.flatFrames = shape.flatFrames && !value::isContainerType(write.type);
		}
		_blockShapes.emplace_back(shape);
	}
	_freezes.resize(_graph->getNodeCount(), 0);
	for (uint32_t n = 0; n < _graph->getNodeCount(); ++n) {
		auto op = _graph->getNodeAt(n).op;
		_freezes[n] = op && op->getParallel() == OpParallel::Serial
				&& (op->getFlags() & (OpFlags::WritesScene | OpFlags::HostCall)) != OpFlags::None;
	}
}

template <typename Graph, typename Local, typename Trace>
uint32_t MachineT<Graph, Local, Trace>::bodyEntry(uint32_t fanOut) const {
	auto &node = _graph->getNodeAt(fanOut);
	auto &scope = _graph->getScopeAt(node.opensScope);
	for (auto e : _graph->getExecOutEdges(fanOut)) {
		auto &edge = _graph->getExecEdges()[e];
		if (edge.srcPin == scope.execPin) {
			return edge.dstNode;
		}
	}
	return InvalidIndex;
}

template <typename Graph, typename Local, typename Trace>
void MachineT<Graph, Local, Trace>::noteThreaded(int32_t delta) {
	auto before = _threaded;
	_threaded = uint32_t(int32_t(_threaded) + delta);
	if constexpr (ArenaType::HasShadow) {
		if (_scene && _scene->getArena()) {
			if (before == 0 && _threaded > 0) {
				_scene->getArena()->setFrozen(true);
			} else if (before > 0 && _threaded == 0) {
				_scene->getArena()->setFrozen(false);
			}
		}
	}
}

template <typename Graph, typename Local, typename Trace>
bool MachineT<Graph, Local, Trace>::launchBlock(uint32_t fanOut, uint32_t activation,
		SpanView<value::EntityId> entities, BlockLaunch choice) {
	if (!_executor || _branchMode || entities.empty()) {
		return false;
	}
	auto &fanOutNode = _graph->getNodeAt(fanOut);
	auto blockIndex = _graph->getScopeAt(fanOutNode.opensScope).block;
	BlockTicket ticket{fanOut, activation, _generation};
	// `Machine` here means the caller has not asked yet (refreshPending relaunches what a rollback
	// left in phase Launched).
	if (choice.kind == BlockLaunchKind::Machine) {
		choice = _executor->chooseLaunch(ticket, makeLaunchInfo(blockIndex, uint32_t(entities.size())));
	}
	if (choice.kind == BlockLaunchKind::Gpu) {
		auto info = makeLaunchInfo(blockIndex, uint32_t(entities.size()));
		if (info.gpu) {
			auto job = new GpuBlockJobT<Graph, Local, Trace>();
			if (job->prepare(*this, ticket, *info.gpu, entities) == Status::Ok) {
				_flights.emplace_back(Flight{fanOut, activation, _clock ? _clock->nowMs() : 0,
					flightTimeout(blockIndex, ticket, choice, uint32_t(entities.size())), false, job});
				noteThreaded(1);
				if (_executor->launchGpu(*job)) {
					return true;
				}
				// Refused right now (no device, no buffers): take the flight back and ask again, so
				// the block falls to threads or to the front in this same frame.
				_flights.pop_back();
				noteThreaded(-1);
			}
			delete job;
		}
		choice = _executor->chooseLaunch(ticket, BlockLaunchInfo{uint32_t(entities.size()),
			blockIndex < _blockExecutors.size() ? _blockExecutors[blockIndex] : uint32_t(0), nullptr, nullptr});
	}
	if (choice.kind == BlockLaunchKind::Threads && choice.batch > 0) {
		auto job = takeBranchJob();
		// Building the batches is a phase of its own: the executor runs it over every batch and the
		// machine waits here, which is exactly where it waited when it built them itself. Only
		// after it is built does the job become a flight - a half-built job has no branches to take
		// back.
		if (job->plan(*this, ticket, blockIndex, entities, choice.batch) != Status::Ok) {
			giveBranchJob(job);
			return false;
		}
		// Every batch seeds itself by reading this store, and the read goes through runRecord()'s
		// memo - which is `mutable`, not atomic, and would otherwise be filled by whichever worker
		// got there first. Filling it here makes every one of those reads a read. The stamp cannot
		// go stale meanwhile: the arena's epoch moves only when a row is swap-removed or the bytes
		// are replaced wholesale, and this thread is standing still until the phase is over.
		(void)_local.getActivationCount();
		_executor->perform(*job);
		job->waitDone();
		if (job->getStatus() != Status::Ok) {
			giveBranchJob(job);
			return false;
		}
		job->beginPhase(BranchJob::Phase::Run);
		_flights.emplace_back(Flight{fanOut, activation, _clock ? _clock->nowMs() : 0,
			flightTimeout(blockIndex, ticket, choice, uint32_t(entities.size())), false, job});
		noteThreaded(1);
		_executor->launch(*job);
		return true;
	}
	return false;
}

// What an executor is told about a block when it chooses: the author's list, and the lowered body
// with the host's shaders when the graph carries them.
template <typename Graph, typename Local, typename Trace>
BlockLaunchInfo MachineT<Graph, Local, Trace>::makeLaunchInfo(uint32_t block, uint32_t branchCount) const {
	BlockLaunchInfo info;
	info.branchCount = branchCount;
	info.allowed = block < _blockExecutors.size() ? _blockExecutors[block] : 0;
	info.gpuTable = _graph->getGpuShaders();
	info.gpu = _graph->getGpuBlock(block);
	if (block < _blockShapes.size()) {
		info.shape = _blockShapes[block];
	}
	return info;
}

// The author's `timeout` is authoritative; 0 means "the executor's own default", and this is where
// the executor is finally asked for one. A block on the machine's own front gets none: that is the
// machine's own time, not a flight's.
template <typename Graph, typename Local, typename Trace>
uint64_t MachineT<Graph, Local, Trace>::flightTimeout(uint32_t block, const BlockTicket &ticket,
		const BlockLaunch &launch, uint32_t branchCount) const {
	if (block >= _graph->getBlockCount()) {
		return 0;
	}
	if (auto authored = _graph->getBlockAt(block).timeoutMs) {
		return authored;
	}
	if (!_executor || launch.kind == BlockLaunchKind::Machine) {
		return 0;
	}
	return _executor->chooseTimeoutMs(ticket, makeLaunchInfo(block, branchCount), launch);
}

template <typename Graph, typename Local, typename Trace>
BranchJobT<Graph, Local, Trace> *MachineT<Graph, Local, Trace>::takeBranchJob() {
	if (!_jobPool.empty()) {
		auto job = _jobPool.back();
		_jobPool.pop_back();
		++_jobsReused;
		return job;
	}
	++_jobsCreated;
	return new BranchJobT<Graph, Local, Trace>();
}

// Only ever reached after waitDone(): until then a batch's arena belongs to its worker.
template <typename Graph, typename Local, typename Trace>
void MachineT<Graph, Local, Trace>::giveBranchJob(BranchJobT<Graph, Local, Trace> *job) {
	job->recycle();
	// A pooled job holds an arena and a memory pool per batch it ever had, so the pool is capped by
	// count rather than trimmed by size: a graph with a handful of blocks keeps all of them warm,
	// and nothing else accumulates. A job past the cap is given back to the host outright.
	if (_jobPool.size() >= JobPoolLimit) {
		delete job;
		return;
	}
	_jobPool.emplace_back(job);
}

template <typename Graph, typename Local, typename Trace>
void MachineT<Graph, Local, Trace>::clearJobPool() {
	for (auto it : _jobPool) { delete it; }
	_jobPool.clear();
}

template <typename Graph, typename Local, typename Trace>
void MachineT<Graph, Local, Trace>::abandonFlights() {
	for (auto it = _flights.begin(); it != _flights.end();) {
		if (!it->job) {
			++it;
			continue;
		}
		it->job->cancel();
		it->job->waitDone();
		if (_executor) {
			if (it->job->getKind() == BranchJob::Kind::Gpu) {
				_executor->forgetGpu(*static_cast<GpuBlockJob *>(it->job));
			} else {
				_executor->forget(*it->job);
			}
		}
		if (it->job->getKind() == BranchJob::Kind::Gpu) {
			delete it->job;
		} else {
			giveBranchJob(static_cast<BranchJobT<Graph, Local, Trace> *>(it->job));
		}
		it = _flights.erase(it);
		noteThreaded(-1);
	}
	_hasPolled = false;
}

template <typename Graph, typename Local, typename Trace>
Status MachineT<Graph, Local, Trace>::beginBranches(const Graph &graph, const OpRegistry &ops,
		ArenaType &arena, const Config &config, RunReport &report) {
	report = RunReport();
	reset();
	auto st = _local.init(arena, graph, ops.getLocalTypes());
	if (st != Status::Ok) {
		return st;
	}
	_graph = &graph;
	_ops = &ops;
	_scene = config.scene;
	_extensions = config.extensions;
	_maxSteps = config.maxSteps ? config.maxSteps : graph.getNodeCount() * 4 + 16;
	_maxActivations = config.maxActivations;
	_state = RunState::Paused;
	_frontWeights = config.frontWeights;
	_local.setWeighted(config.frontWeights);
	_branchMode = true;

	// The four `begin()` clears, every one of which matters the moment a machine is used twice:
	// `report.stepCount - _branchUnits` is unsigned, and a fresh report against a stale count
	// underflows into a budget of four billion.
	_activationsExhausted = false;
	_branchUnits = 0;
	_quantumBranchUnits = 0;
	_budgetFailure = BudgetFailure();
	_status = Status::Ok;
	return Status::Ok;
}

template <typename Graph, typename Local, typename Trace>
template <typename Source>
Status MachineT<Graph, Local, Trace>::seedBranches(const Source &src, uint32_t fanOut,
		uint32_t activation) {
	// The activations from the root down to the fan-out's, opened here in the same shape.
	mem_std::Vector<uint32_t> chain;
	for (auto a = activation; a != RootActivation && a != NullActivation; a = src.readActivation(a).parent) {
		chain.emplace_back(a);
	}
	mem_std::Vector<Pair<uint32_t, uint32_t>> mapped;
	mapped.emplace_back(RootActivation, RootActivation);
	auto map = [&](uint32_t a) {
		for (auto &it : mapped) {
			if (it.first == a) {
				return it.second;
			}
		}
		return NullActivation;
	};
	for (uint32_t i = uint32_t(chain.size()); i > 0; --i) {
		auto a = chain[i - 1];
		auto data = src.readActivation(a);
		auto key = data.openerKey == NullRecordKey
				? NullRecordKey
				: makeRecordKey(recordKeyNode(data.openerKey), map(recordKeyActivation(data.openerKey)));
		uint32_t opened = NullActivation;
		auto st = _local.openActivation(data.scope, map(data.parent), data.iteration, key, opened);
		if (st == Status::Ok) {
			st = _local.materializeScope(data.scope, opened);
		}
		if (st != Status::Ok) {
			return st;
		}
		mapped.emplace_back(a, opened);
	}

	// The rows the body reads from outside itself - the fan-out's, and every producer of an input
	// of the body - as they stand now: the build guarantees none of them changes before delivery.
	auto copy = [&](uint32_t node, uint32_t srcActivation) {
		auto dstActivation = map(srcActivation);
		if (dstActivation == NullActivation) {
			return Status::ErrorInvalidArguemnt;
		}
		return copyNodeRows(_local, _local.activationFrame(dstActivation), src,
				src.activationFrame(srcActivation), *_graph, node);
	};
	auto st = copy(fanOut, activation);
	for (auto e : _graph->getCrossScopeInEdges(fanOut)) {
		if (st != Status::Ok) {
			break;
		}
		auto srcNode = _graph->getDataEdges()[e].srcNode;
		st = copy(srcNode, src.resolveScope(activation, _graph->getNodeAt(srcNode).scope));
	}
	_seedActivation = map(activation);
	return st;
}

template <typename Graph, typename Local, typename Trace>
Status MachineT<Graph, Local, Trace>::openBranches(uint32_t fanOut,
		SpanView<value::EntityId> entities, uint32_t firstIndex) {
	auto activation = _seedActivation;
	auto scope = _graph->getNodeAt(fanOut).opensScope;
	auto firstNode = bodyEntry(fanOut);
	if (firstNode == InvalidIndex) {
		return Status::ErrorInvalidArguemnt;
	}
	uint32_t first = _local.getActivationCount();
	for (uint32_t i = 0; i < uint32_t(entities.size()); ++i) {
		uint32_t opened = 0;
		auto st = _local.openActivation(scope, activation, firstIndex + i, makeRecordKey(fanOut, activation),
				opened);
		if (st == Status::Ok) {
			st = _local.materializeScope(scope, opened);
		}
		if (st != Status::Ok) {
			return st;
		}
		_local.pushOpen(opened);
		writeBranchEntity(*_local.getArena(), _local.activationFrame(opened), entities[i]);
	}
	mem_std::Vector<uint64_t> ready;
	for (uint32_t i = 0; i < uint32_t(entities.size()); ++i) {
		auto act = first + i;
		auto frame = _local.activationFrame(act);
		_local.addFlags(_local.getStateIn(frame, firstNode), NodeFlags::Token);
		for (auto n : _graph->getScopeNodes(scope)) {
			if (isReadyIn(frame, n, act)) {
				ready.emplace_back(makeRecordKey(n, act));
			}
		}
	}
	for (uint32_t i = uint32_t(ready.size()); i > 0; --i) {
		enqueue(recordKeyNode(ready[i - 1]), recordKeyActivation(ready[i - 1]));
	}
	return Status::Ok;
}

// The results of every batch brought back into this machine's store, in branch order: the branch
// frames, the activations the branches opened under themselves, their log and their findings.
template <typename Graph, typename Local, typename Trace>
Status MachineT<Graph, Local, Trace>::importBranches(BranchJobT<Graph, Local, Trace> &job, uint32_t first,
		RunReport &report, bool &cancelled) {
	auto blockIndex = job.getBlock();
	auto &block = _graph->getBlockAt(blockIndex);
	cancelled = false;

	if constexpr (ArenaType::HasShadow) {
		if (_scene) {
			sprt_passert(_scene->getArena()->getEpoch() == job.getEpoch(),
					"MachineT: the scene moved rows while branches were reading it");
		}
	}

	// The insertion points, all of them, before a single byte is copied. Everything here either
	// allocates or numbers something, and both are the machine's alone: an activation's id is the
	// order it was opened in, and the allocator is the one part of a store two threads may never be
	// in at once. What comes out is, for every branch and every activation under it, the exact Addr
	// its bytes will land at - so the copying itself needs nothing but those addresses, and the
	// copying is all of the work. The walk is the one the single-threaded import makes - batches in
	// order, branches in order, a branch's own activations in their order - because that walk is
	// the numbering.
	uint32_t imported = 0;
	const uint32_t logBase = uint32_t(report.log.size());
	for (auto batch : job.getBatchList()) {
		auto &src = batch->machine._local;
		auto count = src.getActivationCount();

		// Which branch of the batch each of its activations belongs to, and where it lands here.
		batch->owner.assign(count, NullActivation);
		batch->map.assign(count, NullActivation);
		batch->frame.assign(count, NullAddr);
		batch->plan.assign(batch->count, BranchPlan());
		for (uint32_t a = batch->rootFirst; a < count; ++a) {
			if (a < batch->rootFirst + batch->count) {
				batch->owner[a] = a;
			} else {
				auto parent = src.readActivation(a).parent;
				batch->owner[a] = parent < count ? batch->owner[parent] : NullActivation;
			}
		}

		for (uint32_t j = 0; j < batch->count; ++j) {
			auto root = batch->rootFirst + j;
			auto target = first + batch->first + j;
			auto &plan = batch->plan[j];
			batch->map[root] = target;
			auto targetFrame = _local.activationFrame(target);
			auto rootFrame = src.activationFrame(root);
			auto status = readBranchInt(*src.getArena(), rootFrame, BranchHeader::StatusOffset);
			if ((status & BranchHeader::Closed) == 0) {
				// Never finished: a timeout or a cancel broke it off. Written here and not by a
				// worker - there is nothing else of this branch to copy.
				auto here = readBranchInt(*_local.getArena(), targetFrame, BranchHeader::StatusOffset);
				writeBranchInt(*_local.getArena(), targetFrame, BranchHeader::StatusOffset,
						here | BranchHeader::Failed | BranchHeader::Closed);
				cancelled = cancelled || block.onFailure == ParallelFailure::CancelFrame;
				continue;
			}
			cancelled = cancelled
					|| ((status & BranchHeader::Failed) != 0 && block.onFailure == ParallelFailure::CancelFrame);
			plan.copy = true;
			plan.targetFrame = targetFrame;
			plan.rootFrame = rootFrame;
			batch->frame[root] = targetFrame;

			// The activations it opened under itself, appended in their own order.
			for (uint32_t a = batch->rootFirst + batch->count; a < count; ++a) {
				if (batch->owner[a] != root) {
					continue;
				}
				if (_local.getActivationCount() >= _maxActivations) {
					_activationsExhausted = true;
					break;
				}
				auto data = src.readActivation(a);
				auto key = makeRecordKey(recordKeyNode(data.openerKey),
						batch->map[recordKeyActivation(data.openerKey)]);
				uint32_t opened = NullActivation;
				auto st = _local.openActivation(data.scope, batch->map[data.parent], data.iteration,
						key, opened);
				if (st == Status::Ok) {
					st = _local.materializeScope(data.scope, opened);
				}
				if (st != Status::Ok) {
					return st;
				}
				batch->map[a] = opened;
				batch->frame[a] = _local.activationFrame(opened);
			}

			// Where this branch's log entries go, and what their step numbers are. Both are a
			// running count over the whole import, so they are settled here and the workers only
			// fill in.
			if constexpr (Trace::Log) {
				plan.logFirst = imported;
				for (auto &it : batch->report.log) {
					if (it.activation < count && batch->owner[it.activation] == root) {
						++plan.logCount;
					}
				}
				imported += plan.logCount;
			}
		}
		if constexpr (!Trace::Log) {
			imported += batch->report.stepCount;
		}
	}
	if constexpr (Trace::Log) {
		report.log.resize(logBase + imported);
	}

	// The copying, which is everything else, and which allocates nothing when the frames are flat.
	auto st = importRows(job, report, logBase);
	if (st != Status::Ok) {
		return st;
	}

	// What is left is a handful of words, and it stays here because it is ordered.
	for (auto batch : job.getBatchList()) {
		if (batch->report.diagnostics.isArray()) {
			for (auto &it : batch->report.diagnostics.asArray()) {
				report.diagnostics.addValue(it);
			}
		}
	}

	report.stepCount += imported;
	_branchUnits += imported;
	return Status::Ok;
}

// One batch's frames copied into the machine's store, at the addresses the reserving pass worked
// out. Nothing here allocates and nothing here numbers anything: given `dst`, it is a function of
// the batch alone, which is what lets the batches run at once.
template <typename Graph, typename Local, typename Trace>
template <typename Dst>
Status MachineT<Graph, Local, Trace>::importBatchRows(BranchJobT<Graph, Local, Trace> &job, Dst &dst,
		uint32_t batchIndex, RunReport &report, uint32_t logBase) {
	auto batch = job.getBatchList()[batchIndex];
	auto &src = batch->machine._local;
	auto branchScope = _graph->getNodeAt(job.getTicket().node).opensScope;
	auto headerBytes = _graph->getScopeAt(branchScope).headerBytes;
	auto count = src.getActivationCount();

	for (uint32_t j = 0; j < batch->count; ++j) {
		auto &plan = batch->plan[j];
		if (!plan.copy) {
			continue;
		}
		auto root = batch->rootFirst + j;

		// The header - the status, the budgets, the entity and the copies - is fixed-size bytes.
		__sprt_memcpy(dst.getArena()->write(plan.targetFrame, headerBytes),
				src.getArena()->read(plan.rootFrame, headerBytes), headerBytes);
		for (auto n : _graph->getScopeNodes(branchScope)) {
			auto st = copyNodeRows(dst, plan.targetFrame, src, plan.rootFrame, *_graph, n);
			if (st != Status::Ok) {
				return st;
			}
		}

		for (uint32_t a = batch->rootFirst + batch->count; a < count; ++a) {
			if (batch->owner[a] != root || batch->map[a] == NullActivation) {
				continue; // not this branch's, or past the activation ceiling
			}
			auto data = src.readActivation(a);
			auto frame = batch->frame[a];
			auto from = src.activationFrame(a);
			for (auto n : _graph->getScopeNodes(data.scope)) {
				auto st = copyNodeRows(dst, frame, src, from, *_graph, n);
				if (st != Status::Ok) {
					return st;
				}
			}
		}

		if constexpr (Trace::Log) {
			uint32_t k = 0;
			for (auto &it : batch->report.log) {
				if (it.activation >= count || batch->owner[it.activation] != root) {
					continue;
				}
				auto &record = report.log[logBase + plan.logFirst + k];
				record = it;
				record.activation = batch->map[it.activation];
				record.step = report.stepCount + plan.logFirst + k;
				record.baseVersion = _quantumBase;
				record.version = 0;
				++k;
			}
		}
	}
	return Status::Ok;
}

/* Every batch's rows, on whichever threads the executor has. The single-threaded road is not a
fallback but the honest answer for a block whose frames are not flat: a container field deep-copies,
a deep copy allocates, and an allocator takes one thread at a time. `flatFrames` is the whole of the
condition, and it is a property of the graph. The machine stands here either way, which is what
makes this safe beyond the flatness: its own front is not running, so nothing is growing the store
under the views, and nothing but the workers is touching it. */
template <typename Graph, typename Local, typename Trace>
Status MachineT<Graph, Local, Trace>::importRows(BranchJobT<Graph, Local, Trace> &job, RunReport &report,
		uint32_t logBase) {
	auto blockIndex = job.getBlock();
	const bool flat = blockIndex < _blockShapes.size() && _blockShapes[blockIndex].flatFrames;
	// Asked before anything is built: a write view per batch is cheap but not free, and a narrow
	// block whose phase the executor would run here anyway must not pay for one.
	if (!flat || !_executor || !_executor->takesPhase(job)) {
		for (uint32_t i = 0; i < job.getBatchCount(); ++i) {
			auto st = importBatchRows(job, _local, i, report, logBase);
			if (st != Status::Ok) {
				return st;
			}
		}
		return Status::Ok;
	}

	auto st = job.openImport(*_local.getArena(), report, logBase);
	if (st != Status::Ok) {
		job.closeImport(*_local.getArena()); // whichever windows it managed to open
		return st;
	}
	job.beginPhase(BranchJob::Phase::Import);
	_executor->perform(job);
	job.waitDone();
	job.closeImport(*_local.getArena());
	return job.getStatus();
}

/* The records of a dispatch, taken back as if the branches had run here. `writeGpuBranch` rebuilds
the frames themselves - the header, the copies, every node's state and every cell. What it cannot
know is what a run wrote down while it happened: the log with the pins each unit fired, the failure
of a branch with its diagnostic, the stalls of a branch that closed with a node still holding a
token, and the counters. Those are here, in the order and with the wording `stepNode`, `failBranch`
and `closeActivation` use, because an oracle compares them against serial. */
template <typename Graph, typename Local, typename Trace>
Status MachineT<Graph, Local, Trace>::importGpu(GpuBlockJobT<Graph, Local, Trace> &job, uint32_t first,
		uint64_t timeoutMs, RunReport &report, bool &cancelled) {
	auto &shaders = job.getShaders();
	auto &p = shaders.program;
	auto blockIndex = p.block;
	auto &block = _graph->getBlockAt(blockIndex);
	auto branchScope = _graph->getNodeAt(job.getTicket().node).opensScope;
	auto branches = job.getBranchCount();
	const bool cancelFrame = block.onFailure == ParallelFailure::CancelFrame;
	cancelled = false;

	// Nothing came back: every branch is broken off, and the break is reported once for the block
	// (a timeout has already reported itself in checkTimeouts).
	if (!job.hasResults()) {
		for (uint32_t i = 0; i < branches; ++i) {
			auto frame = _local.activationFrame(first + i);
			auto status = readBranchInt(*_local.getArena(), frame, BranchHeader::StatusOffset);
			writeBranchInt(*_local.getArena(), frame, BranchHeader::StatusOffset,
					status | BranchHeader::Failed | BranchHeader::Closed);
		}
		cancelled = cancelFrame;
		auto abort = job.getAbort();
		if (abort != GpuAbort::None && abort != GpuAbort::Cancelled) {
			auto &rt = _graph->getNodeAt(job.getTicket().node);
			int64_t values[] = {int64_t(rt.id), int64_t(timeoutMs), block.timeoutMs == 0 ? 1 : 0};
			auto message = DiagDetail::ParallelGpuLost;
			if (abort == GpuAbort::Silent) {
				message = DiagDetail::ParallelGpuSilent;
			} else if (abort == GpuAbort::Invalid) {
				message = DiagDetail::ParallelGpuInvalid;
			}
			reportRun<EnvType>(report, cancelFrame ? DiagSeverity::Error : DiagSeverity::Warning,
					DiagCode::ParallelGpuLost, DiagText(message).number(int64_t(timeoutMs)),
					DiagLocus::Timeout, values);
		}
		return Status::Ok;
	}

	uint32_t units = 0;
	bool concluded = false;
	for (uint32_t i = 0; i < branches; ++i) {
		auto act = first + i;
		auto frame = _local.activationFrame(act);
		if (concluded) {
			// Serial stopped at the failure under cancelFrame: the branches after it never ran.
			auto status = readBranchInt(*_local.getArena(), frame, BranchHeader::StatusOffset);
			writeBranchInt(*_local.getArena(), frame, BranchHeader::StatusOffset,
					status | BranchHeader::Failed | BranchHeader::Closed);
			continue;
		}
		auto &result = job.getResult(i);
		auto st = writeGpuBranch(*_graph, _local, p, act, result);
		if (st != Status::Ok) {
			return st;
		}

		for (uint32_t k = 0; k < uint32_t(result.path.log.size()); ++k) {
			auto node = p.bodyNodes[result.path.log[k]];
			if constexpr (Trace::Log) {
				RunStep record;
				record.step = report.stepCount + units;
				record.kind = RunStepKind::Node;
				record.node = node;
				record.activation = act;
				record.id = _graph->getNodeAt(node).id;
				record.fired = k < result.path.fired.size() ? result.path.fired[k] : 0;
				record.baseVersion = _quantumBase;
				report.log.emplace_back(record);
			} else {
				_lastNode = node;
				_lastActivation = act;
			}
			++units;
		}

		if (result.failed) {
			auto node = result.failNode < uint32_t(p.bodyNodes.size())
					? p.bodyNodes[result.failNode]
					: job.getTicket().node;
			auto &rt = _graph->getNodeAt(node);
			int64_t values[] = {int64_t(rt.id), int64_t(toInt(result.failStatus)), int64_t(i)};
			StringView names[] = {rt.op ? rt.op->getName() : StringView()};
			reportRun<EnvType>(report, cancelFrame ? DiagSeverity::Error : DiagSeverity::Warning,
					DiagCode::ParallelBranchFailed,
					DiagText(result.budget ? DiagDetail::ParallelBranchBudget
										   : DiagDetail::ParallelBranchFailed)
							.name(names[0])
							.number(int64_t(i)),
					DiagLocus::Branch, values, names);
			if constexpr (Trace::Log) {
				RunStep record;
				record.step = report.stepCount + units;
				record.kind = RunStepKind::BranchFailed;
				record.node = node;
				record.activation = act;
				record.id = rt.id;
				record.baseVersion = _quantumBase;
				report.log.emplace_back(record);
			} else {
				_lastNode = node;
				_lastActivation = act;
			}
			++units; // the unit that refused counts, as it does under serial
			if (cancelFrame) {
				// The run ends with that unit (failBranch concludes): no close, and no branch after
				// it.
				cancelled = true;
				concluded = true;
				continue;
			}
		} else {

		// A branch that closed with a node still holding a token stalls it, exactly as
		// closeActivation does, in the scope's own order. A failed branch stalls nothing: its front
		// is dropped.
		for (uint32_t n = 0; n < uint32_t(p.bodyNodes.size()); ++n) {
			if (!result.path.nodes[n].stalled) {
				continue;
			}
			auto node = p.bodyNodes[n];
			_local.setStall(_local.getStateIn(frame, node), NodeFlags::StallData, 0);
			_local.pushStalled(node, act);
			int64_t values[] = {int64_t(_graph->getNodeAt(node).id), int64_t(i)};
			reportRun<EnvType>(report, DiagSeverity::Error, DiagCode::Deadlock,
					DiagText(DiagDetail::Deadlock), DiagLocus::NodeIteration, values);
		}

		}

		// Every branch that was not concluded closes, failed or not (closeActivation).
		if constexpr (Trace::Log) {
			RunStep record;
			record.step = report.stepCount + units;
			record.kind = RunStepKind::CloseActivation;
			record.node = job.getTicket().node;
			record.activation = act;
			record.id = _graph->getNodeAt(job.getTicket().node).id;
			record.baseVersion = _quantumBase;
			report.log.emplace_back(record);
		}
		++units;
	}
	(void)branchScope;

	report.stepCount += units;
	_branchUnits += units;
	if (!cancelled) {
		presetCollectors(job, job.getTicket().activation);
	}
	return Status::Ok;
}

// The folds the device did: the value goes into the collector's record, and the flag says it is
// there. A collector without one folds on the CPU when it runs, which is the same answer in branch
// order rather than in tree order.
template <typename Graph, typename Local, typename Trace>
void MachineT<Graph, Local, Trace>::presetCollectors(GpuBlockJobT<Graph, Local, Trace> &job,
		uint32_t activation) {
	auto &shaders = job.getShaders();
	for (uint32_t r = 0; r < uint32_t(shaders.reducers.size()); ++r) {
		Var folded;
		if (!job.getReduced(r, folded)) {
			continue;
		}
		auto node = shaders.reducers[r].reducer.collector;
		auto &rt = _graph->getNodeAt(node);
		auto record = _local.getRecord(node, activation);
		if (!rt.localSchema || record == NullAddr || rt.localSchema->getFields().empty()) {
			continue;
		}
		// A collector has one output, and a record's fields are indexed by output pin.
		if (rt.localSchema->setField(*_local.getArena(), record, rt.localSchema->getFields()[0], folded)
				== Status::Ok) {
			_local.addFlags(_local.getState(node, activation), NodeFlags::Folded);
		}
	}
}

} // namespace stappler::flow

#endif /* STAPPLER_FLOW_SPFLOWMACHINE_HPP_ */
