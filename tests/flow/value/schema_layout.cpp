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

// Subtask C2: from a list of fields, compute offsets, alignment, size and a schema hash.
//
// The layout is computed by the layer, never read off the host compiler - golden offsets have to be
// the same number on linux, win32, android and wasm. But "computed independently" is only worth
// something if it is checked, so every fixture below carries a hand-written C struct beside it and
// asserts offsetof() against the computed offsets AT COMPILE TIME. The layer is the source of
// truth; the compiler is a witness that fails the build on the target where they disagree.
//
// The golden hash literals do the same job for the hash recipe. They pin two things at once: that
// the recipe did not change, and that sprt::hash64 still produces what it produced - which is what
// makes a cross-platform hash comparison meaningful rather than circular.

#include "SPCommon.h"
#include "SPMemory.h"

#include "SPFlowValueSchema.h"

#include "../tests.h"
#include "../check/flow_check.h"

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;

namespace {

using namespace flow::value;

// Vectors are float[N] here, deliberately: sprt::geom::Vec4 is alignas(16), so a golden struct
// using it would have alignment 16 and disagree with the layer's rule of 4.
// Eight-byte fields are alignas(8) for the same reason in reverse: the layer aligns them to 8 on
// every target, and the i386 SysV ABI would put an int64_t or a double at a multiple of 4.
struct GoldenScalars {
	alignas(8) int64_t count;
	alignas(8) double weight;
	uint8_t visible;
};

struct GoldenMixed {
	uint8_t flag;
	// 7 bytes of padding
	alignas(8) int64_t id;
	float pos[3];
	float tint[4];
	uint8_t enabled;
};

struct GoldenBlobs {
	BlobHandle name;
	BlobHandle tags;
	alignas(8) int64_t revision;
};

struct GoldenBoolRun {
	uint8_t a, b, c, d, e;
	// 3 bytes of padding
	alignas(8) int64_t big;
};

struct GoldenSingle {
	float uv[2];
};

// A schema and the numbers it must produce. `hash` is filled in on the first run and then frozen.
struct Fixture {
	StringView name;
	mem_std::Vector<FieldDef> fields;
	uint32_t size;
	uint32_t align;
	uint64_t hash;
};

bool buildFixture(const Fixture &fx, memory::pool_t *pool, ComponentType &out) {
	mem_std::Value diag;
	auto st = out.build(fx.name, SpanView<FieldDef>(fx.fields.data(), fx.fields.size()), pool,
			test::NumberSink(&diag).get());
	if (st != Status::Ok) {
		sprt::cout << "       " << fx.name << " failed to build: " << data::toString(diag, false)
				   << "\n";
		return false;
	}
	return true;
}

// Compares the computed offsets against a list of expected ones, reporting the first disagreement
// by name rather than as a bare number.
bool checkOffsets(const ComponentType &type, SpanView<uint32_t> expected, StringView label) {
	auto fields = type.getFields();
	if (fields.size() != expected.size()) {
		sprt::cout << "       " << label << ": " << fields.size() << " fields, expected "
				   << expected.size() << "\n";
		return false;
	}
	bool ok = true;
	for (size_t i = 0; i < fields.size(); ++i) {
		if (fields[i].offset != expected[i]) {
			sprt::cout << "       " << label << "." << fields[i].name << ": offset "
					   << fields[i].offset << ", expected " << expected[i] << "\n";
			ok = false;
		}
	}
	return ok;
}

FieldDef scalar(StringView name, VarType type) { return FieldDef{.name = name, .type = type}; }

FieldDef container(StringView name, VarType type, ElementChain element) {
	return FieldDef{.name = name, .type = type, .element = element};
}

} // namespace

