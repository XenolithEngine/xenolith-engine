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

#ifndef STAPPLER_FLOW_SPFLOWMACHINE_H_
#define STAPPLER_FLOW_SPFLOWMACHINE_H_

#include "SPFlowContext.h"
#include "SPFlowStatic.h"
#include "SPVStoreJournal.h"

#include <sprt/cxx/mutex>
#include <sprt/cxx/condition_variable>

// The machine: it walks the graph, it does not linearize it. One machine, written once, over two
// policies (SPFlowStatic.h): how the graph is represented and where the run's records live. The
// interpreter is the machine over the built graph and the arena store (SPFlowInterp.h); a
// generated unit is the same machine over the static tables it carries. That is what makes "a
// generated program executes exactly as the interpreter does" a property of the construction rather
// than an agreement between two implementations - there is one propagate, one close of a turn, one
// sweep, and both engines are instantiations of it. What it does not know is as important as what
// it does: no operation, no arena kind, no idea whether its graph was built from a file or carried
// as constants. Everything it asks of either policy is a method the policy provides, and the list
// of those methods is the policy's contract.
namespace STAPPLER_VERSIONIZED stappler::flow {

// The GPU side of a block (SPFlowGpuJob.hpp): the machine knows only that an executor can be
// handed one; GpuBlockShaders and GpuShaderTable come from SPFlowOp.h.
class GpuBlockJob;

template <typename Graph, typename Local, typename Trace>
class GpuBlockJobT;

enum class RunOutcome : uint8_t {
	Invalid, // nothing ran: a graph that was not built, or a store that would not initialise
	Completed, // the front drained and nothing was left waiting for data
	Deadlock, // a node holding an exec token never got the values it needs
	OpError, // an operation returned a failure
	StepLimit, // the run hit its own ceiling instead of hanging
	ActivationLimit, // a loop opened more iterations than the run was allowed
	Cancelled, // a parallel block under `cancelFrame` failed; the host restores the frame
};

static constexpr uint32_t RunOutcomeCount = uint32_t(RunOutcome::Cancelled) + 1;

SP_PUBLIC StringView getRunOutcomeName(RunOutcome);

// The inverse, in the manner of getVarTypeName/readVarType. The project dispatcher's transitions
// may be conditioned on how a run ended, and the set of legal words there has to be this set. Reads
// the table above rather than carrying a copy of it, so the two cannot disagree.
SP_PUBLIC bool readRunOutcome(StringView, RunOutcome &);

// Where a run is between two calls. There is no `Running`: the machine has no thread of its own,
// so a run is either untouched, sitting between two units of work, or over.
enum class RunState : uint8_t {
	Idle, // begin() has not been called, or reset() undid it
	Paused, // between two units of work - the state a debugger looks at
	Finished, // the outcome in the report is final
	Suspended, // nothing to do until a parallel block is delivered; stepOnce asks again
};

SP_PUBLIC StringView getRunStateName(RunState);

// What one unit of work was.
enum class RunStepKind : uint8_t {
	Node, // an operation was invoked
	CloseActivation, // a loop iteration or a parallel branch ended
	Deliver, // a parallel block was delivered: copies committed, the barrier and collectors woken
	BranchFailed, // a step of a parallel branch failed, and the branch with it
};

// A parallel block whose branches have all closed, waiting to be delivered. `generation` is the
// machine's own counter and is not in the arena: a rollback or a stepBack renews every ticket, so a
// ticket handed out before one cannot deliver after it.
struct BlockTicket {
	uint32_t node = InvalidIndex; // the fan-out
	uint32_t activation = RootActivation; // the fan-out's activation
	uint64_t generation = 0;
};

// The executors a fan-out's `executors` setting permits.
enum class BlockExecutorMask : uint32_t {
	None = 0,
	Threads = 1 << 0,
	Gpu = 1 << 1,
};

/* A block's branches handed to an executor that runs them itself: cut into batches, each a run of
its own over a store of its own, performed on whatever thread the executor chooses. The machine
prepares the batches, and takes the results back when the executor says the job is done. `runBatch`
is called once per batch, on any thread; `cancel` is read between two units of work. */
class SP_PUBLIC BranchJob {
public:
	// What the machine takes back when the job is done: batches of branch runs, or the records of a
	// dispatch.
	enum class Kind : uint8_t {
		Branches,
		Gpu,
	};

	/* What a job is doing right now. A block's cost has three parts and only the middle one is
	divided among threads: the batches are built on the machine's thread, they run on the pool, and
	their frames are copied back on the machine's thread again. All three are phases, run over the
	same batches through the same executor, and the machine waits for them exactly where it already
	waited, so nothing observable moves. */
	enum class Phase : uint8_t {
		Prepare, // build each batch's store: its arena, its seed, its branches
		Run, // step the branches - the only phase that was ever on the pool
		Import, // copy each batch's frames into the machine's store
	};

	virtual ~BranchJob() = default;

	virtual Kind getKind() const { return Kind::Branches; }

	const BlockTicket &getTicket() const { return _ticket; }
	uint32_t getBatchCount() const { return _batches; }
	uint32_t getBranchCount() const { return _branches; }
	Phase getPhase() const { return _phase; }

	// Arms the job for a phase: every batch owes one `finish()` again. `Prepare` also clears the
	// cancel, because a recycled job carries the one its last run was stopped with and nothing else
	// ever cleared it.
	void beginPhase(Phase phase) {
		_phase = phase;
		if (phase == Phase::Prepare) {
			_cancel.store(false);
		}
		_remaining.store(_batches);
	}

