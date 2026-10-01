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

#ifndef STAPPLER_VSTORE_SPVSTOREARENA_H_
#define STAPPLER_VSTORE_SPVSTOREARENA_H_

#include "SPVStore.h"
#include "SPVStoreTracking.h"

namespace STAPPLER_VERSIONIZED stappler::vstore {

// What a chunk table slot holds, packed into a uint32_t: the table is arena state, so its encoding
// is part of the image format. A block larger than one chunk takes a run of slots backed by one
// host allocation; the head slot records the run length, every tail slot the head's index.
enum class ChunkKind : uint8_t {
	Empty = 0,
	Normal = 1,
	OversizeHead = 2,
	OversizeTail = 3,
};

using ChunkDesc = uint32_t; // bits 0..7 = ChunkKind, bits 8..31 = run length (head) / head index

inline ChunkDesc makeChunkDesc(ChunkKind kind, uint32_t payload) {
	return uint32_t(kind) | (payload << 8);
}
inline ChunkKind getChunkKind(ChunkDesc d) { return ChunkKind(d & 0xff); }
inline uint32_t getChunkPayload(ChunkDesc d) { return d >> 8; }

struct BlockFlags {
	static constexpr uint16_t Used = 1 << 0;
	static constexpr uint16_t Oversize = 1 << 1;
	static constexpr uint16_t LastInChunk = 1 << 2;
};

// Sits immediately before the payload. `size` is what the caller asked for, `total` the extent the
// block occupies (header + payload + granule padding); `prevTotal` makes the chain walkable
// backwards, which is what coalescing with the physical predecessor needs.
struct BlockHeader {
	uint32_t size;
	uint32_t total;
	uint32_t prevTotal; // 0 for the first block of a chunk
	uint16_t flags;
	uint16_t sizeClass; // free-list index while free; FreeClassNone otherwise
};

// Lives in the payload of a free block. Doubly linked because coalescing has to unlink a block
// found through its physical neighbours rather than by walking a list to it.
struct FreeLinks {
	Addr next;
	Addr prev;
};

static constexpr uint16_t FreeClassNone = 0xffff;
static constexpr uint32_t BlockHeaderSize = uint32_t(sizeof(BlockHeader));
static constexpr uint32_t MinBlockTotal = BlockHeaderSize + MinPayload;
static constexpr uint32_t MaxInChunkPayload = ChunkSize - BlockHeaderSize;

static constexpr uint32_t NumSizeClasses = 40;

// The store's superblock, at offset 0 of chunk 0, followed by the chunk descriptor table. Its
// fixed, known position is what makes an image self-describing: adopt() reads the first bytes and
// learns everything it needs to rebuild the store, with nothing passed out of band.
struct Root {
	uint32_t magic;
	uint16_t formatVersion;
	uint16_t chunkShift;
	uint32_t maxChunks;
	uint32_t chunkCount;
	uint32_t firstDataOffset; // where chunk 0's block chain starts
	uint32_t oversizeRuns;

	// Checked on adopt() as chunkShift is; neither is used to interpret the image, both are
	// compile-time constants of the reader. Stored so that an image written at a different geometry
	// is refused at the header rather than found out later by a firstDataOffset that does not add
	// up.
	uint16_t pageShift;
	uint16_t pad0;
	uint64_t liveCount, liveBytes, liveTotal;
	uint64_t freeCount, freeBytes, freeTotal;
	Addr freeHead[NumSizeClasses]; // by size class

	// The one address a layer above may plant here, so an image carries its own entry point (an
	// index's root). NullAddr when the store holds no index.
	Addr userRoot;

