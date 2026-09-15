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

#ifndef XENOLITH_REMOTE_XLREMOTESHMBLOCK_H_
#define XENOLITH_REMOTE_XLREMOTESHMBLOCK_H_

#include "XLRemoteTransport.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::remote {

// The shared-memory block of one `shm:` connection: a header and four single-producer
// single-consumer byte rings, one per direction per stream class. Both ends are on one machine, so
// the layout uses native byte order.
//
// The peer is untrusted: the server may run in the kernel and read memory a user task writes. An
// endpoint validates the layout once in attach() and keeps its own copy; every index it did not
// write itself is read once per operation and bounds-checked. A violation marks the endpoint
// corrupted instead of asserting.

constexpr uint32_t kShmBlockMagic = 0x584C'5342; // 'XLSB'
constexpr uint16_t kShmBlockVersion = 1;
constexpr uint16_t kShmBlockRingCount = 4;

constexpr uint32_t kShmRingMinCapacity = 4u * 1'024;
constexpr uint32_t kShmRingMaxCapacity = 1u << 30;

enum class ShmSide : uint8_t {
	Server = 0,
	Client = 1,
};

struct ShmBlockConfig {
	uint32_t controlCapacity = 1u << 20; // power of two
	uint32_t bulkCapacity = 16u << 20; // power of two
};

// capacity is a power of two; offset points at the ring's ShmRingHeader, the data follows it.
struct ShmRingDesc {
	uint32_t offset;
	uint32_t capacity;
};

// head and tail run free modulo 2^32: used = head - tail.
struct ShmRingHeader {
	uint32_t head; // written by the producer only
	uint32_t tail; // written by the consumer only
	uint32_t writerBlocked; // producer ran out of space; the consumer rings it after reading
	uint32_t reserved;
};

struct ShmBlockHeader {
	uint32_t magic;
	uint16_t version;
	uint16_t ringCount;
	uint32_t blockSize;
	uint32_t reserved0;

	// Indexed by getShmRingIndex(producer side, stream class).
	ShmRingDesc rings[kShmBlockRingCount];

	uint32_t closed; // one bit per side, see getShmSideBit
	uint32_t doorbell[2]; // per side: the peer increments it to wake that side
	uint32_t waiting[2]; // per side: nonzero while that side sleeps on its doorbell

	// Who the peer is, written by whoever handed the block out (the kernel on Embox).
	uint32_t identityFlags;
	int64_t peerPid;
	int64_t peerUid;
	int64_t peerGid;

	uint8_t reserved1[32];
};

static_assert(sizeof(ShmRingHeader) == 16);
static_assert(sizeof(ShmBlockHeader) == 128);

constexpr uint32_t getShmSideBit(ShmSide side) { return uint32_t(1) << uint32_t(side); }

constexpr ShmSide getShmPeerSide(ShmSide side) {
	return side == ShmSide::Server ? ShmSide::Client : ShmSide::Server;
}

// Frame shares the control ring, as streamClassForDomain folds it onto Control.
constexpr uint32_t getShmRingIndex(ShmSide producer, StreamClass c) {
	return uint32_t(producer) * 2 + (c == StreamClass::Bulk ? 1 : 0);
}

struct SP_PUBLIC ShmBlock {
	// Bytes a block with this configuration needs; 0 when the configuration is invalid.
	static size_t computeSize(const ShmBlockConfig &);

	// Lay out a fresh block in `mem`. `size` may exceed computeSize (page rounding) and becomes the
	// block size both endpoints check.
	static Status format(uint8_t *mem, size_t size, const ShmBlockConfig &);
};

// One side of a block. Not thread-safe: an endpoint is driven by one thread, its peer by another
// thread or process.
class SP_PUBLIC ShmEndpoint {
public:
	Status attach(uint8_t *mem, size_t size, ShmSide);
	void detach();

	bool isAttached() const { return _header != nullptr; }
	ShmSide getSide() const { return _side; }

	// Same contract as TransportStream::write: `written` may be 0 when the ring is full.
	Status write(StreamClass, BytesView, size_t &written);

	// Same contract as TransportStream::read: `got` == 0 means nothing yet, ask isClosed() for EOF.
	Status read(StreamClass, uint8_t *buf, size_t len, size_t &got);

	// True when an inbound ring has bytes (or indices a read will reject).
	bool hasPending() const;

	// The peer closed and everything it sent has been read, or the block is corrupted.
	bool isClosed() const;

	bool isPeerClosed() const;

	// Mark this side closed and wake the peer; its pending bytes stay readable for it.
	void close();

	bool isCorrupted() const { return _corrupted; }

	// This side's doorbell, for a wait-on-address handle.
	uint32_t *getDoorbell() const;

	// Waiting without losing a wakeup: expected = prepareWait(), then retry what is awaited (read,
	// hasPending/isClosed, or a write that found the ring full), and only then sleep on
	// (getDoorbell(), expected). finishWait() after waking.
	uint32_t prepareWait();
	void finishWait();

protected:
	struct Ring {
		ShmRingHeader *header = nullptr;
		uint8_t *data = nullptr;
		uint32_t capacity = 0;
		uint32_t local = 0; // head for an outbound ring, tail for an inbound one
	};

	static uint32_t getClassIndex(StreamClass c) { return c == StreamClass::Bulk ? 1 : 0; }

	void ringPeer();
	void markCorrupted();

	ShmBlockHeader *_header = nullptr;
	ShmSide _side = ShmSide::Server;
	bool _corrupted = false;
	bool _closed = false;
	Ring _out[2];
	Ring _in[2];
};

} // namespace stappler::xenolith::remote

#endif /* XENOLITH_REMOTE_XLREMOTESHMBLOCK_H_ */