	// Only the run obeys a cancel. A cancel says "stop stepping the branches" - it does not say
	// "leave behind what they already did", and a `partial` policy depends on that difference: a
	// block broken off by a timeout still commits every branch that closed before the break, and
	// the copying back is how it does. `Prepare` clears the flag anyway, so it never arrives there.
	void runBatch(uint32_t batch) {
		if (_phase != Phase::Run || !isCancelled()) {
			performBatch(_phase, batch);
		}
		finish();
	}

	// A job whose end waits on someone else's callback must become done here: the machine waits for
	// it with no time limit, and a stopped loop calls nothing back.
	void cancel() {
		_cancel.store(true);
		onCancel();
	}
	bool isCancelled() const { return _cancel.load(); }
	bool isDone() const { return _remaining.load() == 0; }

	void waitDone() {
		sprt::unique_lock<sprt::mutex> lock(_mutex);
		_done.wait(lock, [&] { return _remaining.load() == 0; });
	}

protected:
	virtual void performBatch(Phase, uint32_t) { }
	virtual void onCancel() { }

	// One step of the job is over; the job is done when none remain.
	void finish() {
		sprt::unique_lock<sprt::mutex> lock(_mutex);
		if (_remaining.load() > 0) {
			_remaining.fetch_sub(1);
		}
		_done.notify_all();
	}

	BlockTicket _ticket;
	uint32_t _batches = 0;
	uint32_t _branches = 0;
	Phase _phase = Phase::Run;
	sprt::atomic<bool> _cancel = false;
	sprt::atomic<uint32_t> _remaining = 0;
	sprt::mutex _mutex;
	sprt::condition_variable _done;
};

// Where one branch's bytes are going. Everything in it is settled by the machine before any of the
// copying starts, which is what makes the copying a function of the batch alone.
struct BranchPlan {
	Addr targetFrame = NullAddr; // the branch's frame in the machine's store
	Addr rootFrame = NullAddr; // and in the batch's own
	uint32_t logFirst = 0; // its first log entry, counted over the whole import
	uint32_t logCount = 0;
	bool copy = false; // false for a branch that never closed: there is nothing of it to take
};

// What a block is, in the numbers a cost is a function of. All of it is the build's: the machine
// works it out once per run and hands it over with every launch, so a cost model needs nothing of
// the graph itself.
struct BlockShape {
	uint32_t block = InvalidIndex; // the block's own index: a model keys its decisions by it
	uint32_t bodyNodes = 0; // the nodes of the body, the nested scopes included
	uint32_t writeCount = 0; // fields a branch writes on its own entity
	uint32_t rowBytes = 0; // one branch's copies: their values and their flags
	uint32_t collectorCount = 0;
	uint32_t maxSteps = 0; // the branch budget the build derived from the body
	bool nested = false; // a scope inside the body: the steps are not the node count

	/* Whether a branch's frames are flat: no field of the branch header's copies, and no field of
	any body node's record, is a container. It is the key to every parallel path a block's
	results can take, and it says exactly one thing: copying a branch's frame into the machine's
	store allocates nothing. A container field does not copy, it deep-copies - `blob::copy` frees
	the destination's old block and allocates a new one - and an allocator is the one part of a
	store that two threads may never be in at once. `interp.NodeState` is five Ints by
	construction (`resolveNodeStateFields`), so it never decides this; what does is the
	operations' own records and the block's write set. Conservative by design: anything unknown
	is not flat, and the block takes the single-threaded road in silence. */
	bool flatFrames = false;
};

// What the machine offers the executor when a block opens, and what the executor answers. `gpu` is
// set only when the graph carries a lowered body with the host's SPIR-V attached for this block;
// the machine asks once per launch.
struct BlockLaunchInfo {
	uint32_t branchCount = 0;
	uint32_t allowed = 0; // BlockExecutorMask, the fan-out's `executors`
	const GpuBlockShaders *gpu = nullptr;
	const GpuShaderTable *gpuTable = nullptr;
	BlockShape shape; // what a cost is a function of
};

enum class BlockLaunchKind : uint8_t {
	Machine, // the branches stay on this machine's front
	Threads, // batches of branch runs
	Gpu, // one dispatch for the whole block
};

struct BlockLaunch {
	BlockLaunchKind kind = BlockLaunchKind::Machine;
	uint32_t batch = 0; // Threads: branches per batch
};

// Who decides when a block is delivered. The `serial` executor is the machine itself and needs no
// object: a block is deliverable the moment its last branch closes. Another executor is told about
// every block that closes and is asked, before every unit, whether one is ready.
class SP_PUBLIC BlockExecutor {
public:
	virtual ~BlockExecutor() = default;

	virtual void submit(const BlockTicket &) = 0;
	virtual bool poll(BlockTicket &) = 0;

	// Called after every unit of work the machine performs.
	virtual void tick() { }

	// What a run records it ran under; a run with no executor ran under `serial`.
	virtual StringView getName() const { return StringView("custom"); }

	// How many branches of the block go into one batch when this executor runs them itself; 0
	// leaves them on the machine's front. `allowed` is the fan-out's `executors`
	// (BlockExecutorMask).
	virtual uint32_t takesBranches(const BlockTicket &, uint32_t branchCount, uint32_t allowed) {
		return 0;
	}

	// How this block runs, asked once when it opens and once more after a refused GPU launch (with
	// `gpu` cleared), so the fall to threads or to the front happens in the same frame.
	virtual BlockLaunch chooseLaunch(const BlockTicket &ticket, const BlockLaunchInfo &info) {
		auto batch = takesBranches(ticket, info.branchCount, info.allowed);
		return batch > 0 ? BlockLaunch{BlockLaunchKind::Threads, batch} : BlockLaunch();
	}