	// How many bytes of the store's first page can hold anything: this superblock plus the part of
	// the descriptor table that describes slots the store actually has. By ARENA-P0 no block is
	// there. A ceiling rather than a description of what changed, derived from chunkCount, so
	// verify() recomputes it rather than trusting it.
	uint32_t firstPageUsed;
	uint32_t reserved[6];
};

static constexpr uint32_t RootMagic = 0x4154'5356; // 'VSTA'
// 3: the page became 16 KiB and Root gained pageShift. There is no migration - an image is a
// snapshot of a running store, not an archive, and a reader that cannot interpret one refuses at
// the header.
static constexpr uint16_t RootFormatVersion = 3;
static constexpr uint32_t DescsOffset = uint32_t((sizeof(Root) + MaxAlign - 1) & ~(MaxAlign - 1));

// ARENA-P0: the first page of the store is the header's, whole, and never holds a block. The data
// area therefore starts at a page boundary at every budget, which is what lets Root::firstPageUsed
// bound page 0's content from the store's shape rather than from its workload.
inline uint32_t firstDataOffsetFor(uint32_t maxChunks) {
	auto tableEnd = alignUp(DescsOffset + maxChunks * uint32_t(sizeof(ChunkDesc)), MaxAlign);
	return tableEnd < PageSize ? PageSize : tableEnd;
}

// The ceiling behind Root::firstPageUsed. Slots the store does not have yet have never had a
// descriptor written, so the table's capacity past chunkCount is untouched zeros.
inline uint32_t firstPageUsedFor(uint32_t chunkCount) {
	auto used = DescsOffset + chunkCount * uint32_t(sizeof(ChunkDesc));
	return used < PageSize ? used : PageSize;
}

// A contiguous stretch of released chunk memory, `count` chunks long. Reuse has to preserve
// contiguity: a run is one allocation, and once it is shredded into loose chunks nothing can put it
// back together.
struct SpareExtent {
	uint8_t *base;
	uint32_t count;
};

// How the host chunk table has to change to match a given descriptor table. Computed and reserved
// before anything is mutated, so a rollback that cannot get memory fails with the store untouched.
// At namespace scope so a plan for a tracked store is the same type as a plan for a shadowed one.
struct RehostPlan {
	struct Entry {
		uint32_t slot;
		uint32_t count; // 1, or the run length for a head
		uint8_t *memory; // reserved buffer; null means "unhost this slot"
	};
	uint32_t chunkCount = 0;
	mem_std::Vector<Entry> entries; // only the slots that change
	bool valid = false;
};

// A relocatable heap addressed by 32-bit offsets. The defining invariant: a byte-wise copy of the
// chunks is the complete state, so an arena rebuilt from that copy is indistinguishable from the
// original, and snapshots, rollback and keyframes all follow from it. Not thread-safe: to move a
// store between threads, move its pool. `Tracking` is what the store remembers about its own writes
// and the only thing that differs between the three aliases below; there is no bare `Arena` alias,
// so every call site says which kind it means.
template <typename Tracking>
class SP_PUBLIC ArenaT final {
public:
	using TrackingType = Tracking;

	// Whether this kind of store announces its writes, and therefore whether a journal can watch
	// it. Ordinary code branches on this with `if constexpr` rather than growing a preprocessor
	// arm.
	static constexpr bool IsTracked = Tracking::IsTracked;
	static constexpr bool HasShadow = Tracking::HasShadow;

	ArenaT() = default;
	~ArenaT();

	ArenaT(const ArenaT &) = delete;
	ArenaT &operator=(const ArenaT &) = delete;

	bool init(const Config & = Config());

	// Rebuilds a store from an image produced by save(). The image is copied, never aliased: a
	// keyframe must not have to outlive every arena ever restored from it.
	Status adopt(BytesView image, const Config & = Config());

	bool isInitialized() const { return !_chunks.empty(); }

	// A read-only view of a live store: the chunk table copied, nothing else. It reads the store's
	// bytes without touching the store's own `_chunks`, which reallocates when the store grows, so
	// a worker thread may read through it while the owner goes on - as long as the owner does not
	// move or free a chunk meanwhile (setFrozen). It allocates nothing and owns no pool; destroying
	// it releases the copy.
	Status initView(const ArenaT &src);
	bool isView() const { return _view; }

	// A write view of a live store: the same snapshot of the chunk table, taken by a worker that
	// will write bytes into blocks the owner allocated before the snapshot. It cannot allocate -
	// alloc(), realloc() and free() refuse through a view in every build (getViewAllocCount) - so
	// the owner's free lists, block headers and tables are never touched from another thread. Its
	// dirty map is its own, because marking is a read-modify-write of one word per page and two
	// frames routinely share a page; the owner takes the marks back with mergeDirtyFrom() on its
	// own thread. The owner must not free or move anything meanwhile (setFrozen).
	Status initWriteView(const ArenaT &src);
	bool isWriteView() const { return _writeView; }

	// Gives up a view: releases the copy of the chunk table and nothing else, after which the
	// object may be pointed at another store. Only legal on a view.
	void closeView();

	// Takes the pages a write view marked and marks them here. The view is left as it is; a store
	// that has grown since the snapshot keeps the marks of its newer pages.
	void mergeDirtyFrom(const ArenaT &view) requires (Tracking::IsTracked);

	// Moves the store here without copying a byte: the pool, the chunk table and the spare extents
	// change hands, and the tracker is rebuilt rather than transferred, a dirty map and a shadow
	// being host-side derived state. The source is left uninitialized - one set of chunks must not
	// have two owners - and the map starts empty, since nothing has been written to this store
	// since it took over.
	template <typename Other>
	Status adoptFrom(ArenaT<Other> &src);

	// How many observers currently hold a reference to this store, a journal in practice. Its one
	// use is catching a move out from under one.
#if DEBUG
	uint32_t getObserverCount() const { return _observers; }
	void noteObserver() { ++_observers; }
	void forgetObserver() {
		if (_observers > 0) {
			--_observers;
		}
	}
#endif

	// The same number where an assertion needs it in both builds; a release build has no counter
	// and answers zero.
	uint32_t getObserverCountForAssert() const {
#if DEBUG
		return _observers;
#else
		return 0;
#endif
	}

	// Returns the payload address, or NullAddr if the request cannot be satisfied. `align` above
	// MaxAlign is rejected rather than silently rounded.
	Addr alloc(uint32_t size, uint32_t align = MaxAlign);

