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

// The 32-bit scalars: Int32, UInt32 and Float32 as names, as fields, as values, as
// conversions and as array elements.

#include "SPCommon.h"
#include "SPMemory.h"

#include "SPFlowValueSchema.h"
#include "SPFlowValueBlob.h"

#include "../tests.h"
#include "../check/flow_check.h"

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;

namespace {

using namespace flow::value;

bool casts(const Var &from, VarType to, CastPolicy policy, const Var &expect) {
	Var out;
	return castVar(from, to, policy, out) == Status::Ok && varBytesEqual(out, expect);
}

bool refuses(const Var &from, VarType to, CastPolicy policy = CastPolicy::Lossy) {
	Var out;
	return castVar(from, to, policy, out) != Status::Ok;
}

} // namespace

void performValueTypes32Tests() {
	sprt::cout << "\n== flow value: 32-bit scalars ==\n";

	// ---- names and storage ---------------------------------------------------------------------------

	{
		VarType t = VarType::Nil;
		check(readVarType(StringView("int32"), t) && t == VarType::Int32
						&& readVarType(StringView("uint32"), t) && t == VarType::UInt32
						&& readVarType(StringView("float32"), t) && t == VarType::Float32,
				"value-types32: the three names read back");
		check(uint32_t(VarType::Bytes) == 13 && uint32_t(VarType::Int32) == 14
						&& uint32_t(VarType::Float32) == 16,
				"value-types32: the ordinals are appended after Bytes");
		check(getTypeSize(VarType::Int32) == 4 && getTypeSize(VarType::UInt32) == 4
						&& getTypeSize(VarType::Float32) == 4 && getTypeAlign(VarType::Int32) == 4
						&& getTypeAlign(VarType::Float32) == 4,
				"value-types32: four bytes, aligned to four");
		check(!isContainerType(VarType::Int32) && !isContainerType(VarType::Float32),
				"value-types32: inline, not containers");
	}

	// ---- values: equality, bytes, projection ---------------------------------------------------------

	{
		check(makeInt32(-5) == makeInt32(-5) && makeInt32(-5) != makeInt(-5)
						&& makeUInt32(5) != makeInt32(5),
				"value-types32: a value is its tag and its payload");
		check(varBytesEqual(makeFloat32(1.5f), makeFloat32(1.5f)),
				"value-types32: equal values have equal bytes");

		const Var corpus[] = {makeInt32(0), makeInt32(-2'147'483'647 - 1), makeInt32(2'147'483'647),
			makeUInt32(0), makeUInt32(4'294'967'295u), makeFloat32(0.25f), makeFloat32(-3.0e38f)};
		const VarType types[] = {VarType::Int32, VarType::Int32, VarType::Int32, VarType::UInt32,
			VarType::UInt32, VarType::Float32, VarType::Float32};
		bool roundTrip = true;
		for (size_t i = 0; i < sizeof(corpus) / sizeof(corpus[0]); ++i) {
			mem_std::Value encoded;
			Var back;
			roundTrip = roundTrip && encodeVar(corpus[i], encoded)
					&& decodeVar(encoded, types[i], back) == Status::Ok
					&& varBytesEqual(back, corpus[i]);
		}
		check(roundTrip, "value-types32: encode -> decode is exact, byte for byte");

		Var out;
		check(decodeVar(mem_std::Value(int64_t(2'147'483'648LL)), VarType::Int32, out) != Status::Ok
						&& decodeVar(mem_std::Value(int64_t(-1)), VarType::UInt32, out) != Status::Ok
						&& decodeVar(mem_std::Value(1.0e39), VarType::Float32, out) != Status::Ok,
				"value-types32: a value outside the range is refused, not wrapped");
		mem_std::Value tooBig(mem_std::Value::Type::ARRAY);
		tooBig.addDouble(1.0);
		tooBig.addDouble(1.0e39);
		check(decodeVar(tooBig, VarType::Vec2, out) != Status::Ok,
				"value-types32: and a vector component beyond float is refused too");
	}

	// ---- conversions ---------------------------------------------------------------------------------

	{
		check(getCastRule(VarType::Int32, VarType::Int) == CastRule::Widen
						&& getCastRule(VarType::UInt32, VarType::Float) == CastRule::Widen
						&& getCastRule(VarType::Float32, VarType::Float) == CastRule::Widen
						&& getCastRule(VarType::Bool, VarType::Float32) == CastRule::Widen,
				"value-types32: the exact directions widen");
		check(getCastRule(VarType::Int, VarType::Int32) == CastRule::Narrow
						&& getCastRule(VarType::Int32, VarType::UInt32) == CastRule::Narrow
						&& getCastRule(VarType::Int32, VarType::Float32) == CastRule::Narrow
						&& getCastRule(VarType::Enum, VarType::Int32) == CastRule::Narrow,
				"value-types32: everything that can lose narrows");

		check(casts(makeInt32(-7), VarType::Int, CastPolicy::Lossless, makeInt(-7))
						&& casts(makeUInt32(4'294'967'295u), VarType::Int, CastPolicy::Lossless,
								makeInt(4'294'967'295LL))
						&& casts(makeFloat32(0.5f), VarType::Float, CastPolicy::Lossless,
								makeFloat(0.5)),
				"value-types32: widening keeps the value");

		check(casts(makeInt(2'147'483'647), VarType::Int32, CastPolicy::Lossless,
					  makeInt32(2'147'483'647))
						&& refuses(makeInt(2'147'483'648LL), VarType::Int32)
						&& refuses(makeInt(-1), VarType::UInt32)
						&& refuses(makeInt32(-1), VarType::UInt32)
						&& refuses(makeUInt32(2'147'483'648u), VarType::Int32),
				"value-types32: an integer outside the target range is refused under any policy");

		check(casts(makeFloat(-2.9), VarType::Int32, CastPolicy::Lossy, makeInt32(-2))
						&& refuses(makeFloat(-2.9), VarType::Int32, CastPolicy::Lossless)
						&& refuses(makeFloat(3.0e9), VarType::Int32)
						&& refuses(makeFloat(__builtin_nan("")), VarType::Int32)
						&& refuses(makeFloat32(-1.0f), VarType::UInt32),
				"value-types32: a real truncates toward zero, and NaN or out of range is refused");

		check(casts(makeFloat(0.1), VarType::Float32, CastPolicy::Lossy, makeFloat32(0.1f))
						&& refuses(makeFloat(0.1), VarType::Float32, CastPolicy::Lossless)
						&& casts(makeFloat(0.5), VarType::Float32, CastPolicy::Lossless,
								makeFloat32(0.5f))
						&& refuses(makeFloat(1.0e39), VarType::Float32),
				"value-types32: a double rounds into Float32 only when that is lossy-allowed");

		check(casts(makeEnum(9, makeTypeId("Dir")), VarType::Int32, CastPolicy::Lossy, makeInt32(9))
						&& refuses(makeEnum(int64_t(1) << 40), VarType::Int32),
				"value-types32: an enum narrows into Int32 within range");

		ValueShape from{VarType::Int32, 0, NullTypeId};
		ValueShape to{VarType::Int, 0, NullTypeId};
		CastRule rule = CastRule::Reject;
		check(valueTypesMeet(from, to, rule) == TypeMeet::Ok && rule == CastRule::Widen,
				"value-types32: an Int32 output meets an Int input");
		ValueShape back{VarType::Int32, 0, NullTypeId};
		check(valueTypesMeet(to, back, rule) == TypeMeet::Tag,
				"value-types32: and an Int output does not meet an Int32 input");
	}

	// ---- fields and arrays ---------------------------------------------------------------------------

	{
		TypeRegistry reg;
		check(reg.init(), "value-types32: registry init");
		auto type = reg.createDerived("Scalars32", [](mem_std::Vector<FieldDef> &out) {
			out.emplace_back(FieldDef{.name = "flag", .type = VarType::Bool});
			out.emplace_back(FieldDef{.name = "count", .type = VarType::Int32});
			out.emplace_back(FieldDef{.name = "mask", .type = VarType::UInt32});
			out.emplace_back(FieldDef{.name = "speed", .type = VarType::Float32});
			out.emplace_back(FieldDef{.name = "hits", .type = VarType::Array,
				.element = makeChain(VarType::Int32)});
		});
		check(type != nullptr, "value-types32: a component of 32-bit fields registers");
		if (!type) {
			return;
		}
		auto count = type->getField("count");
		auto mask = type->getField("mask");
		auto speed = type->getField("speed");
		check(count && mask && speed && count->offset == 4 && mask->offset == 8
						&& speed->offset == 12,
				"value-types32: the fields pack after a bool at four-byte steps");

		Arena arena;
		check(arena.init(Config()), "value-types32: store init");
		auto inst = type->createInstance(arena);

		Var back;
		check(type->setField(arena, inst, *count, makeInt32(-123)) == Status::Ok
						&& type->getField(arena, inst, *count, back) == Status::Ok
						&& varBytesEqual(back, makeInt32(-123)),
				"value-types32: an Int32 field round-trips");
		check(type->setField(arena, inst, *mask, makeUInt32(0xffff'fff0u)) == Status::Ok
						&& type->getField(arena, inst, *mask, back) == Status::Ok
						&& varBytesEqual(back, makeUInt32(0xffff'fff0u)),
				"value-types32: a UInt32 field round-trips");
		check(type->setField(arena, inst, *speed, makeFloat32(-2.5f)) == Status::Ok
						&& type->getField(arena, inst, *speed, back) == Status::Ok
						&& varBytesEqual(back, makeFloat32(-2.5f)),
				"value-types32: a Float32 field round-trips");
		check(type->setField(arena, inst, *count, makeInt(1)) != Status::Ok,
				"value-types32: an Int is not written into an Int32 field");

		auto hits = type->getField("hits");
		auto handle = inst + hits->offset;
		bool pushed = true;
		for (int32_t i = 0; i < 5; ++i) {
			pushed = pushed
					&& blob::arrayPush(arena, handle, hits->element, makeInt32(i * -3)) == Status::Ok;
		}
		int32_t sum = 0;
		bool read = blob::arrayRead<int32_t>(arena, handle, hits->element,
				[&](SpanView<int32_t> span) {
			for (auto v : span) { sum += v; }
		});
		check(pushed && read && sum == -30 && blob::arrayCount(arena, handle, hits->element) == 5,
				"value-types32: an Array<Int32> stores four bytes per element and reads as int32_t");

		mem_std::Value projected;
		type->encodeInstance(arena, inst, projected);
		auto second = type->createInstance(arena);
		mem_std::Value again;
		check(type->decodeInstance(arena, second, projected) == Status::Ok
						&& (type->encodeInstance(arena, second, again), true)
						&& test::compareValues(again, projected, "vstore-types32"),
				"value-types32: the record projects and decodes back to itself");

		type->freeInstance(arena, inst);
		type->freeInstance(arena, second);
	}

}

} // namespace STAPPLER_VERSIONIZED stappler