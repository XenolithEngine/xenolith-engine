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

// ARENA-P0: the store's first page is the header's, whole, and holds nothing else.
//
// It is a property that used to be true by accident. The data area begins after the chunk descriptor
// table, and at the default budget - 64 MiB, so 1024 slots, so a 4 KiB table - the table already runs
// past the first page. At a 1 MiB budget it did not, and the store's first block sat at offset 336.
// Nothing was wrong with that; what was wrong was building on it.
//
// Something now does. Root::firstPageUsed says how many bytes of that page can hold anything, and the
// journal reads it to bound a comparison it makes on nearly every version - the allocator's counters
// live in the header, so the header page is the page marked most often of all. A bound is only worth
// having if it is right at every budget, which is what the first half of this section pins; the
// second half pins that the bound tracks the store as it grows.

#include "SPCommon.h"
#include "SPMemory.h"

#include "SPVStoreArena.h"

#include "../tests.h"
#include "../check/vstore_check.h"

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;

namespace {

using namespace vstore;

// The lowest address the store ever hands out, by asking for the smallest blocks it will make until
// it refuses or the count is convincing. A block below PageSize would be a block in the header page.
Addr lowestAllocation(Arena &arena, uint32_t count) {
	Addr lowest = 0xffff'ffffu;
	for (uint32_t i = 0; i < count; ++i) {
		auto a = arena.alloc(MinPayload);
		if (a == NullAddr) {
			break;
		}
		if (a < lowest) {
			lowest = a;
		}
	}
	return lowest;
}

} // namespace

void performArenaHeaderTests() {
	sprt::cout << "\n== vstore: the header page ==\n";

	// 1. The floor holds at every budget, including the ones where the table alone would not reach a
	//    page. 256 KiB is four slots - sixteen bytes of table - and is the case that used to put the
	//    first block inside the header page.
	{
		struct Case {
			uint32_t budget;
			uint32_t slots;
		};
		const Case cases[] = {
			{uint32_t(128_KiB), 2},
			{uint32_t(256_KiB), 4},
			{uint32_t(4_MiB), 64},
			{uint32_t(64_MiB), 1'024},
		};

		bool floored = true, clear = true;
		for (auto &c : cases) {
			Config cfg;
			cfg.budgetBytes = c.budget;
			Arena arena;
			if (!arena.init(cfg) || arena.getMaxChunks() != c.slots) {
				floored = false;
				sprt::cout << "       budget " << c.budget << ": init or slot count is wrong\n";
				continue;
			}
			if (firstDataOffsetFor(c.slots) < PageSize) {
				floored = false;
				sprt::cout << "       budget " << c.budget << ": data would start at "
						   << firstDataOffsetFor(c.slots) << "\n";
			}
			// Enough allocations to fill chunk 0 several times over, so "the low addresses" are
			// really the ones the allocator prefers rather than the ones it has not reached.
			if (lowestAllocation(arena, 5'000) < PageSize) {
				clear = false;
				sprt::cout << "       budget " << c.budget << ": a block landed in the header page\n";
			}
			if (arena.verify() != Status::Ok) {
				floored = false;
			}
		}
		check(floored, "arena-header: the data area starts at a page boundary at every budget");
		check(clear, "arena-header: and no block is ever handed out below it");
	}

	// 2. The ceiling. It is the superblock plus the live part of the table, and the page beyond it is
	//    table capacity nothing has written - which is exactly what makes it safe to stop there.
	{
		Arena arena;
		check(arena.init(Config()), "arena-header: default store init");

		auto footprint = arena.getPageFootprint(0, 0);
		check(footprint == DescsOffset + uint32_t(sizeof(ChunkDesc)),
				mem_std::toString("arena-header: a one-chunk store uses ", footprint,
						" bytes of its first page"));
		check(footprint < PageSize, "arena-header: which is a small part of it");

		// Every OTHER page is a whole page. The bound is about the header and about nothing else.
		bool others = arena.getPageFootprint(0, 1) == PageSize
				&& arena.getPageFootprint(0, PagesPerChunk - 1) == PageSize;
		check(others, "arena-header: every other page of chunk 0 is a whole page");

		// 3. It tracks the store. Growing past a chunk adds a descriptor, and the ceiling moves with
		//    it - if it did not, the journal would stop comparing the very bytes that just changed.
		auto before = arena.getPageFootprint(0, 0);
		while (arena.getChunkCount() < 8) {
			if (arena.alloc(MaxInChunkPayload / 2) == NullAddr) {
				break;
			}
		}
		check(arena.getChunkCount() >= 8, "arena-header: the store grew to eight chunks");
		check(arena.getPageFootprint(0, 0)
						== DescsOffset + arena.getChunkCount() * uint32_t(sizeof(ChunkDesc)),
				"arena-header: and the ceiling grew with the descriptor table");
		check(arena.getPageFootprint(0, 0) > before, "arena-header: strictly, not by accident");
		check(arena.verify() == Status::Ok, "arena-header: verify agrees with the ceiling");
	}

	// 4. It is part of the image, not of the host. A store rebuilt from its own bytes reports the
	//    same ceiling, because a rollback restores it the same way and a reader has to be able to
	//    trust what it finds.
	{
		Arena arena;
		check(arena.init(Config()), "arena-header: image store init");
		for (uint32_t i = 0; i < 200; ++i) { arena.alloc(1'000); }

		mem_std::Vector<uint8_t> image;
		image.reserve(arena.saveSize());
		arena.save([&](const uint8_t *data, size_t size, bool zero) {
			if (zero) {
				image.resize(image.size() + size, 0);
			} else {
				image.insert(image.end(), data, data + size);
			}
		});

		Arena copy;
		check(copy.adopt(BytesView(image.data(), image.size())) == Status::Ok,
				"arena-header: the image adopts");
		check(copy.getPageFootprint(0, 0) == arena.getPageFootprint(0, 0),
				"arena-header: and carries its own ceiling");
		check(copy.verify() == Status::Ok, "arena-header: the copy verifies");
	}
}

} // namespace STAPPLER_VERSIONIZED stappler
