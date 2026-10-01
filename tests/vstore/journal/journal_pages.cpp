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

// Which pages a version covers, and what a page's previous content is.
//
// Both are load-bearing and neither is obvious. A write legitimately spans several slots (an
// oversize run is marked whole), and a page's "previous content" is not the bytes behind it: by the
// time the journal looks, they are the NEW bytes. The old ones come from the store's baseline, which
// is what makes deferring the capture possible at all - and for a slot that was not live at the last
// commit that baseline is zeros, whatever the recycled memory happens to hold. Get that second rule
// wrong and a rollback resurrects another slot's data, silently, and only in workloads that recycle
// chunks.
//
// The first thing this section establishes, and the one every case after it depends on, is WHEN a
// record comes into being: at the boundary, and never at the write.

#include "SPCommon.h"
#include "SPMemory.h"

#include "SPVStoreJournal.h"

#include <sprt/c/__sprt_string.h>

#include "../tests.h"
#include "../check/vstore_check.h"

namespace STAPPLER_VERSIONIZED stappler {

// This whole section is ABOUT versioning, so without it there is nothing here to test and
// nothing here that would compile. The section keeps its place in the list and says so.

using stappler::test::check;

namespace {

using namespace vstore;

// The pages a store's records name, in order.
mem_std::Vector<sprt::pair<uint32_t, uint32_t>> recordedPages(const JournalStore &store,
		uint32_t from) {
	mem_std::Vector<sprt::pair<uint32_t, uint32_t>> out;
	for (uint32_t i = from; i < store.getRecordCount(); ++i) {
		auto &r = store.getRecord(i);
		out.emplace_back(sprt::pair<uint32_t, uint32_t>(r.slot, r.page));
	}
	return out;
}

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

void performJournalPagesTests() {
	sprt::cout << "\n== vstore: journal pages ==\n";

	Arena arena;
	check(arena.init(Config()), "journal-pages: store init");

	Journal journal;
	check(journal.init(), "journal-pages: journal init");
	auto index = journal.attach(&arena);
	auto &store = journal.getStore(index);
	check(index == 0, "journal-pages: the first store is index 0");

	// A run gives a payload that spans three slots, so the boundary cases are reachable.
	uint32_t bigSize = 3 * ChunkSize - 1000;
	Addr big = arena.alloc(bigSize);
	check(big != NullAddr, "journal-pages: oversize fixture allocated");
	uint32_t slot = big >> ChunkShift;
	journal.commit();

	// 1. Nothing is recorded until a version closes. This is the whole change in one assertion, and
	//    it is the reason every case below has to commit before it counts anything.
	{
		uint32_t before = store.getRecordCount();
		test::fillPattern(arena.write(big + 32, 64), 64, big);
		check(store.getRecordCount() == before, "journal-pages: a write records nothing by itself");
		check(store.getDirtyPageCount() == 1, "journal-pages: it marks a page and stops there");

		journal.commit();
		check(store.getRecordCount() == before + 1,
				"journal-pages: the boundary is where the record comes from");
	}

	// 2. The (address, size) -> page set mapping.
	{
		struct Case {
			uint32_t at; // offset from the START of the run
			uint32_t size;
			uint32_t slotOffset;
			uint32_t firstPage;
			uint32_t lastPage;
			uint32_t pages;
		};
		const Case cases[] = {
			{BlockHeaderSize, 16, 0, 0, 0, 1}, // inside one page
			{PageSize - 6, 16, 0, 0, 1, 2}, // across a page boundary
			{PageSize, 2 * PageSize + 1, 0, 1, 3, 3}, // three pages
			{2 * PageSize - 1, 1, 0, 1, 1, 1}, // the last byte of a page
			// across a chunk boundary: the last page of one, page 0 of the next
			{ChunkSize - 8, 16, 0, PagesPerChunk - 1, PagesPerChunk - 1, 2},
		};

		bool exact = true;
		for (auto &c : cases) {
			uint32_t before = store.getRecordCount();
			Addr a = big + c.at - BlockHeaderSize;
			test::fillPattern(arena.write(a, c.size), c.size, a);
			journal.commit();

			auto pages = recordedPages(store, before);
			if (pages.size() != c.pages) {
				exact = false;
				sprt::cout << "       at " << c.at << " size " << c.size << ": " << pages.size()
						   << " records, expected " << c.pages << "\n";
			} else if (pages.front().first != slot + c.slotOffset
					|| pages.front().second != c.firstPage) {
				exact = false;
				sprt::cout << "       at " << c.at << " size " << c.size << ": starts at slot "
						   << pages.front().first << " page " << pages.front().second << "\n";
			}
		}
		check(exact, "journal-pages: a version records exactly the pages it covered");
	}

	// 3. A zero-length write records nothing. The page arithmetic would underflow on it.
	{
		uint32_t before = store.getRecordCount();
		arena.write(big, 0);
		journal.commit();
		check(store.getRecordCount() == before,
				"journal-pages: a zero-length write records nothing");
	}

	// 4. One record per page per version, however many times it is written.
	{
		uint32_t before = store.getRecordCount();
		for (int i = 0; i < 50; ++i) {
			test::fillPattern(arena.write(big + 32, 64), 64, big + uint32_t(i));
		}
		journal.commit();
		check(store.getRecordCount() == before + 1,
				"journal-pages: fifty writes to one page make one record");

		test::fillPattern(arena.write(big + 32, 64), 64, big);
		journal.commit();
		check(store.getRecordCount() == before + 2,
				"journal-pages: the next version records the page again");
	}

	// 5. The map is what enforces that, and a commit clears it.
	{
		check(store.getDirtyPageCount() == 0, "journal-pages: a commit clears the dirty map");
		test::fillPattern(arena.write(big + 32, 64), 64, big);
		check(store.getDirtyPageCount() == 1, "journal-pages: a write dirties exactly one page");

		// And it says WHERE in the page, which is what a record is then allowed to carry: 64 bytes
		// inside one sub-block is one bit of sixty-four.
		check(store.getDirtySubBlockCount() == 1, "journal-pages: and exactly one sub-block of it");
		journal.commit();
	}

	// 5b. What a record then does with that. Pinned to Scatter and to no codec, because this is about
	//     the RECORD SHAPE and the default policy is deliberately free not to produce one.
	//
	//     Smallest used to be free of it in the other direction: it encoded the whole page as a
	//     candidate, and zstd folds a 16 KiB page of repeating pattern below the 256 bytes a delta
	//     costs, so it picked the page. That case is retired - a wider shape is now asked only when
	//     it could win without compressing (see DeltaPolicy::Smallest), because finding it cost a
	//     16 KiB compression on every recorded page and won on a few. So Smallest would produce the
	//     delta here today; the pin stays because this section is about what the RECORD says, and a
	//     section that depends on a policy's tie-breaking is testing the wrong thing either way.
	//
	//     Which is also why the rollback sweep pins its policies rather than trusting the default to
	//     wander into each shape.
	{
		Arena narrowArena;
		check(narrowArena.init(Config()), "journal-pages: narrow store init");

		Journal::Config cfg;
		cfg.codec = JournalCodec::None;
		cfg.delta = DeltaPolicy::Scatter;

		Journal narrowJournal;
		check(narrowJournal.init(cfg), "journal-pages: narrow journal init");
		auto &narrowStore = narrowJournal.getStore(narrowJournal.attach(&narrowArena));

		Addr a = narrowArena.alloc(4 * PageSize);
		check(a != NullAddr, "journal-pages: narrow fixture allocated");
		test::fillPattern(narrowArena.write(a, 4 * PageSize), 4 * PageSize, a);
		narrowJournal.commit();

		uint32_t before = narrowStore.getRecordCount();
		test::fillPattern(narrowArena.write(a + 32, 64), 64, a + 1);
		narrowJournal.commit();

		bool narrow = false;
		for (uint32_t i = before; i < narrowStore.getRecordCount(); ++i) {
			auto &r = narrowStore.getRecord(i);
			if (r.slot == (a >> ChunkShift) && r.page == ((a & ChunkMask) >> PageShift)) {
				narrow = (r.flags & JournalRecord::Delta) != 0 && __builtin_popcountll(r.mask) == 1
						&& r.size == SubSize;
			}
		}
		check(narrow, "journal-pages: a 64-byte write records one sub-block, not a page");
	}

	// 6. The before-image rule. A slot that was not live at the last commit reads as zeros, so
	//    taking a recycled slot must produce Zero records - not the foreign bytes the recycled
	//    buffer still holds. Under the baseline this is true BY CONSTRUCTION rather than by the
	//    arena announcing the run at exactly the right instant, which is the point.
	{
		test::fillPattern(arena.write(big, bigSize), bigSize, big);
		journal.commit();

		uint32_t before = store.getRecordCount();
		arena.free(big);
		journal.commit();

		// Releasing marks the whole run, and those records must carry the bytes the run held at the
		// previous boundary. Counted over the run's own slots: the free() also touches Root, which
		// lives in slot 0.
		uint32_t runPages = 0;
		bool payloads = true;
		for (uint32_t i = before; i < store.getRecordCount(); ++i) {
			auto &r = store.getRecord(i);
			if (r.slot < slot || r.slot >= slot + 3) {
				continue;
			}
			++runPages;
			if ((r.flags & JournalRecord::Zero) != 0) {
				payloads = false;
			}
		}
		check(runPages == 3 * PagesPerChunk,
				mem_std::toString("journal-pages: releasing a run records all its pages (",
						runPages, ")"));
		check(payloads, "journal-pages: those records carry bytes, not the zero flag");

		// Now take a slot back. The last commit rebased it to zeros because it was not live, so that
		// is its before-image no matter what the recycled buffer holds.
		before = store.getRecordCount();
		Addr fresh = arena.alloc(MaxInChunkPayload);
		check(fresh != NullAddr, "journal-pages: the recycled allocation succeeded");
		journal.commit();

		uint32_t freshSlot = fresh >> ChunkShift;
		uint32_t zeros = 0, total = 0;
		for (uint32_t i = before; i < store.getRecordCount(); ++i) {
			auto &r = store.getRecord(i);
			if (r.slot != freshSlot) {
				continue;
			}
			++total;
			if ((r.flags & JournalRecord::Zero) != 0) {
				++zeros;
			}
		}
		check(total >= 1 && zeros == total,
				mem_std::toString("journal-pages: a recycled slot records zeros, not stale bytes (",
						zeros, " of ", total, ")"));

		// And NOT all sixteen of its pages. The run's baseline was rebased to zeros when it was
		// released, the fresh chunk arrives zeroed, so every page the allocator does not then write
		// into is zeros-to-zeros - marked, looked at, and nothing to say. Only the pages carrying the
		// block header and the free links have anything to record.
		check(total < PagesPerChunk,
				mem_std::toString("journal-pages: and only the pages it really wrote (", total,
						" of ", PagesPerChunk, ")"));
	}

	// 7. A page written and written back inside one version has no undo, and does not get a record.
	//    This is the rule that makes the map "which pages to look at" rather than "which pages to
	//    store" - and it is not a heuristic: the page's content across the version is unchanged, so
	//    a rollback leaving it alone is exactly right.
	{
		journal.commit();
		uint32_t before = store.getRecordCount();
		auto unchangedBefore = store.getUnchangedPageCount();

		// Read the eight bytes, write something else, then write them back.
		uint8_t original[8];
		__sprt_memcpy(original, arena.read(big + 64, 8), 8);
		test::fillPattern(arena.write(big + 64, 8), 8, big + 999);
		__sprt_memcpy(arena.write(big + 64, 8), original, 8);

		check(store.getDirtyPageCount() >= 1, "journal-pages: the page was marked either way");
		journal.commit();

		check(store.getRecordCount() == before,
				mem_std::toString(
						"journal-pages: a page written and written back records nothing (",
						store.getRecordCount() - before, ")"));
		check(store.getUnchangedPageCount() > unchangedBefore,
				"journal-pages: and is counted as looked-at-and-unchanged");

		// The negative twin: the same write NOT put back does produce a record. Without this the
		// check above would also pass against a journal that had stopped recording anything.
		test::fillPattern(arena.write(big + 64, 8), 8, big + 999);
		journal.commit();
		check(store.getRecordCount() == before + 1,
				"journal-pages: while a page left changed still records one");
	}

	// 7. Allocate and release the same run INSIDE one version. The old design had to be told about
	//    the chunk while its descriptor still said Empty, or it would have captured the recycled
	//    buffer; here the baseline never moved, so the version's undo is what the store held before
	//    any of it - and the check is that rolling back really lands there.
	{
		auto mark = journal.commit();
		auto image = takeImage(arena);

		Addr a = arena.alloc(2 * ChunkSize);
		check(a != NullAddr, "journal-pages: the in-version run allocated");
		test::fillPattern(arena.write(a, 2 * ChunkSize), 2 * ChunkSize, a);
		arena.free(a);
		journal.commit();

		check(journal.rollback(mark) == Status::Ok,
				"journal-pages: the in-version churn rolls back");
		auto now = takeImage(arena);
		check(test::compareBytes(BytesView(now.data(), now.size()),
					  BytesView(image.data(), image.size()), "journal-pages"),
				"journal-pages: to the byte, with no ordering rule to get right");
	}

	check(arena.verify() == Status::Ok, "journal-pages: the store is healthy throughout");
}


} // namespace STAPPLER_VERSIONIZED stappler