	// The job to run, every batch of it; poll() hands its ticket back once it is done.
	virtual void launch(BranchJob &) { }

	/* The job's current phase, run over every batch, with no ticket and no flight: the machine is
	standing still waiting for it, and takes it back itself with waitDone(). Used for the two phases
	that are not the run - building the batches and copying their frames back. The default runs them
	here, which is what an executor with no pool effectively does, and what keeps every number
	identical for one. */
	virtual void perform(BranchJob &job) {
		for (uint32_t i = 0; i < job.getBatchCount(); ++i) { job.runBatch(i); }
	}

	// Whether `perform` would actually spread this job's phase over threads. Asked before the
	// machine builds what a spread phase needs - a write view per batch - so a narrow block pays
	// for none of it. An executor that runs phases where they are asked says no, which is the
	// default.
	virtual bool takesPhase(const BranchJob &) const { return false; }

	// The dispatch to run; false refuses it and the machine chooses again. poll() hands the ticket
	// back once the job is done, as for a branch job.
	virtual bool launchGpu(GpuBlockJob &) { return false; }

	// The deadline a block whose own `timeout` is 0 gets, in milliseconds of the run's clock; 0 is
	// "no deadline", which is what an executor with no cost model answers. Asked once, when the
	// flight starts; the diagnostic prints what came back, and says whether it was the author's
	// number or this one.
	virtual uint64_t chooseTimeoutMs(const BlockTicket &, const BlockLaunchInfo &, const BlockLaunch &) {
		return 0;
	}

	// The machine gave up the job (a rollback, a reset, a timeout): whatever the executor holds of
	// it goes. Called after the job was cancelled and waited for.
	virtual void forget(BranchJob &) { }
	virtual void forgetGpu(GpuBlockJob &) { }
};

// The host's clock, in milliseconds, for a block's `timeout`. A host stops it while a debugger
// holds the run. Only a run with an executor reads it: under `serial` a block cannot be overtaken
// by time.
class SP_PUBLIC BlockClock {
public:
	virtual ~BlockClock() = default;
	virtual uint64_t nowMs() const = 0;
};

// How often a run takes a version boundary. A boundary is not free: it turns every page written
// since the last one into an undo record. That cost belongs to the boundary and not to the write -
// the store marks a page in a bitmap and nothing more until somebody asks - so "how far back must
// an undo be able to reach" is a policy question, and it belongs to whoever is running the graph
// rather than to the store underneath it.
enum class RollbackQuantum : uint8_t {
	// A boundary per unit of work. What a debugger needs - stepBack() can only reach a boundary -
	// and the default.
	Step,

	// A boundary only in front of a unit that could ask to be undone: one whose operation does not
	// declare OpFlags::Infallible. A failure inside a stretch of units that could not fail undoes
	// the whole stretch, and going back further than the failing unit is sound - the store lands on
	// a state it really had, and the execution log is truncated to match. Which is why Infallible
	// can be a hint: being wrong about it costs granularity, not correctness.
	Failable,

	// One boundary for the whole run. For a host whose undo is "the last frame" and who therefore
	// has no use for anything finer.
	Run,
};

// The three words, and their inverse. Beside the enum, by the rule a name table keeps here: a
// second table is what drifts, and these three strings are an observable interface the moment one
// file carries them - a debugging session records which mode a run was taken in.
SP_PUBLIC StringView getRollbackQuantumName(RollbackQuantum);
SP_PUBLIC bool readRollbackQuantum(StringView, RollbackQuantum &);

// `A` is the arena kind the run lives in. The scene is a store of that kind; the journal is not
// templated at all, because a journal watches an erased tracked store and can therefore hold this
// run's local arena beside a scene of a different kind.
template <typename A, typename Env = NoEnv>
struct RunConfigT {
	// The scene, a store of the environment's kind. Carried so that the operations that reach it
	// have somewhere to reach; it may be null.
	typename Env::template Scene<A> *scene = nullptr;

	// The scene's extensions. Optional, and null is what every graph that names none passes. It is
	// what an operation reaches an extension through, and it is also who gets told that a version
	// closed or that the run went backwards. Without one the run behaves exactly as it does with no
	// extensions, hooks included: nothing to notify is not a missed notification.
	value::ExtensionHost *extensions = nullptr;

	// The ceiling on units of work one run may perform. Zero means "derive from the graph".
	uint32_t maxSteps = 0;

	// How many loop iterations a run may open, all loops together. Zero derives one. Activations
	// are kept for the whole run - a debugger stopped in iteration seventeen has to be able to read
	// iteration three - so they are spent, not recycled, and the ceiling is what turns "this loop
	// runs forever" into a diagnostic rather than into the entity budget running out.
	uint32_t maxActivations = 0;

	// Optional. With one, every unit of work is a closed version over every store attached to it,
	// which is how the local store and the scene roll back together under a single version counter.
	// Attaching the arenas is the caller's job and cannot be undone (Journal::attach has no
	// counterpart), so the machine will not do it behind their back. Null is an ordinary
	// configuration and not a degraded one: a journal can only watch a store that announces its
	// writes, so a run over a PlainArena passes null here and hasJournal() answers false.
	value::Journal *journal = nullptr;

	// Ignored without a journal. Step by default.
	RollbackQuantum quantum = RollbackQuantum::Step;

	// Who delivers parallel blocks. Null is the `serial` executor: delivery is the unit right after
	// the last branch closes, and the run never suspends.
	BlockExecutor *executor = nullptr;

	// Whose time a block's `timeout` is measured in; null, no block times out.
	BlockClock *clock = nullptr;

