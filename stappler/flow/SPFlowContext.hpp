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

#ifndef STAPPLER_FLOW_SPFLOWCONTEXT_HPP_
#define STAPPLER_FLOW_SPFLOWCONTEXT_HPP_

// The door an operation is handed: the bodies of ContextT<Graph, Local>. An .hpp rather than a .cc
// subunit for the reason SPFlowLocal.hpp gives: instantiated in more than one compile unit - the
// module's own for the built graph and the three arena kinds, and every generated unit for the
// static graph it carries.

#include "SPFlowContext.h"

#include <sprt/c/__sprt_string.h>

namespace STAPPLER_VERSIONIZED stappler::flow {

/* A field of a node's own record, read and written here rather than through
ComponentType::getField/setField. Same rules and one implementation of them: `value::decodeField`,
`checkFieldWrite` and `encodeField` are the bodies of those two members, split out of them for this
(SPFlowValueSchema.h). Nothing is reimplemented and nothing is skipped - the Bool corrupt check, the
zeroed lanes of a short vector, the subtype refusal on a write and the container refusal are all in
there, and the write barrier is still what announces the change, over `field.size` bytes at
`field.offset`. What is saved is the call: the schema layer is instantiated per arena in
`XSVStore.scu.cpp` and this build has no LTO, so a value crossing an edge would otherwise cost a
call across a translation unit to move eight bytes. */

template <typename A>
inline Status readRecordField(const A &arena, Addr record, const FieldShape &shape,
		value::Var &out) {
	SP_FLOW_VALUE_COUNT(getFieldRecord);
	if (record == NullAddr) {
		return Status::ErrorInvalidArguemnt;
	}
	auto src = arena.read(record + shape.offset, shape.size);
	if (!src) {
		return Status::ErrorInvalidArguemnt;
	}
	return value::decodeField(src, shape.type, shape.size, shape.element, shape.subtypeId, out);
}

template <typename A>
inline Status writeRecordField(A &arena, Addr record, const FieldShape &shape,
		const value::Var &value) {
	SP_FLOW_VALUE_COUNT(setFieldRecord);
	if (record == NullAddr) {
		return Status::ErrorInvalidArguemnt;
	}
	auto st = value::checkFieldWrite(shape.type, shape.subtypeId, value);
	if (st != Status::Ok) {
		return st;
	}
	auto dst = arena.write(record + shape.offset, shape.size);
	if (!dst) {
		return Status::ErrorInvalidArguemnt;
	}
	return value::encodeField(dst, shape.type, shape.size, value);
}

// Filling the door: the node's state and its record are an addition on the frame, and which
// addition is the site's answer - the store's layout table for the interpreter, a constant for a
// generated unit. Everything else is copied as the machine resolved it.
template <typename Graph, typename Local, typename Site>
void ContextT<Graph, Local, Site>::bind(const StepSite<Graph, Local> &site) {
	_graph = site.graph;
	_local = site.local;
	_scene = site.scene;
	_op = site.op;
	_node = site.node;
	_id = site.id;
	_activation = site.activation;
	_record = Site::template recordIn<Local>(*site.local, site.frame, site.node);
	_state = Site::template stateIn<Local>(*site.local, site.frame, site.node);
	// Resolved once when the door is filled, not tested on every call.
	_readsScene = (site.op->getFlags() & OpFlags::ReadsScene) != 0;
	_writesScene = (site.op->getFlags() & OpFlags::WritesScene) != 0;
	// Empty for an unbound graph and for a node that names nothing, which is what makes the bound
	// doors an addition rather than a replacement.
	_sceneBind = site.sceneBind;
	_extBind = site.extBind;
	_namedBind = site.namedBind;
	_fired = 0;
	_block = site.block;
	_branchActivation = site.branchActivation;
	_branchFrame = site.branchFrame;
	_branchEntity = site.branchEntity;
}

template <typename Graph, typename Local, typename Site>
void ContextT<Graph, Local, Site>::finish(StepSite<Graph, Local> &site) {
	releaseScratch();
	site.fired = _fired;
}

// The activation the producer's record is in. The same one for a producer in the same scope; for a
// producer outside the loop, the enclosing activation the walk up the tree lands on.
template <typename Graph, typename Local, typename Site>
uint32_t ContextT<Graph, Local, Site>::sourceActivation(uint32_t srcNode) const {
	return _local->resolveScope(_activation, _graph->getNodeAt(srcNode).scope);
}

template <typename Graph, typename Local, typename Site>
Status ContextT<Graph, Local, Site>::readEdgeValue(uint32_t pin, const typename Graph::DataEdge &edge,
		Var &out) const {
	// What a fan-out hands its branches is the branch's own: its entity from the header, its index
	// from the activation.
	if (_block != InvalidIndex && edge.srcNode == _graph->getBlockAt(_block).fanOut) {
		auto &block = _graph->getBlockAt(_block);
		Var raw;
		if (edge.srcPin == block.entityPin) {
			raw = value::makeEntityRef(_branchEntity);
		} else if (edge.srcPin == block.indexPin) {
			raw = value::makeInt32(int32_t(_local->readActivation(_branchActivation).iteration));
		} else {
			return Status::ErrorNotFound;
		}
		if (edge.cast == value::CastRule::Same) {
			out = raw;
			return Status::Ok;
		}
		return value::castVar(raw, _op->getDataIn()[pin].type, value::CastPolicy::Lossless, out);
	}

	auto shape = Site::template fieldAt<Graph>(*_graph, edge.srcNode, edge.srcPin);
	auto record = _local->getRecord(edge.srcNode, sourceActivation(edge.srcNode));
	if (!shape.valid || record == NullAddr) {
		return Status::ErrorNotFound;
	}

	Var raw;
	auto st = readRecordField(*_local->getArena(), record, shape, raw);
	if (st != Status::Ok) {
		return st;
	}

	// The rule was chosen by the build. Nothing here consults the conversion matrix.
	if (edge.cast == value::CastRule::Same) {
		out = raw;
		return Status::Ok;
	}
	return value::castVar(raw, _op->getDataIn()[pin].type, value::CastPolicy::Lossless, out);
}

template <typename Graph, typename Local, typename Site>
bool ContextT<Graph, Local, Site>::hasInput(uint32_t pin) const {
	if (pin >= _op->getDataIn().size()) {
		return false;
	}
	if (incoming(pin)) {
		auto state = _local->readState(_state);
		return (state.inputs & (uint32_t(1) << pin)) != 0;
	}
	return _graph->getConstant(_node, pin) != nullptr;
}

template <typename Graph, typename Local, typename Site>
Status ContextT<Graph, Local, Site>::getInput(uint32_t pin, Var &out) const {
	out = value::makeNil();
	if (pin >= _op->getDataIn().size()) {
		return Status::ErrorInvalidArguemnt;
	}

	if (auto edge = incoming(pin)) {
		return readEdgeValue(pin, *edge, out);
	}

	auto &desc = _op->getDataIn()[pin];
	if (auto constant = _graph->getConstant(_node, pin)) {
		if (!value::isContainerType(desc.type)) {
			return value::decodeVar(*constant, desc.type, out);
		}
		auto handle = materializeConstant(pin);
		if (handle == NullAddr) {
			return Status::ErrorOutOfHostMemory;
		}
		out = value::makeBlob(desc.type, desc.element,
				value::blob::getHandle(*_local->getArena(), handle));
		return Status::Ok;
	}

	// An optional input with nothing behind it reads as the type's zero, which is a value, not a
	// failure: that is what "optional" means here.
	return value::decodeVar(mem_std::Value(), desc.type, out);
}

// A String input, as a view rather than as bytes in the arena. The constant table is a
// mem_std::Vector<Value> sized once by the build and never resized, so a view into one of its
// strings is good for the graph's lifetime, which is longer than any run, and a name that never
// changes is handed back without allocating anything. An edge-fed name still goes through the
// arena, because then it really is a value somebody computed.
template <typename Graph, typename Local, typename Site>
Status ContextT<Graph, Local, Site>::getInputName(uint32_t pin, StringView &out) const {
	out = StringView();
	if (pin >= _op->getDataIn().size()) {
		return Status::ErrorInvalidArguemnt;
	}

	if (!incoming(pin)) {
		if (auto constant = _graph->getConstant(_node, pin)) {
			if (!constant->isString()) {
				return Status::ErrorInvalidArguemnt;
			}
			out = StringView(constant->getString());
			return out.empty() ? Status::ErrorInvalidArguemnt : Status::Ok;
		}
		return Status::ErrorNotFound;
	}

	auto addr = getInputAddr(pin);
	if (addr == NullAddr) {
		return Status::ErrorNotFound;
	}
	// A borrowed view of the producer's own bytes; it dies at the next allocator call, which is why
	// the callers turn it into a TypeId or a descriptor immediately and keep neither.
	value::blob::stringRead(*_local->getArena(), addr, [&](StringView text) { out = text; });
	return out.empty() ? Status::ErrorInvalidArguemnt : Status::Ok;
}

// The two array readers. Both answer from the graph's constant table when the pin has no edge, and
// both fall back to the arena when it has one.
template <typename Graph, typename Local, typename Site>
Status ContextT<Graph, Local, Site>::getInputCount(uint32_t pin, uint32_t &out) const {
	out = 0;
	if (pin >= _op->getDataIn().size()) {
		return Status::ErrorInvalidArguemnt;
	}
	auto &desc = _op->getDataIn()[pin];
	if (desc.type != value::VarType::Array) {
		return Status::ErrorInvalidArguemnt;
	}

	if (!incoming(pin)) {
		auto constant = _graph->getConstant(_node, pin);
		if (!constant) {
			return Status::Ok; // an optional input with nothing behind it is an empty array
		}
		if (!constant->isArray()) {
			return Status::ErrorInvalidArguemnt;
		}
		out = uint32_t(constant->size());
		return Status::Ok;
	}

	auto addr = getInputAddr(pin);
	if (addr == NullAddr) {
		return Status::ErrorNotFound;
	}
	out = value::blob::arrayCount(*_local->getArena(), addr, desc.element);
	return Status::Ok;
}

template <typename Graph, typename Local, typename Site>
Status ContextT<Graph, Local, Site>::getInputElement(uint32_t pin, uint32_t index, Var &out) const {
	out = value::makeNil();
	if (pin >= _op->getDataIn().size()) {
		return Status::ErrorInvalidArguemnt;
	}
	auto &desc = _op->getDataIn()[pin];
	if (desc.type != value::VarType::Array) {
		return Status::ErrorInvalidArguemnt;
	}
	auto head = value::chainHead(desc.element);

	// A container element is a blob, and a blob is an address in a store - so an array of arrays or
	// of strings still has to be materialised to be handed out. Only an array of scalars can be
	// read where it lies.
	if (!value::isContainerType(head) && !incoming(pin)) {
		if (auto constant = _graph->getConstant(_node, pin)) {
			if (!constant->isArray()) {
				return Status::ErrorInvalidArguemnt;
			}
			if (index >= constant->size()) {
				return Status::ErrorNotFound;
			}
			// decodeVar is the same decoder blob::decode would have used on the way in, so the
			// value is the one the arena path would have handed back, without the arena.
			return value::decodeVar(constant->getValue(index), head, out);
		}
		return Status::ErrorNotFound;
	}

	auto addr = getInputAddr(pin);
	if (addr == NullAddr) {
		return Status::ErrorNotFound;
	}
	return value::blob::arrayGet(*_local->getArena(), addr, desc.element, index, out);
}

template <typename Graph, typename Local, typename Site>
Addr ContextT<Graph, Local, Site>::getInputAddr(uint32_t pin) const {
	if (pin >= _op->getDataIn().size()) {
		return NullAddr;
	}

	if (auto edge = incoming(pin)) {
		auto shape = Site::template fieldAt<Graph>(*_graph, edge->srcNode, edge->srcPin);
		auto record = _local->getRecord(edge->srcNode, sourceActivation(edge->srcNode));
		if (!shape.valid || record == NullAddr) {
			return NullAddr;
		}
		// The producer's own field, borrowed. Reading a container across an edge allocates nothing
		//; a copy happens only if this node stores it.
		return record + shape.offset;
	}

	if (_graph->getConstant(_node, pin) != nullptr) {
		return materializeConstant(pin);
	}
	return NullAddr;
}

template <typename Graph, typename Local, typename Site>
Addr ContextT<Graph, Local, Site>::materializeConstant(uint32_t pin) const {
	auto &desc = _op->getDataIn()[pin];
	auto constant = _graph->getConstant(_node, pin);
	if (!constant || !value::isContainerType(desc.type)) {
		return NullAddr;
	}

	// MinPayload, not sizeof(BlobHandle): the arena's smallest payload is 16 bytes and it records
	// what it actually gave, so freeing 12 later would trip its own size check.
	auto arena = _local->getArena();
	auto handle = arena->alloc(value::MinPayload, 8);
	if (handle == NullAddr) {
		return NullAddr;
	}
	if (auto raw = arena->write(handle, uint32_t(sizeof(value::BlobHandle)))) {
		value::BlobHandle empty;
		__sprt_memcpy(raw, &empty, sizeof(empty));
	}
	if (value::blob::decode(*arena, handle, desc.type, desc.element, *constant) != Status::Ok) {
		arena->free(handle, value::MinPayload);
		return NullAddr;
	}

	_scratch.emplace_back(Scratch{handle, desc.type, desc.element});
	return handle;
}

template <typename Graph, typename Local, typename Site>
void ContextT<Graph, Local, Site>::releaseScratch() {
	auto arena = _local->getArena();
	for (auto &it : _scratch) {
		if (it.handle == NullAddr) {
			continue;
		}
		value::blob::destroy(*arena, value::blob::getHandle(*arena, it.handle), it.type,
				it.element);
		arena->free(it.handle, value::MinPayload);
	}
	_scratch.clear();
}

template <typename Graph, typename Local, typename Site>
Addr ContextT<Graph, Local, Site>::claimOutput(uint32_t pin) {
	auto shape = Site::template fieldAt<Graph>(*_graph, _node, pin);
	if (!shape.valid || _record == NullAddr) {
		return NullAddr;
	}
	_local->markProduced(_state, pin);
	return _record + shape.offset;
}

template <typename Graph, typename Local, typename Site>
Status ContextT<Graph, Local, Site>::setOutput(uint32_t pin, const Var &value) {
	auto shape = Site::template fieldAt<Graph>(*_graph, _node, pin);
	if (!shape.valid || _record == NullAddr) {
		return Status::ErrorInvalidArguemnt;
	}
	if (value::isContainerType(shape.type)) {
		// Storing somebody else's handle would give one block two owners. Use claimOutput() with
		// the blob accessors, or copyInputToOutput().
		return Status::ErrorInvalidArguemnt;
	}
	auto st = writeRecordField(*_local->getArena(), _record, shape, value);
	if (st != Status::Ok) {
		return st;
	}
	return _local->markProduced(_state, pin);
}

template <typename Graph, typename Local, typename Site>
Status ContextT<Graph, Local, Site>::copyInputToOutput(uint32_t inPin, uint32_t outPin) {
	auto shape = Site::template fieldAt<Graph>(*_graph, _node, outPin);
	if (!shape.valid || _record == NullAddr) {
		return Status::ErrorInvalidArguemnt;
	}

	auto srcAddr = getInputAddr(inPin);
	if (srcAddr == NullAddr) {
		return Status::ErrorNotFound;
	}

	auto arena = _local->getArena();
	auto st = value::blob::copy(*arena, _record + shape.offset, *arena, srcAddr, shape.type,
			shape.element);
	if (st != Status::Ok) {
		return st;
	}
	return _local->markProduced(_state, outPin);
}

// The declared locals sit after the one-field-per-output prefix the record begins with.
inline uint32_t localFieldIndex(const OpDesc &op, uint32_t index) {
	return uint32_t(op.getDataOut().size()) + index;
}

template <typename Graph, typename Local, typename Site>
Status ContextT<Graph, Local, Site>::getLocal(uint32_t index, Var &out) const {
	out = value::makeNil();
	auto shape = Site::template fieldAt<Graph>(*_graph, _node, localFieldIndex(*_op, index));
	if (!shape.valid || _record == NullAddr) {
		return Status::ErrorInvalidArguemnt;
	}
	return readRecordField(*_local->getArena(), _record, shape, out);
}

template <typename Graph, typename Local, typename Site>
Status ContextT<Graph, Local, Site>::setLocal(uint32_t index, const Var &value) {
	auto shape = Site::template fieldAt<Graph>(*_graph, _node, localFieldIndex(*_op, index));
	if (!shape.valid || _record == NullAddr) {
		return Status::ErrorInvalidArguemnt;
	}
	if (value::isContainerType(shape.type)) {
		return Status::ErrorInvalidArguemnt; // one block, one owner - use localAddr and blob::
	}
	return writeRecordField(*_local->getArena(), _record, shape, value);
}

template <typename Graph, typename Local, typename Site>
Addr ContextT<Graph, Local, Site>::localAddr(uint32_t index) const {
	auto shape = Site::template fieldAt<Graph>(*_graph, _node, localFieldIndex(*_op, index));
	return shape.valid && _record != NullAddr ? _record + shape.offset : NullAddr;
}

template <typename Graph, typename Local, typename Site>
Status ContextT<Graph, Local, Site>::fire(uint32_t execOut) {
	if (execOut >= _op->getExecOut().size() || execOut >= MaxExecOut) {
		return Status::ErrorInvalidArguemnt;
	}
	_fired |= uint32_t(1) << execOut;
	return Status::Ok;
}

template <typename Graph, typename Local, typename Site>
Status ContextT<Graph, Local, Site>::fire(StringView name) {
	uint32_t index = 0;
	if (!_op->findExecOut(name, index)) {
		return Status::ErrorNotFound;
	}
	return fire(index);
}

// One place to ask three questions - is there a scene, does it know this type, did the operation
// declare the access - so that six doors cannot each forget a different one.
template <typename Graph, typename Local, typename Site>
const value::ComponentType *ContextT<Graph, Local, Site>::sceneType(TypeId type, bool write) const {
	if (!_scene || !_scene->isValid()) {
		return nullptr;
	}
	if (write ? !_writesScene : !_readsScene) {
		return nullptr;
	}
	auto reg = _scene->getRegistry();
	return reg ? reg->get(type) : nullptr;
}

template <typename Graph, typename Local, typename Site>
value::ExtensionInstance *ContextT<Graph, Local, Site>::getExtension(uint32_t group) const {
	// The same three questions as sceneType, minus the registry: an extension is reached through
	// the scene's store, so the scene's permission is the permission it needs.
	if (!_scene || !_scene->isValid() || (!_readsScene && !_writesScene)) {
		return nullptr;
	}
	if (group >= _extBind.size()) {
		return nullptr;
	}
	return _extBind[group].instance;
}

template <typename Graph, typename Local, typename Site>
value::EntityId ContextT<Graph, Local, Site>::getGlobalEntity() const {
	// Reading it means reading StoreRoot, which is scene state like any other.
	if (!_scene || !_scene->isValid() || !_readsScene) {
		return value::EntityId();
	}
	return _scene->getGlobalEntity();
}

template <typename Graph, typename Local, typename Site>
bool ContextT<Graph, Local, Site>::sceneHas(value::EntityId id, TypeId type) const {
	if (!_scene || !_scene->isValid() || !_readsScene) {
		return false;
	}
	// By TypeId, so that asking about a component whose descriptor the scene never registered is
	// "no" rather than an error - which is what a node watching for an answer that has not arrived
	// needs.
	return _scene->getComponent(id, type) != NullAddr;
}

template <typename Graph, typename Local, typename Site>
Status ContextT<Graph, Local, Site>::sceneGet(value::EntityId id, TypeId type, StringView field,
		Var &out) const {
	out = value::makeNil();
	if (!_scene || !_scene->isValid()) {
		return Status::ErrorInvalidArguemnt;
	}
	if (!_readsScene) {
		return Status::ErrorNotPermitted;
	}
	auto ct = sceneType(type, false);
	if (!ct) {
		return Status::ErrorNotFound;
	}
	auto record = _scene->getComponent(id, *ct);
	if (record == NullAddr) {
		return Status::ErrorNotFound;
	}
	if (auto copy = ownCopy(id, type, field)) {
		bool written = false;
		auto st = readCopy(*copy, written, out);
		if (st != Status::Ok || written) {
			return st;
		}
	}
	SP_FLOW_VALUE_COUNT(getFieldScene);
	return ct->getField(*_scene->getArena(), record, field, out);
}

template <typename Graph, typename Local, typename Site>
Status ContextT<Graph, Local, Site>::sceneFind(TypeId type, StringView field, const Var &value,
		value::EntityId &out) const {
	out = value::EntityId();
	if (!_scene || !_scene->isValid()) {
		return Status::ErrorInvalidArguemnt;
	}
	if (!_readsScene) {
		return Status::ErrorNotPermitted;
	}
	auto ct = sceneType(type, false);
	if (!ct) {
		return Status::ErrorNotFound;
	}
	auto desc = ct->getField(field);
	if (!desc) {
		return Status::ErrorNotFound;
	}
	// A field whose type is not the one being compared is a mistake in the graph, not a miss.
	// Letting it read as "no match" would make a misspelled comparison look like an empty scene.
	if (desc->type != value.type) {
		return Status::ErrorInvalidArguemnt;
	}

	// Row order, and the first match wins. Row order is a function of the pool's edit history
	// (canonicalised to entity-index order by a load), so a graph that relies on which of several
	// matches it gets is relying on history. Whoever writes the field is responsible for its being
	// unique; this returns an answer, not a set.
	auto &arena = *_scene->getArena();
	auto found = value::EntityId();

	// An integral field is searched by its stored bytes, which lets the store do the whole walk
	// with no callback: no Var built per row, no owner resolved per row, no per-row guard against a
	// mutation a search cannot perform. The gate is by type and not by size, and each exclusion has
	// a reason. Float and the vectors are out because bytes are not equality there: -0.0 and +0.0
	// are equal numbers with different bytes, and NaN is unequal to itself. Enum and EntityRef are
	// out because their identity includes a subtype that lives in the descriptor, not in the eight
	// bytes stored - two references to different schemas can hold the same packed id.
	if (desc->type == value::VarType::Int || desc->type == value::VarType::Bool) {
		uint8_t bytes[8] = {};
		uint32_t size = desc->size;
		if (desc->type == value::VarType::Bool) {
			bytes[0] = value.i != 0 ? 1 : 0;
			size = 1;
		} else {
			auto v = value.i;
			__sprt_memcpy(bytes, &v, sizeof(v));
		}
		found = _scene->findByField(*ct, desc->offset, BytesView(bytes, size));
	} else {
		// The same call, counted the same way: what this answers is whether the graph asked for a
		// search, not how the search was carried out.
		SP_FLOW_VALUE_COUNT(sceneFind);
		// readEachRow, not forEachRow: this callback reads a field and compares it, and a search is
		// exactly the caller that can promise as much. It buys back the per-row mutation guard -
		// two arena reads a row that exist to catch a caller doing what this one cannot.
		_scene->readEachRow(type, [&](value::EntityId owner, Addr record) {
			Var candidate;
			SP_FLOW_VALUE_COUNT(getFieldScene);
			if (ct->getField(arena, record, *desc, candidate) == Status::Ok && candidate == value) {
				found = owner;
				return false;
			}
			return true;
		});
	}

	out = found;
	return found.index != 0 ? Status::Ok : Status::ErrorNotFound;
}

template <typename Graph, typename Local, typename Site>
Status ContextT<Graph, Local, Site>::sceneSet(value::EntityId id, TypeId type, StringView field,
		const Var &value) {
	if (!_scene || !_scene->isValid()) {
		return Status::ErrorInvalidArguemnt;
	}
	if (!_writesScene) {
		return Status::ErrorNotPermitted;
	}
	auto ct = sceneType(type, true);
	if (!ct) {
		return Status::ErrorNotFound;
	}
	auto record = _scene->getComponent(id, *ct);
	if (record == NullAddr) {
		return Status::ErrorNotFound;
	}
	if (auto copy = ownCopy(id, type, field)) {
		return writeCopy(*copy, value);
	}
	// Straight through the scene arena's own write barrier, so the change is in the same version as
	// everything this step did to the local store.
	SP_FLOW_VALUE_COUNT(setFieldScene);
	return ct->setField(*_scene->getArena(), record, field, value);
}

template <typename Graph, typename Local, typename Site>
Status ContextT<Graph, Local, Site>::sceneAdd(value::EntityId id, TypeId type) {
	if (!_scene || !_scene->isValid()) {
		return Status::ErrorInvalidArguemnt;
	}
	if (!_writesScene) {
		return Status::ErrorNotPermitted;
	}
	auto ct = sceneType(type, true);
	if (!ct) {
		return Status::ErrorNotFound;
	}
	return _scene->addComponent(id, *ct) != NullAddr ? Status::Ok : Status::ErrorInvalidArguemnt;
}

template <typename Graph, typename Local, typename Site>
Status ContextT<Graph, Local, Site>::sceneRemove(value::EntityId id, TypeId type) {
	if (!_scene || !_scene->isValid()) {
		return Status::ErrorInvalidArguemnt;
	}
	if (!_writesScene) {
		return Status::ErrorNotPermitted;
	}
	auto ct = sceneType(type, true);
	if (!ct) {
		return Status::ErrorNotFound;
	}
	return _scene->removeComponent(id, *ct);
}

// The bound doors: the same six questions with the type and the field already decided. What is gone
// from every one of them is ComponentRegistry::get walking its whole list, ComponentType::getField
// walking its fields comparing strings, and makeTypeId re-hashing a literal that has not changed
// since the graph was written - the build answered all three. A binding with no `type` is not an
// error here: for an optional group it is the answer, and the scene has no such component.

template <typename Graph, typename Local, typename Site>
bool ContextT<Graph, Local, Site>::sceneHas(value::EntityId id, const SceneBinding &binding) const {
	if (!binding.type || !_scene || !_scene->isValid() || !_readsScene) {
		return false;
	}
	return _scene->getComponent(id, *binding.type) != NullAddr;
}

template <typename Graph, typename Local, typename Site>
Status ContextT<Graph, Local, Site>::sceneGet(value::EntityId id, const SceneBinding &binding,
		Var &out) const {
	out = value::makeNil();
	if (!_scene || !_scene->isValid()) {
		return Status::ErrorInvalidArguemnt;
	}
	if (!_readsScene) {
		return Status::ErrorNotPermitted;
	}
	if (!binding.type || !binding.desc) {
		return Status::ErrorNotFound;
	}
	auto record = _scene->getComponent(id, *binding.type);
	if (record == NullAddr) {
		return Status::ErrorNotFound;
	}
	if (auto copy = ownCopy(id, binding.componentId, binding.field)) {
		bool written = false;
		auto st = readCopy(*copy, written, out);
		if (st != Status::Ok || written) {
			return st;
		}
	}
	SP_FLOW_VALUE_COUNT(getFieldScene);
	return binding.type->getField(*_scene->getArena(), record, *binding.desc, out);
}

template <typename Graph, typename Local, typename Site>
Status ContextT<Graph, Local, Site>::sceneSet(value::EntityId id, const SceneBinding &binding,
		const Var &value) {
	if (!_scene || !_scene->isValid()) {
		return Status::ErrorInvalidArguemnt;
	}
	if (!_writesScene) {
		return Status::ErrorNotPermitted;
	}
	if (!binding.type || !binding.desc) {
		return Status::ErrorNotFound;
	}
	auto record = _scene->getComponent(id, *binding.type);
	if (record == NullAddr) {
		return Status::ErrorNotFound;
	}
	if (auto copy = ownCopy(id, binding.componentId, binding.field)) {
		return writeCopy(*copy, value);
	}
	SP_FLOW_VALUE_COUNT(setFieldScene);
	return binding.type->setField(*_scene->getArena(), record, *binding.desc, value);
}

template <typename Graph, typename Local, typename Site>
Status ContextT<Graph, Local, Site>::sceneFind(const SceneBinding &binding, const Var &value,
		value::EntityId &out) const {
	out = value::EntityId();
	if (!_scene || !_scene->isValid()) {
		return Status::ErrorInvalidArguemnt;
	}
	if (!_readsScene) {
		return Status::ErrorNotPermitted;
	}
	if (!binding.type || !binding.desc) {
		return Status::ErrorNotFound;
	}
	auto desc = binding.desc;
	// Still checked, and it still has to be: the bind proved the field is the type the value pin
	// declares, and this is the value that actually arrived on it - which an edge may have widened.
	if (desc->type != value.type) {
		return Status::ErrorInvalidArguemnt;
	}

	auto found = value::EntityId();
	if (desc->type == value::VarType::Int || desc->type == value::VarType::Bool) {
		uint8_t bytes[8] = {};
		uint32_t size = desc->size;
		if (desc->type == value::VarType::Bool) {
			bytes[0] = value.i != 0 ? 1 : 0;
			size = 1;
		} else {
			auto v = value.i;
			__sprt_memcpy(bytes, &v, sizeof(v));
		}
		found = _scene->findByField(*binding.type, desc->offset, BytesView(bytes, size));
	} else {
		SP_FLOW_VALUE_COUNT(sceneFind);
		// By descriptor rather than by TypeId, so the walk does not pay a registry scan of its own
		// for a type this call already holds - and read-only, for the reason given at the twin
		// above.
		auto &arena = *_scene->getArena();
		auto ct = binding.type;
		_scene->readEachRow(*ct, [&](value::EntityId owner, Addr record) {
			Var candidate;
			SP_FLOW_VALUE_COUNT(getFieldScene);
			if (ct->getField(arena, record, *desc, candidate) == Status::Ok && candidate == value) {
				found = owner;
				return false;
			}
			return true;
		});
	}

	out = found;
	return found.index != 0 ? Status::Ok : Status::ErrorNotFound;
}

template <typename Graph, typename Local, typename Site>
Status ContextT<Graph, Local, Site>::sceneAdd(value::EntityId id, const SceneBinding &binding) {
	if (!_scene || !_scene->isValid()) {
		return Status::ErrorInvalidArguemnt;
	}
	if (!_writesScene) {
		return Status::ErrorNotPermitted;
	}
	if (!binding.type) {
		return Status::ErrorNotFound;
	}
	return _scene->addComponent(id, *binding.type) != NullAddr ? Status::Ok
															   : Status::ErrorInvalidArguemnt;
}

template <typename Graph, typename Local, typename Site>
Status ContextT<Graph, Local, Site>::sceneRemove(value::EntityId id, const SceneBinding &binding) {
	if (!_scene || !_scene->isValid()) {
		return Status::ErrorInvalidArguemnt;
	}
	if (!_writesScene) {
		return Status::ErrorNotPermitted;
	}
	// Nothing registered means nothing to remove, and removing what is not there is done. Zero
	// work, where the unbound path would scan the registry only to be told the same thing.
	if (!binding.type) {
		return Status::Ok;
	}
	return _scene->removeComponent(id, *binding.type);
}

// Parallel blocks.

template <typename Graph, typename Local, typename Site>
const RuntimeBlockWrite *ContextT<Graph, Local, Site>::ownCopy(value::EntityId id, TypeId component,
		StringView field) const {
	if (_block == InvalidIndex || _branchFrame == NullAddr || id != _branchEntity) {
		return nullptr;
	}
	for (auto &write : _graph->getBlockWrites(_block)) {
		if (write.componentId == component && write.field == field) {
			return &write;
		}
	}
	return nullptr;
}

template <typename Graph, typename Local, typename Site>
Status ContextT<Graph, Local, Site>::readCopy(const RuntimeBlockWrite &write, bool &written,
		Var &out) const {
	written = readBranchFlag(*_local->getArena(), _branchFrame, write.flagOffset);
	if (!written) {
		return Status::Ok;
	}
	return readRecordField(*_local->getArena(), _branchFrame, shapeOfCopy(write), out);
}

template <typename Graph, typename Local, typename Site>
Status ContextT<Graph, Local, Site>::writeCopy(const RuntimeBlockWrite &write, const Var &value) {
	auto st = writeRecordField(*_local->getArena(), _branchFrame, shapeOfCopy(write), value);
	if (st != Status::Ok) {
		return st;
	}
	return writeBranchFlag(*_local->getArena(), _branchFrame, write.flagOffset, true);
}

template <typename Graph, typename Local, typename Site>
Status ContextT<Graph, Local, Site>::queryEntities(uint32_t local) {
	if (!_scene || !_scene->isValid()) {
		return Status::ErrorInvalidArguemnt;
	}
	if (!_readsScene) {
		return Status::ErrorNotPermitted;
	}
	auto &node = _graph->getNodeAt(_node);
	if (node.opensScope == InvalidIndex) {
		return Status::ErrorInvalidArguemnt;
	}
	auto blockIndex = _graph->getScopeAt(node.opensScope).block;
	auto addr = localAddr(local);
	auto shape = Site::template fieldAt<Graph>(*_graph, _node, localFieldIndex(*_op, local));
	if (blockIndex == InvalidIndex || addr == NullAddr || shape.type != VarType::Array) {
		return Status::ErrorInvalidArguemnt;
	}
	auto with = _graph->getBlockQuery(blockIndex, false);
	auto without = _graph->getBlockQuery(blockIndex, true);

	// Collected first and appended after: the walk reads the scene's pools and must not interleave
	// with allocations, even in another arena.
	mem_std::Vector<value::EntityId> found;
	_scene->query(with, [&](value::EntityId id) {
		for (auto type : without) {
			if (_scene->getComponent(id, type) != NullAddr) {
				return true;
			}
		}
		found.emplace_back(id);
		return true;
	});

	auto arena = _local->getArena();
	auto st = value::blob::arrayResize(*arena, addr, shape.element, 0);
	if (st != Status::Ok) {
		return st;
	}
	for (auto &id : found) {
		st = value::blob::arrayPush(*arena, localAddr(local), shape.element,
				value::makeEntityRef(id));
		if (st != Status::Ok) {
			return st;
		}
	}
	return Status::Ok;
}

template <typename Graph, typename Local, typename Site>
bool ContextT<Graph, Local, Site>::branchRange(uint32_t blockIndex, BranchRange &out) const {
	if (blockIndex == InvalidIndex || blockIndex >= _graph->getBlockCount()) {
		return false;
	}
	auto &block = _graph->getBlockAt(blockIndex);
	auto &fanOut = _graph->getNodeAt(block.fanOut);
	out.block = blockIndex;
	out.fanOutActivation = _local->resolveScope(_activation, fanOut.scope);
	auto record = _local->getRecord(block.fanOut, out.fanOutActivation);
	if (out.fanOutActivation == NullActivation || record == NullAddr) {
		return false;
	}
	auto arena = _local->getArena();
	Var first;
	auto firstShape = fanOutLocalShape(fanOut, FanOutLocal::First);
	auto entitiesShape = fanOutLocalShape(fanOut, FanOutLocal::Entities);
	if (!firstShape.valid || !entitiesShape.valid
			|| readRecordField(*arena, record, firstShape, first) != Status::Ok) {
		return false;
	}
	out.first = uint32_t(first.i);
	out.count =
			value::blob::arrayCount(*arena, record + entitiesShape.offset, entitiesShape.element);
	return true;
}

template <typename Graph, typename Local, typename Site>
const typename Graph::DataEdge *ContextT<Graph, Local, Site>::branchEdge(uint32_t pin,
		BranchRange &out) const {
	if (pin != _op->getBranchPin()) {
		return nullptr;
	}
	auto edge = incoming(pin);
	if (!edge) {
		return nullptr;
	}
	auto &src = _graph->getNodeAt(edge->srcNode);
	uint32_t scope = src.opensScope != InvalidIndex ? src.opensScope
												   : _graph->getScopeAt(src.scope).branchScope;
	if (scope == InvalidIndex || !branchRange(_graph->getScopeAt(scope).block, out)) {
		return nullptr;
	}
	return edge;
}

template <typename Graph, typename Local, typename Site>
Status ContextT<Graph, Local, Site>::getBranchCount(uint32_t pin, uint32_t &out) const {
	out = 0;
	BranchRange range;
	if (!branchEdge(pin, range)) {
		return Status::ErrorNotFound;
	}
	out = range.count;
	return Status::Ok;
}

template <typename Graph, typename Local, typename Site>
bool ContextT<Graph, Local, Site>::isBranchPresent(uint32_t pin, uint32_t branch) const {
	BranchRange range;
	auto edge = branchEdge(pin, range);
	if (!edge || branch >= range.count) {
		return false;
	}
	auto &block = _graph->getBlockAt(range.block);
	if (block.onFailure == ParallelFailure::Nothing) {
		return false;
	}
	auto act = range.first + branch;
	auto frame = _local->activationFrame(act);
	if (frame == NullAddr
			|| (readBranchInt(*_local->getArena(), frame, BranchHeader::StatusOffset)
					   & BranchHeader::Failed)
					!= 0) {
		return false;
	}
	if (edge->srcNode == block.fanOut) {
		return true;
	}
	auto state = _local->readState(_local->getStateIn(frame, edge->srcNode));
	return (state.produced & (uint32_t(1) << edge->srcPin)) != 0;
}

template <typename Graph, typename Local, typename Site>
Status ContextT<Graph, Local, Site>::getBranchValue(uint32_t pin, uint32_t branch, Var &out) const {
	out = value::makeNil();
	if (!isBranchPresent(pin, branch)) {
		return Status::ErrorNotFound;
	}
	BranchRange range;
	auto edge = branchEdge(pin, range);
	auto &block = _graph->getBlockAt(range.block);
	auto act = range.first + branch;
	Var raw;
	if (edge->srcNode == block.fanOut) {
		if (edge->srcPin == block.entityPin) {
			raw = value::makeEntityRef(
					readBranchEntity(*_local->getArena(), _local->activationFrame(act)));
		} else if (edge->srcPin == block.indexPin) {
			raw = value::makeInt32(int32_t(branch));
		} else {
			return Status::ErrorNotFound;
		}
	} else {
		auto shape = Site::template fieldAt<Graph>(*_graph, edge->srcNode, edge->srcPin);
		auto record = _local->getRecord(edge->srcNode, act);
		if (!shape.valid || record == NullAddr) {
			return Status::ErrorNotFound;
		}
		auto st = readRecordField(*_local->getArena(), record, shape, raw);
		if (st != Status::Ok) {
			return st;
		}
	}
	if (edge->cast == value::CastRule::Same) {
		out = raw;
		return Status::Ok;
	}
	return value::castVar(raw, _op->getDataIn()[pin].type, value::CastPolicy::Lossless, out);
}

// The device folded this collector at delivery and left the value in its record. Taking it clears
// the flag, so a collector that runs again folds on the CPU rather than repeating a fold of
// branches that have since moved.
template <typename Graph, typename Local, typename Site>
Status ContextT<Graph, Local, Site>::takeBranchFold(uint32_t, Var &out) {
	out = value::makeNil();
	if (_state == NullAddr) {
		return Status::ErrorNotFound;
	}
	auto state = _local->readState(_state);
	if ((state.flags & NodeFlags::Folded) == 0) {
		return Status::ErrorNotFound;
	}
	_local->removeFlags(_state, NodeFlags::Folded);
	// A collector has one output, and a record's fields are indexed by output pin.
	auto shape = Site::template fieldAt<Graph>(*_graph, _node, 0);
	if (!shape.valid || _record == NullAddr) {
		return Status::ErrorNotFound;
	}
	return readRecordField(*_local->getArena(), _record, shape, out);
}

template <typename Graph, typename Local, typename Site>
Status ContextT<Graph, Local, Site>::getBarrierCount(uint32_t &out) const {
	out = 0;
	for (uint32_t b = 0; b < _graph->getBlockCount(); ++b) {
		if (_graph->getBlockAt(b).barrier == _node) {
			BranchRange range;
			if (!branchRange(b, range)) {
				return Status::ErrorNotFound;
			}
			out = range.count;
			return Status::Ok;
		}
	}
	return Status::ErrorNotFound;
}

template <typename Graph, typename Local, typename Site>
Status ContextT<Graph, Local, Site>::getBarrierBranch(uint32_t branch, bool &present,
		bool &failed) const {
	present = false;
	failed = false;
	for (uint32_t b = 0; b < _graph->getBlockCount(); ++b) {
		auto &block = _graph->getBlockAt(b);
		if (block.barrier != _node) {
			continue;
		}
		BranchRange range;
		if (!branchRange(b, range) || branch >= range.count) {
			return Status::ErrorNotFound;
		}
		auto frame = _local->activationFrame(range.first + branch);
		auto status = readBranchInt(*_local->getArena(), frame, BranchHeader::StatusOffset);
		failed = (status & BranchHeader::Failed) != 0;
		present = !failed && block.onFailure != ParallelFailure::Nothing;
		return Status::Ok;
	}
	return Status::ErrorNotFound;
}

} // namespace stappler::flow

#endif /* STAPPLER_FLOW_SPFLOWCONTEXT_HPP_ */
