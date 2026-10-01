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

#ifndef STAPPLER_VSTORE_SPVSTOREJOURNAL_H_
#define STAPPLER_VSTORE_SPVSTOREJOURNAL_H_

#include "SPVStoreArena.h"
#include "SPVStoreCodec.h"

// The journal is optional. Guarded here rather than at every include site, so that a header which
// needs a `vstore::Journal *` member can include this unconditionally and put the one `#if` around
// the member instead of around the include as well.

namespace STAPPLER_VERSIONIZED stappler::vstore {

// One entry of the undo log: what a page held before the first write to it in a version. There is
// at most one per (slot, page) per version - the dirty map sees to that - so the log grows with the
// working set of a version, not with the number of writes. The payload is variable-length because
// it may be compressed: `size` is what was actually stored and `flags` says how, and a record whose
// codec is None holds the page verbatim, which is both the uncompressed default and the fallback
// for an incompressible page. `Zero` means the before-image was all zeros, so nothing had to be
// stored. `Delta` means the payload is not the page but the sub-blocks named by `mask`,
// concatenated in ascending bit order, each a full SubSize bytes - so the uncompressed length is
// exactly popcount(mask) * SubSize and is a function of the record alone, and sub-blocks the mask
// does not name are not described by the record and must be left as they are found. The mask is
// meaningful either way: a whole-page record carries SubAll, so the applier has one rule rather
// than two, and the Range and Scatter delta shapes differ only in which mask reaches here.
struct JournalRecord {
	static constexpr uint16_t Zero = 1 << 0; // the page read as zeros; no payload was stored
	static constexpr uint16_t CodecShift = 1;
	static constexpr uint16_t CodecMask = uint16_t(0x7 << CodecShift); // bits 1..3
	static constexpr uint16_t Delta = 1 << 4; // payload is `mask`'s sub-blocks, not the page

	uint32_t slot;
	uint32_t blob; // granule index into the blob log; JournalNoBlob when Zero
	uint16_t page;
	uint16_t flags; // bit 0 Zero, bits 1..3 the codec, bit 4 Delta, 5..15 reserved
	uint16_t size; // stored payload bytes; the uncompressed length when raw, 0 when Zero
	uint16_t reserved;
	uint64_t mask; // sub-blocks this record restores; SubAll for a whole-page record
};

// The uncompressed length of a record's payload: what decodeBlock has to be given as its
// destination size, and what a raw record's `size` equals.
inline uint32_t getRecordPlainSize(const JournalRecord &r) {
	return (r.flags & JournalRecord::Delta) != 0 ? uint32_t(__builtin_popcountll(r.mask)) * SubSize
												 : PageSize;
}

// One contiguous span covering every set bit - the Range delta shape. Storing the untouched
// sub-blocks between the ends is not free, but the payload becomes one stretch of the page instead
// of a shuffled concatenation of fragments, and what lies between them is identical in both images
// and therefore very nearly free to compress. Which shape wins is a fact about a workload's write
// pattern; see Journal::Config::delta.
inline uint64_t widenToRange(uint64_t mask) {
	if (mask == 0) {
		return 0;
	}
	return subRange(uint32_t(__builtin_ctzll(mask)), 63u - uint32_t(__builtin_clzll(mask)));
}

static constexpr uint32_t JournalNoBlob = 0xffff'ffff;

// Whether a recorded page is stored whole or as the sub-blocks that changed, and in which shape. A
// policy rather than a rule, because the answer is a property of the workload's write pattern and
// not of the format: a single field write touches one sub-block, a table row update a short run, and
// a free-list splice touches Root at offset 0 plus a block header deep in the same page. Range wins
// the middle case and loses the last one; Scatter is the reverse.
enum class DeltaPolicy : uint8_t {
	// Always the whole page. The comparison baseline that lets a measurement say what the delta
	// bought, as distinct from what the page size bought.
	None,

	// One contiguous span from the first changed sub-block to the last.
	Range,

	// Exactly the changed sub-blocks, however scattered.
	Scatter,