	// Whether the front has levels. Off, every node goes on level 0 - the order a graph without
	// weights has anyway, and what a comparison of the two orders runs against.
	bool frontWeights = true;
};

struct RunStep {
	uint32_t step = 0;
	RunStepKind kind = RunStepKind::Node;
	uint32_t node = InvalidIndex;
	uint32_t activation = RootActivation;
	NodeId id = NullNodeId;
	uint32_t fired = 0; // bit per exec output the operation fired

	// Zero without a journal. `base` is the version this unit started from - the point an undo of
	// it returns to - and `version` the one it closed. Under a quantum coarser than Step several
	// consecutive units share a `base`, and only the last of them closes a version, so `version`
	// stays 0 on the others: those units have no point of their own on the timeline, which is what
	// the coarser quantum bought, and stepBack() reads it as "pop everything sharing this base".
	value::Version baseVersion = 0;
	value::Version version = 0;
};

// The run's diagnostics, not its state: no operation can read any of this and a rollback has
// nothing to undo here, which is why it may live on the host while the front may not.
struct RunReport {
	RunOutcome outcome = RunOutcome::Invalid;
	uint32_t stepCount = 0;
	uint32_t sweepCount = 0; // sweeps over blocked nodes after the front drained
	mem_std::Vector<RunStep> log;
	mem_std::Value diagnostics;

	// The execution order as node ids: "1 2 3". The golden form the tests compare.
	mem_std::String getTrace() const;
};

// One entry of a run, into its report's diagnostics, in the words of the run's environment.
template <typename Env>
inline void reportRun(RunReport &report, DiagSeverity severity, DiagCode code, const DiagText &text,
		DiagLocus locus = DiagLocus::None, SpanView<int64_t> values = SpanView<int64_t>(),
		SpanView<StringView> names = SpanView<StringView>()) {
	struct Sink final : DiagSink {
		mem_std::Value *out;
		void add(const Diag &d) override { Env::writeRunDiag(*out, d); }
	} sink;
	sink.out = &report.diagnostics;
	DiagReport(&sink).reportAt(severity, code, text, locus, values, names);
}

// The trace policy: whether the machine writes the execution log. The log is host diagnostics
// - no operation reads it and no rollback restores it - so a run that nobody will ask about
// the order of can skip writing it, which is what a shipped game does with a generated unit and
// what every test and every debugger must not. `stepBack` needs the log to find the boundary it
// returns to, so a machine without one refuses to step back rather than guessing.

struct TraceLog {
	static constexpr bool Log = true;
};

struct TraceNone {
	static constexpr bool Log = false;
};

// A breakpoint is a position, a thing to look at, and a comparison. That one shape covers both of
// the families a debugger needs: leave the watch empty and it is "stop before node N in iteration
// A"; fill it in and it is a watchpoint, "stop when this field becomes that". They live on the
// host, and that is not a hole in the rule that a run is its store's bytes but a consequence of it: no operation can read them, they
// change no result, and above all a rollback must not take them away - a breakpoint that disappears
// when you step backwards is useless as a debugging tool. Same category as the execution log, state
// that describes a run rather than taking part in it.

// Where to look. Empty means the breakpoint is positional and looks at nothing. What is watched and
// where the run stops are separate on purpose: "watch node 4's token and stop wherever it changes"
// is the useful question, and it is not the same as "stop at node 4" - the step that delivers a
// token to a node is somebody else's step.
struct Watch {
	enum class Source : uint8_t {
		None,
		NodeRecord, // a field of the node's own record: an output, or a declared local
		NodeState, // interp.NodeState: inputs / produced / flags / stallPin
		Scene, // a component field on a scene entity
	};

	Source source = Source::None;
	StringView field; // interned by the caller; must outlive the breakpoint

	// NodeRecord and NodeState: whose record. Not a position - see Breakpoint::node for that.
	NodeId node = NullNodeId;

	// Scene only.
	value::EntityId entity;
	TypeId type = value::NullTypeId;
};

// The three words, and their inverse, beside the enum by the rule a name table keeps here: a second
// table drifts, and these strings became an observable interface the moment a file carried one -
// the debugging session writes an edit's source under this vocabulary. `None` is spelled too: a
// vocabulary with a hole in it is one whose reader has to guess what the hole means.
SP_PUBLIC StringView getWatchSourceName(Watch::Source);
SP_PUBLIC bool readWatchSource(StringView, Watch::Source &);

/* Where a watch points, and the one place that works it out. A Watch names three sources and this
is the only code that turns one into an address. It exists as a resolver rather than as a reader
because the debugger's edit is addressed exactly as its watch is: one dictionary on reading and on
writing, and - since a second implementation of one dictionary is how "you may watch here but must
edit over there" begins - one resolver as well. A free function and not a method of the machine,
because of when it is called: editing a scene field at a frame boundary happens while no run exists
at all, so the machine holds nothing then and whoever asks holds both the graph and the scene. The
stores arrive as arguments and any of them may be null - what a null one means is "that source
cannot be resolved here", which is a refusal with a reason rather than a crash. `store` says which
arena the record lives in, because the caller has both and only it knows which one it may write
through. */
struct WatchTarget {
	enum class Store : uint8_t {
		Local, // the run's own store: NodeRecord and NodeState
		Scene,
	};

