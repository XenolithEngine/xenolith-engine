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

#ifndef STAPPLER_FLOW_VALUE_SPFLOWVALUEVAR_H_
#define STAPPLER_FLOW_VALUE_SPFLOWVALUEVAR_H_

#include "SPFlowValue.h"

#include <sprt/runtime/hash.h>

namespace STAPPLER_VERSIONIZED stappler::flow::value {

// The stable identity of a component type, an enum family or a referenced schema. Derived from the
// canonical name and nothing else: the engine has no stable type-id facility -
// Component::GetNextId() is a process-local atomic counter and type_index is ABI-local - so neither
// would survive a save/load, let alone another platform. sprt::hash64 and nothing else:
// StringView::hash(), sprt::hashSize() and sprt::hash<T>{} all pick 32 or 64 bits by pointer width.
using TypeId = uint64_t;

static constexpr TypeId NullTypeId = 0;

constexpr TypeId makeTypeId(StringView name) { return sprt::hash64(name.data(), name.size()); }

// Index plus generation, so a stale reference is rejected rather than silently resolving to whoever
// took the slot next. The ECS index owns the allocator; the packing is fixed here because a Var
// carries one and the packing is therefore part of the arena image.
struct EntityId {
	uint32_t index = 0;
	uint32_t generation = 0;

	uint64_t pack() const { return uint64_t(index) | (uint64_t(generation) << 32); }
	static EntityId unpack(uint64_t v) { return EntityId{uint32_t(v), uint32_t(v >> 32)}; }

	bool operator==(const EntityId &) const = default;
	bool operator!=(const EntityId &) const = default;
};

enum class VarType : uint8_t {
	Nil = 0,
	Bool,
	Int,
	Float,
	Vec2,
	Vec3,
	Vec4,
	Color,
	EntityRef,
	String,
	Array,
	Map,
	Enum,

	// Opaque bytes: the same storage as a String, and a different claim about it. Appended rather
	// than placed next to String because the ordinal is part of the image format - it is written
	// into every Var, into every element chain, and into every schema hash - so inserting one in
	// the middle would renumber four types and invalidate every stored scene.
	Bytes,

