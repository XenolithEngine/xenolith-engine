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

#ifndef XENOLITH_REMOTE_XLREMOTECONNECTION_H_
#define XENOLITH_REMOTE_XLREMOTECONNECTION_H_

#include "XLRemoteProtocol.h"
#include "XLRemoteTransport.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::remote {

// One protocol session over a transport connection.
//
// Server and client used to be two classes with the same body: their poll() implementations were
// identical to the byte, and every send differed only in which MessageType it stamped. They are one
// class now, with the role as data -- the only thing that ever actually varied.
//
// Everything here runs on the owning AppThread. The transport underneath may be QUIC, a unix socket,
// TLS over TCP or an in-process pipe; this class does not know and must not care.
class SP_PUBLIC Connection : public Ref {
public:
	virtual ~Connection();

	bool init(Rc<TransportConnection> &&, Role);

	TransportConnection *getTransport() const { return _transport; }
	Role getRole() const { return _role; }

	// The handle for Looper::listenPollableHandle. Invalid on a transport without one, which then
	// drives poll() through setOnReadable instead.
	sprt::dispatch::NativeHandle getPollHandle() const;

	// True once the underlying connection has begun terminating (peer closed, local close, or an idle
	// timeout).
	bool isClosed();

	GlobalError ping();
	GlobalError pong(uint32_t serial);

	GlobalError sendCborMessage(Domain, uint8_t message, const Value &,
			uint32_t *outSerial = nullptr);
	GlobalError sendMessage(Domain, uint8_t message, BytesView, uint32_t *outSerial = nullptr);

	GlobalError sendCborReply(uint32_t serial, Domain, uint8_t message, const Value &);
	GlobalError sendReply(uint32_t serial, Domain, uint8_t message, BytesView);

	// `code` is domain-specific (GlobalError / WindowError / DataError / FontError), hence a raw byte.
	GlobalError sendError(Domain, uint8_t code, uint32_t failedMessageSerial);

	// Non-blocking: push whatever the send queue still holds, drain the transport into the
	// reassembler and dispatch complete messages. `cb` returns true to consume a message, false to
	// defer it (kept and retried on a later poll, so replies/events can be handled out of order by
	// serial). Driven by the host AppThread's looper.
	void poll(const Callback<bool(const MessageHeader &, BytesView)> &cb);

	// Flush what is queued, then shut the connection down gracefully. Idempotent.
	void close();

protected:
	static constexpr uint32_t kStreamClassCount = 3;

	// Everything that belongs to ONE transport stream. A reassembler holds the bytes of a single
	// ordered channel -- feeding it two streams would interleave their bytes and scramble the framing
	// -- so a stream that is really independent needs its own, and its own send queue with it.
	struct StreamState {
		TransportStream *stream = nullptr;
		MessageReader reader; // receive-side stream reassembler + deferred-message queue
		OutgoingQueue out; // send-side buffer, drained from poll()
	};

	// Frame a message into the send queue and push out what the transport takes right now; never
	// blocks (see OutgoingQueue). Every send above funnels through here.
	GlobalError enqueue(MessageType, Domain, uint8_t code, uint32_t serial, BytesView payload);

	StreamState *getStreamState(StreamClass);
	TransportStream *getStream(StreamClass) const;

	Rc<TransportConnection> _transport;
	Role _role = Role::Generic;
	Bytes _dict; // negotiated LZ4 dictionary (empty == none)

	// One serial space for the whole connection, not one per stream. A request and its reply belong to
	// the same domain and therefore ride the same stream, so nothing is gained by splitting the space
	// -- while a shared one keeps AppThread::_requests a plain map keyed by serial.
	uint32_t _serial = 1; // the handshake is always serial 0
	bool _shutdown = false;

	// Indexed by StreamClass. A slot is populated only if it is the CANONICAL owner of its transport
	// stream: a transport that folds classes together (unix, quic, and every class of a single-stream
	// transport) returns the same pointer for several classes, and then the first of them owns the
	// state and the rest alias it through _canonicalClass. So a single-stream transport ends up with
	// exactly one state and behaves precisely as it did before this existed -- no branch on caps.
	StreamState _streams[kStreamClassCount];
	uint8_t _canonicalClass[kStreamClassCount] = {0, 1, 2};
	uint8_t _distinctClasses[kStreamClassCount] = {0, 0, 0};
	uint8_t _distinctCount = 0;
};

} // namespace stappler::xenolith::remote

#endif /* XENOLITH_REMOTE_XLREMOTECONNECTION_H_ */
