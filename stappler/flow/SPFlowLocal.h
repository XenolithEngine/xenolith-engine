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

#ifndef STAPPLER_FLOW_SPFLOWLOCAL_H_
#define STAPPLER_FLOW_SPFLOWLOCAL_H_

#include "SPFlowRuntime.h"
#include "SPFlowEnv.h"

#include <sprt/c/__sprt_string.h>

// The local store of one run. After any step, the complete state of the run is the bytes of
// this store. The ready front lives here and not in a host vector - a snapshot without the front
// can only be replayed from the beginning, never resumed, and a few blob operations per step buy
// that. The one thing deliberately not here is the execution log: it is a diagnostic, no operation
// reads it and no rollback has to erase it, so it stays on the host.
namespace STAPPLER_VERSIONIZED stappler::flow {

// A record is addressed by (node, activation). This packing is the currency of the ready front, the
// stalled list and an activation's opener - a value naming a record, passed around and compared. It
// is not how a record is found or stored: an activation owns a frame, one arena block, and a node's
// state and record are at offsets inside it that the layout below computes once. Finding a record
// is an array read and an addition; there are no entities, no component pools and no sparse sets in
// a run at all.
constexpr uint64_t makeRecordKey(uint32_t node, uint32_t activation) {
	return (uint64_t(node) << 32) | uint64_t(activation);
}

constexpr uint32_t recordKeyNode(uint64_t key) { return uint32_t(key >> 32); }
constexpr uint32_t recordKeyActivation(uint64_t key) { return uint32_t(key); }

// "No record". Not zero: (node 0, activation 0) is a perfectly good key, and the first node of
// every graph has it.
static constexpr uint64_t NullRecordKey = ~uint64_t(0);

// The activation every node of the top-level scope belongs to. Loop iterations allocate the rest.
static constexpr uint32_t RootActivation = 0;

// "No activation" - the root's parent, and the answer when a scope is asked for from somewhere it
// does not enclose.
static constexpr uint32_t NullActivation = 0xffff'ffffu;

// One turn of one loop: which body it is a turn of, which activation it hangs under, which turn it
// is, who opened it, and where its records live. The tree of these is the loop nesting at run time,
// and it is a tree of data in the arena rather than a stack of C++ frames.
struct ActivationData {
	uint32_t scope = 0;
	uint32_t parent = NullActivation;
	uint32_t iteration = 0;
	uint64_t openerKey = NullRecordKey; // packed (node, activation) of the node that opened it
	Addr frame = NullAddr; // the arena block holding this turn's records; 0 until materialized
};

// The five Ints one activation occupies inside `interp.Run.activations`, and the order they lie in.
// Interleaved rather than kept as five parallel arrays, so all five arrive in one arena read of
// forty contiguous bytes - the same trick readState plays on NodeState. The order is not arbitrary
// either: `scope` and `frame` are the two frameFor wants, and they are the first and the last of
// the five, so the read that gets one gets the other.
static constexpr uint32_t ActScope = 0;
static constexpr uint32_t ActParent = 1;
static constexpr uint32_t ActIteration = 2;
static constexpr uint32_t ActOpener = 3;
static constexpr uint32_t ActFrame = 4;
static constexpr uint32_t ActivationStride = 5;

// Bits of interp.NodeState.flags.
struct NodeFlags {
	static constexpr int64_t Token = 1 << 0; // an exec token has arrived
	static constexpr int64_t Ran = 1 << 1;
	static constexpr int64_t StallData = 1 << 2; // waiting for a value on stallPin
	static constexpr int64_t StallExec = 1 << 3; // waiting for a token that may never come

	// Already on the front. A node becomes runnable when its last input arrives or when its token
	// does, and without this bit whichever happened second would queue it a second time.
	static constexpr int64_t Queued = 1 << 4;

	// A barrier whose block is in flight: waiting for a delivery, not for data or a token, and not
	// a deadlock.
	static constexpr int64_t StallExternal = 1 << 5;

	// A collector whose fold the device already did. Its output is in its record, and the operation
	// takes it instead of walking the branches. Set at delivery, taken by the step that follows it,
	// so it never outlives a run.
	static constexpr int64_t Folded = 1 << 6;
};

// interp.NodeState, read in one go. Written field by field, because a step usually changes one.
struct NodeStateData {
	uint32_t inputs = 0; // bit per data input that has a value
	uint32_t produced = 0; // bit per data output that was written
	int64_t flags = 0;
	uint32_t stallPin = 0;

