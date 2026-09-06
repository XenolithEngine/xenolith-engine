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

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class AppThread;

// Bidirectional large-binary block transfer over remote::Domain::Data. Owned by AppThread (the base),
// so the same instance serves both directions on each peer: OUTGOING transfers (we are the sender)
// are keyed by our own minted id in _outgoing; INCOMING transfers (we are the receiver) by the peer's
// id in _incoming. The two id spaces never collide because every Data message is routed by direction
// into exactly one of the maps (a Packet/Complete/Release/Unavailable looks up the map for the role
// it implies), so a sender-id 5 and a receiver-id 5 are distinct entries.
//
// Wire format (see XLRemoteProtocol.h DataCode): Announce is a CBOR request whose reply (no error)
// means "accepted"; Packet is a raw-binary notification [u64 id][u32 index][chunk] (the transport's
// LZ4 supplies compression); Complete/Release/Unavailable are small CBOR {id} notifications.
//
// All methods run on the owning AppThread and touch the connection only through its remoteSend*
// facade -- the manager never sees ServerConnection/ClientConnection directly.
class SP_PUBLIC BlockTransferManager : public Ref {
public:
	using DataType = remote::DataType;

	virtual ~BlockTransferManager() = default;

	bool init(AppThread *owner);

	// Sender entry. Packetizes + hashes `data` (copied into the transfer), sends the Announce carrying
	// `meta` (type-specific, opaque to the manager) and `reason` (which message/type triggered this).
	// On accept it streams every packet, then settles when the peer sends Complete: onComplete(id, true)
	// on success, onComplete(id, false) on decline / reply-timeout / a later Unavailable. The id matches
	// the return value, so the callback can release(id) once it is done referencing the blob. Returns the
	// new transfer id, or 0 on immediate failure.
	//
	// `priority` orders this transfer against the OTHER transfers this manager is streaming: higher
	// goes first, equal priorities take turns. It says nothing to the peer and is not on the wire --
	// the order in which a sender empties its own queue is the sender's business, and telling the
	// receiver would only invite it to have an opinion it cannot act on.
	uint64_t startTransfer(DataType, BytesView data, Value &&meta, Value &&reason,
			Function<void(uint64_t id, bool ok)> &&onComplete, int32_t priority = 0);

	// Sender: announce we will no longer reference id (Release notification) and drop our retained copy.
	//
	// Release means "I am done REFERENCING this", not "stop sending". Calling it mid-stream does stop
	// the stream, as a side effect of the record going away -- use cancelTransfer for that, which says
	// so and reports it.
	void releaseObject(uint64_t id);

	// Sender: abandon a transfer that is still streaming (Cancel notification). The receiver drops
	// whatever it has assembled; onComplete fires with false, so the caller learns the outcome instead
	// of waiting for a completion that is never coming.
	void cancelTransfer(uint64_t id);

	// Cancel every transfer still streaming; returns how many. Not the same as reset(), which is the
	// disconnect path and cannot tell the peer anything.
	size_t cancelAllTransfers();

	// Receiver: announce we can no longer hold id (Unavailable notification) and drop our retained copy.
	void markUnavailable(uint64_t id);

	// Receiver policy + delivery, set by the owning subclass. acceptPolicy decides whether to accept an
	// incoming offer; onReceived delivers the fully-assembled, hash-validated blob (just before Complete
	// is sent back). If acceptPolicy is unset every offer is accepted under the size ceiling.
	Function<bool(DataType, uint64_t size, const Value &meta, const Value &reason)> acceptPolicy;
	Function<void(uint64_t id, DataType, const Value &meta, const Value &reason, BytesView data)>
			onReceived;

	// Receiver: an accepted transfer will NOT arrive after all -- the sender cancelled it, or the
	// connection dropped mid-stream. Whoever was waiting on the blob has to be told, or it waits
	// forever; onReceived cannot carry that, because there is no data to deliver.
	Function<void(uint64_t id, DataType, const Value &meta, const Value &reason)> onCancelled;

	// Route a Domain::Data request/notification (called from the subclass dispatchMessage). The Announce
	// *reply* never arrives here -- it is consumed by AppThread::dispatchMessage's reply-by-serial path
	// into the startTransfer waiter. Always returns true (consume; never defer).
	bool dispatch(const remote::MessageHeader &, BytesView payload);

	// Drop every in-flight transfer (on disconnect).
	void reset();

protected:
	struct OutgoingTransfer {
		uint64_t id = 0;
		DataType type = DataType::Generic;
		Bytes data; // retained until Complete-then-release() (so the sender can reference it by id)
		uint32_t packetSize = 0;
		uint32_t packetCount = 0;
		uint32_t nextPacket = 0; // paced sender cursor (see pumpOutgoing)
		bool completed = false;
		// Streaming starts only once the receiver has accepted the Announce; until then the transfer
		// exists but must not be picked by the scheduler.
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

	/* Paced sender: emit a bounded batch of packets for ONE transfer, then -- if anything remains
	anywhere -- reschedule on the app looper so the peer can drain and extend its flow-control window.
	Streaming a whole blob synchronously instead exhausts the QUIC window and truncates a frame
	mid-write (corrupting the stream).
	
	ONE pump for the manager, not one per transfer. Each transfer used to reschedule itself, which
	meant there was no point at which "which packet goes next" was decided -- two transfers simply
	interleaved in whatever order the looper ran their tasks, and a priority had nowhere to apply.
	Now selectNextTransfer is that point. */
	void schedulePump();
	void pumpOutgoing();
	OutgoingTransfer *selectNextTransfer();

	// Drop an outgoing transfer and settle its caller. `ok` is what onComplete is told; the callback
	// is moved out BEFORE the erase, because it commonly owns things that reference the manager.
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

	AppThread *_owner = nullptr;
	uint64_t _nextId = 1;
	HashMap<uint64_t, OutgoingTransfer> _outgoing; // keyed by our id (we are the sender)
	HashMap<uint64_t, IncomingTransfer> _incoming; // keyed by the peer's id (we are the receiver)

	// One pump task in flight at a time; without this every accept and every batch would queue
	// another, and N transfers would get N times the looper's attention rather than sharing it.
	bool _pumpScheduled = false;
	// Last transfer served a batch, so equal priorities take turns instead of the first-found one
	// starving the rest.
	uint64_t _lastServed = 0;
};

} // namespace stappler::xenolith

#endif /* XENOLITH_APPLICATION_XLREMOTEBLOCKTRANSFER_H_ */