	// Keep the smallest shape that was worth asking about: a wider shape is asked for only when it
	// could win without compressing, so the pass costs nothing on the deltas that dominate and
	// still catches the case where a span really is smaller than the scatter inside it. See
	// appendRecord for the rule. The surplus encodes are counted
	// (JournalStore::getSizingEncodeCount), and on ordinary workloads there are none.
	Smallest,
};

SP_PUBLIC StringView getDeltaPolicyName(DeltaPolicy);

// Free functions over a bitfield packed into an integer, as makeChunkDesc/getChunkKind already do
// for the chunk descriptors.
inline JournalCodec getRecordCodec(const JournalRecord &r) {
	return JournalCodec((r.flags & JournalRecord::CodecMask) >> JournalRecord::CodecShift);
}

inline void setRecordCodec(JournalRecord &r, JournalCodec c) {
	r.flags = uint16_t((r.flags & ~JournalRecord::CodecMask)
			| ((uint16_t(c) << JournalRecord::CodecShift) & JournalRecord::CodecMask));
}

// Page payloads, bump-allocated in segments so that growth never copies what is already stored.
// References are global, monotonic granule indices and are never rebased: an index stays valid
// while the log is trimmed from the front, which is what lets eviction rebase record offsets alone.
class SP_PUBLIC JournalBlobLog {
public:
	static constexpr uint32_t SegmentSize = 1u << 20; // 1 MiB
	static constexpr uint32_t GranulesPerSegment = SegmentSize / Granule;

	void init(memory::pool_t *);

	// A payload is one contiguous run inside one segment - getBlob() hands out a raw pointer - so
	// an append that would straddle a boundary skips to the next segment instead.
	uint32_t append(const uint8_t *data, uint32_t size);
	const uint8_t *getBlob(uint32_t ref, uint32_t size) const;

	uint32_t getEnd() const { return _end; }
	uint32_t getBase() const { return _base; }
	void truncate(uint32_t end); // drops the tail; segments are kept for refill
	void evictTo(uint32_t base); // drops the front; whole segments become reusable

	// Includes the per-payload rounding to a whole granule and the bytes skipped at a segment
	// boundary. Those are not payload, but the log really is holding them down, and this number is
	// what the record budget is spent against.
	size_t getBytes() const { return size_t(_end - _base) * Granule; }

	uint32_t getSegmentCount() const { return uint32_t(_segments.size()); }
	uint32_t getFreeSegmentCount() const { return uint32_t(_free.size()); }

private:
	uint8_t *takeSegment();

	memory::pool_t *_pool = nullptr;
	// _segments[0] is segment number _firstSegment. Front eviction moves segments to _free rather
	// than leaking them: a pool never gives an individual block back, so the log has to recycle its
	// own or a bounded budget would be unenforceable.
	mem_std::Vector<uint8_t *> _segments;
	mem_std::Vector<uint8_t *> _free;
	uint32_t _firstSegment = 0;
	uint32_t _base = 0;
	uint32_t _end = 0;
};

// A store's share of the journal, so that one journal can hold several stores on a single version
// counter without the arena knowing anything about any of it. The store is not told when a write
// happens; it is told when a version ends, reads the arena's dirty map and turns what it finds into
// undo records - which is why it has to carry a baseline, the pages already holding the new bytes
// by the time it looks. That baseline is `_base`, and its price is stated where it is declared.
class SP_PUBLIC JournalStore final {
public:
	void init(TrackedArenaRef, memory::pool_t *, JournalCodecContext *, DeltaPolicy);

	// Reads the arena's dirty map into this version's undo records, rebases `_base` onto what the
	// pages now hold, and clears the map. The whole of the journal's write path is this function.
	void capture();

	TrackedArenaRef getArena() const { return _arena; }
	uint32_t getRecordCount() const { return uint32_t(_records.size()); }
	const JournalRecord &getRecord(uint32_t i) const { return _records[i]; }
	const JournalBlobLog &getBlobs() const { return _blobs; }
	uint64_t getZeroRecordCount() const { return _zeroRecords; }

	// Pages the map named and the capture found nothing to say about - written during the version
	// and written back. Counted rather than assumed, because "half the marks are noise" is a claim
	// about a workload and belongs in a number.
	uint64_t getUnchangedPageCount() const { return _unchangedPages; }
	uint64_t getPayloadBytes() const { return _payloadBytes; }
	uint64_t getCompressedPageCount() const { return _compressedPages; }
	uint64_t getRawPageCount() const { return _rawPages; }

	// Records that stored sub-blocks rather than a page, and the sub-blocks they stored. The second
	// against getDeltaRecordCount() * SubBlocksPerPage is how much of a page a delta really carries
	// - which is the number that says whether Range is paying for the gaps it covers.
	uint64_t getDeltaRecordCount() const { return _deltaRecords; }
	uint64_t getDeltaSubBlockCount() const { return _deltaSubBlocks; }

