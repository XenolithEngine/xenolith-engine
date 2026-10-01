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

// Addresses stay valid across growth.
//
// This is the property the whole layer rests on. Every allocation gets a pattern derived from its
// own address written into it immediately; after the store has grown across many chunk boundaries,
// every block is read back and checked. If growth ever moved a chunk, or two blocks ever
// overlapped, the pattern is what notices - a plain "did alloc return non-null" test would not.

#include "SPCommon.h"
#include "SPMemory.h"

#include "SPVStoreArena.h"

#include "../tests.h"
#include "../check/vstore_check.h"

#include <sprt/c/__sprt_string.h>

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;
using stappler::test::Lcg;

namespace {

using namespace vstore;

struct Model {
	mem_std::Map<Addr, uint32_t> blocks; // address -> requested size, ordered for neighbour checks
	mem_std::Vector<Addr> live;

	// Checks against the immediate neighbours only: the map is ordered, so a block that overlaps
	// anything must overlap one of them. Done at insert time, which pins the failure to the
	// allocation that caused it rather than to a sweep at the end.
	bool add(Addr a, uint32_t size) {
		auto next = blocks.lower_bound(a);
		if (next != blocks.end() && a + size > next->first) {
			return false;
		}
		if (next != blocks.begin()) {
			auto prev = next;
			--prev;
			if (prev->first + prev->second > a) {
				return false;
			}
		}
		blocks.emplace(a, size);
		live.emplace_back(a);
		return true;
	}
};

} // namespace

