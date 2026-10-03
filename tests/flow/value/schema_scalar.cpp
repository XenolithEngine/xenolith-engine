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

// Subtask C4: typed get/set of a component's scalar fields, with every write going through the
// arena's barrier.
//
// The barrier check is the load-bearing one and it is why this section exists separately from
// schema-layout. A field written around the barrier reads back correctly and passes every
// functional assertion; what it breaks is a ROLLBACK, months later, in a way that looks like
// storage corruption rather than a missing write(). The shadow validator is the only thing that
// catches it at the moment it happens, so it is switched on here and asserted to be on - a release
// build reports a skip instead of a vacuous pass.

#include "SPCommon.h"
#include "SPMemory.h"

#include "SPFlowValueSchema.h"

#include "../tests.h"
#include "../check/flow_check.h"

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;
using stappler::test::Lcg;

namespace {

using namespace flow::value;

// One field of every scalar type, in an order that forces padding in three places.
const ComponentType *makeEverything(TypeRegistry &reg) {
	return reg.createDerived("Everything", [](mem_std::Vector<FieldDef> &out) {
		out.emplace_back(FieldDef{.name = "flag", .type = VarType::Bool});
		out.emplace_back(FieldDef{.name = "count", .type = VarType::Int});
		out.emplace_back(FieldDef{.name = "weight", .type = VarType::Float});
		out.emplace_back(FieldDef{.name = "uv", .type = VarType::Vec2});
		out.emplace_back(FieldDef{.name = "pos", .type = VarType::Vec3});
		out.emplace_back(FieldDef{.name = "rot", .type = VarType::Vec4});
		out.emplace_back(FieldDef{.name = "tint", .type = VarType::Color});
		out.emplace_back(FieldDef{.name = "owner", .type = VarType::EntityRef,
			.subtypeId = makeTypeId("Node")});
		out.emplace_back(FieldDef{.name = "blend", .type = VarType::Enum,
			.subtypeId = makeTypeId("BlendMode")});
	});
}

mem_std::Vector<uint8_t> readRecord(const Arena &arena, Addr instance, uint32_t size) {
	mem_std::Vector<uint8_t> out;
	if (auto src = arena.read(instance, size)) {
		out.assign(src, src + size);
	}
	return out;
}

} // namespace

