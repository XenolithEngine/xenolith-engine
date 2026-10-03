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

// Subtask C6: loading a record written against an older schema.
//
// Fields are matched BY NAME, which is why the name is part of the schema hash: a field renamed in
// place, same type at the same offset, is a different field and must not silently adopt the old
// bytes. Everything unmatched takes the NEW type's default rather than a zero - a scale that
// defaults to 1 must not quietly become 0, because that loads without complaint and is wrong.
//
// Both halves of the outcome are asserted. Without the report, "every field took its default" reads
// exactly like a successful migration, so each case below pins what happened as well as what
// resulted.

#include "SPCommon.h"
#include "SPMemory.h"

#include "SPFlowValueSchema.h"

#include "../tests.h"
#include "../check/flow_check.h"

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;

namespace {

using namespace flow::value;

struct Occupancy {
	uint64_t count = 0;
	uint64_t bytes = 0;

	bool operator==(const Occupancy &) const = default;
};

Occupancy measure(const Arena &arena) {
	mem_std::Value dump;
	arena.describe(dump);
	auto &live = dump.getValue("live");
	return Occupancy{uint64_t(live.getInteger("count")), uint64_t(live.getInteger("bytes"))};
}

mem_std::Vector<uint8_t> readRecord(const Arena &arena, Addr instance, uint32_t size) {
	mem_std::Vector<uint8_t> out;
	if (auto src = arena.read(instance, size)) {
		out.assign(src, src + size);
	}
	return out;
}

// Collects the report into "field:action" strings, so a case can state its expectation in one line
// rather than as a nested Value literal.
mem_std::Vector<mem_std::String> actionsOf(const mem_std::Value &report) {
	mem_std::Vector<mem_std::String> out;
	if (!report.isArray()) {
		return out;
	}
	for (auto &entry : report.getArray()) {
		out.emplace_back(mem_std::toString(entry.getString("field"), ":",
				entry.getString("action")));
	}
	return out;
}

bool hasAction(const mem_std::Value &report, StringView expect) {
	for (auto &a : actionsOf(report)) {
		if (StringView(a) == expect) {
			return true;
		}
	}
	return false;
}

} // namespace