	// 32-bit scalars: a parallel segment on a GPU has no 64-bit integers or doubles. Int32 and
	// UInt32 are held in `i`, Float32 in `v[0]`.
	Int32,
	UInt32,
	Float32,
};

static constexpr uint32_t VarTypeCount = 17;

SP_PUBLIC StringView getVarTypeName(VarType);
SP_PUBLIC bool readVarType(StringView, VarType &);

// String, Bytes, Array and Map hold their content in a separate arena block; everything else is
// inline. This predicate is what drives ownership - destroy, copy and projection all recurse on it
// - so a type that owns a block has to be here.
constexpr bool isContainerType(VarType t) {
	return t == VarType::String || t == VarType::Bytes || t == VarType::Array || t == VarType::Map;
}

// A container's element type, one byte per nesting level: byte 0 is the immediate element type, and
// if that byte is itself a container its own element type follows in byte 1. Nil (0) ends the
// chain, so four bytes carry at most four levels - Array<Array<Array<Int>>> is [Array, Array, Int,
// 0]. It lives inside Var's union, in the four bytes a BlobHandle leaves over, which is what buys
// 32 bits for it instead of 16, and with them the nesting.
using ElementChain = uint32_t;

static constexpr uint32_t MaxChainDepth = 4;

constexpr VarType chainHead(ElementChain c) { return VarType(c & 0xff); }
constexpr ElementChain chainTail(ElementChain c) { return c >> 8; }

// Nil-terminated, so the depth is the number of non-Nil bytes before the first Nil.
constexpr uint32_t chainDepth(ElementChain c) {
	uint32_t n = 0;
	while (n < MaxChainDepth && VarType(c & 0xff) != VarType::Nil) {
		++n;
		c >>= 8;
	}
	return n;
}

// Prepends `t` to `tail`. Fails rather than truncating: a silently shortened chain would describe a
// different type and free every block below the cut.
SP_PUBLIC bool chainPush(VarType t, ElementChain tail, ElementChain &out);

// Convenience for the common shallow cases.
constexpr ElementChain makeChain(VarType a) { return ElementChain(a); }
constexpr ElementChain makeChain(VarType a, VarType b) {
	return ElementChain(a) | (ElementChain(b) << 8);
}
constexpr ElementChain makeChain(VarType a, VarType b, VarType c) {
	return ElementChain(a) | (ElementChain(b) << 8) | (ElementChain(c) << 16);
}

// The in-component handle of a blob: one flat arena block, never a rope. A blob larger than
// MaxInChunkPayload simply becomes an oversize run, which the arena handles transparently.
// `capacity` is always alignUp(_, Granule) with a floor of MinPayload, so it equals
// Arena::sizeOf(data) exactly and there is no slack the write barrier does not cover.
struct BlobHandle {
	Addr data = NullAddr;
	uint32_t size = 0;
	uint32_t capacity = 0;
};

static_assert(sizeof(BlobHandle) == 12, "BlobHandle is part of the image format");

// The interpreter's value: a tag plus 16 bytes of payload. Every variant that needs an identity
// carries it inline and fills the union exactly - a container gets a 32-bit element chain next to
// its 12-byte handle, an Enum gets the 64-bit TypeId of its family next to its value, an EntityRef
// gets the TypeId of the schema it points at. Nothing is wasted and nothing is truncated.
// alignas(8): the i386 SysV ABI aligns int64_t and double to 4 inside a struct, and the
// image format is the same on every target.
struct alignas(8) Var {
	VarType type = VarType::Nil;
	uint8_t reserved0 = 0; // named rather than implicit: these bytes are part of the image
	uint16_t reserved1 = 0;
	uint32_t flags = 0;

	union {
		int64_t i;
		double f;
		float v[4];
		struct {
			int64_t value;
			TypeId type;
		} e; // Enum: 8 + 8
		struct {
			uint64_t id;
			TypeId schema;
		} ent; // EntityRef: 8 + 8
		struct {
			BlobHandle blob;
			ElementChain elem;
		} c; // container: 12 + 4
	};

