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

#ifndef STAPPLER_FLOW_SPFLOWENGINE_H_
#define STAPPLER_FLOW_SPFLOWENGINE_H_

#include "SPFlowMachine.h"

// The executor a host holds: one interface, several machines.
// MachineT is a template over how the graph is represented and where the run's records live, and
// the interpreter and a compiled unit's run are two instantiations of it. A host that wants to run
// either - a debugger, a dispatcher, a game that picks its engine on the command line - cannot be a template over
// the pair, because which pair it gets is decided at run time by what the project handed it: a
// file, or a unit. So the machine's verbs are stated once more here as an abstract interface over
// the arena kind alone, and MachineEngineT adapts any machine to it. The machine itself is given no
// virtual functions - a vtable pointer moves every field of it, while an adapter costs one virtual
// call per unit of work, on top of a unit that is hundreds of instructions. What the interface
// carries is what a host reaches for: the run's verbs, the breakpoints, the resolver a watch is
// turned into an address by, and the read half of the local store. What it lacks is the graph -
// which representation the engine runs is the engine's business, and a host that needs to read the
// graph holds the graph it handed the engine.
namespace STAPPLER_VERSIONIZED stappler::flow {

// The read half of a run's store, whichever graph it runs: every question a host asks a paused run
// - which records exist, what a node's state is, where an activation hangs, what is on the front -
// and none of the writes. The machine is the only writer of its store, and a host that wants to
// change a record goes through resolveWatch and the arena, as the debugger's edit does. The two
// engines' local stores are different types, each laying its frames out over its own graph; this is
// the one type a host can hold for either.
template <typename A>
class SP_PUBLIC RunLocalViewT {
public:
	using ArenaType = A;

	virtual ~RunLocalViewT() = default;

	virtual bool isValid() const = 0;

	// The arena the run's records are in, kind-erased - and not necessarily `A`. The arena store
	// keeps its frames in the caller's arena, which is the scene's kind; the fast store keeps them
	// in a scratch PlainArena of its own whatever the scene is (SPFlowFast.h). A host reads a
	// record through this the way an operation reads one, and `A` stays what it always meant here:
	// the kind the scene is in.
	virtual value::ArenaRef getArenaRef() const = 0;

	virtual const value::ComponentType *getStateType() const = 0;

	// Frames and records.
	virtual Addr activationFrame(uint32_t activation) const = 0;
	virtual Addr frameFor(uint32_t node, uint32_t activation) const = 0;
	virtual uint32_t activationScope(uint32_t activation) const = 0;
	virtual bool hasRecord(uint32_t node, uint32_t activation) const = 0;
	virtual uint32_t getFrameBytes(uint32_t scope) const = 0;
	virtual Addr getRecord(uint32_t node, uint32_t activation) const = 0;
	virtual Addr getState(uint32_t node, uint32_t activation) const = 0;
	virtual Addr getRecordIn(Addr frame, uint32_t node) const = 0;
	virtual Addr getStateIn(Addr frame, uint32_t node) const = 0;
	virtual NodeStateData readState(Addr stateAddr) const = 0;
	virtual uint32_t getRecordCount() const = 0;
	virtual void forEachRecord(
			const Callback<bool(uint32_t node, uint32_t activation, Addr frame)> &) const = 0;

	// Activations.
	virtual uint32_t getActivationCount() const = 0;
	virtual ActivationData readActivation(uint32_t activation) const = 0;
	virtual uint32_t resolveScope(uint32_t activation, uint32_t scope) const = 0;
	virtual uint32_t getOpenCount() const = 0;
	virtual uint32_t getOpenAt(uint32_t index) const = 0;

	// The front, the stalled list, the counters.
	virtual bool peekReady(uint32_t &node, uint32_t &activation) const = 0;
	virtual uint32_t getReadyCount() const = 0;
	virtual uint32_t getStalledCount() const = 0;
	virtual uint64_t getStalledAt(uint32_t index) const = 0;
	virtual uint32_t getStep() const = 0;
	virtual uint32_t getPass() const = 0;

