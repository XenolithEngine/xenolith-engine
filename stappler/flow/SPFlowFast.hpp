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

#ifndef STAPPLER_FLOW_SPFLOWFAST_HPP_
#define STAPPLER_FLOW_SPFLOWFAST_HPP_

// The fast local store's bodies. An .hpp for the reason SPFlowLocal.hpp gives: more than one
// compile unit instantiates them. Read it beside SPFlowLocal.hpp - every method here answers the
// question its twin there answers, and where the answer is a rule (what a fresh frame holds, what
// a dying one releases, how a mask is set, where a node's records are, what a paused run looks
// like) it is the same free function (SPFlowLocal.h) rather than a second copy. What is written
// out here is the part that is genuinely different: the run's own bookkeeping, a host vector where
// the arena store has an array blob.

#include "SPFlowFast.h"

namespace STAPPLER_VERSIONIZED stappler::flow {

template <typename A, typename Graph, typename Env>
Status FastLocalT<A, Graph, Env>::registerCoreTypes(value::TypeRegistry &reg) {
	return LocalStoreT<A, Graph, Env>::registerCoreTypes(reg);
}

template <typename A, typename Graph, typename Env>
FastLocalT<A, Graph, Env>::~FastLocalT() {
	destroy();
}

template <typename A, typename Graph, typename Env>
Status FastLocalT<A, Graph, Env>::init(A &, const Graph &graph, const value::TypeRegistry &reg) {
	destroy();

	_stateType = reg.get(NodeStateTypeName);
	if (!_stateType) {
		return Status::ErrorInvalidArguemnt;
	}
	auto st = resolveNodeStateFields(*_stateType, _stateFields);
	if (st != Status::Ok) {
		return st;
	}

	// The scratch arena outlives a run deliberately: this mode exists to be entered once a frame,
	// and re-cutting an allocator every time would be part of the fixed cost it removes. What a run
	// owns is its frames, and destroy() gives those back.
	if (!_scratch.isInitialized() && !_scratch.init()) {
		return Status::ErrorOutOfHostMemory;
	}
	_graph = &graph;

	// The one calculation, from the one function: the store, the generator and the unit loader all
	// call it, and a frame laid out differently here would be a different run.
	st = computeFrameLayout(graph, *_stateType, _slots, _frameBytes);
	if (st != Status::Ok) {
		destroy();
		return st;
	}

	// The root activation, and its frame. Same order as the arena store: open it, then materialize.
	uint32_t root = NullActivation;
	st = openActivation(0, NullActivation, 0, NullRecordKey, root);
	if (st != Status::Ok || root != RootActivation) {
		destroy();
		return Status::ErrorInvalidArguemnt;
	}
	st = materializeScope(0, RootActivation);
	if (st != Status::Ok) {
		destroy();
		return st;
	}
	return Status::Ok;
}

template <typename A, typename Graph, typename Env>
Status FastLocalT<A, Graph, Env>::open(A &, const Graph &, const value::TypeRegistry &) {
	// Not "not implemented yet": there is nothing to implement. Half of this run is host memory and
	// no image holds it, so an image cannot be picked up here. A host that wants to resume a run
	// wants the arena store, and the machine's `attach` reports this outcome as Invalid.
	return Status::ErrorNotSupported;
}

template <typename A, typename Graph, typename Env>
void FastLocalT<A, Graph, Env>::destroy() {
	// The frames first: a record may own a container block, and freeing the arena underneath one
	// would skip the release the schema owns. Doing it in order keeps the two stores' behaviour the
	// same where it can be seen.
	if (_graph && _scratch.isInitialized()) {
		for (uint32_t a = 0; a < uint32_t(_activations.size()); ++a) { releaseFrame(a); }
	}
	_activations.clear();
	_ready.clear();
	_ready1.clear();
	_stalled.clear();
	_open.clear();
	_step = 0;
	_pass = 0;
	_slots.clear();
	_frameBytes.clear();
	_graph = nullptr;
	_stateType = nullptr;
	_stateFields = NodeStateFields();
	// The arena stays. It holds no frame now - every one of them was freed above - and the next run
	// cuts its frames out of the memory this one gave back, which is the whole shape of the mode.
}

template <typename A, typename Graph, typename Env>
void FastLocalT<A, Graph, Env>::releaseFrame(uint32_t activation) {
	if (activation >= _activations.size()) {
		return;
	}
	auto &act = _activations[activation];
	if (act.frame == NullAddr) {
		return;
	}
	releaseFrameRecords(_scratch, *_graph, _slots, act.scope, act.frame);
	_scratch.free(act.frame);
	act.frame = NullAddr;
}

// Frames.

template <typename A, typename Graph, typename Env>
Status FastLocalT<A, Graph, Env>::materializeScope(uint32_t scope, uint32_t activation) {
	if (!_graph || scope >= _graph->getScopeCount() || activation >= _activations.size()) {
		return Status::ErrorInvalidArguemnt;
	}
	auto &act = _activations[activation];
	if (act.scope != scope) {
		return Status::ErrorInvalidArguemnt;
	}
	if (act.frame != NullAddr) {
		return Status::ErrorAlreadyPerformed;
	}

	auto bytes = _frameBytes[scope];
	auto frame = _scratch.alloc(bytes, value::MaxAlign);
	if (frame == NullAddr) {
		return Status::ErrorOutOfHostMemory;
	}
	if (initFrame(_scratch, *_graph, _slots, *_stateType, scope, frame, bytes) != Status::Ok) {
		_scratch.free(frame);
		return Status::ErrorOutOfHostMemory;
	}
	act.frame = frame;
	return Status::Ok;
}

template <typename A, typename Graph, typename Env>
Addr FastLocalT<A, Graph, Env>::activationFrame(uint32_t activation) const {
	return activation < _activations.size() ? _activations[activation].frame : NullAddr;
}

template <typename A, typename Graph, typename Env>
uint32_t FastLocalT<A, Graph, Env>::activationScope(uint32_t activation) const {
	return activation < _activations.size() ? _activations[activation].scope : InvalidIndex;
}

template <typename A, typename Graph, typename Env>
Addr FastLocalT<A, Graph, Env>::frameFor(uint32_t node, uint32_t activation) const {
	return frameForIn(*this, _graph, node, activation);
}

template <typename A, typename Graph, typename Env>
bool FastLocalT<A, Graph, Env>::hasRecord(uint32_t node, uint32_t activation) const {
	return frameFor(node, activation) != NullAddr;
}

template <typename A, typename Graph, typename Env>
uint32_t FastLocalT<A, Graph, Env>::getFrameBytes(uint32_t scope) const {
	return scope < _frameBytes.size() ? _frameBytes[scope] : 0;
}

template <typename A, typename Graph, typename Env>
Addr FastLocalT<A, Graph, Env>::getStateIn(Addr frame, uint32_t node) const {
	if (frame == NullAddr || node >= _slots.size()) {
		return NullAddr;
	}
	return frame + _slots[node].stateOffset;
}

template <typename A, typename Graph, typename Env>
Addr FastLocalT<A, Graph, Env>::getRecordIn(Addr frame, uint32_t node) const {
	if (frame == NullAddr || node >= _slots.size()) {
		return NullAddr;
	}
	auto offset = _slots[node].recordOffset;
	return offset == InvalidIndex ? NullAddr : frame + offset;
}

template <typename A, typename Graph, typename Env>
Addr FastLocalT<A, Graph, Env>::getState(uint32_t node, uint32_t activation) const {
	return getStateIn(frameFor(node, activation), node);
}

template <typename A, typename Graph, typename Env>
Addr FastLocalT<A, Graph, Env>::getRecord(uint32_t node, uint32_t activation) const {
	return getRecordIn(frameFor(node, activation), node);
}

// Activations.

template <typename A, typename Graph, typename Env>
Status FastLocalT<A, Graph, Env>::openActivation(uint32_t scope, uint32_t parent,
		uint32_t iteration, uint64_t openerKey, uint32_t &out) {
	// Never reused within a run, exactly as in the arena store: a debugger stopped inside iteration
	// seventeen has to be able to read iteration three, and an id that came round again would name
	// two turns.
	out = uint32_t(_activations.size());
	_activations.emplace_back(Activation{
		.scope = scope,
		.parent = parent,
		.iteration = iteration,
		.openerKey = openerKey,
		.frame = NullAddr, // no frame until materializeScope gives it one
	});
	return Status::Ok;
}

template <typename A, typename Graph, typename Env>
ActivationData FastLocalT<A, Graph, Env>::readActivation(uint32_t activation) const {
	ActivationData data;
	if (activation >= _activations.size()) {
		return data;
	}
	auto &act = _activations[activation];
	data.scope = act.scope;
	data.parent = act.parent;
	data.iteration = act.iteration;
	data.openerKey = act.openerKey;
	data.frame = act.frame;
	return data;
}

template <typename A, typename Graph, typename Env>
uint32_t FastLocalT<A, Graph, Env>::resolveScope(uint32_t activation, uint32_t scope) const {
	return resolveScopeIn(*this, activation, scope);
}

template <typename A, typename Graph, typename Env>
Status FastLocalT<A, Graph, Env>::pushOpen(uint32_t activation) {
	_open.emplace_back(activation);
	return Status::Ok;
}

template <typename A, typename Graph, typename Env>
bool FastLocalT<A, Graph, Env>::popOpen(uint32_t &out) {
	if (_open.empty()) {
		return false;
	}
	out = _open.back();
	_open.pop_back();
	return true;
}

template <typename A, typename Graph, typename Env>
uint32_t FastLocalT<A, Graph, Env>::getOpenAt(uint32_t index) const {
	return index < _open.size() ? _open[index] : NullActivation;
}

template <typename A, typename Graph, typename Env>
uint32_t FastLocalT<A, Graph, Env>::getRecordCount() const {
	return recordCountIn(*this, _graph);
}

template <typename A, typename Graph, typename Env>
void FastLocalT<A, Graph, Env>::forEachRecord(
		const Callback<bool(uint32_t, uint32_t, Addr)> &cb) const {
	forEachRecordIn(*this, _graph, cb);
}

// Node state.

template <typename A, typename Graph, typename Env>
NodeStateData FastLocalT<A, Graph, Env>::readState(Addr stateAddr) const {
	return readNodeState(_scratch, *_stateType, _stateFields, stateAddr);
}

template <typename A, typename Graph, typename Env>
Status FastLocalT<A, Graph, Env>::setTurns(Addr stateAddr, uint32_t turns) {
	if (stateAddr == NullAddr) {
		return Status::ErrorInvalidArguemnt;
	}
	return writeStateField(*_stateType, _scratch, stateAddr, *_stateFields.turns,
			value::makeInt(int64_t(turns)));
}

template <typename A, typename Graph, typename Env>
Status FastLocalT<A, Graph, Env>::markInput(Addr stateAddr, uint32_t pin) {
	if (pin >= MaxDataPins) {
		return Status::ErrorInvalidArguemnt;
	}
	return setStateBits(_scratch, stateAddr, *_stateFields.inputs, int64_t(1) << pin);
}

template <typename A, typename Graph, typename Env>
Status FastLocalT<A, Graph, Env>::markProduced(Addr stateAddr, uint32_t pin) {
	if (pin >= MaxDataPins) {
		return Status::ErrorInvalidArguemnt;
	}
	return setStateBits(_scratch, stateAddr, *_stateFields.produced, int64_t(1) << pin);
}

template <typename A, typename Graph, typename Env>
Status FastLocalT<A, Graph, Env>::addFlags(Addr stateAddr, int64_t flags) {
	return setStateBits(_scratch, stateAddr, *_stateFields.flags, flags);
}

template <typename A, typename Graph, typename Env>
Status FastLocalT<A, Graph, Env>::removeFlags(Addr stateAddr, int64_t flags) {
	return clearStateBits(_scratch, stateAddr, *_stateFields.flags, flags);
}

template <typename A, typename Graph, typename Env>
Status FastLocalT<A, Graph, Env>::setStall(Addr stateAddr, int64_t reason, uint32_t pin) {
	if (stateAddr == NullAddr) {
		return Status::ErrorInvalidArguemnt;
	}
	auto st = writeStateField(*_stateType, _scratch, stateAddr, *_stateFields.stallPin,
			value::makeInt(int64_t(pin)));
	if (st != Status::Ok) {
		return st;
	}
	return setStateBits(_scratch, stateAddr, *_stateFields.flags, reason);
}

template <typename A, typename Graph, typename Env>
Status FastLocalT<A, Graph, Env>::clearStall(Addr stateAddr) {
	auto st = clearStateBits(_scratch, stateAddr, *_stateFields.flags,
			NodeFlags::StallData | NodeFlags::StallExec);
	if (st != Status::Ok) {
		return st;
	}
	return writeStateField(*_stateType, _scratch, stateAddr, *_stateFields.stallPin,
			value::makeInt(0));
}

// The run's own state.

template <typename A, typename Graph, typename Env>
Status FastLocalT<A, Graph, Env>::pushReady(uint32_t node, uint32_t activation) {
	(frontLevel(*_graph, node, _weighted) > 0 ? _ready1 : _ready)
			.emplace_back(makeRecordKey(node, activation));
	return Status::Ok;
}

template <typename A, typename Graph, typename Env>
bool FastLocalT<A, Graph, Env>::popReady(uint32_t &node, uint32_t &activation) {
	auto &level = _ready1.empty() ? _ready : _ready1;
	if (level.empty()) {
		return false;
	}
	auto key = level.back();
	level.pop_back();
	node = recordKeyNode(key);
	activation = recordKeyActivation(key);
	return true;
}

template <typename A, typename Graph, typename Env>
bool FastLocalT<A, Graph, Env>::peekReady(uint32_t &node, uint32_t &activation) const {
	auto &level = _ready1.empty() ? _ready : _ready1;
	if (level.empty()) {
		return false;
	}
	auto key = level.back();
	node = recordKeyNode(key);
	activation = recordKeyActivation(key);
	return true;
}

template <typename A, typename Graph, typename Env>
Status FastLocalT<A, Graph, Env>::pushStalled(uint32_t node, uint32_t activation) {
	_stalled.emplace_back(makeRecordKey(node, activation));
	return Status::Ok;
}

template <typename A, typename Graph, typename Env>
uint64_t FastLocalT<A, Graph, Env>::getStalledAt(uint32_t index) const {
	return index < _stalled.size() ? _stalled[index] : NullRecordKey;
}

template <typename A, typename Graph, typename Env>
Status FastLocalT<A, Graph, Env>::clearStalled() {
	_stalled.clear();
	return Status::Ok;
}

template <typename A, typename Graph, typename Env>
void FastLocalT<A, Graph, Env>::describe(mem_std::Value &out) const {
	describeLocal(*this, _graph, out);
}

} // namespace stappler::flow

#endif /* STAPPLER_FLOW_SPFLOWFAST_HPP_ */
