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

// Subtask C5: strings, arrays and maps living in the arena, owned strictly by their component.
//
// Ownership is the property that makes the whole layer work without a garbage collector: a block
// has exactly ONE owner, copying a value copies the block, and destroying a component frees
// everything below it. Every fixture here ends by destroying what it built and asserting the arena
// is back at the byte it started from - a leak is a number, not a crash, and the number is the only
// way to see it.
//
// Two more invariants get checked directly rather than inferred:
//
//   BLOB-Z  [size, capacity) is zero at all times. The arena only zeroes the granule padding past
//           the size it was ASKED for, and this layer asks for capacity, so the slack is this
//           layer's to keep deterministic. Two blobs with the same content must have the same
//           bytes, or F1's byte-identical stores are unenforceable.
//
//   PTR-1   A resize is an allocator call, so no accessor may hold a pointer across one. A stale
//           pointer does NOT crash here - released chunks go to the arena's spare list, not the OS
//           - it quietly returns another block's bytes. Content comparison after every operation is
//           what catches that, which is why the torture pass exists.

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
	uint64_t total = 0;

	bool operator==(const Occupancy &) const = default;
};

Occupancy measure(const Arena &arena) {
	mem_std::Value dump;
	arena.describe(dump);
	auto &live = dump.getValue("live");
	return Occupancy{uint64_t(live.getInteger("count")), uint64_t(live.getInteger("bytes")),
		uint64_t(live.getInteger("total"))};
}

// Reads the whole block a handle points at, so the slack past `size` can be inspected.
bool slackIsZero(const Arena &arena, Addr handleAddr) {
	BlobHandle h;
	if (auto src = arena.read(handleAddr, uint32_t(sizeof(BlobHandle)))) {
		__sprt_memcpy(&h, src, sizeof(h));
	}
	if (h.data == NullAddr || h.capacity == h.size) {
		return true;
	}
	auto block = arena.read(h.data, h.capacity);
	if (!block) {
		return false;
	}
	for (uint32_t i = h.size; i < h.capacity; ++i) {
		if (block[i] != 0) {
			return false;
		}
	}
	return true;
}

BlobHandle handleOf(const Arena &arena, Addr handleAddr) {
	BlobHandle h;
	if (auto src = arena.read(handleAddr, uint32_t(sizeof(BlobHandle)))) {
		__sprt_memcpy(&h, src, sizeof(h));
	}
	return h;
}

} // namespace

