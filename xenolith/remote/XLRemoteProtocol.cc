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

#include "XLRemoteProtocol.h"
#include "SPPlatform.h"
#include "SPCoreCrypto.h"

#include <sprt/runtime/utils/compress.h>

#include <stdlib.h> // getenv for the debug-only XL_REMOTE_FAKE_VERSION hook

#include "XLRemoteTransport.h"

#ifdef DELETE
#undef DELETE
#endif

namespace STAPPLER_VERSIONIZED stappler::xenolith::remote {

// --- big-endian writers ---

/*static void putU32(Bytes &b, uint32_t v) {
	b.emplace_back(uint8_t(v >> 24));
	b.emplace_back(uint8_t(v >> 16));
	b.emplace_back(uint8_t(v >> 8));
	b.emplace_back(uint8_t(v));
}

static void putU64(Bytes &b, uint64_t v) {
	putU32(b, uint32_t(v >> 32));
	putU32(b, uint32_t(v));
}

static void putF32(Bytes &b, float f) {
	uint32_t u = 0;
	__builtin_memcpy(&u, &f, sizeof(u));
	putU32(b, u);
}

static void putBlob(Bytes &b, BytesView d) {
	putU32(b, uint32_t(d.size()));
	b.insert(b.end(), d.data(), d.data() + d.size());
}*/

// --- bounds-checked big-endian reader ---

namespace {
struct Reader {
	const uint8_t *p;
	size_t len;