	virtual void describe(mem_std::Value &) const = 0;
};

// The executor. `A` is the arena kind the run and its scene live in; the graph is whatever the
// engine was made over, and it is not named here. The verbs are MachineT's with the graph argument
// dropped - an engine runs the one graph it holds - and their contracts are MachineT's word for
// word: run() is begin() followed by finish() with no second loop, stepOnce() answers whether the
// run goes on, stepBack() needs a journal, finish() stops at a breakpoint or at the end.
template <typename A, typename Env = NoEnv>
class SP_PUBLIC RunEngineT {
public:
	using ArenaType = A;
	using EnvType = Env;
	using SceneStore = typename Env::template Scene<A>;
	using Config = RunConfigT<A, Env>;

	virtual ~RunEngineT() = default;

	// Which engine this is, in the words a run's state will be written in: "interp" for the
	// interpreter, "arena" for a unit run over the arena store. A host records it beside the
	// outcome, so the record says who ran.
	virtual StringView getEngineName() const = 0;

	// Whether the engine holds a graph to run. An engine without one refuses every verb below
	// with RunOutcome::Invalid rather than running nothing quietly.
	virtual bool hasGraph() const = 0;

	virtual Status run(const OpRegistry &, A &, const Config &, RunReport &) = 0;
	virtual Status begin(const OpRegistry &, A &, const Config &, RunReport &) = 0;
	virtual Status attach(const OpRegistry &, A &, const Config &, RunReport &) = 0;
	virtual bool stepOnce(RunReport &) = 0;
	virtual Status stepBack(RunReport &) = 0;
	virtual Status finish(RunReport &) = 0;
	virtual RunState getState() const = 0;
	virtual uint64_t peekNext() const = 0;
	virtual void reset() = 0;

	// Whether an executor is running branches of this run on other threads right now: nothing but
	// the run may write the scene until they are delivered.
	virtual bool hasBlocksInFlight() const { return false; }

	virtual uint32_t addBreakpoint(const Breakpoint &) = 0;
	virtual bool removeBreakpoint(uint32_t id) = 0;
	virtual Status setBreakpointEnabled(uint32_t id, bool) = 0;
	virtual void clearBreakpoints() = 0;
	virtual SpanView<Breakpoint> getBreakpoints() const = 0;
	virtual const Breakpoint *getBreakpoint(uint32_t id) const = 0;
	virtual uint32_t getLastBreakpoint() const = 0;

	// flow::resolveWatch over the engine's graph and store. The scene is the caller's, because a
	// scene watch is resolved between runs too, when the engine holds no run at all.
	virtual WatchResolution resolveWatch(const Watch &, uint32_t activation, const SceneStore *,
			WatchTarget &out) const = 0;

