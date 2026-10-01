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

// The WRITE VIEW (Parallel-14): a snapshot of a store's chunk table, handed to a worker that will
// change bytes rather than only read them.
//
// It exists because a parallel block's results have to land somewhere, and the somewhere is the run's
// own store - which the machine's front goes on growing while the branches are out. Growing moves the
// host chunk table (`mem_std::Vector<uint8_t *>`), and every `write()` resolves through it, so a
// worker holding a raw pointer into that table is holding a pointer the owner may free. A snapshot
// does not have that problem: the chunks themselves never move, only the table naming them.
//
// The safety of the thing is one sentence: A VIEW CANNOT ALLOCATE. Not "must not" - cannot, in every
// build, counted rather than asserted. So a worker can change the bytes of a block that already
// existed and nothing else: the free lists, the block headers, the descriptor table and the chunk
// table stay the owner's alone, and there is no second thread in the allocator at any moment.
//
// Four claims, in order of how much they say:
//
//   1. A write through a view is a write to the store. Same bytes, same image.
//   2. A view refuses to allocate, and says so (getViewAllocCount).
//   3. The owner may go on growing while views are alive: a view keeps resolving every block that
//      existed when it was taken, and never sees a page that did not.
//   4. Two threads writing blocks that do not overlap produce the image one thread produces - and,
//      after mergeDirtyFrom, the same dirty map, which is what a journal will read.
//
// The last one is the whole point and the reason the map is per-view: marking is a read-modify-write
// of one word per page, and two blocks that do not overlap routinely share a page.

#include "SPCommon.h"
#include "SPMemory.h"

#include "SPVStoreArena.h"

#include "../tests.h"
#include "../check/vstore_check.h"

#include <sprt/c/__sprt_string.h>
#include <sprt/cxx/thread>