	// Returns NullAddr on failure, in which case the original block is left completely intact -
	// a caller that cannot grow still has everything it had.
	Addr realloc(Addr, uint32_t newSize, uint32_t align = MaxAlign);

	void free(Addr);

	// Debug cross-check against what the block header says. The header is authoritative - a
	// caller-supplied size is only ever an assertion, never an input to the free path.
	void free(Addr, uint32_t size);

	// The size originally requested, not the extent occupied.
	uint32_t sizeOf(Addr) const;

	// Whether `a` looks like the payload of a live block: inside the store, granule-aligned, and
	// the header before it says Used. Not a proof - only verify()'s structural walk is - but
	// exactly the assumption sizeOf() and free() already make, so a holder of an Addr it did not
	// allocate can be told apart from a number.
	bool isLivePayload(Addr) const;

	// A pointer from read()/write() is valid only until the next allocator call: alloc() may grow
	// the store and free() may release a chunk. Addresses are stable, pointers are not. Inline
	// because in a release build there is nothing left of either but the address arithmetic.
	const uint8_t *read(Addr a, uint32_t size) const {
		SP_VSTORE_COUNT(arenaRead);
		sprt_passert(checkRange(a, size), "vstore::Arena: read outside the store");
		return resolve(a);
	}

	uint8_t *write(Addr a, uint32_t size) {
		SP_VSTORE_COUNT(arenaWrite);
		sprt_passert(checkRange(a, size), "vstore::Arena: write outside the store");
		if constexpr (Tracking::HasShadow) {
			++_tracking._writeSeq;
		}
		markDirty(a, size);
		return resolve(a);
	}

	uint32_t getChunkCount() const;
	uint32_t getMaxChunks() const;

	// Whether a slot carries data, decided by the descriptor alone and never by the host pointer.
	// An observer must use this definition to know a page's previous content: a slot that is not
	// live reads as zeros whatever the memory behind it holds. The two criteria differ only inside
	// addChunkRun().
	bool isSlotLive(uint32_t slot) const;

	// Whether a slot currently has memory attached. Used to decide what can be written into, never
	// what its logical content is.
	bool isHosted(uint32_t slot) const;

	// The slot's descriptor. A rollback restores these from the journal and hands the restored
	// table back to planRehost().
	ChunkDesc getChunkDesc(uint32_t slot) const;

	// Raw page access for an observer capturing before-images. Returns nullptr for a slot that is
	// not live, which the caller must read as "this page is zeros".
	const uint8_t *getPage(uint32_t slot, uint32_t page) const;

	// How many of a page's bytes can differ between two versions. PageSize for every page but the
	// store's first, which by ARENA-P0 holds only the superblock and the live part of the
	// descriptor table, so a reader may stop at Root::firstPageUsed and take the rest as zeros in
	// both images.
	uint32_t getPageFootprint(uint32_t slot, uint32_t page) const;

	// Writes a page without marking it. Only a rollback may use this: it is putting back bytes the
	// journal already holds, and marking them would make the version being rolled into look as
	// though it had written them. Skips checkRange() too - the descriptors and the host table are
	// briefly out of step.
	uint8_t *getPageForRestore(uint32_t slot, uint32_t page);

	// Where the store has been written since the map was last cleared: one entry per page, indexed
	// by the flat page number `addr >> PageShift`, one bit per 256-byte sub-block. This is the
	// whole of the arena's contact with versioning - the arena announces a write to itself and
	// whoever wants to know reads the map at a moment of their own choosing, so no path leads out
	// of the allocator into a compressor. The sub-block width is a word, not a tuning parameter.
	// Constrained rather than emptied: a caller that asks a plain arena fails to compile instead of
	// reading "nothing was written".
	SpanView<uint64_t> getDirtyMap() const requires (Tracking::IsTracked)
	{
		return _tracking.map();
	}

	void clearDirtyMap() requires (Tracking::IsTracked);

	// Reads `targetDescs` - the restored table, not the live one - and reserves every buffer the
	// change would need. On failure it returns everything it took, so the store is untouched. Not
	// const: reserving draws on the spare extents.
	Status planRehost(const ChunkDesc *targetDescs, uint32_t targetCount, RehostPlan &);

	// Hands a plan's reservations back without applying it.
	void discardRehost(RehostPlan &);

	// Applies a plan. Cannot fail, announces nothing, and preserves content: a slot that moves
	// takes its bytes with it, a slot that gains memory it never had is zeroed. Rollback must not
	// depend on the journal covering every page of a re-hosted slot.
	void rehost(RehostPlan &);

	// The image is a flat, slot-indexed buffer of saveSize() bytes: chunk N lies at N * ChunkSize,
	// so a reader finds it without walking anything, and that is also the shape a keyframe wants.
	size_t saveSize() const;

