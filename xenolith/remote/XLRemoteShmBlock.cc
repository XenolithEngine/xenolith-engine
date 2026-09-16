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

#include "XLRemoteShmBlock.h"

#include <sprt/cxx/__atomic/ops.h>
#include <sprt/c/sys/__sprt_sprt.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::remote {

namespace {

static constexpr uint64_t kShmRingAlignment = 64;

static uint64_t shmAlignUp(uint64_t value, uint64_t align) {
	return (value + align - 1) & ~(align - 1);
}

static bool shmIsValidCapacity(uint32_t capacity) {
	return capacity >= kShmRingMinCapacity && capacity <= kShmRingMaxCapacity
			&& (capacity & (capacity - 1)) == 0;
}

// Ring capacities in index order (getShmRingIndex).
static void shmFillCapacities(const ShmBlockConfig &cfg, uint32_t caps[kShmBlockRingCount]) {
	for (uint32_t side = 0; side < 2; ++side) {
		caps[getShmRingIndex(ShmSide(side), StreamClass::Control)] = cfg.controlCapacity;
		caps[getShmRingIndex(ShmSide(side), StreamClass::Bulk)] = cfg.bulkCapacity;
	}
}

} // namespace

size_t ShmBlock::computeSize(const ShmBlockConfig &cfg) {
	if (!shmIsValidCapacity(cfg.controlCapacity) || !shmIsValidCapacity(cfg.bulkCapacity)) {
		return 0;
	}

	uint32_t caps[kShmBlockRingCount];
	shmFillCapacities(cfg, caps);

	uint64_t offset = shmAlignUp(sizeof(ShmBlockHeader), kShmRingAlignment);
	for (auto cap : caps) {
		offset = shmAlignUp(offset + sizeof(ShmRingHeader) + cap, kShmRingAlignment);
	}
	if (offset > maxOf<uint32_t>()) {
		return 0;
	}
	return size_t(offset);
}

Status ShmBlock::format(uint8_t *mem, size_t size, const ShmBlockConfig &cfg) {
	auto required = computeSize(cfg);
	if (!mem || required == 0 || size < required || size > maxOf<uint32_t>()
			|| (uintptr_t(mem) % alignof(ShmBlockHeader)) != 0) {
		return Status::ErrorInvalidArguemnt;
	}

	uint32_t caps[kShmBlockRingCount];
	shmFillCapacities(cfg, caps);

	auto header = new (mem) ShmBlockHeader();
	header->magic = kShmBlockMagic;
	header->version = kShmBlockVersion;
	header->ringCount = kShmBlockRingCount;
	header->blockSize = uint32_t(size);

	uint64_t offset = shmAlignUp(sizeof(ShmBlockHeader), kShmRingAlignment);
	for (uint32_t i = 0; i < kShmBlockRingCount; ++i) {
		header->rings[i].offset = uint32_t(offset);
		header->rings[i].capacity = caps[i];
		new (mem + offset) ShmRingHeader();
		offset = shmAlignUp(offset + sizeof(ShmRingHeader) + caps[i], kShmRingAlignment);
	}
	return Status::Ok;
}

