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
#include <openssl/ssl.h>

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

BytesView getDevBearerKey() {
	static uint8_t key[kBearerKeySize];
	static bool inited = [] {
		for (uint32_t i = 0; i < kBearerKeySize; ++i) { key[i] = uint8_t(0xA5 ^ (i * 7 + 13)); }
		return true;
	}();
	(void)inited;
	return BytesView(key, kBearerKeySize);
}

// --- stream I/O + handshake ---

// Cap on a single frame to bound allocations from a hostile/garbled peer.
static constexpr uint32_t kMaxFrameSize = 64u * 1'024 * 1'024;

// Read exactly n bytes (bounded by an absolute deadline); non-blocking-friendly (pumps QUIC events).
static bool streamReadFull(SSL *ssl, uint8_t *buf, size_t n, uint64_t deadline) {
	size_t got = 0;
	while (got < n) {
		size_t r = 0;
		if (SSL_read_ex(ssl, buf + got, n - got, &r) == 1) {
			got += r;
			continue;
		}
		int err = SSL_get_error(ssl, 0);
		if (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE) {
			return false; // closed or fatal
		}
		SSL_handle_events(ssl);
		if (sp::platform::clock(ClockType::Monotonic) >= deadline) {
			return false;
		}
		sp::platform::sleep(1'000); // 1ms
	}
	return true;
}

// Write exactly n bytes (bounded by an absolute deadline); non-blocking-friendly. SSL_write_ex queues
// into the QUIC stream send buffer; under backpressure (buffer full / peer flow-control window
// exhausted) it returns WANT_WRITE having accepted nothing, so we pump events and retry until the
// deadline rather than failing mid-message (which would truncate the frame on the peer).
static bool streamWriteAll(SSL *ssl, const uint8_t *buf, size_t n, uint64_t deadline) {
	size_t off = 0;
	while (off < n) {
		size_t w = 0;
		if (SSL_write_ex(ssl, buf + off, n - off, &w) == 1) {
			off += w;
			continue;
		}
		int err = SSL_get_error(ssl, 0);
		if (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE) {
			return false; // closed or fatal
		}
		SSL_handle_events(ssl);
		if (sp::platform::clock(ClockType::Monotonic) >= deadline) {
			return false;
		}
		sp::platform::sleep(1'000); // 1ms
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

	auto csize = view.readUnsigned32();
	if (csize > kMaxFrameSize * 4) {
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

bool readFrame(void *ssl, uint64_t deadline, BytesView dict,
		const Callback<void(const MessageHeader &, BytesView payload)> &cb) {
	auto s = (SSL *)ssl;

	MessageHeader h;
	if (!streamReadFull(s, (uint8_t *)&h, sizeof(MessageHeader), deadline)) {
		return false;
	}

	h.serial = sprt::byteorder::NetworkToHost(h.serial);
	h.size = sprt::byteorder::NetworkToHost(h.size);

	if (h.size > kMaxFrameSize) {
		return false;
	}

	auto buf = __sprt_typed_malloca(uint8_t, h.size);

	if (h.size > 0 && !streamReadFull(s, buf, h.size, deadline)) {
		__sprt_freea(buf);
		return false;
	}

	auto frameData = BytesViewNetwork(buf, h.size);

	bool success = readMessagePayloadWithHeader(frameData, h, dict, cb);

	__sprt_freea(buf);

	return success;
}

SP_PUBLIC bool sendFrame(void *ssl, uint64_t deadline, BytesView dict, MessageType t, Domain d,
		uint8_t msg, uint32_t serial, BytesView payload) {
	auto s = (SSL *)ssl;

	if (payload.empty()) {
		MessageHeader mh;
		mh.msgtype = toInt(t);
		mh.msgflags = 0;
		mh.domain = toInt(d);
		mh.code = msg;
		mh.serial = sprt::byteorder::HostToNetwork(serial);
		mh.size = 0;
		return streamWriteAll(s, (const uint8_t *)&mh, sizeof(MessageHeader), deadline);
	}

	uint32_t frameSize = 0;
	bool usingDict = true;

	uint32_t bound = sprt::lz4_getCompressBounds(payload.size());
	if (bound == 0) {
		return false;
	}

	auto frameData = __sprt_typed_malloca(uint8_t, sizeof(MessageHeader) + 4 + bound);

	MessageHeader *mh = (MessageHeader *)frameData;
	mh->msgtype = toInt(t);
	mh->msgflags = 0;
	mh->domain = toInt(d);
	mh->code = msg;
	mh->serial = sprt::byteorder::HostToNetwork(serial);

	uint32_t clen = 0;
	if (!dict.empty()) {
		clen = sprt::lz4_compressDataDict(payload.data(), payload.size(),
				frameData + sizeof(MessageHeader) + 4, bound, dict.data(), dict.size());
		if (clen > 0) {
			usingDict = true;
		}
	}

	if (clen == 0) {
		clen = sprt::lz4_compressData(payload.data(), payload.size(),
				frameData + sizeof(MessageHeader) + 4, bound);
	}

	if (clen + 4 < payload.size()) {
		mh->msgflags = toInt(MessageFlags::Compressed);
		if (usingDict) {
			mh->msgflags |= toInt(MessageFlags::Dictionary);
		}
		mh->size = sprt::byteorder::HostToNetwork(clen + 4);

		// write uncompressed size
		*(uint32_t *)(frameData + sizeof(MessageHeader)) = uint32_t(payload.size());

		frameSize = sizeof(MessageHeader) + 4 + clen;
	} else {
		mh->msgflags = 0;
		mh->size = sprt::byteorder::HostToNetwork(uint32_t(payload.size()));
		frameSize = sizeof(MessageHeader) + payload.size();
		memcpy(frameData + sizeof(MessageHeader), payload.data(), payload.size());
	}

	auto result = streamWriteAll(s, frameData, frameSize, deadline);

	__sprt_freea(frameData);

	return result;
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
	if (rawSize > kMaxFrameSize) {
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

	if (h.status != 0) {
		return true;
	}

	if (h.dictSource == toInt(DictSource::Server)) {
		auto s = in.readUnsigned16();
		h.dict = BytesView(in.readBytes(s));
	}
	return true;
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

GlobalError clientHandshake(void *ssl, BytesView key, BytesView dict, uint64_t deadline,
		const Callback<void(const ServerHello &out)> &cb) {
	uint32_t clientHelloSize = sizeof(uint32_t) * 3 + key.size() + dict.size();

	auto d = __sprt_typed_malloca(uint8_t, clientHelloSize + sizeof(MessageHeader));

	auto buf = writeMessageHeader(d, MessageType::Client, MessageFlags::None, Domain::Global,
			toInt(GlobalCode::ClientHello), 0, clientHelloSize);

	buf = writeValue32(buf, sprt::byteorder::HostToNetwork(kProtocolMagic));
	buf = writeValue16(buf, sprt::byteorder::HostToNetwork(kProtocolVersion));
	buf = writeValue8(buf, toInt(AuthMode::BearerKey));
	buf = writeValue8(buf, toInt(ClientHelloFlags::None));
	buf = writeValue16(buf, sprt::byteorder::HostToNetwork(static_cast<uint16_t>(key.size())));
	buf = writeValue16(buf, sprt::byteorder::HostToNetwork(static_cast<uint16_t>(dict.size())));
	buf = writeData(buf, key);
	if (!dict.empty()) {
		buf = writeData(buf, dict);
	}

	if (!streamWriteAll((SSL *)ssl, d, clientHelloSize + sizeof(MessageHeader), deadline)) {
		return GlobalError::NetworkBackend;
	}

	__sprt_freea(d);

	GlobalError result = GlobalError::NetworkBackend;

	if (!readFrame(ssl, deadline, BytesView(), [&](const MessageHeader &h, BytesView d) {
		ServerHello sh;
		if (!decodeServerHello(d, sh)) {
			result = GlobalError::NetworkBackend;
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

static GlobalError negotiateHello(const ClientHello &ch, BytesView expectedKey) {
	GlobalError status;
	if (ch.version != kProtocolVersion) {
		status = GlobalError::BadProtocol;
	} else if (ch.authMode != toInt(AuthMode::BearerKey)) {
		status = GlobalError::UnsupportedAuth;
	} else if (expectedKey.empty()
			|| !crypto::isEqualConstantTime(BytesView(ch.authData.data(), ch.authData.size()),
					expectedKey)) {
		auto exp = base16::encode<Interface>(expectedKey);
		auto data = base16::encode<Interface>(ch.authData);
		status = GlobalError::AuthFailed;
	} else {
		status = GlobalError::Ok;
	}
	return status;
}

GlobalError serverHandshake(void *ssl, BytesView expectedKey, BytesView serverDict,
		Bytes &negotiatedDict, uint64_t deadline) {
	negotiatedDict.clear();

	GlobalError outStatus = GlobalError::BadProtocol;
	DictSource dictSource = DictSource::None;

	// || !decodeClientHello(payload, ch)
	if (!readFrame(ssl, deadline, BytesView(), [&](const MessageHeader &, BytesView b) {
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

		outStatus = negotiateHello(ch, expectedKey);

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

	// write short error message
	size_t serverHello = sizeof(uint32_t) * 2;
	if (outStatus == GlobalError::Ok && dictSource == DictSource::Server) {
		serverHello += sizeof(uint16_t) + serverDict.size();
	}

	auto d = __sprt_typed_malloca(uint8_t, serverHello + sizeof(MessageHeader));

	auto buf = writeMessageHeader(d, MessageType::Server, MessageFlags::None, Domain::Global,
			toInt(GlobalCode::ServerHello), 0, serverHello);

	buf = writeValue32(buf, sprt::byteorder::HostToNetwork(kProtocolMagic));
	buf = writeValue16(buf, sprt::byteorder::HostToNetwork(kProtocolVersion));
	buf = writeValue8(buf, toInt(outStatus));
	buf = writeValue8(buf, toInt(dictSource));

	if (outStatus == GlobalError::Ok && dictSource == DictSource::Server) {
		buf = writeValue16(buf, uint16_t(serverDict.size()));
		buf = writeData(buf, serverDict);
	}

	streamWriteAll((SSL *)ssl, d, serverHello + sizeof(MessageHeader), deadline);
	__sprt_freea(buf);
	return outStatus;
}

SP_PUBLIC GlobalError sendPing(void *ssl, Role role, uint32_t serial) {
	MessageHeader buf = {0};

	writeMessageHeader((uint8_t *)&buf, MessageTypeRequest(role), MessageFlags::None,
			Domain::Global, toInt(GlobalCode::Ping), serial, 0);

	if (streamWriteAll((SSL *)ssl, (uint8_t *)&buf, sizeof(MessageHeader),
				sp::platform::clock(ClockType::Monotonic) + 500'000)) {
		return GlobalError::Ok;
	}
	return GlobalError::NetworkBackend;
}

SP_PUBLIC GlobalError sendPong(void *ssl, Role role, uint32_t serial) {
	MessageHeader buf = {0};

	writeMessageHeader((uint8_t *)&buf, MessageTypeReply(role), MessageFlags::None, Domain::Global,
			toInt(GlobalCode::Pong), serial, 0);

	if (streamWriteAll((SSL *)ssl, (uint8_t *)&buf, sizeof(MessageHeader),
				sp::platform::clock(ClockType::Monotonic) + 500'000)) {
		return GlobalError::Ok;
	}
	return GlobalError::NetworkBackend;
}

} // namespace stappler::xenolith::remote