	Store store = Store::Local;
	Addr record = NullAddr;
	const value::ComponentType *type = nullptr;
	const value::FieldDesc *field = nullptr;
};

/* What went wrong, in the resolver's own words. Not a diagnostic vocabulary - this layer relays and
does not judge - but enough for the caller to write one: a name that addresses nothing and a record
that is not there right now are different failures with different fixes. `Ok` is the only value for
which `out` is filled. */
enum class WatchResolution : uint8_t {
	Ok,
	// The watch says None, or names no field.
	NotAddressed,
	// The graph has no such node, or the registry no such component.
	UnknownName,
	// The store this source lives in is not there: no graph, or no run in progress.
	NoStore,
	// The name is right and the record is not here: the node has no record in this activation, its
	// operation declares none, or the entity does not carry the component.
	NoRecord,
	// The record is there and has no such field.
	UnknownField,
};

SP_PUBLIC StringView getWatchResolutionName(WatchResolution);

// Over the two policies, deduced from the graph and the local store handed in: a caller that holds
// a built graph and an arena store spells it exactly as before.
template <typename Graph, typename Local>
SP_PUBLIC WatchResolution resolveWatch(const Watch &, uint32_t activation, const Graph *,
		const Local *, const typename Local::SceneType *, WatchTarget &out);

enum class BreakWhen : uint8_t {
	// BeforeStep for a positional breakpoint, AfterStep for a watch. Resolved when the breakpoint
	// is added, because getting it wrong is silent: a positional breakpoint tested afterwards stops
	// one unit past where the author is looking, and a watch tested before it can never see the
	// write.
	Default,
	BeforeStep,
	AfterStep,
};

// The three words and their inverse, beside the enum by the rule a name table keeps here. A file
// carries them the moment a debugging session writes its breakpoints down, and a second table is
// what drifts.
SP_PUBLIC StringView getBreakWhenName(BreakWhen);
SP_PUBLIC bool readBreakWhen(StringView, BreakWhen &);

// A comparison fires on the transition into it, never on it merely holding: a watch for "== 5" that
// stopped on every unit while the field stayed at five would be unusable, and worse, it would hide
// the second time something set it. So a breakpoint fires when the condition becomes true, and goes
// quiet until it has been false again.
enum class BreakCompare : uint8_t {
	Any, // the position alone
	Equal,
	NotEqual,
	Less,
	Greater,
	Changed, // different from what it was at the previous evaluation of this breakpoint
};

SP_PUBLIC StringView getBreakCompareName(BreakCompare);
SP_PUBLIC bool readBreakCompare(StringView, BreakCompare &);

struct Breakpoint {
	BreakWhen when = BreakWhen::Default;

	// Where to stop. NullNodeId is "at any unit", InvalidIndex is "in any activation". Independent
	// of Watch::node, which says what to read.
	NodeId node = NullNodeId;
	uint32_t activation = InvalidIndex;

	Watch watch;
	BreakCompare compare = BreakCompare::Any;
	Var value;

	bool enabled = true;

	// Filled in by the machine; ignored on input.
	uint32_t id = 0;
	uint32_t hits = 0;
	Var last; // bookkeeping for Changed
	bool hasLast = false;
};

// Runs one system once. Holds the local store so that a finished run can still be inspected - by a
// test, by the editor's inspector, and by whatever takes the snapshots. `Graph` is how the graph is
// represented, `Local` where the run's records live, `Trace` whether the execution log is written;
// the arena kind is the local store's. The three arena kinds over the built graph are instantiated
// in XSGraph.scu.cpp under the name InterpreterT; a generated unit instantiates its own pair from
// the bodies in SPFlowMachine.hpp.
template <typename Graph, typename Local, typename Trace>
class BranchJobT;

template <typename Graph, typename Local, typename Trace = TraceLog>
class SP_PUBLIC MachineT final {
public:
	using GraphType = Graph;
	using LocalType = Local;
	using TraceType = Trace;
	using ArenaType = typename Local::ArenaType;
	using EnvType = typename Local::EnvType;
	using SceneStore = typename Local::SceneType;
	using Config = RunConfigT<ArenaType, EnvType>;
	using Context = ContextT<Graph, Local>;

	static_assert(IsGraphPolicy<Graph>, "MachineT: the graph does not answer what the machine asks");
	static_assert(IsLocalPolicy<Local>, "MachineT: the local store does not answer what the machine asks");

	// Registers interp.Run and interp.NodeState. The same call as the local store's, named here too
	// so a caller does not have to know which of the two owns them.
	static Status registerCoreTypes(value::TypeRegistry &reg) {
		return Local::registerCoreTypes(reg);
	}

	~MachineT();

	MachineT() = default;
	MachineT(const MachineT &) = delete;
	MachineT &operator=(const MachineT &) = delete;

	// The graph, the registry its operations came from, and an arena for the run's own store. The
	// graph and the registry must outlive the run; the arena is the caller's. Exactly begin()
	// followed by finish(), and deliberately nothing more: a batch run and a stepped run have to
	// execute the same thing, and the way to be sure of that is for there to be only one loop. Two
	// would diverge quietly, in the execution order, which is the golden in every test.
	Status run(const Graph &, const OpRegistry &, ArenaType &, const Config &, RunReport &);

	// One unit at a time. Pausing between units costs nothing, because after any unit the complete
	// state of the run is the bytes of the local store and the host holds only pointers.
	// Those pointers - the graph, the registry, the arena and the scene - must stay alive between
	// calls; they are the same ones run() holds for its own duration. A paused run is still one run
	//: nothing here serialises a cursor or lets a run survive a frame.

	// Prepares the store and puts the entry points on the front. Executes nothing.
	Status begin(const Graph &, const OpRegistry &, ArenaType &, const Config &, RunReport &);

	// Picks a run back up out of an arena that already holds one, instead of starting a new one.
	// This is what "the state of a run is the bytes of its store" means when taken literally: an
	// image saved at a pause, adopted into another arena, continues into the same result. The
	// report is the caller's to carry - the execution log is host diagnostics and the arena
	// holds no copy of it, so a report that was not carried across resumes with a short log and a
	// correct run.
	Status attach(const Graph &, const OpRegistry &, ArenaType &, const Config &, RunReport &);

