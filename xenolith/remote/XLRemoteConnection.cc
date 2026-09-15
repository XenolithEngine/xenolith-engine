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
	if (!_transport) {
		return false;
	}

	// Resolve the class -> stream mapping once and deduplicate by pointer: per-stream state
	// (partial frame, unsent tail) is keyed by the identity settled here.
	for (uint32_t i = 0; i < kStreamClassCount; ++i) {
		auto stream = _transport->getStream(StreamClass(i));
		uint32_t canonical = i;
		for (uint32_t j = 0; j < i; ++j) {
			if (_streams[j].stream == stream) {
				canonical = j;
				break;
			}
		}
		_canonicalClass[i] = uint8_t(canonical);
		if (canonical == i) {
			_streams[i].stream = stream;
			_distinctClasses[_distinctCount++] = uint8_t(i);
		}
	}
	return true;
}

Connection::StreamState *Connection::getStreamState(StreamClass c) {
	auto idx = _canonicalClass[toInt(c)];
	return _streams[idx].stream ? &_streams[idx] : nullptr;
}

TransportStream *Connection::getStream(StreamClass c) const {
	return _streams[_canonicalClass[toInt(c)]].stream;
}

sprt::dispatch::NativeHandle Connection::getPollHandle() const {
	return _transport ? _transport->getPollHandle() : sprt::dispatch::NativeHandle(-1);
}

TransportWaitAddress Connection::getWaitAddress() {
	return _transport ? _transport->getWaitAddress() : TransportWaitAddress();
}

bool Connection::isClosed() { return !_transport || _transport->isClosed(); }

GlobalError Connection::enqueue(MessageType t, Domain d, uint8_t code, uint32_t serial,
		BytesView payload) {
	if (!_transport) {
		return GlobalError::NetworkBackend;
	}
	auto state = getStreamState(streamClassForDomain(d));
	if (!state) {
		return GlobalError::NetworkBackend;
	}
	if (!state->out.push(BytesView(_dict.data(), _dict.size()), t, d, code, serial, payload)) {
		log::source().error("remote::Connection", "send queue overflow (", state->out.pending(),
				" bytes unsent); the peer is not draining");
		return GlobalError::NetworkBackend;
	}
	return state->out.flush(state->stream) ? GlobalError::Ok : GlobalError::NetworkBackend;
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
	if (!_transport || _distinctCount == 0) {
		return;
	}

	// Drain send queues left by backpressure. This is the only place a stalled write makes
	// progress, so the looper drives poll() on readiness and on every update tick.
	for (uint32_t i = 0; i < _distinctCount; ++i) {
		auto &state = _streams[_distinctClasses[i]];
		if (!state.out.flush(state.stream)) {
			log::source().error("remote::Connection", "send failed; dropping connection");
			close();
			return;
		}
	}

	// Service the transport (datagrams, retransmit timers) once, not per stream: a multi-stream
	// transport still has a single event source underneath.
	_transport->handleEvents();

	// Streams are visited in StreamClass order, so Control drains before Bulk.
	uint8_t buf[4'096];
	for (uint32_t i = 0; i < _distinctCount; ++i) {
		auto &state = _streams[_distinctClasses[i]];
		for (;;) {
			size_t n = 0;
			if (state.stream->read(buf, sizeof(buf), n) != Status::Ok || n == 0) {
				break; // drained or closed
			}

			BytesViewNetwork nw(buf, n);

			// Fast path, only when the reassembler is idle: no buffered partial (the chunk then
			// starts on a frame boundary) and nothing pending (so newer frames cannot overtake
			// queued ones, e.g. a FrameInput after its FrameCommit). Otherwise use append().
			if (!state.reader.hasPartialMessage() && !state.reader.hasPending()) {
				while (readMessagePayload(nw, _dict, [&](const MessageHeader &h, BytesView data) {
					if (!dispatchCb(h, data)) {
						state.reader.addMessage(h, data);
					}
				})) { }
			}

			// Buffer the remainder: a trailing partial frame, or the whole chunk when a partial was
			// already buffered. The reassembler completes it on a later read.
			if (!nw.empty()) {
				if (!state.reader.append(BytesView(nw.data(), nw.size()),
							BytesView(_dict.data(), _dict.size()))) {
					log::source().error("remote::Connection",
							"framing violation; dropping connection");
					close();
					return;
				}
			}
		}

		state.reader.dispatch(dispatchCb);
	}
}

void Connection::close() {
	if (!_transport || _shutdown) {
		return;
	}
	// Flush queued messages before the shutdown handshake.
	for (uint32_t i = 0; i < _distinctCount; ++i) {
		auto &state = _streams[_distinctClasses[i]];
		state.out.flush(state.stream);
		state.out.clear();
	}
	_transport->close(true);
	_shutdown = true;
}

} // namespace stappler::xenolith::remote
