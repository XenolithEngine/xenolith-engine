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

// The write barrier, and the proof that nothing writes around it.
//
// The barrier is the seam group B attaches to: the journal learns what a version changed by reading
// the map the barrier fills. A single write that goes around it - most likely one of the allocator's
// own, into a block header or a free-list link - would leave a future rollback restoring the caller's
// bytes but not the allocator's state. That failure is silent: the store looks fine until the next
// allocation walks a list that no longer describes reality.
//
// So the case that matters most here is not the one about user writes. It is the one that performs
// five thousand allocator operations and no user writes at all, and then demands that every page
// which changed was marked.
//
// Two halves, and they are gated differently on purpose. The MAP is an ordinary product of the store
// - it exists wherever anything reads it - so the cases about what it contains run in a release build
// too. The SHADOW is the debug-only cross-check that the map is complete, and only it needs DEBUG.

#include "SPCommon.h"
#include "SPMemory.h"

#include "SPVStoreArena.h"

#include "../tests.h"
#include "../check/vstore_check.h"

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;
using stappler::test::Lcg;

namespace {

using namespace vstore;

// The map as a list of (slot, page), which is the shape every assertion below wants. The map itself
// is indexed by flat page number and says more than that - see subMask() - but a page is marked iff
// any sub-block of it is, and most of what follows is about pages.
mem_std::Vector<sprt::pair<uint32_t, uint32_t>> markedPages(const Arena &arena) {
	mem_std::Vector<sprt::pair<uint32_t, uint32_t>> out;
	auto map = arena.getDirtyMap();
	for (uint32_t index = 0; index < map.size(); ++index) {
		if (map[index] != 0) {
			out.emplace_back(
					sprt::pair<uint32_t, uint32_t>(index / PagesPerChunk, index % PagesPerChunk));
		}
	}
	return out;
}

bool isMarked(const Arena &arena, uint32_t slot, uint32_t page) {
	auto map = arena.getDirtyMap();
	uint32_t index = slot * PagesPerChunk + page;
	return index < map.size() && map[index] != 0;
}

// The sub-block mask of one page, or zero when the store does not have it. Through dirtyMaskOf()
// rather than off the entry, so the assertions below hold under either encoding of it.
uint64_t subMask(const Arena &arena, uint32_t slot, uint32_t page) {
	auto map = arena.getDirtyMap();
	uint32_t index = slot * PagesPerChunk + page;
	return index < map.size() ? dirtyMaskOf(map[index]) : uint64_t(0);
}

} // namespace

