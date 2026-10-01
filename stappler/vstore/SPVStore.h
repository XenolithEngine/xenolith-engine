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

#ifndef STAPPLER_VSTORE_SPVSTORE_H_
#define STAPPLER_VSTORE_SPVSTORE_H_

#include "SPCommon.h"
#include "SPMemory.h"

#include <sprt/c/__sprt_assert.h>

namespace STAPPLER_VERSIONIZED stappler::vstore {

// Everything inside a store is addressed by an offset into a logical address space, never by a
// pointer: that is what lets the whole store be copied byte-wise, snapshotted and rolled back. The
// space is carved into fixed-size chunks so that growing it never invalidates an address already
// handed out - resolve(a) is chunks[a >> ChunkShift] + (a & ChunkMask), one formula, no branches.
using Addr = uint32_t;

// A version is one consistent slice of both stores, persistent and local; the counter is shared, so
// rolling back to a version rolls back both.
using Version = uint64_t;

// Address 0 is never a valid payload: the Root structure physically occupies offset 0 of chunk 0,
// so there is nothing to allocate there in the first place.
static constexpr Addr NullAddr = 0;

static constexpr uint32_t ChunkShift = 16;
static constexpr uint32_t ChunkSize = 1u << ChunkShift; // 64 KiB
static constexpr uint32_t ChunkMask = ChunkSize - 1;

// Journal granularity. Nothing in the arena may key off it except the debug write-barrier
// validator, or the arena would bake in a decision the journal has to make. At 16 KiB a page's
// dirty-map entry is 64 sub-blocks of 256 bytes, exactly one uint64_t, so the arena announces not
// just which page a write landed on but where inside it at the same one-word cost, and the
// journal's per-page work narrows to the sub-blocks really touched. The ceiling on this direction
// is 64 KiB: JournalRecord::size and KeyframeBlock::size are uint16_t, and a raw record stores size
// == PageSize.
static constexpr uint32_t PageShift = 14;
static constexpr uint32_t PageSize = 1u << PageShift; // 16 KiB
static constexpr uint32_t PageMask = PageSize - 1;
static constexpr uint32_t PagesPerChunk = 1u << (ChunkShift - PageShift);

// Sub-page granularity: exactly 64 per page, so a page's dirty extent is one machine word and the
// mask needs no addressing of its own. 64 is not a tuning parameter - it is the width of the word,
// and picking anything else would mean the map becoming a bitset again.
static constexpr uint32_t SubBlocksPerPage = 64;
static constexpr uint32_t SubShift = PageShift - 6;
static constexpr uint32_t SubSize = 1u << SubShift; // 256
static constexpr uint32_t SubMask = SubSize - 1;

// The sub-blocks from `first` to `last` inclusive. Both ends must be in [0, 63] - a shift by 64 is
// undefined, which is why the callers bound the range rather than clamping here.
static constexpr uint64_t subRange(uint32_t first, uint32_t last) {
	return (~uint64_t(0) << first) & (~uint64_t(0) >> (63 - last));
}

// Every sub-block of a page. Named because "the whole page" is a case the journal states often, and
// a bare ~0ull at a call site reads as a sentinel rather than as a mask.
static constexpr uint64_t SubAll = ~uint64_t(0);

// Two encodings of the same 64 bits, chosen at build time (SP_VSTORE_DIRTY_RANGE=1): 0, the default, is a
// bitmap of the 64 sub-blocks that were written; 1 is the packed byte extent of the write, lo in
// the high half and hi (exclusive) in the low one, which is finer than a sub-block within one run
// and cannot express two. Both agree that a zero entry means clean, which is what lets every "has
// this page been touched" test stay a compare against zero - in the range encoding hi is exclusive
// and so at least 1 whenever anything was written, so the two cannot collide. Everything downstream
// consumes dirtyMaskOf() and never the entry, so the journal, the record format and the restore
// path are identical under both.
#ifndef SP_VSTORE_DIRTY_RANGE
#define SP_VSTORE_DIRTY_RANGE 0
#endif

static constexpr bool DirtyRangeEncoding = SP_VSTORE_DIRTY_RANGE != 0;

// An entry covering the whole page. Assigned rather than merged by the paths that know a page is
// covered outright, which is why it is a value and not an operation.
#if SP_VSTORE_DIRTY_RANGE
static constexpr uint64_t DirtyAll = uint64_t(PageSize); // lo = 0, hi = PageSize
#else
static constexpr uint64_t DirtyAll = SubAll;
#endif

// Folds the byte extent [first, last] - both offsets into the page, both inclusive - into an entry.
static constexpr uint64_t dirtyMerge(uint64_t entry, uint32_t first, uint32_t last) {
#if SP_VSTORE_DIRTY_RANGE
	uint32_t lo = uint32_t(entry >> 32);
	uint32_t hi = uint32_t(entry);
	if (entry == 0) {
		lo = first;
		hi = last + 1;
	} else {
		lo = first < lo ? first : lo;
		hi = last + 1 > hi ? last + 1 : hi;
	}
	return (uint64_t(lo) << 32) | hi;
#else
	return entry | subRange(first >> SubShift, last >> SubShift);
#endif
}

// The union of two entries, as merging the dirty maps of two views needs. Under the range encoding
// that is the covering extent, not the bitwise OR of the packed halves.
static constexpr uint64_t dirtyUnion(uint64_t a, uint64_t b) {
#if SP_VSTORE_DIRTY_RANGE
	if (a == 0 || b == 0) {
		return a | b;
	}
	uint32_t lo = sprt::min(uint32_t(a >> 32), uint32_t(b >> 32));
	uint32_t hi = sprt::max(uint32_t(a), uint32_t(b));
	return (uint64_t(lo) << 32) | hi;
#else
	return a | b;
#endif
}

// The sub-blocks an entry names. The journal works in these and only these, so a range is rounded
// out here, which is where the range encoding's extra precision inside a sub-block is discarded.
static constexpr uint64_t dirtyMaskOf(uint64_t entry) {
#if SP_VSTORE_DIRTY_RANGE
	if (entry == 0) {
		return 0;
	}
	return subRange(uint32_t(entry >> 32) >> SubShift, (uint32_t(entry) - 1) >> SubShift);
#else
	return entry;
#endif
}

// How many bytes the entry really covers: the mask's popcount * SubSize under the bitmap, the
// extent itself under the range. It is the upper bound on what a byte-granular record could have
// saved, which is what journal-keyframe measures the two against each other with.
static constexpr uint32_t dirtySpanOf(uint64_t entry) {
#if SP_VSTORE_DIRTY_RANGE
	return entry == 0 ? 0 : uint32_t(entry) - uint32_t(entry >> 32);
#else
	return uint32_t(__builtin_popcountll(entry)) * SubSize;
#endif
}

// Block granularity and the maximum alignment a payload can request. Chunks are 16-aligned, the
// block header is 16 bytes and every block extent is a multiple of 16, so every payload is
// 16-aligned unconditionally - alignment costs no padding and needs no per-block bookkeeping.
static constexpr uint32_t MaxAlign = 16;
static constexpr uint32_t Granule = 16;
static constexpr uint32_t MinPayload = 16;

// Shared by the allocator and by layouts built on top of the store. Here rather than file-local,
// because a module is one compile unit: two static copies would collide at the first build.
// `align` must be a power of two - every alignment in this layer is.
static constexpr uint32_t alignUp(uint32_t value, uint32_t align) {
	return (value + align - 1) & ~(align - 1);
}

// The descriptor table lives inside chunk 0, right after Root, so the number of chunks a store can
// ever hold is bounded by what fits there. 8192 chunks is 512 MiB of store and a 32 KiB table.
static constexpr uint32_t MaxChunks = 8192;

// Versioning is a type rather than a build flag. A PlainArena announces nothing and its write()
// compiles to what its read() compiles to; a TrackedArena carries the map a Journal reads; a
// ShadowArena carries a copy of itself to check the map against. All three exist in every build,
// one process may hold all three at once, and an image passes between them because an image never
// depended on any of it. Every mutation goes through Arena::write in one source instantiated three
// times, so a barrier hole the shadow validator finds in one instantiation is a barrier hole in all
// of them. See SPVStoreTracking.h.

// Call counters, compiled out unless SP_VSTORE_COUNTERS is set: an increment inside Arena::read
// would be measuring the counter, and a tally nobody reads is a tax on every build. A per-call count
// is what checks an attribution against the code rather than against another timing - if a change
// halves a timing and does not move the count, the saving came from somewhere the model does not
// name. A consumer that counts its own calls keeps its own structure.
#ifndef SP_VSTORE_COUNTERS
#define SP_VSTORE_COUNTERS 0
#endif

struct Counters {
	uint64_t arenaRead = 0;
	uint64_t arenaWrite = 0;