	// Turns of a loop this record has opened; zero for every node that opens none.
	uint32_t turns = 0;
};

// Where a node's two records sit inside its scope's frame: the offset of its interp.NodeState and
// the offset of its own record, or InvalidIndex when the operation declares none.
/* The shape of one record field - the fourth question a site answers. `incoming`, `recordIn` and
`stateIn` say where a node's rows are; this says what a field of the record is: offset, size, type,
element and subtype, everything `value::decodeField` needs and nothing else. The interpreter reads
it off the schema the registry resolved; a generated unit reads it out of a constant table, and then
the codec's switch folds to the one case that field can be. A value and not a `const FieldDesc *` on
purpose: a descriptor is a runtime object with a name, an alias, a default and a meta block, and a
pointer to one cannot be a constant. Five numbers can. */
struct FieldShape {
	uint32_t offset = 0;
	uint32_t size = 0;
	value::VarType type = value::VarType::Nil;
	value::ElementChain element = 0;
	value::TypeId subtypeId = value::NullTypeId;
	bool valid = false;
};

// The derived local schema's first N fields are the N data outputs, in order, and the locals follow
// them - so a pin index is a field index, and a local's index is its own plus the number of
// outputs. Both sites agree on that arithmetic; only where they read it differs.
inline FieldShape shapeOfField(const value::FieldDesc &f) {
	return FieldShape{f.offset, f.size, f.type, f.element, f.subtypeId, true};
}

struct FrameSlot {
	uint32_t stateOffset = 0;
	uint32_t recordOffset = InvalidIndex;
};

/* The frame layout, as a function of the graph and the two schemas and of nothing else. A stack
frame, laid out by the same rules a component record is laid out by, and for the same reason: the
set of nodes in a scope is a build-time fact, so where each one's bookkeeping sits inside a
turn is decidable before the run starts. A node's state and its record are adjacent, state first,
because a step that reads a node's flags almost always reads its record in the same breath.
Alignment is computed here and not taken from the host compiler, exactly as ComponentType::build
does it, because a frame's bytes are image bytes and must be identical on every target. A free
function rather than a private step of the store, because three things have to say the same numbers:
the store when it attaches, the generator when it writes a unit, and the unit when
it is loaded and checks that the layout it carries is the layout this process would compute. */
template <typename Graph>
inline Status computeFrameLayout(const Graph &graph, const value::ComponentType &stateType,
		mem_std::Vector<FrameSlot> &slots, mem_std::Vector<uint32_t> &frameBytes) {
	slots.clear();
	frameBytes.clear();
	slots.resize(graph.getNodeCount());
	frameBytes.resize(graph.getScopeCount(), 0);

	auto stateSize = stateType.getSize();
	auto stateAlign = stateType.getAlign();

	for (uint32_t s = 0; s < graph.getScopeCount(); ++s) {
		// A branch frame starts with its header (BranchHeader); every other scope has none.
		uint32_t cursor = graph.getScopeAt(s).headerBytes;
		uint32_t frameAlign = cursor > 0 ? sprt::max(stateAlign, uint32_t(8)) : stateAlign;

		for (auto n : graph.getScopeNodes(s)) {
			auto &rt = graph.getNodeAt(n);
			auto &slot = slots[n];

			slot.stateOffset = value::alignUp(cursor, stateAlign);
			cursor = slot.stateOffset + stateSize;

			if (rt.localSchema) {
				auto align = rt.localSchema->getAlign();
				slot.recordOffset = value::alignUp(cursor, align);
				cursor = slot.recordOffset + rt.localSchema->getSize();
				if (align > frameAlign) {
					frameAlign = align;
				}
			} else {
				slot.recordOffset = InvalidIndex;
			}
		}

		// Rounded up to the frame's own alignment, so that the size is a property of the scope and
		// not of what happened to be laid out last.
		frameBytes[s] = value::alignUp(cursor, frameAlign);
	}
	return Status::Ok;
}

// The branch header at the start of every frame of a parallel scope (BranchHeader). Raw bytes
// through the arena's write barrier, so a version covers them with everything else the unit did.

template <typename A>
inline int32_t readBranchInt(const A &arena, Addr frame, uint32_t offset) {
	int32_t value = 0;
	if (auto src = arena.read(frame + offset, sizeof(value))) {
		__sprt_memcpy(&value, src, sizeof(value));
	}
	return value;
}

template <typename A>
inline Status writeBranchInt(A &arena, Addr frame, uint32_t offset, int32_t value) {
	auto dst = arena.write(frame + offset, sizeof(value));
	if (!dst) {
		return Status::ErrorInvalidArguemnt;
	}
	__sprt_memcpy(dst, &value, sizeof(value));
	return Status::Ok;
}

template <typename A>
inline value::EntityId readBranchEntity(const A &arena, Addr frame) {
	uint64_t packed = 0;
	if (auto src = arena.read(frame + BranchHeader::EntityOffset, sizeof(packed))) {
		__sprt_memcpy(&packed, src, sizeof(packed));
	}
	return value::EntityId::unpack(packed);
}

template <typename A>
inline Status writeBranchEntity(A &arena, Addr frame, value::EntityId id) {
	auto dst = arena.write(frame + BranchHeader::EntityOffset, sizeof(uint64_t));
	if (!dst) {
		return Status::ErrorInvalidArguemnt;
	}
	auto packed = id.pack();
	__sprt_memcpy(dst, &packed, sizeof(packed));
	return Status::Ok;
}

template <typename A>
inline bool readBranchFlag(const A &arena, Addr frame, uint32_t offset) {
	auto src = arena.read(frame + offset, 1);
	return src && *reinterpret_cast<const uint8_t *>(src) != 0;
}

template <typename A>
inline Status writeBranchFlag(A &arena, Addr frame, uint32_t offset, bool value) {
	auto dst = arena.write(frame + offset, 1);
	if (!dst) {
		return Status::ErrorInvalidArguemnt;
	}
	*reinterpret_cast<uint8_t *>(dst) = value ? 1 : 0;
	return Status::Ok;
}

// The shape of a block write's copy inside the header.
inline FieldShape shapeOfCopy(const RuntimeBlockWrite &write) {
	return FieldShape{write.valueOffset, value::getTypeSize(write.type), write.type, 0,
		write.subtypeId, true};
}

// A fan-out's local field `index` (FanOutLocal), off its derived schema.
inline FieldShape fanOutLocalShape(const RuntimeNode &node, uint32_t index) {
	if (!node.op || !node.localSchema) {
		return FieldShape();
	}
	auto fields = node.localSchema->getFields();
	auto field = node.op->getDataOut().size() + index;
	return field < fields.size() ? shapeOfField(fields[field]) : FieldShape();
}

// The two types a run's store is described by. Named here rather than inside one store's bodies
// because both stores resolve them out of the same registry, and a second spelling of a name is how
// two callers come to look for different types.
static constexpr StringView RunTypeName("interp.Run");

// The front's upper level: one Array<Int> per level above 0. A second component of the run's root,
// added only for a graph that has a node of weight above 0, so a graph without weights carries no
// such component in its image.
static constexpr StringView RunLevelsTypeName("interp.RunLevels");

// The level of the front a node is pushed on: its weight, or 0 when weights are off.
template <typename Graph>
inline uint32_t frontLevel(const Graph &graph, uint32_t node, bool weighted) {
	return weighted && node < graph.getNodeCount() ? graph.getNodeAt(node).weight : 0;
}

template <typename Graph>
inline bool graphHasWeights(const Graph &graph) {
	for (uint32_t n = 0; n < graph.getNodeCount(); ++n) {
		if (graph.getNodeAt(n).weight != 0) {
			return true;
		}
	}
	return false;
}
static constexpr StringView NodeStateTypeName("interp.NodeState");

// What every local store does the same way. There are two: the arena store below, whose whole state
// is the bytes of an arena, and the fast store (SPFlowFast.h), whose bookkeeping is host memory.
// They differ in where the run's own state lives and in nothing else - a frame is laid out by the
// same function, initialised by the same rules, read through the same descriptors, and described in
// the same words. So those rules are here, once, as free templates over "a store": a second copy of
// any of them would agree on the day it was written and on no day after, and what they
// encode is not arithmetic but semantics - which bytes a fresh frame holds, what a record owns when
// it dies, how a mask is set, and what a paused run looks like to a debugger.

// The five fields of interp.NodeState, resolved once from the descriptor rather than assumed from
// declaration order. All five are Ints, and a store that finds otherwise refuses the type.
struct NodeStateFields {
	const value::FieldDesc *inputs = nullptr;
	const value::FieldDesc *produced = nullptr;
	const value::FieldDesc *flags = nullptr;
	const value::FieldDesc *stallPin = nullptr;
	const value::FieldDesc *turns = nullptr;

