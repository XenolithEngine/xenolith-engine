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

// The single decision point for "may this frame decompress to rawSize", so the two decode paths
// (readFrame and MessageReader::append) cannot drift apart again -- they used to disagree by 4x.
//
// The bound is ABSOLUTE, with no compression-ratio test on top, and that is deliberate. A ratio cap
// would have to sit below what LZ4 can actually produce to reject anything at all (its own ceiling
// is around 250:1), and legitimate traffic on this protocol reaches that range: a batch of near
// identical InputEventData structs, or a CBOR queue blob full of zeroed fields, compresses an order
// of magnitude better than typical data. So a ratio low enough to fire would refuse real frames,
// and one high enough to be safe could never fire. kMaxFrameSize bounds the allocation either way,
// which is the property that actually matters against a decompression bomb.
static bool isAcceptableRawSize(const MessageHeader &, uint32_t rawSize, size_t) {
	return rawSize <= kMaxFrameSize;
}

// Read exactly n bytes, bounded by an absolute deadline.
//
// Still blocking, and still only used by the SETUP HANDSHAKE: it runs once per connection, before
// any frame, and its deadline is the point. Every other path goes through OutgoingQueue and the
// non-blocking drain in poll(). Between retries the transport is pumped, which is what lets a
// datagram-based one make progress at all.
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

