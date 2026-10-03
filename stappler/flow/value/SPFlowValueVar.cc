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

// The interpreter's value type. Two things here are load-bearing and easy to lose in a refactor.
// Every make* zeroes all 24 bytes before writing its payload: a Var reaches the arena inside a map
// spine and stores have to be byte-identical, so the bytes a variant does not use are part of the
// contract, not slack, and varBytesEqual() exists to prove it. And the union is exactly filled by
// every variant that needs an identity - 12 + 4 for a container and its element chain, 8 + 8 for an
// Enum or an EntityRef and its TypeId - which is why the element type has 32 bits, hence nesting.

#include "SPFlowValueVar.h"

#include <sprt/c/__sprt_string.h>

namespace STAPPLER_VERSIONIZED stappler::flow::value {

static const StringView s_varTypeNames[VarTypeCount] = {
	StringView("nil"),
	StringView("bool"),
	StringView("int"),
	StringView("float"),
	StringView("vec2"),
	StringView("vec3"),
	StringView("vec4"),
	StringView("color"),
	StringView("entity"),
	StringView("string"),
	StringView("array"),
	StringView("map"),
	StringView("enum"),
	StringView("bytes"),
	StringView("int32"),
	StringView("uint32"),
	StringView("float32"),
};

StringView getVarTypeName(VarType t) {
	auto i = uint32_t(t);
	return i < VarTypeCount ? s_varTypeNames[i] : StringView("?");
}

bool readVarType(StringView name, VarType &out) {
	for (uint32_t i = 0; i < VarTypeCount; ++i) {
		if (s_varTypeNames[i] == name) {
			out = VarType(i);
			return true;
		}
	}
	return false;
}

bool chainPush(VarType t, ElementChain tail, ElementChain &out) {
	// Truncating would describe a different type and orphan every block below the cut, so a chain
	// that does not fit is refused instead.
	if (t == VarType::Nil || chainDepth(tail) + 1 > MaxChainDepth) {
		return false;
	}
	out = ElementChain(uint8_t(t)) | (tail << 8);
	return true;
}

// One place where the zeroing happens, so there is one place to get it right.
static Var makeBlank(VarType t) {
	Var v;
	__sprt_memset(&v, 0, sizeof(Var));
	v.type = t;
	return v;
}

Var makeNil() { return makeBlank(VarType::Nil); }

Var makeBool(bool b) {
	auto v = makeBlank(VarType::Bool);
	v.i = b ? 1 : 0;
	return v;
}

Var makeInt(int64_t i) {
	auto v = makeBlank(VarType::Int);
	v.i = i;
	return v;
}

Var makeFloat(double f) {
	auto v = makeBlank(VarType::Float);
	v.f = f;
	return v;
}

Var makeInt32(int32_t i) {
	auto v = makeBlank(VarType::Int32);
	v.i = i;
	return v;
}

Var makeUInt32(uint32_t i) {
	auto v = makeBlank(VarType::UInt32);
	v.i = int64_t(i);
	return v;
}

Var makeFloat32(float f) {
	auto v = makeBlank(VarType::Float32);
	v.v[0] = f;
	return v;
}

Var makeVec2(float x, float y) {
	auto v = makeBlank(VarType::Vec2);
	v.v[0] = x;
	v.v[1] = y;
	return v;
}

Var makeVec3(float x, float y, float z) {
	auto v = makeBlank(VarType::Vec3);
	v.v[0] = x;
	v.v[1] = y;
	v.v[2] = z;
	return v;
}

Var makeVec4(float x, float y, float z, float w) {
	auto v = makeBlank(VarType::Vec4);
	v.v[0] = x;
	v.v[1] = y;
	v.v[2] = z;
	v.v[3] = w;
	return v;
}

Var makeColor(float r, float g, float b, float a) {
	auto v = makeBlank(VarType::Color);
	v.v[0] = r;
	v.v[1] = g;
	v.v[2] = b;
	v.v[3] = a;
	return v;
}

Var makeEntityRef(EntityId id, TypeId schema) {
	auto v = makeBlank(VarType::EntityRef);
	v.ent.id = id.pack();
	v.ent.schema = schema;
	return v;
}

Var makeEnum(int64_t value, TypeId family) {
	auto v = makeBlank(VarType::Enum);
	v.e.value = value;
	v.e.type = family;
	return v;
}

Var makeBlob(VarType t, ElementChain elem, BlobHandle h) {
	sprt_passert(isContainerType(t), "vstore::Var: makeBlob on a non-container type");
	auto v = makeBlank(t);
	v.c.blob = h;
	v.c.elem = elem;
	return v;
}

// How many floats the tag makes meaningful. Comparing all four for a Vec2 would make two logically
// equal values differ on bytes the type does not have.
static uint32_t getVectorArity(VarType t) {
	switch (t) {
	case VarType::Vec2: return 2;
	case VarType::Vec3: return 3;
	case VarType::Vec4:
	case VarType::Color: return 4;
	default: return 0;
	}
}

bool operator==(const Var &l, const Var &r) {
	if (l.type != r.type) {
		return false;
	}
	switch (l.type) {
	case VarType::Nil: return true;
	case VarType::Bool:
	case VarType::Int:
	case VarType::Int32:
	case VarType::UInt32: return l.i == r.i;
	case VarType::Float: return l.f == r.f;
	case VarType::Float32: return l.v[0] == r.v[0];
	case VarType::Vec2:
	case VarType::Vec3:
	case VarType::Vec4:
	case VarType::Color: {
		auto n = getVectorArity(l.type);
		for (uint32_t k = 0; k < n; ++k) {
			if (l.v[k] != r.v[k]) {
				return false;
			}
		}
		return true;
	}
	case VarType::EntityRef: return l.ent.id == r.ent.id && l.ent.schema == r.ent.schema;
	case VarType::Enum: return l.e.value == r.e.value && l.e.type == r.e.type;
	case VarType::String:
	case VarType::Bytes:
	case VarType::Array:
	case VarType::Map:
		// Handle identity, not content: comparing content needs the arena, and that is the blob
		// layer's job. Two handles to the same block are the same value here.
		return l.c.blob.data == r.c.blob.data && l.c.blob.size == r.c.blob.size
				&& l.c.elem == r.c.elem;
	}
	return false;
}

bool operator!=(const Var &l, const Var &r) { return !(l == r); }

bool varBytesEqual(const Var &l, const Var &r) {
	return __builtin_memcmp(&l, &r, sizeof(Var)) == 0;
}

// -0.0 and +0.0 have different bytes, and a NaN's payload bits are not canonical across platforms,
// so a value that reached us through data::Value must be refused rather than canonicalized:
// canonicalizing would silently change the author's data, which is worse than saying no.
static bool isStorableDouble(double d) {
	return d == d && d < __builtin_huge_val() && d > -__builtin_huge_val();
}

bool encodeVar(const Var &var, mem_std::Value &out) {
	switch (var.type) {
	case VarType::Nil: out = mem_std::Value(); return true;
	case VarType::Bool: out = mem_std::Value(var.i != 0); return true;
	case VarType::Int: out = mem_std::Value(var.i); return true;
	case VarType::Float: out = mem_std::Value(var.f); return true;
	case VarType::Int32:
	case VarType::UInt32: out = mem_std::Value(var.i); return true;
	case VarType::Float32: out = mem_std::Value(double(var.v[0])); return true;
	case VarType::Vec2:
	case VarType::Vec3:
	case VarType::Vec4:
	case VarType::Color: {
		auto n = getVectorArity(var.type);
		out = mem_std::Value(mem_std::Value::Type::ARRAY);
		for (uint32_t k = 0; k < n; ++k) { out.addDouble(double(var.v[k])); }
		return true;
	}
	case VarType::EntityRef: {
		auto id = EntityId::unpack(var.ent.id);
		out = mem_std::Value(mem_std::Value::Type::DICTIONARY);
		out.setInteger(int64_t(id.index), "index");
		out.setInteger(int64_t(id.generation), "generation");
		out.setInteger(int64_t(var.ent.schema), "schema");
		return true;
	}
	case VarType::Enum: {
		out = mem_std::Value(mem_std::Value::Type::DICTIONARY);
		out.setInteger(var.e.value, "value");
		out.setInteger(int64_t(var.e.type), "type");
		return true;
	}
	case VarType::String:
	case VarType::Bytes:
	case VarType::Array:
	case VarType::Map:
		// The content is in the arena; only the blob layer can project it.
		return false;
	}
	return false;
}

// The conversion matrix, rows = from, cols = to, in VarType order. var-cast holds a second copy of
// this drawn as characters and compares it cell by cell, so the two cannot drift apart. String ->
// Bytes is Same and Bytes -> String is Reject, which is the EntityRef/Int asymmetry again: reading
// the bytes out of a string drops a guarantee and can never harm, while calling arbitrary bytes
// "text" is a claim this layer cannot check, and one that surfaces later as a malformed projection.
//
//                Nil Bool Int Flt Vec2 Vec3 Vec4 Col Ent Str Arr Map Enum Byt I32 U32 F32
static constexpr CastRule R = CastRule::Reject;
static constexpr CastRule S = CastRule::Same;
static constexpr CastRule W = CastRule::Widen;
static constexpr CastRule N = CastRule::Narrow;
static constexpr CastRule P = CastRule::Parse;
static constexpr CastRule F = CastRule::Format;

static const CastRule s_castMatrix[VarTypeCount][VarTypeCount] = {
	/* Nil    */ {S, R, R, R, R, R, R, R, R, R, R, R, R, R, R, R, R},
	/* Bool   */ {R, S, W, W, R, R, R, R, R, F, R, R, R, R, W, W, W},
	/* Int    */ {R, N, S, N, R, R, R, R, R, F, R, R, N, R, N, N, N},
	/* Float  */ {R, N, N, S, R, R, R, R, R, F, R, R, R, R, N, N, N},
	/* Vec2   */ {R, R, R, R, S, W, W, R, R, R, R, R, R, R, R, R, R},
	/* Vec3   */ {R, R, R, R, N, S, W, R, R, R, R, R, R, R, R, R, R},
	/* Vec4   */ {R, R, R, R, N, N, S, S, R, R, R, R, R, R, R, R, R},
	/* Color  */ {R, R, R, R, N, N, S, S, R, R, R, R, R, R, R, R, R},
	/* Entity */ {R, R, N, R, R, R, R, R, S, R, R, R, R, R, R, R, R},
	/* String */ {R, P, P, P, R, R, R, R, R, S, R, R, P, S, P, P, P},
	/* Array  */ {R, R, R, R, R, R, R, R, R, R, S, R, R, R, R, R, R},
	/* Map    */ {R, R, R, R, R, R, R, R, R, R, R, S, R, R, R, R, R},
	/* Enum   */ {R, R, W, R, R, R, R, R, R, F, R, R, S, R, N, N, R},
	/* Bytes  */ {R, R, R, R, R, R, R, R, R, R, R, R, R, S, R, R, R},
	/* Int32  */ {R, N, W, W, R, R, R, R, R, F, R, R, N, R, S, N, N},
	/* UInt32 */ {R, N, W, W, R, R, R, R, R, F, R, R, N, R, N, S, N},
	/* Float32*/ {R, N, N, W, R, R, R, R, R, F, R, R, R, R, N, N, S},
};

CastRule getCastRule(VarType from, VarType to) {
	auto f = uint32_t(from);
	auto t = uint32_t(to);
	if (f >= VarTypeCount || t >= VarTypeCount) {
		return CastRule::Reject;
	}
	return s_castMatrix[f][t];
}

StringView getCastRuleName(CastRule r) {
	switch (r) {
	case CastRule::Reject: return StringView("reject");
	case CastRule::Same: return StringView("same");
	case CastRule::Widen: return StringView("widen");
	case CastRule::Narrow: return StringView("narrow");
	case CastRule::Parse: return StringView("parse");
	case CastRule::Format: return StringView("format");
	}
	return StringView("?");
}

bool castNeedsArena(VarType from, VarType to) {
	auto rule = getCastRule(from, to);
	if (rule == CastRule::Reject) {
		return false;
	}
	// Parse reads a string's bytes, Format allocates one, and a container's identity conversion is
	// a deep copy. Everything else is arithmetic on inline bytes.
	return rule == CastRule::Parse || rule == CastRule::Format || isContainerType(from)
			|| isContainerType(to);
}

static constexpr double MaxFloat32 = 3.4028234663852886e38;

// A narrowing into 32 bits refuses a value outside the target range under every policy.
static bool intToScalar32(int64_t i, VarType to, Var &out) {
	switch (to) {
	case VarType::Int32:
		if (i < -2'147'483'648LL || i > 2'147'483'647LL) {
			return false;
		}
		out = makeInt32(int32_t(i));
		return true;
	case VarType::UInt32:
		if (i < 0 || i > 4'294'967'295LL) {
			return false;
		}
		out = makeUInt32(uint32_t(i));
		return true;
	case VarType::Float32: out = makeFloat32(float(i)); return true;
	default: return false;
	}
}

// Truncates toward zero; a non-finite value or one outside the target range is refused.
static bool realToScalar32(double d, VarType to, Var &out) {
	if (!isStorableDouble(d)) {
		return false;
	}
	switch (to) {
	case VarType::Int32:
		if (!(d > -2'147'483'649.0 && d < 2'147'483'648.0)) {
			return false;
		}
		out = makeInt32(int32_t(d));
		return true;
	case VarType::UInt32:
		if (!(d > -1.0 && d < 4'294'967'296.0)) {
			return false;
		}
		out = makeUInt32(uint32_t(d));
		return true;
	case VarType::Float32:
		if (d > MaxFloat32 || d < -MaxFloat32) {
			return false;
		}
		out = makeFloat32(float(d));
		return true;
	default: return false;
	}
}

// The raw conversion, with no policy applied. Split out because Lossless needs to run the inverse
// cell to decide, and running it through the policy again would recurse.
static bool applyCastCell(const Var &from, VarType to, Var &out) {
	switch (from.type) {
	case VarType::Nil: out = makeNil(); return to == VarType::Nil;
	case VarType::Bool:
		switch (to) {
		case VarType::Bool: out = from; return true;
		case VarType::Int: out = makeInt(from.i != 0 ? 1 : 0); return true;
		case VarType::Float: out = makeFloat(from.i != 0 ? 1.0 : 0.0); return true;
		case VarType::Int32: out = makeInt32(from.i != 0 ? 1 : 0); return true;
		case VarType::UInt32: out = makeUInt32(from.i != 0 ? 1 : 0); return true;
		case VarType::Float32: out = makeFloat32(from.i != 0 ? 1.0f : 0.0f); return true;
		default: return false;
		}
	case VarType::Int:
		switch (to) {
		case VarType::Bool: out = makeBool(from.i != 0); return true;
		case VarType::Int: out = from; return true;
		case VarType::Float: out = makeFloat(double(from.i)); return true;
		// The family is unknown here; a declared field's setField stamps the descriptor's TypeId.
		case VarType::Enum: out = makeEnum(from.i); return true;
		case VarType::Int32:
		case VarType::UInt32:
		case VarType::Float32: return intToScalar32(from.i, to, out);
		default: return false;
		}
	case VarType::Float:
		switch (to) {
		case VarType::Bool: out = makeBool(from.f != 0.0); return true;
		case VarType::Int:
			if (!isStorableDouble(from.f) || !(from.f > -9.2233720368547758e18 && from.f < 9.2233720368547758e18)) {
				return false;
			}
			out = makeInt(int64_t(from.f));
			return true;
		case VarType::Float: out = from; return true;
		case VarType::Int32:
		case VarType::UInt32:
		case VarType::Float32: return realToScalar32(from.f, to, out);
		default: return false;
		}
	case VarType::Int32:
	case VarType::UInt32:
		switch (to) {
		case VarType::Bool: out = makeBool(from.i != 0); return true;
		case VarType::Int: out = makeInt(from.i); return true;
		case VarType::Float: out = makeFloat(double(from.i)); return true;
		case VarType::Enum: out = makeEnum(from.i); return true;
		case VarType::Int32:
		case VarType::UInt32:
		case VarType::Float32: return intToScalar32(from.i, to, out);
		default: return false;
		}
	case VarType::Float32:
		switch (to) {
		case VarType::Bool: out = makeBool(from.v[0] != 0.0f); return true;
		case VarType::Int:
			if (!isStorableDouble(double(from.v[0]))
					|| !(from.v[0] > -9.2233720e18f && from.v[0] < 9.2233720e18f)) {
				return false;
			}
			out = makeInt(int64_t(from.v[0]));
			return true;
		case VarType::Float: out = makeFloat(double(from.v[0])); return true;
		case VarType::Int32:
		case VarType::UInt32: return realToScalar32(double(from.v[0]), to, out);
		case VarType::Float32: out = from; return true;
		default: return false;
		}
	case VarType::Vec2:
	case VarType::Vec3:
	case VarType::Vec4:
	case VarType::Color:
		// Widening fills with zero, never with one: w = 1 (a homogeneous point) and alpha = 1 (an
		// opaque colour) are different operations, and an implicit cast must not guess which the
		// author meant. Vec2/Vec3 -> Color is refused outright for the same reason - alpha 0 would
		// silently make the value invisible.
		switch (to) {
		case VarType::Vec2: out = makeVec2(from.v[0], from.v[1]); return true;
		case VarType::Vec3: out = makeVec3(from.v[0], from.v[1], from.v[2]); return true;
		case VarType::Vec4: out = makeVec4(from.v[0], from.v[1], from.v[2], from.v[3]); return true;
		case VarType::Color:
			if (from.type != VarType::Vec4 && from.type != VarType::Color) {
				return false;
			}
			out = makeColor(from.v[0], from.v[1], from.v[2], from.v[3]);
			return true;
		default: return false;
		}
	case VarType::EntityRef:
		switch (to) {
		case VarType::EntityRef: out = from; return true;
		// Reading an id out for diagnostics is harmless; the inverse is refused so that a reference
		// cannot be forged out of arithmetic, which would defeat the generation check.
		case VarType::Int: out = makeInt(int64_t(from.ent.id)); return true;
		default: return false;
		}
	case VarType::Enum:
		switch (to) {
		case VarType::Enum: out = from; return true;
		case VarType::Int: out = makeInt(from.e.value); return true;
		case VarType::Int32:
		case VarType::UInt32: return intToScalar32(from.e.value, to, out);
		default: return false;
		}
	case VarType::String:
	case VarType::Bytes:
	case VarType::Array:
	case VarType::Map: return false; // needs an arena
	}
	return false;
}

Status castVar(const Var &from, VarType to, CastPolicy policy, Var &out) {
	auto rule = getCastRule(from.type, to);
	if (rule == CastRule::Reject) {
		return Status::ErrorInvalidArguemnt;
	}
	if (castNeedsArena(from.type, to)) {
		return Status::Declined;
	}

	Var result;
	if (!applyCastCell(from, to, result)) {
		return Status::ErrorInvalidArguemnt;
	}

	if (rule == CastRule::Narrow && policy == CastPolicy::Lossless) {
		// Exactness is demonstrated, not assumed: convert back and require the original. A cell
		// whose inverse is Reject can never demonstrate it and is therefore refused - which is the
		// right answer, since nothing else could establish that the value survived.
		Var back;
		if (getCastRule(to, from.type) == CastRule::Reject
				|| !applyCastCell(result, from.type, back) || back != from) {
			return Status::ErrorInvalidArguemnt;
		}
	}

	out = result;
	return Status::Ok;
}

Status decodeVar(const mem_std::Value &value, VarType type, Var &out) {
	switch (type) {
	case VarType::Nil: out = makeNil(); return Status::Ok;
	case VarType::Bool: out = makeBool(value.asBool()); return Status::Ok;
	case VarType::Int: out = makeInt(value.asInteger()); return Status::Ok;
	case VarType::Float: {
		auto d = value.asDouble();
		if (!isStorableDouble(d)) {
			return Status::ErrorInvalidArguemnt;
		}
		out = makeFloat(d);
		return Status::Ok;
	}
	case VarType::Int32:
	case VarType::UInt32:
		return intToScalar32(value.asInteger(), type, out) ? Status::Ok : Status::ErrorInvalidArguemnt;
	case VarType::Float32:
		return realToScalar32(value.asDouble(), type, out) ? Status::Ok : Status::ErrorInvalidArguemnt;
	case VarType::Vec2:
	case VarType::Vec3:
	case VarType::Vec4:
	case VarType::Color: {
		auto n = getVectorArity(type);
		if (!value.isArray() || value.size() != n) {
			return Status::ErrorInvalidArguemnt;
		}
		float f[4] = {0.0f, 0.0f, 0.0f, 0.0f};
		auto &arr = value.asArray();
		for (uint32_t k = 0; k < n; ++k) {
			auto d = arr[k].asDouble();
			if (!isStorableDouble(d) || d > MaxFloat32 || d < -MaxFloat32) {
				return Status::ErrorInvalidArguemnt;
			}
			f[k] = float(d);
		}
		switch (type) {
		case VarType::Vec2: out = makeVec2(f[0], f[1]); break;
		case VarType::Vec3: out = makeVec3(f[0], f[1], f[2]); break;
		case VarType::Vec4: out = makeVec4(f[0], f[1], f[2], f[3]); break;
		default: out = makeColor(f[0], f[1], f[2], f[3]); break;
		}
		return Status::Ok;
	}
	case VarType::EntityRef: {
		if (!value.isDictionary()) {
			return Status::ErrorInvalidArguemnt;
		}
		EntityId id{uint32_t(value.getInteger("index")), uint32_t(value.getInteger("generation"))};
		out = makeEntityRef(id, TypeId(value.getInteger("schema")));
		return Status::Ok;
	}
	case VarType::Enum: {
		if (!value.isDictionary()) {
			return Status::ErrorInvalidArguemnt;
		}
		out = makeEnum(value.getInteger("value"), TypeId(value.getInteger("type")));
		return Status::Ok;
	}
	case VarType::String:
	case VarType::Bytes:
	case VarType::Array:
	case VarType::Map: return Status::ErrorNotImplemented;
	}
	return Status::ErrorInvalidArguemnt;
}

// The two shapes meet if the conversion matrix admits the tags and the parameters of the type agree
// exactly. The matrix knows nothing about element chains or enum families - Array<Int> and
// Array<Float> are both `array` to it - so a table check alone would connect them.
TypeMeet valueTypesMeet(const ValueShape &from, const ValueShape &to, CastRule &outRule) {
	outRule = getCastRule(from.type, to.type);
	if (outRule != CastRule::Same && outRule != CastRule::Widen) {
		return TypeMeet::Tag;
	}

	if (from.type == to.type && from.element != to.element) {
		return TypeMeet::Element;
	}

	// An unconstrained destination takes anything; a constrained one takes exactly its own family,
	// including from a source that names none - this layer cannot prove that one would fit.
	if (to.subtype != NullTypeId && from.subtype != to.subtype) {
		return TypeMeet::Subtype;
	}
	return TypeMeet::Ok;
}

} // namespace stappler::flow::value