	bool isValid() const { return inputs && produced && flags && stallPin && turns; }
};

// Resolves them, and checks the one thing the readers below assume: that every one of them is an
// Int. Nothing here assumes an order - the offsets come from the schema.
SP_PUBLIC Status resolveNodeStateFields(const value::ComponentType &, NodeStateFields &out);

// One read of the whole record, not one per field: the five fields are five Ints inside forty
// contiguous bytes, a single cache line, and they arrive together anyway.
template <typename A>
inline NodeStateData readNodeState(const A &arena, const value::ComponentType &type,
		const NodeStateFields &fields, Addr stateAddr) {
	NodeStateData out;
	if (stateAddr == NullAddr) {
		return out;
	}
	auto src = arena.read(stateAddr, type.getSize());
	if (!src) {
		return out;
	}
	auto intAt = [src](const value::FieldDesc *field) {
		int64_t v = 0;
		__sprt_memcpy(&v, src + field->offset, sizeof(v));
		return v;
	};

	out.inputs = uint32_t(intAt(fields.inputs));
	out.produced = uint32_t(intAt(fields.produced));
	out.flags = intAt(fields.flags);
	out.stallPin = uint32_t(intAt(fields.stallPin));
	out.turns = uint32_t(intAt(fields.turns));
	return out;
}

// The remaining descriptor path onto interp.NodeState: a field written whole, with no read to fuse
// into it - the stall pin and the turn count. Kept behind one name for the counter's sake, and
// because there is exactly one article here (value::Counters).
template <typename A>
inline Status writeStateField(const value::ComponentType &type, A &arena, Addr stateAddr,
		const value::FieldDesc &field, const value::Var &value) {
	SP_FLOW_VALUE_COUNT(setFieldState);
	return type.setField(arena, stateAddr, field, value);
}

// The one place a mask changes, as one barrier rather than a read and a write: an arena.write()
// over field.size bytes at field.offset, which is the extent the schema states and not
// sizeof(int64_t). write() is what announces a change (SPVStoreArena.h in stappler_vstore) and read() announces
// nothing, so a mask set here marks exactly the bytes a descriptor path marked. Int-ness is a
// precondition the store settles once when it resolves the five fields, and it is refused here as
// well rather than assumed. Two masks are set and cleared and never both at once, but one core
// takes both, so the rule that matters - which bytes are announced, and that the read and the write
// are the same resolution - is written once.
template <typename A>
inline Status updateStateBits(A &arena, Addr stateAddr, const value::FieldDesc &field,
		int64_t setBits, int64_t clearBits) {
	SP_FLOW_VALUE_COUNT(stateRmw);
	if (stateAddr == NullAddr || field.type != value::VarType::Int
			|| field.size != sizeof(int64_t)) {
		return Status::ErrorInvalidArguemnt;
	}
	auto dst = arena.write(stateAddr + field.offset, field.size);
	if (!dst) {
		return Status::ErrorInvalidArguemnt;
	}
	int64_t v = 0;
	__sprt_memcpy(&v, dst, sizeof(v));
	v = (v | setBits) & ~clearBits;
	__sprt_memcpy(dst, &v, sizeof(v));
	return Status::Ok;
}

template <typename A>
inline Status setStateBits(A &arena, Addr stateAddr, const value::FieldDesc &field, int64_t bits) {
	return updateStateBits(arena, stateAddr, field, bits, 0);
}

template <typename A>
inline Status clearStateBits(A &arena, Addr stateAddr, const value::FieldDesc &field,
		int64_t bits) {
	return updateStateBits(arena, stateAddr, field, 0, bits);
}

// A fresh frame: the arena hands back dirty bytes, so the whole block is zeroed - which is what
// makes the padding between records zero too, and "two runs give byte-identical images" a test -
// and then every record takes its defaults through initInstance. initInstance zeroes again, which
// is redundant and cheap: the pages are already dirty in this version, so a journal charges a bit
// test for the second announcement.
template <typename A, typename Graph>
inline Status initFrame(A &arena, const Graph &graph, SpanView<FrameSlot> slots,
		const value::ComponentType &stateType, uint32_t scope, Addr frame, uint32_t bytes) {
	if (auto dst = arena.write(frame, bytes)) {
		__sprt_memset(dst, 0, bytes);
	}
	for (auto n : graph.getScopeNodes(scope)) {
		auto &rt = graph.getNodeAt(n);
		auto &slot = slots[n];
		if (stateType.initInstance(arena, frame + slot.stateOffset) != Status::Ok) {
			return Status::ErrorOutOfHostMemory;
		}
		if (rt.localSchema && slot.recordOffset != InvalidIndex
				&& rt.localSchema->initInstance(arena, frame + slot.recordOffset) != Status::Ok) {
			return Status::ErrorOutOfHostMemory;
		}
	}
	return Status::Ok;
}

// And a dying one. A local record may hold a container field, and a container field owns a block
// that nothing else will free.
template <typename A, typename Graph>
inline void releaseFrameRecords(A &arena, const Graph &graph, SpanView<FrameSlot> slots,
		uint32_t scope, Addr frame) {
	if (frame == NullAddr || scope >= graph.getScopeCount()) {
		return;
	}
	for (auto n : graph.getScopeNodes(scope)) {
		auto &rt = graph.getNodeAt(n);
		if (rt.localSchema && slots[n].recordOffset != InvalidIndex) {
			rt.localSchema->destroyInstance(arena, frame + slots[n].recordOffset);
		}
	}
}

// The activation of `scope` that encloses `activation` - up the tree until the scopes match. The
// walk is bounded by the nesting depth, which is what makes it cheap: loops nest a handful deep,
// however many times they turn. This is how a node in a loop body reads a value produced outside it
//.
template <typename Local>
inline uint32_t resolveScopeIn(const Local &local, uint32_t activation, uint32_t scope) {
	auto count = local.getActivationCount();
	uint32_t guard = 0;
	while (activation != NullActivation && activation < count && guard++ <= count) {
		auto data = local.readActivation(activation);
		if (data.scope == scope) {
			return activation;
		}
		activation = data.parent;
	}
	return NullActivation;
}

// Where a node's records live, and the check that makes the arithmetic safe. The scope check is not
// defensive tidiness, it is the whole safety of the scheme: an offset cannot fail to land
// somewhere, so asking for a body node in the root activation would compute an address inside a
// frame laid out for a different scope and hand back somebody else's bytes. A debugger does ask
// exactly that - a breakpoint watching a node of a loop body defaults to the root activation.
template <typename Local, typename Graph>
inline Addr frameForIn(const Local &local, const Graph *graph, uint32_t node, uint32_t activation) {
	if (!graph || node >= graph->getNodeCount()) {
		return NullAddr;
	}
	auto data = local.readActivation(activation);
	if (data.scope != graph->getNodeAt(node).scope) {
		return NullAddr; // no such activation, or one of a scope this node does not belong to
	}
	// Zero is an activation that was opened and whose scope has not been materialised yet.
	return data.frame;
}

// Every record of the run as (node, activation, frame), in activation order and then in scope
// order. The frame comes with the pair because the walk already holds it and every node of one
// activation shares it - a callback that looked it up per node would pay for an answer the loop
// just computed.
template <typename Local, typename Graph>
inline void forEachRecordIn(const Local &local, const Graph *graph,
		const Callback<bool(uint32_t, uint32_t, Addr)> &cb) {
	if (!graph) {
		return;
	}
	auto count = local.getActivationCount();
	for (uint32_t a = 0; a < count; ++a) {
		auto data = local.readActivation(a);
		if (data.frame == NullAddr || data.scope >= graph->getScopeCount()) {
			continue;
		}
		for (auto n : graph->getScopeNodes(data.scope)) {
			if (!cb(n, a, data.frame)) {
				return;
			}
		}
	}
}

// The sum of the scope widths of every activation that has been materialised. Counted rather than
// held, which is what makes it honest about activations that were opened and never filled.
template <typename Local, typename Graph>
inline uint32_t recordCountIn(const Local &local, const Graph *graph) {
	if (!graph) {
		return 0;
	}
	uint32_t total = 0;
	auto count = local.getActivationCount();
	for (uint32_t a = 0; a < count; ++a) {
		auto data = local.readActivation(a);
		if (data.frame != NullAddr && data.scope < graph->getScopeCount()) {
			total += uint32_t(graph->getScopeNodes(data.scope).size());
		}
	}
	return total;
}

// What a paused run looks like to a debugger. An observable form, so it is written once: a second
// one would be a second answer to "where is the run standing".
template <typename Local, typename Graph>
inline void describeLocal(const Local &local, const Graph *graph, mem_std::Value &out) {
	out = mem_std::Value(mem_std::Value::Type::DICTIONARY);
	out.setInteger(local.getRecordCount(), "records");
	out.setInteger(local.getStep(), "step");
	out.setInteger(local.getPass(), "pass");
	out.setInteger(local.getReadyCount(), "ready");

	auto &records = out.newArray("nodes");
	local.forEachRecord([&](uint32_t node, uint32_t activation, Addr frame) {
		mem_std::Value entry(mem_std::Value::Type::DICTIONARY);
		if (graph && node < graph->getNodeCount()) {
			entry.setInteger(int64_t(graph->getNodeAt(node).id), "id");
		}
		entry.setInteger(node, "node");
		entry.setInteger(activation, "activation");

		auto state = local.readState(local.getStateIn(frame, node));
		entry.setInteger(state.inputs, "inputs");
		entry.setInteger(state.produced, "produced");
		if ((state.flags & NodeFlags::Ran) != 0) {
			entry.setBool(true, "ran");
		}
		if ((state.flags & NodeFlags::Token) != 0) {
			entry.setBool(true, "token");
		}
		if ((state.flags & NodeFlags::StallData) != 0) {
			entry.setInteger(state.stallPin, "stallData");
		}
		if ((state.flags & NodeFlags::StallExec) != 0) {
			entry.setBool(true, "stallExec");
		}
		records.addValue(sprt::move(entry));
		return true;
	});

	auto &stalled = out.newArray("stalled");
	for (uint32_t i = 0; i < local.getStalledCount(); ++i) {
		auto node = recordKeyNode(local.getStalledAt(i));
		stalled.addInteger(graph && node < graph->getNodeCount()
						? int64_t(graph->getNodeAt(node).id)
						: int64_t(node));
	}

	// The activation tree, but only when there is one to speak of: a run without loops has the root
	// and nothing else, and saying so on every dump would be noise.
	if (local.getActivationCount() > 1) {
		auto &activations = out.newArray("activations");
		for (uint32_t i = 0; i < local.getActivationCount(); ++i) {
			auto data = local.readActivation(i);
			mem_std::Value entry(mem_std::Value::Type::DICTIONARY);
			entry.setInteger(i, "activation");
			entry.setInteger(data.scope, "scope");
			if (data.parent != NullActivation) {
				entry.setInteger(data.parent, "parent");
				entry.setInteger(data.iteration, "iteration");
				auto opener = recordKeyNode(data.openerKey);
				if (graph && opener < graph->getNodeCount()) {
					entry.setInteger(int64_t(graph->getNodeAt(opener).id), "opener");
				}
			}
			activations.addValue(sprt::move(entry));
		}
		out.setInteger(local.getOpenCount(), "open");
	}
}

// `A` is the arena kind the run lives in - see SPVStoreArena.h in stappler_vstore. A run's whole state is the
// arena's bytes, so the kind travels with the store rather than being fixed by the build: a
// debugger steps a ShadowArena run and a shipped host runs the identical code over a PlainArena.
// `Graph` is the representation the store reads the graph through - which nodes a scope consists
// of, each node's local schema, each node's scope - and it is a policy (SPFlowStatic.h) for the
// same reason the arena kind is a type: the frame layout is a function of the graph, and a graph a
// generated unit carries as static tables has to lay out the same frames as the one the build
// produced. The default is the built graph.
template <typename A, typename Graph = RuntimeGraph, typename Env = NoEnv>
class SP_PUBLIC LocalStoreT final {
public:
	using ArenaType = A;