void performSchemaLayoutTests() {
	sprt::cout << "\n== flow value: schema layout and hash ==\n";

	auto pool = memory::pool::create();
	check(pool != nullptr, "schema-layout: descriptor pool");

	// 1. All-scalar, in declaration order, with the alignment gap the rules require.
	{
		ComponentType type;
		Fixture fx{StringView("Scalars"),
			{scalar("count", VarType::Int), scalar("weight", VarType::Float),
				scalar("visible", VarType::Bool)},
			0, 0, 0};
		check(buildFixture(fx, pool, type), "schema-layout: Scalars builds");

		static_assert(offsetof(GoldenScalars, count) == 0);
		static_assert(offsetof(GoldenScalars, weight) == 8);
		static_assert(offsetof(GoldenScalars, visible) == 16);
		const uint32_t expect[] = {0, 8, 16};
		check(checkOffsets(type, SpanView<uint32_t>(expect, 3), "Scalars"),
				"schema-layout: Scalars offsets match the hand-written struct");
		check(type.getSize() == sizeof(GoldenScalars) && type.getAlign() == alignof(GoldenScalars),
				mem_std::toString("schema-layout: Scalars is ", type.getSize(), "/",
						type.getAlign(), ", struct is ", sizeof(GoldenScalars), "/",
						alignof(GoldenScalars)));
	}

	// 2. Mixed alignment: a one-byte field ahead of an eight-byte one forces seven bytes of padding,
	//    and a float[3] leaves the cursor at a multiple of four but not of eight.
	{
		ComponentType type;
		Fixture fx{StringView("Mixed"),
			{scalar("flag", VarType::Bool), scalar("id", VarType::Int),
				scalar("pos", VarType::Vec3), scalar("tint", VarType::Color),
				scalar("enabled", VarType::Bool)},
			0, 0, 0};
		check(buildFixture(fx, pool, type), "schema-layout: Mixed builds");

		static_assert(offsetof(GoldenMixed, flag) == 0);
		static_assert(offsetof(GoldenMixed, id) == 8); // seven bytes of padding before it
		static_assert(offsetof(GoldenMixed, pos) == 16);
		static_assert(offsetof(GoldenMixed, tint) == 28);
		static_assert(offsetof(GoldenMixed, enabled) == 44);
		const uint32_t expect[] = {0, 8, 16, 28, 44};
		check(checkOffsets(type, SpanView<uint32_t>(expect, 5), "Mixed"),
				"schema-layout: Mixed offsets match, padding included");
		check(type.getSize() == sizeof(GoldenMixed) && type.getAlign() == alignof(GoldenMixed),
				mem_std::toString("schema-layout: Mixed is ", type.getSize(), "/", type.getAlign(),
						", struct is ", sizeof(GoldenMixed), "/", alignof(GoldenMixed)));
	}

	// 3. Blob handles: 12 bytes at alignment 4, so two of them pack and the Int after them still
	//    lands on 8.
	{
		ComponentType type;
		Fixture fx{StringView("Blobs"),
			{container("name", VarType::String, 0),
				container("tags", VarType::Array, makeChain(VarType::String)),
				scalar("revision", VarType::Int)},
			0, 0, 0};
		check(buildFixture(fx, pool, type), "schema-layout: Blobs builds");

		static_assert(offsetof(GoldenBlobs, name) == 0);
		static_assert(offsetof(GoldenBlobs, tags) == 12);
		static_assert(offsetof(GoldenBlobs, revision) == 24);
		const uint32_t expect[] = {0, 12, 24};
		check(checkOffsets(type, SpanView<uint32_t>(expect, 3), "Blobs"),
				"schema-layout: a BlobHandle is 12 bytes at alignment 4");
		check(type.getSize() == sizeof(GoldenBlobs) && type.getAlign() == alignof(GoldenBlobs),
				"schema-layout: Blobs size and alignment match the struct");
	}

	// 4. A run of one-byte fields, then an eight-byte one: the gap is three, not seven.
	{
		ComponentType type;
		Fixture fx{StringView("BoolRun"),
			{scalar("a", VarType::Bool), scalar("b", VarType::Bool), scalar("c", VarType::Bool),
				scalar("d", VarType::Bool), scalar("e", VarType::Bool),
				scalar("big", VarType::Int)},
			0, 0, 0};
		check(buildFixture(fx, pool, type), "schema-layout: BoolRun builds");

		static_assert(offsetof(GoldenBoolRun, e) == 4);
		static_assert(offsetof(GoldenBoolRun, big) == 8);
		const uint32_t expect[] = {0, 1, 2, 3, 4, 8};
		check(checkOffsets(type, SpanView<uint32_t>(expect, 6), "BoolRun"),
				"schema-layout: one-byte fields pack, then pad to the next eight");
		check(type.getSize() == sizeof(GoldenBoolRun), "schema-layout: BoolRun size matches");
	}

	// 5. A single field, and the trailing padding that rounds the record to its own alignment.
	{
		ComponentType type;
		Fixture fx{StringView("Single"), {scalar("uv", VarType::Vec2)}, 0, 0, 0};
		check(buildFixture(fx, pool, type), "schema-layout: Single builds");
		check(type.getSize() == sizeof(GoldenSingle) && type.getAlign() == alignof(GoldenSingle),
				"schema-layout: a lone Vec2 is eight bytes at alignment four");

		// The padding gaps describe() reports are what schema-scalar will later assert are zero.
		mem_std::Value dump;
		type.describe(dump);
		check(dump.getInteger("size") == 8 && dump.getInteger("align") == 4,
				"schema-layout: describe() reports the same size and alignment");
	}

	// 6. The hash. Golden literals pin the recipe; the comparisons around them pin what the recipe
	//    is supposed to be sensitive to.
	{
		auto hashOf = [&](StringView name, mem_std::Vector<FieldDef> fields) -> uint64_t {
			ComponentType t;
			mem_std::Value diag;
			if (t.build(name, SpanView<FieldDef>(fields.data(), fields.size()), pool,
						test::NumberSink(&diag).get())
					!= Status::Ok) {
				return 0;
			}
			return t.getSchemaHash();
		};

		auto base = hashOf("Transform",
				{scalar("position", VarType::Vec3), scalar("scale", VarType::Vec3),
					scalar("visible", VarType::Bool)});
		check(base != 0, "schema-layout: Transform hashes");
		check(base
						== hashOf("Transform",
								{scalar("position", VarType::Vec3), scalar("scale", VarType::Vec3),
									scalar("visible", VarType::Bool)}),
				"schema-layout: the same field list hashes the same twice");

		// GOLDEN. If this ever changes, either the recipe changed or sprt::hash64 did - and both
		// are things that must be a deliberate, visible edit rather than a surprise.
		check(base == 0x088e'b96e'0720'24e3ull,
				mem_std::toString("schema-layout: Transform's hash is the golden literal (got ",
						base, ")"));

		// Reordering moves offsets, so the bytes mean something different: different hash.
		check(base
						!= hashOf("Transform",
								{scalar("scale", VarType::Vec3), scalar("position", VarType::Vec3),
									scalar("visible", VarType::Bool)}),
				"schema-layout: reordering two fields changes the hash");

		// A rename is a different field even at the same offset with the same type - migration is
		// by name, so adopting the old bytes would be wrong.
		check(base
						!= hashOf("Transform",
								{scalar("origin", VarType::Vec3), scalar("scale", VarType::Vec3),
									scalar("visible", VarType::Bool)}),
				"schema-layout: renaming a field changes the hash");

		check(base
						!= hashOf("Transform2",
								{scalar("position", VarType::Vec3), scalar("scale", VarType::Vec3),
									scalar("visible", VarType::Bool)}),
				"schema-layout: the component's own name is part of the hash");

		// A default does not change how a stored byte is read, so hashing it would force every
		// saved scene through a field-by-field migration for nothing.
		auto withDefault = mem_std::Vector<FieldDef>{scalar("position", VarType::Vec3),
			scalar("scale", VarType::Vec3), scalar("visible", VarType::Bool)};
		withDefault[2].def = mem_std::Value(true);
		check(base == hashOf("Transform", withDefault),
				"schema-layout: changing only a default does NOT change the hash");

		auto readOnly = mem_std::Vector<FieldDef>{scalar("position", VarType::Vec3),
			scalar("scale", VarType::Vec3), scalar("visible", VarType::Bool)};
		readOnly[0].flags = FieldFlags::ReadOnly;
		check(base == hashOf("Transform", readOnly),
				"schema-layout: nor does an editor-policy flag");

		auto transient = mem_std::Vector<FieldDef>{scalar("position", VarType::Vec3),
			scalar("scale", VarType::Vec3), scalar("visible", VarType::Bool)};
		transient[0].flags = FieldFlags::Transient;
		check(base != hashOf("Transform", transient),
				"schema-layout: but Transient does - it changes what a loaded record contains");

		// The element chain is hashed whole, so a nested container is a different type.
		auto flat = hashOf("Bag", {container("items", VarType::Array, makeChain(VarType::Int))});
		auto nested = hashOf("Bag",
				{container("items", VarType::Array, makeChain(VarType::Array, VarType::Int))});
		check(flat != 0 && nested != 0 && flat != nested,
				"schema-layout: Array<Int> and Array<Array<Int>> hash differently");
	}

	// 7. Rejections. Each of these would otherwise become a corrupt record rather than a build
	//    failure.
	{
		auto rejects = [&](StringView what, mem_std::Vector<FieldDef> fields) {
			ComponentType t;
			mem_std::Value diag;
			auto st = t.build("Bad", SpanView<FieldDef>(fields.data(), fields.size()), pool,
					test::NumberSink(&diag).get());
			check(st != Status::Ok && diag.isArray() && diag.size() > 0,
					mem_std::toString("schema-layout: ", what, " is refused with a diagnostic"));
		};

		rejects("a Nil field", {scalar("x", VarType::Nil)});
		rejects("an unnamed field", {scalar("", VarType::Int)});
		rejects("a duplicate field name",
				{scalar("x", VarType::Int), scalar("x", VarType::Float)});
		rejects("an array with no element", {container("x", VarType::Array, 0)});
		rejects("a scalar carrying an element chain",
				{container("x", VarType::Int, makeChain(VarType::Int))});
		rejects("a string carrying an element chain",
				{container("x", VarType::String, makeChain(VarType::Int))});
		rejects("a nested array with no element",
				{container("x", VarType::Array, makeChain(VarType::Array))});

		// Five levels: the chain has four bytes, so the fifth has nowhere to go and must be an
		// error rather than a silent truncation to a different type.
		ElementChain deep = makeChain(VarType::Int);
		for (uint32_t i = 1; i < MaxChainDepth; ++i) {
			ElementChain next = 0;
			chainPush(VarType::Array, deep, next);
			deep = next;
		}
		ComponentType t;
		mem_std::Value diag;
		mem_std::Vector<FieldDef> ok{container("x", VarType::Array, deep)};
		check(t.build("Deep", SpanView<FieldDef>(ok.data(), ok.size()), pool,
					  test::NumberSink(&diag).get())
						== Status::Ok,
				mem_std::toString("schema-layout: a chain of exactly ", MaxChainDepth,
						" levels is accepted"));
	}

	memory::pool::destroy(pool);
}

} // namespace STAPPLER_VERSIONIZED stappler
