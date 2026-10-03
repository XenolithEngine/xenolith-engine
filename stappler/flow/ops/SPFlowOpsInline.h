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

#ifndef STAPPLER_FLOW_OPS_SPFLOWOPSINLINE_H_
#define STAPPLER_FLOW_OPS_SPFLOWOPSINLINE_H_

#include "SPFlowInterp.h"

/* The bodies of the node library, as templates over the door: one body, two instantiations. The
registration files beside this one register `&inl::name<flow::OpContext>`, the function pointer the
interpreter calls through a vtable; a generated unit calls `ops::inl::name(ctx)` on a door whose
node index is a constant (flow::StaticContext), and the compiler inlines the body into the step.
What an operation means therefore cannot diverge between interpreting and compiling. Every operation
carries the name of its template in OpDef::inlineName, which is how the generator finds it; one
without an inline name is called through the registry's OpFn against the same door, one indirect
call slower. Nothing here reaches anything OpContext does not offer - `Ctx` is a door and only a
door. */
namespace STAPPLER_VERSIONIZED stappler::flow::ops::inl {

// This node's scene group number `k`, resolved - or null, and then the operation falls back to
// working the name out for itself. Null is not an error: the graph was never bound to a scene, or
// this group's name arrives on an edge and so was never a fact the build could know. A component
// the scene does not have is a binding that exists and holds no type, which the door answers
// without a lookup.
template <typename Ctx>
static const flow::SceneBinding *bound(Ctx &ctx, uint32_t k) {
	auto bindings = ctx.getSceneBindings();
	if (k >= bindings.size() || bindings[k].dynamic) {
		return nullptr;
	}
	return &bindings[k];
}

// The component or field name behind a String pin. Reading it is the only place the scene family
// can fail for a reason the author can fix by typing, so an empty name is refused rather than
// hashed: makeTypeId("") is a perfectly good number that names nothing. A view, not a string - the
// name has been in the graph's constant table since the build. It is borrowed: hash it or resolve
// it, and keep neither.
template <typename Ctx>
static bool readName(Ctx &ctx, uint32_t pin, StringView &out) {
	return ctx.getInputName(pin, out) == Status::Ok;
}

template <typename Ctx>
static bool readEntity(Ctx &ctx, uint32_t pin, value::EntityId &out) {
	value::Var var;
	if (ctx.getInput(pin, var) != Status::Ok) {
		return false;
	}
	out = value::EntityId::unpack(var.ent.id);
	return out.index != 0;
}

// Flow: where a run starts, how it goes in order, and how it chooses. None of these needs support
// from the interpreter. `branch` is an ordinary operation that fires one of its two exec outputs;
// the one it does not fire simply ends that path, which is a legal end of a branch and not a stall.

template <typename Ctx>
Status flowEvent(Ctx &ctx) {
	return ctx.fire(uint32_t(0));
}

template <typename Ctx>
Status flowSequence(Ctx &ctx) {
	// Both, in declaration order. The interpreter's front discipline is what turns that into "the
	// first branch runs to completion before the second".
	auto st = ctx.fire(uint32_t(0));
	if (st != Status::Ok) {
		return st;
	}
	return ctx.fire(uint32_t(1));
}

template <typename Ctx>
Status flowBranch(Ctx &ctx) {
	value::Var condition;
	auto st = ctx.getInput(0, condition);
	if (st != Status::Ok) {
		return st;
	}
	return ctx.fire(condition.i != 0 ? uint32_t(0) : uint32_t(1));
}

// The one operation in the library that opens a scope. Everything its `body` output reaches is a
// loop body: the interpreter gives it a fresh record per turn, and hands control back here when
// that turn runs out of work. `forEach` itself only fires an exec output and advances a counter.
// The cursor is a declared local: it has to survive from one turn to the next, and it is nobody's
// output.
template <typename Ctx>
Status flowForEach(Ctx &ctx) {
	value::Var cursorVar;
	auto st = ctx.getLocal(0, cursorVar);
	if (st != Status::Ok) {
		return st;
	}
	auto cursor = uint32_t(cursorVar.i);

	// Length and element through the context, so that the common shape - a literal list written
	// into the graph - is read where it already is rather than rebuilt in the arena once per turn.
	uint32_t count = 0;
	if (ctx.getInputCount(0, count) != Status::Ok) {
		count = 0; // nothing on the pin at all: an empty list, and the loop ends immediately
	}

	if (cursor >= count) {
		// Reset, so that entering this node again - from a turn of an enclosing loop, say - starts
		// over instead of finding a cursor already at the end.
		st = ctx.setLocal(0, value::makeInt(0));
		if (st != Status::Ok) {
			return st;
		}
		return ctx.fire(uint32_t(1));
	}

	value::Var item;
	st = ctx.getInputElement(0, cursor, item);
	if (st != Status::Ok) {
		return st;
	}
	st = ctx.setOutput(0, item);
	if (st != Status::Ok) {
		return st;
	}
	st = ctx.setOutput(1, value::makeInt(int64_t(cursor)));
	if (st != Status::Ok) {
		return st;
	}
	st = ctx.setLocal(0, value::makeInt(int64_t(cursor + 1)));
	if (st != Status::Ok) {
		return st;
	}
	return ctx.fire(uint32_t(0));
}

// Arithmetic, logic, comparison, conversion and literals. Every one of these reads its inputs,
// computes, and writes its outputs: no state between calls, no reach outside the signature.

template <typename Ctx, typename Fn>
static Status binaryFloat(Ctx &ctx, Fn &&fn) {
	value::Var lhs;
	value::Var rhs;
	auto st = ctx.getInput(0, lhs);
	if (st != Status::Ok) {
		return st;
	}
	st = ctx.getInput(1, rhs);
	if (st != Status::Ok) {
		return st;
	}
	return ctx.setOutput(0, value::makeFloat(fn(lhs.f, rhs.f)));
}

template <typename Ctx, typename Fn>
static Status binaryInt(Ctx &ctx, Fn &&fn) {
	value::Var lhs;
	value::Var rhs;
	auto st = ctx.getInput(0, lhs);
	if (st != Status::Ok) {
		return st;
	}
	st = ctx.getInput(1, rhs);
	if (st != Status::Ok) {
		return st;
	}
	return ctx.setOutput(0, value::makeInt(fn(lhs.i, rhs.i)));
}

// Bool in, Bool out. A Bool output pin must carry a Bool Var - ComponentType::setField refuses a
// Var whose type is not the field's - so this cannot be `binaryInt`.
template <typename Ctx, typename Fn>
static Status binaryBool(Ctx &ctx, Fn &&fn) {
	value::Var lhs;
	value::Var rhs;
	auto st = ctx.getInput(0, lhs);
	if (st != Status::Ok) {
		return st;
	}
	st = ctx.getInput(1, rhs);
	if (st != Status::Ok) {
		return st;
	}
	return ctx.setOutput(0, value::makeBool(fn(lhs.i != 0, rhs.i != 0)));
}

template <typename Ctx>
Status mathAddFloat(Ctx &ctx) {
	return binaryFloat(ctx, [](double a, double b) { return a + b; });
}

template <typename Ctx>
Status mathSubFloat(Ctx &ctx) {
	return binaryFloat(ctx, [](double a, double b) { return a - b; });
}

template <typename Ctx>
Status mathMulFloat(Ctx &ctx) {
	return binaryFloat(ctx, [](double a, double b) { return a * b; });
}

/* Integer arithmetic wraps, modulo 2^64, and is written through `uint64_t` to say so. Signed
overflow is undefined in C++, and a graph is data an author types: a counter pressed once too often
must give a number - the same number under the interpreter and both compiled executors, on every
ABI. This body is the only one; the generated code calls it. */
template <typename Ctx>
Status mathAddInt(Ctx &ctx) {
	return binaryInt(ctx, [](int64_t a, int64_t b) { return int64_t(uint64_t(a) + uint64_t(b)); });
}

template <typename Ctx>
Status mathMulInt(Ctx &ctx) {
	return binaryInt(ctx, [](int64_t a, int64_t b) { return int64_t(uint64_t(a) * uint64_t(b)); });
}

template <typename Ctx>
Status logicAnd(Ctx &ctx) {
	return binaryBool(ctx, [](bool a, bool b) { return a && b; });
}

template <typename Ctx>
Status logicOr(Ctx &ctx) {
	return binaryBool(ctx, [](bool a, bool b) { return a || b; });
}

template <typename Ctx>
Status logicNot(Ctx &ctx) {
	value::Var value;
	auto st = ctx.getInput(0, value);
	if (st != Status::Ok) {
		return st;
	}
	return ctx.setOutput(0, value::makeBool(value.i == 0));
}

template <typename Ctx>
Status compareLessFloat(Ctx &ctx) {
	value::Var lhs;
	value::Var rhs;
	auto st = ctx.getInput(0, lhs);
	if (st != Status::Ok) {
		return st;
	}
	st = ctx.getInput(1, rhs);
	if (st != Status::Ok) {
		return st;
	}
	return ctx.setOutput(0, value::makeBool(lhs.f < rhs.f));
}

template <typename Ctx>
Status compareEqualInt(Ctx &ctx) {
	value::Var lhs;
	value::Var rhs;
	auto st = ctx.getInput(0, lhs);
	if (st != Status::Ok) {
		return st;
	}
	st = ctx.getInput(1, rhs);
	if (st != Status::Ok) {
		return st;
	}
	return ctx.setOutput(0, value::makeBool(lhs.i == rhs.i));
}

// Ordering on integers. Spelling `a < b` by widening both sides to Float - through the Narrow cell
// convert.intToFloat exists to make explicit - is correct for small numbers and quietly wrong past
// 2^53, so the comparison exists on Int in its own right.
template <typename Ctx>
Status compareLessInt(Ctx &ctx) {
	value::Var lhs;
	value::Var rhs;
	auto st = ctx.getInput(0, lhs);
	if (st != Status::Ok) {
		return st;
	}
	st = ctx.getInput(1, rhs);
	if (st != Status::Ok) {
		return st;
	}
	return ctx.setOutput(0, value::makeBool(lhs.i < rhs.i));
}

// Element of an Array<Int> by index. `flow.forEach` hands out `item` and `index`, so this is what
// pairs a second table with the one being walked - two parallel arrays read at the same index. An
// index outside the array is an error, not a zero: a silent zero is a legal-looking value that
// nothing downstream can distinguish from a real one.
template <typename Ctx>
Status arrayGetInt(Ctx &ctx) {
	value::Var indexVar;
	auto st = ctx.getInput(1, indexVar);
	if (st != Status::Ok) {
		return st;
	}
	if (indexVar.i < 0) {
		return Status::ErrorInvalidArguemnt;
	}

	// Read where the list already is when it is a literal. Nothing here needs the list to exist in
	// the store.
	uint32_t count = 0;
	if (ctx.getInputCount(0, count) != Status::Ok || uint64_t(indexVar.i) >= count) {
		return Status::ErrorInvalidArguemnt;
	}

	value::Var value;
	st = ctx.getInputElement(0, uint32_t(indexVar.i), value);
	if (st != Status::Ok) {
		return st;
	}
	return ctx.setOutput(0, value);
}

template <typename Ctx>
Status convertIntToFloat(Ctx &ctx) {
	value::Var value;
	auto st = ctx.getInput(0, value);
	if (st != Status::Ok) {
		return st;
	}
	return ctx.setOutput(0, value::makeFloat(double(value.i)));
}

// A literal node: an input with a default and an output that passes it through. A pin parameter can
// already hold a literal, so this exists for the case where a value needs a place on the canvas of
// its own, and for the case where several inputs share one. One body under three registrations
// (`value.float`, `value.int`, `value.bool`).
template <typename Ctx>
Status valuePassthrough(Ctx &ctx) {
	value::Var value;
	auto st = ctx.getInput(0, value);
	if (st != Status::Ok) {
		return st;
	}
	return ctx.setOutput(0, value);
}

// `string.concat` is the library's only operation with a container output, and therefore the only
// one that has to obey the ownership rule: a value read across an edge belongs to the node that
// produced it, so anything this node stores it must own itself. It writes through claimOutput() and
// the blob accessors; storing the handle it was handed would give one block two owners and free it
// twice at the end of the run.
template <typename Ctx>
Status stringConcat(Ctx &ctx) {
	// By value: the kind is erased at this seam, and an ArenaRef is two words.
	auto arena = ctx.getArena();

	// Copied out before anything allocates: a pointer into the arena dies at the next allocator
	// call, and writing the output is one.
	mem_std::String result;
	if (auto lhs = ctx.getInputAddr(0)) {
		result.append(value::blob::stringGet(arena, lhs));
	}
	if (auto rhs = ctx.getInputAddr(1)) {
		result.append(value::blob::stringGet(arena, rhs));
	}

	auto out = ctx.claimOutput(0);
	if (out == flow::NullAddr) {
		return Status::ErrorInvalidArguemnt;
	}
	return value::blob::stringAssign(arena, out, StringView(result));
}

// A container cannot be passed through by value: setOutput refuses a container Var, and the copy is
// what makes the output the node's own.
template <typename Ctx>
Status valueString(Ctx &ctx) {
	return ctx.copyInputToOutput(0, 0);
}

// Passes execution through and takes a value it does nothing with. Its whole purpose is to be a
// point in the execution log that a test can name - which is more honest than making a real
// operation carry that job.
template <typename Ctx>
Status debugTrace(Ctx &ctx) {
	return ctx.fire(uint32_t(0));
}

// Waiting is data, not a state of the interpreter: there are no latent nodes and no suspended runs.
// "wait a tenth of a second" is a component in the scene that survives between runs, plus a node
// that reads the frame's dt and subtracts it.

constexpr StringView TimerTypeName("wait.Timer");
constexpr StringView FrameTypeName("Frame");

inline flow::TypeId timerType() { return value::makeTypeId(TimerTypeName); }
inline flow::TypeId frameType() { return value::makeTypeId(FrameTypeName); }

template <typename Ctx>
Status waitTimer(Ctx &ctx) {
	value::Var target;
	auto st = ctx.getInput(0, target);
	if (st != Status::Ok) {
		return st;
	}
	auto entity = value::EntityId::unpack(target.ent.id);

	value::Var timeout;
	st = ctx.getInput(1, timeout);
	if (st != Status::Ok) {
		return st;
	}

	// The three things this operation names in the scene, in the order its signature declares them
	// (see registerTimeOps). No pin mentions any of them, so they arrive through OpDef's own
	// declaration rather than through PinRole; a bound graph has them as descriptors.
	auto bindings = ctx.getSceneBindings();
	auto haveBindings = bindings.size() == 3 && bindings[0].desc && bindings[2].desc;

	// The frame's dt, from the global entity. Written by the environment before the frame's
	// keyframe, which is what makes a replayed frame use the same dt as the first run.
	value::Var dt;
	st = haveBindings
			? ctx.sceneGet(ctx.getGlobalEntity(), bindings[0], dt)
			: ctx.sceneGet(ctx.getGlobalEntity(), frameType(), StringView("dt"), dt);
	if (st != Status::Ok) {
		return st;
	}

	// Arming is idempotent by construction: the component is added only when it is not there, and
	// EntityStore::addComponent leaves an existing record alone anyway.
	auto armed = haveBindings ? ctx.sceneHas(entity, bindings[1]) : ctx.sceneHas(entity, timerType());
	if (!armed) {
		st = haveBindings ? ctx.sceneAdd(entity, bindings[1]) : ctx.sceneAdd(entity, timerType());
		if (st != Status::Ok) {
			return st;
		}
		st = haveBindings ? ctx.sceneSet(entity, bindings[2], timeout)
						  : ctx.sceneSet(entity, timerType(), StringView("remaining"), timeout);
		if (st != Status::Ok) {
			return st;
		}
	}

	value::Var remaining;
	st = haveBindings ? ctx.sceneGet(entity, bindings[2], remaining)
					  : ctx.sceneGet(entity, timerType(), StringView("remaining"), remaining);
	if (st != Status::Ok) {
		return st;
	}

	auto left = remaining.f - dt.f;
	if (left > 0.0) {
		// Still waiting. Nothing fires, so the branch behind this node simply ends for this frame.
		return haveBindings ? ctx.sceneSet(entity, bindings[2], value::makeFloat(left))
							: ctx.sceneSet(entity, timerType(), StringView("remaining"),
									  value::makeFloat(left));
	}

	// Spent. The component goes, so that entering this node again starts a new wait rather than
	// firing straight through on every frame from now on.
	st = haveBindings ? ctx.sceneRemove(entity, bindings[1]) : ctx.sceneRemove(entity, timerType());
	if (st != Status::Ok) {
		return st;
	}
	return ctx.fire(uint32_t(0));
}

template <typename Ctx>
Status awaitComponent(Ctx &ctx) {
	value::Var target;
	auto st = ctx.getInput(0, target);
	if (st != Status::Ok) {
		return st;
	}
	auto entity = value::EntityId::unpack(target.ent.id);

	// The component, resolved by the build when the name was a literal - which it is in every graph
	// anyone writes. The unbound path reads the name instead, as a view into the constant table.
	auto bindings = ctx.getSceneBindings();
	auto binding = bindings.size() == 1 && !bindings[0].dynamic ? &bindings[0] : nullptr;

	flow::TypeId type = value::NullTypeId;
	if (!binding) {
		StringView name;
		if (ctx.getInputName(1, name) != Status::Ok || name.empty()) {
			return Status::ErrorInvalidArguemnt;
		}
		type = value::makeTypeId(name);
	}

	// A component nothing has ever registered is an answer that has not arrived, not a mistake -
	// which is why this pin is optional and why a null binding here means "still waiting".
	auto arrived = binding ? ctx.sceneHas(entity, *binding) : ctx.sceneHas(entity, type);
	if (!arrived) {
		return Status::Ok; // no answer yet; the branch behind this node ends for this frame
	}

	// Taking the answer away is what makes this fire once. The same answer left in place would
	// continue the branch on every frame after it arrived, and "the response was handled" would
	// have nowhere to live.
	st = binding ? ctx.sceneRemove(entity, *binding) : ctx.sceneRemove(entity, type);
	if (st != Status::Ok) {
		return st;
	}
	return ctx.fire(uint32_t(0));
}

// Reaching the scene from a graph. The type is in the operation, the name is in the data:
// `makeTypeId` is a pure function of a string, so naming a component by string decides nothing at
// run time. What data must not decide is the type of the value, and it does not.

template <typename Ctx>
Status sceneGlobal(Ctx &ctx) {
	auto id = ctx.getGlobalEntity();
	if (id.index == 0) {
		return Status::ErrorNotFound;
	}
	return ctx.setOutput(0, value::makeEntityRef(id));
}

// A named entity of the project: its id was resolved when the graph was bound to the table, so
// this is a constant handed back, with no lookup.
template <typename Ctx>
Status sceneNamed(Ctx &ctx) {
	auto named = ctx.getNamedBindings();
	if (named.empty() || named[0].entity.index == 0) {
		return Status::ErrorNotFound;
	}
	return ctx.setOutput(0, value::makeEntityRef(named[0].entity));
}

template <typename Ctx>
Status sceneHas(Ctx &ctx) {
	value::EntityId target;
	if (!readEntity(ctx, 0, target)) {
		return Status::ErrorInvalidArguemnt;
	}

	// Absence is an answer, including the absence of the whole type: a graph may ask about a
	// component nothing has ever produced, and "no" is the truth about it. Bound, that answer costs
	// nothing at all - the build already found there was no such type.
	bool has = false;
	if (auto binding = bound(ctx, 0)) {
		has = ctx.sceneHas(target, *binding);
	} else {
		StringView component;
		if (!readName(ctx, 1, component)) {
			return Status::ErrorInvalidArguemnt;
		}
		has = ctx.sceneHas(target, value::makeTypeId(component));
	}

	auto st = ctx.setOutput(0, value::makeBool(has));
	if (st != Status::Ok) {
		return st;
	}
	return ctx.fire(uint32_t(0));
}

template <typename Ctx>
Status sceneGetInt(Ctx &ctx) {
	value::EntityId target;
	if (!readEntity(ctx, 0, target)) {
		return Status::ErrorInvalidArguemnt;
	}

	value::Var value;
	Status st;
	if (auto binding = bound(ctx, 0)) {
		st = ctx.sceneGet(target, *binding, value);
	} else {
		StringView component;
		StringView field;
		if (!readName(ctx, 1, component) || !readName(ctx, 2, field)) {
			return Status::ErrorInvalidArguemnt;
		}
		st = ctx.sceneGet(target, value::makeTypeId(component), field, value);
	}
	if (st != Status::Ok) {
		return st;
	}
	if (value.type != value::VarType::Int) {
		return Status::ErrorInvalidArguemnt;
	}
	st = ctx.setOutput(0, value);
	if (st != Status::Ok) {
		return st;
	}
	return ctx.fire(uint32_t(0));
}

template <typename Ctx>
Status sceneSetInt(Ctx &ctx) {
	value::EntityId target;
	if (!readEntity(ctx, 0, target)) {
		return Status::ErrorInvalidArguemnt;
	}

	value::Var value;
	auto st = ctx.getInput(3, value);
	if (st != Status::Ok) {
		return st;
	}

	if (auto binding = bound(ctx, 0)) {
		st = ctx.sceneSet(target, *binding, value);
	} else {
		StringView component;
		StringView field;
		if (!readName(ctx, 1, component) || !readName(ctx, 2, field)) {
			return Status::ErrorInvalidArguemnt;
		}
		st = ctx.sceneSet(target, value::makeTypeId(component), field, value);
	}
	if (st != Status::Ok) {
		return st;
	}
	return ctx.fire(uint32_t(0));
}

template <typename Ctx>
Status sceneFindByField(Ctx &ctx) {
	value::Var value;
	auto st = ctx.getInput(2, value);
	if (st != Status::Ok) {
		return st;
	}

	value::EntityId found;
	if (auto binding = bound(ctx, 0)) {
		st = ctx.sceneFind(*binding, value, found);
	} else {
		StringView component;
		StringView field;
		if (!readName(ctx, 0, component) || !readName(ctx, 1, field)) {
			return Status::ErrorInvalidArguemnt;
		}
		st = ctx.sceneFind(value::makeTypeId(component), field, value, found);
	}
	// ErrorNotFound is the honest "nothing matches" and is reported through `found`, not as a
	// failed step. Everything else - an unreadable field, a type that does not match - is a mistake
	// in the graph and stops the run, because continuing would mean answering a question nobody
	// asked.
	if (st != Status::Ok && st != Status::ErrorNotFound) {
		return st;
	}

	st = ctx.setOutput(0, value::makeBool(found.index != 0));
	if (st != Status::Ok) {
		return st;
	}
	st = ctx.setOutput(1, value::makeEntityRef(found));
	if (st != Status::Ok) {
		return st;
	}
	return ctx.fire(uint32_t(0));
}

template <typename Ctx>
Status sceneAddComponent(Ctx &ctx) {
	value::EntityId target;
	if (!readEntity(ctx, 0, target)) {
		return Status::ErrorInvalidArguemnt;
	}

	Status st;
	if (auto binding = bound(ctx, 0)) {
		st = ctx.sceneAdd(target, *binding);
	} else {
		StringView component;
		if (!readName(ctx, 1, component)) {
			return Status::ErrorInvalidArguemnt;
		}
		st = ctx.sceneAdd(target, value::makeTypeId(component));
	}
	if (st != Status::Ok) {
		return st;
	}
	return ctx.fire(uint32_t(0));
}

template <typename Ctx>
Status sceneRemoveComponent(Ctx &ctx) {
	value::EntityId target;
	if (!readEntity(ctx, 0, target)) {
		return Status::ErrorInvalidArguemnt;
	}

	Status st;
	if (auto binding = bound(ctx, 0)) {
		st = ctx.sceneRemove(target, *binding);
	} else {
		StringView component;
		if (!readName(ctx, 1, component)) {
			return Status::ErrorInvalidArguemnt;
		}
		st = ctx.sceneRemove(target, value::makeTypeId(component));
	}
	// Removing what is not there is done, not failed. Graphs sweep marks off a whole collection
	// without first asking which members carry one, and making that an error would force every such
	// sweep to be written as a test and a branch.
	if (st != Status::Ok && st != Status::ErrorNotFound) {
		return st;
	}
	return ctx.fire(uint32_t(0));
}

// The numeric library over every scalar type: one template per operation, instantiated per value
// type through the registration's inline name (`ops::inl::mathAdd<value::VarType::Int32>`).
// Integer arithmetic wraps, as mathAddInt does.

template <value::VarType T>
struct Scalar;

template <>
struct Scalar<value::VarType::Int> {
	using Type = int64_t;
	static Type get(const value::Var &v) { return v.i; }
	static value::Var make(Type x) { return value::makeInt(x); }
};

template <>
struct Scalar<value::VarType::Float> {
	using Type = double;
	static Type get(const value::Var &v) { return v.f; }
	static value::Var make(Type x) { return value::makeFloat(x); }
};

template <>
struct Scalar<value::VarType::Int32> {
	using Type = int32_t;
	static Type get(const value::Var &v) { return int32_t(v.i); }
	static value::Var make(Type x) { return value::makeInt32(x); }
};

template <>
struct Scalar<value::VarType::UInt32> {
	using Type = uint32_t;
	static Type get(const value::Var &v) { return uint32_t(v.i); }
	static value::Var make(Type x) { return value::makeUInt32(x); }
};

template <>
struct Scalar<value::VarType::Float32> {
	using Type = float;
	static Type get(const value::Var &v) { return v.v[0]; }
	static value::Var make(Type x) { return value::makeFloat32(x); }
};

inline int64_t addOf(int64_t a, int64_t b) { return int64_t(uint64_t(a) + uint64_t(b)); }
inline int32_t addOf(int32_t a, int32_t b) { return int32_t(uint32_t(a) + uint32_t(b)); }
inline uint32_t addOf(uint32_t a, uint32_t b) { return a + b; }
inline double addOf(double a, double b) { return a + b; }
inline float addOf(float a, float b) { return a + b; }

inline int64_t subOf(int64_t a, int64_t b) { return int64_t(uint64_t(a) - uint64_t(b)); }
inline int32_t subOf(int32_t a, int32_t b) { return int32_t(uint32_t(a) - uint32_t(b)); }
inline uint32_t subOf(uint32_t a, uint32_t b) { return a - b; }
inline double subOf(double a, double b) { return a - b; }
inline float subOf(float a, float b) { return a - b; }

inline int64_t mulOf(int64_t a, int64_t b) { return int64_t(uint64_t(a) * uint64_t(b)); }
inline int32_t mulOf(int32_t a, int32_t b) { return int32_t(uint32_t(a) * uint32_t(b)); }
inline uint32_t mulOf(uint32_t a, uint32_t b) { return a * b; }
inline double mulOf(double a, double b) { return a * b; }
inline float mulOf(float a, float b) { return a * b; }

// Integer division truncates toward zero and refuses a zero divisor; MIN / -1 wraps to MIN.
inline bool divOf(int64_t a, int64_t b, int64_t &out) {
	if (b == 0) {
		return false;
	}
	out = (b == -1) ? int64_t(0 - uint64_t(a)) : a / b;
	return true;
}
inline bool divOf(int32_t a, int32_t b, int32_t &out) {
	if (b == 0) {
		return false;
	}
	out = (b == -1) ? int32_t(0 - uint32_t(a)) : a / b;
	return true;
}
inline bool divOf(uint32_t a, uint32_t b, uint32_t &out) {
	if (b == 0) {
		return false;
	}
	out = a / b;
	return true;
}
inline bool divOf(double a, double b, double &out) {
	out = a / b;
	return true;
}
inline bool divOf(float a, float b, float &out) {
	out = a / b;
	return true;
}

// abs(MIN) wraps to MIN.
inline int64_t absOf(int64_t a) { return a < 0 ? int64_t(0 - uint64_t(a)) : a; }
inline int32_t absOf(int32_t a) { return a < 0 ? int32_t(0 - uint32_t(a)) : a; }
inline double absOf(double a) { return a < 0.0 ? -a : a; }
inline float absOf(float a) { return a < 0.0f ? -a : a; }

template <value::VarType T, typename Ctx, typename Fn>
static Status binaryScalar(Ctx &ctx, Fn &&fn) {
	value::Var lhs;
	value::Var rhs;
	auto st = ctx.getInput(0, lhs);
	if (st != Status::Ok) {
		return st;
	}
	st = ctx.getInput(1, rhs);
	if (st != Status::Ok) {
		return st;
	}
	return ctx.setOutput(0, Scalar<T>::make(fn(Scalar<T>::get(lhs), Scalar<T>::get(rhs))));
}

template <value::VarType T, typename Ctx, typename Fn>
static Status unaryScalar(Ctx &ctx, Fn &&fn) {
	value::Var value;
	auto st = ctx.getInput(0, value);
	if (st != Status::Ok) {
		return st;
	}
	return ctx.setOutput(0, Scalar<T>::make(fn(Scalar<T>::get(value))));
}

template <value::VarType T, typename Ctx>
Status mathAdd(Ctx &ctx) {
	using V = typename Scalar<T>::Type;
	return binaryScalar<T>(ctx, [](V a, V b) { return addOf(a, b); });
}

template <value::VarType T, typename Ctx>
Status mathSub(Ctx &ctx) {
	using V = typename Scalar<T>::Type;
	return binaryScalar<T>(ctx, [](V a, V b) { return subOf(a, b); });
}

template <value::VarType T, typename Ctx>
Status mathMul(Ctx &ctx) {
	using V = typename Scalar<T>::Type;
	return binaryScalar<T>(ctx, [](V a, V b) { return mulOf(a, b); });
}

template <value::VarType T, typename Ctx>
Status mathDiv(Ctx &ctx) {
	value::Var lhs;
	value::Var rhs;
	auto st = ctx.getInput(0, lhs);
	if (st != Status::Ok) {
		return st;
	}
	st = ctx.getInput(1, rhs);
	if (st != Status::Ok) {
		return st;
	}
	typename Scalar<T>::Type out;
	if (!divOf(Scalar<T>::get(lhs), Scalar<T>::get(rhs), out)) {
		return Status::ErrorInvalidArguemnt;
	}
	return ctx.setOutput(0, Scalar<T>::make(out));
}

template <value::VarType T, typename Ctx>
Status mathMin(Ctx &ctx) {
	using V = typename Scalar<T>::Type;
	return binaryScalar<T>(ctx, [](V a, V b) { return b < a ? b : a; });
}

template <value::VarType T, typename Ctx>
Status mathMax(Ctx &ctx) {
	using V = typename Scalar<T>::Type;
	return binaryScalar<T>(ctx, [](V a, V b) { return a < b ? b : a; });
}

template <value::VarType T, typename Ctx>
Status mathAbs(Ctx &ctx) {
	using V = typename Scalar<T>::Type;
	return unaryScalar<T>(ctx, [](V a) { return absOf(a); });
}

template <value::VarType T, typename Ctx>
Status compareLess(Ctx &ctx) {
	value::Var lhs;
	value::Var rhs;
	auto st = ctx.getInput(0, lhs);
	if (st != Status::Ok) {
		return st;
	}
	st = ctx.getInput(1, rhs);
	if (st != Status::Ok) {
		return st;
	}
	return ctx.setOutput(0, value::makeBool(Scalar<T>::get(lhs) < Scalar<T>::get(rhs)));
}

template <value::VarType T, typename Ctx>
Status compareEqual(Ctx &ctx) {
	value::Var lhs;
	value::Var rhs;
	auto st = ctx.getInput(0, lhs);
	if (st != Status::Ok) {
		return st;
	}
	st = ctx.getInput(1, rhs);
	if (st != Status::Ok) {
		return st;
	}
	return ctx.setOutput(0, value::makeBool(Scalar<T>::get(lhs) == Scalar<T>::get(rhs)));
}

template <value::VarType T, typename Ctx>
Status mathFloor(Ctx &ctx) {
	using V = typename Scalar<T>::Type;
	return unaryScalar<T>(ctx, [](V a) { return V(sprt::floor(a)); });
}

template <value::VarType T, typename Ctx>
Status mathCeil(Ctx &ctx) {
	using V = typename Scalar<T>::Type;
	return unaryScalar<T>(ctx, [](V a) { return V(sprt::ceil(a)); });
}

template <value::VarType T, typename Ctx>
Status mathSqrt(Ctx &ctx) {
	using V = typename Scalar<T>::Type;
	return unaryScalar<T>(ctx, [](V a) { return V(sprt::sqrt(a)); });
}

template <value::VarType T, typename Ctx>
Status mathSin(Ctx &ctx) {
	using V = typename Scalar<T>::Type;
	return unaryScalar<T>(ctx, [](V a) { return V(sprt::sin(a)); });
}

template <value::VarType T, typename Ctx>
Status mathCos(Ctx &ctx) {
	using V = typename Scalar<T>::Type;
	return unaryScalar<T>(ctx, [](V a) { return V(sprt::cos(a)); });
}

template <value::VarType T, typename Ctx>
Status mathTan(Ctx &ctx) {
	using V = typename Scalar<T>::Type;
	return unaryScalar<T>(ctx, [](V a) { return V(sprt::tan(a)); });
}

template <value::VarType T, typename Ctx>
Status mathPow(Ctx &ctx) {
	using V = typename Scalar<T>::Type;
	return binaryScalar<T>(ctx, [](V a, V b) { return V(sprt::pow(a, b)); });
}

// Vectors are float[N]; N is 2, 3 or 4.
inline value::Var makeVector(uint32_t n, const float v[4]) {
	switch (n) {
	case 2: return value::makeVec2(v[0], v[1]);
	case 3: return value::makeVec3(v[0], v[1], v[2]);
	default: return value::makeVec4(v[0], v[1], v[2], v[3]);
	}
}

template <uint32_t N, typename Ctx, typename Fn>
static Status binaryVector(Ctx &ctx, Fn &&fn) {
	value::Var lhs;
	value::Var rhs;
	auto st = ctx.getInput(0, lhs);
	if (st != Status::Ok) {
		return st;
	}
	st = ctx.getInput(1, rhs);
	if (st != Status::Ok) {
		return st;
	}
	float out[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	for (uint32_t k = 0; k < N; ++k) { out[k] = fn(lhs.v[k], rhs.v[k]); }
	return ctx.setOutput(0, makeVector(N, out));
}

template <uint32_t N, typename Ctx>
Status mathAddVec(Ctx &ctx) {
	return binaryVector<N>(ctx, [](float a, float b) { return a + b; });
}

template <uint32_t N, typename Ctx>
Status mathSubVec(Ctx &ctx) {
	return binaryVector<N>(ctx, [](float a, float b) { return a - b; });
}

template <uint32_t N, typename Ctx>
Status mathScaleVec(Ctx &ctx) {
	value::Var vec;
	value::Var factor;
	auto st = ctx.getInput(0, vec);
	if (st != Status::Ok) {
		return st;
	}
	st = ctx.getInput(1, factor);
	if (st != Status::Ok) {
		return st;
	}
	float out[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	for (uint32_t k = 0; k < N; ++k) { out[k] = vec.v[k] * factor.v[0]; }
	return ctx.setOutput(0, makeVector(N, out));
}

inline float dotOf(uint32_t n, const value::Var &a, const value::Var &b) {
	float sum = 0.0f;
	for (uint32_t k = 0; k < n; ++k) { sum += a.v[k] * b.v[k]; }
	return sum;
}

template <uint32_t N, typename Ctx>
Status mathDotVec(Ctx &ctx) {
	value::Var lhs;
	value::Var rhs;
	auto st = ctx.getInput(0, lhs);
	if (st != Status::Ok) {
		return st;
	}
	st = ctx.getInput(1, rhs);
	if (st != Status::Ok) {
		return st;
	}
	return ctx.setOutput(0, value::makeFloat32(dotOf(N, lhs, rhs)));
}

template <uint32_t N, typename Ctx>
Status mathLengthVec(Ctx &ctx) {
	value::Var value;
	auto st = ctx.getInput(0, value);
	if (st != Status::Ok) {
		return st;
	}
	return ctx.setOutput(0, value::makeFloat32(sprt::sqrt(dotOf(N, value, value))));
}

// A zero vector normalizes to itself.
template <uint32_t N, typename Ctx>
Status mathNormalizeVec(Ctx &ctx) {
	value::Var value;
	auto st = ctx.getInput(0, value);
	if (st != Status::Ok) {
		return st;
	}
	auto length = sprt::sqrt(dotOf(N, value, value));
	float out[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	if (length > 0.0f) {
		for (uint32_t k = 0; k < N; ++k) { out[k] = value.v[k] / length; }
	}
	return ctx.setOutput(0, makeVector(N, out));
}

// A conversion through the value layer's own cell, refused where the cell refuses: out of range,
// NaN, a real into an integer outside its range.
template <value::VarType To, typename Ctx>
Status convertScalar(Ctx &ctx) {
	value::Var value;
	auto st = ctx.getInput(0, value);
	if (st != Status::Ok) {
		return st;
	}
	value::Var out;
	if (value::castVar(value, To, value::CastPolicy::Lossy, out) != Status::Ok) {
		return Status::ErrorInvalidArguemnt;
	}
	return ctx.setOutput(0, out);
}

// The family is not the body's: the node's EnumFamily output reads back with it (nodeFieldSubtype).
template <typename Ctx>
Status enumFromInt32(Ctx &ctx) {
	value::Var value;
	auto st = ctx.getInput(0, value);
	if (st != Status::Ok) {
		return st;
	}
	return ctx.setOutput(0, value::makeEnum(value.i));
}

template <value::VarType T, typename Ctx>
Status sceneGetScalar(Ctx &ctx) {
	value::EntityId target;
	if (!readEntity(ctx, 0, target)) {
		return Status::ErrorInvalidArguemnt;
	}

	value::Var value;
	Status st;
	if (auto binding = bound(ctx, 0)) {
		st = ctx.sceneGet(target, *binding, value);
	} else {
		StringView component;
		StringView field;
		if (!readName(ctx, 1, component) || !readName(ctx, 2, field)) {
			return Status::ErrorInvalidArguemnt;
		}
		st = ctx.sceneGet(target, value::makeTypeId(component), field, value);
	}
	if (st != Status::Ok) {
		return st;
	}
	if (value.type != T) {
		return Status::ErrorInvalidArguemnt;
	}
	st = ctx.setOutput(0, value);
	if (st != Status::Ok) {
		return st;
	}
	return ctx.fire(uint32_t(0));
}

// The parallel block: a fan-out keeps the set it runs over in its first local and fires `body`, and
// the machine opens a branch per element. The barrier and the collectors run after the block is
// delivered and read the branches through the door.

template <typename Ctx>
Status parFanOutFire(Ctx &ctx) {
	// What a branch reads from these two comes from the branch; these values only mark them
	// produced.
	auto st = ctx.setOutput(0, value::makeEntityRef(value::EntityId()));
	if (st == Status::Ok) {
		st = ctx.setOutput(1, value::makeInt32(0));
	}
	return st != Status::Ok ? st : ctx.fire(uint32_t(0));
}

// Over a pin. Refuses a set that names an entity twice: two branches would write one entity.
template <typename Ctx>
Status parForEach(Ctx &ctx) {
	uint32_t count = 0;
	auto st = ctx.getInputCount(0, count);
	if (st != Status::Ok) {
		return st;
	}
	auto element = ctx.getOp().getDataIn()[0].element;
	auto arena = ctx.getArena();
	st = value::blob::arrayResize(arena, ctx.localAddr(0), element, 0);
	if (st != Status::Ok) {
		return st;
	}
	mem_std::Set<uint64_t> seen;
	for (uint32_t i = 0; i < count; ++i) {
		value::Var item;
		st = ctx.getInputElement(0, i, item);
		if (st != Status::Ok) {
			return st;
		}
		if (!seen.emplace(item.ent.id).second) {
			return Status::ErrorInvalidArguemnt;
		}
		st = value::blob::arrayPush(arena, ctx.localAddr(0), element, item);
		if (st != Status::Ok) {
			return st;
		}
	}
	return parFanOutFire(ctx);
}

// Over the block's query.
template <typename Ctx>
Status parForEachWith(Ctx &ctx) {
	auto st = ctx.queryEntities(0);
	return st != Status::Ok ? st : parFanOutFire(ctx);
}

template <typename Ctx>
Status parBarrier(Ctx &ctx) {
	uint32_t count = 0;
	auto st = ctx.getBarrierCount(count);
	if (st != Status::Ok) {
		return st;
	}
	auto masks = ctx.getOp().getDataOut()[0].element;
	auto present = ctx.claimOutput(0);
	auto failed = ctx.claimOutput(1);
	auto arena = ctx.getArena();
	if (present == flow::NullAddr || failed == flow::NullAddr) {
		return Status::ErrorInvalidArguemnt;
	}
	for (uint32_t i = 0; i < count; ++i) {
		bool isPresent = false;
		bool isFailed = false;
		st = ctx.getBarrierBranch(i, isPresent, isFailed);
		if (st == Status::Ok) {
			st = value::blob::arrayPush(arena, ctx.claimOutput(0), masks,
					value::makeBool(isPresent));
		}
		if (st == Status::Ok) {
			st = value::blob::arrayPush(arena, ctx.claimOutput(1), masks,
					value::makeBool(isFailed));
		}
		if (st != Status::Ok) {
			return st;
		}
	}
	return ctx.fire(uint32_t(0));
}

// Every present branch value, in branch order.
template <value::VarType T, typename Ctx>
Status parGather(Ctx &ctx) {
	uint32_t count = 0;
	auto st = ctx.getBranchCount(0, count);
	if (st != Status::Ok) {
		return st;
	}
	auto element = ctx.getOp().getDataOut()[0].element;
	auto arena = ctx.getArena();
	if (ctx.claimOutput(0) == flow::NullAddr) {
		return Status::ErrorInvalidArguemnt;
	}
	for (uint32_t i = 0; i < count; ++i) {
		if (!ctx.isBranchPresent(0, i)) {
			continue;
		}
		value::Var value;
		st = ctx.getBranchValue(0, i, value);
		if (st == Status::Ok) {
			st = value::blob::arrayPush(arena, ctx.claimOutput(0), element, value);
		}
		if (st != Status::Ok) {
			return st;
		}
	}
	return Status::Ok;
}

// A fold over the present branches in branch order; no branch at all is the zero of the type.
template <typename Ctx, typename Fold>
static Status parFold(Ctx &ctx, value::Var &acc, Fold &&fold) {
	// The device folded this block's branches as a tree and left the value here; otherwise the fold
	// happens in branch order.
	value::Var folded;
	if (ctx.takeBranchFold(0, folded) == Status::Ok) {
		return ctx.setOutput(0, folded);
	}
	uint32_t count = 0;
	auto st = ctx.getBranchCount(0, count);
	if (st != Status::Ok) {
		return st;
	}
	bool first = true;
	for (uint32_t i = 0; i < count; ++i) {
		if (!ctx.isBranchPresent(0, i)) {
			continue;
		}
		value::Var value;
		st = ctx.getBranchValue(0, i, value);
		if (st != Status::Ok) {
			return st;
		}
		fold(acc, value, first);
		first = false;
	}
	return ctx.setOutput(0, acc);
}

template <value::VarType T, typename Ctx>
Status parSum(Ctx &ctx) {
	auto acc = Scalar<T>::make(typename Scalar<T>::Type(0));
	return parFold(ctx, acc, [](value::Var &a, const value::Var &v, bool) {
		a = Scalar<T>::make(addOf(Scalar<T>::get(a), Scalar<T>::get(v)));
	});
}

template <value::VarType T, typename Ctx>
Status parMin(Ctx &ctx) {
	auto acc = Scalar<T>::make(typename Scalar<T>::Type(0));
	return parFold(ctx, acc, [](value::Var &a, const value::Var &v, bool first) {
		if (first || Scalar<T>::get(v) < Scalar<T>::get(a)) {
			a = v;
		}
	});
}

template <value::VarType T, typename Ctx>
Status parMax(Ctx &ctx) {
	auto acc = Scalar<T>::make(typename Scalar<T>::Type(0));
	return parFold(ctx, acc, [](value::Var &a, const value::Var &v, bool first) {
		if (first || Scalar<T>::get(a) < Scalar<T>::get(v)) {
			a = v;
		}
	});
}

template <uint32_t N, typename Ctx>
Status parSumVec(Ctx &ctx) {
	const float zero[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	auto acc = makeVector(N, zero);
	return parFold(ctx, acc, [](value::Var &a, const value::Var &v, bool) {
		for (uint32_t k = 0; k < N; ++k) { a.v[k] += v.v[k]; }
	});
}

template <typename Ctx>
Status parCount(Ctx &ctx) {
	auto acc = value::makeInt32(0);
	return parFold(ctx, acc, [](value::Var &a, const value::Var &v, bool) {
		if (v.i != 0) {
			a = value::makeInt32(int32_t(a.i) + 1);
		}
	});
}

template <typename Ctx>
Status parAny(Ctx &ctx) {
	auto acc = value::makeBool(false);
	return parFold(ctx, acc, [](value::Var &a, const value::Var &v, bool) {
		if (v.i != 0) {
			a = value::makeBool(true);
		}
	});
}

template <typename Ctx>
Status parAll(Ctx &ctx) {
	auto acc = value::makeBool(true);
	return parFold(ctx, acc, [](value::Var &a, const value::Var &v, bool) {
		if (v.i == 0) {
			a = value::makeBool(false);
		}
	});
}

} // namespace stappler::flow::ops::inl

#endif /* STAPPLER_FLOW_OPS_SPFLOWOPSINLINE_H_ */
