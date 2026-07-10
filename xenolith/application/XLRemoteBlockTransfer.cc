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

#include "XLRemoteBlockTransfer.h"
#include "XLAppThread.h"

#include <sprt/runtime/hash.h>
#include <sprt/runtime/utils/byteorder.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith {

// Reply deadline for an Announce: the receiver answers immediately (accept/decline), so a few seconds
// is generous. On timeout the request watchdog fails the waiter with a local protocol error and the
// connection is reset -- the same fate as any other unanswered request.
static constexpr uint64_t kAnnounceReplyTimeoutUs = 5'000'000; // 5s

// 12-byte binary header of a Data packet: [u64 id][u32 index], network byte order, then the chunk.
static constexpr size_t kPacketPrefixSize = sizeof(uint64_t) + sizeof(uint32_t);

bool BlockTransferManager::init(AppThread *owner) {
	_owner = owner;
	return true;
}

uint64_t BlockTransferManager::startTransfer(DataType type, BytesView data, Value &&meta,
		Value &&reason, Function<void(uint64_t id, bool ok)> &&onComplete) {
	if (!_owner || data.size() > remote::kMaxBlockTransferSize) {
		if (onComplete) {
			onComplete(0, false);
		}
		return 0;
	}

	const uint32_t psize = remote::kRecommendedPacketSize;
	const uint32_t packetCount = data.empty() ? 0 : uint32_t((data.size() + psize - 1) / psize);

	// Per-packet xxh32, packed network-order into one bytes blob carried by the announce.
	Bytes hashes;
	hashes.resize(size_t(packetCount) * sizeof(uint32_t));
	for (uint32_t i = 0; i < packetCount; ++i) {
		size_t off = size_t(i) * psize;
		size_t len = sprt::min(size_t(psize), data.size() - off);
		uint32_t h = sprt::xxh32::hash(reinterpret_cast<const char *>(data.data() + off),
				uint32_t(len), 0);
		uint32_t hN = sprt::byteorder::HostToNetwork(h);
		__sprt_memcpy(hashes.data() + size_t(i) * sizeof(uint32_t), &hN, sizeof(uint32_t));
	}

	uint64_t id = _nextId++;

	OutgoingTransfer t;
	t.id = id;
	t.type = type;
	t.data = data.bytes<Interface>();
	t.packetSize = psize;
	t.packetCount = packetCount;
	t.onComplete = sp::move(onComplete);
	_outgoing.emplace(id, sp::move(t));

	Value announce;
	announce.setInteger(int64_t(id), "id");
	announce.setInteger(int64_t(toInt(type)), "type");
	announce.setInteger(int64_t(data.size()), "size");
	announce.setInteger(int64_t(packetCount), "packets");
	announce.setInteger(int64_t(psize), "psize");
	announce.setBytes(BytesView(hashes.data(), hashes.size()), "hashes");
	announce.setValue(sp::move(reason), "reason");
	announce.setValue(sp::move(meta), "meta");

	auto failOut = [this](uint64_t tid) {
		auto it = _outgoing.find(tid);
		if (it == _outgoing.end()) {
			return;
		}
		auto cb = sp::move(it->second.onComplete);
		_outgoing.erase(it);
		if (cb) {
			cb(tid, false);
		}
	};

	if (!_owner->remoteSendCborWithReply(remote::Domain::Data, toInt(remote::DataCode::Announce),
				announce, [this, id](const remote::MessageHeader &h, BytesView) {
		auto it = _outgoing.find(id);
		if (it == _outgoing.end()) {
			return; // released / reset while the announce was in flight
		}
		if (remote::isError(h)) {
			log::source().warn("BlockTransfer", "transfer ", id, " declined (code ",
					uint32_t(h.code), ")");
			auto cb = sp::move(it->second.onComplete);
			_outgoing.erase(it);
			if (cb) {
				cb(id, false);
			}
			return;
		}
		// Accepted (mirror reply, no error): start the paced packet stream.
		it->second.nextPacket = 0;
		pumpOutgoing(id);
	}, kAnnounceReplyTimeoutUs)) {
		failOut(id);
		return 0;
	}

	log::source().info("BlockTransfer", "announced transfer ", id, " (type ", uint32_t(toInt(type)),
			", ", data.size(), " bytes, ", packetCount, " packets)");
	return id;
}