// Write exactly n bytes, bounded by an absolute deadline. Handshake-only, for the same reason.
static bool streamWriteAll(TransportConnection &conn, const uint8_t *buf, size_t n,
		uint64_t deadline) {
	auto stream = conn.getStream(StreamClass::Control);
	if (!stream) {
		return false;
	}
	size_t off = 0;
	while (off < n) {
		size_t w = 0;
		if (stream->write(BytesView(buf + off, n - off), w) != Status::Ok) {
			return false; // closed or fatal
		}
		if (w > 0) {
			off += w;
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

	// See the note in sendFrame's original body: Domain::Data blocks do not survive the LZ4
	// dictionary round-trip, and the dictionary buys raw pixels nothing anyway.
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
	// Re-try deferred messages within this call until a full pass consumes nothing -- so a reply and a
	// message it depends on can both resolve in one pump regardless of arrival order. Terminates
	// because every productive pass shrinks _pending.
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
	// Through the bits, never through a numeric conversion: NaN is a value this protocol carries on
	// purpose (see the class comment), and a NaN is not required to survive float -> anything -> float.
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

[[nodiscard]]
static uint8_t *writeValue8(uint8_t *buf, uint8_t t) {
	buf[0] = t;
	return buf + 1;
}

[[nodiscard]]
static uint8_t *writeValue16(uint8_t *buf, uint16_t t) {
	__sprt_memcpy(buf, &t, sizeof(uint16_t));
	return buf + sizeof(uint16_t);
}

[[nodiscard]]
static uint8_t *writeValue32(uint8_t *buf, uint32_t t) {
	__sprt_memcpy(buf, &t, sizeof(uint32_t));
	return buf + sizeof(uint32_t);
}

[[nodiscard]]
static uint8_t *writeData(uint8_t *buf, BytesView d) {
	__sprt_memcpy(buf, d.data(), d.size());
	return buf + d.size();
}

static bool decodeServerHello(BytesViewNetwork in, ServerHello &h) {
	h.magic = in.readUnsigned32();
	h.version = in.readUnsigned16();
	h.status = in.readUnsigned();
	h.dictSource = in.readUnsigned();

	// The magic answers "is this our protocol at all". A reply from a foreign one used to be accepted
	// as far as its status byte; the readers zero-fill past the end, so a truncated hello lands here
	// as magic == 0 and is caught by the same check.
	if (h.magic != kProtocolMagic) {
		return false;
	}

	// The STATUS is read before the version is judged, and the order matters. A refusal carries a
	// reason -- Busy, AuthFailed -- and rejecting the hello on its version first threw that reason
	// away, leaving the client to report a local BadProtocol for a server that had told it exactly
	// what was wrong. A peer that got the magic right has earned being listened to.
	if (h.status != 0) {
		return true;
	}

	// An accepting hello of another version is not usable: the codes are the same and the bytes
	// under them are not (see kProtocolVersion).
	if (h.version != kProtocolVersion) {
		return false;
	}

	if (h.dictSource == toInt(DictSource::Server)) {
		auto s = in.readUnsigned16();
		h.dict = BytesView(in.readBytes(s));
	}
	return true;
}

/* The version this client puts in its ClientHello.
 *
 * Normally kProtocolVersion. XL_REMOTE_FAKE_VERSION=<n> makes it something else, and it exists for
 * the same reason XL_REMOTE_FAKE_ABI does: two binaries built from one tree necessarily agree on the
 * version, so "a peer of the wrong version is refused" would be a claim nobody had ever executed.
 * Debug-only, and it can only get this client REFUSED -- there is no value it can carry that gets a
 * client accepted which would not have been.
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

static uint8_t *writeMessageHeader(uint8_t *buf, MessageType type, MessageFlags flags,
		Domain domain, uint8_t code, uint32_t serial, uint32_t messageSize) {
	buf = writeValue8(buf, uint8_t(toInt(type)));
	buf = writeValue8(buf, uint8_t(toInt(flags)));
	buf = writeValue8(buf, uint8_t(toInt(domain)));
	buf = writeValue8(buf, uint8_t(code));
	buf = writeValue32(buf, sprt::byteorder::HostToNetwork(serial));
	buf = writeValue32(buf, sprt::byteorder::HostToNetwork(messageSize));
	return buf;
}

GlobalError clientHandshake(TransportConnection &conn, BytesView key, BytesView dict,
		uint64_t deadline,
		const Callback<void(const ServerHello &out)> &cb) {
	uint32_t clientHelloSize = sizeof(uint32_t) * 3 + key.size() + dict.size();

	auto d = __sprt_typed_malloca(uint8_t, clientHelloSize + sizeof(MessageHeader));

	auto buf = writeMessageHeader(d, MessageType::Client, MessageFlags::None, Domain::Global,
			toInt(GlobalCode::ClientHello), 0, clientHelloSize);

	buf = writeValue32(buf, sprt::byteorder::HostToNetwork(kProtocolMagic));
	buf = writeValue16(buf, sprt::byteorder::HostToNetwork(announcedProtocolVersion()));
	buf = writeValue8(buf, toInt(AuthMode::BearerKey));
	buf = writeValue8(buf, toInt(ClientHelloFlags::None));
	buf = writeValue16(buf, sprt::byteorder::HostToNetwork(static_cast<uint16_t>(key.size())));
	buf = writeValue16(buf, sprt::byteorder::HostToNetwork(static_cast<uint16_t>(dict.size())));
	buf = writeData(buf, key);
	if (!dict.empty()) {
		buf = writeData(buf, dict);
	}

	// Free before branching: the early return on a write failure used to leak `d` whenever
	// __sprt_malloca had fallen back to the heap (a large suggested dictionary).
	auto sent = streamWriteAll(conn, d, clientHelloSize + sizeof(MessageHeader), deadline);

	__sprt_freea(d);

	if (!sent) {
		return GlobalError::NetworkBackend;
	}

	GlobalError result = GlobalError::NetworkBackend;

	if (!readFrame(conn, deadline, BytesView(), [&](const MessageHeader &h, BytesView d) {
		ServerHello sh;
		if (!decodeServerHello(d, sh)) {
			result = GlobalError::BadProtocol;
			return;
		}

		if (sh.status != toInt(GlobalError::Ok)) {
			result = GlobalError(sh.status);
			return;
		}

		cb(sh);
		result = GlobalError::Ok;
	})) {
		return GlobalError::NetworkBackend;
	}
	return result;
}

static GlobalError negotiateHello(const ClientHello &ch, BytesView expectedKey,
		bool requireBearerKey) {
	GlobalError status;
	// The magic was parsed but never checked, so a foreign protocol reached the key comparison.
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

// Emit the ServerHello reply. Shared by the negotiating path and by serverHandshakeReject, so the
// refusal answer cannot drift from the accepting one.
static bool writeServerHello(TransportConnection &conn, GlobalError status, DictSource dictSource,
		BytesView serverDict, uint64_t deadline) {
	size_t serverHello = sizeof(uint32_t) * 2;
	bool withDict = (status == GlobalError::Ok && dictSource == DictSource::Server);
	if (withDict) {
		serverHello += sizeof(uint16_t) + serverDict.size();
	}

	auto d = __sprt_typed_malloca(uint8_t, serverHello + sizeof(MessageHeader));

	auto buf = writeMessageHeader(d, MessageType::Server, MessageFlags::None, Domain::Global,
			toInt(GlobalCode::ServerHello), 0, serverHello);

	buf = writeValue32(buf, sprt::byteorder::HostToNetwork(kProtocolMagic));
	buf = writeValue16(buf, sprt::byteorder::HostToNetwork(kProtocolVersion));
	buf = writeValue8(buf, toInt(status));
	buf = writeValue8(buf, toInt(dictSource));

	if (withDict) {
		// Network byte order, like every other size on the wire: decodeServerHello reads it through
		// a BytesViewNetwork, so a host-order write here was byte-swapped on every little-endian peer.
		buf = writeValue16(buf, sprt::byteorder::HostToNetwork(uint16_t(serverDict.size())));
		buf = writeData(buf, serverDict);
	}

	// Free the ALLOCATION, not the write cursor: `buf` has been advanced past the start of `d`, and
	// releasing it corrupted the heap whenever __sprt_malloca had spilled (a large dictionary).
	auto result = streamWriteAll(conn, d, serverHello + sizeof(MessageHeader), deadline);
	__sprt_freea(d);
	return result;
}

GlobalError serverHandshake(TransportConnection &conn, BytesView expectedKey, BytesView serverDict,
		Bytes &negotiatedDict, uint64_t deadline, bool requireBearerKey) {
	negotiatedDict.clear();

	GlobalError outStatus = GlobalError::BadProtocol;
	DictSource dictSource = DictSource::None;

	// || !decodeClientHello(payload, ch)
	if (!readFrame(conn, deadline, BytesView(), [&](const MessageHeader &, BytesView b) {
		ClientHello ch;

		auto networkBytes = BytesViewNetwork(b);

		ch.magic = networkBytes.readUnsigned32();
		ch.version = networkBytes.readUnsigned16();
		ch.authMode = networkBytes.readUnsigned();
		ch.clientHelloFlags = networkBytes.readUnsigned();

		auto authDataSize = networkBytes.readUnsigned16();
		auto dictSize = networkBytes.readUnsigned16();

		ch.authData = networkBytes.readBytes<sprt::endian::native>(authDataSize);
		ch.suggestedDict = networkBytes.readBytes<sprt::endian::native>(dictSize);

		outStatus = negotiateHello(ch, expectedKey, requireBearerKey);

		if (outStatus == GlobalError::Ok) {
			if (serverDict.empty() && !ch.suggestedDict.empty()) {
				dictSource = DictSource::Client;
				negotiatedDict = ch.suggestedDict.bytes<Interface>();
			} else if (!serverDict.empty()) {
				dictSource = DictSource::Server;
			}
		}
	})) {
		outStatus = GlobalError::BadProtocol;
	}

	writeServerHello(conn, outStatus, dictSource, serverDict, deadline);
	return outStatus;
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

// How long a refusal waits for the ClientHello before answering anyway. Short on purpose -- see
// serverHandshakeReject.
static constexpr uint64_t kRejectHelloWaitUs = 100'000;

GlobalError serverHandshakeReject(TransportConnection &conn, GlobalError status,
		uint64_t deadline) {
	/* Read the ClientHello and throw it away, THEN answer.
	 *
	 * This used to write immediately, on the reasoning that "QUIC's two directions are independent,
	 * so the refusal is delivered whether or not the hello has arrived". That is wrong, and the way
	 * it is wrong is invisible on any other transport. QUIC has no single byte pipe: it has streams,
	 * and this connection runs in OpenSSL's AUTO_BIDI default-stream mode, where each side's default
	 * stream is the FIRST one to exist. A client's default stream is the one it created by sending
	 * its hello. A server that writes before anything has arrived has no incoming stream to bind to,
	 * so it creates a server-initiated one instead -- and the client, reading its own default stream,
	 * never sees a byte of it. The refusal was written successfully, to a stream nobody was reading,
	 * and the peer learned of it by timing out: exactly the silence this function exists to replace.
	 *
	 * So the read is not a formality, it is what binds the default stream to the client's. The wait
	 * is short and separate from `deadline` because the original concern was real: a peer that
	 * connects and says nothing must not park this thread. A legitimate client's hello is already in
	 * flight when the connection completes, and one that misses the window is no worse off than
	 * before -- it times out, as it did for every refusal until now. */
	auto helloDeadline = sprt::min(deadline,
			sp::platform::clock(ClockType::Monotonic) + kRejectHelloWaitUs);
	readFrame(conn, helloDeadline, BytesView(), [](const MessageHeader &, BytesView) { });

	if (!writeServerHello(conn, status, DictSource::None, BytesView(), deadline)) {
		log::source().error("remote::Protocol", "failed to deliver the refusal (",
				getGlobalErrorName(status), "); the peer will only learn of it by timing out");
	}
	return status;
}

} // namespace stappler::xenolith::remote
