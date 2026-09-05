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

#include "XLRemoteConnection.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::remote {

__SPRT_PUSH_ALLOW_CXXABI_ALLOC

Connection::~Connection() { }

__SPRT_POP_ALLOW_CXXABI_ALLOC

bool Connection::init(Rc<TransportConnection> &&conn, Role role) {
	_transport = sp::move(conn);
	_role = role;
	return _transport != nullptr;
}

TransportStream *Connection::getStream() const {
	// Every message class rides one stream for now. StreamClass exists so that mapping can change on
	// a MultiStream transport without touching a single caller.
	return _transport ? _transport->getStream(StreamClass::Control) : nullptr;
}

sprt::dispatch::NativeHandle Connection::getPollHandle() const {
	return _transport ? _transport->getPollHandle() : sprt::dispatch::NativeHandle(-1);
}

bool Connection::isClosed() { return !_transport || _transport->isClosed(); }

GlobalError Connection::enqueue(MessageType t, Domain d, uint8_t code, uint32_t serial,
		BytesView payload) {
	if (!_transport) {
		return GlobalError::NetworkBackend;
	}
	if (!_out.push(BytesView(_dict.data(), _dict.size()), t, d, code, serial, payload)) {
		log::source().error("remote::Connection", "send queue overflow (", _out.pending(),
				" bytes unsent); the peer is not draining");
		return GlobalError::NetworkBackend;
	}
	return _out.flush(getStream()) ? GlobalError::Ok : GlobalError::NetworkBackend;
}

GlobalError Connection::ping() {
	return enqueue(MessageTypeRequest(_role), Domain::Global, toInt(GlobalCode::Ping), _serial++,
			BytesView());
}

GlobalError Connection::pong(uint32_t serial) {
	return enqueue(MessageTypeReply(_role), Domain::Global, toInt(GlobalCode::Pong), serial,
			BytesView());
}

GlobalError Connection::sendCborMessage(Domain d, uint8_t message, const Value &val,
		uint32_t *outSerial) {
	Bytes bytes = data::write<Interface>(val, data::EncodeFormat::Cbor);
	return sendMessage(d, message, bytes, outSerial);
}

GlobalError Connection::sendMessage(Domain d, uint8_t message, BytesView payload,
		uint32_t *outSerial) {
	auto serial = _serial++;
	auto st = enqueue(MessageTypeRequest(_role), d, message, serial, payload);
	if (st == GlobalError::Ok && outSerial) {
		*outSerial = serial;
	}
	return st;
}

GlobalError Connection::sendCborReply(uint32_t serial, Domain d, uint8_t message,
		const Value &val) {
	Bytes bytes = data::write<Interface>(val, data::EncodeFormat::Cbor);
	return sendReply(serial, d, message, bytes);
}

GlobalError Connection::sendReply(uint32_t serial, Domain d, uint8_t message, BytesView payload) {
	return enqueue(MessageTypeReply(_role), d, message, serial, payload);
}

GlobalError Connection::sendError(Domain d, uint8_t code, uint32_t failedMessageSerial) {
	return enqueue(MessageTypeError(_role), d, code, failedMessageSerial, BytesView());
}

void Connection::poll(const Callback<bool(const MessageHeader &, BytesView)> &dispatchCb) {
	auto stream = getStream();
	if (!stream) {
		return;
	}

	// Drain whatever the send queue still holds from earlier enqueues that hit backpressure. This is
	// the only place a stalled write makes progress, which is why the looper drives poll() on
	// readiness AND on every update tick.
	if (!_out.flush(stream)) {
		log::source().error("remote::Connection", "send failed; dropping connection");
		close();
		return;
	}

	// Service the transport (datagrams, retransmit timers), then drain it into the reassembler. The
	// reader holds any partial frame across pumps, so a message spanning two reads is handled
	// correctly; complete frames are then dispatched (deferred ones stay queued).
	_transport->handleEvents();

	uint8_t buf[4'096];
	for (;;) {
		size_t n = 0;
		if (stream->read(buf, sizeof(buf), n) != Status::Ok || n == 0) {
			break; // drained or closed
		}

		BytesViewNetwork nw(buf, n);

		// Fast path: only when the reassembler is fully idle -- no buffered partial AND nothing queued.
		// Then this chunk begins on a frame boundary and can be parsed (and dispatched inline) straight
		// out of it, no copy into _buffer. Requiring an empty buffer avoids desync: with a buffered
		// partial the fresh bytes are that frame's continuation, and parsing them as a new header would
		// scramble the stream. Requiring an empty pending queue preserves order: queued frames dispatch
		// at end-of-poll, so fast-path-dispatching newer frames ahead of them would reorder the stream
		// (e.g. a FrameInput after its FrameCommit). When either holds, fall through to append().
		if (!_reader.hasPartialMessage() && !_reader.hasPending()) {
			while (readMessagePayload(nw, _dict, [&](const MessageHeader &h, BytesView data) {
				if (!dispatchCb(h, data)) {
					_reader.addMessage(h, data);
				}
			})) { }
		}

		// Buffer the remainder: a trailing partial frame, or the whole chunk when a partial was already
		// buffered. The reassembler completes it on a later read.
		if (!nw.empty()) {
			if (!_reader.append(BytesView(nw.data(), nw.size()),
						BytesView(_dict.data(), _dict.size()))) {
				log::source().error("remote::Connection", "framing violation; dropping connection");
				close();
				return;
			}
		}
	}

	_reader.dispatch(dispatchCb);
}

void Connection::close() {
	if (!_transport || _shutdown) {
		return;
	}
	// Give the queued messages one last chance to leave before the shutdown handshake: whatever the
	// caller enqueued a moment ago has not necessarily reached the wire yet.
	_out.flush(getStream());
	_out.clear();
	_transport->close(true);
	_shutdown = true;
}

} // namespace stappler::xenolith::remote