	// Emits the image as pieces to be concatenated in order. A piece with `zero` set carries a null
	// pointer and stands for `size` bytes of zeros, so a released slot costs nothing to emit. Piece
	// boundaries are not part of the format - only the concatenation is; adjacent slots with
	// contiguous memory are merged.
	void save(const Callback<void(const uint8_t *data, size_t size, bool zero)> &) const;

	// Full structural sweep in both directions. Returns ErrorNotRecoverable and logs the first
	// violation it finds; callers treat anything but Ok as "the store is corrupt".
	Status verify() const;

	// Logical dump: what the store holds, independent of where the allocator happened to put it.
	void describe(mem_std::Value &) const;

	// The single entry point an upper layer may leave in the image; without it a save() image could
	// only be interpreted by remembering an Addr out of band. One address rather than a directory,
	// because the layer above can build any directory it likes behind it. Announced through the
	// barrier like any other Root write; verify() then requires it to be NullAddr or a live block's
	// payload.
	Status setUserRoot(Addr);
	Addr getUserRoot() const;

	// A layer above may cache an address obtained from this store only while this number is
	// unchanged. It is bumped by everything that can move a row or replace the store's bytes: a
	// swap-remove in a component pool, Journal::rollback, adopt(), setUserRoot(). Host-side, so a
	// rollback cannot return it to a value a cache has already seen. Allocating, freeing or growing
	// a block does not change it - pages never move, so a record's address survives every growth of
	// the array holding it.
	uint32_t getEpoch() const { return _epoch; }

	// "Anything derived from my bytes may now be wrong." Called by the store and the journal; a
	// caller that mutates the arena through them never needs to.
	void invalidateDerived() { ++_epoch; }

	// The shadow validator: the barrier is only as good as its coverage, and a metadata write that
	// went around it would leave a rollback restoring user bytes but not the allocator's own state.
	// Constrained to the kind that carries a shadow rather than gated on DEBUG, so the proof is
	// about the same source a PlainArena instantiates.

	// Whether this store actually carries one: the kind decides whether it can - a PlainArena
	// answers false with no runtime test - and Config::debugShadow whether it does.
	bool hasShadow() const {
		if constexpr (Tracking::HasShadow) {
			return _tracking._enabled;
		} else {
			return false;
		}
	}

	// Whether an owner may assert verify() on its own schedule. A runtime flag rather than part of
	// the kind: the shadow checks the write barrier's coverage and this the arena's structure, they
	// fail for unrelated reasons, and a benchmark needs to silence one without the other.
	bool hasVerifyEnabled() const { return _verifyEnabled; }

	// Refreshes the shadow copy and clears the dirty map. Everything after this point is what the
	// next validate() will judge.
	void resync() requires (Tracking::HasShadow);

	// Reports the first page that changed without being marked.
	Status validate(const Callback<void(uint32_t slot, uint32_t page)> &onMismatch) const
			requires (Tracking::HasShadow);

	// Test-only. Hands out a writable pointer without marking, so the negative case need not
	// fabricate an out-of-barrier write through a const_cast.
	uint8_t *unbarrieredWriteForTesting(Addr, uint32_t size) requires (Tracking::HasShadow);

	// How many times the barrier has been asked for a writable pointer. Monotone, meaningless as a
	// quantity and useful for one thing: proving that a stretch of code wrote nothing. That is what
	// the extension time hooks need, running on the far side of a version boundary where a write
	// has no unit of work.
	uint64_t getWriteSeq() const requires (Tracking::HasShadow)
	{
		return _tracking._writeSeq;
	}

	// The thread the store belongs to: from now on a write from any other is a violation. Claimed
	// by whoever runs the workload over the store.
	void setOwnerThread() requires (Tracking::HasShadow)
	{
		_tracking._owner = sprt::this_thread::get_id();
	}
	void clearOwnerThread() requires (Tracking::HasShadow)
	{
		_tracking._owner = sprt::thread::id();
	}

	// While frozen, an allocation, a reallocation or a free is a violation: views of the store are
	// being read on other threads.
	void setFrozen(bool value) requires (Tracking::HasShadow)
	{
		if (value) {
			++_tracking._frozen;
		} else if (_tracking._frozen > 0) {
			--_tracking._frozen;
		}
	}
	bool isFrozen() const requires (Tracking::HasShadow)
	{
		return _tracking._frozen > 0;
	}

private:
	uint8_t *resolve(Addr a) const { return _chunks[a >> ChunkShift] + (a & ChunkMask); }

	Root *root() const { return reinterpret_cast<Root *>(_chunks[0]); }
	ChunkDesc *descs() const { return reinterpret_cast<ChunkDesc *>(_chunks[0] + DescsOffset); }
	BlockHeader *blockAt(Addr blockAddr) const {
		return reinterpret_cast<BlockHeader *>(resolve(blockAddr));
	}

	uint32_t chunkDataBegin(uint32_t slot) const { return slot == 0 ? root()->firstDataOffset : 0; }

	// The extent a slot's block chain covers: one chunk normally, the whole run for an oversize
	// head. Tail slots have no chain of their own.
	uint32_t chunkExtent(uint32_t slot) const;