// Number of packets to emit per scheduler tick before yielding. Small enough that a batch fits
// comfortably inside the QUIC flow-control window even on the first tick, so a frame never has to wait
// long enough for streamWriteAll's deadline to truncate it.
static constexpr uint32_t kPacketBatch = 8;

void BlockTransferManager::pumpOutgoing(uint64_t id) {
	auto it = _outgoing.find(id);
	if (it == _outgoing.end()) {
		return; // released / reset while paced send was in flight
	}
	auto &t = it->second;

	uint32_t emitted = 0;
	while (t.nextPacket < t.packetCount && emitted < kPacketBatch) {
		uint32_t i = t.nextPacket;
		size_t off = size_t(i) * t.packetSize;
		size_t len = sprt::min(size_t(t.packetSize), t.data.size() - off);

		Bytes packet;
		packet.resize(kPacketPrefixSize + len);
		uint64_t idN = sprt::byteorder::HostToNetwork(t.id);
		uint32_t ixN = sprt::byteorder::HostToNetwork(i);
		__sprt_memcpy(packet.data(), &idN, sizeof(uint64_t));
		__sprt_memcpy(packet.data() + sizeof(uint64_t), &ixN, sizeof(uint32_t));
		__sprt_memcpy(packet.data() + kPacketPrefixSize, t.data.data() + off, len);

		if (!_owner->remoteSendRaw(remote::Domain::Data, toInt(remote::DataCode::Packet),
					BytesView(packet.data(), packet.size()), nullptr)) {
			// Backpressure: leave nextPacket on this packet and retry it on the next tick once the
			// peer has drained and extended its flow-control window.
			break;
		}
		++t.nextPacket;
		++emitted;
	}

	if (t.nextPacket < t.packetCount) {
		// More to send: yield to the looper so I/O is serviced (the peer drains, MAX_DATA/MAX_STREAM_DATA
		// arrive) before the next batch. Keep the manager alive across the deferral.
		auto self = Rc<BlockTransferManager>(this);
		_owner->performOnAppThread([self, id] { self->pumpOutgoing(id); }, _owner, true);
	} else {
		log::source().info("BlockTransfer", "streamed all ", t.packetCount,
				" packet(s) for transfer ", t.id);
	}
}

void BlockTransferManager::deliverIncoming(IncomingTransfer &t) {
	log::source().info("BlockTransfer", "transfer ", t.id, " fully received (", t.packetCount,
			" packets, ", t.size, " bytes); delivering");
	if (onReceived) {
		onReceived(t.id, t.type, t.meta, t.reason, BytesView(t.buffer.data(), t.buffer.size()));
	}
	// Tell the sender we hold the complete data. We retain t.buffer (referenceable by id) until the
	// sender sends Release or we evict it (markUnavailable).
	Value done;
	done.setInteger(int64_t(t.id), "id");
	_owner->remoteSendCbor(remote::Domain::Data, toInt(remote::DataCode::Complete), done, nullptr);
}