	// The store a frame's bytes live in. Here that is the run's own arena; the fast store cuts its
	// frames elsewhere and says so.
	using FrameArena = A;
	using GraphType = Graph;

	// What the run executes in (SPFlowEnv.h): the scene its operations reach is a store of the
	// run's arena kind.
	using EnvType = Env;
	using SceneType = typename Env::template Scene<A>;

	// A unit of work here can be undone: the whole state of the run is the bytes of one arena, so a
	// version covers all of it. That is the promise the fast store gives up (SPFlowFast.h).
	static constexpr bool Rollbackable = true;

	// Registers interp.Run and interp.NodeState. Idempotent, like EntityStore::registerCoreTypes,
	// and for the same reason: a caller should not have to know whether someone else did it.
	static Status registerCoreTypes(value::TypeRegistry &);

	~LocalStoreT();

	LocalStoreT() = default;
	LocalStoreT(const LocalStoreT &) = delete;
	LocalStoreT &operator=(const LocalStoreT &) = delete;

	// Builds the store and materializes activation 0 of every node. The registry must already hold
	// the core types and the derived local schemas - in practice it is OpRegistry::getLocalTypes().
	Status init(A &, const Graph &, const value::TypeRegistry &);

	// Attaches to a run that is already in the arena, instead of building one - after
	// Arena::adopt() of an image taken mid-run, or after a rollback that replaced the arena's bytes
	// wholesale. That this is possible at all is the invariant: a run's state is the arena's bytes
	// and nothing else, so the same bytes anywhere are the same run. Nothing is reconstructed here:
	// the run's root block is where the arena's user root says it is.
	Status open(A &, const Graph &, const value::TypeRegistry &);