void performSchemaScalarTests() {
	sprt::cout << "\n== flow value: scalar field accessors ==\n";

	TypeRegistry reg;
	check(reg.init(), "schema-scalar: registry init");
	auto type = makeEverything(reg);
	check(type != nullptr, "schema-scalar: Everything registers");
	if (!type) {
		return;
	}

	Arena arena;
	check(arena.init(Config()), "schema-scalar: store init");

	// 1. Round-trip every scalar type through set -> get.
	{
		auto inst = type->createInstance(arena);
		check(inst != NullAddr, "schema-scalar: an instance is created");

		mem_std::Vector<Var> values;
		values.emplace_back(makeBool(true));
		values.emplace_back(makeInt(-4'242));
		values.emplace_back(makeFloat(0.125));
		values.emplace_back(makeVec2(1.0f, 2.0f));
		values.emplace_back(makeVec3(3.0f, 4.0f, 5.0f));
		values.emplace_back(makeVec4(6.0f, 7.0f, 8.0f, 9.0f));
		values.emplace_back(makeColor(0.25f, 0.5f, 0.75f, 1.0f));
		values.emplace_back(makeEntityRef(EntityId{11, 2}, makeTypeId("Node")));
		values.emplace_back(makeEnum(3, makeTypeId("BlendMode")));

		bool ok = true;
		auto fields = type->getFields();
		for (size_t i = 0; i < fields.size() && ok; ++i) {
			if (type->setField(arena, inst, fields[i], values[i]) != Status::Ok) {
				ok = false;
				sprt::cout << "       set " << fields[i].name << " failed\n";
				break;
			}
			Var back;
			if (type->getField(arena, inst, fields[i], back) != Status::Ok
					|| !varBytesEqual(back, values[i])) {
				ok = false;
				sprt::cout << "       " << fields[i].name << " did not round-trip\n";
			}
		}
		check(ok, "schema-scalar: every scalar type round-trips through set and get");

		// And through the projection, which is what the later sections use as an oracle.
		mem_std::Value first;
		type->encodeInstance(arena, inst, first);

		auto second = type->createInstance(arena);
		check(type->decodeInstance(arena, second, first) == Status::Ok,
				"schema-scalar: the projection decodes back into a fresh instance");
		mem_std::Value again;
		type->encodeInstance(arena, second, again);
		check(test::compareValues(again, first, "schema-scalar"),
				"schema-scalar: encode -> decode -> encode is a fixed point");

		// Same values, same bytes: the record is a function of its content, not of the order the
		// fields were written in.
		check(test::compareBytes(
					  BytesView(readRecord(arena, second, type->getSize()).data(), type->getSize()),
					  BytesView(readRecord(arena, inst, type->getSize()).data(), type->getSize()),
					  "schema-scalar"),
				"schema-scalar: two instances with the same values have identical bytes");

		type->freeInstance(arena, inst);
		type->freeInstance(arena, second);
	}

	// 2. A fresh record is entirely zero, PADDING INCLUDED. Nothing else guarantees the gaps: the
	//    arena hands out dirty payload bytes, and initInstance's single whole-record memset is what
	//    covers them.
	{
		// Dirty the memory first, so a missing memset shows up rather than being masked by a
		// coincidentally clean block.
		mem_std::Vector<Addr> churn;
		for (uint32_t i = 0; i < 64; ++i) {
			auto a = arena.alloc(type->getSize());
			if (a != NullAddr) {
				test::fillPattern(arena.write(a, type->getSize()), type->getSize(), a);
				churn.emplace_back(a);
			}
		}
		for (auto a : churn) { arena.free(a); }

		auto inst = type->createInstance(arena);
		auto record = readRecord(arena, inst, type->getSize());
		bool allZero = true;
		for (auto b : record) {
			if (b != 0) {
				allZero = false;
				break;
			}
		}
		check(allZero, "schema-scalar: a fresh instance is zero from end to end, padding included");

		// The gaps describe() reports are where the layout rules say they are.
		mem_std::Value dump;
		type->describe(dump);
		uint32_t padBytes = 0;
		for (auto &f : dump.getArray("fields")) {
			if (f.getString("name") == "<padding>") {
				padBytes += uint32_t(f.getInteger("size"));
			}
		}
		check(padBytes > 0,
				mem_std::toString("schema-scalar: the layout really has padding to cover (",
						padBytes, " bytes)"));

		type->freeInstance(arena, inst);
	}

	// 3. Refusals. Each of these would put a value in a field that cannot hold it.
	{
		auto inst = type->createInstance(arena);
		auto before = readRecord(arena, inst, type->getSize());

		check(type->setField(arena, inst, "count", makeFloat(1.0)) != Status::Ok,
				"schema-scalar: setField refuses a value of the wrong type");
		check(type->setField(arena, inst, "nonesuch", makeInt(1)) == Status::ErrorNotFound,
				"schema-scalar: and an unknown field name");

		// The descriptor is authoritative for a declared field's identity. A Var carrying a
		// different family is a mistake, not an override - if it were accepted, a heterogeneous
		// map's values and a declared field would disagree about what the same ordinal means.
		check(type->setField(arena, inst, "blend", makeEnum(1, makeTypeId("CullMode")))
						!= Status::Ok,
				"schema-scalar: an Enum carrying a foreign family is refused");
		check(type->setField(arena, inst, "blend", makeEnum(1, makeTypeId("BlendMode")))
						== Status::Ok,
				"schema-scalar: the declared family is accepted");
		check(type->setField(arena, inst, "blend", makeEnum(2)) == Status::Ok,
				"schema-scalar: and so is an unstamped one - setField stamps it");

		Var stamped;
		type->getField(arena, inst, "blend", stamped);
		check(stamped.e.type == makeTypeId("BlendMode"),
				"schema-scalar: a read carries the descriptor's family back out");

		// A refused write must not have touched the record.
		type->setField(arena, inst, "blend", makeEnum(0, makeTypeId("BlendMode")));
		auto after = readRecord(arena, inst, type->getSize());
		check(test::compareBytes(BytesView(after.data(), after.size()),
					  BytesView(before.data(), before.size()), "schema-scalar-untouched"),
				"schema-scalar: a refused write leaves the record byte-identical");

		type->freeInstance(arena, inst);
	}

	// 4. THE BARRIER. Twenty thousand writes through the accessors, then the shadow validator
	//    is asked whether any page changed without being announced.
#if DEBUG
	{
		check(arena.hasShadow(), "schema-scalar: the shadow validator is on in a debug build");

		mem_std::Vector<Addr> live;
		for (uint32_t i = 0; i < 64; ++i) {
			auto inst = type->createInstance(arena);
			if (inst != NullAddr) {
				live.emplace_back(inst);
			}
		}

		arena.resync();

		Lcg lcg(0x5ca1'a000'0000'0001ull);
		auto fields = type->getFields();
		for (uint32_t i = 0; i < 20'000; ++i) {
			auto &inst = live[lcg.next(uint32_t(live.size()))];
			auto &field = fields[lcg.next(uint32_t(fields.size()))];
			Var value;
			switch (field.type) {
			case VarType::Bool: value = makeBool((lcg.next(2)) != 0); break;
			case VarType::Int: value = makeInt(int64_t(lcg.next())); break;
			case VarType::Float: value = makeFloat(double(lcg.next(10'000)) / 7.0); break;
			case VarType::Vec2: value = makeVec2(float(lcg.next(100)), float(lcg.next(100))); break;
			case VarType::Vec3:
				value = makeVec3(float(lcg.next(100)), float(lcg.next(100)), float(lcg.next(100)));
				break;
			case VarType::Vec4:
				value = makeVec4(float(lcg.next(100)), float(lcg.next(100)), float(lcg.next(100)),
						float(lcg.next(100)));
				break;
			case VarType::Color:
				value = makeColor(float(lcg.next(100)), float(lcg.next(100)), float(lcg.next(100)),
						float(lcg.next(100)));
				break;
			case VarType::EntityRef:
				value = makeEntityRef(EntityId{lcg.next(1'000), lcg.next(8)}, field.subtypeId);
				break;
			case VarType::Enum: value = makeEnum(int64_t(lcg.next(8)), field.subtypeId); break;
			default: continue;
			}
			type->setField(arena, inst, field, value);
		}

		uint32_t unannounced = 0;
		auto st = arena.validate([&](uint32_t slot, uint32_t page) {
			if (unannounced < 4) {
				sprt::cout << "       slot " << slot << " page " << page
						   << " changed without being announced\n";
			}
			++unannounced;
		});
		check(st == Status::Ok && unannounced == 0,
				"schema-scalar: 20 000 accessor writes, every changed page announced");
		check(arena.verify() == Status::Ok, "schema-scalar: and the store is structurally sound");

		for (auto inst : live) { type->freeInstance(arena, inst); }
	}
#else
	sprt::cout << "       (release build: the shadow validator is compiled out, barrier check "
				  "skipped)\n";
#endif
}

} // namespace STAPPLER_VERSIONIZED stappler
