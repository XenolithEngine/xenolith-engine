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

#ifndef STAPPLER_VSTORE_SPVSTORETRACKING_H_
#define STAPPLER_VSTORE_SPVSTORETRACKING_H_

#include "SPVStore.h"

namespace STAPPLER_VERSIONIZED stappler::vstore {

// What an arena remembers about its own writes, and the whole of the difference between a release
// store and a debugger's. A tag type rather than a build flag, which is what lets one process hold
// all three kinds and hand a store between them: the barrier is the same source instantiated three
// times, so a hole the shadow validator finds in one instantiation is a hole in every one.
// SP_PUBLIC is on the tags and not only on ArenaT, because an instantiation's visibility is the
// minimum of the template's and its arguments', and an unmarked tag would hide every symbol of the
// arena it selects. The instantiation lines themselves carry no attribute at all - an attribute
// list cannot appear there.

// The release arena's tracker. Empty, so [[no_unique_address]] gives it no storage, and every call
// below folds away: ArenaT<NoTracking>::write compiles to exactly what read compiles to, one shift
// and one add, with no preprocessor anywhere in the source that says so.
struct SP_PUBLIC NoTracking {
	static constexpr bool IsTracked = false;
	static constexpr bool HasShadow = false;

	void resize(uint32_t /*pages*/) { }
	void clear() { }
	void mark(Addr, uint32_t) { }
};

// The journal's tracker: one entry per page, indexed by the flat page number `addr >> PageShift`,
// holding one bit per SubSize sub-block (or the packed byte extent - see dirtyMerge). This is the
// whole of the arena's contact with versioning: it announces a write to itself in the cheapest form
// there is, and whoever wants to know reads the map at a moment of their own choosing.
struct SP_PUBLIC DirtyTracking {
	static constexpr bool IsTracked = true;
	static constexpr bool HasShadow = false;

	// The straight-line half of the barrier: a shift, the merge, and a store. Every field write in
	// the layers above lands here - a scalar inside one record inside one page - and that case is
	// what the shape of this function is for. The test is "does the last byte share a page with the
	// first", which is one xor rather than two divisions; a zero-length write marks nothing and
	// must not underflow into it, so it goes to the slow path, which returns immediately. It never
	// needed the arena, which is why it lives here rather than there: the page index is flat over
	// the whole address space, so two addresses share a page iff they share every bit above
	// PageShift.
	void mark(Addr a, uint32_t size) {
		uint32_t last = a + size - 1;
		if (size != 0 && size <= PageSize && ((a ^ last) >> PageShift) == 0) {
			auto &entry = _dirty[a >> PageShift];
			entry = dirtyMerge(entry, a & PageMask, last & PageMask);
			return;
		}
		markRange(a, size);
	}

	// Everything the fast path above cannot do in one word: a memset over a fresh block, a whole
	// announced chunk run. Out of line precisely so that the common case can be inlined without
	// dragging the loop along.
	void markRange(Addr, uint32_t size);

	// Grows with zeros and shrinks outright. A slot that goes away takes its marks with it: a
	// rollback that removes it has already restored everything the marks could have meant, and a
	// stale bit would name a page the store no longer has.
	void resize(uint32_t pages) { _dirty.resize(pages, 0); }
	void clear() { _dirty.assign(_dirty.size(), 0); }

	// Takes another map's marks into this one. A write view keeps a map of its own so that two
	// workers marking a page they share cannot lose each other's bits; this is where the owner
	// picks them up, on its own thread, when the workers are done. A view of a store that has grown
	// since the snapshot is shorter than this map, which is why the loop is over the source.
	void mergeFrom(const DirtyTracking &other) {
		auto n = sprt::min(_dirty.size(), other._dirty.size());
		for (size_t i = 0; i < n; ++i) { _dirty[i] = dirtyUnion(_dirty[i], other._dirty[i]); }
	}

	SpanView<uint64_t> map() const { return SpanView<uint64_t>(_dirty.data(), _dirty.size()); }

	// One entry per page, kept exactly `chunks * PagesPerChunk` long, so a page's entry is found
	// with a single shift and a slot needs no arithmetic of its own. 32 bytes a slot - 32 KiB at
	// the default budget, against a store of 64 MiB.
	mem_std::Vector<uint64_t> _dirty;
};

#if DEBUG

// The debugger's tracker: the map, plus a byte copy of the store to check the map against. The
// shadow exists because the barrier is only as good as its coverage - a single metadata write that
// went around it would leave a rollback restoring user bytes but not the allocator's own state,
// which is silent corruption rather than a visible failure. It reads the same map the journal does,
// one array with two consumers, so a hole the journal would fall into is a hole the validator sees.
// A kind rather than a flag, because doubling the store's memory is a decision and a decision
// belongs in a type. Debug only: in a release build ShadowArena is an alias of TrackedArena, since
// a release build has nothing to gain from a third instantiation of the whole stack, and the name
// still compiles so nothing above has to branch to spell it.
struct SP_PUBLIC ShadowTracking : DirtyTracking {
	static constexpr bool HasShadow = true;

	// Config::debugShadow. What it is for is keeping a caller that does not want the copy - the
	// journal's own benchmarks - from paying for a facility it never asked for; a caller that wants
	// a store without the copy builds a TrackedArena instead.
	bool _enabled = false;

	// How many times the barrier has been asked for a writable pointer. Monotone, meaningless as a
	// quantity, and useful for exactly one thing: proving that a stretch of code wrote nothing.
	uint64_t _writeSeq = 0;

	mem_std::Vector<uint8_t> _shadow; // byte copy of the chunks, in slot order

	// The thread the store belongs to: a write from any other is counted and, unless the check is
	// told otherwise, fatal. A default id is "not claimed". A read-only view admits no write.
	sprt::thread::id _owner;
	bool _readOnly = false;

	// Nonzero while the allocator must not move anything: workers are reading the store through a
	// snapshot of its chunk table.
	uint32_t _frozen = 0;

	bool admitsWrite() const {
		return !_readOnly && (_owner == sprt::thread::id() || _owner == sprt::this_thread::get_id());
	}
};

#endif

// What the shadow's ownership checks have caught, over every arena in the process. Zero in a
// release build, where there is nothing to catch with.
SP_PUBLIC uint64_t getForeignWriteCount();
SP_PUBLIC uint64_t getFrozenAllocCount();

// How many times a view was asked to allocate. Counted in every build, unlike the two above: a
// write view refuses the call and returns nothing, so this is a refusal that happened rather than a
// check that caught something, and a release build refuses just the same.
SP_PUBLIC uint64_t getViewAllocCount();

// Whether a caught violation aborts (the default) or is only counted - for the check that proves
// the counting works.
SP_PUBLIC void setArenaViolationsFatal(bool);

SP_PUBLIC void noteForeignWrite();
SP_PUBLIC void noteFrozenAlloc();
SP_PUBLIC void noteViewAlloc();

} // namespace stappler::vstore

#endif /* STAPPLER_VSTORE_SPVSTORETRACKING_H_ */