bool BlockTransferManager::handleAnnounce(const remote::MessageHeader &h, BytesView payload) {
	auto val = data::read<Interface>(payload);
	uint64_t id = uint64_t(val.getInteger("id"));
	auto type = DataType(val.getInteger("type"));
	uint64_t size = uint64_t(val.getInteger("size"));
	uint32_t packets = uint32_t(val.getInteger("packets"));
	uint32_t psize = uint32_t(val.getInteger("psize"));
	const auto &hashesBytes = val.getBytes("hashes");

	auto fail = [&](remote::DataError e) -> bool {
		log::source().warn("BlockTransfer", "rejecting incoming transfer ", id, " (error ",
				uint32_t(toInt(e)), ")");
		if (_owner) {
			_owner->remoteSendError(remote::Domain::Data, toInt(e), h.serial);
		}
		return true;
	};

	// Validate consistency. The psize <= kRecommendedPacketSize bound is the real decompression-bomb
	// guard for this domain: it caps every subsequent packet frame's decompressed size to a small,
	// known value, so the transport never needs a ratio heuristic that would reject a strongly
	// compressed (e.g. flat-colour screenshot) packet.
	if (size > remote::kMaxBlockTransferSize) {
		return fail(remote::DataError::TooLarge);
	}
	if (psize == 0 || psize > remote::kRecommendedPacketSize) {
		return fail(remote::DataError::BadHeader);
	}
	uint32_t expectedPackets = size == 0 ? 0 : uint32_t((size + psize - 1) / psize);
	if (packets != expectedPackets || hashesBytes.size() != size_t(packets) * sizeof(uint32_t)) {
		return fail(remote::DataError::BadHeader);
	}

	Value meta = val.getValue("meta");
	Value reason = val.getValue("reason");

	if (acceptPolicy && !acceptPolicy(type, size, meta, reason)) {
		return fail(remote::DataError::Declined);
	}

	IncomingTransfer t;
	t.id = id;
	t.type = type;
	t.size = size;
	t.packetSize = psize;
	t.packetCount = packets;
	t.hashes.resize(packets);
	for (uint32_t i = 0; i < packets; ++i) {
		uint32_t hN = 0;
		__sprt_memcpy(&hN, hashesBytes.data() + size_t(i) * sizeof(uint32_t), sizeof(uint32_t));
		t.hashes[i] = sprt::byteorder::NetworkToHost(hN);
	}
	t.received.resize(packets, false);
	t.buffer.resize(size);
	t.meta = sp::move(meta);
	t.reason = sp::move(reason);

	auto res = _incoming.insert_or_assign(id, sp::move(t));

	// Accept == mirror reply without error (empty payload).
	if (_owner) {
		_owner->remoteSendCborReply(h.serial, remote::Domain::Data,
				toInt(remote::DataCode::Announce), Value());
	}
	log::source().info("BlockTransfer", "accepted incoming transfer ", id, " (type ",
			uint32_t(toInt(type)), ", ", packets, " packets, ", size, " bytes)");

	// Degenerate empty transfer: there are no packets to wait for.
	if (packets == 0) {
		deliverIncoming(res.first->second);
	}
	return true;
}

bool BlockTransferManager::handlePacket(const remote::MessageHeader &, BytesView payload) {
	if (payload.size() < kPacketPrefixSize) {
		return true; // malformed; drop
	}
	BytesViewNetwork r(payload.data(), payload.size());
	uint64_t id = r.readUnsigned64();
	uint32_t index = r.readUnsigned32();
	BytesView chunk(r.data(), r.size());

	auto it = _incoming.find(id);
	if (it == _incoming.end()) {
		return true; // unknown/already-released transfer
	}
	auto &t = it->second;
	if (index >= t.packetCount) {
		log::source().warn("BlockTransfer", "packet index ", index, " out of range for transfer ",
				id);
		return true;
	}
	size_t off = size_t(index) * t.packetSize;
	size_t expected = sprt::min(size_t(t.packetSize), size_t(t.size - off));
	if (chunk.size() != expected) {
		log::source().warn("BlockTransfer", "packet ", index, " size ", chunk.size(),
				" != ", expected, " for transfer ", id, "; aborting");
		markUnavailable(id);
		return true;
	}
	uint32_t hash = sprt::xxh32::hash(reinterpret_cast<const char *>(chunk.data()),
			uint32_t(chunk.size()), 0);
	if (hash != t.hashes[index]) {
		log::source().warn("BlockTransfer", "packet ", index, " hash mismatch for transfer ", id,
				"; aborting");
		markUnavailable(id);
		return true;
	}
	if (!t.received[index]) {
		__sprt_memcpy(t.buffer.data() + off, chunk.data(), chunk.size());
		t.received[index] = true;
		++t.receivedCount;
	}
	if (t.receivedCount == t.packetCount) {
		deliverIncoming(t);
	}
	return true;
}

