/**
Copyright (c) 2026 Stappler Team <admin@stappler.org>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
**/

#include "SPCommon.h"
#include "SPMemory.h"

#include "SPVStoreArena.h"
#include "SPVStoreJournal.h"

#include "../tests.h"
#include "../check/vstore_check.h"

// The two chunk geometries as types: DefaultGeometry (~1016 MiB over 64 KiB chunks) beside
// LargeGeometry (~3.99 GiB over 128 KiB chunks), both with a descriptor table that fills chunk 0
// to the byte. What that filling means is the section's subject: a chunk 0 that holds no blocks at
// all, an image whose geometry is refused by a reader of the other one, and a journal that watches
// one store of each under a single version counter.
namespace STAPPLER_VERSIONIZED stappler {

using test::check;
using test::Lcg;

namespace {

using namespace vstore;

// A store's content, as the layer defines it: the bytes of its save() image.
template <typename A>
mem_std::Vector<uint8_t> takeImage(const A &arena) {
	mem_std::Vector<uint8_t> image;
	image.reserve(arena.saveSize());
	arena.save([&](const uint8_t *data, size_t size, bool zero) {
		if (zero) {
			image.resize(image.size() + size, 0);
		} else {
			image.insert(image.end(), data, data + size);
		}
	});
	return image;
}

// Blocks of several classes with holes between them, on a store of whatever geometry: the same
// shape arena-kinds fills, kept here so the section stands alone.
template <typename A>
void fill(A &arena, mem_std::Vector<Addr> &live) {
	Lcg rng(0xC0FF'EE11);
	for (uint32_t i = 0; i < 300; ++i) {
		auto size = 16 + (rng.next() % 3000);
		auto a = arena.alloc(size);
		if (a == NullAddr) {
			continue;
		}
		if (auto dst = arena.write(a, size)) {
			__sprt_memset(dst, uint8_t(i), size);
		}
		live.emplace_back(a);
	}
	for (uint32_t i = 0; i < live.size(); i += 3) {
		arena.free(live[i]);
		live[i] = NullAddr;
	}
}

// The one store every case below starts from: the geometry's own ceiling as its budget, so the
// descriptor table is at full capacity and chunk 0 holds no blocks. The budget only names the
// table's capacity - init() hosts chunk 0 and nothing else, so a full-capacity store costs the
// same 64/128 KiB a small one does.
template <typename A>
bool initFull(A &arena) {
	Config cfg;
	cfg.budgetBytes = uint32_t(A::GeometryType::MaxChunks) << A::GeometryType::ChunkShift;
	cfg.debugShadow = false;
	return arena.init(cfg);
}

} // namespace

void performArenaGeometryTests() {
	sprt::cout << "\n-- arena-geometry --\n";

	// 1. A full table is chunk 0 whole. firstDataOffset meets the chunk's end, the verifier accepts
	//    the empty chain for slot 0 alone, and the first allocation grows a chunk of its own rather
	//    than trying to carve the table. Both geometries, because both fill chunk 0 exactly.
	{
		ShadowArena arena;
		check(initFull(arena), "arena-geometry: a default store at full table capacity inits");
		check(arena.getMaxChunks() == DefaultGeometry::MaxChunks,
				"arena-geometry: and its table describes every chunk of the ceiling");
		check(arena.verify() == Status::Ok, "arena-geometry: the empty chunk 0 verifies");

		auto first = arena.alloc(64);
		check(first != NullAddr && (first >> DefaultGeometry::ChunkShift) == 1,
				"arena-geometry: the first allocation grows chunk 1, not chunk 0");
		check(arena.verify() == Status::Ok, "arena-geometry: and the store still verifies");
	}

	{
		LargeShadowArena arena;
		check(initFull(arena), "arena-geometry: a large store at full table capacity inits");
		check(arena.getMaxChunks() == LargeGeometry::MaxChunks
						&& arena.getCapacityBytes() == uint64_t(LargeGeometry::MaxChunks)
								<< LargeGeometry::ChunkShift,
				"arena-geometry: the large ceiling is 32700 chunks of 128 KiB");
		check(arena.verify() == Status::Ok, "arena-geometry: the large empty chunk 0 verifies");

		auto first = arena.alloc(64);
		check(first != NullAddr && (first >> LargeGeometry::ChunkShift) == 1,
				"arena-geometry: the large first allocation grows chunk 1 too");
	}

	// 2. A blob over the geometry's in-chunk ceiling takes a run of its own - two chunks for one
	//    byte past it - and releasing it gives the slots back whole.
	{
		LargeTrackedArena arena;
		check(initFull(arena), "arena-geometry: a large tracked store at full capacity");

		auto big = arena.alloc(LargeGeometry::ChunkSize - BlockHeaderSize + 1);
		check(big != NullAddr, "arena-geometry: a blob one byte past the chunk takes a run");
		check(arena.verify() == Status::Ok, "arena-geometry: the run verifies");
		arena.free(big);
		check(arena.verify() == Status::Ok, "arena-geometry: and releases back to whole slots");
	}

	// 3. The ceiling itself. A budget one chunk past the table's capacity is refused, in uint32_t
	//    arithmetic, for both geometries - the large ceiling fits a uint32_t budget with room to
	//    spare, which is part of what that geometry is.
	{
		Config over;
		over.budgetBytes = uint32_t((DefaultGeometry::MaxChunks + 1) << DefaultGeometry::ChunkShift);
		ShadowArena def;
		check(!def.init(over), "arena-geometry: a default budget past the table is refused");

		over.budgetBytes = uint32_t((LargeGeometry::MaxChunks + 1) << LargeGeometry::ChunkShift);
		check(over.budgetBytes > LargeGeometry::MaxChunks << LargeGeometry::ChunkShift,
				"arena-geometry: the refusing large budget is expressible in uint32_t");
		LargeShadowArena lg;
		check(!lg.init(over), "arena-geometry: and a large budget past the table is refused");
	}

	// 4. An image is a fact about its geometry and not about the reader's. An image of one
	//    geometry is refused by a store of the other at the header - chunkShift is the first thing
	//    compared - and within one geometry the round trip is byte for byte, at full capacity
	//    where chunk 0 is all table.
	{
		ShadowArena small;
		check(initFull(small), "arena-geometry: a full-capacity source store");
		mem_std::Vector<Addr> live;
		fill(small, live);
		auto image = takeImage(small);

		ShadowArena same;
		check(same.adopt(BytesView(image.data(), image.size())) == Status::Ok,
				"arena-geometry: its image adopts into the same geometry");
		check(same.verify() == Status::Ok && takeImage(same) == image,
				"arena-geometry: and round-trips byte for byte with an empty chunk 0");

		LargeShadowArena other;
		check(other.adopt(BytesView(image.data(), image.size())) != Status::Ok,
				"arena-geometry: and is refused by the large geometry at the header");

		LargeShadowArena large;
		check(initFull(large), "arena-geometry: a large full-capacity source store");
		fill(large, live);
		auto largeImage = takeImage(large);

		LargeShadowArena largeSame;
		check(largeSame.adopt(BytesView(largeImage.data(), largeImage.size())) == Status::Ok
						&& largeSame.verify() == Status::Ok
						&& takeImage(largeSame) == largeImage,
				"arena-geometry: the large image round-trips the same way");

		ShadowArena smallReader;
		check(smallReader.adopt(BytesView(largeImage.data(), largeImage.size())) != Status::Ok,
				"arena-geometry: and is refused by the default geometry too");
	}

	// 5. A live store moves within its geometry and never across one: adoptFrom hands the chunks
	//    over whole, and the addresses inside them mean what they meant.
	{
		LargeShadowArena src;
		check(initFull(src), "arena-geometry: a large store to move");
		mem_std::Vector<Addr> live;
		fill(src, live);
		auto before = takeImage(src);

		LargeTrackedArena dst;
		check(dst.adoptFrom(src) == Status::Ok && !src.isInitialized(),
				"arena-geometry: it moves into a tracked store of the same geometry");
		check(dst.verify() == Status::Ok && takeImage(dst) == before,
				"arena-geometry: holding exactly the bytes it was given");
	}

	// 6. An old image - maxChunks 8192, the 512 MiB ceiling this layer used to have - still
	//    adopts into the wider default table and still round-trips, because widening the ceiling
	//    changed no byte of the format: firstDataOffset for 8192 chunks is the number it always
	//    was.
	{
		Config old;
		old.budgetBytes = 8192 << DefaultGeometry::ChunkShift;
		old.debugShadow = false;
		ShadowArena legacy;
		check(legacy.init(old) && legacy.getMaxChunks() == 8192,
				"arena-geometry: a store with the old 8192-chunk ceiling inits");
		mem_std::Vector<Addr> live;
		fill(legacy, live);
		auto image = takeImage(legacy);

		ShadowArena modern;
		check(modern.adopt(BytesView(image.data(), image.size())) == Status::Ok,
				"arena-geometry: its image adopts into the wider table");
		check(modern.getMaxChunks() == 8192,
				"arena-geometry: and keeps the ceiling it was written with");
		check(modern.verify() == Status::Ok && takeImage(modern) == image,
				"arena-geometry: and round-trips byte for byte");
	}

	// 7. One journal, two geometries. The journal's chunk math comes from the watched store, not
	//    from this reader's constants - which only a store of each geometry under one version
	//    counter can tell: a rollback must restore the default store's 64 KiB chunks and the large
	//    store's 128 KiB ones by their own shapes, byte for byte.
	{
		ShadowArena small;
		LargeShadowArena large;
		check(initFull(small) && initFull(large),
				"arena-geometry: two full-capacity stores, one of each geometry");

		mem_std::Vector<Addr> smallLive, largeLive;
		fill(small, smallLive);
		fill(large, largeLive);
		auto smallImage = takeImage(small);
		auto largeImage = takeImage(large);

		Journal journal;
		check(journal.init() && journal.attach(&small) == 0 && journal.attach(&large) == 1,
				"arena-geometry: one journal watches both geometries at once");

		auto v0 = journal.commit();
		for (auto a : smallLive) {
			if (a != NullAddr) {
				__sprt_memset(small.write(a, 16), 0x5A, 16);
			}
		}
		for (auto a : largeLive) {
			if (a != NullAddr) {
				__sprt_memset(large.write(a, 16), 0xA5, 16);
			}
		}
		journal.commit();

		check(journal.rollback(v0) == Status::Ok,
				"arena-geometry: a rollback reaches both stores");
		check(takeImage(small) == smallImage && takeImage(large) == largeImage,
				"arena-geometry: and restores each geometry byte for byte");
		check(small.verify() == Status::Ok && large.verify() == Status::Ok,
				"arena-geometry: and both verify afterwards");
	}
}

} // namespace STAPPLER_VERSIONIZED stappler
