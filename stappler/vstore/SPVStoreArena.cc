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

// The arena: a relocatable heap addressed by 32-bit offsets. See SPVStoreArena.h for the invariant
// everything here serves. One rule governs the whole file: a chunk is always fully covered by a
// chain of blocks, from its data start to its end. There is no virgin tail outside the chain and no
// partially described region, so a structural walk is total - it visits every byte of the store and
// can therefore check that the accounting, the free lists and the physical layout all agree.
// Allocation is carving from a free block, never bumping a pointer into undescribed space.

#include "SPVStoreArena.h"

#include "SPLog.h"

#include <sprt/c/__sprt_string.h>

namespace STAPPLER_VERSIONIZED stappler::vstore {

// One tally for the whole module. A function-local static rather than a namespace-scope object so
// that the counters exist before any store does - a store built by a static initializer would
// otherwise increment an object that had not been constructed.
Counters &getCounters() {
	static Counters s_counters;
	return s_counters;
}

static sprt::atomic<uint64_t> s_foreignWrites(0);
static sprt::atomic<uint64_t> s_frozenAllocs(0);
static sprt::atomic<uint64_t> s_viewAllocs(0);
static sprt::atomic<bool> s_violationsFatal(true);

uint64_t getForeignWriteCount() { return s_foreignWrites.load(); }
uint64_t getFrozenAllocCount() { return s_frozenAllocs.load(); }
uint64_t getViewAllocCount() { return s_viewAllocs.load(); }
void setArenaViolationsFatal(bool value) { s_violationsFatal.store(value); }

void noteForeignWrite() {
	++s_foreignWrites;
	sprt_passert(!s_violationsFatal.load(),
			"vstore::Arena: a write from a thread that does not own the store, or through a view");
}

void noteFrozenAlloc() {
	++s_frozenAllocs;
	sprt_passert(!s_violationsFatal.load(),
			"vstore::Arena: the allocator was called while the store was frozen for its views");
}

// The refusal itself is the answer - a view hands back NullAddr the way a full store does, and
// every caller of alloc() already has that path - but asking a view to allocate is a bug in the
// caller, so a debug build stops on it under the same switch the other two use.
void noteViewAlloc() {
	++s_viewAllocs;
	sprt_passert(!s_violationsFatal.load(), "vstore::Arena: the allocator was called through a view");
}

// Classes 0..31 are exact: a free block's payload is always a multiple of Granule, so class c holds
// exactly Granule * (c + 1) bytes and a request lands in the one class that fits it. Above 512
// bytes the classes are power-of-two buckets, where a block in the request's own class may still be
// too small - which is why the search checks the head of that class for a fit and otherwise moves
// up, where every block is guaranteed to fit.
static constexpr uint32_t ExactClasses = 32;
static constexpr uint32_t ExactLimit = ExactClasses * Granule; // 512

static uint16_t sizeClassOf(uint32_t payload) {
	if (payload <= ExactLimit) {
		return uint16_t((payload - 1) / Granule);
	}
	uint32_t cls = ExactClasses;
	uint32_t limit = ExactLimit * 2;
	while (payload > limit && cls + 1 < NumSizeClasses) {
		limit *= 2;
		++cls;
	}
	return uint16_t(cls);
}

// The slow half of the barrier. Out of line so that the single-page case above it can be inlined
// without dragging this loop along.
void DirtyTracking::markRange(Addr a, uint32_t size) {
	if (size == 0) {
		return;
	}

	// The page index is flat over the whole address space, so a range that spans slots - an
	// oversize run is addressed as several consecutive ones - is nothing but a range that spans
	// pages, and needs no loop of its own. Only the two ends carry a partial mask; everything
	// between them is covered whole.
	uint32_t last = a + size - 1;
	uint32_t firstPage = a >> PageShift;
	uint32_t lastPage = last >> PageShift;

	sprt_passert(lastPage < _dirty.size(), "vstore::Arena: marking a page the store does not have");

	if (firstPage == lastPage) {
		_dirty[firstPage] = dirtyMerge(_dirty[firstPage], a & PageMask, last & PageMask);
		return;
	}

	_dirty[firstPage] = dirtyMerge(_dirty[firstPage], a & PageMask, PageSize - 1);
	for (uint32_t p = firstPage + 1; p < lastPage; ++p) { _dirty[p] = DirtyAll; }
	_dirty[lastPage] = dirtyMerge(_dirty[lastPage], 0, last & PageMask);
}

template <typename Tracking>
void ArenaT<Tracking>::releasePool() {
	if (_pool && _ownsPool) {
		memory::pool::destroy(_pool);
	}
	_pool = nullptr;
	_ownsPool = false;
	_view = false;
	_writeView = false;
	_chunks.clear();
	_spareChunks.clear();
	syncSlotArrays();
	_verifyEnabled = true;
	if constexpr (Tracking::HasShadow) {
		_tracking._shadow.clear();
		_tracking._readOnly = false;
	}
}

template <typename Tracking>
Status ArenaT<Tracking>::initView(const ArenaT &src) {
	if (isInitialized() || !src.isInitialized()) {
		return Status::ErrorInvalidArguemnt;
	}
	_pool = nullptr;
	_ownsPool = false;
	_view = true;
	_chunks = src._chunks;
	_epoch = src._epoch;
	_verifyEnabled = false;
	if constexpr (Tracking::HasShadow) {
		_tracking._readOnly = true;
	}
	return Status::Ok;
}

// The write view. Everything initView does, minus the read-only stamp, plus a dirty map of its own:
// the owner merges it back when the workers are done, so two workers marking a page they happen to
// share cannot lose each other's bits. The map is sized from the snapshot, which is the honest
// length: a page that does not exist yet cannot be written through this view, because the only
// addresses a worker may touch are blocks the owner allocated before the snapshot was taken.
template <typename Tracking>
Status ArenaT<Tracking>::initWriteView(const ArenaT &src) {
	auto st = initView(src);
	if (st != Status::Ok) {
		return st;
	}
	_writeView = true;
	if constexpr (Tracking::HasShadow) {
		_tracking._readOnly = false;
	}
	syncSlotArrays();
	return Status::Ok;
}

template <typename Tracking>
void ArenaT<Tracking>::closeView() {
	sprt_passert(_view, "vstore::Arena: closeView on a store that owns its memory");
	if (!_view) {
		return;
	}
	_writeView = false;
	releasePool(); // a view owns no pool: this releases the copy and nothing else
}

template <typename Tracking>
void ArenaT<Tracking>::mergeDirtyFrom(const ArenaT &view) requires (Tracking::IsTracked)
{
	_tracking.mergeFrom(view._tracking);
}

// A view never allocates, and that is enforced rather than asserted: a write view is handed to a
// worker thread, and a release build that let the call through would be rearranging the owner's
// free lists from two threads at once. The refusal is spelled the way a full store's is - NullAddr
// out of alloc, nothing out of free - so every caller already has the path.
template <typename Tracking>
bool ArenaT<Tracking>::checkAllocatorCall() const {
	if (_view) {
		noteViewAlloc();
		return false;
	}
	if constexpr (Tracking::HasShadow) {
		if (_tracking._frozen > 0) {
			noteFrozenAlloc();
		}
	}
	return true;
}

template <typename Tracking>
ArenaT<Tracking>::~ArenaT() {
	releasePool();
}

template <typename Tracking>
bool ArenaT<Tracking>::init(const Config &cfg) {
	if (isInitialized()) {
		return false;
	}

	// The budget and the descriptor table capacity are the same quantity on purpose: they cannot
	// drift apart, so "out of budget" and "out of table" are one check instead of two.
	uint64_t chunks = uint64_t(cfg.budgetBytes) >> ChunkShift;
	if (chunks > MaxChunks) {
		slog().error("vstore::Arena", "budget of ", cfg.budgetBytes,
				" bytes exceeds the maximum store size of ", uint64_t(MaxChunks) * ChunkSize);
		return false;
	}
	if (chunks < 1) {
		chunks = 1;
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

	auto mem = reinterpret_cast<uint8_t *>(memory::pool::palloc(_pool, ChunkSize, MaxAlign));
	if (!mem) {
		if (_ownsPool) {
			memory::pool::destroy(_pool);
		}
		_pool = nullptr;
		return false;
	}

	// Zeroed on creation: an image must be a function of the store's logical state alone, so no
	// byte of it may be left holding whatever the allocator handed us.
	__sprt_memset(mem, 0, ChunkSize);
	_chunks.emplace_back(mem);
	syncSlotArrays();

	auto r = root();
	r->magic = RootMagic;
	r->formatVersion = RootFormatVersion;
	r->chunkShift = uint16_t(ChunkShift);
	r->pageShift = uint16_t(PageShift);
	r->maxChunks = uint32_t(chunks);
	r->chunkCount = 1;
	r->firstDataOffset = firstDataOffsetFor(uint32_t(chunks));
	r->firstPageUsed = firstPageUsedFor(1);

	descs()[0] = makeChunkDesc(ChunkKind::Normal, 1);

	// Chunk 0's data area is one free block covering everything after the table.
	Addr blockAddr = r->firstDataOffset;
	uint32_t total = ChunkSize - r->firstDataOffset;

	auto h = blockAt(blockAddr);
	h->size = total - BlockHeaderSize;
	h->total = total;
	h->prevTotal = 0;
	h->flags = BlockFlags::LastInChunk;

	r->freeCount = 1;
	r->freeBytes = h->size;
	r->freeTotal = total;
	pushFree(blockAddr);

	// Taken only after the store is laid out: init()'s own writes are the initial state, not a
	// delta against anything, so there is nothing for them to be checked against.
	_verifyEnabled = cfg.debugVerify;
	if constexpr (Tracking::HasShadow) {
		_tracking._enabled = cfg.debugShadow;
		resync();
	}

	return true;
}

// One entry per page of the store, so the tracker follows _chunks rather than growing on demand at
// every use. For a PlainArena this is a call to an empty function and the size never exists.
template <typename Tracking>
void ArenaT<Tracking>::syncSlotArrays() {
	_tracking.resize(uint32_t(_chunks.size()) * PagesPerChunk);
}

template <typename Tracking>
void ArenaT<Tracking>::clearDirtyMap() requires (Tracking::IsTracked)
{
	_tracking.clear();
}

#if DEBUG

template <typename Tracking>
void ArenaT<Tracking>::resync() requires (Tracking::HasShadow)
{
	if (!_tracking._enabled) {
		return;
	}

	auto r = root();
	_tracking._shadow.assign(size_t(r->chunkCount) * ChunkSize, 0);
	for (uint32_t slot = 0; slot < r->chunkCount; ++slot) {
		if (auto mem = _chunks[slot]) {
			__sprt_memcpy(_tracking._shadow.data() + size_t(slot) * ChunkSize, mem, ChunkSize);
		}
	}

	// The shadow and the dirty map answer to the same moment: everything before this point is the
	// state being compared against, and the map has to be as empty as the comparison is.
	clearDirtyMap();
}

template <typename Tracking>
Status ArenaT<Tracking>::validate(
		const Callback<void(uint32_t slot, uint32_t page)> &onMismatch) const
		requires (Tracking::HasShadow)
{
	if (!_tracking._enabled) {
		return Status::Ok;
	}

	auto r = root();
	auto &shadow = _tracking._shadow;
	auto map = _tracking.map();
	uint32_t shadowSlots = uint32_t(shadow.size() / ChunkSize);
	// A page of zeros stands in for a slot that did not exist at the last resync: a new chunk is
	// created zeroed, so any non-zero byte in it must have been written - and therefore marked. In
	// .bss rather than on the stack: a page is 16 KiB, and a validator that runs on every commit of
	// every debug build has no business putting that in a frame.
	static const uint8_t zero[PageSize] = {0};

	for (uint32_t slot = 0; slot < r->chunkCount; ++slot) {
		auto mem = _chunks[slot];
		if (!mem) {
			continue;
		}

		for (uint32_t page = 0; page < PagesPerChunk; ++page) {
			uint32_t index = slot * PagesPerChunk + page;
			uint64_t marks = dirtyMaskOf(index < map.size() ? map[index] : uint64_t(0));
			if (marks == SubAll) {
				continue;
			}

			const uint8_t *before = slot < shadowSlots
					? shadow.data() + size_t(slot) * ChunkSize + size_t(page) * PageSize
					: zero;
			auto live = mem + size_t(page) * PageSize;

			// Per sub-block rather than per page: a page can be announced and still hold a hole, if
			// a write inside it went around the barrier while another one through it marked a
			// different part of the same page.
			for (uint32_t s = 0; s < SubBlocksPerPage; ++s) {
				if ((marks & (uint64_t(1) << s)) != 0) {
					continue;
				}
				auto at = size_t(s) * SubSize;
				if (__builtin_memcmp(live + at, before + at, SubSize) != 0) {
					onMismatch(slot, page);
					return Status::ErrorNotRecoverable;
				}
			}
		}
	}

	return Status::Ok;
}

template <typename Tracking>
uint8_t *ArenaT<Tracking>::unbarrieredWriteForTesting(Addr a, uint32_t size)
		requires (Tracking::HasShadow)
{
	sprt_passert(checkRange(a, size), "vstore::Arena: write outside the store");
	return resolve(a);
}

#endif

template <typename Tracking>
void ArenaT<Tracking>::pushFree(Addr block) {
	auto r = rootForWrite();
	auto h = headerForWrite(block);
	auto cls = sizeClassOf(h->size);

	h->sizeClass = cls;

	auto links = linksForWrite(block);
	links->prev = NullAddr;
	links->next = r->freeHead[cls];
	if (links->next != NullAddr) {
		linksForWrite(links->next)->prev = block;
	}
	r->freeHead[cls] = block;
}

template <typename Tracking>
void ArenaT<Tracking>::unlinkFree(Addr block) {
	auto r = rootForWrite();
	auto h = headerForWrite(block);
	auto links = linksForWrite(block);

	if (links->prev != NullAddr) {
		linksForWrite(links->prev)->next = links->next;
	} else {
		r->freeHead[h->sizeClass] = links->next;
	}
	if (links->next != NullAddr) {
		linksForWrite(links->next)->prev = links->prev;
	}

	links->next = NullAddr;
	links->prev = NullAddr;
	h->sizeClass = FreeClassNone;
}

template <typename Tracking>
void ArenaT<Tracking>::setNextPrevTotal(Addr block, uint32_t total) {
	auto h = blockAt(block);
	if ((h->flags & BlockFlags::LastInChunk) == 0) {
		headerForWrite(block + h->total)->prevTotal = total;
	}
}

template <typename Tracking>
size_t ArenaT<Tracking>::saveSize() const {
	return isInitialized() ? size_t(root()->chunkCount) * ChunkSize : 0;
}

template <typename Tracking>
void ArenaT<Tracking>::save(const Callback<void(const uint8_t *, size_t, bool)> &cb) const {
	if (!isInitialized()) {
		return;
	}

	// Holes are described, not skipped and not materialised: the image is indexed by slot, so the
	// zeros have to be accounted for, but nobody has to carry them around. A hole is decided by the
	// descriptor, not by the host pointer: the two agree everywhere except inside addChunkRun(),
	// where a slot briefly has memory while its descriptor still says Empty, which is precisely the
	// window in which an observer decides what a page's previous content was. Keying both on the
	// descriptor means the image and the journal can never disagree.
	auto r = root();
	uint32_t slot = 0;
	while (slot < r->chunkCount) {
		auto mem = isSlotLive(slot) ? _chunks[slot] : nullptr;
		uint32_t end = slot + 1;

		if (!mem) {
			while (end < r->chunkCount && !isSlotLive(end)) { ++end; }
			cb(nullptr, size_t(end - slot) * ChunkSize, true);
		} else {
			// The slots of an oversize run come from one allocation, so they are contiguous and
			// go out as a single piece. Unrelated chunks that happen to land next to each other
			// merge on the same test, which is harmless - the bytes are the same either way.
			while (end < r->chunkCount && _chunks[end] == mem + size_t(end - slot) * ChunkSize) {
				++end;
			}
			cb(mem, size_t(end - slot) * ChunkSize, false);
		}

		slot = end;
	}
}

template <typename Tracking>
bool ArenaT<Tracking>::rebuildHostState(BytesView image) {
	auto imageRoot = reinterpret_cast<const Root *>(image.data());
	uint32_t chunkCount = imageRoot->chunkCount;
	auto imageDescs = reinterpret_cast<const ChunkDesc *>(image.data() + DescsOffset);

	_chunks.clear();
	_chunks.resize(chunkCount, nullptr);
	syncSlotArrays();

	for (uint32_t slot = 0; slot < chunkCount; ++slot) {
		auto kind = getChunkKind(imageDescs[slot]);
		if (kind == ChunkKind::Empty || kind == ChunkKind::OversizeTail) {
			continue; // a tail is filled in by its head
		}

		uint32_t count = kind == ChunkKind::OversizeHead ? getChunkPayload(imageDescs[slot]) : 1;
		if (count == 0 || slot + count > chunkCount) {
			return false;
		}

		size_t bytes = size_t(count) * ChunkSize;
		auto mem = reinterpret_cast<uint8_t *>(memory::pool::palloc(_pool, bytes, MaxAlign));
		if (!mem) {
			return false;
		}
		__sprt_memcpy(mem, image.data() + size_t(slot) * ChunkSize, bytes);

		for (uint32_t k = 0; k < count; ++k) { _chunks[slot + k] = mem + size_t(k) * ChunkSize; }
	}

	return true;
}

template <typename Tracking>
Status ArenaT<Tracking>::adopt(BytesView image, const Config &cfg) {
	if (isInitialized()) {
		return Status::ErrorAlreadyPerformed;
	}

	// Everything needed to validate the image is inside the image, at a fixed offset. That is the
	// point of putting Root at address 0: an image is self-describing, with nothing passed
	// alongside it that could be lost or disagree.
	if (image.size() < DescsOffset) {
		slog().error("vstore::Arena", "adopt: image is shorter than its own header");
		return Status::ErrorInvalidArguemnt;
	}

	auto r = reinterpret_cast<const Root *>(image.data());
	if (r->magic != RootMagic) {
		slog().error("vstore::Arena", "adopt: bad magic ", r->magic);
		return Status::ErrorInvalidArguemnt;
	}
	if (r->formatVersion != RootFormatVersion) {
		slog().error("vstore::Arena", "adopt: format version ", r->formatVersion, ", expected ",
				RootFormatVersion);
		return Status::ErrorInvalidArguemnt;
	}
	if (r->chunkShift != ChunkShift) {
		slog().error("vstore::Arena", "adopt: chunk shift ", r->chunkShift, ", expected ",
				ChunkShift);
		return Status::ErrorInvalidArguemnt;
	}
	if (r->pageShift != PageShift) {
		slog().error("vstore::Arena", "adopt: page shift ", r->pageShift, ", expected ", PageShift);
		return Status::ErrorInvalidArguemnt;
	}
	if (r->maxChunks == 0 || r->maxChunks > MaxChunks || r->chunkCount == 0
			|| r->chunkCount > r->maxChunks) {
		slog().error("vstore::Arena", "adopt: chunk counts out of range: ", r->chunkCount, "/",
				r->maxChunks);
		return Status::ErrorInvalidArguemnt;
	}
	if (r->firstDataOffset != firstDataOffsetFor(r->maxChunks)) {
		slog().error("vstore::Arena", "adopt: firstDataOffset does not match maxChunks");
		return Status::ErrorInvalidArguemnt;
	}
	// Checked, never trusted: it is a bound a reader stops at, so an image claiming a small one
	// would make the journal skip bytes that really do change.
	if (r->firstPageUsed != firstPageUsedFor(r->chunkCount)) {
		slog().error("vstore::Arena", "adopt: firstPageUsed does not match chunkCount");
		return Status::ErrorInvalidArguemnt;
	}
	if (image.size() != size_t(r->chunkCount) * ChunkSize) {
		slog().error("vstore::Arena", "adopt: image is ", image.size(), " bytes, header implies ",
				size_t(r->chunkCount) * ChunkSize);
		return Status::ErrorInvalidArguemnt;
	}
	// Range only. That the root also names a live block is verify()'s business, and asking for it
	// here would mean walking the whole block chain before a single chunk is hosted.
	if (r->userRoot != NullAddr
			&& (r->userRoot < r->firstDataOffset
					|| r->userRoot >= size_t(r->chunkCount) * ChunkSize)) {
		slog().error("vstore::Arena", "adopt: userRoot ", r->userRoot, " is outside the image");
		return Status::ErrorInvalidArguemnt;
	}

	_pool = cfg.parentPool;
	_ownsPool = false;
	if (!_pool) {
		_pool = memory::pool::create();
		_ownsPool = true;
	}
	if (!_pool) {
		return Status::ErrorOutOfHostMemory;
	}

	if (!rebuildHostState(image)) {
		releasePool();
		slog().error("vstore::Arena", "adopt: image chunk table is inconsistent");
		return Status::ErrorInvalidArguemnt;
	}

	// A structurally broken image is rejected here rather than at the first allocation into it:
	// by the time a corrupt free list or block chain is noticed through use, the damage has
	// already spread.
	if (verify() != Status::Ok) {
		releasePool();
		return Status::ErrorInvalidArguemnt;
	}

	_verifyEnabled = cfg.debugVerify;
	if constexpr (Tracking::HasShadow) {
		_tracking._enabled = cfg.debugShadow;
		resync();
	}

	return Status::Ok;
}

// Zero copying: the chunks are the state, so handing them over is the whole of the move. What is
// not handed over is everything the kinds differ in - the map, the shadow - because none of it is
// state.
template <typename Tracking>
template <typename Other>
Status ArenaT<Tracking>::adoptFrom(ArenaT<Other> &src) {
	if (isInitialized()) {
		slog().error("vstore::Arena", "adoptFrom: this store already holds one");
		return Status::ErrorInvalidArguemnt;
	}
	if (!src.isInitialized()) {
		slog().error("vstore::Arena", "adoptFrom: the source store is empty");
		return Status::ErrorInvalidArguemnt;
	}

	// A journal holds a reference to the store it watches, and the reference would go on naming
	// this object after its chunks had left. There is no detach, so the only correct order is to
	// move the store first and attach the journal afterwards.
	sprt_passert(src.getObserverCountForAssert() == 0,
			"vstore::Arena: adoptFrom of a store something is already watching");

	_pool = src._pool;
	_ownsPool = src._ownsPool;
	_chunks = sp::move(src._chunks);
	_spareChunks = sp::move(src._spareChunks);
	_verifyEnabled = src._verifyEnabled;

	src._pool = nullptr;
	src._ownsPool = false;
	src._chunks.clear();
	src._spareChunks.clear();
	src.syncSlotArrays();

	// Rebuilt, not transferred. The map is derived host state and the shadow is a copy of bytes
	// this store has not written yet, so both start from where this store starts: nothing recorded.
	syncSlotArrays();
	if constexpr (Tracking::IsTracked) {
		clearDirtyMap();
	}
	if constexpr (Tracking::HasShadow) {
		_tracking._enabled = true;
		resync();
	}

	// Everything a caller derived from the source's addresses was derived from another object, and
	// this one has never certified any of it.
	invalidateDerived();
	return Status::Ok;
}

template <typename Tracking>
uint32_t ArenaT<Tracking>::chunkExtent(uint32_t slot) const {
	auto desc = descs()[slot];
	if (getChunkKind(desc) == ChunkKind::OversizeHead) {
		return getChunkPayload(desc) * ChunkSize;
	}
	return ChunkSize;
}

// Smallest extent that fits, so that a long stretch is not spent on a single chunk and left unable
// to host a run later.
template <typename Tracking>
uint8_t *ArenaT<Tracking>::takeSpare(uint32_t count) {
	size_t best = _spareChunks.size();
	for (size_t i = 0; i < _spareChunks.size(); ++i) {
		if (_spareChunks[i].count < count) {
			continue;
		}
		if (best == _spareChunks.size() || _spareChunks[i].count < _spareChunks[best].count) {
			best = i;
		}
	}
	if (best == _spareChunks.size()) {
		return nullptr;
	}

	auto mem = _spareChunks[best].base;
	if (_spareChunks[best].count == count) {
		_spareChunks[best] = _spareChunks.back();
		_spareChunks.pop_back();
	} else {
		_spareChunks[best].base += size_t(count) * ChunkSize;
		_spareChunks[best].count -= count;
	}
	return mem;
}

template <typename Tracking>
void ArenaT<Tracking>::giveSpare(uint8_t *base, uint32_t count) {
	if (!base || count == 0) {
		return;
	}
	_spareChunks.emplace_back(SpareExtent{base, count});

	// Coalesce until nothing more merges. A run given back one chunk at a time has to come out the
	// other side as one extent, or it could never host a run again - and that is precisely the
	// shape a rollback to an earlier layout asks for.
	bool merged = true;
	while (merged) {
		merged = false;
		for (size_t i = 0; i < _spareChunks.size() && !merged; ++i) {
			for (size_t j = i + 1; j < _spareChunks.size(); ++j) {
				auto &a = _spareChunks[i];
				auto &b = _spareChunks[j];
				if (a.base + size_t(a.count) * ChunkSize == b.base) {
					a.count += b.count;
				} else if (b.base + size_t(b.count) * ChunkSize == a.base) {
					a.base = b.base;
					a.count += b.count;
				} else {
					continue;
				}
				_spareChunks[j] = _spareChunks.back();
				_spareChunks.pop_back();
				merged = true;
				break;
			}
		}
	}
}

template <typename Tracking>
uint8_t *ArenaT<Tracking>::getPageForRestore(uint32_t slot, uint32_t page) {
	sprt_passert(isHosted(slot) && page < PagesPerChunk,
			"vstore::Arena: restoring a page of an unhosted slot");
	return _chunks[slot] + size_t(page) * PageSize;
}

template <typename Tracking>
Status ArenaT<Tracking>::planRehost(const ChunkDesc *targetDescs, uint32_t targetCount,
		RehostPlan &plan) {
	plan.entries.clear();
	plan.chunkCount = targetCount;
	plan.valid = false;

	if (!isInitialized() || targetCount == 0 || targetCount > root()->maxChunks) {
		slog().error("vstore::Arena", "planRehost: chunk count ", targetCount, " out of range");
		return Status::ErrorNotRecoverable;
	}

	for (uint32_t slot = 0; slot < targetCount;) {
		auto kind = getChunkKind(targetDescs[slot]);

		if (kind == ChunkKind::Empty) {
			if (isHosted(slot)) {
				plan.entries.emplace_back(RehostPlan::Entry{slot, 1, nullptr});
			}
			++slot;
			continue;
		}
		if (kind == ChunkKind::OversizeTail) {
			// A tail is always consumed by its head, so reaching one directly means the table is
			// malformed - the head either does not exist or does not cover it.
			slog().error("vstore::Arena", "planRehost: slot ", slot, " is an orphaned run tail");
			discardRehost(plan);
			return Status::ErrorNotRecoverable;
		}

		uint32_t count = 1;
		if (kind == ChunkKind::OversizeHead) {
			count = getChunkPayload(targetDescs[slot]);
			if (count < 2 || slot + count > targetCount) {
				slog().error("vstore::Arena", "planRehost: run at ", slot, " has bad length ",
						count);
				discardRehost(plan);
				return Status::ErrorNotRecoverable;
			}
			for (uint32_t k = 1; k < count; ++k) {
				if (getChunkKind(targetDescs[slot + k]) != ChunkKind::OversizeTail
						|| getChunkPayload(targetDescs[slot + k]) != slot) {
					slog().error("vstore::Arena", "planRehost: run at ", slot,
							" is not covered by its tails");
					discardRehost(plan);
					return Status::ErrorNotRecoverable;
				}
			}
		}

		// The current hosting is kept only if it already satisfies the target's shape. For a run
		// that means contiguity: keeping a run whose slots point into different allocations would
		// hand out a block that runs off the end of one of them.
		bool usable = isHosted(slot);
		for (uint32_t k = 1; usable && k < count; ++k) {
			usable = isHosted(slot + k)
					&& _chunks[slot + k] == _chunks[slot] + size_t(k) * ChunkSize;
		}

		if (!usable) {
			auto mem = takeSpare(count);
			if (!mem) {
				mem = reinterpret_cast<uint8_t *>(
						memory::pool::palloc(_pool, size_t(count) * ChunkSize, MaxAlign));
			}
			if (!mem) {
				slog().error("vstore::Arena", "planRehost: out of memory for ", count,
						" chunks at slot ", slot);
				discardRehost(plan);
				return Status::ErrorOutOfHostMemory;
			}
			plan.entries.emplace_back(RehostPlan::Entry{slot, count, mem});
		}

		slot += count;
	}

	for (uint32_t slot = targetCount; slot < _chunks.size(); ++slot) {
		if (_chunks[slot]) {
			plan.entries.emplace_back(RehostPlan::Entry{slot, 1, nullptr});
		}
	}

	plan.valid = true;
	return Status::Ok;
}

template <typename Tracking>
void ArenaT<Tracking>::discardRehost(RehostPlan &plan) {
	for (auto &it : plan.entries) {
		if (it.memory) {
			giveSpare(it.memory, it.count);
		}
	}
	plan.entries.clear();
	plan.valid = false;
}

template <typename Tracking>
void ArenaT<Tracking>::rehost(RehostPlan &plan) {
	sprt_passert(plan.valid, "vstore::Arena: applying an unplanned rehost");

	// The old table is snapshotted first: a slot's new buffer is filled from its old one, and a
	// buffer must not be both a source and a destination in the same pass.
	mem_std::Vector<uint8_t *> old = _chunks;
	if (_chunks.size() < plan.chunkCount) {
		_chunks.resize(plan.chunkCount, nullptr);
		syncSlotArrays();
	}

	for (auto &e : plan.entries) {
		if (!e.memory) {
			_chunks[e.slot] = nullptr;
			continue;
		}
		for (uint32_t k = 0; k < e.count; ++k) {
			auto dst = e.memory + size_t(k) * ChunkSize;
			auto src = (e.slot + k < old.size()) ? old[e.slot + k] : nullptr;
			// Content-preserving: a slot that moves takes its bytes along, and a slot that gains
			// memory it never had reads as zeros - which is what an unhosted slot's image says.
			// Without this, a rollback would depend on the journal happening to cover every page.
			if (src) {
				__sprt_memcpy(dst, src, ChunkSize);
			} else {
				__sprt_memset(dst, 0, ChunkSize);
			}
			_chunks[e.slot + k] = dst;
		}
	}

	for (uint32_t slot = 0; slot < old.size(); ++slot) {
		auto was = old[slot];
		if (was && (slot >= _chunks.size() || _chunks[slot] != was)) {
			giveSpare(was, 1);
		}
	}

	_chunks.resize(plan.chunkCount);
	syncSlotArrays();
	plan.entries.clear();
	plan.valid = false;
}

// Finds `count` consecutive slots and backs them with one host allocation, so that a block spanning
// them stays contiguous in memory and resolve() needs no special case. Holes left by released runs
// are reused first: slots are 32-bit address space, and leaking them would exhaust the store long
// before its memory budget.
template <typename Tracking>
bool ArenaT<Tracking>::addChunkRun(uint32_t count, uint32_t &outSlot) {
	auto r = rootForWrite();

	uint32_t slot = r->maxChunks;
	uint32_t run = 0;
	for (uint32_t i = 0; i < r->chunkCount; ++i) {
		if (getChunkKind(descs()[i]) == ChunkKind::Empty) {
			if (++run == count) {
				slot = i + 1 - count;
				break;
			}
		} else {
			run = 0;
		}
	}

	bool append = false;
	if (slot == r->maxChunks) {
		if (r->chunkCount + count > r->maxChunks) {
			return false;
		}
		slot = r->chunkCount;
		append = true;
	}

	uint8_t *mem = takeSpare(count);
	if (!mem) {
		mem = reinterpret_cast<uint8_t *>(
				memory::pool::palloc(_pool, size_t(count) * ChunkSize, MaxAlign));
	}
	if (!mem) {
		return false;
	}

	if (append) {
		_chunks.resize(slot + count, nullptr);
		syncSlotArrays();
		r->chunkCount = slot + count;
		// The table just gained entries, so the header's footprint in page 0 grew with it. Only the
		// appending path can move it: reusing a hole writes a descriptor that was already counted.
		r->firstPageUsed = firstPageUsedFor(r->chunkCount);
	}
	for (uint32_t k = 0; k < count; ++k) { _chunks[slot + k] = mem + size_t(k) * ChunkSize; }

	// The mark is required - these pages really do change. Where it stands relative to the
	// descriptor writes is not: a reader's before-image comes from its own baseline, and a slot
	// that did not exist at the last commit has a baseline of zeros by construction.
	markDirty(slot << ChunkShift, count * ChunkSize);
	__sprt_memset(mem, 0, size_t(count) * ChunkSize);

	if (count == 1) {
		*descForWrite(slot) = makeChunkDesc(ChunkKind::Normal, 1);
	} else {
		*descForWrite(slot) = makeChunkDesc(ChunkKind::OversizeHead, count);
		for (uint32_t k = 1; k < count; ++k) {
			*descForWrite(slot + k) = makeChunkDesc(ChunkKind::OversizeTail, slot);
		}
	}

	outSlot = slot;
	return true;
}

template <typename Tracking>
bool ArenaT<Tracking>::growChunk() {
	uint32_t slot = 0;
	if (!addChunkRun(1, slot)) {
		return false;
	}

	auto r = rootForWrite();
	Addr blockAddr = slot << ChunkShift;
	auto h = headerForWrite(blockAddr);
	h->size = ChunkSize - BlockHeaderSize;
	h->total = ChunkSize;
	h->prevTotal = 0;
	h->flags = BlockFlags::LastInChunk;

	++r->freeCount;
	r->freeBytes += h->size;
	r->freeTotal += h->total;
	pushFree(blockAddr);

	// addChunkRun() already reported the new slot; reporting it again here would make an observer
	// that counts events see two chunks where one was created.
	return true;
}

// Turns a free block into a live one of `total` bytes, returning whatever is left over to the free
// lists. The leftover keeps the physical chain intact, which is what lets the following block's
// prevTotal - and therefore backwards coalescing - stay correct.
template <typename Tracking>
Addr ArenaT<Tracking>::carveFrom(Addr b, uint32_t size, uint32_t total) {
	auto r = rootForWrite();
	auto h = headerForWrite(b);

	uint32_t avail = h->total;
	uint32_t rem = avail - total;

	unlinkFree(b);
	--r->freeCount;
	r->freeBytes -= h->size;
	r->freeTotal -= avail;

	if (rem >= MinBlockTotal) {
		Addr n = b + total;
		auto nh = headerForWrite(n);
		nh->size = rem - BlockHeaderSize;
		nh->total = rem;
		nh->prevTotal = total;
		nh->flags = uint16_t(h->flags & BlockFlags::LastInChunk);
		nh->sizeClass = FreeClassNone;

		h->flags = uint16_t(h->flags & ~BlockFlags::LastInChunk);
		h->total = total; // so that setNextPrevTotal() looks past the right block

		setNextPrevTotal(n, rem);

		++r->freeCount;
		r->freeBytes += nh->size;
		r->freeTotal += nh->total;
		pushFree(n);
	} else {
		// The remainder is too small to describe as a block, so it becomes padding of this one.
		total = avail;
	}

	h->size = size;
	h->total = total;
	h->flags = uint16_t(h->flags | BlockFlags::Used);
	h->sizeClass = FreeClassNone;

	++r->liveCount;
	r->liveBytes += size;
	r->liveTotal += total;

	// Padding after the payload is zeroed so that two stores holding the same data hold the same
	// bytes - image equality is the strongest form of the relocation invariant, and it would fail
	// on stale bytes left over from a freed block.
	uint32_t payloadSpace = total - BlockHeaderSize;
	if (payloadSpace > size) {
		__sprt_memset(write(b + BlockHeaderSize + size, payloadSpace - size), 0,
				payloadSpace - size);
	}

	return b + BlockHeaderSize;
}

// A block that cannot fit a chunk gets a run of its own. The run is one host allocation, so the
// block is contiguous and read()/write() can keep handing out a plain pointer; nothing on the read
// path knows oversize blocks exist.
template <typename Tracking>
Addr ArenaT<Tracking>::allocOversize(uint32_t size) {
	uint64_t needed = uint64_t(BlockHeaderSize) + size;
	uint32_t count = uint32_t((needed + ChunkSize - 1) / ChunkSize);

	uint32_t slot = 0;
	if (!addChunkRun(count, slot)) {
		return NullAddr;
	}

	auto r = rootForWrite();
	Addr blockAddr = slot << ChunkShift;
	uint32_t total = count * ChunkSize;

	auto h = headerForWrite(blockAddr);
	h->size = size;
	h->total = total;
	h->prevTotal = 0;
	h->flags = BlockFlags::Used | BlockFlags::Oversize | BlockFlags::LastInChunk;
	h->sizeClass = FreeClassNone;

	++r->oversizeRuns;
	++r->liveCount;
	r->liveBytes += size;
	r->liveTotal += total;

	return blockAddr + BlockHeaderSize;
}

template <typename Tracking>
Addr ArenaT<Tracking>::alloc(uint32_t size, uint32_t align) {
	if (!checkAllocatorCall()) {
		return NullAddr;
	}
	if (align == 0 || align > MaxAlign || (align & (align - 1)) != 0) {
		sprt_passert(false, "vstore::Arena: alignment must be a power of two, at most MaxAlign");
		return NullAddr;
	}
	if (size < MinPayload) {
		size = MinPayload;
	}
	if (size > MaxInChunkPayload) {
		return allocOversize(size);
	}

	uint32_t total = alignUp(BlockHeaderSize + size, Granule);

	// The head of the request's own class may not fit (the classes above 512 bytes are ranges);
	// every class above it does, so at most one fit test can fail before the search becomes a
	// straight scan for a non-empty list.
	for (uint32_t attempt = 0; attempt < 2; ++attempt) {
		auto r = root();
		for (uint32_t cls = sizeClassOf(size); cls < NumSizeClasses; ++cls) {
			Addr b = r->freeHead[cls];
			if (b != NullAddr && blockAt(b)->total >= total) {
				return carveFrom(b, size, total);
			}
		}

		if (attempt == 0 && !growChunk()) {
			return NullAddr;
		}
	}

	return NullAddr;
}

template <typename Tracking>
void ArenaT<Tracking>::free(Addr a) {
	if (a == NullAddr) {
		return;
	}
	if (!checkAllocatorCall()) {
		return;
	}

	Addr b = a - BlockHeaderSize;
	auto h = headerForWrite(b);
	sprt_passert((h->flags & BlockFlags::Used) != 0, "vstore::Arena: double free");

	auto r = rootForWrite();
	--r->liveCount;
	r->liveBytes -= h->size;
	r->liveTotal -= h->total;

	if ((h->flags & BlockFlags::Oversize) != 0) {
		releaseRun(b);
		return;
	}

	h->flags = uint16_t(h->flags & ~BlockFlags::Used);
	// A free block owns its whole payload: the requested size is gone, and keeping it would leave
	// the size class disagreeing with what the block can actually hold.
	h->size = h->total - BlockHeaderSize;

	insertFreeRange(b);
}

// `block` must already be marked free with its size covering its whole payload, and must not be
// accounted or linked yet.
template <typename Tracking>
void ArenaT<Tracking>::insertFreeRange(Addr block) {
	auto r = rootForWrite();
	auto h = headerForWrite(block);

	// Forwards first: the next block's own next is untouched by the merge, so the two merges do
	// not interfere and the order only affects which header survives.
	if ((h->flags & BlockFlags::LastInChunk) == 0) {
		Addr n = block + h->total;
		auto nh = blockAt(n);
		if ((nh->flags & BlockFlags::Used) == 0) {
			unlinkFree(n);
			nh = headerForWrite(n);
			--r->freeCount;
			r->freeBytes -= nh->size;
			r->freeTotal -= nh->total;

			h->total += nh->total;
			h->size = h->total - BlockHeaderSize;
			h->flags = uint16_t(h->flags | (nh->flags & BlockFlags::LastInChunk));
		}
	}

	// Backwards: this is what prevTotal exists for. Without it a free() could only ever merge in
	// one direction, and a chunk would end up as an alternating comb of used and free blocks.
	if (h->prevTotal != 0) {
		Addr p = block - h->prevTotal;
		auto ph = blockAt(p);
		if ((ph->flags & BlockFlags::Used) == 0) {
			unlinkFree(p);
			ph = headerForWrite(p);
			--r->freeCount;
			r->freeBytes -= ph->size;
			r->freeTotal -= ph->total;

			ph->total += h->total;
			ph->size = ph->total - BlockHeaderSize;
			ph->flags = uint16_t(ph->flags | (h->flags & BlockFlags::LastInChunk));

			block = p;
			h = ph;
		}
	}

	setNextPrevTotal(block, h->total);

	++r->freeCount;
	r->freeBytes += h->size;
	r->freeTotal += h->total;
	pushFree(block);
}

template <typename Tracking>
Addr ArenaT<Tracking>::realloc(Addr a, uint32_t newSize, uint32_t align) {
	if (a == NullAddr) {
		return alloc(newSize, align);
	}
	if (!checkAllocatorCall()) {
		return NullAddr;
	}
	if (align == 0 || align > MaxAlign || (align & (align - 1)) != 0) {
		sprt_passert(false, "vstore::Arena: alignment must be a power of two, at most MaxAlign");
		return NullAddr;
	}
	if (newSize < MinPayload) {
		newSize = MinPayload;
	}

	Addr b = a - BlockHeaderSize;
	auto h = headerForWrite(b);
	sprt_passert((h->flags & BlockFlags::Used) != 0, "vstore::Arena: realloc of a free block");

	uint32_t oldSize = h->size;

	// An oversize block owns whole chunks, so resizing it in place would mean resizing its run;
	// relocating is simpler and rare enough not to matter.
	if ((h->flags & BlockFlags::Oversize) != 0 || newSize > MaxInChunkPayload) {
		Addr moved = alloc(newSize, align);
		if (moved == NullAddr) {
			return NullAddr;
		}
		__sprt_memcpy(write(moved, newSize), read(a, oldSize),
				newSize < oldSize ? newSize : oldSize);
		free(a);
		return moved;
	}

	uint32_t newTotal = alignUp(BlockHeaderSize + newSize, Granule);
	auto r = rootForWrite();

	// Grow in place by swallowing a free neighbour.
	if (newTotal > h->total && (h->flags & BlockFlags::LastInChunk) == 0) {
		Addr n = b + h->total;
		auto nh = blockAt(n);
		if ((nh->flags & BlockFlags::Used) == 0 && h->total + nh->total >= newTotal) {
			unlinkFree(n);
			--r->freeCount;
			r->freeBytes -= nh->size;
			r->freeTotal -= nh->total;

			r->liveTotal += nh->total;
			h->total += nh->total;
			h->flags = uint16_t(h->flags | (nh->flags & BlockFlags::LastInChunk));
		}
	}

	if (newTotal <= h->total) {
		uint32_t rem = h->total - newTotal;
		if (rem >= MinBlockTotal) {
			Addr tail = b + newTotal;
			auto th = headerForWrite(tail);
			th->size = rem - BlockHeaderSize;
			th->total = rem;
			th->prevTotal = newTotal;
			th->flags = uint16_t(h->flags & BlockFlags::LastInChunk);
			th->sizeClass = FreeClassNone;

			r->liveTotal -= rem;
			h->flags = uint16_t(h->flags & ~BlockFlags::LastInChunk);
			h->total = newTotal;

			insertFreeRange(tail);
		}

		// Both paths above can have changed this block's extent - absorbing a neighbour without
		// splitting changes it without touching any other header at all. The block behind it has
		// to be told, or the next backwards walk through here lands in the middle of a block.
		setNextPrevTotal(b, h->total);

		r->liveBytes += newSize;
		r->liveBytes -= h->size;
		h->size = newSize;

		uint32_t payloadSpace = h->total - BlockHeaderSize;
		if (payloadSpace > newSize) {
			__sprt_memset(write(b + BlockHeaderSize + newSize, payloadSpace - newSize), 0,
					payloadSpace - newSize);
		}
		return a;
	}

	// Nothing in place: allocate, copy, release. The original is untouched until the new block
	// exists, so a failure here costs the caller nothing.
	Addr moved = alloc(newSize, align);
	if (moved == NullAddr) {
		return NullAddr;
	}
	__sprt_memcpy(write(moved, newSize), read(a, oldSize), newSize < oldSize ? newSize : oldSize);
	free(a);
	return moved;
}

template <typename Tracking>
void ArenaT<Tracking>::free(Addr a, uint32_t size) {
	sprt_passert(a == NullAddr || sizeOf(a) == size,
			"vstore::Arena: free() size does not match the block header");
	(void)size;
	free(a);
}

// An oversize block owns its whole run, so freeing it releases the slots outright rather than
// leaving a free block behind - a run is allocated for one block and can never hold another.
template <typename Tracking>
void ArenaT<Tracking>::releaseRun(Addr block) {
	auto r = rootForWrite();
	uint32_t slot = block >> ChunkShift;
	uint32_t count = getChunkPayload(descs()[slot]);

	// Marked while the run is still reachable, though now only for tidiness: a moment later this
	// address range reads as a hole. What the mark is for is that these pages change - from content
	// to zeros, as far as the image is concerned - and a reader whose baseline still holds the
	// content has to be told to look.
	markDirty(slot << ChunkShift, count * ChunkSize);

	// The run was one allocation and goes back as one extent, so it can be taken as a run again.
	giveSpare(_chunks[slot], count);
	for (uint32_t k = 0; k < count; ++k) {
		_chunks[slot + k] = nullptr;
		*descForWrite(slot + k) = makeChunkDesc(ChunkKind::Empty, 0);
	}

	--r->oversizeRuns;
}

template <typename Tracking>
uint32_t ArenaT<Tracking>::sizeOf(Addr a) const {
	if (a == NullAddr) {
		return 0;
	}
	return blockAt(a - BlockHeaderSize)->size;
}

template <typename Tracking>
bool ArenaT<Tracking>::isLivePayload(Addr a) const {
	// The header has to fit in front of it, and the payload has to start where a payload can: every
	// block is carved at a granule, so an address that is not is not one this allocator handed out.
	if (!isInitialized() || a == NullAddr || a < BlockHeaderSize || (a % Granule) != 0) {
		return false;
	}
	// Deliberately checkRange on the header: a payload one byte past the end of a slot would pass a
	// check on the payload alone and then be read through the header in front of it.
	if (!checkRange(a - BlockHeaderSize, BlockHeaderSize)) {
		return false;
	}
	auto h = blockAt(a - BlockHeaderSize);
	if ((h->flags & BlockFlags::Used) == 0 || h->total < MinBlockTotal || h->size > h->total) {
		return false;
	}
	// And the block's own extent has to be inside the store, or its `size` is not a length anything
	// may be read at.
	return checkRange(a, h->size);
}

// A returned pointer is only good for `size` bytes if that range is contiguous in host memory,
// which means it must stay inside one chunk - or inside one oversize run, whose slots are backed by
// a single allocation. Checked in debug only; the cost of the run lookup does not belong on the
// release read path.
template <typename Tracking>
bool ArenaT<Tracking>::checkRange(Addr a, uint32_t size) const {
	if (a == NullAddr) {
		return false;
	}

	uint32_t slot = a >> ChunkShift;
	auto r = root();
	if (slot >= r->chunkCount || !_chunks[slot]) {
		return false;
	}

	uint64_t offset = a & ChunkMask;
	uint64_t limit = ChunkSize;

	auto kind = getChunkKind(descs()[slot]);
	if (kind == ChunkKind::OversizeHead || kind == ChunkKind::OversizeTail) {
		uint32_t head = kind == ChunkKind::OversizeHead ? slot : getChunkPayload(descs()[slot]);
		uint32_t count = getChunkPayload(descs()[head]);
		offset += uint64_t(slot - head) * ChunkSize;
		limit = uint64_t(count) * ChunkSize;
	}

	return offset + size <= limit;
}

template <typename Tracking>
Status ArenaT<Tracking>::setUserRoot(Addr a) {
	if (!isInitialized()) {
		return Status::ErrorInvalidArguemnt;
	}
	// Checked here rather than left to verify(): a root pointing outside the store is a caller
	// error caught at the call, while the same value found later by verify() is indistinguishable
	// from corruption and gets reported as such.
	if (a != NullAddr && !checkRange(a, 1)) {
		slog().error("vstore::Arena", "setUserRoot: ", a, " is outside the store");
		return Status::ErrorInvalidArguemnt;
	}
	rootForWrite()->userRoot = a;

	// The index was replaced. Everything any layer above derived from the old one describes a store
	// that is no longer there.
	invalidateDerived();
	return Status::Ok;
}

template <typename Tracking>
Addr ArenaT<Tracking>::getUserRoot() const {
	return isInitialized() ? root()->userRoot : NullAddr;
}

template <typename Tracking>
uint32_t ArenaT<Tracking>::getChunkCount() const {
	return isInitialized() ? root()->chunkCount : 0;
}

template <typename Tracking>
uint32_t ArenaT<Tracking>::getMaxChunks() const {
	return isInitialized() ? root()->maxChunks : 0;
}

template <typename Tracking>
bool ArenaT<Tracking>::isSlotLive(uint32_t slot) const {
	return isInitialized() && slot < root()->chunkCount
			&& getChunkKind(descs()[slot]) != ChunkKind::Empty;
}

template <typename Tracking>
bool ArenaT<Tracking>::isHosted(uint32_t slot) const {
	return slot < _chunks.size() && _chunks[slot] != nullptr;
}

template <typename Tracking>
ChunkDesc ArenaT<Tracking>::getChunkDesc(uint32_t slot) const {
	return (isInitialized() && slot < root()->maxChunks) ? descs()[slot]
														 : makeChunkDesc(ChunkKind::Empty, 0);
}

template <typename Tracking>
const uint8_t *ArenaT<Tracking>::getPage(uint32_t slot, uint32_t page) const {
	if (!isSlotLive(slot) || !isHosted(slot) || page >= PagesPerChunk) {
		return nullptr;
	}
	return _chunks[slot] + size_t(page) * PageSize;
}

template <typename Tracking>
uint32_t ArenaT<Tracking>::getPageFootprint(uint32_t slot, uint32_t page) const {
	if (slot != 0 || page != 0 || !isInitialized()) {
		return PageSize;
	}
	sprt_passert(root()->firstPageUsed == firstPageUsedFor(root()->chunkCount),
			"vstore::Arena: firstPageUsed has drifted from chunkCount");
	return root()->firstPageUsed;
}

template <typename Tracking>
Status ArenaT<Tracking>::verify() const {
	if (!isInitialized()) {
		slog().error("vstore::Arena", "verify: not initialized");
		return Status::ErrorNotRecoverable;
	}

	auto r = root();
	if (r->magic != RootMagic || r->formatVersion != RootFormatVersion
			|| r->chunkShift != ChunkShift || r->pageShift != PageShift) {
		slog().error("vstore::Arena", "verify: bad root header");
		return Status::ErrorNotRecoverable;
	}
	if (r->maxChunks == 0 || r->maxChunks > MaxChunks || r->chunkCount == 0
			|| r->chunkCount > r->maxChunks) {
		slog().error("vstore::Arena", "verify: chunk counts out of range: ", r->chunkCount, "/",
				r->maxChunks);
		return Status::ErrorNotRecoverable;
	}
	if (r->chunkCount != _chunks.size()) {
		slog().error("vstore::Arena", "verify: host chunk table holds ", _chunks.size(),
				" entries, root says ", r->chunkCount);
		return Status::ErrorNotRecoverable;
	}
	if (r->firstDataOffset != firstDataOffsetFor(r->maxChunks)) {
		slog().error("vstore::Arena", "verify: firstDataOffset does not match maxChunks");
		return Status::ErrorNotRecoverable;
	}
	if (r->firstPageUsed != firstPageUsedFor(r->chunkCount)) {
		slog().error("vstore::Arena", "verify: firstPageUsed ", r->firstPageUsed,
				" does not match ", r->chunkCount, " chunks");
		return Status::ErrorNotRecoverable;
	}

	uint64_t liveCount = 0, liveBytes = 0, liveTotal = 0;
	uint64_t freeCount = 0, freeBytes = 0, freeTotal = 0;
	mem_std::Set<Addr> freeByWalk;

	// The block walk below is the only place that knows which addresses are live payload starts,
	// so the user root is checked from inside it rather than by a second sweep. A root left
	// pointing at freed space is precisely what a rollback whose rehost phase went wrong would
	// leave behind, and it is the one index-shaped corruption this layer can see for free.
	bool userRootSeen = (r->userRoot == NullAddr);

	uint32_t oversizeRuns = 0;

	for (uint32_t slot = 0; slot < r->chunkCount; ++slot) {
		auto desc = descs()[slot];
		auto kind = getChunkKind(desc);

		if (kind == ChunkKind::Empty) {
			if (_chunks[slot]) {
				slog().error("vstore::Arena", "verify: empty slot ", slot, " still has memory");
				return Status::ErrorNotRecoverable;
			}
			continue;
		}
		if (kind == ChunkKind::OversizeTail) {
			uint32_t head = getChunkPayload(desc);
			if (head >= slot || getChunkKind(descs()[head]) != ChunkKind::OversizeHead
					|| head + getChunkPayload(descs()[head]) <= slot) {
				slog().error("vstore::Arena", "verify: slot ", slot,
						" is a tail of a run that does not cover it");
				return Status::ErrorNotRecoverable;
			}
			if (_chunks[slot] != _chunks[head] + size_t(slot - head) * ChunkSize) {
				slog().error("vstore::Arena", "verify: run slot ", slot, " is not contiguous");
				return Status::ErrorNotRecoverable;
			}
			continue;
		}
		if (kind == ChunkKind::OversizeHead) {
			uint32_t count = getChunkPayload(desc);
			if (count < 2 || slot + count > r->chunkCount) {
				slog().error("vstore::Arena", "verify: run at ", slot, " has bad length ", count);
				return Status::ErrorNotRecoverable;
			}
			++oversizeRuns;
		}
		if (!_chunks[slot]) {
			slog().error("vstore::Arena", "verify: live slot ", slot, " has no memory");
			return Status::ErrorNotRecoverable;
		}

		Addr begin = (slot << ChunkShift) + chunkDataBegin(slot);
		uint64_t end = uint64_t(slot << ChunkShift) + chunkExtent(slot);

		uint32_t prevTotal = 0;
		uint32_t lastFlags = 0;
		bool prevWasFree = false;
		uint64_t at = begin;

		while (at < end) {
			auto h = blockAt(Addr(at));
			if (h->total < MinBlockTotal || (h->total % Granule) != 0 || at + h->total > end) {
				slog().error("vstore::Arena", "verify: block at ", at, " has bad extent ",
						h->total);
				return Status::ErrorNotRecoverable;
			}
			if (h->prevTotal != prevTotal) {
				slog().error("vstore::Arena", "verify: block at ", at, " says prevTotal ",
						h->prevTotal, ", walk says ", prevTotal);
				return Status::ErrorNotRecoverable;
			}
			if (h->size > h->total - BlockHeaderSize) {
				slog().error("vstore::Arena", "verify: block at ", at, " has size ", h->size,
						" beyond its extent ", h->total);
				return Status::ErrorNotRecoverable;
			}

			bool last = (h->flags & BlockFlags::LastInChunk) != 0;
			if (last != (at + h->total == end)) {
				slog().error("vstore::Arena", "verify: LastInChunk mismatch at ", at);
				return Status::ErrorNotRecoverable;
			}

			if ((h->flags & BlockFlags::Used) != 0) {
				++liveCount;
				liveBytes += h->size;
				liveTotal += h->total;
				if (Addr(at) + BlockHeaderSize == r->userRoot) {
					userRootSeen = true;
				}
			} else {
				++freeCount;
				freeBytes += h->size;
				freeTotal += h->total;
				if (h->size != h->total - BlockHeaderSize) {
					slog().error("vstore::Arena", "verify: free block at ", at,
							" does not own its whole payload");
					return Status::ErrorNotRecoverable;
				}
				// Two adjacent free blocks mean a merge was missed, and a missed merge is how a
				// store slowly shreds its large blocks into classes nothing can use.
				if (prevWasFree) {
					slog().error("vstore::Arena", "verify: free block at ", at,
							" follows another free block");
					return Status::ErrorNotRecoverable;
				}
				freeByWalk.emplace(Addr(at));
			}
			prevWasFree = (h->flags & BlockFlags::Used) == 0;

			lastFlags = h->flags;
			prevTotal = h->total;
			at += h->total;
		}

		if (at != end) {
			slog().error("vstore::Arena", "verify: chunk ", slot, " block chain ends at ", at,
					", expected ", end);
			return Status::ErrorNotRecoverable;
		}
		if ((lastFlags & BlockFlags::LastInChunk) == 0) {
			slog().error("vstore::Arena", "verify: chunk ", slot, " has no LastInChunk block");
			return Status::ErrorNotRecoverable;
		}

		// A run exists for exactly one block; if it ever held two, or a free one, the reuse rules
		// for runs (release the whole run at once) would already have been violated.
		if (kind == ChunkKind::OversizeHead) {
			auto h = blockAt(begin);
			constexpr uint16_t needed = BlockFlags::Used | BlockFlags::Oversize;
			if (h->total != chunkExtent(slot) || (h->flags & needed) != needed) {
				slog().error("vstore::Arena", "verify: run at ", slot,
						" is not one live oversize block");
				return Status::ErrorNotRecoverable;
			}
		}
	}

	// The free lists are walked separately and the two views are compared as sets, not just as
	// counts: a block that is physically free but on no list is leaked, and a block on a list that
	// the physical walk says is live would be handed out twice. Neither shows up in a count.
	{
		mem_std::Set<Addr> freeByList;
		for (uint32_t cls = 0; cls < NumSizeClasses; ++cls) {
			Addr prev = NullAddr;
			Addr b = r->freeHead[cls];
			uint64_t guard = 0;
			while (b != NullAddr) {
				if (++guard > freeCount + 1) {
					slog().error("vstore::Arena", "verify: free list ", cls, " is cyclic");
					return Status::ErrorNotRecoverable;
				}
				auto h = blockAt(b);
				if ((h->flags & BlockFlags::Used) != 0 || h->sizeClass != cls) {
					slog().error("vstore::Arena", "verify: block ", b, " is on list ", cls,
							" but says used=", (h->flags & BlockFlags::Used) != 0,
							" class=", h->sizeClass);
					return Status::ErrorNotRecoverable;
				}
				if (linksOf(b)->prev != prev) {
					slog().error("vstore::Arena", "verify: free list ", cls,
							" is not doubly linked at ", b);
					return Status::ErrorNotRecoverable;
				}
				if (!freeByList.emplace(b).second) {
					slog().error("vstore::Arena", "verify: block ", b,
							" appears twice in the lists");
					return Status::ErrorNotRecoverable;
				}
				prev = b;
				b = linksOf(b)->next;
			}
		}

		if (freeByList != freeByWalk) {
			slog().error("vstore::Arena", "verify: the free lists hold ", freeByList.size(),
					" blocks, the physical walk found ", freeByWalk.size());
			return Status::ErrorNotRecoverable;
		}
	}

	if (!userRootSeen) {
		slog().error("vstore::Arena", "verify: userRoot ", r->userRoot,
				" is not the payload of a live block");
		return Status::ErrorNotRecoverable;
	}

	if (oversizeRuns != r->oversizeRuns) {
		slog().error("vstore::Arena", "verify: walk found ", oversizeRuns,
				" oversize runs, root says ", r->oversizeRuns);
		return Status::ErrorNotRecoverable;
	}

	if (liveCount != r->liveCount || liveBytes != r->liveBytes || liveTotal != r->liveTotal
			|| freeCount != r->freeCount || freeBytes != r->freeBytes
			|| freeTotal != r->freeTotal) {
		slog().error("vstore::Arena", "verify: accounting mismatch: walk live ", liveCount, "/",
				liveBytes, "/", liveTotal, " free ", freeCount, "/", freeBytes, "/", freeTotal,
				"; root live ", r->liveCount, "/", r->liveBytes, "/", r->liveTotal, " free ",
				r->freeCount, "/", r->freeBytes, "/", r->freeTotal);
		return Status::ErrorNotRecoverable;
	}

	return Status::Ok;
}

template <typename Tracking>
void ArenaT<Tracking>::describe(mem_std::Value &out) const {
	out = mem_std::Value();
	if (!isInitialized()) {
		return;
	}

	auto r = root();
	out.setInteger(r->chunkCount, "chunkCount");
	out.setInteger(r->maxChunks, "maxChunks");
	out.setInteger(r->firstDataOffset, "firstDataOffset");
	out.setInteger(r->firstPageUsed, "firstPageUsed");
	out.setInteger(r->oversizeRuns, "oversizeRuns");
	out.setInteger(r->userRoot, "userRoot");

	auto &live = out.emplace("live");
	live.setInteger(r->liveCount, "count");
	live.setInteger(r->liveBytes, "bytes");
	live.setInteger(r->liveTotal, "total");

	auto &free = out.emplace("free");
	free.setInteger(r->freeCount, "count");
	free.setInteger(r->freeBytes, "bytes");
	free.setInteger(r->freeTotal, "total");

	auto &chunks = out.emplace("chunks");
	for (uint32_t slot = 0; slot < r->chunkCount; ++slot) {
		auto desc = descs()[slot];
		auto &c = chunks.addDict();
		c.setInteger(uint32_t(getChunkKind(desc)), "kind");
		c.setInteger(getChunkPayload(desc), "payload");
	}

	// Blocks in address order: this is the logical content of the store, and it is what two
	// arenas holding the same data must agree on regardless of how they got there.
	auto &blocks = out.emplace("blocks");
	for (uint32_t slot = 0; slot < r->chunkCount; ++slot) {
		auto kind = getChunkKind(descs()[slot]);
		if (kind == ChunkKind::OversizeTail || kind == ChunkKind::Empty) {
			continue;
		}
		uint64_t at = uint64_t(slot << ChunkShift) + chunkDataBegin(slot);
		uint64_t end = uint64_t(slot << ChunkShift) + chunkExtent(slot);
		while (at < end) {
			auto h = blockAt(Addr(at));
			if (h->total < MinBlockTotal) {
				break; // corrupt; verify() reports it properly
			}
			auto &b = blocks.addDict();
			b.setInteger(at, "addr");
			b.setInteger(h->size, "size");
			b.setInteger(h->total, "total");
			b.setBool((h->flags & BlockFlags::Used) != 0, "used");
			at += h->total;
		}
	}
}

// The instantiations. Everything above is a template whose definition lives in a subunit, so it is
// invisible to other compile units; they see the declaration, emit a plain external reference and
// link against these. No attribute goes on these lines: an attribute list cannot appear in
// explicit-instantiation position, and SP_PUBLIC expands to one. What carries the visibility
// instead is SP_PUBLIC on the class template and on the tracker tags, because an instantiation's
// visibility is the minimum of the template's and its arguments' - an unmarked tag would hide every
// symbol of the arena it selects and the shared build would fail to link against a library that
// compiled.
template class ArenaT<NoTracking>;
template class ArenaT<DirtyTracking>;
#if DEBUG
template class ArenaT<ShadowTracking>;
#endif

} // namespace stappler::vstore