Status ShmEndpoint::attach(uint8_t *mem, size_t size, ShmSide side) {
	detach();

	if (!mem || size < sizeof(ShmBlockHeader) || size > maxOf<uint32_t>()
			|| (uintptr_t(mem) % alignof(ShmBlockHeader)) != 0) {
		return Status::ErrorInvalidArguemnt;
	}

	// Validate a private copy: the peer may be rewriting the shared header while we look at it.
	ShmBlockHeader desc;
	__sprt_memcpy(&desc, mem, sizeof(ShmBlockHeader));

	if (desc.magic != kShmBlockMagic || desc.version != kShmBlockVersion
			|| desc.ringCount != kShmBlockRingCount || desc.blockSize != size) {
		return Status::ErrorInvalidArguemnt;
	}

	uint64_t rangeStart[kShmBlockRingCount];
	uint64_t rangeEnd[kShmBlockRingCount];
	for (uint32_t i = 0; i < kShmBlockRingCount; ++i) {
		auto &ring = desc.rings[i];
		if (!shmIsValidCapacity(ring.capacity) || ring.offset < sizeof(ShmBlockHeader)
				|| (ring.offset % alignof(ShmRingHeader)) != 0) {
			return Status::ErrorInvalidArguemnt;
		}
		rangeStart[i] = ring.offset;
		rangeEnd[i] = uint64_t(ring.offset) + sizeof(ShmRingHeader) + ring.capacity;
		if (rangeEnd[i] > size) {
			return Status::ErrorInvalidArguemnt;
		}
		for (uint32_t j = 0; j < i; ++j) {
			if (rangeStart[i] < rangeEnd[j] && rangeStart[j] < rangeEnd[i]) {
				return Status::ErrorInvalidArguemnt;
			}
		}
	}

	auto peer = getShmPeerSide(side);
	static constexpr StreamClass s_classes[] = {StreamClass::Control, StreamClass::Bulk};
	for (auto c : s_classes) {
		auto idx = getClassIndex(c);
		auto &outDesc = desc.rings[getShmRingIndex(side, c)];
		auto &inDesc = desc.rings[getShmRingIndex(peer, c)];

		auto &out = _out[idx];
		out.header = reinterpret_cast<ShmRingHeader *>(mem + outDesc.offset);
		out.data = mem + outDesc.offset + sizeof(ShmRingHeader);
		out.capacity = outDesc.capacity;
		out.local = sprt::_atomic::loadSeq(&out.header->head);

		auto &in = _in[idx];
		in.header = reinterpret_cast<ShmRingHeader *>(mem + inDesc.offset);
		in.data = mem + inDesc.offset + sizeof(ShmRingHeader);
		in.capacity = inDesc.capacity;
		in.local = sprt::_atomic::loadSeq(&in.header->tail);
	}

	_header = reinterpret_cast<ShmBlockHeader *>(mem);
	_side = side;
	_corrupted = false;
	_closed = false;

	for (auto &ring : _out) {
		if (ring.local - sprt::_atomic::loadSeq(&ring.header->tail) > ring.capacity) {
			markCorrupted();
		}
	}
	for (auto &ring : _in) {
		if (sprt::_atomic::loadSeq(&ring.header->head) - ring.local > ring.capacity) {
			markCorrupted();
		}
	}
	return _corrupted ? Status::ErrorInvalidArguemnt : Status::Ok;
}

void ShmEndpoint::detach() {
	_header = nullptr;
	_corrupted = false;
	_closed = false;
	for (auto &ring : _out) { ring = Ring(); }
	for (auto &ring : _in) { ring = Ring(); }
}

Status ShmEndpoint::write(StreamClass c, BytesView data, size_t &written) {
	written = 0;
	if (!_header || _corrupted) {
		return Status::ErrorInvalidArguemnt;
	}
	if (_closed || isPeerClosed()) {
		return Status::ErrorNotPermitted;
	}
	if (data.empty()) {
		return Status::Ok;
	}

	auto &ring = _out[getClassIndex(c)];
	auto used = ring.local - sprt::_atomic::loadSeq(&ring.header->tail);
	if (used > ring.capacity) {
		markCorrupted();
		return Status::ErrorInvalidArguemnt;
	}

	auto free = ring.capacity - used;
	if (free < data.size()) {
		// Raise the flag before re-reading the tail, so a read that frees space in between rings us.
		sprt::_atomic::storeSeq(&ring.header->writerBlocked, uint32_t(1));
		used = ring.local - sprt::_atomic::loadSeq(&ring.header->tail);
		if (used > ring.capacity) {
			markCorrupted();
			return Status::ErrorInvalidArguemnt;
		}
		free = ring.capacity - used;
	}

	auto n = uint32_t(sprt::min(size_t(free), data.size()));
	if (n == 0) {
		return Status::Ok;
	}

	auto mask = ring.capacity - 1;
	auto pos = ring.local & mask;
	auto first = sprt::min(n, ring.capacity - pos);
	__sprt_memcpy(ring.data + pos, data.data(), first);
	if (n > first) {
		__sprt_memcpy(ring.data, data.data() + first, n - first);
	}

	ring.local += n;
	sprt::_atomic::storeSeq(&ring.header->head, ring.local);
	written = n;

	ringPeer();
	return Status::Ok;
}