	// Frees everything. The arena is the caller's; what this releases is the run inside it.
	void destroy();

	bool isValid() const { return _arena != nullptr; }

	// The run's own bookkeeping is where it says it is: a root block at the arena's user root, and
	// the records it names. The arena is checked too.
	Status verify() const;

	A *getArena() const { return _arena; }

	// The same arena, kind-erased - the one spelling that works for both stores, and what the
	// executor interface's view hands a host (SPFlowEngine.h).
	value::ArenaRef getArenaRef() const {
		return _arena ? value::ArenaRef::of(*_arena) : value::ArenaRef();
	}

	// Frames. One activation owns one arena block; inside it every node of the scope has its
	// interp.NodeState and, if its operation declares one, its own record - at offsets this class
	// computes once from the graph and the schemas, the way a compiler lays out a stack frame. A
	// node's two records are adjacent, because a step that touches one almost always touches the
	// other. The layout is derived from the graph, which is code and not state, so holding it on
	// the host is not caching that a rollback could make stale: a rollback restores arena bytes, and the
	// offsets of a graph that has not changed are the same offsets.

	// Allocates `activation`'s frame and initialises every record in it: one allocation and one
	// zeroing per iteration.
	Status materializeScope(uint32_t scope, uint32_t activation);

	// The frame of an activation, or NullAddr when it has none yet. No scope check - this is the
	// raw slot, and `frameFor` is the one that answers for a node.
	Addr activationFrame(uint32_t activation) const;

