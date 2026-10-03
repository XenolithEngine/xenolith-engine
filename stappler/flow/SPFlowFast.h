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

#ifndef STAPPLER_FLOW_SPFLOWFAST_H_
#define STAPPLER_FLOW_SPFLOWFAST_H_

#include "SPFlowLocal.h"

/* The fast local store: the second place a run's records can live.

The arena store (SPFlowLocal.h) keeps everything in an arena: after any unit of work the
complete state of the run is that arena's bytes, so a run can be paused, saved, rolled back and
picked up somewhere else. A shipped game does none of those and pays for them every frame: an
`interp.Run` record whose front and activation tree are arena blobs, found through the arena's
root, and a blob operation for every push and pop of the front. This store is the same run with that price removed. The frames - a
node's `interp.NodeState` and its record, at the offsets computeFrameLayout gives, initialised by
initFrame and destroyed by releaseFrameRecords - live in a scratch `PlainArena` this object owns,
because a container field is a 32-bit address inside an arena and `blob::` is written against
exactly that; all of them, not only those holding a container, since a field is read through
`ComponentType::getField(A &, Addr, ...)` and anything but an arena would need a second
implementation of the field rules. What moves to the host is the run's own
bookkeeping: the activation tree, the ready front, the open list, the stalled list, the two
counters. The trade is the whole of the mode - `attach` refuses, `stepBack` refuses, there is no
image, and a journal may watch the scene but never this - while everything a run computes is
unchanged, which is why this mode promises the interpreter's results, measured against it case for
case. */
namespace STAPPLER_VERSIONIZED stappler::flow {

// `A` is the kind of arena the scene is in - the machine names its scene and its config through the
// local store's `ArenaType`, and a run reads and writes one scene. It is not the kind of the
// frames: those are always in the PlainArena below, which is the point of the mode. `Graph` is the
// representation the frames are laid out over, exactly as for the arena store; it has no default
// here, where the arena store's is `RuntimeGraph`, because this mode exists for a generated unit
// whose type is declared one header further along. `CompiledFastRunT<A>` (SPFlowCompiled.h) is the
// name that fills it in.
template <typename A, typename Graph, typename Env = NoEnv>
class SP_PUBLIC FastLocalT final {
public:
	using ArenaType = A;
	using GraphType = Graph;

	// What the run executes in (SPFlowEnv.h): the scene its operations reach is a store of the
	// run's arena kind, the frames above are in a scratch arena regardless.
	using EnvType = Env;
	using SceneType = typename Env::template Scene<A>;

	// Where the frames are cut from. Named so that a reader of the door's `getArena()` can see what
	// kind of arena an operation is handed in this mode.
	using ScratchArena = value::PlainArena;

	// The store a frame's bytes live in, which is what a copy into one has to be told. Here it is
	// the scratch; in the arena store it is the run's own arena. The two names are deliberately
	// separate: `ArenaType` is the kind the run was started with, and for this store that is not
	// where the frames are.
	using FrameArena = ScratchArena;

	// Whether a unit of work in this store can be undone. It cannot: half of the run is host memory
	// that no version covers, so rolling the frames back would leave the front and the activation
	// tree describing a run that no longer exists. The machine reads this to refuse `stepBack` and
	// to refuse a journal at any quantum finer than `Run`.
	static constexpr bool Rollbackable = false;

	// The same two types the arena store registers, through the same registrar. This store needs
	// only `interp.NodeState` - there is no `interp.Run` record to hold - but the two engines share
	// one operation registry, the registration is idempotent, and one registrar is better than a
	// second list of the same fields.
	static Status registerCoreTypes(value::TypeRegistry &);

	~FastLocalT();

	FastLocalT() = default;
	FastLocalT(const FastLocalT &) = delete;
	FastLocalT &operator=(const FastLocalT &) = delete;

	/* Builds the store and materializes activation 0. The registry must hold interp.NodeState,
	which in practice is OpRegistry::getLocalTypes(). The arena argument is not used, and the
	signature keeps it because the machine's is one signature for both stores: this store's
	frames come from the scratch arena it makes here. */
	Status init(A &, const Graph &, const value::TypeRegistry &);

	// Refuses, and that is the mode. Picking a run up out of an image is the arena store's whole
	// reason for putting everything in the arena; here half the run is in host vectors that no
	// image contains, so there is nothing to pick up.
	Status open(A &, const Graph &, const value::TypeRegistry &);

	// Gives the run's frames back and forgets the graph. The scratch arena stays: it belongs to
	// this object rather than to the run - unlike the arena store's, which belongs to the caller -
	// and a mode entered once a frame must not re-cut an allocator every time.
	void destroy();

	bool isValid() const { return _graph != nullptr && _scratch.isInitialized(); }

	// The frames' arena. An operation reaching for a container gets this, kind-erased.
	ScratchArena *getArena() const { return isValid() ? &_scratch : nullptr; }
	value::ArenaRef getArenaRef() const {
		return isValid() ? value::ArenaRef::of(_scratch) : value::ArenaRef();
	}

