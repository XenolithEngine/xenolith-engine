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

// The allocator under a long random workload, checked against a shadow model.
//
// Two things make this section worth its runtime. First, every live block carries a pattern derived
// from its own address, and 10% of the operations read one back: an allocator that ever hands out
// an overlapping block corrupts a pattern, which a "did it return non-null" test would never see.
// Second, everything is deterministic - fixed seed, fixed op mix - so the numbers it reports
// (chunk count, fragmentation) are bit-identical on every platform, which is what makes a threshold
// on them a regression tripwire rather than a statistical guess.

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

// Address -> the size the arena reports for the block. Ordered, so an overlap can be caught against
// the immediate neighbours at the moment the block is handed out rather than by a sweep at the end.
struct Model {
	mem_std::Map<Addr, uint32_t> blocks;
	mem_std::Vector<Addr> live;
	uint64_t bytes = 0; // maintained incrementally: summing a million-entry map per op is not free

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
		bytes += size;
		return true;
	}

	void remove(Addr a, size_t index) {
		bytes -= blocks[a];
		blocks.erase(a);
		live[index] = live.back();
		live.pop_back();
	}
};

} // namespace

// realloc's edge cases are scripted rather than left to the random mix: each of them is a distinct
// path through the allocator (in-place shrink, in-place grow by absorbing a neighbour, relocation,
// oversize) and the random workload gives no control over which one it exercises.
static void performReallocCases() {
	Arena arena;
	if (!arena.init(Config())) {
		check(false, "arena-torture: realloc fixture init");
		return;
	}

	// Consecutive allocations in a fresh store are physically adjacent, which is what makes the
	// neighbour cases below deterministic.
	Addr a = arena.alloc(100);
	Addr b = arena.alloc(100);
	Addr c = arena.alloc(100);
	Addr d = arena.alloc(100);
	for (Addr x : {a, b, c, d}) { test::fillPattern(arena.write(x, arena.sizeOf(x)), arena.sizeOf(x), x); }

	arena.free(b);

	// Grow in place: the hole left by b is adjacent and big enough.
	{
		Addr moved = arena.realloc(a, 200);
		check(moved == a, "arena-torture: realloc grows in place into a free neighbour");
		check(test::checkPattern(arena.read(moved, 100), 100, a),
				"arena-torture: an in-place grow preserves the contents");
		check(arena.sizeOf(moved) == 200, "arena-torture: an in-place grow reports the new size");
		check(arena.verify() == Status::Ok, "arena-torture: verify after an in-place grow");
	}

	// Relocate: c is boxed in by d, so there is nowhere to grow.
	{
		Addr moved = arena.realloc(c, 5'000);
		check(moved != NullAddr && moved != c, "arena-torture: realloc relocates when boxed in");
		check(test::checkPattern(arena.read(moved, 100), 100, c),
				"arena-torture: a relocating grow preserves the contents");
		check(arena.verify() == Status::Ok, "arena-torture: verify after a relocating grow");
		c = moved;
	}

	// Shrink in place, then a no-op resize.
	{
		Addr moved = arena.realloc(d, 48);
		check(moved == d, "arena-torture: realloc shrinks in place");
		check(test::checkPattern(arena.read(moved, 48), 48, d),
				"arena-torture: a shrink preserves the surviving prefix");
		check(arena.realloc(d, 48) == d, "arena-torture: resizing to the same size is a no-op");
		check(arena.verify() == Status::Ok, "arena-torture: verify after a shrink");
	}

	// Oversize blocks own whole runs, so they relocate rather than resize.
	{
		uint32_t size = MaxInChunkPayload + 1'000;
		Addr o = arena.alloc(size);
		test::fillPattern(arena.write(o, size), size, o);

		Addr moved = arena.realloc(o, 300'000);
		check(moved != NullAddr, "arena-torture: an oversize block reallocates");
		check(test::checkPattern(arena.read(moved, size), size, o),
				"arena-torture: an oversize realloc preserves the contents");
		check(arena.verify() == Status::Ok, "arena-torture: verify after an oversize realloc");
	}

	check(arena.realloc(NullAddr, 64) != NullAddr, "arena-torture: realloc of null allocates");
}

void performArenaTortureTests() {
	sprt::cout << "\n== vstore: arena torture ==\n";

	performReallocCases();

	Arena arena;
	check(arena.init(Config()), "arena-torture: init");

	Lcg lcg(0x0bad'c0ffe'e0dd'f00dull);
	Model model;

	bool ok = true;
	bool everyOpVerified = true;
	uint64_t peakLiveBytes = 0;
	uint32_t peakChunkCount = 0;
	uint32_t allocs = 0, frees = 0, reallocs = 0, reads = 0;

	// The op mix alone is a random walk, so the live set would drift until it hit the budget and
	// the section would be testing exhaustion rather than reuse. Capping it keeps the workload
	// stationary: the arena spends the run recycling memory, which is the thing under test.
	static constexpr size_t LiveCap = 40'000;

	// The reduced run walks a tenth of the ops (tests.h, `test::full`). Every op is still verified
	// against the model and the arena is still verify()-ed at the same cadence; what the full
	// million adds is the recycling depth - the arena spends the tail of the walk reusing memory it
	// has already reused, which is the state a fresh short walk never reaches.
	const int torture = int(test::sized(1'000'000, 100'000));
	for (int op = 0; op < torture && ok; ++op) {
		auto roll = lcg.next(100);
		if (model.live.size() >= LiveCap && roll < 45) {
			roll += 45;
		}

		if (roll < 45 || model.live.empty()) {
			uint32_t size = 1 + lcg.next(512);
			if (lcg.next(64) == 0) {
				size = 512 + lcg.next(7'680);
			}
			if (lcg.next(4096) == 0) {
				size = MaxInChunkPayload + 1 + lcg.next(128 * 1024);
			}

			Addr a = arena.alloc(size);
			if (a == NullAddr) {
				ok = false;
				sprt::cout << "       allocation of " << size << " bytes failed at op " << op
						   << "\n";
				break;
			}
			uint32_t actual = arena.sizeOf(a);
			if (!model.add(a, actual)) {
				ok = false;
				sprt::cout << "       block at " << a << " overlaps a live block, op " << op
						   << "\n";
				break;
			}
			test::fillPattern(arena.write(a, actual), actual, a);
			++allocs;

			if (model.bytes > peakLiveBytes) {
				peakLiveBytes = model.bytes;
				peakChunkCount = arena.getChunkCount();
			}
		} else if (roll < 80) {
			size_t index = lcg.next(uint32_t(model.live.size()));
			Addr a = model.live[index];
			auto it = model.blocks.find(a);
			if (!test::checkPattern(arena.read(a, it->second), it->second, a)) {
				ok = false;
				sprt::cout << "       block at " << a << " was damaged before free, op " << op
						   << "\n";
				break;
			}
			arena.free(a, it->second);
			model.remove(a, index);
			++frees;
		} else if (roll < 90) {
			size_t index = lcg.next(uint32_t(model.live.size()));
			Addr a = model.live[index];
			uint32_t oldSize = model.blocks.find(a)->second;
			uint32_t newSize = 1 + lcg.next(1'024);

			Addr moved = arena.realloc(a, newSize);
			if (moved == NullAddr) {
				ok = false;
				sprt::cout << "       realloc to " << newSize << " failed at op " << op << "\n";
				break;
			}

			// The surviving prefix must still hold the pattern of the OLD address - that is what
			// proves realloc copied rather than merely resized.
			uint32_t kept = newSize < oldSize ? newSize : oldSize;
			if (!test::checkPattern(arena.read(moved, kept), kept, a)) {
				ok = false;
				sprt::cout << "       realloc lost data moving " << a << " -> " << moved
						   << ", op " << op << "\n";
				break;
			}

			model.remove(a, index);
			uint32_t actual = arena.sizeOf(moved);
			if (!model.add(moved, actual)) {
				ok = false;
				sprt::cout << "       reallocated block at " << moved << " overlaps, op " << op
						   << "\n";
				break;
			}
			test::fillPattern(arena.write(moved, actual), actual, moved);
			++reallocs;
		} else {
			size_t index = lcg.next(uint32_t(model.live.size()));
			Addr a = model.live[index];
			auto it = model.blocks.find(a);
			if (!test::checkPattern(arena.read(a, it->second), it->second, a)) {
				ok = false;
				sprt::cout << "       block at " << a << " was damaged, op " << op << "\n";
				break;
			}
			++reads;
		}

		if ((op % 50'000) == 0 && arena.verify() != Status::Ok) {
			everyOpVerified = false;
			sprt::cout << "       verify failed at op " << op << "\n";
			break;
		}
	}

	check(ok, mem_std::toString("arena-torture: ", torture,
					" operations against the shadow model"));
	check(everyOpVerified, "arena-torture: periodic verify stayed clean");
	sprt::cout << "       " << allocs << " allocations, " << frees << " frees, " << reallocs
			   << " reallocs, " << reads << " reads; " << arena.getChunkCount() << " chunks\n";

	// Final sweep, forwards: every live block still holds its own pattern.
	{
		bool patterns = true;
		for (auto &it : model.blocks) {
			patterns = patterns && test::checkPattern(arena.read(it.first, it.second), it.second,
									 it.first);
		}
		check(patterns, "arena-torture: every live block reads back at the end");
	}

	// And backwards: the arena's own accounting matches the model exactly - not a bound, an
	// equality. liveBytes counts requested payload, so anything the allocator lost or double
	// counted shows up here.
	{
		mem_std::Value dump;
		arena.describe(dump);
		auto count = dump.getValue("live").getInteger("count");
		auto bytes = dump.getValue("live").getInteger("bytes");
		check(uint64_t(count) == model.blocks.size(),
				mem_std::toString("arena-torture: live count matches the model (", count, " vs ",
						model.blocks.size(), ")"));
		check(uint64_t(bytes) == model.bytes,
				mem_std::toString("arena-torture: live bytes match the model (", bytes, " vs ",
						model.bytes, ")"));
	}

	// Fragmentation. The workload is deterministic, so this number is bit-identical everywhere and
	// the bound is a regression tripwire rather than a statistical guess.
	//
	// Measured at 0.319 with coalescing; the bound is that plus a quarter, so ordinary drift does
	// not trip it but a structural regression does. Without coalescing the same workload measures
	// 17.56 - splitting shreds every large free block into small classes, and a large request then
	// has nowhere to come from but a fresh chunk. The two numbers are what makes the bound
	// meaningful rather than arbitrary.
	//
	// A SHORTER WALK IS A DIFFERENT WORKLOAD, so it gets a baseline of its own rather than the
	// full one's bound: at 100 000 ops the live set has not yet reached its peak and the same
	// allocator measures 0.099. Reusing 0.40 there would have left the tripwire passing by a
	// factor of four - green, and no longer evidence of anything. Both bounds are the recorded
	// figure plus a quarter, and both are far below the 17.56 that a lost coalesce produces.
	{
		double overhead = double(uint64_t(peakChunkCount) * ChunkSize - peakLiveBytes)
				/ double(peakLiveBytes > 0 ? peakLiveBytes : 1);
		double bound = test::full() ? 0.40 : 0.13;
		sprt::cout << "       peak: " << peakLiveBytes << " live bytes in " << peakChunkCount
				   << " chunks, overhead " << overhead << " (bound " << bound << ")\n";
		check(overhead < bound, "arena-torture: fragmentation stays within the recorded baseline");
	}

	// Free everything. What is left has to be a healthy, empty store - and one that can still hand
	// out a whole chunk's worth of memory.
	{
		auto blocks = model.blocks;
		for (auto &it : blocks) { arena.free(it.first, it.second); }
		model.blocks.clear();
		model.live.clear();

		check(arena.verify() == Status::Ok, "arena-torture: verify after freeing everything");

		mem_std::Value dump;
		arena.describe(dump);
		check(dump.getValue("live").getInteger("count") == 0
						&& dump.getValue("live").getInteger("bytes") == 0
						&& dump.getValue("live").getInteger("total") == 0,
				"arena-torture: nothing is live after freeing everything");

		// The headline assertion for coalescing, and the only one that proves it works in BOTH
		// directions: with everything freed, each surviving chunk has to have collapsed into
		// exactly one free block. Any missed merge leaves a chunk holding two or more.
		int64_t normalChunks = 0;
		for (auto &it : dump.getValue("chunks").getArray()) {
			if (it.getInteger("kind") == int64_t(ChunkKind::Normal)) {
				++normalChunks;
			}
		}
		auto freeBlocks = dump.getValue("free").getInteger("count");
		check(freeBlocks == normalChunks,
				mem_std::toString("arena-torture: every chunk collapsed to one free block (",
						freeBlocks, " free blocks in ", normalChunks, " chunks)"));

		// And that single block per chunk must be usable as a whole.
		check(arena.alloc(MaxInChunkPayload) != NullAddr,
				"arena-torture: a full chunk can be allocated again afterwards");
		check(arena.verify() == Status::Ok, "arena-torture: verify at the end");
	}
}

} // namespace STAPPLER_VERSIONIZED stappler