void performArenaBasicTests() {
	sprt::cout << "\n== vstore: arena basic ==\n";

	Arena arena;
	check(arena.init(Config()), "arena-basic: init");
	check(arena.verify() == Status::Ok, "arena-basic: verify after init");

	Lcg lcg(0x1234'5678'9abc'def0ull);
	Model model;

	// 1. Allocate and immediately stamp each block with its own address.
	bool allNonNull = true;
	bool noOverlap = true;
	for (int i = 0; i < 10'000; ++i) {
		uint32_t size = 1 + lcg.next(512);
		Addr a = arena.alloc(size);
		if (a == NullAddr) {
			allNonNull = false;
			break;
		}
		// The arena rounds a sub-minimum request up; ask it what it actually reserved.
		uint32_t actual = arena.sizeOf(a);
		if (!model.add(a, actual)) {
			noOverlap = false;
			break;
		}
		test::fillPattern(arena.write(a, actual), actual, a);
	}
	check(allNonNull, "arena-basic: 10 000 allocations all succeed");
	check(noOverlap, "arena-basic: no allocation overlaps another");

	// 2. The test must actually have crossed chunk boundaries, or it proves nothing about growth.
	check(arena.getChunkCount() > 16,
			mem_std::toString("arena-basic: the store grew past 16 chunks (",
					arena.getChunkCount(), ")"));

	// 3. Re-read everything AFTER the growth.
	bool patternsHold = true;
	uint32_t badIndex = 0;
	Addr badAddr = NullAddr;
	for (auto &it : model.blocks) {
		if (!test::checkPattern(arena.read(it.first, it.second), it.second, it.first, &badIndex)) {
			patternsHold = false;
			badAddr = it.first;
			break;
		}
	}
	if (!patternsHold) {
		sprt::cout << "       first damaged block: addr " << badAddr << ", byte " << badIndex
				   << "\n";
	}
	check(patternsHold, "arena-basic: every block still reads back after growth");

	check(arena.verify() == Status::Ok, "arena-basic: verify after 10 000 allocations");

	// 4. resolve() is affine within a chunk: two addresses in the same chunk are exactly as far
	//    apart in memory as their addresses say.
	{
		bool affine = true;
		bool checkedAny = false;
		Addr prev = NullAddr;
		for (auto &it : model.blocks) {
			if (prev != NullAddr && (prev >> ChunkShift) == (it.first >> ChunkShift)) {
				auto d = arena.read(it.first, 1) - arena.read(prev, 1);
				affine = affine && (d == ptrdiff_t(it.first) - ptrdiff_t(prev));
				checkedAny = true;
			}
			prev = it.first;
		}
		check(checkedAny && affine, "arena-basic: addresses within a chunk resolve affinely");
	}

	// 5. Alignment: everything is 16-aligned for free, and an over-large request is refused
	//    rather than quietly rounded.
	{
		bool aligned = true;
		for (uint32_t align : {1u, 2u, 4u, 8u, 16u}) {
			for (int i = 0; i < 40; ++i) {
				Addr a = arena.alloc(1 + lcg.next(300), align);
				aligned = aligned && a != NullAddr
						&& (uintptr_t(arena.read(a, 1)) % MaxAlign) == 0;
			}
		}
		check(aligned, "arena-basic: payloads are 16-aligned for every supported alignment");
		check(arena.verify() == Status::Ok, "arena-basic: verify after aligned allocations");
	}

	// 6. Oversize blocks: a run of consecutive slots backed by one host allocation. The point of
	//    the run is that the block stays contiguous, so the check is that the last byte of the
	//    block is exactly (total - 1) bytes past the first - if the run were assembled from
	//    separate allocations, that would not hold.
	{
		mem_std::Vector<sprt::pair<Addr, uint32_t>> big;
		bool allocated = true;
		bool contiguous = true;

		for (int i = 0; i < 8; ++i) {
			uint32_t size = MaxInChunkPayload + 1 + lcg.next(200'000);
			Addr a = arena.alloc(size);
			if (a == NullAddr) {
				allocated = false;
				break;
			}
			big.emplace_back(a, size);
			test::fillPattern(arena.write(a, size), size, a);

			auto first = arena.read(a, size);
			auto last = arena.read(a + size - 1, 1);
			contiguous = contiguous && (last - first == ptrdiff_t(size) - 1);
			contiguous = contiguous && (a >> ChunkShift) != ((a + size - 1) >> ChunkShift);
		}

		check(allocated, "arena-basic: oversize allocations succeed");
		check(contiguous, "arena-basic: an oversize block spans its run contiguously");
		check(arena.verify() == Status::Ok, "arena-basic: verify with oversize runs");

		bool patterns = true;
		for (auto &it : big) {
			patterns = patterns && test::checkPattern(arena.read(it.first, it.second), it.second,
									 it.first);
		}
		check(patterns, "arena-basic: oversize blocks read back");

		// Small allocations must still work with runs in the middle of the chunk table.
		Addr small = arena.alloc(64);
		check(small != NullAddr, "arena-basic: small allocation still works after oversize runs");
		check(arena.verify() == Status::Ok, "arena-basic: verify after mixing sizes");
	}

	// 7. verify() has to have teeth: everything from here on treats it as the definition of a
	//    healthy store, so a validator that always returned Ok would make the later sections
	//    meaningless. Damage a header through its address, confirm the report, put it back.
	{
		Addr victim = model.live[model.live.size() / 2];
		auto header = arena.write(victim - BlockHeaderSize, BlockHeaderSize);
		BlockHeader saved;
		__sprt_memcpy(&saved, header, BlockHeaderSize);

		sprt::cout << "       (the two reports below are expected)\n";

		reinterpret_cast<BlockHeader *>(header)->total += Granule;
		check(arena.verify() != Status::Ok, "arena-basic: verify catches a corrupted extent");

		__sprt_memcpy(header, &saved, BlockHeaderSize);
		reinterpret_cast<BlockHeader *>(header)->prevTotal += Granule;
		check(arena.verify() != Status::Ok, "arena-basic: verify catches a broken block chain");

		__sprt_memcpy(header, &saved, BlockHeaderSize);
		check(arena.verify() == Status::Ok, "arena-basic: verify passes again once restored");
	}
}

} // namespace STAPPLER_VERSIONIZED stappler