	virtual const RunLocalViewT<A> &getLocal() const = 0;
};

// The name an engine states for itself - a property of the pair of policies a machine is over, and
// therefore a trait over the machine rather than a member of either policy: the built graph over
// the arena store is "interp", a unit over the arena store is "arena", a unit over the fast store
// is "fast". No primary definition: a machine nobody named is not an engine.
template <typename Machine>
struct RunEngineName;

// Any machine, behind the interface. Holds the machine by value and the graph by pointer; the
// graph, the registry and the arena outlive the run exactly as they must for the machine itself.
template <typename Machine>
class SP_PUBLIC MachineEngineT final
: public RunEngineT<typename Machine::ArenaType, typename Machine::EnvType>,
  private RunLocalViewT<typename Machine::ArenaType> {
public:
	using MachineType = Machine;
	using GraphType = typename Machine::GraphType;
	using LocalType = typename Machine::LocalType;
	using ArenaType = typename Machine::ArenaType;
	using EnvType = typename Machine::EnvType;
	using Base = RunEngineT<ArenaType, EnvType>;
	using SceneStore = typename Base::SceneStore;
	using Config = typename Base::Config;
	using View = RunLocalViewT<ArenaType>;

	~MachineEngineT() override = default;

	MachineEngineT() = default;
	explicit MachineEngineT(const GraphType &graph) : _graph(&graph) { }
	MachineEngineT(const MachineEngineT &) = delete;
	MachineEngineT &operator=(const MachineEngineT &) = delete;

	// The graph the engine runs. Changing it forgets a run in progress, because a run is one graph
	// and a store laid out for another is not a paused run of this one.
	void setGraph(const GraphType *);
	const GraphType *getGraph() const { return _graph; }

	// The machine itself, for a caller that holds the concrete type and wants what the interface
	// does not carry - the typed local store, for one.
	Machine &getMachine() { return _machine; }
	const Machine &getMachine() const { return _machine; }

	StringView getEngineName() const override { return RunEngineName<Machine>::Value; }
	bool hasGraph() const override { return _graph != nullptr; }

	Status run(const OpRegistry &, ArenaType &, const Config &, RunReport &) override;
	Status begin(const OpRegistry &, ArenaType &, const Config &, RunReport &) override;
	Status attach(const OpRegistry &, ArenaType &, const Config &, RunReport &) override;
	bool stepOnce(RunReport &report) override { return _machine.stepOnce(report); }
	Status stepBack(RunReport &report) override { return _machine.stepBack(report); }
	Status finish(RunReport &report) override { return _machine.finish(report); }
	RunState getState() const override { return _machine.getState(); }
	uint64_t peekNext() const override { return _machine.peekNext(); }
	void reset() override { _machine.reset(); }
	bool hasBlocksInFlight() const override { return _machine.hasBlocksInFlight(); }

	uint32_t addBreakpoint(const Breakpoint &b) override { return _machine.addBreakpoint(b); }
	bool removeBreakpoint(uint32_t id) override { return _machine.removeBreakpoint(id); }
	Status setBreakpointEnabled(uint32_t id, bool enabled) override {
		return _machine.setBreakpointEnabled(id, enabled);
	}
	void clearBreakpoints() override { _machine.clearBreakpoints(); }
	SpanView<Breakpoint> getBreakpoints() const override { return _machine.getBreakpoints(); }
	const Breakpoint *getBreakpoint(uint32_t id) const override {
		return _machine.getBreakpoint(id);
	}
	uint32_t getLastBreakpoint() const override { return _machine.getLastBreakpoint(); }

	WatchResolution resolveWatch(const Watch &, uint32_t activation, const SceneStore *,
			WatchTarget &out) const override;

	const View &getLocal() const override { return *this; }

private:
	// RunLocalViewT's reads, forwarded to the machine's store.

	bool isValid() const override { return _machine.getLocal().isValid(); }
	value::ArenaRef getArenaRef() const override { return _machine.getLocal().getArenaRef(); }
	const value::ComponentType *getStateType() const override {
		return _machine.getLocal().getStateType();
	}

	Addr activationFrame(uint32_t activation) const override;
	Addr frameFor(uint32_t node, uint32_t activation) const override;
	uint32_t activationScope(uint32_t activation) const override;
	bool hasRecord(uint32_t node, uint32_t activation) const override;
	uint32_t getFrameBytes(uint32_t scope) const override;
	Addr getRecord(uint32_t node, uint32_t activation) const override;
	Addr getState(uint32_t node, uint32_t activation) const override;
	Addr getRecordIn(Addr frame, uint32_t node) const override;
	Addr getStateIn(Addr frame, uint32_t node) const override;
	NodeStateData readState(Addr stateAddr) const override;
	uint32_t getRecordCount() const override;
	void forEachRecord(
			const Callback<bool(uint32_t, uint32_t, Addr)> &) const override;

	uint32_t getActivationCount() const override;
	ActivationData readActivation(uint32_t activation) const override;
	uint32_t resolveScope(uint32_t activation, uint32_t scope) const override;
	uint32_t getOpenCount() const override;
	uint32_t getOpenAt(uint32_t index) const override;

	bool peekReady(uint32_t &node, uint32_t &activation) const override;
	uint32_t getReadyCount() const override;
	uint32_t getStalledCount() const override;
	uint64_t getStalledAt(uint32_t index) const override;
	uint32_t getStep() const override;
	uint32_t getPass() const override;

	void describe(mem_std::Value &) const override;

	const GraphType *_graph = nullptr;
	Machine _machine;
};

} // namespace stappler::flow

#endif /* STAPPLER_FLOW_SPFLOWENGINE_H_ */