	bool growChunk();
	Addr carveFrom(Addr block, uint32_t size, uint32_t total);

	FreeLinks *linksOf(Addr block) const {
		return reinterpret_cast<FreeLinks *>(resolve(block + BlockHeaderSize));
	}
	void pushFree(Addr block);
	void unlinkFree(Addr block);

	// The single place that turns an extent back into free space. Merges with the physical
	// neighbours, which is what keeps "no two adjacent free blocks" true - and that rule is what
	// stops the store from shredding its large blocks into unusable small classes.
	void insertFreeRange(Addr block);

	// The physically following block's prevTotal has to track any change to this block's extent,
	// or the backwards walk that coalescing depends on silently goes wrong.
	void setNextPrevTotal(Addr block, uint32_t total);

	void releaseRun(Addr block);

	// Reserves `count` consecutive descriptor slots backed by one host allocation.
	bool addChunkRun(uint32_t count, uint32_t &outSlot);

	// Takes `count` contiguous chunks out of the spare extents, splitting a larger one if need be.
	uint8_t *takeSpare(uint32_t count);
	void giveSpare(uint8_t *base, uint32_t count);
	Addr allocOversize(uint32_t size);

	// Derives the host chunk table from the descriptor table, allocating and filling from `image`.
	bool rebuildHostState(BytesView image);

	void releasePool();

	// Every allocator entry point asks first: a view never allocates, and a frozen store does not
	// move. False means the call was refused and the caller answers as it does for a full store.
	bool checkAllocatorCall() const;

	bool checkRange(Addr, uint32_t size) const;

	// The barrier. Every mutation of arena state - the caller's payload and the allocator's own
	// metadata alike - goes through here; the *ForWrite accessors below exist so that a metadata
	// write cannot easily be spelled without also marking itself. What marking does is the
	// tracker's business and lives in SPVStoreTracking.h; for a PlainArena the call folds to
	// nothing and this body is empty.
	void markDirty(Addr a, uint32_t size) {
		SP_VSTORE_COUNT(arenaMark);
		if constexpr (Tracking::HasShadow) {
			if (!_tracking.admitsWrite()) {
				noteForeignWrite();
				return;
			}
		} else {
			sprt_passert(!_view || _writeView, "vstore::Arena: a view is read-only");
		}
		_tracking.mark(a, size);
	}

	// The dirty map and the debug shadow are both derived from _chunks, so they follow it rather
	// than growing on demand at every use. One call after each place _chunks changes size is what
	// buys the barrier its unchecked index.
	void syncSlotArrays();

	void markRoot() { markDirty(0, uint32_t(sizeof(Root))); }
	void markDesc(uint32_t slot) {
		markDirty(DescsOffset + slot * uint32_t(sizeof(ChunkDesc)), uint32_t(sizeof(ChunkDesc)));
	}

	Root *rootForWrite() {
		markRoot();
		return root();
	}
	ChunkDesc *descForWrite(uint32_t slot) {
		markDesc(slot);
		return descs() + slot;
	}
	BlockHeader *headerForWrite(Addr block) {
		markDirty(block, BlockHeaderSize);
		return blockAt(block);
	}
	FreeLinks *linksForWrite(Addr block) {
		markDirty(block + BlockHeaderSize, uint32_t(sizeof(FreeLinks)));
		return linksOf(block);
	}

	memory::pool_t *_pool = nullptr;
	bool _ownsPool = false;

	// Derived host state: rebuilt from the descriptor table, never the source of truth.
	mem_std::Vector<uint8_t *> _chunks;

	// Memory whose slots were released; a pool never gives an individual block back, so reuse has
	// to happen here. Kept as extents rather than single chunks: a run needs contiguous memory, and
	// a run shredded into loose chunks can never be reassembled.
	mem_std::Vector<SpareExtent> _spareChunks;

	// initView(): a copy of another store's chunk table, owning nothing.
	bool _view = false;

	// initWriteView(): a view whose holder may change bytes of blocks that existed when the
	// snapshot was taken. Still cannot allocate - `_view` governs that, and this only lifts the
	// read-only stamp on the barrier.
	bool _writeView = false;

	// See getEpoch(). Starts at 1 rather than 0 so that a zero-initialized cache stamp cannot match
	// a fresh arena's, and a cache that was never filled cannot read as valid.
	uint32_t _epoch = 1;

#if DEBUG
	// See getObserverCount(). Debug only: it exists to make one assertion possible.
	uint32_t _observers = 0;
#endif

	// Whether an owner may assert verify(). Not part of the tracker: it governs a check of the
	// arena's structure, which every kind has.
	bool _verifyEnabled = true;

	// So that adoptFrom() may take another specialization's chunks. Specializations of one template
	// are unrelated types and are not friends of each other by default.
	template <typename Other>
	friend class ArenaT;