	// One unit of work. Returns false when the run is over - the outcome is then in the report.
	// Breakpoints are not consulted: this is "step over", and stepping over a breakpoint is what a
	// debugger does.
	bool stepOnce(RunReport &);

	// Undoes the last unit of work: the stores go back to what they were before the run entered
	// that node, the front included, and the run is paused there again. Needs a journal - without
	// one there is nothing to undo with, and saying so is better than doing nothing quietly. Two
	// host-side mirrors have to be repaired by hand, because a rollback moves arena bytes and
	// nothing else: the execution log loses its last entry, and every watch re-reads the value it
	// is watching.
	Status stepBack(RunReport &);

	// Runs to the end, or to the first breakpoint that fires. `while (stepOnce(report))` with the
	// breakpoints tested around it, and no second loop. Continuing from a BeforeStep breakpoint
	// always takes at least one unit: otherwise "continue" would stop where it already is, forever.
	Status finish(RunReport &);

	RunState getState() const { return _state; }

	// Breakpoints.

	// The id, or 0 when the description is refused. Refused at once rather than accepted and never
	// fired: a breakpoint that silently does not work sends its owner looking for the bug in the
	// graph. A breakpoint naming a node needs a run to name it in - begin() first.
	uint32_t addBreakpoint(const Breakpoint &);

	bool removeBreakpoint(uint32_t id);
	Status setBreakpointEnabled(uint32_t id, bool);
	void clearBreakpoints();

	SpanView<Breakpoint> getBreakpoints() const { return _breakpoints; }
	const Breakpoint *getBreakpoint(uint32_t id) const;

	// What stopped the last finish(); 0 when it stopped of its own accord.
	uint32_t getLastBreakpoint() const { return _lastBreakpoint; }

	// The packed (node, activation) of the unit stepOnce() would perform next, or NullRecordKey
	// when there is none. Reads nothing and changes nothing - a debugger asks this before it
	// decides.
	uint64_t peekNext() const;

	// Destroys the local store. Called by begin() before it starts and by the destructor: a run
	// lives exactly once, and what survives it is the store's bytes, until someone asks for
	// another.
	void reset();

	Local &getLocal() { return _local; }
	const Local &getLocal() const { return _local; }

	// What a GPU job's loader reads: the graph, the scene it runs against, and where a value's
	// producer lives.
	const Graph *getGraph() const { return _graph; }
	SceneStore *getScene() const { return _scene; }

	// Whether an executor is running branches of this run right now: the scene is being read on
	// other threads, and nothing may change it but the machine's own deliveries.
	bool hasBlocksInFlight() const { return _threaded > 0; }

	// How many branch jobs this run has built, and how many it took back out of its own pool.
	// Counts, not times: a section proves the reuse by reading them.
	uint32_t getJobsCreated() const { return _jobsCreated; }
	uint32_t getJobsReused() const { return _jobsReused; }

	// How many jobs the pool keeps. Blocks in flight at once is what it has to cover - two is what
	// the suite exercises - and a job past it is released rather than kept.
	static constexpr uint32_t JobPoolLimit = 4;

	// A batch of branches, run by an executor: the same machine over a store of its own.
	// `beginBranches` prepares it with no entry points, `seedBranches` copies in, from the machine
	// that fired the block, the activations above the fan-out and every record the body reads from
	// outside itself, and `openBranches` opens the batch's branches exactly as the fan-out would.
	// Then stepOnce until it answers false.
	Status beginBranches(const Graph &, const OpRegistry &, ArenaType &, const Config &, RunReport &);
	template <typename Source>
	Status seedBranches(const Source &, uint32_t fanOut, uint32_t activation);
	Status openBranches(uint32_t fanOut, SpanView<value::EntityId>, uint32_t firstIndex);

private:
	Status step(uint32_t node, uint32_t activation, RunReport &);

	// One unit of work performed: through the unit's own compiled step when the graph carries one,
	// and through the registry's function pointer against this machine's door when it does not.
	Status invokeAt(StepSite<Graph, Local> &);

	// `frame` is the one the unit already resolved. A node, its state, its record and every
	// same-scope consumer of it live in one frame, and step() has it before propagate is called;
	// resolving it again is a walk of the activation row for an answer that cannot differ.
	void propagate(uint32_t node, uint32_t activation, uint32_t fired, Addr frame);
	bool isReady(uint32_t node, uint32_t activation) const;
	uint32_t requiredInputs(uint32_t node) const;
	Status enqueue(uint32_t node, uint32_t activation);

	// The same two with the activation's frame already resolved. Finding a frame is two array
	// reads; reading a record out of one is an addition. So every place that asks about several
	// nodes of one activation - and there are four - resolves it once and uses these.
	bool isReadyIn(Addr frame, uint32_t node, uint32_t activation) const;
	Status enqueueIn(Addr frame, uint32_t node, uint32_t activation);
	void collectBlocked(RunReport &);

	// The ready front drained but a loop iteration is still open: close it, and hand control back
	// to the node that opened it so that it can decide on another turn or on `completed`. This is
	// what spares an author from wiring the body's tail back by hand - and a body whose branch did
	// not fire has no tail to wire.
	bool closeActivation(RunReport &);

	// Parallel blocks.

	// The branch activation and block a step in `activation` of `node` belongs to; false outside.
	bool branchOf(uint32_t node, uint32_t activation, uint32_t &block, uint32_t &branch) const;