	// Under Smallest alone: how often each candidate won. A tally rather than a ratio, because the
	// question it answers - "is the third encode ever the one that is kept" - is about the count.
	uint64_t getDeltaWholeWins() const { return _wholeWins; }
	uint64_t getDeltaRangeWins() const { return _rangeWins; }
	uint64_t getDeltaScatterWins() const { return _scatterWins; }

	// Encodes the sizing pass performed beyond the one that stores the record - what asking "is a
	// wider shape smaller" costs, as a count rather than as a timing. Every recorded page pays one
	// encode no matter what; this is the surplus, and appendRecord's whole job is to keep it near
	// zero.
	uint64_t getSizingEncodeCount() const { return _sizingEncodes; }

	// What the dirty map's entries covered, in bytes, against what the sub-blocks they round out to
	// cover. Equal under the bitmap encoding by construction; under SP_VSTORE_DIRTY_RANGE the first
	// is the byte extent and the ratio is the ceiling on what a byte-granular record could save.
	uint64_t getMarkedBytes() const { return _markedBytes; }
	uint64_t getMarkedSubBlockBytes() const { return _markedSubBlockBytes; }

	// How many pages the arena has marked and this store has not yet turned into records. The
	// store's own question - "is there anything uncommitted" - asked of the arena, which is where
	// the answer now lives.
	uint32_t getDirtyPageCount() const;

	// The same question at the granularity the map actually keeps. A page counts once above however
	// much of it was written; this says how much, and the ratio between the two is what decides
	// whether a sub-page record has anything to save on this workload.
	uint32_t getDirtySubBlockCount() const;

private:
	friend class Journal;

	// Copies the live content of one page into the baseline, or zeros when the slot is not live.
	void rebasePage(uint32_t slot, uint32_t page, uint32_t bytes = PageSize);

	// Turns one changed page into one undo record, choosing the payload's shape per _delta.
	// `changed` is the sub-blocks that really differ and is never zero.
	void appendRecord(uint32_t slot, uint32_t page, const uint8_t *before, uint64_t changed);

	// The bytes a record with this mask would store, laid out as the format wants them: the named
	// sub-blocks in ascending order. Valid until the next call - it may be a pointer into `page`.
	const uint8_t *payloadFor(const uint8_t *page, uint64_t mask);

	// Puts the whole baseline back in step with the arena. Only for attach: there is no smaller
	// answer when the store's entire content is news to us.
	void rebaseAll();

	// The same after a rollback, over the pages that can possibly have moved: the ones the records
	// restored, plus every page of a slot the rehost touched (it may have gained zeroed memory or
	// lost its memory entirely, and neither shows up as a record). Everything else is unchanged
	// since the target version by the same argument that makes the undo log complete - a page that
	// changed in an undone version has a record in it. Not rebaseAll(): a rollback is O(what was
	// undone) everywhere else, and a store of a few hundred chunks rebuilt in full per rollback is
	// O(the whole store) in a loop.
	void rebaseAfterRollback(SpanView<uint32_t> rehosted);

	// By value and kind-erased - see TrackedArenaRef. A journal watching one kind of store and not
	// another would make "which arena did you build" a question the undo log had an opinion about,
	// and one journal could then not hold a tracked scene beside a plain local store.
	TrackedArenaRef _arena;
	JournalCodecContext *_codec = nullptr; // owned by the Journal, shared by all of its stores
	DeltaPolicy _delta = DeltaPolicy::Smallest;

	// The store as it stood at the last commit: a flat, slot-indexed buffer, the same shape as
	// Arena::save()'s image. This is what deferring the capture costs - one arena's worth of memory
	// per attached store, and one page copy per page recorded - and what it buys is that a write is
	// a bit in a map and nothing else. Grown with zeros, because a slot that did not exist at the
	// last commit reads as zeros in that version's image, so zeros are its before-image.
	mem_std::Vector<uint8_t> _base;

	mem_std::Vector<JournalRecord> _records;
	JournalBlobLog _blobs;

	// Event counters, cumulative and never decremented: they describe what the journal did, not
	// what it currently holds, so eviction and rollback truncation leave them alone.
	uint64_t _zeroRecords = 0;
	uint64_t _unchangedPages = 0;
	uint64_t _payloadBytes = 0;
	uint64_t _compressedPages = 0;
	uint64_t _rawPages = 0;
	uint64_t _deltaRecords = 0;
	uint64_t _deltaSubBlocks = 0;
	uint64_t _wholeWins = 0;
	uint64_t _rangeWins = 0;
	uint64_t _scatterWins = 0;
	uint64_t _sizingEncodes = 0;
	uint64_t _markedBytes = 0;
	uint64_t _markedSubBlockBytes = 0;