	// The frame a node's records live in, or NullAddr when the pair names none. The scope check is
	// the whole safety of the scheme: an offset cannot fail to land somewhere, so asking for a body
	// node in the root activation would compute an address inside a frame laid out for a different
	// scope. A debugger asks exactly that - a breakpoint watching a node of a loop body defaults to
	// the root activation.
	Addr frameFor(uint32_t node, uint32_t activation) const;

	uint32_t activationScope(uint32_t activation) const;

	// Whether the pair names a record that exists.
	bool hasRecord(uint32_t node, uint32_t activation) const;

	// The frame layout, for a caller that wants to know what it costs. `frameBytes` is 0 for a
	// scope no graph declares.
	uint32_t getFrameBytes(uint32_t scope) const;

	// Activations.

	// Opens one and returns its id. Activations are never reused within a run: a debugger that
	// stops inside iteration seventeen has to be able to read iteration three, and the run is one
	// frame.
	Status openActivation(uint32_t scope, uint32_t parent, uint32_t iteration, uint64_t openerKey,
			uint32_t &out);

	uint32_t getActivationCount() const;
	ActivationData readActivation(uint32_t activation) const;

	// The activation of `scope` that encloses `activation` - walking up the tree until the scopes
	// match. NullActivation when `scope` does not enclose it. This is how a node in a loop body
	// reads a value produced outside it: the value lives where it was produced, and this
	// says where that is.
	uint32_t resolveScope(uint32_t activation, uint32_t scope) const;