	// Frames.

	Status materializeScope(uint32_t scope, uint32_t activation);

	Addr activationFrame(uint32_t activation) const;
	Addr frameFor(uint32_t node, uint32_t activation) const;
	uint32_t activationScope(uint32_t activation) const;
	bool hasRecord(uint32_t node, uint32_t activation) const;
	uint32_t getFrameBytes(uint32_t scope) const;

	// Activations.

	Status openActivation(uint32_t scope, uint32_t parent, uint32_t iteration, uint64_t openerKey,
			uint32_t &out);

	uint32_t getActivationCount() const { return uint32_t(_activations.size()); }
	ActivationData readActivation(uint32_t activation) const;
	uint32_t resolveScope(uint32_t activation, uint32_t scope) const;

	Status pushOpen(uint32_t activation);
	bool popOpen(uint32_t &out);
	uint32_t getOpenCount() const { return uint32_t(_open.size()); }
	uint32_t getOpenAt(uint32_t index) const;

	uint32_t getRecordCount() const;
	void forEachRecord(const Callback<bool(uint32_t node, uint32_t activation, Addr frame)> &) const;

	Addr getRecord(uint32_t node, uint32_t activation) const;
	Addr getState(uint32_t node, uint32_t activation) const;
	Addr getRecordIn(Addr frame, uint32_t node) const;
	Addr getStateIn(Addr frame, uint32_t node) const;

	// Node state: the same five fields at the same offsets inside the same frame, read and written
	// by the same functions the arena store uses. The state is not a host struct - it is in the
	// frame beside the record, because a step that touches one touches the other and the door's
	// static site resolves both by the same constant offsets whichever store it is over.

	NodeStateData readState(Addr stateAddr) const;
	const value::ComponentType *getStateType() const { return _stateType; }

	Status markInput(Addr stateAddr, uint32_t pin);
	Status markProduced(Addr stateAddr, uint32_t pin);
	Status addFlags(Addr stateAddr, int64_t flags);
	Status removeFlags(Addr stateAddr, int64_t flags);
	Status setStall(Addr stateAddr, int64_t reason, uint32_t pin);
	Status clearStall(Addr stateAddr);
	Status setTurns(Addr stateAddr, uint32_t);

	// The run's own state, as host vectors, and this is the whole difference between the two
	// stores. In the arena store every one of these is an array blob inside the `interp.Run`
	// record, because a snapshot without the front cannot be resumed. This mode does not resume,
	// so a push is a push.

	Status pushReady(uint32_t node, uint32_t activation);
	bool popReady(uint32_t &node, uint32_t &activation);
	bool peekReady(uint32_t &node, uint32_t &activation) const;
	uint32_t getReadyCount() const { return uint32_t(_ready.size() + _ready1.size()); }
	void setWeighted(bool value) { _weighted = value; }

	Status pushStalled(uint32_t node, uint32_t activation);
	uint32_t getStalledCount() const { return uint32_t(_stalled.size()); }
	uint64_t getStalledAt(uint32_t index) const;
	Status clearStalled();

	uint32_t getStep() const { return _step; }
	Status setStep(uint32_t v) {
		_step = v;
		return Status::Ok;
	}
	uint32_t getPass() const { return _pass; }
	Status setPass(uint32_t v) {
		_pass = v;
		return Status::Ok;
	}

	void describe(mem_std::Value &) const;

private:
	// One turn, as five host fields rather than five Ints in an array blob. The same five, in the
	// same meaning: ActivationData is what both stores hand out.
	struct Activation {
		uint32_t scope = 0;
		uint32_t parent = NullActivation;
		uint32_t iteration = 0;
		uint64_t openerKey = NullRecordKey;
		Addr frame = NullAddr;
	};

	void releaseFrame(uint32_t activation);

	// `mutable` because the door and every reader take the arena from a const store, exactly as the
	// arena store hands out its caller's arena from a const method.
	mutable ScratchArena _scratch;
	const Graph *_graph = nullptr;

	const value::ComponentType *_stateType = nullptr;
	NodeStateFields _stateFields;

	// The frame layout: derived from the graph by the same free function the arena store, the
	// generator and the unit loader all call. One calculation, four callers.
	mem_std::Vector<FrameSlot> _slots;
	mem_std::Vector<uint32_t> _frameBytes;

	mem_std::Vector<Activation> _activations;
	mem_std::Vector<uint64_t> _ready;
	mem_std::Vector<uint64_t> _ready1; // the upper level of the front
	bool _weighted = true;
	mem_std::Vector<uint64_t> _stalled;
	mem_std::Vector<uint32_t> _open;
	uint32_t _step = 0;
	uint32_t _pass = 0;
};

} // namespace stappler::flow

#endif /* STAPPLER_FLOW_SPFLOWFAST_H_ */