	// "Oldest record wins" scratch, shared by the rollback phases: one entry per page, indexed as
	// the dirty map is, holding the sub-blocks already put back. Per sub-block and not per page,
	// which is the whole of what deltas cost the restore path: a delta answers for part of a page,
	// so later records still have something to say about the sub-blocks the earlier ones did not
	// name.
	mem_std::Vector<uint64_t> _restored;

	// Gather/scatter buffer for delta payloads, one page long. A member so that the page loop does
	// not allocate, on the same grounds as the codec's scratch.
	mem_std::Vector<uint8_t> _deltaScratch;
};

// The undo journal: a shared version counter over one or more stores, with exact rollback. A
// version is one consistent slice of every attached store, and rolling back to it restores each
// store byte for byte - "byte for byte" meaning its save() image, which is the only definition of a
// store's content the layer recognises. The counter lives here and never in the arena: a keyframe
// image taken at version 42 has to stay byte-comparable with a state reached again at 42 by rolling
// back, and it could not if the version number were part of the image.
class SP_PUBLIC Journal final {
public:
	static constexpr uint32_t MaxStores = 4;

	struct Config {
		// Undo records are dropped from the front once they exceed this, which moves the horizon
		// up to the next keyframe. A store with no keyframes keeps everything.
		size_t recordBudget = size_t(64_MiB);
		uint32_t maxKeyframes = 8;
		memory::pool_t *parentPool = nullptr;

		// Zstd where its module is linked in, uncompressed otherwise - see DefaultCodec. Anything
		// measuring page granularity itself has to set None explicitly, or it would be reporting a
		// compound of two unrelated effects.
		JournalCodec codec = DefaultCodec;

		DeltaPolicy delta = DeltaPolicy::Smallest;
	};

	~Journal();

	bool init(const Config &);

	// A nested Config cannot supply its own default argument in-class, so the no-argument form is
	// an overload rather than a default.
	bool init() { return init(Config()); }

	// The store must already be initialized: attaching takes its baseline, and there is no baseline
	// to take of a store that does not exist yet.
	uint32_t attach(TrackedArenaRef);

	// The ordinary spelling. It is where "a journal needs a store that announces its writes" is
	// enforced: a PlainArena does not satisfy the constraint, so attaching one is a compile error
	// at the call rather than an empty dirty map discovered at the first commit.
	template <typename Tracking>
	requires (Tracking::IsTracked)
	uint32_t attach(ArenaT<Tracking> *a) {
		return attach(TrackedArenaRef::of(*a));
	}

	uint32_t getStoreCount() const { return _storeCount; }
	JournalStore &getStore(uint32_t i) { return _stores[i]; }

	JournalCodec getCodec() const { return _codecCtx.getCodec(); }
	DeltaPolicy getDeltaPolicy() const { return _delta; }

	// The version being accumulated. Everything written since the last commit belongs to it.
	Version getVersion() const { return _version; }

	// The oldest version that can still be reached.
	Version getHorizon() const { return _horizon; }

	// Closes the open version and opens the next. Returns the version just closed. This is where
	// the undo log is written, not on the write that dirtied a page, which makes the cost of
	// versioning a function of how often this is called rather than of how much was written between
	// two calls, and puts "how far back must an undo reach" in the hands of whoever is doing the
	// work.
	Version commit();

	// Restores every attached store to its state at the end of version `v`.
	Status rollback(Version v);

	// Takes a full image of every store at the version just committed. Keyframes anchor the
	// retention horizon and serve as verification anchors; rollback itself is undo-only and never
	// reads one. Legal only immediately after commit(): a keyframe taken mid-version is not a
	// version's state and would be useless as an anchor.
	Status setKeyframe();

	uint32_t getKeyframeCount() const { return uint32_t(_keyframes.size()); }
	Version getKeyframeVersion(uint32_t i) const { return _keyframes[i].version; }