	// Opens every branch of the block `fanOut` fired in `activation`, or none when the run would go
	// past its activation limit.
	void openBlock(uint32_t fanOut, uint32_t activation, uint32_t scope, uint32_t firstNode,
			mem_std::Vector<uint64_t> &ready);

	// A branch failed: marked, logged, reported; under `cancelFrame` the run is concluded, and the
	// failing unit is still the caller's to settle.
	void failBranch(uint32_t node, uint32_t activation, uint32_t block, uint32_t branch, Status,
			bool budget, bool log, RunReport &);

	// Takes every key of a failed branch off the top of the front.
	void drainFailed();

	// Whether the open activation `activation` holds, at or under it, a block not yet delivered.
	bool closeBlocked(uint32_t activation) const;

	int64_t readFanOut(uint32_t fanOut, uint32_t activation, uint32_t local) const;
	void writeFanOut(uint32_t fanOut, uint32_t activation, uint32_t local, int64_t);

	void submitBlock(uint32_t fanOut, uint32_t activation);

	// A block past its `timeout` is broken off: its unclosed branches fail and it is delivered
	// without asking the executor. False when the block's policy ends the run.
	bool checkTimeouts(RunReport &);
	bool nextDelivery(BlockTicket &);
	void refreshPending();
	Status deliver(const BlockTicket &, RunReport &);

	// The branches of a block with an executor that runs them, and their way back.
	bool launchBlock(uint32_t fanOut, uint32_t activation, SpanView<value::EntityId>,
			BlockLaunch choice = BlockLaunch());
	BlockLaunchInfo makeLaunchInfo(uint32_t block, uint32_t branchCount) const;

	// The deadline of a flight about to start: the author's `timeout`, or the executor's own
	// default for the kind of launch it chose.
	uint64_t flightTimeout(uint32_t block, const BlockTicket &, const BlockLaunch &,
			uint32_t branchCount) const;
	Status importBranches(BranchJobT<Graph, Local, Trace> &, uint32_t first, RunReport &, bool &cancelled);

	// The copying half of an import. `importBatchRows` is one batch, at the addresses the reserving
	// pass settled, into whatever destination it is given - this store, or a write view of it held
	// by a worker. `importRows` is however many of those the executor can run at once.
	template <typename Dst>
	Status importBatchRows(BranchJobT<Graph, Local, Trace> &, Dst &, uint32_t batchIndex, RunReport &,
			uint32_t logBase);
	Status importRows(BranchJobT<Graph, Local, Trace> &, RunReport &, uint32_t logBase);

	// The job pool. A branch job holds a machine, an arena and a memory pool per batch, so a graph
	// that runs the same block every frame reuses them rather than building and destroying one
	// every time. Taken here, given back at the two places a job dies - the delivery and the
	// abandonment - and released outright only when the run is.
	BranchJobT<Graph, Local, Trace> *takeBranchJob();
	void giveBranchJob(BranchJobT<Graph, Local, Trace> *);
	void clearJobPool();

	// The same for a dispatch: the frames the device's records rebuild, and everything the rebuild
	// does not carry - the log, the branch failures, the stalls and the counters.
	Status importGpu(GpuBlockJobT<Graph, Local, Trace> &, uint32_t first, uint64_t timeoutMs,
			RunReport &, bool &cancelled);

	// The folds the device did, written into the collectors' records before they run.
	void presetCollectors(GpuBlockJobT<Graph, Local, Trace> &, uint32_t activation);
	void abandonFlights();
	void noteThreaded(int32_t);
	void prepareParallel();
	uint32_t bodyEntry(uint32_t fanOut) const;

	// One turn of a loop: a fresh activation, a fresh record per body node, the token on the node
	// the body starts at. Done by the machine and not by the operation, which fires an exec output
	// and knows nothing about loops - exactly as `branch` does.
	void openIteration(uint32_t opener, uint32_t activation, uint32_t scope, uint32_t firstNode,
			mem_std::Vector<uint64_t> &ready);

	// Where the value on this edge lives, and where the consumer of it lives. Null activations mean
	// "not yet": a body node's producer outside the loop is found by walking up, and a body node's
	// activation does not exist until the iteration opens.
	uint32_t producerActivation(uint32_t srcNode, uint32_t consumerActivation) const;

	// The last unit's verdict, so that finish() can hand back the operation's own status rather
	// than a generic failure.
	void conclude(RunReport &, RunOutcome, Status);

	// The watched value, or false when there is nothing there to read. Independent of where the run
	// currently is: the watch names its own node.
	bool readWatch(const Breakpoint &, uint32_t activation, Var &out) const;

	// The id of the first breakpoint that fires, or 0. Samples every watch of this phase before
	// testing positions, so that Changed means "since the previous unit" rather than "since the
	// previous unit that happened to be at a matching position".
	uint32_t evaluateBreakpoints(BreakWhen, uint32_t node, uint32_t activation);

	// Re-reads every watch out of the arena. Called after a rollback, where the values a breakpoint
	// remembers are the only thing that did not move back with the bytes.
	void resampleWatches();

	const Graph *_graph = nullptr;
	const OpRegistry *_ops = nullptr;
	SceneStore *_scene = nullptr;
	value::ExtensionHost *_extensions = nullptr;
	Local _local;

	// The four things the step path asks of a version. Behind functions rather than behind an `#if`
	// at each of the ten call sites - the run reads the same in both builds, and the unversioned
	// answers are the honest ones: a unit of work is not a version, and there is nothing to undo.
	value::Journal *_journal = nullptr;

	// Const because they change the journal and not the machine - which is what lets peekNext(),
	// which promises to look without running anything, still open the version boundary it looks in.
	bool hasJournal() const { return _journal != nullptr; }
	value::Version openVersion() const { return _journal ? _journal->commit() : 0; }
	value::Version closeVersion() const { return _journal->commit(); }
	Status undoTo(value::Version base) const { return _journal->rollback(base); }