Status ShmEndpoint::read(StreamClass c, uint8_t *buf, size_t len, size_t &got) {
	got = 0;
	if (!_header || _corrupted) {
		return Status::ErrorInvalidArguemnt;
	}

	// The close bit is read before the head: bytes sent before a close are always seen.
	auto peerClosed = isPeerClosed();

	auto &ring = _in[getClassIndex(c)];
	auto avail = sprt::_atomic::loadSeq(&ring.header->head) - ring.local;
	if (avail > ring.capacity) {
		markCorrupted();
		return Status::ErrorInvalidArguemnt;
	}

	auto n = uint32_t(sprt::min(size_t(avail), len));
	if (n == 0) {
		return (avail == 0 && peerClosed) ? Status::ErrorNotPermitted : Status::Ok;
	}

	auto mask = ring.capacity - 1;
	auto pos = ring.local & mask;
	auto first = sprt::min(n, ring.capacity - pos);
	__sprt_memcpy(buf, ring.data + pos, first);
	if (n > first) {
		__sprt_memcpy(buf + first, ring.data, n - first);
	}

	ring.local += n;
	sprt::_atomic::storeSeq(&ring.header->tail, ring.local);
	got = n;

	if (sprt::_atomic::exchange(&ring.header->writerBlocked, uint32_t(0)) != 0) {
		ringPeer();
	}
	return Status::Ok;
}

bool ShmEndpoint::hasPending() const {
	if (!_header) {
		return false;
	}
	for (auto &ring : _in) {
		if (sprt::_atomic::loadSeq(&ring.header->head) != ring.local) {
			return true;
		}
	}
	return false;
}

bool ShmEndpoint::isClosed() const {
	if (!_header || _corrupted) {
		return true;
	}
	return isPeerClosed() && !hasPending();
}

bool ShmEndpoint::isPeerClosed() const {
	if (!_header) {
		return true;
	}
	return (sprt::_atomic::loadSeq(&_header->closed) & getShmSideBit(getShmPeerSide(_side))) != 0;
}

void ShmEndpoint::close() {
	if (!_header || _closed) {
		return;
	}
	_closed = true;
	sprt::_atomic::fetchOr(&_header->closed, getShmSideBit(_side));
	ringPeer();
}

uint32_t *ShmEndpoint::getDoorbell() const {
	return _header ? &_header->doorbell[uint32_t(_side)] : nullptr;
}

uint32_t ShmEndpoint::prepareWait() {
	if (!_header) {
		return 0;
	}
	sprt::_atomic::storeSeq(&_header->waiting[uint32_t(_side)], uint32_t(1));
	return sprt::_atomic::loadSeq(&_header->doorbell[uint32_t(_side)]);
}

void ShmEndpoint::finishWait() {
	if (_header) {
		sprt::_atomic::storeSeq(&_header->waiting[uint32_t(_side)], uint32_t(0));
	}
}

void ShmEndpoint::ringPeer() {
	auto peer = uint32_t(getShmPeerSide(_side));
	sprt::_atomic::fetchAdd(&_header->doorbell[peer], uint32_t(1));
	if (sprt::_atomic::loadSeq(&_header->waiting[peer]) != 0) {
		__sprt_sprt_qlock_wake_all(&_header->doorbell[peer], __SPRT_SPRT_LOCK_FLAG_SHARED);
	}
}

void ShmEndpoint::markCorrupted() {
	if (!_corrupted) {
		_corrupted = true;
		log::source().error("remote::shm", "shared block indices are out of range; closing");
	}
}

} // namespace stappler::xenolith::remote