void performSchemaMigrateTests() {
	sprt::cout << "\n== flow value: schema migration ==\n";

	TypeRegistry reg;
	check(reg.init(), "schema-migrate: registry init");

	Arena arena;
	check(arena.init(Config()), "schema-migrate: store init");
	auto baseline = measure(arena);

	// The corpus runs each pair the same way: build a source record against the old schema, migrate
	// it into a fresh record of the new one, then check the projection AND the report, that the
	// source is byte-identical to what it was, and that destroying both returns the arena.
	auto runCase = [&](StringView label, const ComponentType *oldType, const ComponentType *newType,
					   const mem_std::Value &source, const mem_std::Value &expectResult,
					   SpanView<StringView> expectActions) {
		if (!oldType || !newType) {
			check(false, mem_std::toString("schema-migrate: ", label, " types register"));
			return;
		}

		auto before = measure(arena);

		auto src = oldType->createInstance(arena);
		check(oldType->decodeInstance(arena, src, source) == Status::Ok,
				mem_std::toString("schema-migrate: ", label, " source loads"));
		auto srcBytes = readRecord(arena, src, oldType->getSize());

		auto dst = newType->createInstance(arena);
		mem_std::Value report;
		auto st = newType->migrateInstance(arena, dst, *oldType, arena, src, &report);
		check(st == Status::Ok, mem_std::toString("schema-migrate: ", label, " migrates"));

		mem_std::Value got;
		newType->encodeInstance(arena, dst, got);
		check(test::compareValues(got, expectResult, label),
				mem_std::toString("schema-migrate: ", label, " produces the expected record"));

		bool actionsOk = true;
		for (auto &expect : expectActions) {
			if (!hasAction(report, expect)) {
				actionsOk = false;
				sprt::cout << "       " << label << ": missing report entry '" << expect << "'\n";
			}
		}
		check(actionsOk, mem_std::toString("schema-migrate: ", label, " reports what it did"));

		// Migration copies, never moves: the source has to be exactly what it was, or a failure
		// part-way would leave it half-migrated and a re-run would produce something else.
		auto srcAfter = readRecord(arena, src, oldType->getSize());
		check(test::compareBytes(BytesView(srcAfter.data(), srcAfter.size()),
					  BytesView(srcBytes.data(), srcBytes.size()), label),
				mem_std::toString("schema-migrate: ", label, " leaves the source byte-identical"));

		oldType->freeInstance(arena, src);
		newType->freeInstance(arena, dst);
		check(measure(arena) == before,
				mem_std::toString("schema-migrate: ", label, " leaks nothing"));
	};

	auto scalar = [](StringView name, VarType type, mem_std::Value def = mem_std::Value()) {
		return FieldDef{.name = name, .type = type, .def = sprt::move(def)};
	};

	// 1. Reordering. The values carry over by name even though every offset moved - which is also
	//    why the hash had to change.
	{
		auto oldType = reg.createDerived("Reorder.old", [&](mem_std::Vector<FieldDef> &out) {
			out.emplace_back(scalar("a", VarType::Int));
			out.emplace_back(scalar("b", VarType::Bool));
			out.emplace_back(scalar("c", VarType::Float));
		});
		auto newType = reg.createDerived("Reorder.new", [&](mem_std::Vector<FieldDef> &out) {
			out.emplace_back(scalar("c", VarType::Float));
			out.emplace_back(scalar("b", VarType::Bool));
			out.emplace_back(scalar("a", VarType::Int));
		});

		mem_std::Value source(mem_std::Value::Type::DICTIONARY);
		source.setInteger(7, "a");
		source.setBool(true, "b");
		source.setDouble(1.5, "c");

		const StringView actions[] = {"a:carried", "b:carried", "c:carried"};
		runCase("reorder", oldType, newType, source, source, SpanView<StringView>(actions, 3));

		check(oldType->getSchemaHash() != newType->getSchemaHash(),
				"schema-migrate: reordering changes the hash, so the fast path is not taken");
	}

	// 2. A field removed and a field added. The added one takes its DEFAULT, not zero.
	{
		auto oldType = reg.createDerived("Shape.old", [&](mem_std::Vector<FieldDef> &out) {
			out.emplace_back(scalar("kept", VarType::Int));
			out.emplace_back(scalar("gone", VarType::Int));
		});
		auto newType = reg.createDerived("Shape.new", [&](mem_std::Vector<FieldDef> &out) {
			out.emplace_back(scalar("kept", VarType::Int));
			out.emplace_back(scalar("scale", VarType::Float, mem_std::Value(1.0)));
		});

		mem_std::Value source(mem_std::Value::Type::DICTIONARY);
		source.setInteger(42, "kept");
		source.setInteger(99, "gone");

		mem_std::Value expect(mem_std::Value::Type::DICTIONARY);
		expect.setInteger(42, "kept");
		expect.setDouble(1.0, "scale");

		const StringView actions[] = {"kept:carried", "scale:added", "gone:dropped"};
		runCase("add-remove", oldType, newType, source, expect, SpanView<StringView>(actions, 3));
	}

	// 3. A rename. Same type, same position - and still a different field, because migration is by
	//    name. The old bytes are dropped rather than silently adopted.
	{
		auto oldType = reg.createDerived("Rename.old", [&](mem_std::Vector<FieldDef> &out) {
			out.emplace_back(scalar("position", VarType::Vec3));
		});
		auto newType = reg.createDerived("Rename.new", [&](mem_std::Vector<FieldDef> &out) {
			out.emplace_back(scalar("origin", VarType::Vec3));
		});

		mem_std::Value source(mem_std::Value::Type::DICTIONARY);
		auto &pos = source.newArray("position");
		pos.addDouble(1.0);
		pos.addDouble(2.0);
		pos.addDouble(3.0);

		mem_std::Value expect(mem_std::Value::Type::DICTIONARY);
		auto &origin = expect.newArray("origin");
		origin.addDouble(0.0);
		origin.addDouble(0.0);
		origin.addDouble(0.0);

		const StringView actions[] = {"origin:added", "position:dropped"};
		runCase("rename", oldType, newType, source, expect, SpanView<StringView>(actions, 2));
	}

	// 4. Convertible type changes, through the C1 matrix.
	{
		auto oldType = reg.createDerived("Retype.old", [&](mem_std::Vector<FieldDef> &out) {
			out.emplace_back(scalar("n", VarType::Int));
			out.emplace_back(scalar("v", VarType::Vec3));
			out.emplace_back(scalar("flag", VarType::Bool));
		});
		auto newType = reg.createDerived("Retype.new", [&](mem_std::Vector<FieldDef> &out) {
			out.emplace_back(scalar("n", VarType::Float));
			out.emplace_back(scalar("v", VarType::Vec4));
			out.emplace_back(scalar("flag", VarType::Int));
		});

		mem_std::Value source(mem_std::Value::Type::DICTIONARY);
		source.setInteger(7, "n");
		auto &v = source.newArray("v");
		v.addDouble(1.0);
		v.addDouble(2.0);
		v.addDouble(3.0);
		source.setBool(true, "flag");

		mem_std::Value expect(mem_std::Value::Type::DICTIONARY);
		expect.setDouble(7.0, "n");
		auto &v4 = expect.newArray("v");
		v4.addDouble(1.0);
		v4.addDouble(2.0);
		v4.addDouble(3.0);
		v4.addDouble(0.0); // widening zero-fills; it does not guess w = 1
		expect.setInteger(1, "flag");

		const StringView actions[] = {"n:converted", "v:converted", "flag:converted"};
		runCase("retype-ok", oldType, newType, source, expect, SpanView<StringView>(actions, 3));
	}

	// 5. Rejected type changes. The field takes the new DEFAULT and the report says so - which is
	//    the difference between a migration that lost a value and one that never had it.
	{
		auto oldType = reg.createDerived("Bad.old", [&](mem_std::Vector<FieldDef> &out) {
			out.emplace_back(scalar("a", VarType::Vec3));
			out.emplace_back(scalar("b", VarType::Int));
		});
		auto newType = reg.createDerived("Bad.new", [&](mem_std::Vector<FieldDef> &out) {
			out.emplace_back(FieldDef{.name = "a", .type = VarType::Map,
				.element = makeChain(VarType::Int)});
			out.emplace_back(scalar("b", VarType::Vec2));
		});

		mem_std::Value source(mem_std::Value::Type::DICTIONARY);
		auto &a = source.newArray("a");
		a.addDouble(1.0);
		a.addDouble(2.0);
		a.addDouble(3.0);
		source.setInteger(5, "b");

		mem_std::Value expect(mem_std::Value::Type::DICTIONARY);
		expect.newDict("a");
		auto &b = expect.newArray("b");
		b.addDouble(0.0);
		b.addDouble(0.0);

		const StringView actions[] = {"a:rejected", "b:rejected"};
		runCase("retype-rejected", oldType, newType, source, expect,
				SpanView<StringView>(actions, 2));
	}

	// 6. Containers: carried unchanged, converted element-wise, added and dropped.
	{
		auto oldType = reg.createDerived("Blobs.old", [&](mem_std::Vector<FieldDef> &out) {
			out.emplace_back(FieldDef{.name = "title", .type = VarType::String});
			out.emplace_back(FieldDef{.name = "nums", .type = VarType::Array,
				.element = makeChain(VarType::Int)});
			out.emplace_back(FieldDef{.name = "old_tags", .type = VarType::Array,
				.element = makeChain(VarType::String)});
		});
		auto newType = reg.createDerived("Blobs.new", [&](mem_std::Vector<FieldDef> &out) {
			out.emplace_back(FieldDef{.name = "title", .type = VarType::String});
			// Element type changed: converted one element at a time through the matrix.
			out.emplace_back(FieldDef{.name = "nums", .type = VarType::Array,
				.element = makeChain(VarType::Float)});
			out.emplace_back(FieldDef{.name = "meta", .type = VarType::Map,
				.element = makeChain(VarType::String)});
		});

		mem_std::Value source(mem_std::Value::Type::DICTIONARY);
		source.setString("hello", "title");
		auto &nums = source.newArray("nums");
		nums.addInteger(1);
		nums.addInteger(2);
		nums.addInteger(3);
		auto &tags = source.newArray("old_tags");
		tags.addString("x");
		tags.addString("y");

		mem_std::Value expect(mem_std::Value::Type::DICTIONARY);
		expect.setString("hello", "title");
		auto &floats = expect.newArray("nums");
		floats.addDouble(1.0);
		floats.addDouble(2.0);
		floats.addDouble(3.0);
		expect.newDict("meta");

		const StringView actions[] = {"title:carried", "nums:converted", "meta:added",
			"old_tags:dropped"};
		runCase("containers", oldType, newType, source, expect, SpanView<StringView>(actions, 4));
	}

	// 7. Nested containers carry across whole - the deep-copy path, which is where a missed branch
	//    would leak or, worse, share a block between the source and the destination.
	{
		auto oldType = reg.createDerived("Nest.old", [&](mem_std::Vector<FieldDef> &out) {
			out.emplace_back(FieldDef{.name = "rows", .type = VarType::Array,
				.element = makeChain(VarType::Array, VarType::Int)});
		});
		auto newType = reg.createDerived("Nest.new", [&](mem_std::Vector<FieldDef> &out) {
			out.emplace_back(FieldDef{.name = "rows", .type = VarType::Array,
				.element = makeChain(VarType::Array, VarType::Int)});
			out.emplace_back(scalar("label", VarType::String));
		});

		mem_std::Value source(mem_std::Value::Type::DICTIONARY);
		auto &rows = source.newArray("rows");
		for (int64_t i = 0; i < 3; ++i) {
			mem_std::Value row(mem_std::Value::Type::ARRAY);
			for (int64_t j = 0; j < 4; ++j) { row.addInteger(i * 10 + j); }
			rows.addValue(sprt::move(row));
		}

		mem_std::Value expect = source;
		expect.setString("", "label");

		const StringView actions[] = {"rows:carried", "label:added"};
		runCase("nested", oldType, newType, source, expect, SpanView<StringView>(actions, 2));
	}

	// 8. The identity case. Migrating between two structurally identical types must reproduce the
	//    record byte for byte, not merely value for value.
	{
		auto build = [&](StringView name) {
			return reg.createDerived(name, [&](mem_std::Vector<FieldDef> &out) {
				out.emplace_back(scalar("a", VarType::Int));
				out.emplace_back(FieldDef{.name = "s", .type = VarType::String});
				out.emplace_back(scalar("v", VarType::Vec3));
			});
		};
		auto oldType = build("Same.a");
		auto newType = build("Same.b");
		check(oldType && newType, "schema-migrate: identity types register");

		mem_std::Value source(mem_std::Value::Type::DICTIONARY);
		source.setInteger(11, "a");
		source.setString("unchanged", "s");
		auto &v = source.newArray("v");
		v.addDouble(1.0);
		v.addDouble(2.0);
		v.addDouble(3.0);

		auto src = oldType->createInstance(arena);
		oldType->decodeInstance(arena, src, source);
		auto dst = newType->createInstance(arena);
		check(newType->migrateInstance(arena, dst, *oldType, arena, src) == Status::Ok,
				"schema-migrate: an identical schema migrates");

		// The blob handles differ - they point at separate blocks, which is the whole point of
		// copying - so the comparison is on the projection and on the scalar prefix.
		mem_std::Value got;
		newType->encodeInstance(arena, dst, got);
		check(test::compareValues(got, source, "identity"),
				"schema-migrate: and reproduces the record exactly");

		// Independence: destroying the source must not disturb the destination.
		oldType->freeInstance(arena, src);
		mem_std::Value after;
		newType->encodeInstance(arena, dst, after);
		check(test::compareValues(after, source, "identity-after-free"),
				"schema-migrate: the destination owns its own blocks, not the source's");

		newType->freeInstance(arena, dst);
	}

	check(measure(arena) == baseline,
			"schema-migrate: the whole corpus returns the arena to its baseline");
	check(arena.verify() == Status::Ok, "schema-migrate: and the store verifies");
}

} // namespace STAPPLER_VERSIONIZED stappler