	// Barrier invocations, which is not the same number as arenaWrite. Arena::write() is one way
	// in; the allocator's own metadata marks - rootForWrite, descForWrite, headerForWrite,
	// linksForWrite - are the others, and they never pass through write(). A workload that
	// allocates marks far more often than it writes, and an attribution built on arenaWrite alone
	// under-counts the barrier by that difference.
	uint64_t arenaMark = 0;
};

SP_PUBLIC Counters &getCounters();

// Whether the increments are compiled in. A constexpr rather than a macro, so that a caller decides
// in ordinary code whether its numbers mean anything. The struct and the accessor exist either way,
// so nothing needs guarding to compile.
static constexpr bool CountersEnabled = SP_VSTORE_COUNTERS != 0;

#if SP_VSTORE_COUNTERS
#define SP_VSTORE_COUNT(field) (++::stappler::vstore::getCounters().field)
#else
#define SP_VSTORE_COUNT(field) ((void)0)
#endif

struct Config {
	// Rounded down to whole chunks; also the capacity of the chunk descriptor table, so the budget
	// and the table can never disagree.
	uint32_t budgetBytes = uint32_t(64_MiB);

	// When set, the store's chunks are taken from this pool instead of a freshly created one. The
	// store then must not outlive it.
	memory::pool_t *parentPool = nullptr;

	// Keep a shadow copy of the store to check the write barrier against. Doubles the memory, and
	// is ignored by every kind of arena but ShadowArena: a caller that wants the barrier checked
	// builds a ShadowArena, and one that does not builds a TrackedArena and has nothing to turn
	// off.
	bool debugShadow = true;

	// Let the journal assert a full structural verify() after every rollback. A flag rather than a
	// kind of arena: the shadow checks the write barrier's coverage, this checks the arena's
	// structure, they fail for unrelated reasons, and this one costs nothing to carry. Only a
	// benchmark should turn it off - a whole-store sweep per rollback otherwise dominates any
	// timing of the restore path.
	bool debugVerify = true;
};

} // namespace stappler::vstore

#endif /* STAPPLER_VSTORE_SPVSTORE_H_ */
