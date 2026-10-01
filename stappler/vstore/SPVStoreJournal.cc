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

// The undo journal. The whole mechanism rests on one property of the arena: every mutation goes
// through the write barrier, the allocator's own metadata included, and the barrier marks the page
// it lands on. The journal is not told and does not run; it reads the map when a version closes,
// and putting the pages it found back, in order, restores the version. Three rules do most of the
// work, and each is easy to get subtly wrong. A page's previous content is what save() would have
// emitted for it at the last commit - zeros for a slot that was not live then - and it comes from
// `_base`, the store's own baseline, because by the time this code looks the pages hold the new
// bytes; that is also what makes a recycled buffer safe with no ordering rule at all, a slot the
// store did not have at the last commit having a baseline of zeros by construction. A marked page
// is a page to look at, not a page to record: written and written back inside one version is the
// common case rather than the exotic one, and such a page has no undo, so capture() compares and
// skips. And records are applied oldest first, the first record for a page winning, because the
// oldest record in (V, now] is the one holding V's content.

#include "SPVStoreJournal.h"

#include "SPLog.h"

#include <sprt/c/__sprt_string.h>

namespace STAPPLER_VERSIONIZED stappler::vstore {

// A page that was zeros before the version touched it needs no payload at all. Worth a scan of its
// own rather than leaving it to the codec: the answer skips an encode and an append, and the pages
// it catches are not rare - every page of a chunk the store has just grown into is one. Read as
// 64-bit words because the baseline is a pool allocation at MaxAlign and a page is a whole number
// of them, and the loop stops at the first non-zero. `bytes` is the page's footprint, not always
// the page: the store's first page holds only the superblock and the live part of the descriptor
// table, and whatever the arena says is the bound is the bound.
static bool isZeroPage(const uint8_t *page, uint32_t bytes) {
	auto words = reinterpret_cast<const uint64_t *>(page);
	for (uint32_t i = 0; i < bytes / sizeof(uint64_t); ++i) {
		if (words[i] != 0) {
			return false;
		}
	}
	return true;
}

// Which of the sub-blocks the barrier marked really differ from the baseline. `live` is null for a
// slot that is not live, whose image is zeros by definition - the same comparison against a zero
// page, which is why this is one function and not two. Bits at or past `footprint` are not examined
// at all: the only page whose footprint is under a page is the store's header page, and by ARENA-P0
// its tail is descriptor-table capacity that nothing has ever written.
static uint64_t changedSubBlocks(const uint8_t *before, const uint8_t *live, uint64_t marks,
		uint32_t footprint) {
	uint64_t changed = 0;
	for (uint64_t rest = marks; rest != 0; rest &= rest - 1) {
		uint32_t s = uint32_t(__builtin_ctzll(rest));
		uint32_t at = s * SubSize;
		if (at >= footprint) {
			break; // the bits come out ascending, so nothing after this one is inside either
		}

		uint32_t n = footprint - at < SubSize ? footprint - at : SubSize;
		bool same = live ? __builtin_memcmp(before + at, live + at, n) == 0
						 : isZeroPage(before + at, n);
		if (!same) {
			changed |= uint64_t(1) << s;
		}
	}
	return changed;
}

StringView getDeltaPolicyName(DeltaPolicy policy) {
	switch (policy) {
	case DeltaPolicy::None: return StringView("whole");
	case DeltaPolicy::Range: return StringView("range");
	case DeltaPolicy::Scatter: return StringView("scatter");
	case DeltaPolicy::Smallest: return StringView("smallest");
	}
	return StringView("?");
}

void JournalBlobLog::init(memory::pool_t *pool) {
	_pool = pool;
	_segments.clear();
	_free.clear();
	_firstSegment = 0;
	_base = 0;
	_end = 0;
}

uint8_t *JournalBlobLog::takeSegment() {
	if (!_free.empty()) {
		auto mem = _free.back();
		_free.pop_back();
		return mem;
	}
	return reinterpret_cast<uint8_t *>(memory::pool::palloc(_pool, SegmentSize, MaxAlign));
}

uint32_t JournalBlobLog::append(const uint8_t *data, uint32_t size) {
	uint32_t granules = (size + Granule - 1) / Granule;

	// A payload must not straddle two segments: getBlob() returns a plain pointer into one of them.
	// The skipped tail is at most 4080 bytes per 1 MiB and stays counted in getBytes() - nothing
	// reclaims it, deliberately, because doing so would need a free list inside the log.
	uint32_t space = GranulesPerSegment - (_end % GranulesPerSegment);
	if (granules > space) {
		_end += space;
	}

	// The reference counter is global and never rebased, so a wrap would silently invalidate every
	// bounds test in getBlob(). 64 GiB is 1000x the default budget; if it is ever reached, that is
	// a leak somewhere in retention, not a case to handle gracefully.
	sprt_passert(_end + granules >= _end, "vstore::JournalBlobLog: blob reference overflow");

	uint32_t ref = _end;
	uint32_t local = ref / GranulesPerSegment - _firstSegment;

	if (local >= _segments.size()) {
		auto mem = takeSegment();
		if (!mem) {
			return JournalNoBlob;
		}
		_segments.emplace_back(mem);
	}

	__sprt_memcpy(_segments[local] + size_t(ref % GranulesPerSegment) * Granule, data, size);
	_end += granules;
	return ref;
}

const uint8_t *JournalBlobLog::getBlob(uint32_t ref, uint32_t size) const {
	uint32_t granules = (size + Granule - 1) / Granule;
	if (ref < _base || ref >= _end || granules > _end - ref) {
		return nullptr;
	}
	uint32_t local = ref / GranulesPerSegment - _firstSegment;
	if (local >= _segments.size()) {
		return nullptr;
	}
	return _segments[local] + size_t(ref % GranulesPerSegment) * Granule;
}

void JournalBlobLog::truncate(uint32_t end) {
	// Segments are kept: the next version refills them, and a pool would not take them back anyway.
	if (end < _end) {
		_end = end < _base ? _base : end;
	}
}

void JournalBlobLog::evictTo(uint32_t base) {
	if (base <= _base) {
		return;
	}
	_base = base > _end ? _end : base;

	// Whole segments below the new base are recycled rather than dropped.
	while (_base / GranulesPerSegment > _firstSegment && !_segments.empty()) {
		_free.emplace_back(_segments.front());
		_segments.erase(_segments.begin());
		++_firstSegment;
	}

	// Drained completely: no reference is live, which is the one and only moment at which the
	// counter may be renumbered. Doing it here keeps a long session away from the 64 GiB ceiling.
	if (_base == _end && _segments.empty()) {
		_base = _end = _firstSegment = 0;
	}
}

void JournalStore::init(TrackedArenaRef arena, memory::pool_t *pool, JournalCodecContext *codec,
		DeltaPolicy delta) {
	_arena = arena;
	_arena.noteObserver();
	_codec = codec;
	_delta = delta;
	_blobs.init(pool);
	_records.clear();
	_deltaScratch.assign(PageSize, 0);

	// The baseline is taken, and the arena's map is deliberately left alone. It is not this store's
	// to clear: in a debug build the shadow validator is also keeping score in it, and a store that
	// was written into before the journal arrived still owes that validator an explanation. Marks
	// that predate the attach cost one record each on the first commit, whose payload is the state
	// at the attach - which is what an undo to version 1 should put back anyway.
	rebaseAll();
}

// `bytes` under a page is only ever the store's first page, and only from capture(): what follows
// the footprint is zeros on both sides and copying it would be copying zeros over zeros. The paths
// that rebuild a baseline wholesale keep the default, because a rollback can move the footprint
// down - it is derived from chunkCount - and a truncated copy there would leave the tail of the
// baseline holding bytes the store no longer has.
void JournalStore::rebasePage(uint32_t slot, uint32_t page, uint32_t bytes) {
	auto dst = _base.data() + size_t(slot) * ChunkSize + size_t(page) * PageSize;
	if (auto src = _arena.getPage(slot, page)) {
		__sprt_memcpy(dst, src, bytes);
	} else {
		// Not live, so its image is zeros - which is the same rule the growth path relies on,
		// applied to a slot that has gone away rather than to one that has arrived.
		__sprt_memset(dst, 0, PageSize);
	}
}

void JournalStore::rebaseAll() {
	_base.assign(size_t(_arena.getChunkCount()) * ChunkSize, 0);
	for (uint32_t slot = 0; slot < _arena.getChunkCount(); ++slot) {
		for (uint32_t p = 0; p < PagesPerChunk; ++p) { rebasePage(slot, p); }
	}
}

void JournalStore::rebaseAfterRollback(SpanView<uint32_t> rehosted) {
	uint32_t count = _arena.getChunkCount();
	_base.resize(size_t(count) * ChunkSize, 0);

	for (auto slot : rehosted) {
		if (slot < count) {
			for (uint32_t p = 0; p < PagesPerChunk; ++p) { rebasePage(slot, p); }
		}
	}

	for (uint32_t index = 0; index < _restored.size(); ++index) {
		if (_restored[index] == 0) {
			continue;
		}
		uint32_t slot = index >> (ChunkShift - PageShift);
		if (slot >= count) {
			break; // the index ascends, so every page after this one is out of the store too
		}
		// The whole page, not the sub-blocks that were put back: whatever the records restored, the
		// baseline as a whole has to describe the state the store is in now.
		rebasePage(slot, index & (PagesPerChunk - 1));
	}
}

void JournalStore::capture() {
	// Growth first, and with zeros: a slot the store gained since the last commit has a baseline of
	// zeros, which is exactly what that version's image says about it.
	size_t want = size_t(_arena.getChunkCount()) * ChunkSize;
	if (_base.size() != want) {
		_base.resize(want, 0);
	}

	// Indexed by flat page number, exactly as the arena's map is - and _base has the same shape as
	// a save() image, so page N of the baseline is simply N * PageSize into it, with no per-slot
	// arithmetic on the way.
	auto map = _arena.getDirtyMap();
	for (uint32_t index = 0; index < map.size(); ++index) {
		if (map[index] == 0) {
			continue; // the overwhelmingly common page in a version that touched a handful of them
		}
		uint64_t marks = dirtyMaskOf(map[index]);

		// What the entry covers in bytes against what the sub-blocks it rounds out to do. Under the
		// bitmap the two are equal by construction; under the range encoding the gap is the upper
		// bound on what a byte-granular record could have saved, and it is the only reason to want
		// one. Counted here because this is the last place the entry exists.
		_markedBytes += dirtySpanOf(map[index]);
		_markedSubBlockBytes += uint32_t(__builtin_popcountll(marks)) * SubSize;

		uint32_t slot = index >> (ChunkShift - PageShift);
		uint32_t p = index & (PagesPerChunk - 1);

		auto before = _base.data() + size_t(index) * PageSize;

		// How much of the page can differ at all. It is a page everywhere but slot 0 page 0, which
		// is the header - and that is the page marked most often of all, because every alloc and
		// every free moves the allocator's counters inside it.
		auto footprint = _arena.getPageFootprint(slot, p);
		auto live = _arena.getPage(slot, p);

		// A page can be written and written back inside one version, and a great many are: a run
		// that gives back everything it took leaves the allocator's counters and free lists exactly
		// where it found them, so slot 0 is marked on every unit of work and identical at the end
		// of most of them. So the map says where to look and this says whether there is anything to
		// say about it. It is not an approximation - a page whose content did not change across the
		// version has no undo, and rollback leaving it alone is exactly right. The comparison runs
		// over the marked sub-blocks alone, which is what the 64-bit map bought.
		uint64_t changed = changedSubBlocks(before, live, marks, footprint);
		if (changed == 0) {
			++_unchangedPages;
			continue;
		}

		if (isZeroPage(before, footprint)) {
			++_zeroRecords;
			_records.emplace_back(JournalRecord{slot, JournalNoBlob, uint16_t(p),
				JournalRecord::Zero, 0, 0, SubAll});
		} else {
			appendRecord(slot, p, before, changed);
		}

		// The second copy. It happens per recorded page, so it is a cost of taking a version
		// boundary rather than a cost of writing, and it covers the changed sub-blocks rather than
		// the page, on the same grounds the comparison above narrowed: the rest of the page already
		// equals the baseline.
		if (live) {
			for (uint64_t rest = changed; rest != 0; rest &= rest - 1) {
				auto at = size_t(__builtin_ctzll(rest)) * SubSize;
				__sprt_memcpy(before + at, live + at, SubSize);
			}
		} else {
			rebasePage(slot, p, footprint);
		}
	}

	_arena.clearDirtyMap();
}

const uint8_t *JournalStore::payloadFor(const uint8_t *page, uint64_t mask) {
	// A contiguous mask needs no gather at all: the sub-blocks it names are already adjacent in the
	// page, so the payload is a stretch of the page and the copy is a pointer. That, rather than
	// the byte count, is what Range is really buying - and it is why the whole-page case (mask ==
	// SubAll, which is contiguous) needs no arm of its own here.
	if (mask == widenToRange(mask)) {
		return page + size_t(__builtin_ctzll(mask)) * SubSize;
	}

	uint32_t at = 0;
	for (uint64_t rest = mask; rest != 0; rest &= rest - 1) {
		__sprt_memcpy(_deltaScratch.data() + at, page + size_t(__builtin_ctzll(rest)) * SubSize,
				SubSize);
		at += SubSize;
	}
	return _deltaScratch.data();
}

void JournalStore::appendRecord(uint32_t slot, uint32_t page, const uint8_t *before,
		uint64_t changed) {
	uint64_t rangeMask = widenToRange(changed);

	// The shapes this page's payload could take, narrowest first. The order is load-bearing twice
	// over: it is what lets the loop below stop asking, since `changed` is a subset of `rangeMask`
	// is a subset of `SubAll` and plain sizes only grow; and it makes `<=` the right comparison for
	// the winner, so that on a tie the wider, more contiguous shape - fewer memcpys to put back -
	// wins.
	uint64_t cand[3];
	uint32_t n = 0;
	switch (_delta) {
	case DeltaPolicy::None: cand[n++] = SubAll; break;
	case DeltaPolicy::Range: cand[n++] = rangeMask; break;
	case DeltaPolicy::Scatter: cand[n++] = changed; break;
	case DeltaPolicy::Smallest:
		cand[n++] = changed;
		if (rangeMask != changed) {
			cand[n++] = rangeMask;
		}
		if (SubAll != rangeMask) {
			cand[n++] = SubAll;
		}
		break;
	}

	// The sizing pass, and the two rules that keep it from costing more than it saves. A wider
	// shape is only asked if it could win without compressing: to beat the best answer so far it
	// would have to compress below it, and even then it saves at most `bestSize` bytes while the
	// asking costs `plain` bytes of compression - which for a one-sub-block delta is 16 KiB of work
	// bidding for 256 bytes of log, on every recorded page, win or lose. What that gives up is the
	// page of repeating pattern that folds below the delta; a wholly-zero baseline still costs
	// nothing to spot, since capture() routes it to a Zero record before reaching here. And the
	// winner is not re-encoded: the codec keeps one scratch, so a later encode invalidates an
	// earlier answer, but under the rule above the winner is very nearly always the last shape
	// encoded and the scratch still holds exactly what is about to be stored. `held` is that
	// condition, tracked rather than assumed.
	uint64_t mask = cand[0];
	const uint8_t *data = nullptr;
	uint32_t size = 0;
	auto used = JournalCodec::None;
	bool held = false;

	for (uint32_t i = 0; i < n; ++i) {
		uint32_t plain = uint32_t(__builtin_popcountll(cand[i])) * SubSize;
		if (i > 0) {
			if (plain > size) {
				continue;
			}
			++_sizingEncodes; // every encode past the first is what the pass costs
		}

		auto candSrc = payloadFor(before, cand[i]);
		const uint8_t *out = nullptr;
		uint32_t outSize = 0;
		auto codec = _codec->encodeBlock(candSrc, plain, out, outSize);
		if (codec == JournalCodec::None) {
			out = candSrc;
			outSize = plain;
		}

		// Whatever was in the scratches belongs to this candidate now, so a previous winner's
		// pointers are stale from here whether it wins or not.
		held = false;
		if (i == 0 || outSize <= size) {
			mask = cand[i];
			data = out;
			size = outSize;
			used = codec;
			held = true;
		}
	}

	if (!held) {
		// A wider shape was asked and lost, so both the codec's scratch and the gather buffer have
		// moved on since the winner was sized. Re-derive it - and count it, because it is an encode
		// the record did not need: the winner had already been encoded once, and only the single
		// scratch is why the answer could not be kept.
		++_sizingEncodes;

		uint32_t plain = uint32_t(__builtin_popcountll(mask)) * SubSize;
		auto src = payloadFor(before, mask);
		data = src;
		size = plain;
		used = _codec->encodeBlock(src, plain, data, size);
		if (used == JournalCodec::None) {
			data = src;
			size = plain;
		}
	}

	// Whether the compressed form is kept was decided per payload by encodeBlock: an incompressible
	// one is stored verbatim under JournalCodec::None, and the record carries the codec that was
	// applied rather than the one that was asked for.
	if (used == JournalCodec::None) {
		++_rawPages;
	} else {
		++_compressedPages;
	}
	_payloadBytes += size;

	uint16_t flags = 0;
	if (mask != SubAll) {
		flags |= JournalRecord::Delta;
		++_deltaRecords;
		_deltaSubBlocks += uint32_t(__builtin_popcountll(mask));
	}
	if (_delta == DeltaPolicy::Smallest) {
		if (mask == SubAll) {
			++_wholeWins;
		} else if (mask == rangeMask) {
			++_rangeWins;
		} else {
			++_scatterWins;
		}
	}

	JournalRecord rec{slot, _blobs.append(data, size), uint16_t(page), flags, uint16_t(size), 0,
		mask};
	setRecordCodec(rec, used);
	_records.emplace_back(rec);
}

uint32_t JournalStore::getDirtyPageCount() const {
	uint32_t total = 0;
	for (auto it : _arena.getDirtyMap()) {
		if (it != 0) {
			++total;
		}
	}
	return total;
}

uint32_t JournalStore::getDirtySubBlockCount() const {
	uint32_t total = 0;
	for (auto it : _arena.getDirtyMap()) {
		total += uint32_t(__builtin_popcountll(dirtyMaskOf(it)));
	}
	return total;
}

Journal::~Journal() {
	// The stores stop watching. A journal holds a reference to each arena for its whole life and
	// there is nothing else that ends that life, so this is where it ends - and until it does,
	// Arena::adoptFrom refuses to move the store out from under the reference.
	for (uint32_t i = 0; i < _storeCount; ++i) { _stores[i]._arena.forgetObserver(); }
	_storeCount = 0;

	// A keyframe carries a pool of its own - an image is sized once at capture and freed whole -
	// and that pool is a sibling of this journal's, not a child of it, so destroying _pool below
	// does not reach it. Eviction releases the ones it drops but never takes the last keyframe, so
	// there is always at least one left for this loop.
	for (auto &kf : _keyframes) { memory::pool::destroy(kf.pool); }
	_keyframes.clear();

	if (_pool && _ownsPool) {
		memory::pool::destroy(_pool);
	}
	_pool = nullptr;
}

bool Journal::init(const Config &cfg) {
	if (_pool) {
		return false;
	}

	_pool = cfg.parentPool;
	_ownsPool = false;
	if (!_pool) {
		_pool = memory::pool::create();
		_ownsPool = true;
	}
	if (!_pool) {
		return false;
	}

	// A codec whose optional module is not linked in fails here rather than at compile time: the
	// module set is a build fact, while the codec is a config value that may come from settings.
	if (_codecCtx.init(cfg.codec, _pool) != Status::Ok) {
		if (_ownsPool) {
			memory::pool::destroy(_pool);
		}
		_pool = nullptr;
		_ownsPool = false;
		return false;
	}

	_recordBudget = cfg.recordBudget;
	_maxKeyframes = cfg.maxKeyframes;
	_delta = cfg.delta;
	_version = 1;
	_horizon = 0;
	_marks.clear();

	// Version 0 is the state every attached store is in when it joins, so rolling back to it is
	// meaningful from the first moment.
	_marks.emplace_back(Mark{0, {}, {}});
	return true;
}

uint32_t Journal::attach(TrackedArenaRef arena) {
	sprt_passert(_pool != nullptr, "vstore::Journal: attach before init()");
	sprt_passert(_storeCount < MaxStores, "vstore::Journal: too many stores");
	sprt_passert(arena.isInitialized(), "vstore::Journal: attaching an uninitialized store");

	uint32_t index = _storeCount++;
	_stores[index].init(arena, _pool, &_codecCtx, _delta);
	return index;
}

Version Journal::commit() {
	Version closed = _version;

	Mark mark{closed, {}, {}};
	for (uint32_t i = 0; i < _storeCount; ++i) {
#if DEBUG
		// The shadow proves the barrier had no holes over this version - which is the same thing as
		// proving the map the capture below is about to read is complete. It has to run before the
		// capture, because the capture clears the very map it judges.
		if (_stores[i]._arena.hasShadow()) {
			auto st = _stores[i]._arena.validate([&](uint32_t slot, uint32_t page) {
				slog().error("vstore::Journal", "commit: store ", i, " changed slot ", slot,
						" page ", page, " without marking it");
			});
			sprt_passert(st == Status::Ok, "vstore::Journal: unmarked write in this version");
		}
#endif

		_stores[i].capture();
		mark.recordEnd[i] = uint32_t(_stores[i]._records.size());
		mark.blobEnd[i] = _stores[i]._blobs.getEnd();

#if DEBUG
		// Resyncing here makes the next version's validation start from the state just committed.
		// The capture already cleared the map, and resync() clears it again - harmlessly, and
		// rather than teaching one of them about the other.
		if (_stores[i]._arena.hasShadow()) {
			_stores[i]._arena.resync();
		}
#endif
	}
	_marks.emplace_back(mark);

	++_version;
	++_metrics.versions;
	return closed;
}

// Reads what the store's metadata looked like at the target version, without touching the store.
// Slot 0 carries Root and the descriptor table, so restoring only its pages into a scratch copy is
// enough to learn the target's chunk count and layout.
Status Journal::planStore(uint32_t index, uint32_t recordFrom, RehostPlan &plan,
		mem_std::Vector<uint8_t> &scratch) {
	auto &store = _stores[index];
	auto arena = store._arena;

	scratch.assign(ChunkSize, 0);
	for (uint32_t p = 0; p < PagesPerChunk; ++p) {
		if (auto src = arena.getPage(0, p)) {
			__sprt_memcpy(scratch.data() + size_t(p) * PageSize, src, PageSize);
		}
	}

	uint64_t seen[PagesPerChunk] = {};
	for (uint32_t i = recordFrom; i < store._records.size(); ++i) {
		auto &r = store._records[i];
		if (r.slot != 0) {
			continue;
		}

		uint64_t apply = r.mask & ~seen[r.page];
		if (apply == 0) {
			continue; // oldest wins, per sub-block - see applyRecords
		}
		seen[r.page] |= r.mask;

		auto st = applyOne(store, r, apply, scratch.data() + size_t(r.page) * PageSize);
		if (st != Status::Ok) {
			slog().error("vstore::Journal", "rollback: store ", index, " could not apply blob ",
					r.blob, " as ", getCodecName(getRecordCodec(r)));
			return st;
		}
	}

	auto root = reinterpret_cast<const Root *>(scratch.data());
	if (root->magic != RootMagic || root->chunkShift != ChunkShift
			|| root->pageShift != PageShift) {
		slog().error("vstore::Journal", "rollback: store ", index, " restored a broken superblock");
		return Status::ErrorNotRecoverable;
	}

	auto targetDescs = reinterpret_cast<const ChunkDesc *>(scratch.data() + DescsOffset);
	return arena.planRehost(targetDescs, root->chunkCount, plan);
}

// Puts the sub-blocks in `apply` back into `dst`, out of one record. `apply` is always a subset of
// r.mask and is never zero. A record that covers the page, applied to a page nothing has restored
// yet, decodes straight into the destination with no bounce buffer; anything narrower has to land
// somewhere first, because a decoder writes all of its output or none of it.
Status Journal::applyOne(JournalStore &store, const JournalRecord &r, uint64_t apply,
		uint8_t *dst) {
	if ((r.flags & JournalRecord::Zero) != 0) {
		if (apply == SubAll) {
			__sprt_memset(dst, 0, PageSize);
		} else {
			for (uint64_t rest = apply; rest != 0; rest &= rest - 1) {
				__sprt_memset(dst + size_t(__builtin_ctzll(rest)) * SubSize, 0, SubSize);
			}
		}
		return Status::Ok;
	}

	auto blob = store._blobs.getBlob(r.blob, r.size);
	if (!blob) {
		return Status::ErrorNotFound;
	}

	uint32_t plain = getRecordPlainSize(r);
	if (apply == SubAll) {
		return _codecCtx.decodeBlock(getRecordCodec(r), blob, r.size, dst, plain)
				? Status::Ok
				: Status::ErrorNotRecoverable;
	}

	// The payload is decoded whole - it is at most a page, which is what the gather buffer is - and
	// then only the wanted sub-blocks are scattered out of it. `at` walks the payload while `s`
	// walks the page, which is the same correspondence payloadFor() wrote it with.
	auto buf = store._deltaScratch.data();
	if (!_codecCtx.decodeBlock(getRecordCodec(r), blob, r.size, buf, plain)) {
		return Status::ErrorNotRecoverable;
	}

	uint32_t at = 0;
	for (uint64_t rest = r.mask; rest != 0; rest &= rest - 1) {
		uint32_t s = uint32_t(__builtin_ctzll(rest));
		if ((apply & (uint64_t(1) << s)) != 0) {
			__sprt_memcpy(dst + size_t(s) * SubSize, buf + at, SubSize);
		}
		at += SubSize;
	}
	return Status::Ok;
}

void Journal::applyRecords(uint32_t index, uint32_t recordFrom, bool metadataOnly) {
	auto &store = _stores[index];
	auto arena = store._arena;

	for (uint32_t i = recordFrom; i < store._records.size(); ++i) {
		auto &r = store._records[i];
		if ((r.slot == 0) != metadataOnly) {
			continue;
		}
		// Whether the page can be written at all is Phase 2's decision, read here rather than
		// derived again: a slot that is Empty at the target reads as zeros regardless of what any
		// record says, and a second opinion could only disagree.
		if (!metadataOnly && !arena.isHosted(r.slot)) {
			continue;
		}

		uint32_t pageIndex = r.slot * PagesPerChunk + r.page;
		if (pageIndex >= store._restored.size()) {
			store._restored.resize(pageIndex + 1, 0);
		}

		// Oldest wins, per sub-block: a delta answers only for what it names, so a later record
		// still has something to say about the sub-blocks no earlier one covered. Getting this
		// wrong does not corrupt a page visibly - it restores part of it to the wrong version,
		// which is what journal-rollback's byte-exact oracle exists to catch.
		uint64_t apply = r.mask & ~store._restored[pageIndex];
		if (apply == 0) {
			continue;
		}
		store._restored[pageIndex] |= r.mask;

		// This phase cannot fail - it is past the point where the store could be left untouched,
		// and a failed decode has already written part of the destination. A corrupt blob log is
		// neither recoverable nor distinguishable from a corrupt arena, so there is no fallback to
		// invent.
		if (applyOne(store, r, apply, arena.getPageForRestore(r.slot, r.page)) != Status::Ok) {
			slog().error("vstore::Journal", "rollback: store ", index, " could not apply blob ",
					r.blob, " as ", getCodecName(getRecordCodec(r)));
			sprt_passert(false, "vstore::Journal: undecodable payload during rollback");
		}
		++_metrics.pagesRestored;
	}
}

Status Journal::rollback(Version v) {
	// Only closed versions can be targets: the open one has no mark, and "roll back to the version
	// I am still writing" has no meaning.
	if (v >= _version) {
		slog().error("vstore::Journal", "rollback: version ", v, " is not closed (open is ",
				_version, ")");
		return Status::ErrorInvalidArguemnt;
	}
	if (v < _horizon) {
		return Status::ErrorNotFound;
	}

	// Whatever has been written since the last boundary belongs to the open version, and the open
	// version is one of the ones being undone - so it has to become records before the undo can
	// find it. Without this line a rollback would quietly leave the newest work in place.
	for (uint32_t i = 0; i < _storeCount; ++i) { _stores[i].capture(); }

	size_t markIndex = _marks.size();
	for (size_t i = 0; i < _marks.size(); ++i) {
		if (_marks[i].version == v) {
			markIndex = i;
			break;
		}
	}
	if (markIndex == _marks.size()) {
		return Status::ErrorNotFound;
	}
	auto &mark = _marks[markIndex];

	// Phase 0 - plan. The only phase that can fail, and it touches nothing: if a store cannot get
	// the memory its old layout needs, everything is left exactly as it was.
	RehostPlan plans[MaxStores];
	mem_std::Vector<uint8_t> scratch;
	for (uint32_t i = 0; i < _storeCount; ++i) {
		auto st = planStore(i, mark.recordEnd[i], plans[i], scratch);
		if (st != Status::Ok) {
			for (uint32_t k = 0; k < i; ++k) { _stores[k]._arena.discardRehost(plans[k]); }
			return st;
		}
	}

	mem_std::Vector<uint32_t> rehosted;
	for (uint32_t i = 0; i < _storeCount; ++i) {
		auto &store = _stores[i];
		// Indexed by page, like the dirty map: sized for whichever of the two shapes has more
		// slots, since records name the store as it was and the plan names it as it will be.
		store._restored.assign(size_t(store._arena.getChunkCount() > plans[i].chunkCount
											   ? store._arena.getChunkCount()
											   : plans[i].chunkCount)
						* PagesPerChunk,
				0);

		// Taken before Phase 2, which consumes the plan. These are the slots whose content can
		// change without any record saying so - one that gains memory it never had is zeroed, one
		// that loses its memory becomes a hole - so the baseline has to be re-read over them.
		rehosted.clear();
		for (auto &e : plans[i].entries) {
			for (uint32_t k = 0; k < e.count; ++k) { rehosted.emplace_back(e.slot + k); }
		}

		// Phase 1 - metadata. Slot 0 holds Root and the descriptor table; its buffer always exists,
		// so this is always writable. Afterwards the descriptors describe the target and the host
		// chunk table does not yet agree with them: nothing may read the store until Phase 2.
		applyRecords(i, mark.recordEnd[i], true);

		// Phase 2 - hosting. Puts the host table back in step, carrying each slot's bytes to its
		// new home rather than trusting Phase 3 to cover every page of it.
		store._arena.rehost(plans[i]);

		// Phase 3 - data.
		applyRecords(i, mark.recordEnd[i], false);

		// Phase 4 - forget the undone versions.
		store._records.resize(mark.recordEnd[i]);
		store._blobs.truncate(mark.blobEnd[i]);

		// Phase 5 - the baseline describes a state that never happened now, over exactly the pages
		// the phases above put back.
		store.rebaseAfterRollback(SpanView<uint32_t>(rehosted.data(), rehosted.size()));
		store._arena.clearDirtyMap();

		// The bytes a layer above derived anything from have just been replaced under it. This is
		// the notification that makes a cache checking the stamp correct across a rollback; without
		// it nothing host-side could hold anything derived from an address.
		store._arena.invalidateDerived();

#if DEBUG
		if (store._arena.hasShadow()) {
			store._arena.resync();
		}
		// A whole-store structural sweep per rollback. Gated separately from the shadow because it
		// is the single thing that would dominate a measurement of the restore path.
		if (store._arena.hasVerifyEnabled()) {
			sprt_passert(store._arena.verify() == Status::Ok,
					"vstore::Journal: rollback produced a broken store");
		}
#endif
	}

	_marks.resize(markIndex + 1);

	// The versions above the target never happened now. Keeping a keyframe from that timeline
	// would leave two keyframes able to claim the same version number, and the one describing a
	// state that no longer exists would be the older claim.
	dropKeyframesAbove(v);

	_version = v + 1;
	++_metrics.rollbacks;
	return Status::Ok;
}

Status Journal::setKeyframe() {
	// Only a committed state is a version's state. A keyframe taken with pages still dirty would
	// describe a moment no version ever had, and anchoring the horizon to it would be meaningless.
	for (uint32_t i = 0; i < _storeCount; ++i) {
		if (_stores[i].getDirtyPageCount() != 0) {
			slog().error("vstore::Journal", "setKeyframe: store ", i, " has uncommitted changes");
			return Status::ErrorInvalidArguemnt;
		}
	}
	if (_marks.empty()) {
		return Status::ErrorInvalidArguemnt;
	}

	Keyframe kf{_marks.back().version, nullptr, {}, 0};
	kf.pool = memory::pool::create();
	if (!kf.pool) {
		return Status::ErrorOutOfHostMemory;
	}

	// Accumulated in a vector and copied into an exactly-sized pool block afterwards: the final
	// length is not knowable in advance, and reserving the uncompressed size up front would defeat
	// the point.
	mem_std::Vector<uint8_t> payload;

	for (uint32_t i = 0; i < _storeCount; ++i) {
		auto arena = _stores[i]._arena;
		uint32_t blockCount = uint32_t(arena.saveSize() / PageSize);

		auto blocks = reinterpret_cast<KeyframeBlock *>(
				memory::pool::palloc(kf.pool, sizeof(KeyframeBlock) * blockCount, MaxAlign));
		if (!blocks) {
			memory::pool::destroy(kf.pool);
			return Status::ErrorOutOfHostMemory;
		}

		payload.clear();
		uint32_t at = 0;

		// save() emits whole chunks, so every piece is a multiple of ChunkSize and splitting it
		// into pages needs no carry across pieces.
		arena.save([&](const uint8_t *data, size_t bytes, bool zero) {
			for (size_t off = 0; off < bytes; off += PageSize) {
				auto &block = blocks[at++];
				if (zero) {
					block = KeyframeBlock{uint32_t(payload.size()), 0, JournalRecord::Zero};
					continue;
				}

				auto page = data + off;
				const uint8_t *out = page;
				uint32_t size = PageSize;
				auto used = _codecCtx.encodePage(page, out, size);

				block = KeyframeBlock{uint32_t(payload.size()), uint16_t(size), 0};
				block.flags = uint16_t(uint16_t(used) << JournalRecord::CodecShift);
				payload.insert(payload.end(), out, out + size);
			}
		});

		auto mem = reinterpret_cast<uint8_t *>(
				memory::pool::palloc(kf.pool, payload.size() + 1, MaxAlign));
		if (!mem) {
			memory::pool::destroy(kf.pool);
			return Status::ErrorOutOfHostMemory;
		}
		__sprt_memcpy(mem, payload.data(), payload.size());

		kf.stores[i] = KeyframeStore{blocks, blockCount, mem, payload.size()};
		kf.bytes += payload.size() + sizeof(KeyframeBlock) * blockCount;
	}

	_keyframes.emplace_back(kf);
	++_metrics.keyframes;

	// The oldest keyframe is the horizon: everything before it is what eviction may drop.
	if (_horizon == 0 && _keyframes.size() == 1) {
		_horizon = 0; // version 0 stays reachable until the first eviction
	}
	evictKeyframes();
	return Status::Ok;
}

size_t Journal::getKeyframeImageSize(uint32_t i, uint32_t store) const {
	return size_t(_keyframes[i].stores[store].blockCount) * PageSize;
}

Status Journal::readKeyframeImage(uint32_t i, uint32_t store, mem_std::Vector<uint8_t> &out) const {
	auto &ks = _keyframes[i].stores[store];
	out.assign(size_t(ks.blockCount) * PageSize, 0);

	for (uint32_t b = 0; b < ks.blockCount; ++b) {
		auto &block = ks.blocks[b];
		if ((block.flags & JournalRecord::Zero) != 0) {
			continue; // the buffer is already zeroed
		}

		auto codec =
				JournalCodec((block.flags & JournalRecord::CodecMask) >> JournalRecord::CodecShift);
		if (!_codecCtx.decodePage(codec, ks.data + block.offset, block.size,
					out.data() + size_t(b) * PageSize)) {
			slog().error("vstore::Journal", "keyframe ", i, ": store ", store,
					" failed to decode block ", b, " as ", getCodecName(codec));
			return Status::ErrorNotRecoverable;
		}
	}
	return Status::Ok;
}

size_t Journal::getKeyframeBytes() const {
	size_t total = 0;
	for (auto &it : _keyframes) { total += it.bytes; }
	return total;
}

// Retention: the journal keeps records back to the oldest keyframe. Once it costs more than the
// budget, the oldest keyframe goes and the horizon moves up to the next one - which is exactly the
// point below which a rollback can no longer be reconstructed.
void Journal::evictKeyframes() {
	while (_keyframes.size() > 1
			&& (_keyframes.size() > _maxKeyframes || getJournalBytes() > _recordBudget)) {
		memory::pool::destroy(_keyframes.front().pool);
		_keyframes.erase(_keyframes.begin());

		Version horizon = _keyframes.front().version;
		_horizon = horizon;

		// Records at or below the new horizon can never be needed again: nothing may target a
		// version below it.
		size_t keep = 0;
		while (keep < _marks.size() && _marks[keep].version < horizon) { ++keep; }
		if (keep > 0 && keep < _marks.size()) {
			for (uint32_t i = 0; i < _storeCount; ++i) {
				// Read out before rebasing: `_marks[keep]` is one of the entries being rebased, so
				// a reference to it would be zeroed by the first iteration and every later mark
				// would then be shifted by nothing.
				uint32_t recordOffset = _marks[keep].recordEnd[i];
				uint32_t blobOffset = _marks[keep].blobEnd[i];

				auto &store = _stores[i];
				store._records.erase(store._records.begin(), store._records.begin() + recordOffset);
				// Blob references are global and stay valid as the log is trimmed, so only the
				// record offsets need rebasing.
				store._blobs.evictTo(blobOffset);
				for (size_t m = keep; m < _marks.size(); ++m) {
					_marks[m].recordEnd[i] -= recordOffset;
				}
			}
			_marks.erase(_marks.begin(), _marks.begin() + keep);
		}
	}
}

void Journal::dropKeyframesAbove(Version v) {
	while (!_keyframes.empty() && _keyframes.back().version > v) {
		memory::pool::destroy(_keyframes.back().pool);
		_keyframes.pop_back();
	}
}

const Journal::Metrics &Journal::getMetrics() const {
	_metrics.records = 0;
	_metrics.zeroRecords = 0;
	_metrics.unchangedPages = 0;
	_metrics.pageBytes = 0;
	_metrics.payloadBytes = 0;
	_metrics.compressedPages = 0;
	_metrics.rawPages = 0;
	_metrics.deltaRecords = 0;
	_metrics.deltaSubBlocks = 0;
	_metrics.wholeWins = 0;
	_metrics.rangeWins = 0;
	_metrics.scatterWins = 0;
	_metrics.sizingEncodes = 0;
	_metrics.markedBytes = 0;
	_metrics.markedSubBlockBytes = 0;
	for (uint32_t i = 0; i < _storeCount; ++i) {
		_metrics.records += _stores[i].getRecordCount();
		_metrics.zeroRecords += _stores[i].getZeroRecordCount();
		_metrics.unchangedPages += _stores[i].getUnchangedPageCount();
		_metrics.pageBytes += _stores[i].getBlobs().getBytes();
		_metrics.payloadBytes += _stores[i].getPayloadBytes();
		_metrics.compressedPages += _stores[i].getCompressedPageCount();
		_metrics.rawPages += _stores[i].getRawPageCount();
		_metrics.deltaRecords += _stores[i].getDeltaRecordCount();
		_metrics.deltaSubBlocks += _stores[i].getDeltaSubBlockCount();
		_metrics.wholeWins += _stores[i].getDeltaWholeWins();
		_metrics.rangeWins += _stores[i].getDeltaRangeWins();
		_metrics.scatterWins += _stores[i].getDeltaScatterWins();
		_metrics.sizingEncodes += _stores[i].getSizingEncodeCount();
		_metrics.markedBytes += _stores[i].getMarkedBytes();
		_metrics.markedSubBlockBytes += _stores[i].getMarkedSubBlockBytes();
	}
	return _metrics;
}

size_t Journal::getJournalBytes() const {
	size_t total = 0;
	for (uint32_t i = 0; i < _storeCount; ++i) {
		total += _stores[i].getBlobs().getBytes();
		total += _stores[i].getRecordCount() * sizeof(JournalRecord);
	}
	return total;
}

} // namespace stappler::vstore
