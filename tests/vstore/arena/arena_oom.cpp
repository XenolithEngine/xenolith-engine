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

// What happens at the edge of the budget.
//
// Refusing an allocation is easy; refusing it without leaving anything behind is the part that has
// to be checked. A failed growth that half-registered a chunk, or a failed realloc that already
// released the original, would both show up long after the fact and look like corruption rather
// than exhaustion. So every refusal here is followed by the same question: is the store still
// exactly what it was, and does it still work?

#include "SPCommon.h"
#include "SPMemory.h"

#include "SPVStoreArena.h"

#include "../tests.h"
#include "../check/vstore_check.h"

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;

namespace {

using namespace vstore;

mem_std::Vector<uint8_t> takeImage(const Arena &arena) {
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

} // namespace

void performArenaOomTests() {
	sprt::cout << "\n== vstore: arena out of memory ==\n";

	// The budget and the chunk table capacity are the same number, so a 256 KiB store is exactly
	// four slots - small enough to exhaust in a test, large enough to hold a real workload.
	Config cfg;
	cfg.budgetBytes = uint32_t(256_KiB);

	Arena arena;
	check(arena.init(cfg), "arena-oom: init with a small budget");
	check(arena.getMaxChunks() == 4, "arena-oom: the budget becomes four slots");

	// 1. Allocate until it refuses.
	mem_std::Vector<Addr> blocks;
	{
		bool terminated = false;
		for (int i = 0; i < 100'000; ++i) {
			Addr a = arena.alloc(512);
			if (a == NullAddr) {
				terminated = true;
				break;
			}
			test::fillPattern(arena.write(a, 512), 512, a);
			blocks.emplace_back(a);
		}
		check(terminated, "arena-oom: allocation eventually refuses instead of looping");
		check(!blocks.empty(), "arena-oom: something was allocated before the refusal");
		sprt::cout << "       " << blocks.size() << " blocks of 512 bytes fit the budget\n";
		check(arena.getChunkCount() == 4, "arena-oom: the store used its whole budget");
		check(arena.verify() == Status::Ok, "arena-oom: verify straight after the refusal");
	}

	// 2. The relocation invariant still holds at the edge - an exhausted store is still a store.
	{
		auto image = takeImage(arena);

		Arena copy;
		check(copy.adopt(BytesView(image.data(), image.size())) == Status::Ok,
				"arena-oom: an exhausted store still adopts");
		check(copy.verify() == Status::Ok, "arena-oom: the copy verifies");

		mem_std::Value a, b;
		arena.describe(a);
		copy.describe(b);
		check(test::compareValues(b, a, "arena-oom"), "arena-oom: the copy describes identically");

		auto again = takeImage(copy);
		check(test::compareBytes(BytesView(again.data(), again.size()),
					  BytesView(image.data(), image.size()), "arena-oom"),
				"arena-oom: the copy saves to a byte-identical image");

		bool patterns = true;
		for (auto a2 : blocks) {
			patterns = patterns && test::checkPattern(copy.read(a2, 512), 512, a2);
		}
		check(patterns, "arena-oom: every block survives the round trip");
	}

	// 3. Free every other block and allocate again: the freed holes are exact fits, so most of
	//    them have to come back.
	{
		size_t freed = 0;
		for (size_t i = 0; i < blocks.size(); i += 2) {
			arena.free(blocks[i]);
			++freed;
		}
		check(arena.verify() == Status::Ok, "arena-oom: verify after freeing every other block");

		size_t regained = 0;
		while (arena.alloc(512) != NullAddr) { ++regained; }
		sprt::cout << "       freed " << freed << " blocks, allocated " << regained << " again\n";
		check(regained + 4 >= freed, "arena-oom: freed space is handed back out");
	}

	// 4. An oversize request that does not fit the remaining budget fails without consuming
	//    anything - not even a reserved slot.
	{
		Arena small;
		Config tiny;
		tiny.budgetBytes = uint32_t(256_KiB);
		check(small.init(tiny), "arena-oom: second store init");

		Addr first = small.alloc(64);
		auto before = small.getChunkCount();

		check(small.alloc(3 * ChunkSize) == NullAddr,
				"arena-oom: an oversize run beyond the budget is refused");
		check(small.getChunkCount() == before, "arena-oom: the refusal reserved no slots");
		check(small.verify() == Status::Ok, "arena-oom: verify after the refused run");
		check(small.alloc(64) != NullAddr, "arena-oom: small allocations still work afterwards");
		check(first != NullAddr, "arena-oom: the earlier block is still there");
	}

	// 5. A realloc that cannot grow must leave the original completely intact - the caller still
	//    has everything it had before asking.
	{
		Arena full;
		Config tiny;
		tiny.budgetBytes = uint32_t(128_KiB);
		check(full.init(tiny), "arena-oom: third store init");

		Addr victim = full.alloc(1'000);
		test::fillPattern(full.write(victim, 1'000), 1'000, victim);

		while (full.alloc(1'000) != NullAddr) { }

		auto image = takeImage(full);
		check(full.realloc(victim, 60'000) == NullAddr, "arena-oom: a realloc that cannot fit fails");

		auto after = takeImage(full);
		check(test::compareBytes(BytesView(after.data(), after.size()),
					  BytesView(image.data(), image.size()), "arena-oom"),
				"arena-oom: the failed realloc changed nothing at all");
		check(full.sizeOf(victim) == 1'000, "arena-oom: the original block kept its size");
		check(test::checkPattern(full.read(victim, 1'000), 1'000, victim),
				"arena-oom: the original block kept its contents");
		check(full.verify() == Status::Ok, "arena-oom: verify after the failed realloc");
	}

	// 6. A budget past what the chunk table can describe is refused at init, not at the first
	//    allocation that happens to cross the line.
	{
		Arena tooBig;
		Config huge;
		huge.budgetBytes = uint32_t(1_GiB);
		sprt::cout << "       (the report below is expected)\n";
		check(!tooBig.init(huge), "arena-oom: a budget beyond the maximum store size is refused");
		check(!tooBig.isInitialized(), "arena-oom: the refused store stays uninitialized");
	}
}

} // namespace STAPPLER_VERSIONIZED stappler