	// What this store remembers about its own writes, and the only member whose presence depends on
	// the kind. [[no_unique_address]] is what makes NoTracking cost a PlainArena nothing at all.
	SPRT_NO_UNIQUE_ADDRESS
	Tracking _tracking;
};

// The three kinds. There is deliberately no bare `Arena` alias: every site that holds a store is
// made to say which kind it means.

// The release store. Its write() compiles to exactly what its read() compiles to.
using PlainArena = ArenaT<NoTracking>;

// The journalled store: it announces its writes, so a Journal can watch it.
using TrackedArena = ArenaT<DirtyTracking>;

#if DEBUG
// The debugger's store: announces its writes AND keeps a copy to check the announcements against.
using ShadowArena = ArenaT<ShadowTracking>;
#else
// In a release build the shadow is a check with nothing to check for, so this is an alias and not a
// third instantiation of the whole stack. Code that names it still compiles.
using ShadowArena = TrackedArena;
#endif

// A store whose kind is not known at compile time, for the seams where the type cannot travel: a
// virtual function cannot be a template, and a plain function pointer in a registry cannot carry
// one. It offers the same members ArenaT does, so every template above it instantiates for it
// exactly as for a real kind. A table of function pointers rather than a virtual base, because ArenaT is
// `final` and stays a value type, and rather than an inlined copy of the resolve state, because
// `_chunks` reallocates when the store grows. The cost is one indirect call per access.
struct SP_PUBLIC ArenaOps {
	const uint8_t *(*read)(const void *, Addr, uint32_t);
	uint8_t *(*write)(void *, Addr, uint32_t);
	Addr (*alloc)(void *, uint32_t, uint32_t);
	Addr (*realloc)(void *, Addr, uint32_t, uint32_t);
	void (*free)(void *, Addr);
	uint32_t (*sizeOf)(const void *, Addr);
	bool (*isLivePayload)(const void *, Addr);
	bool (*isInitialized)(const void *);
	Addr (*getUserRoot)(const void *);
	Status (*setUserRoot)(void *, Addr);
	uint32_t (*getEpoch)(const void *);
	void (*invalidateDerived)(void *);

	// Zero for a kind that keeps no shadow, which is the one narrowing this erasure costs: the "a
	// time hook wrote the store" alarm can only fire on a ShadowArena.
	uint64_t (*getWriteSeq)(const void *);
};

class SP_PUBLIC ArenaRef final {
public:
	ArenaRef() = default;

	// Deliberately not explicit and not a constructor on ArenaT: `arena.ref()` reads as a
	// conversion the caller asked for, and an implicit one would let a store be erased by accident.
	template <typename Tracking>
	static ArenaRef of(ArenaT<Tracking> &);

	const uint8_t *read(Addr a, uint32_t size) const { return _ops->read(_arena, a, size); }
	uint8_t *write(Addr a, uint32_t size) { return _ops->write(_arena, a, size); }
	Addr alloc(uint32_t size, uint32_t align = MaxAlign) {
		return _ops->alloc(_arena, size, align);
	}
	Addr realloc(Addr a, uint32_t size, uint32_t align = MaxAlign) {
		return _ops->realloc(_arena, a, size, align);
	}
	void free(Addr a) { _ops->free(_arena, a); }
	uint32_t sizeOf(Addr a) const { return _ops->sizeOf(_arena, a); }
	bool isLivePayload(Addr a) const { return _ops->isLivePayload(_arena, a); }
	bool isInitialized() const { return _ops && _ops->isInitialized(_arena); }
	Addr getUserRoot() const { return _ops->getUserRoot(_arena); }
	Status setUserRoot(Addr a) { return _ops->setUserRoot(_arena, a); }
	uint32_t getEpoch() const { return _ops->getEpoch(_arena); }
	void invalidateDerived() { _ops->invalidateDerived(_arena); }
	uint64_t getWriteSeq() const { return _ops->getWriteSeq(_arena); }

	bool isNull() const { return _ops == nullptr; }