	// The image is stored as compressed 4 KiB blocks, so it has to be materialised rather than
	// pointed at. Deliberately not a BytesView into a hidden scratch: that would hand out a pointer
	// which the next call silently invalidates, exactly the trap the arena's "addresses are stable,
	// pointers are not" rule exists to avoid.
	Status readKeyframeImage(uint32_t i, uint32_t store, mem_std::Vector<uint8_t> &out) const;
	size_t getKeyframeImageSize(uint32_t i, uint32_t store) const;

	size_t getKeyframeBytes() const;

	struct Metrics {
		uint64_t versions = 0;
		uint64_t records = 0;
		uint64_t zeroRecords = 0;
		uint64_t unchangedPages = 0; // marked, looked at, and nothing to record
		uint64_t pageBytes = 0;
		uint64_t rollbacks = 0;
		uint64_t pagesRestored = 0;
		uint64_t keyframes = 0;

		// Compression, all cumulative. payloadBytes is what was actually stored (compressed or not)
		// against compressedPages + rawPages pages, so their ratio is the codec's real yield -
		// pageBytes above is the log's current memory, which eviction moves.
		uint64_t payloadBytes = 0;
		uint64_t compressedPages = 0;
		uint64_t rawPages = 0;

		// Sub-page deltas. deltaSubBlocks against deltaRecords * SubBlocksPerPage is how much of a
		// page a delta really carries; the three win counters are meaningful under Smallest alone.
		uint64_t deltaRecords = 0;
		uint64_t deltaSubBlocks = 0;
		uint64_t wholeWins = 0;
		uint64_t rangeWins = 0;
		uint64_t scatterWins = 0;

		// Encodes past the one that stores each record - see JournalStore::getSizingEncodeCount().
		uint64_t sizingEncodes = 0;

		// See JournalStore::getMarkedBytes(). Equal under the bitmap encoding; their ratio under
		// SP_VSTORE_DIRTY_RANGE is the ceiling on a byte-granular record's saving.
		uint64_t markedBytes = 0;
		uint64_t markedSubBlockBytes = 0;
	};

	const Metrics &getMetrics() const;
	size_t getJournalBytes() const;

private:
	struct Mark {
		Version version;
		uint32_t recordEnd[MaxStores];
		uint32_t blobEnd[MaxStores];
	};

	// One 4 KiB block of a keyframe image. The Zero flag drops an all-zero page entirely - a save()
	// image describes released slots as zeros, and there are a lot of them - which is why a
	// keyframe shrinks even under JournalCodec::None.
	struct KeyframeBlock {
		uint32_t offset; // into the keyframe's byte buffer
		uint16_t size;
		uint16_t flags; // same layout as JournalRecord::flags
	};

	struct KeyframeStore {
		KeyframeBlock *blocks;
		uint32_t blockCount; // the image is exactly blockCount * PageSize bytes
		const uint8_t *data;
		size_t dataSize;
	};

	struct Keyframe {
		Version version;
		memory::pool_t *pool; // its own, so eviction is one destroy
		KeyframeStore stores[MaxStores];
		size_t bytes;
	};

	void evictKeyframes();
	void dropKeyframesAbove(Version);

	// Reads the restored metadata of one store out of the undo log without touching the store.
	Status planStore(uint32_t index, uint32_t recordFrom, RehostPlan &,
			mem_std::Vector<uint8_t> &scratch);

	void applyRecords(uint32_t index, uint32_t recordFrom, bool metadataOnly);

	// One record's contribution to one page: the sub-blocks in `apply`, which is a non-empty subset
	// of the record's own mask. ErrorNotFound for a blob the log no longer holds,
	// ErrorNotRecoverable for one that will not decode - the two callers differ on what those mean
	// to them.
	Status applyOne(JournalStore &, const JournalRecord &, uint64_t apply, uint8_t *dst);

	memory::pool_t *_pool = nullptr;
	bool _ownsPool = false;

	// One per journal, not per store: the stores are single-threaded together and share the pool,
	// so four of them share one ZSTD_CCtx rather than holding four.
	JournalCodecContext _codecCtx;

	JournalStore _stores[MaxStores];
	uint32_t _storeCount = 0;

	Version _version = 1;
	Version _horizon = 0;
	mem_std::Vector<Mark> _marks;

	mem_std::Vector<Keyframe> _keyframes;

	size_t _recordBudget = 0;
	uint32_t _maxKeyframes = 0;
	DeltaPolicy _delta = DeltaPolicy::Smallest;
	mutable Metrics _metrics;
};

} // namespace stappler::vstore


#endif /* STAPPLER_VSTORE_SPVSTOREJOURNAL_H_ */