void performArenaBarrierTests() {
	sprt::cout << "\n== vstore: arena barrier ==\n";

	Arena arena;
	check(arena.init(Config()), "arena-barrier: init");


	// 1. Page marking is exact. An oversize block starts at offset 0 of its run, so offsets into
	//    its payload map onto pages predictably - including across a chunk boundary, which is the
	//    case the inline fast path is most likely to get wrong, since that is precisely the case it
	//    hands off to the slow one.
	{
		Addr big = arena.alloc(3 * ChunkSize - 1000);
		check(big != NullAddr, "arena-barrier: oversize fixture allocated");

		struct Case {
			uint32_t at; // offset from the START of the run, not of the payload
			uint32_t size;
			uint32_t firstPage;
			uint32_t lastPage;
			uint32_t slots;
		};
		const Case cases[] = {
			{BlockHeaderSize, 16, 0, 0, 1}, // inside one page - the fast path
			{PageSize - 6, 16, 0, 1, 1}, // across a page boundary
			{PageSize, 2 * PageSize + 1, 1, 3, 1}, // three pages
			{2 * PageSize - 1, 1, 1, 1, 1}, // the last byte of a page
			{ChunkSize - 8, 16, PagesPerChunk - 1, 0, 2}, // across a chunk boundary
		};

		bool exact = true;
		for (auto &c : cases) {
			// The payload starts one header into the run, so a run offset maps to this address.
			Addr a = big + c.at - BlockHeaderSize;

			arena.clearDirtyMap();
			test::fillPattern(arena.write(a, c.size), c.size, a);

			auto marked = markedPages(arena);
			uint32_t expected = c.slots == 1 ? c.lastPage - c.firstPage + 1
											 : (PagesPerChunk - c.firstPage) + c.lastPage + 1;
			if (marked.size() != expected) {
				exact = false;
				sprt::cout << "       at " << c.at << " size " << c.size << ": marked "
						   << marked.size() << " pages, expected " << expected << "\n";
			} else if (marked.front().second != c.firstPage || marked.back().second != c.lastPage) {
				exact = false;
				sprt::cout << "       at " << c.at << " size " << c.size << ": pages "
						   << marked.front().second << ".." << marked.back().second << ", expected "
						   << c.firstPage << ".." << c.lastPage << "\n";
			}
		}
		check(exact, "arena-barrier: a write marks exactly the pages it touches");

		// A zero-length write is a legal thing to ask for and must mark nothing - the fast path's
		// page test would underflow on it, which is why it is sent to the slow one.
		arena.clearDirtyMap();
		arena.write(big, 0);
		check(markedPages(arena).empty(), "arena-barrier: a zero-length write marks nothing");

		// 1b. And WHERE inside the page, which is the part a whole-page bit cannot say. The cases
		//     are the same three shapes as above, one page down: inside one sub-block, across a
		//     sub-block boundary, and a span that covers several whole ones. What is being checked
		//     is that the mask is exact in both directions - no bit missing, and none set that the
		//     write did not reach.
		{
			struct SubCase {
				uint32_t at; // offset from the START of the run
				uint32_t size;
				uint32_t firstSub;
				uint32_t lastSub;
			};
			const SubCase subCases[] = {
				{BlockHeaderSize, 16, 0, 0}, // one sub-block
				{SubSize - 6, 16, 0, 1}, // across a sub-block boundary
				{SubSize, 3 * SubSize, 1, 3}, // three whole ones
				{2 * SubSize - 1, 1, 1, 1}, // the last byte of a sub-block
				{PageSize - SubSize, SubSize, SubBlocksPerPage - 1, SubBlocksPerPage - 1},
			};

			bool exactSub = true;
			for (auto &c : subCases) {
				Addr a = big + c.at - BlockHeaderSize;

				arena.clearDirtyMap();
				test::fillPattern(arena.write(a, c.size), c.size, a);

				uint64_t expected = subRange(c.firstSub, c.lastSub);
				uint64_t got = subMask(arena, a >> ChunkShift, (a & ChunkMask) >> PageShift);
				if (got != expected) {
					exactSub = false;
					sprt::cout << "       at " << c.at << " size " << c.size << ": mask " << got
							   << ", expected " << expected << "\n";
				}
			}
			check(exactSub, "arena-barrier: and exactly the sub-blocks inside them");

			// A write that spans pages marks the tail of the first and the head of the last, not
			// both in full: the slow path has to carry the partial ends through, and getting that
			// wrong is invisible to any assertion counted in pages.
			arena.clearDirtyMap();
			Addr across = big + PageSize - SubSize - BlockHeaderSize;
			test::fillPattern(arena.write(across, 2 * SubSize), 2 * SubSize, across);
			uint32_t slot = across >> ChunkShift;
			uint32_t page = (across & ChunkMask) >> PageShift;
			bool ends = subMask(arena, slot, page)
							== subRange(SubBlocksPerPage - 1, SubBlocksPerPage - 1)
					&& subMask(arena, slot, page + 1) == subRange(0, 0);
			check(ends, "arena-barrier: a write across a page boundary marks only the two ends");
		}

		arena.free(big);
	}

	// 2. Growth marks the whole new run. A reader's baseline says zeros for a slot it has never
	//    seen, and the run is zeroed on arrival - but the DESCRIPTORS moved, and every page of the
	//    run may be written before the next boundary, so the mark has to cover it.
	{
		Arena watched;
		check(watched.init(Config()), "arena-barrier: growth store init");
		watched.clearDirtyMap();

		Addr big = watched.alloc(2 * ChunkSize);
		check(big != NullAddr, "arena-barrier: the oversize run allocated");
		uint32_t slot = big >> ChunkShift;

		bool whole = true;
		for (uint32_t k = 0; k < 3; ++k) {
			for (uint32_t p = 0; p < PagesPerChunk; ++p) {
				if (!isMarked(watched, slot + k, p)) {
					whole = false;
				}
			}
		}
		check(whole, "arena-barrier: taking a three-chunk run marks every page of it");

		// And releasing it marks them again: as far as the image is concerned those pages go from
		// content to zeros, which is a change like any other.
		watched.clearDirtyMap();
		watched.free(big);

		whole = true;
		for (uint32_t k = 0; k < 3; ++k) {
			for (uint32_t p = 0; p < PagesPerChunk; ++p) {
				if (!isMarked(watched, slot + k, p)) {
					whole = false;
				}
			}
		}
		check(whole, "arena-barrier: releasing the run marks every page of it too");
		check(watched.verify() == Status::Ok, "arena-barrier: the store survives the release");
	}

	// 3. An ordinary growth marks one chunk and not the store. The map is the journal's whole input,
	//    so a barrier that marked generously would cost a page of undo log per version per slot it
	//    was wrong about.
	{
		Arena watched;
		check(watched.init(Config()), "arena-barrier: counted store init");

		auto before = watched.getChunkCount();
		watched.clearDirtyMap();
		while (watched.getChunkCount() == before) { watched.alloc(4000); }

		bool onlyTheNewOnes = true;
		for (auto &it : markedPages(watched)) {
			if (it.first != 0 && it.first < before) {
				onlyTheNewOnes = false;
			}
		}
		check(onlyTheNewOnes,
				"arena-barrier: a growth marks the new chunk and the root, and no other slot");
	}

	// 4. The one door that writes WITHOUT marking. A rollback puts back bytes the journal already
	//    holds; marking them would make the version being restored into look as though it had
	//    written them, and the next boundary would record the restored state as somebody's undo.
	{
		Arena watched;
		check(watched.init(Config()), "arena-barrier: restore-door store init");
		Addr a = watched.alloc(64);
		check(a != NullAddr, "arena-barrier: the restore fixture allocated");

		watched.clearDirtyMap();
		auto page = watched.getPageForRestore(a >> ChunkShift, (a & ChunkMask) >> PageShift);
		check(page != nullptr, "arena-barrier: the restore door hands out the page");
		page[0] = uint8_t(page[0] ^ 0xFF);
		check(markedPages(watched).empty(), "arena-barrier: and writing through it marks nothing");
		page[0] = uint8_t(page[0] ^ 0xFF);
	}


#if DEBUG
	check(arena.hasShadow(), "arena-barrier: the shadow is on in a debug build");

	auto report = [](uint32_t slot, uint32_t page) {
		sprt::cout << "       unmarked change in slot " << slot << ", page " << page << "\n";
	};

	Lcg lcg(0xba77'1e00'0000'0001ull);

	// 5. User writes are marked.
	{
		mem_std::Vector<sprt::pair<Addr, uint32_t>> blocks;
		for (int i = 0; i < 200; ++i) {
			Addr a = arena.alloc(1 + lcg.next(400));
			blocks.emplace_back(sprt::pair<Addr, uint32_t>(a, arena.sizeOf(a)));
		}

		arena.resync();
		for (int i = 0; i < 5000; ++i) {
			auto &b = blocks[lcg.next(uint32_t(blocks.size()))];
			test::fillPattern(arena.write(b.first, b.second), b.second, b.first + uint32_t(i));
		}
		check(arena.validate(report) == Status::Ok, "arena-barrier: user writes are announced");

		for (auto &b : blocks) { arena.free(b.first); }
	}

	// 6. THE case: the allocator's own writes, with no user writes at all to hide behind.
	{
		mem_std::Vector<Addr> live;

		arena.resync();
		for (int i = 0; i < 5000; ++i) {
			auto roll = lcg.next(100);
			if (roll < 50 || live.empty()) {
				uint32_t size = 1 + lcg.next(2000);
				if (lcg.next(200) == 0) {
					size = MaxInChunkPayload + 1 + lcg.next(100000);
				}
				Addr a = arena.alloc(size);
				if (a != NullAddr) {
					live.emplace_back(a);
				}
			} else if (roll < 80) {
				size_t index = lcg.next(uint32_t(live.size()));
				arena.free(live[index]);
				live[index] = live.back();
				live.pop_back();
			} else {
				size_t index = lcg.next(uint32_t(live.size()));
				Addr moved = arena.realloc(live[index], 1 + lcg.next(2000));
				if (moved != NullAddr) {
					live[index] = moved;
				}
			}
		}
		check(arena.validate(report) == Status::Ok,
				"arena-barrier: the allocator's own metadata writes are announced");
		check(arena.verify() == Status::Ok, "arena-barrier: the store is still healthy");

		for (auto a : live) { arena.free(a); }
	}

	// 7. Growth: a fresh chunk is announced as a whole, because a reused slot's old contents are
	//    exactly what a rollback would have to put back.
	{
		arena.resync();
		auto before = arena.getChunkCount();
		while (arena.getChunkCount() == before) { arena.alloc(4000); }
		check(arena.validate(report) == Status::Ok, "arena-barrier: growth is announced");
	}

	// 8. The negative case. Without this, every check above would also pass against a validator
	//    that always said Ok.
	{
		Addr a = arena.alloc(64);
		arena.resync();

		auto p = arena.unbarrieredWriteForTesting(a, 1);
		uint8_t before = p[0];
		p[0] = uint8_t(before ^ 0xFF);
		// Read back and feed the result into the assertion, so the write cannot be optimized away.
		check(p[0] == uint8_t(before ^ 0xFF), "arena-barrier: the planted write happened");

		uint32_t badSlot = 0xffff'ffff, badPage = 0xffff'ffff;
		auto st = arena.validate([&](uint32_t slot, uint32_t page) {
			badSlot = slot;
			badPage = page;
		});
		check(st != Status::Ok, "arena-barrier: a write around the barrier is caught");
		check(badSlot == (a >> ChunkShift) && badPage == ((a & ChunkMask) >> PageShift),
				"arena-barrier: the report names the right page");

		// Put it back, and the store is clean again.
		p[0] = before;
		check(arena.validate(report) == Status::Ok, "arena-barrier: restoring the byte clears it");
		arena.free(a);
	}

#else
	sprt::cout << "       (release build: the shadow validator is compiled out, skipping)\n";
#endif
}

} // namespace STAPPLER_VERSIONIZED stappler