	// Two refs are the same store when they name the same object, whatever kind either was made
	// from.
	bool operator==(const ArenaRef &other) const { return _arena == other._arena; }

private:
	void *_arena = nullptr;
	const ArenaOps *_ops = nullptr;
};

// The table for one kind, built once per instantiation. Out of line in the class so that `ref()`
// is the only way a caller ever names it.
template <typename Tracking>
ArenaRef ArenaRef::of(ArenaT<Tracking> &a) {
	using A = ArenaT<Tracking>;
	static const ArenaOps ops = {
		[](const void *p, Addr x, uint32_t n) { return static_cast<const A *>(p)->read(x, n); },
		[](void *p, Addr x, uint32_t n) { return static_cast<A *>(p)->write(x, n); },
		[](void *p, uint32_t n, uint32_t al) { return static_cast<A *>(p)->alloc(n, al); },
		[](void *p, Addr x, uint32_t n, uint32_t al) {
		return static_cast<A *>(p)->realloc(x, n, al);
	},
		[](void *p, Addr x) { static_cast<A *>(p)->free(x); },
		[](const void *p, Addr x) { return static_cast<const A *>(p)->sizeOf(x); },
		[](const void *p, Addr x) { return static_cast<const A *>(p)->isLivePayload(x); },
		[](const void *p) { return static_cast<const A *>(p)->isInitialized(); },
		[](const void *p) { return static_cast<const A *>(p)->getUserRoot(); },
		[](void *p, Addr x) { return static_cast<A *>(p)->setUserRoot(x); },
		[](const void *p) { return static_cast<const A *>(p)->getEpoch(); },
		[](void *p) { static_cast<A *>(p)->invalidateDerived(); },
		[](const void *p) -> uint64_t {
		if constexpr (Tracking::HasShadow) {
			return static_cast<const A *>(p)->getWriteSeq();
		} else {
			return 0;
		}
	},
	};
	ArenaRef r;
	r._arena = &a;
	r._ops = &ops;
	return r;
}

// The observer's view of a tracked store, erased. Everything the journal asks of an arena is coarse
// - a page, a slot, a whole image, once per version - so an indirect call costs nothing measurable,
// and the Journal stops being a template over the kind: one journal can watch a tracked scene and a
// plain local store at once. Constructible only from a kind that tracks.
class SP_PUBLIC TrackedArenaRef;

struct SP_PUBLIC TrackedArenaOps {
	bool (*isInitialized)(const void *);
	uint32_t (*getChunkCount)(const void *);
	bool (*isHosted)(const void *, uint32_t);
	const uint8_t *(*getPage)(const void *, uint32_t, uint32_t);
	uint32_t (*getPageFootprint)(const void *, uint32_t, uint32_t);
	uint8_t *(*getPageForRestore)(void *, uint32_t, uint32_t);
	SpanView<uint64_t> (*getDirtyMap)(const void *);
	void (*clearDirtyMap)(void *);
	Status (*planRehost)(void *, const ChunkDesc *, uint32_t, RehostPlan &);
	void (*discardRehost)(void *, RehostPlan &);
	void (*rehost)(void *, RehostPlan &);
	size_t (*saveSize)(const void *);
	void (*save)(const void *, const Callback<void(const uint8_t *, size_t, bool)> &);
	Status (*verify)(const void *);
	void (*invalidateDerived)(void *);

	// The shadow, if there is one. `resync` and `validate` are no-ops for a kind that keeps none,
	// so a caller guards with hasShadow() and does not branch on the kind.
	bool (*hasShadow)(const void *);
	bool (*hasVerifyEnabled)(const void *);
	void (*resync)(void *);
	Status (*validate)(const void *, const Callback<void(uint32_t, uint32_t)> &);

	// Records that somebody is watching, and that they have stopped. See
	// ArenaT::getObserverCount(): it is what lets adoptFrom() refuse to move a store out from under
	// a journal, and a destroyed journal stop standing in the way.
	void (*noteObserver)(void *);
	void (*forgetObserver)(void *);
};

class SP_PUBLIC TrackedArenaRef final {
public:
	TrackedArenaRef() = default;

	template <typename Tracking>
	requires (Tracking::IsTracked)
	static TrackedArenaRef of(ArenaT<Tracking> &);

	bool isNull() const { return _ops == nullptr; }
	bool isInitialized() const { return _ops && _ops->isInitialized(_arena); }

	uint32_t getChunkCount() const { return _ops->getChunkCount(_arena); }
	bool isHosted(uint32_t slot) const { return _ops->isHosted(_arena, slot); }
	const uint8_t *getPage(uint32_t slot, uint32_t page) const {
		return _ops->getPage(_arena, slot, page);
	}
	uint32_t getPageFootprint(uint32_t slot, uint32_t page) const {
		return _ops->getPageFootprint(_arena, slot, page);
	}
	uint8_t *getPageForRestore(uint32_t slot, uint32_t page) {
		return _ops->getPageForRestore(_arena, slot, page);
	}
	SpanView<uint64_t> getDirtyMap() const { return _ops->getDirtyMap(_arena); }
	void clearDirtyMap() { _ops->clearDirtyMap(_arena); }
	Status planRehost(const ChunkDesc *descs, uint32_t count, RehostPlan &plan) {
		return _ops->planRehost(_arena, descs, count, plan);
	}
	void discardRehost(RehostPlan &plan) { _ops->discardRehost(_arena, plan); }
	void rehost(RehostPlan &plan) { _ops->rehost(_arena, plan); }
	size_t saveSize() const { return _ops->saveSize(_arena); }
	void save(const Callback<void(const uint8_t *, size_t, bool)> &cb) const {
		_ops->save(_arena, cb);
	}
	Status verify() const { return _ops->verify(_arena); }
	void invalidateDerived() { _ops->invalidateDerived(_arena); }

	bool hasShadow() const { return _ops->hasShadow(_arena); }
	bool hasVerifyEnabled() const { return _ops->hasVerifyEnabled(_arena); }
	void resync() { _ops->resync(_arena); }
	Status validate(const Callback<void(uint32_t, uint32_t)> &cb) const {
		return _ops->validate(_arena, cb);
	}
	void noteObserver() { _ops->noteObserver(_arena); }
	void forgetObserver() {
		if (_ops) {
			_ops->forgetObserver(_arena);
		}
	}