	// The activations currently open, innermost last. When the ready front drains, the interpreter
	// closes the one on top and hands control back to whoever opened it.
	Status pushOpen(uint32_t activation);
	bool popOpen(uint32_t &out);
	uint32_t getOpenCount() const;
	uint32_t getOpenAt(uint32_t index) const; // NullActivation when out of range

	// How many records the run holds: the sum of the scope widths of every activation opened. A
	// count, not a directory - there is no array of that length any more.
	uint32_t getRecordCount() const;

	// Every record of the run as (node, activation, frame), in activation order and then in scope
	// order. Return false from the callback to stop. The frame comes with the pair because the walk
	// already holds it and every node of one activation shares it - a callback that looked it up
	// per node would pay two array reads for an answer the loop above it just computed.
	void forEachRecord(
			const Callback<bool(uint32_t node, uint32_t activation, Addr frame)> &) const;

	// The node's own record - the schema derived from its operation's outputs - or NullAddr when
	// the operation has none.
	Addr getRecord(uint32_t node, uint32_t activation) const;

	// The interp.NodeState record, which every node of a materialized scope has.
	Addr getState(uint32_t node, uint32_t activation) const;

	// The same two, with the frame already in hand. A step resolves the frame once and then asks
	// for both, which is the shape the whole layout exists to make cheap: an addition each.
	Addr getRecordIn(Addr frame, uint32_t node) const;
	Addr getStateIn(Addr frame, uint32_t node) const;

	// Node state.

	NodeStateData readState(Addr stateAddr) const;

	// The interp.NodeState descriptor, so that a debugger can read one of its fields by name
	// without the layer growing a getter per field.
	const value::ComponentType *getStateType() const { return _stateType; }

	Status markInput(Addr stateAddr, uint32_t pin);
	Status markProduced(Addr stateAddr, uint32_t pin);
	Status addFlags(Addr stateAddr, int64_t flags);
	Status removeFlags(Addr stateAddr, int64_t flags);
	Status setStall(Addr stateAddr, int64_t reason, uint32_t pin);
	Status clearStall(Addr stateAddr);

	// How many turns this record has opened. Written by the interpreter when it opens one, and read
	// back as the turn's number - which is why closing an activation, which clears Ran and Queued,
	// must not touch it.
	Status setTurns(Addr stateAddr, uint32_t);