bool BlockTransferManager::handleComplete(const remote::MessageHeader &, BytesView payload) {
	auto val = data::read<Interface>(payload);
	uint64_t id = uint64_t(val.getInteger("id"));
	auto it = _outgoing.find(id);
	if (it == _outgoing.end()) {
		return true;
	}
	it->second.completed = true;
	log::source().info("BlockTransfer", "transfer ", id, " acknowledged complete by receiver");
	auto cb = sp::move(it->second.onComplete);
	it->second.onComplete = nullptr;
	if (cb) {
		cb(id, true);
	}
	// Keep the data so the sender can still reference it by id until it calls release().
	return true;
}

bool BlockTransferManager::handleRelease(const remote::MessageHeader &, BytesView payload) {
	auto val = data::read<Interface>(payload);
	uint64_t id = uint64_t(val.getInteger("id"));
	if (_incoming.erase(id)) {
		log::source().info("BlockTransfer", "released incoming transfer ", id);
	}
	return true;
}

bool BlockTransferManager::handleUnavailable(const remote::MessageHeader &, BytesView payload) {
	auto val = data::read<Interface>(payload);
	uint64_t id = uint64_t(val.getInteger("id"));
	auto it = _outgoing.find(id);
	if (it == _outgoing.end()) {
		return true;
	}
	log::source().info("BlockTransfer", "receiver dropped transfer ", id, " (unavailable)");
	bool wasPending = !it->second.completed;
	auto cb = sp::move(it->second.onComplete);
	_outgoing.erase(it);
	if (wasPending && cb) {
		cb(id, false);
	}
	return true;
}

void BlockTransferManager::releaseObject(uint64_t id) {
	if (!_outgoing.erase(id)) {
		return;
	}
	if (_owner) {
		Value v;
		v.setInteger(int64_t(id), "id");
		_owner->remoteSendCbor(remote::Domain::Data, toInt(remote::DataCode::Release), v, nullptr);
	}
	log::source().info("BlockTransfer", "released outgoing transfer ", id);
}

void BlockTransferManager::markUnavailable(uint64_t id) {
	_incoming.erase(id);
	if (_owner) {
		Value v;
		v.setInteger(int64_t(id), "id");
		_owner->remoteSendCbor(remote::Domain::Data, toInt(remote::DataCode::Unavailable), v,
				nullptr);
	}
	log::source().info("BlockTransfer", "marked incoming transfer ", id, " unavailable");
}

bool BlockTransferManager::dispatch(const remote::MessageHeader &h, BytesView payload) {
	switch (remote::DataCode(h.code)) {
	case remote::DataCode::Announce: return handleAnnounce(h, payload);
	case remote::DataCode::Packet: return handlePacket(h, payload);
	case remote::DataCode::Complete: return handleComplete(h, payload);
	case remote::DataCode::Release: return handleRelease(h, payload);
	case remote::DataCode::Unavailable: return handleUnavailable(h, payload);
	default:
		log::source().warn("BlockTransfer", "unhandled data message (code ", uint32_t(h.code), ")");
		return true; // consume unknown control messages (don't defer indefinitely)
	}
}

void BlockTransferManager::reset() {
	_outgoing.clear();
	_incoming.clear();
	// _nextId stays monotonic across reconnects so transfer ids remain unambiguous in logs.
}

} // namespace stappler::xenolith