	bool operator==(const TrackedArenaRef &other) const { return _arena == other._arena; }

private:
	void *_arena = nullptr;
	const TrackedArenaOps *_ops = nullptr;
};

template <typename Tracking>
requires (Tracking::IsTracked)
TrackedArenaRef TrackedArenaRef::of(ArenaT<Tracking> &a) {
	using A = ArenaT<Tracking>;
	static const TrackedArenaOps ops = {
		[](const void *p) { return static_cast<const A *>(p)->isInitialized(); },
		[](const void *p) { return static_cast<const A *>(p)->getChunkCount(); },
		[](const void *p, uint32_t s) { return static_cast<const A *>(p)->isHosted(s); },
		[](const void *p, uint32_t s, uint32_t g) {
		return static_cast<const A *>(p)->getPage(s, g);
	},
		[](const void *p, uint32_t s, uint32_t g) {
		return static_cast<const A *>(p)->getPageFootprint(s, g);
	},
		[](void *p, uint32_t s, uint32_t g) {
		return static_cast<A *>(p)->getPageForRestore(s, g);
	},
		[](const void *p) { return static_cast<const A *>(p)->getDirtyMap(); },
		[](void *p) { static_cast<A *>(p)->clearDirtyMap(); },
		[](void *p, const ChunkDesc *d, uint32_t n, RehostPlan &pl) {
		return static_cast<A *>(p)->planRehost(d, n, pl);
	},
		[](void *p, RehostPlan &pl) { static_cast<A *>(p)->discardRehost(pl); },
		[](void *p, RehostPlan &pl) { static_cast<A *>(p)->rehost(pl); },
		[](const void *p) { return static_cast<const A *>(p)->saveSize(); },
		[](const void *p, const Callback<void(const uint8_t *, size_t, bool)> &cb) {
		static_cast<const A *>(p)->save(cb);
	},
		[](const void *p) { return static_cast<const A *>(p)->verify(); },
		[](void *p) { static_cast<A *>(p)->invalidateDerived(); },
		[](const void *p) { return static_cast<const A *>(p)->hasShadow(); },
		[](const void *p) { return static_cast<const A *>(p)->hasVerifyEnabled(); },
		[](void *p) {
		if constexpr (Tracking::HasShadow) {
			static_cast<A *>(p)->resync();
		}
	},
		[](const void *p, const Callback<void(uint32_t, uint32_t)> &cb) {
		if constexpr (Tracking::HasShadow) {
			return static_cast<const A *>(p)->validate(cb);
		} else {
			return Status::Ok;
		}
	},
		[](void *p) {
#if DEBUG
		static_cast<A *>(p)->noteObserver();
#else
		(void)p;
#endif
	},
		[](void *p) {
#if DEBUG
		static_cast<A *>(p)->forgetObserver();
#else
		(void)p;
#endif
	},
	};
	TrackedArenaRef r;
	r._arena = &a;
	r._ops = &ops;
	return r;
}

// How a host-side handle keeps hold of its arena. A real kind is held by pointer, the arena being
// an object with a life of its own that the handle does not own; the erased kind by value, an
// ArenaRef being two words with nowhere else to live - which is what lets a store over ArenaRef be
// the ordinary template rather than a hand-written second copy of its API.
template <typename A>
class ArenaHolder {
public:
	ArenaHolder() = default;

	ArenaHolder &operator=(A *a) {
		_a = a;
		return *this;
	}
	ArenaHolder &operator=(std::nullptr_t) {
		_a = nullptr;
		return *this;
	}

	A *get() const { return _a; }
	A *operator->() const { return _a; }
	A &operator*() const { return *_a; }

	explicit operator bool() const { return _a != nullptr; }
	bool operator==(std::nullptr_t) const { return _a == nullptr; }

private:
	A *_a = nullptr;
};

template <>
class ArenaHolder<ArenaRef> {
public:
	ArenaHolder() = default;

	ArenaHolder &operator=(ArenaRef *a) {
		_a = a ? *a : ArenaRef();
		return *this;
	}
	ArenaHolder &operator=(std::nullptr_t) {
		_a = ArenaRef();
		return *this;
	}

	// Into this object, so a copy of the holder resolves through the copy's own ref. That is what
	// makes the erased store an ordinary copyable value the way the real one is.
	ArenaRef *get() const { return const_cast<ArenaRef *>(&_a); }
	ArenaRef *operator->() const { return get(); }
	ArenaRef &operator*() const { return *get(); }

	explicit operator bool() const { return !_a.isNull(); }
	bool operator==(std::nullptr_t) const { return _a.isNull(); }

private:
	ArenaRef _a;
};

} // namespace stappler::vstore

#endif /* STAPPLER_VSTORE_SPVSTOREARENA_H_ */