	// The run's own state. The front holds (node, activation) pairs, packed the same way the
	// directory keys are: with loops the same node is on it more than once, in different
	// iterations. Two levels - a node of weight 1 goes on the upper one, and the top of the
	// heaviest level that is not empty comes off first. Within a level the front is the stack it
	// always was.
	Status pushReady(uint32_t node, uint32_t activation);
	bool popReady(uint32_t &node, uint32_t &activation);

	// The top of the front without taking it. What a debugger asks before it decides to step.
	bool peekReady(uint32_t &node, uint32_t &activation) const;

	uint32_t getReadyCount() const;

	// Whether the weights decide the level; off, everything goes on level 0.
	void setWeighted(bool value) { _weighted = value; }

	Status pushStalled(uint32_t node, uint32_t activation);
	uint32_t getStalledCount() const;
	uint64_t getStalledAt(uint32_t index) const; // packed key, NullRecordKey when out of range
	Status clearStalled();

	uint32_t getStep() const;
	Status setStep(uint32_t);
	uint32_t getPass() const;
	Status setPass(uint32_t);

	void describe(mem_std::Value &) const;

private:
	Status resolveTypes(const value::TypeRegistry &);

	// Lays out every scope through the free function above. Fails only if a schema is missing,
	// which resolveTypes has already refused, so in practice this cannot fail after it.
	Status computeFrameLayout();

	// Frees one activation's frame, releasing whatever its records own first. A local record may
	// hold a container field, and a container field owns an arena block that nothing else will
	// free.
	void releaseFrame(uint32_t activation);

	// The run's root block: where the interp.Run record and the interp.RunLevels record are. A
	// block of its own at the arena's user root rather than either record there, so that an image
	// says in one place which records are the run's.
	struct RunRoot {
		static constexpr uint32_t Magic = 0x6e75'7258; // "Xrun"

		uint32_t magic = Magic;
		Addr run = NullAddr;
		Addr levels = NullAddr;
		uint32_t reserved = 0;
	};

	bool readRoot(RunRoot &) const;

	// The run's own record, and the address of one of its fields. Memoized under the arena's
	// derived-value stamp, because this is the single hottest lookup in the interpreter. Sound as
	// well as fast: the root block is never moved and the records it names are never replaced, so
	// their addresses change only when the arena's bytes are replaced wholesale
	// (Journal::rollback, adopt(), setUserRoot()), and Arena::getEpoch() changes for that. What is
	// held is two Addrs and the stamp they were true under.
	Addr runRecord() const;
	Addr runField(const value::FieldDesc *) const;

	// Drops the memo. Called wherever the store changes arena, which the stamp alone cannot notice.
	void forgetRunRecord();

	// One activation's five Ints, in one handle read and one arena read. False when there is no
	// such activation. Everything that asks about an activation goes through this.
	bool readActivationRaw(uint32_t activation, int64_t (&out)[ActivationStride]) const;

	// The address of an activation's `frame` slot, for the two places that write one.
	Addr frameSlotAddr(uint32_t activation) const;

	Status arrayPushInt(const value::FieldDesc *, int64_t);
	uint32_t arrayCountOf(const value::FieldDesc *) const;

	A *_arena = nullptr;
	const Graph *_graph = nullptr;

	const value::ComponentType *_runType = nullptr;
	const value::ComponentType *_stateType = nullptr;

	const value::FieldDesc *_activations = nullptr;
	const value::FieldDesc *_ready = nullptr;
	const value::ComponentType *_levelsType = nullptr;
	const value::FieldDesc *_ready1 = nullptr;
	bool _hasLevels = false;
	bool _weighted = true;
	Addr levelField() const;
	const value::FieldDesc *_stalled = nullptr;
	const value::FieldDesc *_step = nullptr;
	const value::FieldDesc *_pass = nullptr;

	const value::FieldDesc *_open = nullptr;

	NodeStateFields _stateFields;

	// The frame layout: one entry per node of the graph, one size per scope. Derived from the
	// graph, so it is rebuilt by init()/open() and is not state the arena owns.
	mem_std::Vector<FrameSlot> _slots;
	mem_std::Vector<uint32_t> _frameBytes;

	// The memo behind runRecord(), and the arena stamp it was taken under. `mutable` because
	// runRecord() is const and filling a memo is not a change of state - the store answers the same
	// question either way.
	mutable Addr _runRecordMemo = NullAddr;
	mutable Addr _levelsRecordMemo = NullAddr;
	mutable uint32_t _runRecordEpoch = 0;
};

} // namespace stappler::flow

#endif /* STAPPLER_FLOW_SPFLOWLOCAL_H_ */