	// The version boundary, and the one place an extension is told about it. Wrapped rather than
	// called beside each of the three close sites, because "the hook fires exactly when a version
	// closes" is a rule that a fourth site would break silently. What an extension does with it is
	// its business; what it may not do is write the store, and that is enforced by SceneExtension
	// holding a const store rather than by an assertion here.
	value::Version closeVersionAndNotify() const {
		auto v = closeVersion();
		if (_extensions) {
			_extensions->handleVersionClosed(v);
		}
		return v;
	}

	Status undoToAndNotify(value::Version base) const {
		auto st = undoTo(base);
		if (st == Status::Ok && _extensions) {
			// After the rollback, never before: an extension that rebuilds here has to see the
			// bytes it is rebuilding from. An anchor the rollback took away simply reads as absent.
			_extensions->handleRollback(base);
		}
		return st;
	}

	// Whether the unit about to run needs a boundary in front of it. `node` is InvalidIndex for the
	// unit that closes a loop iteration, which runs no operation and cannot fail.
	bool needsBoundary(uint32_t node) const;

	RunState _state = RunState::Idle;
	Status _status = Status::Ok;
	uint32_t _maxSteps = 0;
	uint32_t _maxActivations = 0;
	RollbackQuantum _quantum = RollbackQuantum::Step;

	// The version the current quantum started from - what an undo of anything inside it returns to.
	// Equal to the previous unit's base whenever no boundary was taken.
	value::Version _quantumBase = 0;

	// Three fields a machine without a log keeps in its place, and a machine with one never
	// touches: how many units the report counted when the current quantum opened (what an undo
	// inside it sets the count back to, which the log otherwise says), and the unit the last
	// stepOnce() performed (what finish() tests the AfterStep breakpoints against, which the log's
	// last entry otherwise says). Written under `if constexpr (!Trace::Log)` and nowhere else, so
	// that the TraceLog instantiation's instruction stream is unchanged by the policy existing.
	uint32_t _quantumStepCount = 0;
	uint32_t _lastNode = InvalidIndex;
	uint32_t _lastActivation = RootActivation;

	// A loop asked for a turn the run could not afford. Reported when the run ends rather than at
	// once, so that the store is left in a state somebody can look at.
	bool _activationsExhausted = false;

	// Parallel blocks. The pending list is derived from the arena (fan-outs in phase Ready) and
	// renewed with the generation after anything that moves the arena back.
	BlockExecutor *_executor = nullptr;
	uint64_t _generation = 0;
	mem_std::Vector<BlockTicket> _pending;
	BlockTicket _polled;
	bool _hasPolled = false;
	BlockClock *_clock = nullptr;

	// Blocks in flight, with the time they started; host-side, restamped after the arena moves
	// back. `job` is set when an executor runs the branches, and owned by the flight.
	struct Flight {
		uint32_t fanOut = InvalidIndex;
		uint32_t activation = InvalidIndex;
		uint64_t start = 0;
		uint64_t timeoutMs = 0; // the author's number, or the executor's default
		bool aborted = false;
		BranchJob *job = nullptr; // a batch of branch runs, or a dispatch (getKind)
	};
	mem_std::Vector<Flight> _flights;

	// Flights with a job; the fan-out's `executors` per block; per node, whether its unit waits
	// while a job is out (it changes the scene's structure or allocates in its arena); a batch
	// run's own flag and the fan-out activation its seed mapped to.
	uint32_t _threaded = 0;
	mem_std::Vector<uint32_t> _blockExecutors;
	mem_std::Vector<BlockShape> _blockShapes;
	mem_std::Vector<uint8_t> _freezes;
	bool _frontWeights = true;
	bool _branchMode = false;
	uint32_t _seedActivation = RootActivation;

	template <typename G, typename L, typename T>
	friend class BranchJobT;

	template <typename G, typename L, typename T>
	friend class GpuBlockJobT;

	// A machine takes back what a batch run over another store left.
	template <typename G, typename L, typename T>
	friend class MachineT;

	// Branch jobs not currently in flight, with their batch stores still warm, and what a check
	// reads instead of timing anything.
	mem_std::Vector<BranchJobT<Graph, Local, Trace> *> _jobPool;
	uint32_t _jobsCreated = 0;
	uint32_t _jobsReused = 0;

	// Units of work spent inside branches: they count in the report and against the branch's
	// budget, not against the run's ceiling. Host-side like the report, with the value at the
	// current quantum's boundary so an undo can put it back; stepBack recounts it from the log.
	uint32_t _branchUnits = 0;
	uint32_t _quantumBranchUnits = 0;
	bool isBranchUnit(const RunStep &) const;

	// A loop in a branch that went past the branch's activation budget while the unit ran; the unit
	// reports it when it settles.
	struct BudgetFailure {
		uint32_t node = InvalidIndex;
		uint32_t activation = InvalidIndex;
		uint32_t block = InvalidIndex;
		uint32_t branch = InvalidIndex;
	} _budgetFailure;

	// Host-side, and outliving both reset() and any rollback - see the note above the Breakpoint
	// declaration. Ids are never reused, so a stale id is rejected rather than silently retargeted.
	mem_std::Vector<Breakpoint> _breakpoints;
	uint32_t _nextBreakpointId = 1;
	uint32_t _lastBreakpoint = 0;
	bool _stoppedBefore = false;
};

} // namespace stappler::flow

#endif /* STAPPLER_FLOW_SPFLOWMACHINE_H_ */
