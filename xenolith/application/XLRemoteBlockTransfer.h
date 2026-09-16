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

#ifndef XENOLITH_APPLICATION_XLREMOTEBLOCKTRANSFER_H_
#define XENOLITH_APPLICATION_XLREMOTEBLOCKTRANSFER_H_

#include "XLCommon.h"
#include "XLRemoteProtocol.h"
#include "XLRemotePeer.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

// Bidirectional large-binary block transfer over remote::Domain::Data, one per connection (owned by
// the client AppThread, or by a server RemoteSession). Outgoing
// transfers are keyed by our id in _outgoing, incoming ones by the peer's id in _incoming; each
// message is routed by the role it implies, so the id spaces never collide.
//
// Wire format (see XLRemoteProtocol.h DataCode): Announce is a CBOR request whose non-error reply
// means accepted; Packet is a raw notification [u64 id][u32 index][chunk] (LZ4 from the transport);
// Complete/Release/Unavailable are small CBOR {id} notifications.
//
// All methods run on the peer thread and send only through the RemotePeer.
class SP_PUBLIC BlockTransferManager : public Ref {
public:
	using DataType = remote::DataType;

	virtual ~BlockTransferManager() = default;

	bool init(RemotePeer *owner);

	// Sender entry. Packetizes and hashes a copy of `data`, sends the Announce with `meta` (opaque,
	// type-specific) and `reason` (the triggering message). On accept it streams all packets, then
	// calls onComplete(id, true) when the peer sends Complete, or onComplete(id, false) on decline,
	// reply timeout or a later Unavailable; the callback may release(id). Returns the transfer id,
	// or 0 on immediate failure.
	//
	// `priority` orders this transfer against others from this manager (higher first, equal ones
	// take turns); it is local and not sent on the wire.
	uint64_t startTransfer(DataType, BytesView data, Value &&meta, Value &&reason,
			Function<void(uint64_t id, bool ok)> &&onComplete, int32_t priority = 0);

	// Sender: announce we no longer reference id (Release) and drop our copy. Mid-stream this also
	// stops streaming; use cancelTransfer to abandon a transfer explicitly.
	void releaseObject(uint64_t id);

	// Sender: abandon a transfer still streaming (Cancel). The receiver drops what it assembled;
	// onComplete fires with false.
	void cancelTransfer(uint64_t id);

	// Cancel every transfer still streaming; returns how many. Unlike reset() (disconnect), this
	// notifies the peer.
	size_t cancelAllTransfers();

	// Receiver: announce we can no longer hold id (Unavailable) and drop our retained copy.
	void markUnavailable(uint64_t id);

	// Receiver policy and delivery, set by the owning subclass. acceptPolicy decides whether to
	// accept an offer (unset: everything under the size ceiling); onReceived delivers the
	// assembled, hash-validated blob just before Complete is sent.
	Function<bool(DataType, uint64_t size, const Value &meta, const Value &reason)> acceptPolicy;
	Function<void(uint64_t id, DataType, const Value &meta, const Value &reason, BytesView data)>
			onReceived;

	// Receiver: an accepted transfer will not arrive (sender cancelled, or the connection dropped
	// mid-stream).
	Function<void(uint64_t id, DataType, const Value &meta, const Value &reason)> onCancelled;

	// Route a Domain::Data request/notification (from the subclass dispatchMessage). The Announce
	// reply arrives through AppThread's reply-by-serial path instead. Always returns true.
	bool dispatch(const remote::MessageHeader &, BytesView payload);

	// Drop every in-flight transfer (on disconnect).
	void reset();

	// reset() and forget the peer, which is going away; a pump still scheduled then does nothing.
	void invalidate();

protected:
	struct OutgoingTransfer {
		uint64_t id = 0;
		DataType type = DataType::Generic;
		Bytes data; // retained until Complete-then-release() (so the sender can reference it by id)
		uint32_t packetSize = 0;
		uint32_t packetCount = 0;
		uint32_t nextPacket = 0; // paced sender cursor (see pumpOutgoing)
		bool completed = false;
		// Streaming starts only after the receiver accepts the Announce.
		bool accepted = false;
		int32_t priority = 0;
		Function<void(uint64_t, bool)> onComplete;
	};

	struct IncomingTransfer {
		uint64_t id = 0;
		DataType type = DataType::Generic;
		uint64_t size = 0;
		uint32_t packetSize = 0;
		uint32_t packetCount = 0;
		uint32_t receivedCount = 0;
		Vector<uint32_t> hashes; // per-packet xxh32 from the announce
		Vector<bool> received;
		Bytes buffer; // pre-sized to `size`; chunks land at index*packetSize
		Value meta;
		Value reason;
	};

	/* Paced sender: emit a bounded batch for one transfer (chosen by selectNextTransfer), then
	reschedule on the app looper if anything remains, so the peer can extend its flow-control
	window. Streaming a whole blob synchronously exhausts the QUIC window and corrupts the
	stream. One pump serves the whole manager. */
	void schedulePump();
	void pumpOutgoing();
	OutgoingTransfer *selectNextTransfer();

	// Drop an outgoing transfer and settle its caller with `ok`; the callback is moved out before
	// the erase, since it often owns things that reference the manager.
	void finishOutgoing(uint64_t id, bool ok);

	void deliverIncoming(IncomingTransfer &);

	// Drop an incoming transfer that will never complete and tell whoever was waiting.
	void abandonIncoming(uint64_t id);

	bool handleAnnounce(const remote::MessageHeader &, BytesView payload);
	bool handlePacket(const remote::MessageHeader &, BytesView payload);
	bool handleComplete(const remote::MessageHeader &, BytesView payload);
	bool handleRelease(const remote::MessageHeader &, BytesView payload);
	bool handleUnavailable(const remote::MessageHeader &, BytesView payload);
	bool handleCancel(const remote::MessageHeader &, BytesView payload);

	RemotePeer *_owner = nullptr;
	uint64_t _nextId = 1;
	HashMap<uint64_t, OutgoingTransfer> _outgoing; // keyed by our id (we are the sender)
	HashMap<uint64_t, IncomingTransfer> _incoming; // keyed by the peer's id (we are the receiver)

	// At most one pump task in flight, so transfers share the looper's attention.
	bool _pumpScheduled = false;
	// Last transfer served, so equal priorities take turns.
	uint64_t _lastServed = 0;
};

} // namespace stappler::xenolith

#endif /* XENOLITH_APPLICATION_XLREMOTEBLOCKTRANSFER_H_ */