	Var() : i(0) { }
};

static_assert(sizeof(Var) == 24, "Var is part of the image format");
static_assert(alignof(Var) == 8, "Var is part of the image format");

// Constructors. Every one of them zeroes all 24 bytes before writing the payload: a Var reaches the
// arena inside a map spine and stores have to be byte-identical, so the bytes a variant does not
// use are part of the contract rather than slack.
SP_PUBLIC Var makeNil();
SP_PUBLIC Var makeBool(bool);
SP_PUBLIC Var makeInt(int64_t);
SP_PUBLIC Var makeFloat(double);
SP_PUBLIC Var makeInt32(int32_t);
SP_PUBLIC Var makeUInt32(uint32_t);
SP_PUBLIC Var makeFloat32(float);
SP_PUBLIC Var makeVec2(float x, float y);
SP_PUBLIC Var makeVec3(float x, float y, float z);
SP_PUBLIC Var makeVec4(float x, float y, float z, float w);
SP_PUBLIC Var makeColor(float r, float g, float b, float a);
SP_PUBLIC Var makeEntityRef(EntityId, TypeId schema = NullTypeId);
SP_PUBLIC Var makeEnum(int64_t value, TypeId family = NullTypeId);
SP_PUBLIC Var makeBlob(VarType, ElementChain elem, BlobHandle);

// Compares the tag and the payload the tag makes meaningful.
SP_PUBLIC bool operator==(const Var &, const Var &);
SP_PUBLIC bool operator!=(const Var &, const Var &);

// Compares all 24 bytes. Test-facing: the two must agree on every value make* can produce, and that
// agreement is the proof that the zeroing discipline holds.
SP_PUBLIC bool varBytesEqual(const Var &, const Var &);

// Projection of the inline variants. A container's content lives in the arena and is projected by
// the blob layer, which has one; encodeVar returns false when handed a container. Encoding is bare
// - no type tag - because the reader always knows the type: a declared field takes it from its
// descriptor and a heterogeneous map value carries it in its own Var, so decodeVar is told which
// type to produce. A TypeId does not fit data::Value's signed integer, so it goes in bit-for-bit as
// int64_t: the value may print negative and round-trips exactly.
SP_PUBLIC bool encodeVar(const Var &, mem_std::Value &out);
SP_PUBLIC Status decodeVar(const mem_std::Value &, VarType, Var &out);

// What a conversion from one VarType to another is, if it exists at all. A table rather than a
// switch, because var-cast walks every cell and asserts every one against a copy of the matrix
// written out separately - a matrix that lived only inside a switch could drift a cell at a time.
enum class CastRule : uint8_t {
	Reject = 0, // '.' no conversion exists
	Same, // '=' identity, or identical bytes under a different tag (Vec4 <-> Color)
	Widen, // '>' cannot lose anything
	Narrow, // '<' defined, but may lose information
	Parse, // 'p' String -> scalar; legal by type, may still fail on the VALUE
	Format, // 'f' scalar -> String; always succeeds, always allocates
};

enum class CastPolicy {
	// Narrow is accepted only when this particular value survives the inverse conversion unchanged.
	// A cell whose inverse is Reject can never demonstrate that, so it is refused. This is what the
	// interpreter's implicit conversions will use.
	Lossless,
	// Narrow is accepted outright. Migration uses this: it is a one-time, author-visible event that
	// reports what it did, unlike a conversion happening every frame.
	Lossy,
};

SP_PUBLIC CastRule getCastRule(VarType from, VarType to);
SP_PUBLIC StringView getCastRuleName(CastRule);

// Whether the cell needs an arena - to read a string's bytes, to allocate a result, or both.
// castVar() declines those; castVarInArena() implements every cell.
SP_PUBLIC bool castNeedsArena(VarType from, VarType to);

// Status::Declined - not an error - when the cell needs an arena. Refusal by the table or by the
// policy is Status::ErrorInvalidArguemnt.
SP_PUBLIC Status castVar(const Var &from, VarType to, CastPolicy, Var &out);

// Whether a value of one shape may become a value of another. The matrix above answers about tags
// and nothing else: `Array<Int>` and `Array<Float>` are both `array` to it, and two enum families
// are both `enum`. The whole question has three halves - the tag, the element chain and the subtype
// - and it lives here rather than beside any one consumer, because the graph's build and the
// screen's control binding both ask it and a second implementation would agree on the day it was
// written and on no day after. What deliberately did not come with it is the diagnostic code:
// this layer answers which half said no and each caller names it in its own words - the universal
// part is shared, the code stays with whoever reports.

// The three things about a value that decide whether it fits somewhere. A pin has them, a field has
// them, and neither of those types belongs here - so the question takes the triple rather than
// either of the things that carry it. The same triple `controlForType` takes.
struct ValueShape {
	VarType type = VarType::Nil;
	ElementChain element = 0;
	TypeId subtype = NullTypeId; // enum family or referenced component type
};

// Which of the three halves refused, or that none did. Not a diagnostic code: it carries no name,
// no severity and no prose, because those are the caller's - see the note above.
enum class TypeMeet : uint8_t {
	Ok,
	Tag, // the matrix rejects the conversion, or it is not Same/Widen
	Element, // same tag, different element chain
	Subtype, // same tag, different enum family or referenced schema
};

// On Ok, `outRule` is the cell (Same or Widen) and the caller stores it; on a refusal it is
// whatever the matrix said and the answer says which half to report.
SP_PUBLIC TypeMeet valueTypesMeet(const ValueShape &from, const ValueShape &to, CastRule &outRule);

} // namespace stappler::flow::value

#endif /* STAPPLER_FLOW_VALUE_SPFLOWVALUEVAR_H_ */