void performSchemaBlobTests() {
	sprt::cout << "\n== flow value: blob fields ==\n";

	TypeRegistry reg;
	check(reg.init(), "schema-blob: registry init");

	Arena arena;
	check(arena.init(Config()), "schema-blob: store init");
	auto baseline = measure(arena);

	// 1. A string across every size boundary the allocator has: below MinPayload, across a Granule,
	//    and across MaxInChunkPayload into an oversize run - then all the way back down.
	{
		auto type = reg.createDerived("Text", [](mem_std::Vector<FieldDef> &out) {
			out.emplace_back(FieldDef{.name = "value", .type = VarType::String});
		});
		check(type != nullptr, "schema-blob: Text registers");

		auto inst = type->createInstance(arena);
		auto field = type->getField("value");
		auto handleAddr = inst + field->offset;

		check(blob::stringSize(arena, handleAddr) == 0 && slackIsZero(arena, handleAddr),
				"schema-blob: a fresh string is empty");

		const uint32_t sizes[] = {1, 8, MinPayload, MinPayload + 1, Granule * 4, 1'000, 65'519,
			MaxInChunkPayload, MaxInChunkPayload + 1, 200'000, 4'096, 16, 0};

		bool ok = true;
		bool sawOversize = false;
		for (auto size : sizes) {
			mem_std::String expect(size, '\0');
			for (uint32_t i = 0; i < size; ++i) { expect[i] = char('a' + (i % 26)); }

			if (blob::stringAssign(arena, handleAddr, StringView(expect.data(), size))
					!= Status::Ok) {
				ok = false;
				sprt::cout << "       assign of " << size << " failed\n";
				break;
			}
			auto got = blob::stringGet(arena, handleAddr);
			if (got != expect) {
				ok = false;
				sprt::cout << "       size " << size << " read back wrong\n";
				break;
			}
			if (!slackIsZero(arena, handleAddr)) {
				ok = false;
				sprt::cout << "       size " << size << " left non-zero slack\n";
				break;
			}
			if (handleOf(arena, handleAddr).size > MaxInChunkPayload) {
				sawOversize = true;
			}
		}
		check(ok, "schema-blob: a string round-trips across every allocator size boundary");
		check(sawOversize,
				"schema-blob: including past MaxInChunkPayload, where the arena uses a run");

		// Append walks the geometric growth rather than the exact-fit path.
		blob::stringAssign(arena, handleAddr, StringView());
		mem_std::String built;
		bool appended = true;
		for (uint32_t i = 0; i < 500 && appended; ++i) {
			auto piece = mem_std::toString(i, ",");
			appended = blob::stringAppend(arena, handleAddr, StringView(piece)) == Status::Ok;
			built.append(piece);
		}
		check(appended && blob::stringGet(arena, handleAddr) == built,
				"schema-blob: five hundred appends build the same string");
		check(slackIsZero(arena, handleAddr), "schema-blob: and leave the slack zero");

		type->freeInstance(arena, inst);
		check(measure(arena) == baseline,
				"schema-blob: destroying the component returns the arena to its baseline");
	}

	// 2. Arrays: resize both ways, element read-back at every index, erase from the middle.
	{
		auto type = reg.createDerived("Points", [](mem_std::Vector<FieldDef> &out) {
			out.emplace_back(FieldDef{.name = "items", .type = VarType::Array,
				.element = makeChain(VarType::Vec3)});
		});
		check(type != nullptr, "schema-blob: Points registers");

		auto inst = type->createInstance(arena);
		auto field = type->getField("items");
		auto handleAddr = inst + field->offset;
		auto chain = field->element;

		check(blob::arrayResize(arena, handleAddr, chain, 300) == Status::Ok,
				"schema-blob: an array grows");
		bool ok = true;
		for (uint32_t i = 0; i < 300 && ok; ++i) {
			ok = blob::arraySet(arena, handleAddr, chain, i,
						makeVec3(float(i), float(i * 2), float(i * 3)))
					== Status::Ok;
		}
		check(ok, "schema-blob: every element is writable");

		ok = true;
		for (uint32_t i = 0; i < 300 && ok; ++i) {
			Var value;
			ok = blob::arrayGet(arena, handleAddr, chain, i, value) == Status::Ok
					&& value == makeVec3(float(i), float(i * 2), float(i * 3));
		}
		check(ok, "schema-blob: and reads back at every index");
		check(blob::arrayCount(arena, handleAddr, chain) == 300, "schema-blob: the count is 300");
		check(slackIsZero(arena, handleAddr), "schema-blob: array slack is zero");

		// Erase from the front: everything shifts down by one and the tail is dropped.
		check(blob::arrayErase(arena, handleAddr, chain, 0) == Status::Ok,
				"schema-blob: an element erases");
		Var first;
		blob::arrayGet(arena, handleAddr, chain, 0, first);
		check(blob::arrayCount(arena, handleAddr, chain) == 299 && first == makeVec3(1.0f, 2.0f, 3.0f),
				"schema-blob: erasing the head shifts the rest down");
		check(slackIsZero(arena, handleAddr), "schema-blob: and leaves the freed tail zero");

		// Shrinking then regrowing must not resurrect the old values: the grown region is zero.
		blob::arrayResize(arena, handleAddr, chain, 10);
		blob::arrayResize(arena, handleAddr, chain, 20);
		Var regrown;
		blob::arrayGet(arena, handleAddr, chain, 15, regrown);
		check(regrown == makeVec3(0.0f, 0.0f, 0.0f),
				"schema-blob: a shrunk-then-regrown element reads as zero, not as what it held");

		check(blob::arraySet(arena, handleAddr, chain, 999, makeVec3(1, 2, 3)) != Status::Ok,
				"schema-blob: an out-of-range index is refused");
		check(blob::arraySet(arena, handleAddr, chain, 0, makeInt(1)) != Status::Ok,
				"schema-blob: and an element of the wrong type");

		// push and pop are the stack pair, and they exist as their own paths because they reach the
		// element having read the handle ONCE where the general spelling reads it five times. That is
		// an optimization, so what has to be pinned is that it is only that: the same elements, the
		// same count, and the same BLOB-Z at every step.
		blob::arrayResize(arena, handleAddr, chain, 0);
		ok = true;
		for (uint32_t i = 0; i < 100 && ok; ++i) {
			ok = blob::arrayPush(arena, handleAddr, chain, makeVec3(float(i), 0.0f, float(i)))
					== Status::Ok;
		}
		check(ok && blob::arrayCount(arena, handleAddr, chain) == 100,
				"schema-blob: a hundred pushes make a hundred elements");
		check(slackIsZero(arena, handleAddr), "schema-blob: and the slack behind them is zero");

		ok = true;
		for (uint32_t i = 100; i-- > 0 && ok;) {
			Var popped;
			ok = blob::arrayPop(arena, handleAddr, chain, popped) == Status::Ok
					&& popped == makeVec3(float(i), 0.0f, float(i))
					&& blob::arrayCount(arena, handleAddr, chain) == i;
		}
		check(ok, "schema-blob: and they pop off in reverse, one element shorter each time");
		check(slackIsZero(arena, handleAddr),
				"schema-blob: a pop leaves what it dropped zeroed, so BLOB-Z survives the stack");

		Var nothing;
		check(blob::arrayPop(arena, handleAddr, chain, nothing) == Status::ErrorNotFound,
				"schema-blob: popping an empty array is not-found rather than a failure");

		// Regrowing after the stack has drained must still read as zero: the pops are what put those
		// bytes back, and a pop that only moved the size would leave the old values to reappear.
		blob::arrayResize(arena, handleAddr, chain, 5);
		Var afterPops;
		blob::arrayGet(arena, handleAddr, chain, 3, afterPops);
		check(afterPops == makeVec3(0.0f, 0.0f, 0.0f),
				"schema-blob: and what the stack held does not come back with the size");

		type->freeInstance(arena, inst);
		check(measure(arena) == baseline, "schema-blob: the array's blocks are all released");
	}

	// 2a. A pop of a CONTAINER element frees the block that element owned. Left to the general path
	//     for the same reason arraySet refuses to assign over one, and worth its own case because a
	//     leak here is invisible until the arena runs out.
	{
		auto type = reg.createDerived("Rows", [](mem_std::Vector<FieldDef> &out) {
			out.emplace_back(FieldDef{.name = "rows", .type = VarType::Array,
				.element = makeChain(VarType::Array, VarType::Int)});
		});
		check(type != nullptr, "schema-blob: Rows registers");

		auto inst = type->createInstance(arena);
		auto field = type->getField("rows");
		auto handleAddr = inst + field->offset;
		auto chain = field->element;
		auto inner = makeChain(VarType::Int);

		check(blob::arrayResize(arena, handleAddr, chain, 4) == Status::Ok,
				"schema-blob: four nested rows");
		for (uint32_t i = 0; i < 4; ++i) {
			auto slot = blob::arrayElementAddr(arena, handleAddr, chain, i);
			for (uint32_t k = 0; k < 50; ++k) {
				blob::arrayPush(arena, slot, inner, makeInt(int64_t(k)));
			}
		}
		auto withRows = measure(arena);

		Var row;
		check(blob::arrayPop(arena, handleAddr, chain, row) == Status::Ok,
				"schema-blob: a nested row pops");
		check(blob::arrayCount(arena, handleAddr, chain) == 3, "schema-blob: three rows are left");
		check(measure(arena).count < withRows.count,
				"schema-blob: and the block it owned went back to the arena");

		type->freeInstance(arena, inst);
		check(measure(arena) == baseline, "schema-blob: the nested array releases everything");
	}

	// 3. THE sorted-spine proof. The same entries inserted in opposite orders must give byte-equal
	//    spines - which is what makes logical equality imply byte equality, and therefore what makes
	//    load->save->load converge and replay comparison meaningful.
	{
		auto type = reg.createDerived("Props", [](mem_std::Vector<FieldDef> &out) {
			out.emplace_back(FieldDef{.name = "props", .type = VarType::Map,
				.element = makeChain(VarType::Int)});
		});
		check(type != nullptr, "schema-blob: Props registers");

		const StringView keys[] = {"alpha", "beta", "gamma", "delta", "epsilon", "zeta", "eta",
			"theta", "a", "aa", "aaa", ""};
		const uint32_t keyCount = 12;

		auto forward = type->createInstance(arena);
		auto backward = type->createInstance(arena);
		auto field = type->getField("props");
		auto chain = field->element;

		for (uint32_t i = 0; i < keyCount; ++i) {
			blob::mapSet(arena, forward + field->offset, chain, keys[i], makeInt(int64_t(i)));
		}
		for (uint32_t i = keyCount; i > 0; --i) {
			blob::mapSet(arena, backward + field->offset, chain, keys[i - 1],
					makeInt(int64_t(i - 1)));
		}

		check(blob::mapCount(arena, forward + field->offset) == keyCount
						&& blob::mapCount(arena, backward + field->offset) == keyCount,
				"schema-blob: both maps hold every key");

		auto fh = handleOf(arena, forward + field->offset);
		auto bh = handleOf(arena, backward + field->offset);
		check(fh.size == bh.size, "schema-blob: the two spines are the same length");

		// Compare the VALUE half of every entry: the key handles point at different blocks, which
		// is an allocation detail, while the order and the values are the content.
		bool sameOrder = true;
		for (uint32_t i = 0; i < keyCount && sameOrder; ++i) {
			mem_std::String fk, bk;
			blob::mapKeyAt(arena, forward + field->offset, i,
					[&](StringView s) { fk.assign(s.data(), s.size()); });
			blob::mapKeyAt(arena, backward + field->offset, i,
					[&](StringView s) { bk.assign(s.data(), s.size()); });
			Var fv, bv;
			blob::mapValueAt(arena, forward + field->offset, i, fv);
			blob::mapValueAt(arena, backward + field->offset, i, bv);
			sameOrder = fk == bk && varBytesEqual(fv, bv);
			if (!sameOrder) {
				sprt::cout << "       entry " << i << ": '" << fk << "' vs '" << bk << "'\n";
			}
		}
		check(sameOrder,
				"schema-blob: opposite insertion orders give the same spine, entry for entry");

		// The order is by key bytes, so it is also the order data::Value's dictionary uses - which
		// is what lets the projection round-trip with no fixup.
		mem_std::Value projected;
		blob::encode(arena, forward + field->offset, VarType::Map, chain, projected);
		bool ascending = true;
		mem_std::String previous;
		for (auto &it : projected.asDict()) {
			if (!previous.empty() && StringView(it.first) < StringView(previous)) {
				ascending = false;
			}
			previous = it.first;
		}
		check(ascending && projected.size() == keyCount,
				"schema-blob: and the projection comes out in the same ascending order");

		Var found;
		check(blob::mapGet(arena, forward + field->offset, "gamma", found) == Status::Ok
						&& found == makeInt(2),
				"schema-blob: a key looks up to its value");
		check(blob::mapGet(arena, forward + field->offset, "nonesuch", found) == Status::ErrorNotFound,
				"schema-blob: an absent key is ErrorNotFound");

		blob::mapSet(arena, forward + field->offset, chain, "gamma", makeInt(99));
		blob::mapGet(arena, forward + field->offset, "gamma", found);
		check(found == makeInt(99) && blob::mapCount(arena, forward + field->offset) == keyCount,
				"schema-blob: setting an existing key replaces rather than duplicates");

		check(blob::mapErase(arena, forward + field->offset, "gamma") == Status::Ok
						&& blob::mapCount(arena, forward + field->offset) == keyCount - 1,
				"schema-blob: erasing a key removes exactly one entry");
		check(slackIsZero(arena, forward + field->offset), "schema-blob: map slack is zero");

		type->freeInstance(arena, forward);
		type->freeInstance(arena, backward);
		check(measure(arena) == baseline,
				"schema-blob: every key block and both spines are released");
	}

	// 4. NESTING to the full depth the element chain can express. This is the fixture that proves
	//    destroy() and copy() walk the tree rather than the top level: a missed branch is a leak
	//    that only the occupancy check below can see, and only if this ran.
	{
		auto type = reg.createDerived("Deep", [](mem_std::Vector<FieldDef> &out) {
			// Array<Array<Array<Int>>> - three container levels plus the leaf.
			out.emplace_back(FieldDef{.name = "grid", .type = VarType::Array,
				.element = makeChain(VarType::Array, VarType::Array, VarType::Int)});
			out.emplace_back(FieldDef{.name = "named", .type = VarType::Map,
				.element = makeChain(VarType::Array, VarType::String)});
		});
		check(type != nullptr, "schema-blob: Deep registers");

		auto inst = type->createInstance(arena);
		auto grid = type->getField("grid");
		auto named = type->getField("named");

		// Fill the three-level array: 4 x 3 x 5 integers.
		check(blob::arrayResize(arena, inst + grid->offset, grid->element, 4) == Status::Ok,
				"schema-blob: the outer array grows");
		bool filled = true;
		for (uint32_t i = 0; i < 4 && filled; ++i) {
			auto lvl1 = blob::arrayElementAddr(arena, inst + grid->offset, grid->element, i);
			auto chain1 = chainTail(grid->element);
			filled = blob::arrayResize(arena, lvl1, chain1, 3) == Status::Ok;
			for (uint32_t j = 0; j < 3 && filled; ++j) {
				// Re-resolved every time: the nested resize allocates, which moves the outer block.
				lvl1 = blob::arrayElementAddr(arena, inst + grid->offset, grid->element, i);
				auto lvl2 = blob::arrayElementAddr(arena, lvl1, chain1, j);
				auto chain2 = chainTail(chain1);
				filled = blob::arrayResize(arena, lvl2, chain2, 5) == Status::Ok;
				for (uint32_t k = 0; k < 5 && filled; ++k) {
					lvl1 = blob::arrayElementAddr(arena, inst + grid->offset, grid->element, i);
					lvl2 = blob::arrayElementAddr(arena, lvl1, chain1, j);
					filled = blob::arraySet(arena, lvl2, chain2, k,
								   makeInt(int64_t(i * 100 + j * 10 + k)))
							== Status::Ok;
				}
			}
		}
		check(filled, "schema-blob: three levels of nested arrays fill");

		// A map whose values are arrays of strings: the other nesting shape.
		for (uint32_t i = 0; i < 5; ++i) {
			auto key = mem_std::toString("row", i);
			auto placeholder = makeBlob(VarType::Array, chainTail(named->element), BlobHandle{});
			blob::mapSet(arena, inst + named->offset, named->element, StringView(key), placeholder);
			uint32_t at = 0;
			blob::mapFind(arena, inst + named->offset, StringView(key), at);
			auto slot = blob::mapValueAddr(arena, inst + named->offset, at);
			auto inner = chainTail(named->element);
			blob::arrayResize(arena, slot, inner, 3);
			for (uint32_t j = 0; j < 3; ++j) {
				blob::mapFind(arena, inst + named->offset, StringView(key), at);
				slot = blob::mapValueAddr(arena, inst + named->offset, at);
				auto strSlot = blob::arrayElementAddr(arena, slot, inner, j);
				auto text = mem_std::toString("cell-", i, "-", j);
				blob::stringAssign(arena, strSlot, StringView(text));
			}
		}

		// The projection reaches all the way down, which is the readable proof the tree is intact.
		mem_std::Value dump;
		type->encodeInstance(arena, inst, dump);
		check(dump.getValue("grid").size() == 4
						&& dump.getValue("grid").getValue(0).size() == 3
						&& dump.getValue("grid").getValue(0).getValue(0).size() == 5
						&& dump.getValue("grid").getValue(2).getValue(1).getValue(3).getInteger()
								== 213,
				"schema-blob: the projection reaches the leaf of a three-level array");
		check(dump.getValue("named").size() == 5
						&& dump.getValue("named").getValue("row2").getValue(1).getString()
								== "cell-2-1",
				"schema-blob: and into a map of arrays of strings");

		// A deep copy must produce an equal tree owned entirely by the destination.
		auto clone = type->createInstance(arena);
		check(type->copyInstance(arena, clone, arena, inst) == Status::Ok,
				"schema-blob: a nested instance deep-copies");
		mem_std::Value cloned;
		type->encodeInstance(arena, clone, cloned);
		check(test::compareValues(cloned, dump, "schema-blob-clone"),
				"schema-blob: and the copy projects identically");

		// Destroying the original must not disturb the copy: the two own separate blocks.
		type->freeInstance(arena, inst);
		mem_std::Value afterFree;
		type->encodeInstance(arena, clone, afterFree);
		check(test::compareValues(afterFree, dump, "schema-blob-independent"),
				"schema-blob: destroying the original leaves the copy untouched");

		type->freeInstance(arena, clone);
		check(measure(arena) == baseline,
				"schema-blob: a four-level tree destroys down to the last block");
	}

	// 5. Projection round trip through decode, including the nested shapes.
	{
		auto type = reg.createDerived("Mixed", [](mem_std::Vector<FieldDef> &out) {
			out.emplace_back(FieldDef{.name = "title", .type = VarType::String});
			out.emplace_back(FieldDef{.name = "counts", .type = VarType::Array,
				.element = makeChain(VarType::Int)});
			out.emplace_back(FieldDef{.name = "tags", .type = VarType::Array,
				.element = makeChain(VarType::String)});
			out.emplace_back(FieldDef{.name = "meta", .type = VarType::Map,
				.element = makeChain(VarType::String)});
			out.emplace_back(FieldDef{.name = "weight", .type = VarType::Float});
		});
		check(type != nullptr, "schema-blob: Mixed registers");

		mem_std::Value source(mem_std::Value::Type::DICTIONARY);
		source.setString("a title", "title");
		auto &counts = source.newArray("counts");
		for (int64_t i = 0; i < 6; ++i) { counts.addInteger(i * 11); }
		auto &tags = source.newArray("tags");
		tags.addString("red");
		tags.addString("green");
		tags.addString("blue");
		auto &meta = source.newDict("meta");
		meta.setString("author", "amstin");
		meta.setString("license", "unsettled");
		source.setDouble(2.5, "weight");

		auto inst = type->createInstance(arena);
		check(type->decodeInstance(arena, inst, source) == Status::Ok,
				"schema-blob: a nested value decodes into a record");

		mem_std::Value back;
		type->encodeInstance(arena, inst, back);
		check(test::compareValues(back, source, "schema-blob-roundtrip"),
				"schema-blob: and projects back to what it came from");

		// Decoding twice must not accumulate: the second decode replaces the containers rather
		// than appending to them.
		check(type->decodeInstance(arena, inst, source) == Status::Ok,
				"schema-blob: the same value decodes a second time");
		mem_std::Value twice;
		type->encodeInstance(arena, inst, twice);
		check(test::compareValues(twice, source, "schema-blob-idempotent"),
				"schema-blob: and the result is the same, not doubled");

		type->freeInstance(arena, inst);
		check(measure(arena) == baseline, "schema-blob: Mixed releases everything");
	}

	check(arena.verify() == Status::Ok, "schema-blob: the store is structurally sound throughout");

	// 6. THE TORTURE PASS. Two hundred thousand deterministic operations against a shadow model,
	//    compared after EVERY one.
	//
	//    Comparing after every operation rather than at the end is the point. A stale pointer -
	//    PTR-1's failure mode - does not crash and does not corrupt the structure; it returns
	//    another block's bytes. The only moment that is visible is the read immediately after the
	//    resize that moved the block, and a comparison at the end would have been overwritten by
	//    then. The budget is deliberately small so that relocation and run release are constant
	//    rather than occasional.
	{
		Config small;
		small.budgetBytes = uint32_t(4_MiB);

		Arena tortured;
		check(tortured.init(small), "schema-blob: torture store init");
		auto tortureBase = measure(tortured);

		auto type = reg.createDerived("Torture", [](mem_std::Vector<FieldDef> &out) {
			out.emplace_back(FieldDef{.name = "text", .type = VarType::String});
			out.emplace_back(FieldDef{.name = "nums", .type = VarType::Array,
				.element = makeChain(VarType::Int)});
			out.emplace_back(FieldDef{.name = "props", .type = VarType::Map,
				.element = makeChain(VarType::Int)});
		});
		check(type != nullptr, "schema-blob: Torture registers");

		// The shadow: what the arena is supposed to be holding, in host containers.
		struct Shadow {
			Addr instance = NullAddr;
			mem_std::String text;
			mem_std::Vector<int64_t> nums;
			mem_std::Map<mem_std::String, int64_t> props;
		};

		auto textField = type->getField("text");
		auto numsField = type->getField("nums");
		auto propsField = type->getField("props");

		mem_std::Vector<Shadow> live;
		test::Lcg lcg(0xb10b'7047'0000'0001ull);

		auto compare = [&](const Shadow &s) -> bool {
			if (blob::stringGet(tortured, s.instance + textField->offset) != s.text) {
				return false;
			}
			auto count = blob::arrayCount(tortured, s.instance + numsField->offset,
					numsField->element);
			if (count != s.nums.size()) {
				return false;
			}
			for (uint32_t i = 0; i < count; ++i) {
				Var value;
				if (blob::arrayGet(tortured, s.instance + numsField->offset, numsField->element, i,
							value)
								!= Status::Ok
						|| value.i != s.nums[i]) {
					return false;
				}
			}
			if (blob::mapCount(tortured, s.instance + propsField->offset) != s.props.size()) {
				return false;
			}
			for (auto &it : s.props) {
				Var value;
				if (blob::mapGet(tortured, s.instance + propsField->offset,
							StringView(it.first.data(), it.first.size()), value)
								!= Status::Ok
						|| value.i != it.second) {
					return false;
				}
			}
			// Slack has to be zero on every blob, at every moment - not just at the end.
			return slackIsZero(tortured, s.instance + textField->offset)
					&& slackIsZero(tortured, s.instance + numsField->offset)
					&& slackIsZero(tortured, s.instance + propsField->offset);
		};

		// A capped live set makes the workload stationary: without the cap it would simply grow
		// until the budget ran out, and the interesting churn would never happen.
		constexpr uint32_t LiveCap = 24;
		// Same trade as ecs-pool's torture: the reduced run walks a tenth of the ops with the same
		// shadow, the same slack check after every one of them, and the same seed.
		const uint32_t Ops = test::sized(200'000, 20'000);

		bool matches = true;
		bool healthy = true;
		uint32_t performed = 0;
		uint32_t oversizeSeen = 0;

		for (uint32_t op = 0; op < Ops && matches && healthy; ++op) {
			if (live.empty() || (live.size() < LiveCap && lcg.next(100) < 10)) {
				Shadow s;
				s.instance = type->createInstance(tortured);
				if (s.instance == NullAddr) {
					break;
				}
				live.emplace_back(sprt::move(s));
				++performed;
				continue;
			}

			auto index = lcg.next(uint32_t(live.size()));
			auto &s = live[index];

			switch (lcg.next(10)) {
			case 0: { // destroy and replace
				type->freeInstance(tortured, s.instance);
				live[index] = live.back();
				live.pop_back();
				break;
			}
			case 1: { // string assign, sometimes big enough to become an oversize run
				uint32_t size = lcg.next(100) < 4 ? 60'000 + lcg.next(80'000) : lcg.next(400);
				mem_std::String text(size, '\0');
				for (uint32_t i = 0; i < size; ++i) { text[i] = char('a' + ((i + op) % 26)); }
				blob::stringAssign(tortured, s.instance + textField->offset,
						StringView(text.data(), text.size()));
				s.text = text;
				if (size > MaxInChunkPayload) {
					++oversizeSeen;
				}
				break;
			}
			case 2: { // string append
				auto piece = mem_std::toString(op % 1'000, ";");
				blob::stringAppend(tortured, s.instance + textField->offset, StringView(piece));
				s.text.append(piece);
				break;
			}
			case 3: { // string truncate
				auto size = s.text.empty() ? 0 : lcg.next(uint32_t(s.text.size()));
				blob::stringResize(tortured, s.instance + textField->offset, size);
				s.text.resize(size);
				break;
			}
			case 4: { // array push
				int64_t value = int64_t(lcg.next());
				blob::arrayPush(tortured, s.instance + numsField->offset, numsField->element,
						makeInt(value));
				s.nums.emplace_back(value);
				break;
			}
			case 5: { // array resize either way
				uint32_t count = lcg.next(300);
				blob::arrayResize(tortured, s.instance + numsField->offset, numsField->element,
						count);
				s.nums.resize(count, 0);
				break;
			}
			case 6: { // array set
				if (!s.nums.empty()) {
					auto at = lcg.next(uint32_t(s.nums.size()));
					int64_t value = int64_t(lcg.next());
					blob::arraySet(tortured, s.instance + numsField->offset, numsField->element, at,
							makeInt(value));
					s.nums[at] = value;
				}
				break;
			}
			case 7: { // array erase
				if (!s.nums.empty()) {
					auto at = lcg.next(uint32_t(s.nums.size()));
					blob::arrayErase(tortured, s.instance + numsField->offset, numsField->element,
							at);
					s.nums.erase(s.nums.begin() + at);
				}
				break;
			}
			case 8: { // map set
				auto key = mem_std::toString("k", lcg.next(64));
				int64_t value = int64_t(lcg.next());
				blob::mapSet(tortured, s.instance + propsField->offset, propsField->element,
						StringView(key), makeInt(value));
				// find-then-assign rather than props[key] = value: the engine's sprt::__map
				// operator[] does not compile (access_token.h calls move_unsafe with two args).
				auto it = s.props.find(key);
				if (it != s.props.end()) {
					it->second = value;
				} else {
					s.props.emplace(key, value);
				}
				break;
			}
			default: { // map erase
				auto key = mem_std::toString("k", lcg.next(64));
				blob::mapErase(tortured, s.instance + propsField->offset, StringView(key));
				s.props.erase(key);
				break;
			}
			}
			++performed;

			// After EVERY operation - see the note above.
			if (index < live.size() && !compare(live[index])) {
				matches = false;
				sprt::cout << "       op " << op << " diverged from the shadow model\n";
				break;
			}
			if ((op % 5'000) == 0 && tortured.verify() != Status::Ok) {
				healthy = false;
				sprt::cout << "       store broken at op " << op << "\n";
				break;
			}
		}

		check(matches,
				mem_std::toString("schema-blob: ", performed,
						" operations agree with the shadow model, every step"));
		check(healthy, "schema-blob: and the store stays structurally sound");
		check(oversizeSeen > 0,
				mem_std::toString("schema-blob: ", oversizeSeen,
						" of them pushed a blob into an oversize run"));

		mem_std::Value dump;
		tortured.describe(dump);
		sprt::cout << "       " << dump.getInteger("chunkCount") << " chunks, "
				   << dump.getInteger("oversizeRuns") << " oversize runs, "
				   << (uint64_t(dump.getValue("live").getInteger("total")) >> 10) << " KiB live\n";

		// The cost the flat-block decision introduces is oversize churn, and arena occupancy alone
		// cannot see it - a released run goes back to the spare list, so the chunk count is what
		// tells the story. Bounded as a regression tripwire, not as a hard requirement.
		//
		// The bound follows the walk's length for the reason arena-torture's fragmentation bound
		// does: churn that leaks accumulates per op, so a bound set for 200 000 ops is four times
		// too loose for 20 000 and would pass a leak the full run catches. Measured 50 chunks at
		// the full length and 16 at the reduced one; each bound is its figure with room to drift.
		const int64_t chunkBound = test::full() ? 200 : 64;
		check(dump.getInteger("chunkCount") < chunkBound,
				mem_std::toString("schema-blob: oversize churn stayed bounded (",
						dump.getInteger("chunkCount"), " chunks, bound ", chunkBound, ")"));

		for (auto &s : live) { type->freeInstance(tortured, s.instance); }
		check(measure(tortured) == tortureBase,
				mem_std::toString("schema-blob: after ", performed,
						" operations the arena is back at its baseline"));
		check(tortured.verify() == Status::Ok, "schema-blob: and verifies");
	}

	// 7. Bytes. The same storage as a String and a different claim about it, so what is under test
	//    is not the storage - fixture 1 already covered that - but the three places the claim shows:
	//    values that are not text survive intact, the projection is a BYTESTRING, and the type
	//    cannot be reached from a String by accident.
	{
		auto type = reg.createDerived("Binary", [](mem_std::Vector<FieldDef> &out) {
			out.emplace_back(FieldDef{.name = "blob", .type = VarType::Bytes});
			out.emplace_back(FieldDef{.name = "name", .type = VarType::String});
		});
		check(type != nullptr, "schema-blob: Binary registers");
		check(type->getField("blob")->size == uint32_t(sizeof(BlobHandle))
						&& type->getField("blob")->align == 4,
				"schema-blob: a Bytes field is a BlobHandle, like a String");

		// A Bytes field must not declare an element type: its element is implicit, as a String's is.
		{
			mem_std::Value diagnostic;
			sprt::cout << "       (the report below is expected)\n";
			auto bad = reg.createDerived("BadBinary", [](mem_std::Vector<FieldDef> &out) {
				out.emplace_back(FieldDef{.name = "blob", .type = VarType::Bytes,
					.element = makeChain(VarType::Int)});
			}, test::NumberSink(&diagnostic).get());
			check(bad == nullptr, "schema-blob: a Bytes field that declares an element is refused");
		}

		auto base = measure(arena);
		auto instance = type->createInstance(arena);
		check(instance != NullAddr, "schema-blob: Binary instance");
		auto handleAddr = instance + type->getField("blob")->offset;

		// Every byte value, embedded NULs and 0xff included. A string-shaped implementation that
		// stopped at a NUL or tried to validate UTF-8 fails right here.
		mem_std::Vector<uint8_t> payload;
		for (uint32_t i = 0; i < 512; ++i) { payload.emplace_back(uint8_t(i * 37 + (i >> 3))); }
		payload[0] = 0;
		payload[1] = 0xff;
		payload[100] = 0;

		check(blob::bytesAssign(arena, handleAddr, BytesView(payload.data(), payload.size()))
						== Status::Ok,
				"schema-blob: bytesAssign");
		check(blob::bytesSize(arena, handleAddr) == payload.size(), "schema-blob: and the size");

		bool exact = true;
		blob::bytesRead(arena, handleAddr, [&](BytesView data) {
			exact = data.size() == payload.size()
					&& __sprt_memcmp(data.data(), payload.data(), payload.size()) == 0;
		});
		check(exact, "schema-blob: every byte reads back, NULs and 0xff included");
		check(blob::bytesGet(arena, handleAddr).size() == payload.size(),
				"schema-blob: and bytesGet copies it out");

		check(blob::bytesAppend(arena, handleAddr, BytesView(payload.data(), 8)) == Status::Ok
						&& blob::bytesSize(arena, handleAddr) == payload.size() + 8,
				"schema-blob: bytesAppend extends it");
		check(blob::bytesResize(arena, handleAddr, 16) == Status::Ok
						&& blob::bytesSize(arena, handleAddr) == 16,
				"schema-blob: bytesResize truncates");
		check(slackIsZero(arena, handleAddr), "schema-blob: and BLOB-Z holds for a Bytes blob");

		// The projection. A String goes out as text and a Bytes as bytes, which is the entire
		// observable difference between them.
		{
			check(blob::bytesAssign(arena, handleAddr, BytesView(payload.data(), payload.size()))
							== Status::Ok,
					"schema-blob: refilled");
			blob::stringAssign(arena, instance + type->getField("name")->offset, "titled");

			mem_std::Value dump;
			type->encodeInstance(arena, instance, dump);
			check(dump.getValue("blob").isBytes(), "schema-blob: a Bytes field projects as bytes");
			check(dump.getValue("name").isString(), "schema-blob: and a String field as a string");
			check(dump.getValue("blob").getBytes().size() == payload.size(),
					"schema-blob: with every byte of it");

			auto second = type->createInstance(arena);
			check(type->decodeInstance(arena, second, dump) == Status::Ok,
					"schema-blob: and decodes back");
			bool same = false;
			blob::bytesRead(arena, second + type->getField("blob")->offset, [&](BytesView data) {
				same = data.size() == payload.size()
						&& __sprt_memcmp(data.data(), payload.data(), payload.size()) == 0;
			});
			check(same, "schema-blob: byte for byte");
			type->freeInstance(arena, second);
		}

		// CBOR carries a byte string natively. JSON does not: the engine's writer emits
		// "BASE64:<base64url>" and its reader hands that back as a CHARSTRING - so a Bytes field
		// would lose its value on the way home unless the decoder repairs it, which it does. A bare
		// string is still refused, so a typo in an asset fails instead of loading as data.
		{
			mem_std::Value dump;
			type->encodeInstance(arena, instance, dump);

			auto viaCbor = data::read<mem_std::Interface>(
					data::write<mem_std::Interface>(dump, data::EncodeFormat::Cbor));
			auto viaJson = data::read<mem_std::Interface>(
					data::toString<mem_std::Interface>(dump, false));

			check(viaCbor.getValue("blob").isBytes(),
					"schema-blob: CBOR round-trips the field as bytes");
			check(viaJson.getValue("blob").isString(),
					"schema-blob: JSON round-trips it as a BASE64 string, which is the problem");

			auto fromCbor = type->createInstance(arena);
			auto fromJson = type->createInstance(arena);
			check(type->decodeInstance(arena, fromCbor, viaCbor) == Status::Ok
							&& type->decodeInstance(arena, fromJson, viaJson) == Status::Ok,
					"schema-blob: both encodings decode");

			mem_std::Value a, b;
			type->encodeInstance(arena, fromCbor, a);
			type->encodeInstance(arena, fromJson, b);
			check(test::compareValues(b, a, "schema-blob"),
					"schema-blob: and produce the same record, so JSON is not a one-way trip");

			mem_std::Value plain;
			plain.setString(StringView("not base64"), "blob");
			auto refused = type->createInstance(arena);
			sprt::cout << "       (the report below is expected)\n";
			check(type->decodeInstance(arena, refused, plain) != Status::Ok,
					"schema-blob: a bare string is NOT taken as its own bytes");

			type->freeInstance(arena, fromCbor);
			type->freeInstance(arena, fromJson);
			type->freeInstance(arena, refused);
		}

		// Ownership: a Bytes field is a container, so destroyInstance has to free it. If
		// isContainerType had missed it, the record would leak its block and this is the number
		// that would say so.
		type->freeInstance(arena, instance);
		check(measure(arena) == base, "schema-blob: a Bytes field is released with its record");
	}

	// 8. Reading an array as a typed span. This is what storing arrays flat is FOR: an element is
	//    its own bytes, so a reader can walk the storage rather than build a Var per element.
	{
		auto type = reg.createDerived("Mesh", [](mem_std::Vector<FieldDef> &out) {
			out.emplace_back(FieldDef{.name = "points", .type = VarType::Vec3,
				.element = 0});
			out.emplace_back(FieldDef{.name = "verts", .type = VarType::Array,
				.element = makeChain(VarType::Vec3)});
			out.emplace_back(FieldDef{.name = "ids", .type = VarType::Array,
				.element = makeChain(VarType::Int)});
			out.emplace_back(FieldDef{.name = "weights", .type = VarType::Array,
				.element = makeChain(VarType::Float)});
			out.emplace_back(FieldDef{.name = "flags", .type = VarType::Array,
				.element = makeChain(VarType::Bool)});
			out.emplace_back(FieldDef{.name = "names", .type = VarType::Array,
				.element = makeChain(VarType::String)});
		});
		check(type != nullptr, "schema-blob: Mesh registers");

		auto base = measure(arena);
		auto instance = type->createInstance(arena);
		auto vertsAddr = instance + type->getField("verts")->offset;
		auto idsAddr = instance + type->getField("ids")->offset;
		auto weightsAddr = instance + type->getField("weights")->offset;
		auto flagsAddr = instance + type->getField("flags")->offset;
		auto namesAddr = instance + type->getField("names")->offset;

		constexpr uint32_t Count = 300;
		for (uint32_t i = 0; i < Count; ++i) {
			blob::arrayPush(arena, vertsAddr, makeChain(VarType::Vec3),
					makeVec3(float(i), float(i) * 2.0f, float(i) * 3.0f));
			blob::arrayPush(arena, idsAddr, makeChain(VarType::Int), makeInt(int64_t(i) * 7));
			blob::arrayPush(arena, weightsAddr, makeChain(VarType::Float),
					makeFloat(double(i) / 4.0));
			blob::arrayPush(arena, flagsAddr, makeChain(VarType::Bool), makeBool((i & 1) != 0));
		}
		blob::arrayResize(arena, namesAddr, makeChain(VarType::String), 4);

		// A vector is N floats because that is exactly how it is stored - so the span is 3 * count
		// long and the values arrive in declaration order with no unpacking anywhere.
		bool vecs = false;
		check(blob::arrayRead<float>(arena, vertsAddr, makeChain(VarType::Vec3),
					  [&](SpanView<float> span) {
			vecs = span.size() == Count * 3;
			for (uint32_t i = 0; i < Count && vecs; ++i) {
				vecs = span[i * 3] == float(i) && span[i * 3 + 1] == float(i) * 2.0f
						&& span[i * 3 + 2] == float(i) * 3.0f;
			}
		}), "schema-blob: an Array<Vec3> reads as a span of floats");
		check(vecs, "schema-blob: three floats per element, in order");

		bool ints = false;
		check(blob::arrayRead<int64_t>(arena, idsAddr, makeChain(VarType::Int),
					  [&](SpanView<int64_t> span) {
			ints = span.size() == Count;
			for (uint32_t i = 0; i < Count && ints; ++i) { ints = span[i] == int64_t(i) * 7; }
		}), "schema-blob: an Array<Int> reads as a span of int64");
		check(ints, "schema-blob: every element matches");

		bool doubles = false;
		check(blob::arrayRead<double>(arena, weightsAddr, makeChain(VarType::Float),
					  [&](SpanView<double> span) {
			doubles = span.size() == Count;
			for (uint32_t i = 0; i < Count && doubles; ++i) { doubles = span[i] == double(i) / 4.0; }
		}), "schema-blob: an Array<Float> reads as a span of double");
		check(doubles, "schema-blob: every element matches");

		bool bools = false;
		check(blob::arrayRead<uint8_t>(arena, flagsAddr, makeChain(VarType::Bool),
					  [&](SpanView<uint8_t> span) {
			bools = span.size() == Count;
			for (uint32_t i = 0; i < Count && bools; ++i) { bools = span[i] == ((i & 1) != 0 ? 1 : 0); }
		}), "schema-blob: an Array<Bool> reads as a span of bytes");
		check(bools, "schema-blob: every element matches");

		bool handles = false;
		check(blob::arrayRead<BlobHandle>(arena, namesAddr, makeChain(VarType::String),
					  [&](SpanView<BlobHandle> span) { handles = span.size() == 4; }),
				"schema-blob: an Array<String> reads as a span of handles");
		check(handles, "schema-blob: one handle per element");

		// THE check that makes the typed reader safe. An Array<Int> and an Array<Float> are both
		// eight bytes per element, so a size-based match would accept this and hand back 300
		// doubles reinterpreted from int64 - values that are wrong and look plausible.
		bool called = false;
		check(!blob::arrayRead<double>(arena, idsAddr, makeChain(VarType::Int),
					  [&](SpanView<double>) { called = true; }),
				"schema-blob: reading an Array<Int> as doubles is refused");
		check(!called, "schema-blob: and the callback is not reached");
		check(!blob::arrayRead<float>(arena, idsAddr, makeChain(VarType::Int),
					  [&](SpanView<float>) { called = true; }),
				"schema-blob: nor as floats");
		check(!called, "schema-blob: still not reached");

		// An empty array calls back with an empty span rather than not calling, the way stringRead
		// does - so a caller does not have to distinguish "empty" from "wrong type" twice.
		{
			auto empty = type->createInstance(arena);
			bool emptyCalled = false;
			check(blob::arrayRead<int64_t>(arena, empty + type->getField("ids")->offset,
						  makeChain(VarType::Int), [&](SpanView<int64_t> span) {
				emptyCalled = span.empty();
			}), "schema-blob: an empty array is still read");
			check(emptyCalled, "schema-blob: with an empty span");
			type->freeInstance(arena, empty);
		}

		// The raw form, for a hash or a codec: the same bytes, no element type needed.
		bool raw = false;
		check(blob::arrayReadRaw(arena, idsAddr, makeChain(VarType::Int), [&](BytesView data) {
			raw = data.size() == Count * sizeof(int64_t);
		}), "schema-blob: arrayReadRaw hands out the storage");
		check(raw, "schema-blob: sized to whole elements");

		type->freeInstance(arena, instance);
		check(measure(arena) == base, "schema-blob: the Mesh record is released");
	}
}

} // namespace STAPPLER_VERSIONIZED stappler