	bool u32(uint32_t &out) {
		if (len < 4) {
			return false;
		}
		out = (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8)
				| uint32_t(p[3]);
		p += 4;
		len -= 4;
		return true;
	}
	bool u64(uint64_t &out) {
		uint32_t hi = 0, lo = 0;
		if (!u32(hi) || !u32(lo)) {
			return false;
		}
		out = (uint64_t(hi) << 32) | uint64_t(lo);
		return true;
	}
	bool f32(float &out) {
		uint32_t u = 0;
		if (!u32(u)) {
			return false;
		}
		__builtin_memcpy(&out, &u, sizeof(out));
		return true;
	}
	bool blob(Bytes &out) {
		uint32_t n = 0;
		if (!u32(n) || len < n) {
			return false;
		}
		out.assign(p, p + n);
		p += n;
		len -= n;
		return true;
	}
	bool blobString(String &out) {
		uint32_t n = 0;
		if (!u32(n) || len < n) {
			return false;
		}
		out = String(reinterpret_cast<const char *>(p), n);
		p += n;
		len -= n;
		return true;
	}
};
} // namespace

// --- FrameConstraints (de)serialization ---
/*
static void putConstraints(Bytes &b, const core::FrameConstraints &c) {
	putU32(b, c.extent.width);
	putU32(b, c.extent.height);
	putU32(b, c.extent.depth);
	putF32(b, c.contentPadding.top);
	putF32(b, c.contentPadding.right);
	putF32(b, c.contentPadding.bottom);
	putF32(b, c.contentPadding.left);
	putU32(b, uint32_t(c.transform));
	putF32(b, c.density);
	putF32(b, c.surfaceDensity);
	putU64(b, c.frameInterval);
}

static bool getConstraints(Reader &r, core::FrameConstraints &c) {
	uint32_t transform = 0;
	if (!r.u32(c.extent.width) || !r.u32(c.extent.height) || !r.u32(c.extent.depth)) {
		return false;
	}
	if (!r.f32(c.contentPadding.top) || !r.f32(c.contentPadding.right)
			|| !r.f32(c.contentPadding.bottom) || !r.f32(c.contentPadding.left)) {
		return false;
	}
	if (!r.u32(transform)) {
		return false;
	}
	c.transform = core::SurfaceTransformFlags(transform);
	if (!r.f32(c.density) || !r.f32(c.surfaceDensity) || !r.u64(c.frameInterval)) {
		return false;
	}
	return true;
}
*/

// --- dev key ---

#if DEBUG
BytesView getDevBearerKey() {
	static uint8_t key[kBearerKeySize];
	static bool inited = [] {
		for (uint32_t i = 0; i < kBearerKeySize; ++i) { key[i] = uint8_t(0xA5 ^ (i * 7 + 13)); }
		return true;
	}();
	(void)inited;
	return BytesView(key, kBearerKeySize);
}
#endif

// --- stream I/O + handshake ---

// Cap on a single frame to bound allocations from a hostile/garbled peer. It bounds the on-wire
// size AND the decompressed size, so a compressed frame can never expand past it.
static constexpr uint32_t kMaxFrameSize = 64u * 1'024 * 1'024;

// The single decision point for "may this frame decompress to rawSize", shared by readFrame and
// MessageReader::append. The bound is absolute (kMaxFrameSize), with no compression-ratio test:
// legitimate traffic (input batches, zero-filled CBOR blobs) reaches LZ4's ratio ceiling.
static bool isAcceptableRawSize(const MessageHeader &, uint32_t rawSize, size_t) {
	return rawSize <= kMaxFrameSize;
}

// Read exactly n bytes, bounded by an absolute deadline. Blocking, so used only by the setup
// handshake; other paths use OutgoingQueue. The transport is pumped between retries, which a
// datagram-based transport needs to make progress.
static bool streamReadFull(TransportConnection &conn, uint8_t *buf, size_t n, uint64_t deadline) {
	auto stream = conn.getStream(StreamClass::Control);
	if (!stream) {
		return false;
	}
	size_t got = 0;
	while (got < n) {
		size_t r = 0;
		if (stream->read(buf + got, n - got, r) != Status::Ok) {
			return false; // closed or fatal
		}
		if (r > 0) {
			got += r;
			continue;
		}
		conn.handleEvents();
		if (sp::platform::clock(ClockType::Monotonic) >= deadline) {
			return false;
		}
		sp::platform::sleep(1'000);
	}
	return true;
}

static bool readMessagePayloadWithHeader(BytesViewNetwork view, const MessageHeader &h,
		BytesView dict, const Callback<void(const MessageHeader &, BytesView)> &cb) {

	if ((h.msgflags & toInt(MessageFlags::Compressed)) == 0) {
		// uncompressed
		cb(h, BytesView(view.data(), h.size));
		return true;
	}

	// `view` has already consumed the 4-byte raw size, so what is left is the compressed payload.
	auto csize = view.readUnsigned32();
	if (!isAcceptableRawSize(h, csize, view.size())) {
		return false;
	}

	bool success = true;
	auto ucbuf = __sprt_typed_malloca(uint8_t, csize);

	if (h.msgflags & toInt(MessageFlags::Dictionary)) {
		if (!dict.empty()) {
			size_t d = sprt::lz4_decompressDataDict(view.data(), view.size(), ucbuf, csize,
					dict.data(), dict.size());
			if (d == csize) {
				cb(h, BytesView(ucbuf, csize));
			} else {
				success = false;
			}
		} else {
			success = false;
		}
	} else {
		size_t d = sprt::lz4_decompressData(view.data(), view.size(), ucbuf, csize);
		if (d == csize) {
			cb(h, BytesView(ucbuf, csize));
		} else {
			success = false;
		}
	}

	__sprt_freea(ucbuf);

	return success;
}

bool readMessagePayload(BytesViewNetwork &view, BytesView dict,
		const Callback<void(const MessageHeader &, BytesView)> &cb) {

	if (view.size() < sizeof(MessageHeader)) {
		return false;
	}

	auto tmp = view;

	MessageHeader h;
	h.msgtype = view.readUnsigned();
	h.msgflags = view.readUnsigned();
	h.domain = view.readUnsigned();
	h.code = view.readUnsigned();
	h.serial = view.readUnsigned32();
	h.size = view.readUnsigned32();

	if (view.size() < h.size) {
		view = tmp;
		return false;
	}

	if (!readMessagePayloadWithHeader(view.readBytes(h.size), h, dict, cb)) {
		view = tmp;
		return false;
	}

	return true;
}

bool readFrame(TransportConnection &conn, uint64_t deadline, BytesView dict,
		const Callback<void(const MessageHeader &, BytesView payload)> &cb) {
	MessageHeader h;
	if (!streamReadFull(conn, (uint8_t *)&h, sizeof(MessageHeader), deadline)) {
		return false;
	}

	h.serial = sprt::byteorder::NetworkToHost(h.serial);
	h.size = sprt::byteorder::NetworkToHost(h.size);

	if (h.size > kMaxFrameSize) {
		return false;
	}

	auto buf = __sprt_typed_malloca(uint8_t, h.size);

	if (h.size > 0 && !streamReadFull(conn, buf, h.size, deadline)) {
		__sprt_freea(buf);
		return false;
	}

	auto frameData = BytesViewNetwork(buf, h.size);

	bool success = readMessagePayloadWithHeader(frameData, h, dict, cb);

	__sprt_freea(buf);

	return success;
}

void encodeFrame(Bytes &out, BytesView dict, MessageType t, Domain d, uint8_t msg, uint32_t serial,
		BytesView payload) {
	auto at = out.size();

	if (payload.empty()) {
		MessageHeader mh;
		mh.msgtype = toInt(t);
		mh.msgflags = 0;
		mh.domain = toInt(d);
		mh.code = msg;
		mh.serial = sprt::byteorder::HostToNetwork(serial);
		mh.size = 0;
		out.resize(at + sizeof(MessageHeader));
		__sprt_memcpy(out.data() + at, &mh, sizeof(MessageHeader));
		return;
	}

	uint32_t bound = sprt::lz4_getCompressBounds(payload.size());
	if (bound == 0) {
		return;
	}

	// Worst case up front, then shrink to what was actually produced.
	out.resize(at + sizeof(MessageHeader) + 4 + sprt::max(bound, uint32_t(payload.size())));
	auto frameData = out.data() + at;

	MessageHeader *mh = (MessageHeader *)frameData;
	mh->msgtype = toInt(t);
	mh->msgflags = 0;
	mh->domain = toInt(d);
	mh->code = msg;
	mh->serial = sprt::byteorder::HostToNetwork(serial);

	// Domain::Data blocks do not survive the LZ4 dictionary round-trip, and gain nothing from it.
	bool useDict = !dict.empty() && d != Domain::Data;

	uint32_t clen = 0;
	bool usingDict = false;
	if (useDict) {
		clen = sprt::lz4_compressDataDict(payload.data(), payload.size(),
				frameData + sizeof(MessageHeader) + 4, bound, dict.data(), dict.size());
		if (clen > 0) {
			usingDict = true;
		}
	}
	if (clen == 0) {
		clen = sprt::lz4_compressData(payload.data(), payload.size(),
				frameData + sizeof(MessageHeader) + 4, bound);
		usingDict = false;
	}

	size_t frameSize;
	if (clen + 4 < payload.size()) {
		mh->msgflags = toInt(MessageFlags::Compressed);
		if (usingDict) {
			mh->msgflags |= toInt(MessageFlags::Dictionary);
		}
		mh->size = sprt::byteorder::HostToNetwork(clen + 4);
		// Uncompressed size in network byte order, to match the BytesViewNetwork reader on the peer.
		*(uint32_t *)(frameData + sizeof(MessageHeader)) =
				sprt::byteorder::HostToNetwork(uint32_t(payload.size()));
		frameSize = sizeof(MessageHeader) + 4 + clen;
	} else {
		mh->msgflags = 0;
		mh->size = sprt::byteorder::HostToNetwork(uint32_t(payload.size()));
		frameSize = sizeof(MessageHeader) + payload.size();
		__sprt_memcpy(frameData + sizeof(MessageHeader), payload.data(), payload.size());
	}

	out.resize(at + frameSize);
}

bool OutgoingQueue::push(BytesView dict, MessageType t, Domain d, uint8_t code, uint32_t serial,
		BytesView payload) {
	if (pending() > kMaxPending) {
		return false;
	}
	encodeFrame(_buffer, dict, t, d, code, serial, payload);
	return true;
}

bool OutgoingQueue::flush(TransportStream *stream) {
	if (!stream || empty()) {
		return true;
	}
	while (_offset < _buffer.size()) {
		size_t w = 0;
		if (stream->write(BytesView(_buffer.data() + _offset, _buffer.size() - _offset), w)
				!= Status::Ok) {
			return false; // closed or fatal
		}
		if (w == 0) {
			break; // backpressure, not an error: leave the rest for the next poll
		}
		_offset += w;
	}

	if (_offset >= _buffer.size()) {
		_buffer.clear();
		_offset = 0;
	} else if (_offset >= 64u * 1'024) {
		// Reclaim the consumed prefix once it is worth the move, so a long-lived connection does not
		// keep growing a buffer it has already sent.
		_buffer.erase(_buffer.begin(), _buffer.begin() + _offset);
		_offset = 0;
	}
	return true;
}

void OutgoingQueue::clear() {
	_buffer.clear();
	_offset = 0;
}

// --- MessageReader (receive-side stream reassembler) ---

static_assert(sizeof(MessageHeader) == 12, "MessageHeader must be a tight 12-byte wire struct");

// Decode an on-wire message payload into `out`, decompressing per the header flags. A compressed
// payload is [u32 rawSize][lz4 bytes]; the dictionary variant uses the negotiated dict.
static bool decodeMessagePayload(const MessageHeader &h, BytesView wire, BytesView dict,
		Bytes &out) {
	if (!(h.msgflags & toInt(MessageFlags::Compressed))) {
		out.assign(wire.data(), wire.data() + wire.size());
		return true;
	}

	if (wire.size() < sizeof(uint32_t)) {
		return false;
	}

	auto r = BytesViewNetwork(wire);
	uint32_t rawSize = r.readUnsigned32();
	if (!isAcceptableRawSize(h, rawSize, r.size())) {
		return false;
	}
	out.resize(rawSize);
	if (rawSize == 0) {
		return true;
	}
	size_t d = 0;
	if (h.msgflags & toInt(MessageFlags::Dictionary)) {
		if (dict.empty()) {
			return false;
		}
		d = sprt::lz4_decompressDataDict(r.data(), r.size(), out.data(), rawSize, dict.data(),
				dict.size());
	} else {
		d = sprt::lz4_decompressData(r.data(), r.size(), out.data(), rawSize);
	}
	return d == rawSize;
}

bool MessageReader::append(BytesView raw, BytesView dict) {
	_buffer.insert(_buffer.end(), raw.data(), raw.data() + raw.size());

	const size_t hsz = sizeof(MessageHeader);
	size_t off = 0;
	while (_buffer.size() - off >= hsz) {
		MessageHeader h;
		__sprt_memcpy(&h, _buffer.data() + off, hsz);
		uint32_t size = sprt::byteorder::NetworkToHost(h.size);
		if (size > kMaxFrameSize) {
			return false; // framing violation
		}
		if (_buffer.size() - off < hsz + size) {
			break; // frame not fully arrived yet; keep it buffered for the next append
		}
		h.serial = sprt::byteorder::NetworkToHost(h.serial);
		h.size = size;

		Message m;
		m.header = h;
		if (!decodeMessagePayload(h, BytesView(_buffer.data() + off + hsz, size), dict,
					m.payload)) {
			return false; // corrupt / undictionaried compressed payload
		}
		_pending.emplace_back(sp::move(m));
		off += hsz + size;
	}
	if (off > 0) {
		_buffer.erase(_buffer.begin(), _buffer.begin() + off);
	}
	return true;
}

void MessageReader::addMessage(const MessageHeader &h, BytesView data) {
	_pending.emplace_back(Message(h, data.bytes<Interface>()));
}

void MessageReader::dispatch(const Callback<bool(const MessageHeader &, BytesView)> &cb) {
	// Retry deferred messages until a full pass consumes nothing, so a reply and a message it
	// depends on resolve in one pump regardless of arrival order.
	bool progress = true;
	while (progress && !_pending.empty()) {
		progress = false;
		for (size_t i = 0; i < _pending.size();) {
			auto &m = _pending[i];
			if (cb(m.header, BytesView(m.payload.data(), m.payload.size()))) {
				_pending.erase(_pending.begin() + i);
				progress = true;
			} else {
				++i;
			}
		}
	}
}

void MessageReader::clear() {
	_buffer.clear();
	_pending.clear();
}

void WireWriter::writeU8(uint8_t v) { _out->emplace_back(v); }

void WireWriter::writeU16(uint16_t v) {
	auto n = sprt::byteorder::HostToNetwork(v);
	auto p = reinterpret_cast<const uint8_t *>(&n);
	_out->insert(_out->end(), p, p + sizeof(n));
}

void WireWriter::writeU32(uint32_t v) {
	auto n = sprt::byteorder::HostToNetwork(v);
	auto p = reinterpret_cast<const uint8_t *>(&n);
	_out->insert(_out->end(), p, p + sizeof(n));
}

void WireWriter::writeU64(uint64_t v) {
	auto n = sprt::byteorder::HostToNetwork(v);
	auto p = reinterpret_cast<const uint8_t *>(&n);
	_out->insert(_out->end(), p, p + sizeof(n));
}

void WireWriter::writeFloatBits(float v) {
	// Through the bits, never a numeric conversion: NaN must round-trip exactly (see WireWriter).
	uint32_t bits = 0;
	__sprt_memcpy(&bits, &v, sizeof(bits));
	writeU32(bits);
}

void WireWriter::writeBytes(BytesView d) {
	_out->insert(_out->end(), d.data(), d.data() + d.size());
}

void WireWriter::writeZero(size_t count) { _out->insert(_out->end(), count, uint8_t(0)); }

float readFloatBits(BytesViewNetwork &in) {
	auto bits = in.readUnsigned32();
	float v = 0.0f;
	__sprt_memcpy(&v, &bits, sizeof(v));
	return v;
}

static bool decodeServerHello(BytesViewNetwork in, ServerHello &h) {
	h.magic = in.readUnsigned32();
	h.version = in.readUnsigned16();
	h.status = in.readUnsigned();
	h.dictSource = in.readUnsigned();

	// The magic identifies the protocol. Readers zero-fill past the end, so a truncated hello is
	// caught here as magic == 0.
	if (h.magic != kProtocolMagic) {
		return false;
	}

	// Status before version: a refusal's reason (Busy, AuthFailed) must reach the client instead of
	// a local BadProtocol.
	if (h.status != 0) {
		return true;
	}

	// An accepting hello of another version is not usable (see kProtocolVersion).
	if (h.version != kProtocolVersion) {
		return false;
	}

	if (h.dictSource == toInt(DictSource::Server)) {
		auto s = in.readUnsigned16();
		h.dict = BytesView(in.readBytes(s));
	}
	return true;
}

/* The version this client puts in its ClientHello: kProtocolVersion, or XL_REMOTE_FAKE_VERSION=<n>
 * to test version refusal (like XL_REMOTE_FAKE_ABI). Debug-only; it can only get a client refused.
 */
static uint16_t announcedProtocolVersion() {
#if DEBUG
	if (auto env = ::getenv("XL_REMOTE_FAKE_VERSION")) {
		auto forced = uint16_t(StringView(env).readInteger(10).get(kProtocolVersion));
		log::source().warn("remote::Protocol", "XL_REMOTE_FAKE_VERSION: announcing version ", forced,
				" instead of ", kProtocolVersion);
		return forced;
	}
#endif
	return kProtocolVersion;
}

// Read what a hello frame still needs: the header, then exactly its payload, never more. True once
// the frame is complete; `failed` on a closed stream or an oversized frame.
static bool readHelloFrame(TransportStream *stream, Bytes &buf, bool &failed) {
	failed = false;
	for (;;) {
		size_t need = 0;
		if (buf.size() < sizeof(MessageHeader)) {
			need = sizeof(MessageHeader) - buf.size();
		} else {
			MessageHeader h;
			__sprt_memcpy(&h, buf.data(), sizeof(MessageHeader));
			auto size = sprt::byteorder::NetworkToHost(h.size);
			if (size > kMaxFrameSize) {
				failed = true;
				return false;
			}
			auto total = sizeof(MessageHeader) + size_t(size);
			if (buf.size() >= total) {
				return true;
			}
			need = total - buf.size();
		}

		uint8_t tmp[512];
		size_t got = 0;
		if (stream->read(tmp, sprt::min(need, sizeof(tmp)), got) != Status::Ok) {
			failed = true;
			return false;
		}
		if (got == 0) {
			return false;
		}
		buf.insert(buf.end(), tmp, tmp + got);
	}
}

// Write what the transport takes; true once everything is out.
static bool flushHelloFrame(TransportStream *stream, const Bytes &out, size_t &offset,
		bool &failed) {
	failed = false;
	while (offset < out.size()) {
		size_t written = 0;
		if (stream->write(BytesView(out.data() + offset, out.size() - offset), written)
				!= Status::Ok) {
			failed = true;
			return false;
		}
		if (written == 0) {
			return false;
		}
		offset += written;
	}
	return true;
}

// The frame header of a hello: never compressed, serial 0.
static void writeHelloHeader(WireWriter &w, MessageType type, GlobalCode code, uint32_t size) {
	w.writeU8(uint8_t(toInt(type)));
	w.writeU8(uint8_t(toInt(MessageFlags::None)));
	w.writeU8(uint8_t(toInt(Domain::Global)));
	w.writeU8(uint8_t(toInt(code)));
	w.writeU32(0);
	w.writeU32(size);
}

static void encodeClientHello(Bytes &out, BytesView key, BytesView dict) {
	auto size = uint32_t(sizeof(uint32_t) * 3 + key.size() + dict.size());
	WireWriter w(out);
	writeHelloHeader(w, MessageType::Client, GlobalCode::ClientHello, size);
	w.writeU32(kProtocolMagic);
	w.writeU16(announcedProtocolVersion());
	w.writeU8(toInt(AuthMode::BearerKey));
	w.writeU8(toInt(ClientHelloFlags::None));
	w.writeU16(uint16_t(key.size()));
	w.writeU16(uint16_t(dict.size()));
	w.writeBytes(key);
	w.writeBytes(dict);
}

static void encodeServerHello(Bytes &out, GlobalError status, DictSource dictSource,
		BytesView serverDict) {
	bool withDict = (status == GlobalError::Ok && dictSource == DictSource::Server);
	auto size = uint32_t(sizeof(uint32_t) * 2);
	if (withDict) {
		size += sizeof(uint16_t) + serverDict.size();
	}
	WireWriter w(out);
	writeHelloHeader(w, MessageType::Server, GlobalCode::ServerHello, size);
	w.writeU32(kProtocolMagic);
	w.writeU16(kProtocolVersion);
	w.writeU8(toInt(status));
	w.writeU8(toInt(withDict ? DictSource::Server : dictSource));
	if (withDict) {
		w.writeU16(uint16_t(serverDict.size()));
		w.writeBytes(serverDict);
	}
}

static void decodeClientHello(BytesView payload, ClientHello &ch) {
	auto in = BytesViewNetwork(payload);
	ch.magic = in.readUnsigned32();
	ch.version = in.readUnsigned16();
	ch.authMode = in.readUnsigned();
	ch.clientHelloFlags = in.readUnsigned();
	auto authDataSize = in.readUnsigned16();
	auto dictSize = in.readUnsigned16();
	ch.authData = in.readBytes<sprt::endian::native>(authDataSize);
	ch.suggestedDict = in.readBytes<sprt::endian::native>(dictSize);
}

// The payload of a complete hello frame, or false when the frame is not a plain hello.
static bool helloPayload(const Bytes &frame, BytesView &payload) {
	MessageHeader h;
	__sprt_memcpy(&h, frame.data(), sizeof(MessageHeader));
	if ((h.msgflags & toInt(MessageFlags::Compressed)) != 0) {
		return false;
	}
	payload = BytesView(frame.data() + sizeof(MessageHeader), frame.size() - sizeof(MessageHeader));
	return true;
}

static GlobalError negotiateHello(const ClientHello &ch, BytesView expectedKey,
		bool requireBearerKey) {
	GlobalError status;
	// Reject a foreign protocol or version before the key comparison.
	if (ch.magic != kProtocolMagic || ch.version != kProtocolVersion) {
		status = GlobalError::BadProtocol;
	} else if (ch.authMode != toInt(AuthMode::BearerKey)) {
		status = GlobalError::UnsupportedAuth;
	} else if (!requireBearerKey) {
		// The transport vouched for the peer; the key is not consulted.
		status = GlobalError::Ok;
	} else if (expectedKey.empty()
			|| !crypto::isEqualConstantTime(BytesView(ch.authData.data(), ch.authData.size()),
					expectedKey)) {
		status = GlobalError::AuthFailed;
	} else {
		status = GlobalError::Ok;
	}
	return status;
}

void ServerHandshake::begin(uint64_t deadlineUs) {
	_state = State::ReadingHello;
	_deadline = deadlineUs;
	_in.clear();
	_out.clear();
	_outOffset = 0;
	_helloValid = false;
	_hello = ClientHello();
	_dictSource = DictSource::None;
	_negotiatedDict.clear();
	_replied = GlobalError::BadProtocol;
}

ServerHandshake::State ServerHandshake::step(TransportConnection &conn, uint64_t nowUs) {
	auto stream = conn.getStream(StreamClass::Control);
	if (!stream) {
		_state = State::Failed;
		return _state;
	}

	bool failed = false;
	switch (_state) {
	case State::ReadingHello:
		if (readHelloFrame(stream, _in, failed)) {
			BytesView payload;
			_helloValid = helloPayload(_in, payload);
			if (_helloValid) {
				decodeClientHello(payload, _hello);
			}
			_state = State::HelloReceived;
		} else if (failed || nowUs >= _deadline) {
			_state = State::Failed;
		}
		break;
	case State::Replying:
		if (flushHelloFrame(stream, _out, _outOffset, failed)) {
			_state = State::Done;
		} else if (failed || nowUs >= _deadline) {
			_state = State::Failed;
		}
		break;
	default: break;
	}
	return _state;
}

GlobalError ServerHandshake::negotiate(BytesView expectedKey, BytesView serverDict,
		bool requireBearerKey) {
	if (_state != State::HelloReceived || !_helloValid) {
		return GlobalError::BadProtocol;
	}
	auto status = negotiateHello(_hello, expectedKey, requireBearerKey);
	if (status == GlobalError::Ok) {
		if (serverDict.empty() && !_hello.suggestedDict.empty()) {
			_dictSource = DictSource::Client;
			_negotiatedDict = _hello.suggestedDict.bytes<Interface>();
		} else if (!serverDict.empty()) {
			_dictSource = DictSource::Server;
		}
	}
	return status;
}

void ServerHandshake::reply(GlobalError status, BytesView serverDict) {
	if (_state == State::Idle || _state == State::Replying || _state == State::Done) {
		return;
	}
	_replied = status;
	_out.clear();
	_outOffset = 0;
	encodeServerHello(_out, status, status == GlobalError::Ok ? _dictSource : DictSource::None,
			serverDict);
	_state = State::Replying;
}

void ClientHandshake::begin(BytesView bearerKey, BytesView dict, uint64_t deadlineUs) {
	_state = State::Writing;
	_deadline = deadlineUs;
	_in.clear();
	_out.clear();
	_outOffset = 0;
	_hello = ServerHello();
	_result = GlobalError::NetworkBackend;
	encodeClientHello(_out, bearerKey, dict);
}

ClientHandshake::State ClientHandshake::step(TransportConnection &conn, uint64_t nowUs) {
	auto stream = conn.getStream(StreamClass::Control);
	if (!stream) {
		_state = State::Failed;
		return _state;
	}

	bool failed = false;
	if (_state == State::Writing) {
		if (flushHelloFrame(stream, _out, _outOffset, failed)) {
			_state = State::ReadingReply;
		} else if (failed || nowUs >= _deadline) {
			_result = GlobalError::NetworkBackend;
			_state = State::Failed;
			return _state;
		}
	}

	if (_state == State::ReadingReply) {
		if (readHelloFrame(stream, _in, failed)) {
			BytesView payload;
			if (!helloPayload(_in, payload) || !decodeServerHello(payload, _hello)) {
				_result = GlobalError::BadProtocol;
				_state = State::Failed;
			} else {
				_result = GlobalError(_hello.status);
				_state = State::Done;
			}
		} else if (failed || nowUs >= _deadline) {
			_result = GlobalError::NetworkBackend;
			_state = State::Failed;
		}
	}
	return _state;
}

static uint64_t handshakeNow() { return sp::platform::clock(ClockType::Monotonic); }

GlobalError clientHandshake(TransportConnection &conn, BytesView key, BytesView dict,
		uint64_t deadline, const Callback<void(const ServerHello &out)> &cb) {
	ClientHandshake handshake;
	handshake.begin(key, dict, deadline);
	for (;;) {
		auto state = handshake.step(conn, handshakeNow());
		if (state == ClientHandshake::State::Done || state == ClientHandshake::State::Failed) {
			break;
		}
		conn.handleEvents();
		sp::platform::sleep(1'000);
	}
	if (handshake.getResult() == GlobalError::Ok) {
		cb(handshake.getServerHello());
	}
	return handshake.getResult();
}

// Drive a server handshake to its end with the transport pumped in between.
static void runServerHandshake(TransportConnection &conn, ServerHandshake &handshake,
		ServerHandshake::State until) {
	for (;;) {
		auto state = handshake.step(conn, handshakeNow());
		if (state == until || state == ServerHandshake::State::Done
				|| state == ServerHandshake::State::Failed) {
			return;
		}
		conn.handleEvents();
		sp::platform::sleep(1'000);
	}
}

GlobalError serverHandshake(TransportConnection &conn, BytesView expectedKey, BytesView serverDict,
		Bytes &negotiatedDict, uint64_t deadline, bool requireBearerKey) {
	negotiatedDict.clear();

	ServerHandshake handshake;
	handshake.begin(deadline);
	runServerHandshake(conn, handshake, ServerHandshake::State::HelloReceived);

	auto status = handshake.negotiate(expectedKey, serverDict, requireBearerKey);
	handshake.reply(status, serverDict);
	runServerHandshake(conn, handshake, ServerHandshake::State::Done);

	negotiatedDict = handshake.getNegotiatedDict().bytes<Interface>();
	return status;
}

StringView getGlobalErrorName(GlobalError e) {
	switch (e) {
	case GlobalError::Ok: return StringView("Ok"); break;
	case GlobalError::BadProtocol: return StringView("BadProtocol"); break;
	case GlobalError::UnsupportedAuth: return StringView("UnsupportedAuth"); break;
	case GlobalError::AuthFailed: return StringView("AuthFailed"); break;
	case GlobalError::Busy: return StringView("Busy"); break;
	case GlobalError::IncompatiblePeer: return StringView("IncompatiblePeer"); break;
	case GlobalError::NotImplemented: return StringView("NotImplemented"); break;
	case GlobalError::NetworkBackend: return StringView("NetworkBackend"); break;
	}
	return StringView("Unknown");
}

// How long a refusal waits for the ClientHello before answering anyway; see serverHandshakeReject.
static constexpr uint64_t kRejectHelloWaitUs = 100'000;

GlobalError serverHandshakeReject(TransportConnection &conn, GlobalError status,
		uint64_t deadline) {
	/* Read the ClientHello and discard it, then answer. With QUIC in OpenSSL's AUTO_BIDI mode, a
	 * server writing before the client's stream arrives creates its own stream the client never
	 * reads, so the read binds the default stream. The wait is short and separate from `deadline`,
	 * so a silent peer cannot park this thread. */
	ServerHandshake handshake;
	handshake.begin(sprt::min(deadline, handshakeNow() + kRejectHelloWaitUs));
	runServerHandshake(conn, handshake, ServerHandshake::State::HelloReceived);

	handshake.setDeadline(deadline);
	handshake.reply(status, BytesView());
	runServerHandshake(conn, handshake, ServerHandshake::State::Done);

	if (handshake.getState() != ServerHandshake::State::Done) {
		log::source().error("remote::Protocol", "failed to deliver the refusal (",
				getGlobalErrorName(status), "); the peer will only learn of it by timing out");
	}
	return status;
}

} // namespace stappler::xenolith::remote