namespace STAPPLER_VERSIONIZED stappler {

using stappler::test::check;

namespace {

using namespace vstore;

// A store's content, as this layer defines it: the bytes of its save() image.
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

mem_std::Vector<uint64_t> takeMap(const TrackedArena &arena) {
	auto map = arena.getDirtyMap();
	return mem_std::Vector<uint64_t>(map.begin(), map.end());
}

// Fills `bytes` of the block at `a` with a byte derived from `seed`, through whichever store it is
// given - the owner or a view of it. The same function for both is the point: nothing above the
// arena knows which one it holds.
template <typename A>
void fill(A &arena, Addr a, uint32_t bytes, uint8_t seed) {
	auto p = arena.write(a, bytes);
	for (uint32_t i = 0; i < bytes; ++i) { p[i] = uint8_t(seed + i); }
}

constexpr uint32_t BlockBytes = 96;
constexpr uint32_t Blocks = 64;

} // namespace

void performArenaWriteViewTests() {
	// 1. A write through a view is a write to the store.
	{
		TrackedArena arena;
		check(arena.init(), "arena-write-view: the store initializes");

		Addr a = arena.alloc(BlockBytes);
		Addr b = arena.alloc(BlockBytes);
		check(a != NullAddr && b != NullAddr, "arena-write-view: two blocks are allocated");

		TrackedArena direct;
		check(direct.initWriteView(arena) == Status::Ok,
				"arena-write-view: a write view opens on a live store");
		check(direct.isView() && direct.isWriteView(),
				"arena-write-view: it says it is a view, and a writing one");

		fill(direct, a, BlockBytes, 0x10);
		fill(arena, b, BlockBytes, 0x40);

		bool same = true;
		auto read = arena.read(a, BlockBytes);
		for (uint32_t i = 0; i < BlockBytes; ++i) { same = same && read[i] == uint8_t(0x10 + i); }
		check(same, "arena-write-view: what the view wrote is what the store reads back");

		// The same store written entirely by its owner, for the image to be compared against.
		TrackedArena plain;
		plain.init();
		Addr pa = plain.alloc(BlockBytes);
		Addr pb = plain.alloc(BlockBytes);
		fill(plain, pa, BlockBytes, 0x10);
		fill(plain, pb, BlockBytes, 0x40);

		check(takeImage(arena) == takeImage(plain),
				"arena-write-view: and the image is the one the owner would have produced");

		direct.closeView();
		check(!direct.isView() && !direct.isInitialized(),
				"arena-write-view: closeView gives up the copy and nothing else");
		check(arena.verify() == Status::Ok, "arena-write-view: the store still verifies");
	}

	// 2. A view does not allocate. Both kinds of view, both directions of the answer.
	{
		// A refusal is the subject here rather than a failure, so the switch goes down for it -
		// the same way parallel-threads does when it proves the ownership counter counts.
		setArenaViolationsFatal(false);

		TrackedArena arena;
		arena.init();
		Addr a = arena.alloc(BlockBytes);

		TrackedArena writing;
		writing.initWriteView(arena);

		auto before = getViewAllocCount();
		check(writing.alloc(BlockBytes) == NullAddr,
				"arena-write-view: a write view refuses to allocate");
		check(writing.realloc(a, BlockBytes * 2) == NullAddr,
				"arena-write-view: ... and to grow a block");
		writing.free(a);
		check(getViewAllocCount() == before + 3,
				"arena-write-view: every refusal is counted (alloc, realloc, free)");

		// The block the view tried to free is still there, and the store is intact.
		check(arena.isLivePayload(a), "arena-write-view: the block a view tried to free is still live");
		check(arena.verify() == Status::Ok, "arena-write-view: a refused call left nothing behind");

		TrackedArena reading;
		reading.initView(arena);
		before = getViewAllocCount();
		check(reading.alloc(BlockBytes) == NullAddr && getViewAllocCount() == before + 1,
				"arena-write-view: a read view refuses the same way");

		setArenaViolationsFatal(true);
	}

	// 3. The owner goes on growing while a view is alive.
	{
		TrackedArena arena;
		arena.init();
		Addr early = arena.alloc(BlockBytes);
		fill(arena, early, BlockBytes, 0x01);

		TrackedArena view;
		view.initWriteView(arena);

		// Enough to add chunks - the table the view copied is then no longer the owner's table.
		mem_std::Vector<Addr> late;
		for (uint32_t i = 0; i < 4096; ++i) {
			auto a = arena.alloc(BlockBytes);
			if (a == NullAddr) {
				break;
			}
			late.emplace_back(a);
		}
		check(late.size() > 0, "arena-write-view: the owner allocated while the view was open");

		fill(view, early, BlockBytes, 0x21);
		bool same = true;
		auto read = arena.read(early, BlockBytes);
		for (uint32_t i = 0; i < BlockBytes; ++i) { same = same && read[i] == uint8_t(0x21 + i); }
		check(same, "arena-write-view: a view still writes the blocks it was taken with");
		check(arena.verify() == Status::Ok,
				"arena-write-view: and the store the owner grew still verifies");

		view.closeView();
	}

	// 4. Two threads, blocks that do not overlap: the owner's image and the owner's dirty map.
	{
		TrackedArena arena;
		arena.init();

		mem_std::Vector<Addr> blocks;
		for (uint32_t i = 0; i < Blocks; ++i) { blocks.emplace_back(arena.alloc(BlockBytes)); }

		// What one thread would have written, and the map it would have left.
		TrackedArena serial;
		serial.init();
		mem_std::Vector<Addr> serialBlocks;
		for (uint32_t i = 0; i < Blocks; ++i) { serialBlocks.emplace_back(serial.alloc(BlockBytes)); }
		serial.clearDirtyMap();
		for (uint32_t i = 0; i < Blocks; ++i) {
			fill(serial, serialBlocks[i], BlockBytes, uint8_t(i * 3 + 1));
		}
		auto serialImage = takeImage(serial);
		auto serialMap = takeMap(serial);

		arena.clearDirtyMap();

		TrackedArena left;
		TrackedArena right;
		left.initWriteView(arena);
		right.initWriteView(arena);

		// Interleaved halves rather than a split down the middle: neighbouring blocks share a page,
		// so this is the arrangement in which a shared dirty-map word is unavoidable.
		auto half = [&](TrackedArena &view, uint32_t from) {
			for (uint32_t i = from; i < Blocks; i += 2) {
				fill(view, blocks[i], BlockBytes, uint8_t(i * 3 + 1));
			}
		};

		sprt::thread one([&] { half(left, 0); });
		sprt::thread two([&] { half(right, 1); });
		one.join();
		two.join();

		check(takeImage(arena) == serialImage,
				"arena-write-view: two threads through two views leave the bytes one thread leaves");

		check(takeMap(arena) != serialMap,
				"arena-write-view: before the merge the owner's map holds none of it");
		arena.mergeDirtyFrom(left);
		arena.mergeDirtyFrom(right);
		check(takeMap(arena) == serialMap,
				"arena-write-view: and after the merge it is the map one thread would have left");

		left.closeView();
		right.closeView();
		check(arena.verify() == Status::Ok, "arena-write-view: the store verifies after both");
	}
}

} // namespace STAPPLER_VERSIONIZED stappler
