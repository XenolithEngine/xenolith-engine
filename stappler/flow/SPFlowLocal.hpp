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

#ifndef STAPPLER_FLOW_SPFLOWLOCAL_HPP_
#define STAPPLER_FLOW_SPFLOWLOCAL_HPP_

// The local store of one run: the bodies of LocalStoreT<A, Graph, Env>. An .hpp and not a .cc subunit,
// because they are instantiated in more than one compile unit: the module's own (XSGraph.scu.cpp,
// for the built graph and the three arena kinds) and every generated unit, for the static graph it
// carries. A definition that lived in one SCU would leave the second instantiation with nothing to
// link against.

#include "SPFlowLocal.h"

#include <sprt/c/__sprt_string.h>

namespace STAPPLER_VERSIONIZED stappler::flow {

template <typename A, typename Graph, typename Env>
Status LocalStoreT<A, Graph, Env>::registerCoreTypes(value::TypeRegistry &reg) {
	using value::FieldDef;
	using value::VarType;

	if (!reg.get(RunTypeName)) {
		auto ints = value::makeChain(VarType::Int);
		FieldDef fields[] = {
			// The activation tree: five Ints per turn - scope, parent, iteration, opener, frame -
			// interleaved in one array, so one read reaches all five (ActivationStride in the
			// header). A tree of data rather than a stack of C++ frames, for the same reason the
			// front is: a snapshot that does not contain it cannot be resumed, and a debugger that
			// cannot read it cannot say which iteration it is standing in. `frame` is the arena
			// block holding every record of that turn, and a node's state and record are at offsets
			// inside it that the build computes, so finding a record is one array read and an
			// addition.
			FieldDef{.name = StringView("activations"), .type = VarType::Array, .element = ints},
			// The execution front. In the arena, not on the host: see the note in the header.
			FieldDef{.name = StringView("ready"), .type = VarType::Array, .element = ints},
			FieldDef{.name = StringView("stalled"), .type = VarType::Array, .element = ints},
			FieldDef{.name = StringView("step"), .type = VarType::Int},
			FieldDef{.name = StringView("pass"), .type = VarType::Int},
			// Which activations are still running, innermost last.
			FieldDef{.name = StringView("open"), .type = VarType::Array, .element = ints},
		};
		if (!reg.createNative(RunTypeName, SpanView<FieldDef>(fields, 6))) {
			return Status::ErrorInvalidArguemnt;
		}
	}

	if (!reg.get(RunLevelsTypeName)) {
		FieldDef fields[] = {
			FieldDef{.name = StringView("ready1"),
				.type = VarType::Array,
				.element = value::makeChain(VarType::Int)},
		};
		if (!reg.createNative(RunLevelsTypeName, SpanView<FieldDef>(fields, 1))) {
			return Status::ErrorInvalidArguemnt;
		}
	}

	if (!reg.get(NodeStateTypeName)) {
		// Two 32-bit masks in 64-bit fields, because MaxDataPins is 32: the readiness of a node
		// fits in a machine word, which is exactly why the pin count is capped where it is.
		FieldDef fields[] = {
			FieldDef{.name = StringView("inputs"), .type = VarType::Int},
			FieldDef{.name = StringView("produced"), .type = VarType::Int},
			FieldDef{.name = StringView("flags"), .type = VarType::Int},
			FieldDef{.name = StringView("stallPin"), .type = VarType::Int},
			// How many turns of a loop this record has opened. Only a scope-opening node ever has a
			// non-zero one. Per (node, activation) is exactly the right granularity: an opener
			// entered again from the next turn of an enclosing loop is a different record, and its
			// count starts at zero.
			FieldDef{.name = StringView("turns"), .type = VarType::Int},
		};
		if (!reg.createNative(NodeStateTypeName, SpanView<FieldDef>(fields, 5))) {
			return Status::ErrorInvalidArguemnt;
		}
	}

	return Status::Ok;
}

template <typename A, typename Graph, typename Env>
LocalStoreT<A, Graph, Env>::~LocalStoreT() {
	destroy();
}

// The descriptors, from the registry. Host-side and address-stable, so caching them is caching a
// pointer to something the arena does not own - which is the only kind of cache this class may
// hold, and the reason a rollback under a live store needs no reattachment.
template <typename A, typename Graph, typename Env>
Status LocalStoreT<A, Graph, Env>::resolveTypes(const value::TypeRegistry &reg) {
	_runType = reg.get(RunTypeName);
	_stateType = reg.get(NodeStateTypeName);
	_levelsType = reg.get(RunLevelsTypeName);
	_ready1 = _levelsType ? _levelsType->getField(StringView("ready1")) : nullptr;
	if (!_runType || !_stateType) {
		return Status::ErrorInvalidArguemnt;
	}

	_activations = _runType->getField(StringView("activations"));
	_ready = _runType->getField(StringView("ready"));
	_stalled = _runType->getField(StringView("stalled"));
	_step = _runType->getField(StringView("step"));
	_pass = _runType->getField(StringView("pass"));
	_open = _runType->getField(StringView("open"));

	// The five of interp.NodeState, and the check that readState's one-read decode is sound - both
	// through the one resolver, which the fast store uses too.
	auto st = resolveNodeStateFields(*_stateType, _stateFields);
	if (st != Status::Ok) {
		return st;
	}

	// Every descriptor, or none: a field looked up by a name the schema does not have comes back
	// null and then degrades to NullAddr at the first use, which is a run that quietly does nothing
	// rather than a registry that failed.
	const value::FieldDesc *all[] = {_activations, _ready, _stalled, _step, _pass, _open};
	for (auto it : all) {
		if (!it) {
			return Status::ErrorInvalidArguemnt;
		}
	}

	return Status::Ok;
}

template <typename A, typename Graph, typename Env>
Status LocalStoreT<A, Graph, Env>::computeFrameLayout() {
	_slots.clear();
	_frameBytes.clear();
	if (!_graph || !_stateType) {
		return Status::ErrorInvalidArguemnt;
	}
	return flow::computeFrameLayout(*_graph, *_stateType, _slots, _frameBytes);
}

template <typename A, typename Graph, typename Env>
uint32_t LocalStoreT<A, Graph, Env>::getFrameBytes(uint32_t scope) const {
	return scope < _frameBytes.size() ? _frameBytes[scope] : 0;
}

template <typename A, typename Graph, typename Env>
bool LocalStoreT<A, Graph, Env>::readRoot(RunRoot &out) const {
	if (!_arena) {
		return false;
	}
	auto root = _arena->getUserRoot();
	if (root == NullAddr) {
		return false;
	}
	auto src = _arena->read(root, uint32_t(sizeof(RunRoot)));
	if (!src) {
		return false;
	}
	__sprt_memcpy(&out, src, sizeof(RunRoot));
	return out.magic == RunRoot::Magic && out.run != NullAddr;
}

template <typename A, typename Graph, typename Env>
Status LocalStoreT<A, Graph, Env>::verify() const {
	RunRoot root;
	if (!readRoot(root)) {
		return Status::ErrorInvalidArguemnt;
	}
	if (_hasLevels != (root.levels != NullAddr)) {
		return Status::ErrorInvalidArguemnt;
	}
	return _arena->verify();
}

template <typename A, typename Graph, typename Env>
Status LocalStoreT<A, Graph, Env>::open(A &arena, const Graph &graph,
		const value::TypeRegistry &reg) {
	if (_arena) {
		return Status::ErrorAlreadyPerformed;
	}
	auto st = resolveTypes(reg);
	if (st != Status::Ok) {
		return st;
	}
	forgetRunRecord();

	_arena = &arena;
	RunRoot root;
	if (!readRoot(root)) {
		_arena = nullptr;
		return Status::ErrorNotFound;
	}

	_graph = &graph;
	if (auto layout = computeFrameLayout(); layout != Status::Ok) {
		_graph = nullptr;
		_arena = nullptr;
		return layout;
	}

	// Everything else is read out of the arena on every call, which is precisely what makes an
	// image of the arena a complete run.
	_hasLevels = _levelsType && root.levels != NullAddr;
	return Status::Ok;
}

template <typename A, typename Graph, typename Env>
Status LocalStoreT<A, Graph, Env>::init(A &arena, const Graph &graph,
		const value::TypeRegistry &reg) {
	if (_arena) {
		return Status::ErrorAlreadyPerformed;
	}

	auto st = resolveTypes(reg);
	if (st != Status::Ok) {
		return st;
	}
	forgetRunRecord();

	_arena = &arena;
	_graph = &graph;
	if (auto layout = computeFrameLayout(); layout != Status::Ok) {
		destroy();
		return layout;
	}

	// The head block first, then the records it names, in the same order in every run of every
	// graph - which is one less thing that can differ between two runs that must produce identical
	// images. A run has no Frame and no Random: time belongs to the scene.
	auto rootAddr = arena.alloc(uint32_t(sizeof(RunRoot)));
	if (rootAddr == NullAddr) {
		destroy();
		return Status::ErrorOutOfHostMemory;
	}
	RunRoot head;
	head.run = _runType->createInstance(arena);
	_hasLevels = false;
	if (head.run != NullAddr && graphHasWeights(graph) && _levelsType && _ready1) {
		head.levels = _levelsType->createInstance(arena);
		_hasLevels = head.levels != NullAddr;
	}
	auto dst = arena.write(rootAddr, uint32_t(sizeof(RunRoot)));
	if (!dst || head.run == NullAddr
			|| (graphHasWeights(graph) && _levelsType && _ready1 && !_hasLevels)) {
		if (dst) {
			__sprt_memcpy(dst, &head, sizeof(RunRoot));
		}
		arena.setUserRoot(rootAddr);
		destroy();
		return Status::ErrorOutOfHostMemory;
	}
	__sprt_memcpy(dst, &head, sizeof(RunRoot));
	if (arena.setUserRoot(rootAddr) != Status::Ok) {
		arena.free(rootAddr);
		destroy();
		return Status::ErrorInvalidArguemnt;
	}

	// Activation 0: the graph itself, running once. Everything else is a loop iteration and is
	// opened as the run reaches it.
	uint32_t root = 0;
	st = openActivation(0, NullActivation, 0, NullRecordKey, root);
	if (st != Status::Ok) {
		destroy();
		return st;
	}

	// Only the root scope, not every node: the nodes of a loop body get their records when an
	// iteration opens, and they get a fresh set on every turn.
	st = materializeScope(0, RootActivation);
	if (st != Status::Ok) {
		destroy();
		return st;
	}

	return Status::Ok;
}

// One allocation for a whole turn - an alloc, a memset and one array write. This is where the frame
// layout pays the second time.
template <typename A, typename Graph, typename Env>
Status LocalStoreT<A, Graph, Env>::materializeScope(uint32_t scope, uint32_t activation) {
	if (!_graph || scope >= _graph->getScopeCount() || activation >= getActivationCount()) {
		return Status::ErrorInvalidArguemnt;
	}
	if (activationScope(activation) != scope) {
		return Status::ErrorInvalidArguemnt;
	}
	if (activationFrame(activation) != NullAddr) {
		return Status::ErrorAlreadyPerformed;
	}

	auto arena = _arena;
	auto bytes = _frameBytes[scope];
	auto frame = arena->alloc(bytes, value::MaxAlign);
	if (frame == NullAddr) {
		return Status::ErrorOutOfHostMemory;
	}

	// Zeroed whole and then given its defaults, by the one function both stores use.
	if (initFrame(*arena, *_graph, _slots, *_stateType, scope, frame, bytes) != Status::Ok) {
		arena->free(frame);
		return Status::ErrorOutOfHostMemory;
	}

	auto slot = frameSlotAddr(activation);
	if (slot == NullAddr) {
		arena->free(frame);
		return Status::ErrorInvalidArguemnt;
	}
	if (auto dst = arena->write(slot, uint32_t(sizeof(int64_t)))) {
		int64_t v = int64_t(frame);
		__sprt_memcpy(dst, &v, sizeof(v));
	}
	return Status::Ok;
}

// Frees what the frame's records own, then the frame. A local record may declare a container field,
// and a container owns an arena block that nothing else is going to free.
template <typename A, typename Graph, typename Env>
void LocalStoreT<A, Graph, Env>::releaseFrame(uint32_t activation) {
	auto frame = activationFrame(activation);
	if (frame == NullAddr || !_graph) {
		return;
	}
	auto scope = activationScope(activation);
	if (scope < _graph->getScopeCount()) {
		auto arena = _arena;
		releaseFrameRecords(*arena, *_graph, _slots, scope, frame);
		arena->free(frame);
	}
	if (auto slot = frameSlotAddr(activation)) {
		if (auto dst = _arena->write(slot, uint32_t(sizeof(int64_t)))) {
			int64_t v = 0;
			__sprt_memcpy(dst, &v, sizeof(v));
		}
	}
}

template <typename A, typename Graph, typename Env>
Status LocalStoreT<A, Graph, Env>::openActivation(uint32_t scope, uint32_t parent,
		uint32_t iteration, uint64_t openerKey, uint32_t &out) {
	auto index = getActivationCount();

	// One resize and one write of forty bytes: the five numbers belong together and are appended
	// together.
	auto addr = runField(_activations);
	if (addr == NullAddr) {
		return Status::ErrorInvalidArguemnt;
	}
	auto arena = _arena;
	auto st = value::blob::arrayResize(*arena, addr, _activations->element,
			(index + 1) * ActivationStride);
	if (st != Status::Ok) {
		return st;
	}

	// The resize may have moved the block, so the element address is taken after it, not before.
	auto slot = value::blob::arrayElementAddr(*arena, runField(_activations), _activations->element,
			index * ActivationStride);
	if (slot == NullAddr) {
		return Status::ErrorInvalidArguemnt;
	}

	int64_t raw[ActivationStride] = {};
	raw[ActScope] = int64_t(scope);
	// int32_t on the way in and out, so that NullActivation survives the round trip through a
	// signed field instead of becoming 4294967295.
	raw[ActParent] = int64_t(int32_t(parent));
	raw[ActIteration] = int64_t(iteration);
	raw[ActOpener] = int64_t(openerKey);
	raw[ActFrame] = 0; // no frame until materializeScope gives it one

	if (auto dst = arena->write(slot, ActivationStride * uint32_t(sizeof(int64_t)))) {
		__sprt_memcpy(dst, raw, sizeof(raw));
	} else {
		return Status::ErrorInvalidArguemnt;
	}

	out = index;
	return Status::Ok;
}

template <typename A, typename Graph, typename Env>
uint32_t LocalStoreT<A, Graph, Env>::getActivationCount() const {
	return arrayCountOf(_activations) / ActivationStride;
}

// One activation's five Ints, in one handle read and one arena read. Everything that asks about an
// activation goes through this, which is the whole reason the five are interleaved rather than
// parallel.
template <typename A, typename Graph, typename Env>
bool LocalStoreT<A, Graph, Env>::readActivationRaw(uint32_t activation,
		int64_t (&out)[ActivationStride]) const {
	auto addr = runField(_activations);
	if (addr == NullAddr) {
		return false;
	}
	auto arena = _arena;
	auto slot = value::blob::arrayElementAddr(*arena, addr, _activations->element,
			activation * ActivationStride);
	if (slot == NullAddr) {
		return false;
	}
	// The five lie in forty contiguous bytes of one flat block, so one read reaches all of them.
	// The last element of the array is still followed by nothing this reads past: the block always
	// holds whole activations, because openActivation only ever grows it by ActivationStride.
	auto src = arena->read(slot, ActivationStride * uint32_t(sizeof(int64_t)));
	if (!src) {
		return false;
	}
	__sprt_memcpy(out, src, sizeof(out));
	return true;
}

template <typename A, typename Graph, typename Env>
ActivationData LocalStoreT<A, Graph, Env>::readActivation(uint32_t activation) const {
	ActivationData data;
	int64_t raw[ActivationStride];
	if (!readActivationRaw(activation, raw)) {
		return data;
	}
	data.scope = uint32_t(raw[ActScope]);
	// int32_t on the way out, so that NullActivation survives the round trip through a signed field
	// instead of becoming 4294967295.
	data.parent = uint32_t(int32_t(raw[ActParent]));
	data.iteration = uint32_t(raw[ActIteration]);
	data.openerKey = uint64_t(raw[ActOpener]);
	data.frame = Addr(raw[ActFrame]);
	return data;
}

template <typename A, typename Graph, typename Env>
uint32_t LocalStoreT<A, Graph, Env>::resolveScope(uint32_t activation, uint32_t scope) const {
	return resolveScopeIn(*this, activation, scope);
}

template <typename A, typename Graph, typename Env>
Status LocalStoreT<A, Graph, Env>::pushOpen(uint32_t activation) {
	return arrayPushInt(_open, int64_t(activation));
}

template <typename A, typename Graph, typename Env>
bool LocalStoreT<A, Graph, Env>::popOpen(uint32_t &out) {
	auto addr = runField(_open);
	if (addr == NullAddr) {
		return false;
	}
	value::Var v;
	if (value::blob::arrayPop(*_arena, addr, _open->element, v) != Status::Ok) {
		return false;
	}
	out = uint32_t(v.i);
	return true;
}

template <typename A, typename Graph, typename Env>
uint32_t LocalStoreT<A, Graph, Env>::getOpenCount() const {
	return arrayCountOf(_open);
}

template <typename A, typename Graph, typename Env>
uint32_t LocalStoreT<A, Graph, Env>::getOpenAt(uint32_t index) const {
	value::Var v;
	if (value::blob::arrayGet(*_arena, runField(_open), _open->element, index, v) != Status::Ok) {
		return NullActivation;
	}
	return uint32_t(v.i);
}

template <typename A, typename Graph, typename Env>
void LocalStoreT<A, Graph, Env>::destroy() {
	// The frames first, and only then the records: a frame is an ordinary arena block that nothing
	// else is going to free, and the run record is where the list of frames lives - so the order is
	// not a preference.
	if (_arena && _graph) {
		auto count = getActivationCount();
		for (uint32_t a = 0; a < count; ++a) { releaseFrame(a); }
	}
	if (_arena) {
		RunRoot root;
		if (readRoot(root)) {
			_runType->freeInstance(*_arena, root.run);
			if (root.levels != NullAddr && _levelsType) {
				_levelsType->freeInstance(*_arena, root.levels);
			}
			_arena->free(_arena->getUserRoot());
			_arena->setUserRoot(NullAddr);
		}
	}
	_arena = nullptr;
	_graph = nullptr;
	_slots.clear();
	_frameBytes.clear();
	_hasLevels = false;
	forgetRunRecord();
}

// The stamp alone is not enough across a change of arena: every arena starts its epoch at the same
// number, so a store torn down and rebuilt on a different arena could present a stamp the memo has
// already seen while naming an address from the old one. Every entry and exit of a store's lifetime
// therefore says so explicitly, and the stamp is left to do the job it is actually good at -
// noticing that the bytes moved under a store that is still the same store.
template <typename A, typename Graph, typename Env>
void LocalStoreT<A, Graph, Env>::forgetRunRecord() {
	_runRecordMemo = NullAddr;
	_levelsRecordMemo = NullAddr;
	_runRecordEpoch = 0;
}

template <typename A, typename Graph, typename Env>
Addr LocalStoreT<A, Graph, Env>::runRecord() const {
	// See the header for why two Addrs may be held across calls and nothing else may.
	if (!_arena) {
		return NullAddr;
	}
	auto epoch = _arena->getEpoch();
	if (_runRecordMemo != NullAddr && _runRecordEpoch == epoch) {
		return _runRecordMemo;
	}
	RunRoot root;
	if (!readRoot(root)) {
		return NullAddr;
	}
	_runRecordMemo = root.run;
	_levelsRecordMemo = root.levels;
	_runRecordEpoch = epoch;
	return _runRecordMemo;
}

template <typename A, typename Graph, typename Env>
Addr LocalStoreT<A, Graph, Env>::runField(const value::FieldDesc *field) const {
	if (!field) {
		return NullAddr;
	}
	auto record = runRecord();
	return record == NullAddr ? NullAddr : record + field->offset;
}

template <typename A, typename Graph, typename Env>
Status LocalStoreT<A, Graph, Env>::arrayPushInt(const value::FieldDesc *field, int64_t value) {
	auto addr = runField(field);
	if (addr == NullAddr) {
		return Status::ErrorInvalidArguemnt;
	}
	return value::blob::arrayPush(*_arena, addr, field->element, value::makeInt(value));
}

template <typename A, typename Graph, typename Env>
uint32_t LocalStoreT<A, Graph, Env>::arrayCountOf(const value::FieldDesc *field) const {
	auto addr = runField(field);
	if (addr == NullAddr) {
		return 0;
	}
	return value::blob::arrayCount(*_arena, addr, field->element);
}

template <typename A, typename Graph, typename Env>
Addr LocalStoreT<A, Graph, Env>::activationFrame(uint32_t activation) const {
	int64_t raw[ActivationStride];
	return readActivationRaw(activation, raw) ? Addr(raw[ActFrame]) : NullAddr;
}

template <typename A, typename Graph, typename Env>
uint32_t LocalStoreT<A, Graph, Env>::activationScope(uint32_t activation) const {
	int64_t raw[ActivationStride];
	return readActivationRaw(activation, raw) ? uint32_t(raw[ActScope]) : InvalidIndex;
}

// The address of an activation's `frame` slot, for the two places that write one.
template <typename A, typename Graph, typename Env>
Addr LocalStoreT<A, Graph, Env>::frameSlotAddr(uint32_t activation) const {
	auto addr = runField(_activations);
	if (addr == NullAddr) {
		return NullAddr;
	}
	return value::blob::arrayElementAddr(*_arena, addr, _activations->element,
			activation * ActivationStride + ActFrame);
}

// Where a node's records live, and the check that makes the arithmetic safe. The scope check is not
// defensive tidiness, it is the whole safety of the scheme: an offset cannot fail to land
// somewhere, so asking for a body node in the root activation would compute an address inside a
// frame laid out for a different scope and hand back somebody else's bytes. A debugger does ask
// exactly that - a breakpoint watching a node of a loop body defaults to the root activation. Two
// array reads, and then everything about the node is an addition away.
template <typename A, typename Graph, typename Env>
Addr LocalStoreT<A, Graph, Env>::frameFor(uint32_t node, uint32_t activation) const {
	return frameForIn(*this, _graph, node, activation);
}

template <typename A, typename Graph, typename Env>
bool LocalStoreT<A, Graph, Env>::hasRecord(uint32_t node, uint32_t activation) const {
	return frameFor(node, activation) != NullAddr;
}

// By node index and not by RuntimeNode: the index is what the caller already has, and looking one
// up from a reference would be a search on the path this whole layout exists to make an addition.
template <typename A, typename Graph, typename Env>
Addr LocalStoreT<A, Graph, Env>::getStateIn(Addr frame, uint32_t node) const {
	if (frame == NullAddr || node >= _slots.size()) {
		return NullAddr;
	}
	return frame + _slots[node].stateOffset;
}

template <typename A, typename Graph, typename Env>
Addr LocalStoreT<A, Graph, Env>::getRecordIn(Addr frame, uint32_t node) const {
	if (frame == NullAddr || node >= _slots.size()) {
		return NullAddr;
	}
	auto offset = _slots[node].recordOffset;
	return offset == InvalidIndex ? NullAddr : frame + offset;
}

template <typename A, typename Graph, typename Env>
Addr LocalStoreT<A, Graph, Env>::getState(uint32_t node, uint32_t activation) const {
	auto frame = frameFor(node, activation);
	return frame == NullAddr ? NullAddr : frame + _slots[node].stateOffset;
}

template <typename A, typename Graph, typename Env>
Addr LocalStoreT<A, Graph, Env>::getRecord(uint32_t node, uint32_t activation) const {
	if (!_graph || node >= _graph->getNodeCount()) {
		return NullAddr;
	}
	auto offset = _slots[node].recordOffset;
	if (offset == InvalidIndex) {
		return NullAddr;
	}
	auto frame = frameFor(node, activation);
	return frame == NullAddr ? NullAddr : frame + offset;
}

// The sum of the scope widths of every activation that has been materialised. There is no array of
// this length any more - it is counted, which is what makes it honest about activations that were
// opened and never filled.
template <typename A, typename Graph, typename Env>
uint32_t LocalStoreT<A, Graph, Env>::getRecordCount() const {
	return recordCountIn(*this, _graph);
}

template <typename A, typename Graph, typename Env>
void LocalStoreT<A, Graph, Env>::forEachRecord(
		const Callback<bool(uint32_t, uint32_t, Addr)> &cb) const {
	forEachRecordIn(*this, _graph, cb);
}

template <typename A, typename Graph, typename Env>
NodeStateData LocalStoreT<A, Graph, Env>::readState(Addr stateAddr) const {
	return readNodeState(*_arena, *_stateType, _stateFields, stateAddr);
}

template <typename A, typename Graph, typename Env>
Status LocalStoreT<A, Graph, Env>::setTurns(Addr stateAddr, uint32_t turns) {
	if (stateAddr == NullAddr) {
		return Status::ErrorInvalidArguemnt;
	}
	return writeStateField(*_stateType, *_arena, stateAddr, *_stateFields.turns,
			value::makeInt(int64_t(turns)));
}

template <typename A, typename Graph, typename Env>
Status LocalStoreT<A, Graph, Env>::markInput(Addr stateAddr, uint32_t pin) {
	if (pin >= MaxDataPins) {
		return Status::ErrorInvalidArguemnt;
	}
	return setStateBits(*_arena, stateAddr, *_stateFields.inputs, int64_t(1) << pin);
}

template <typename A, typename Graph, typename Env>
Status LocalStoreT<A, Graph, Env>::markProduced(Addr stateAddr, uint32_t pin) {
	if (pin >= MaxDataPins) {
		return Status::ErrorInvalidArguemnt;
	}
	return setStateBits(*_arena, stateAddr, *_stateFields.produced, int64_t(1) << pin);
}

template <typename A, typename Graph, typename Env>
Status LocalStoreT<A, Graph, Env>::addFlags(Addr stateAddr, int64_t flags) {
	return setStateBits(*_arena, stateAddr, *_stateFields.flags, flags);
}

template <typename A, typename Graph, typename Env>
Status LocalStoreT<A, Graph, Env>::removeFlags(Addr stateAddr, int64_t flags) {
	return clearStateBits(*_arena, stateAddr, *_stateFields.flags, flags);
}

template <typename A, typename Graph, typename Env>
Status LocalStoreT<A, Graph, Env>::setStall(Addr stateAddr, int64_t reason, uint32_t pin) {
	if (stateAddr == NullAddr) {
		return Status::ErrorInvalidArguemnt;
	}
	auto arena = _arena;
	auto st = writeStateField(*_stateType, *arena, stateAddr, *_stateFields.stallPin,
			value::makeInt(int64_t(pin)));
	if (st != Status::Ok) {
		return st;
	}
	return setStateBits(*arena, stateAddr, *_stateFields.flags, reason);
}

template <typename A, typename Graph, typename Env>
Status LocalStoreT<A, Graph, Env>::clearStall(Addr stateAddr) {
	auto st = clearStateBits(*_arena, stateAddr, *_stateFields.flags,
			NodeFlags::StallData | NodeFlags::StallExec);
	if (st != Status::Ok) {
		return st;
	}
	return writeStateField(*_stateType, *_arena, stateAddr, *_stateFields.stallPin,
			value::makeInt(0));
}

template <typename A, typename Graph, typename Env>
Addr LocalStoreT<A, Graph, Env>::levelField() const {
	if (!_hasLevels) {
		return NullAddr;
	}
	if (runRecord() == NullAddr || _levelsRecordMemo == NullAddr) {
		return NullAddr;
	}
	return _levelsRecordMemo + _ready1->offset;
}

template <typename A, typename Graph, typename Env>
Status LocalStoreT<A, Graph, Env>::pushReady(uint32_t node, uint32_t activation) {
	if (_hasLevels && frontLevel(*_graph, node, _weighted) > 0) {
		auto addr = levelField();
		if (addr == NullAddr) {
			return Status::ErrorInvalidArguemnt;
		}
		return value::blob::arrayPush(*_arena, addr, _ready1->element,
				value::makeInt(int64_t(makeRecordKey(node, activation))));
	}
	return arrayPushInt(_ready, int64_t(makeRecordKey(node, activation)));
}

// The front is a stack, so a pop is a read of the last element and a truncate - and both go through
// one address of the handle, which is what blob::arrayPop is for.
template <typename A, typename Graph, typename Env>
bool LocalStoreT<A, Graph, Env>::popReady(uint32_t &node, uint32_t &activation) {
	auto upper = levelField();
	if (upper != NullAddr && value::blob::arrayCount(*_arena, upper, _ready1->element) > 0) {
		value::Var v;
		if (value::blob::arrayPop(*_arena, upper, _ready1->element, v) != Status::Ok) {
			return false;
		}
		node = recordKeyNode(uint64_t(v.i));
		activation = recordKeyActivation(uint64_t(v.i));
		return true;
	}
	auto addr = runField(_ready);
	if (addr == NullAddr) {
		return false;
	}
	value::Var v;
	if (value::blob::arrayPop(*_arena, addr, _ready->element, v) != Status::Ok) {
		return false;
	}
	node = recordKeyNode(uint64_t(v.i));
	activation = recordKeyActivation(uint64_t(v.i));
	return true;
}

template <typename A, typename Graph, typename Env>
bool LocalStoreT<A, Graph, Env>::peekReady(uint32_t &node, uint32_t &activation) const {
	auto arena = _arena;
	auto upper = levelField();
	if (upper != NullAddr) {
		auto count = value::blob::arrayCount(*arena, upper, _ready1->element);
		value::Var v;
		if (count > 0
				&& value::blob::arrayGet(*arena, upper, _ready1->element, count - 1, v)
						== Status::Ok) {
			node = recordKeyNode(uint64_t(v.i));
			activation = recordKeyActivation(uint64_t(v.i));
			return true;
		}
	}
	auto addr = runField(_ready);
	if (addr == NullAddr) {
		return false;
	}
	auto count = value::blob::arrayCount(*arena, addr, _ready->element);
	if (count == 0) {
		return false;
	}
	value::Var v;
	if (value::blob::arrayGet(*arena, addr, _ready->element, count - 1, v) != Status::Ok) {
		return false;
	}
	node = recordKeyNode(uint64_t(v.i));
	activation = recordKeyActivation(uint64_t(v.i));
	return true;
}

template <typename A, typename Graph, typename Env>
uint32_t LocalStoreT<A, Graph, Env>::getReadyCount() const {
	auto upper = levelField();
	auto count = arrayCountOf(_ready);
	return upper == NullAddr ? count
							 : count + value::blob::arrayCount(*_arena, upper, _ready1->element);
}

template <typename A, typename Graph, typename Env>
Status LocalStoreT<A, Graph, Env>::pushStalled(uint32_t node, uint32_t activation) {
	return arrayPushInt(_stalled, int64_t(makeRecordKey(node, activation)));
}

template <typename A, typename Graph, typename Env>
uint32_t LocalStoreT<A, Graph, Env>::getStalledCount() const {
	return arrayCountOf(_stalled);
}

template <typename A, typename Graph, typename Env>
uint64_t LocalStoreT<A, Graph, Env>::getStalledAt(uint32_t index) const {
	value::Var v;
	if (value::blob::arrayGet(*_arena, runField(_stalled), _stalled->element, index, v)
			!= Status::Ok) {
		return NullRecordKey;
	}
	return uint64_t(v.i);
}

template <typename A, typename Graph, typename Env>
Status LocalStoreT<A, Graph, Env>::clearStalled() {
	auto addr = runField(_stalled);
	if (addr == NullAddr) {
		return Status::ErrorInvalidArguemnt;
	}
	return value::blob::arrayResize(*_arena, addr, _stalled->element, 0);
}

template <typename A, typename Graph, typename Env>
uint32_t LocalStoreT<A, Graph, Env>::getStep() const {
	value::Var v;
	auto record = runRecord();
	SP_FLOW_VALUE_COUNT(getFieldRun);
	if (record == NullAddr || _runType->getField(*_arena, record, *_step, v) != Status::Ok) {
		return 0;
	}
	return uint32_t(v.i);
}

template <typename A, typename Graph, typename Env>
Status LocalStoreT<A, Graph, Env>::setStep(uint32_t value) {
	auto record = runRecord();
	if (record == NullAddr) {
		return Status::ErrorInvalidArguemnt;
	}
	SP_FLOW_VALUE_COUNT(setFieldRun);
	return _runType->setField(*_arena, record, *_step, value::makeInt(int64_t(value)));
}

template <typename A, typename Graph, typename Env>
uint32_t LocalStoreT<A, Graph, Env>::getPass() const {
	value::Var v;
	auto record = runRecord();
	SP_FLOW_VALUE_COUNT(getFieldRun);
	if (record == NullAddr || _runType->getField(*_arena, record, *_pass, v) != Status::Ok) {
		return 0;
	}
	return uint32_t(v.i);
}

template <typename A, typename Graph, typename Env>
Status LocalStoreT<A, Graph, Env>::setPass(uint32_t value) {
	auto record = runRecord();
	if (record == NullAddr) {
		return Status::ErrorInvalidArguemnt;
	}
	SP_FLOW_VALUE_COUNT(setFieldRun);
	return _runType->setField(*_arena, record, *_pass, value::makeInt(int64_t(value)));
}

template <typename A, typename Graph, typename Env>
void LocalStoreT<A, Graph, Env>::describe(mem_std::Value &out) const {
	describeLocal(*this, _graph, out);
}

} // namespace stappler::flow

#endif /* STAPPLER_FLOW_SPFLOWLOCAL_HPP_ */
