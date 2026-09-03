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
	// Frame a message into the send queue and push out what the transport takes right now; never
	// blocks (see OutgoingQueue). Every send above funnels through here.
	GlobalError enqueue(MessageType, Domain, uint8_t code, uint32_t serial, BytesView payload);

	TransportStream *getStream() const;

	Rc<TransportConnection> _transport;
	Role _role = Role::Generic;
	Bytes _dict; // negotiated LZ4 dictionary (empty == none)
	uint32_t _serial = 1; // the handshake is always serial 0
	bool _shutdown = false;
	MessageReader _reader; // receive-side stream reassembler + deferred-message queue
	OutgoingQueue _out; // send-side buffer, drained from poll()
};

} // namespace stappler::xenolith::remote

#endif /* XENOLITH_REMOTE_XLREMOTECONNECTION_H_ */
