/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

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

#ifndef XENOLITH_CORE_XLCOREFRAMEDATACACHE_H_
#define XENOLITH_CORE_XLCOREFRAMEDATACACHE_H_

#include "XLCoreObject.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::core {

/* THE DATA A REMOTE CLIENT HAS ALREADY SENT: a per-session cache of frame data on the server.

A remote client draws the same vertex sets and gradients frame after frame; only their transforms
move. Each set carries a client identity {id, generation} that changes whenever its content does
(XLNodeInfo.h, DataIdentity), so once the server holds a set the client can send the identity alone.

The CLIENT decides everything: it keeps a mirror of what the server holds (FrameDataMirror), evicts
the least recently used entries to stay within the budget the server announced, and describes each
change as an operation - Store (id, generation, bytes) or Drop (id). The operations of one serialized
input travel in the same FrameInput message as the references to them, and the server applies them
BEFORE it looks the frame up - even for a frame it has already cancelled - so the mirror and the
cache change in the same order on both sides, and no acknowledgement is needed.

The server only checks (FrameDataCache): a reference to data it does not hold, or a store past the
budget, is a protocol error. It drops the whole cache and starts a new EPOCH; every operation list
carries the epoch it was written in, and the ones written before the client saw the reset are
refused without another reset, together with their frames.

A cache belongs to one session (one RemoteRenderClient), and so do the identities in it. A client
that reconnects gets a new session, a new cache and a new mirror; a window that changes its client
does that through the reset of its owner's session, whose cache goes with it.

The entries are opaque here: `raw` is the body the client encoded, `object` whatever the consumer
decoded it into (basic2d: VertexData, LinearGradientData), which is then shared by every frame that
references it. Both classes are app-thread only. */

enum class FrameDataKind : uint8_t {
	None = 0,
	VertexSet = 1,
	Gradient = 2,
};

enum class FrameDataOp : uint8_t {
	Store = 1,
	Drop = 2,
};

// What one entry costs against the budget, on both sides: the body plus a fixed overhead, so that
// a flood of tiny sets is bounded too.
static constexpr size_t FrameDataEntryOverhead = 64;

inline size_t getFrameDataCost(size_t bodySize) { return bodySize + FrameDataEntryOverhead; }

struct SP_PUBLIC FrameDataMirrorStats {
	size_t entries = 0;
	size_t bytes = 0;
	size_t budget = 0;
	uint32_t epoch = 0;
	uint64_t stores = 0;
	uint64_t references = 0;
	uint64_t drops = 0;
	uint64_t inlined = 0;
	uint64_t resets = 0;
};

// Client side: what the server holds, as far as this client knows. One per connection.
class SP_PUBLIC FrameDataMirror : public Ref {
public:
	virtual ~FrameDataMirror() = default;

	// Budget from the server's PeerInfo; 0 disables the mirror (everything is sent inline)
	bool init(size_t budget);

	bool isEnabled() const { return _budget > 0; }
	size_t getBudget() const { return _budget; }
	uint32_t getEpoch() const { return _epoch; }

	// Starts one serialized input: the entries it references can not be evicted until the next one,
	// because the server resolves the references after applying all of its operations.
	void beginSerialization();

	// True if the server holds {id, generation}: write a reference.
	bool reference(FrameDataKind, uint64_t id, uint32_t generation);

	// Records a Store of `body` (evicting what it has to); true - write a reference, false - the body
	// does not fit the budget or would replace an entry this input already references: write it inline.
	bool store(FrameDataKind, uint64_t id, uint32_t generation, BytesView body);

	// The operations since the last call, as {e: epoch, ops: [...]}; an empty Value if the mirror is
	// disabled. The epoch is sent even without operations: it is what tells the server a reference
	// was written before a reset.
	Value takeOps();

	// The server dropped its cache (WindowCode::FrameDataReset): start over in the new epoch.
	void reset(uint32_t epoch);

	FrameDataMirrorStats getStats() const;

protected:
	struct Entry {
		FrameDataKind kind = FrameDataKind::None;
		uint32_t generation = 0;
		size_t cost = 0;
		uint64_t lastUse = 0;
	};

	void touch(uint64_t id, Entry &);
	void erase(Map<uint64_t, Entry>::iterator);

	size_t _budget = 0;
	size_t _bytes = 0;
	uint32_t _epoch = 0;
	uint64_t _serial = 0;

	Map<uint64_t, Entry> _entries;
	Set<Pair<uint64_t, uint64_t>> _lru; // {lastUse, id}
	Value _ops;

	FrameDataMirrorStats _stats;
};

struct SP_PUBLIC FrameDataCacheStats {
	size_t entries = 0;
	size_t bytes = 0;
	size_t budget = 0;
	uint32_t epoch = 0;
	uint64_t stores = 0;
	uint64_t storedBytes = 0; // the bodies of every Store
	uint64_t drops = 0;
	uint64_t hits = 0;
	uint64_t misses = 0;
	uint64_t resets = 0;
	uint64_t declined = 0;
};

// Server side: the data one session has stored.
class SP_PUBLIC FrameDataCache : public Ref {
public:
	struct Entry {
		FrameDataKind kind = FrameDataKind::None;
		uint32_t generation = 0;
		size_t cost = 0;

		// the body as the client encoded it, until the first consumer decodes it into `object` and
		// releases it
		Bytes raw;
		Rc<Ref> object;
	};

	// Entries alive in every cache of the process: what a leak check compares against zero.
	static size_t getLiveEntries();

	virtual ~FrameDataCache();

	bool init(size_t budget);

	size_t getBudget() const { return _budget; }
	uint32_t getEpoch() const { return _epoch; }

	// Applies an operation list from FrameInput. Ok (also for an empty Value), Declined - written in
	// an older epoch, nothing applied; an error - a malformed list, a Drop of an unknown entry or the
	// budget exceeded: the caller resets.
	Status apply(const Value &);

	// The entry for {kind, id, generation}, or nullptr (a miss is a protocol error for the caller).
	Entry *find(FrameDataKind, uint64_t id, uint32_t generation);

	// Drops everything and starts a new epoch, which is returned.
	uint32_t reset();

	FrameDataCacheStats getStats() const;

protected:
	void clear();

	size_t _budget = 0;
	size_t _bytes = 0;
	uint32_t _epoch = 0;
	Map<uint64_t, Entry> _entries;
	FrameDataCacheStats _stats;
};

} // namespace stappler::xenolith::core

#endif /* XENOLITH_CORE_XLCOREFRAMEDATACACHE_H_ */
