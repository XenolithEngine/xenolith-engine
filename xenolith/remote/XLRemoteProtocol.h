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

#ifndef XENOLITH_REMOTE_XLREMOTEPROTOCOL_H_
#define XENOLITH_REMOTE_XLREMOTEPROTOCOL_H_

#include "XLCommon.h"
#include "XLCoreInfo.h" // core::FrameConstraints

namespace STAPPLER_VERSIONIZED stappler::xenolith::remote {

// X11-like connection-setup protocol (the first wire messages). Setup messages are uncompressed and
// length-framed ([u32 len][payload]); subsequent data messages are LZ4-compressed with the
// dictionary negotiated here. Everything is network byte order.

constexpr uint32_t kProtocolMagic = 0x584C'5231; // 'XLR1'
constexpr uint16_t kProtocolVersion = 1;
constexpr uint32_t kBearerKeySize = 64;

enum class Role {
	Generic = 0,
	Server = 1,
	Client = 2,
};

// Single auth mode for now; the field is reserved so future modes (challenge/response, cert, ...)
// fit without a format change.
enum class AuthMode : uint8_t {
	None = 0,
	BearerKey = 1,
};

// Domain defines a subsets of messages and errors;
// Note that NetworkBackend (255, local-only) and NotImplemented (254) is common for any domain
enum class Domain : uint8_t {
	Global = 0,
	Window = 1,
	Data = 2, // large binary block transfer (announce + chunked packets); see DataCode below
	Error = 255,
};

enum class GlobalCode {
	ClientHello = 0,
	ServerHello = 1,
	Ping = 2,
	Pong = 3,
	SharedObjectsAnnounce = 4,
};

enum class GlobalError : uint8_t {
	Ok = 0,
	BadProtocol = 2, // magic/version mismatch or malformed
	UnsupportedAuth = 3, // unknown auth mode
	AuthFailed = 4, // bearer key mismatch / no server key configured
	NotImplemented = 254,
	NetworkBackend = 255, // not protocol-related, check backend error reporting
};

enum class WindowCode {
	CompileQueue = 0,
	UpdateMaterials = 1, // server -> client push: a shared queue's MaterialSet changed
	AcquireFrame = 2, // server -> client request: produce a frame; reply selects a render queue
	FrameInput = 3, // client -> server: one serialized per-attachment input for a frame
	FrameCommit = 4, // client -> server: all inputs for a frame have been submitted
	AttachQueue = 5, // client -> server request [windowId]: the client compiled the shared queue and
					 // attached it to its Director; only on this sync does the server route the
					 // window's frames to the client (so AcquireFrame can't arrive before the client
					 // is ready). Reply is an empty, atomic acknowledgement.
	ReadyForNextFrame = 6, // client -> server notification [windowId]: the client's scene has active
						   // actions/input and wants another frame; the server schedules the next
						   // frame on the window's PresentationEngine. Fire-and-forget (no reply).
	RequestScreenshot = 7, // client -> server notification [windowId]: capture the window's current
						   // contents and hand them back over Domain::Data (a Screenshot transfer whose
						   // announce `reason` points back at this message). Fire-and-forget (no reply);
						   // the captured pixels arrive asynchronously as a block transfer.
};

enum class WindowError : uint8_t {
	Ok = 0,
	InvalidObjecthandle = 1,
	SerializationFailed = 2,
	FrameRejected = 3, // client could not produce/select a frame for an AcquireFrame request
	NotImplemented = 254,
	NetworkBackend = 255, // not protocol-related, check backend error reporting
};

// Domain::Data: move a large opaque binary blob in either direction and reference it later by id.
//
// Lifecycle (the "sender" is whoever offers the data; works client->server AND server->client):
//   Announce  (REQUEST, CBOR header) -- sender offers a blob: id, type, total size, packet count,
//             packet size and a per-packet hash. The receiver agrees by replying without error (a
//             mirror reply == accept) or declines with an error reply.
//   Packet    (notification, RAW BINARY -- not CBOR) -- one chunk: [u64 id][u32 index][chunk bytes];
//             the transport LZ4 in sendFrame supplies the compression. Streamed back-to-back.
//   Complete  (notification) -- receiver: all packets arrived and every per-packet hash matched.
//   Release   (notification) -- sender: it will no longer reference the blob; the receiver may drop it.
//   Unavailable(notification) -- receiver: it can no longer hold the blob (eviction, even after a
//             Release race); the sender must stop referencing that id.
enum class DataCode : uint8_t {
	Announce = 0,
	Packet = 1,
	Complete = 2,
	Release = 3,
	Unavailable = 4,
};

enum class DataError : uint8_t {
	Ok = 0,
	Declined = 1, // receiver refuses the offer (policy / capacity); the Announce error-reply code
	TooLarge = 2, // total size or packet size exceeds the agreed limits
	BadHeader = 3, // malformed/inconsistent Announce header
	HashMismatch = 4, // a received packet failed its announced hash
	UnknownTransfer = 5, // a Packet/Complete/Release/Unavailable referenced an unknown id
	NotImplemented = 254,
	NetworkBackend = 255, // not protocol-related, check backend error reporting
};

// What a transferred blob carries; lets the receiver's accept policy and consumer route it. The
// announce also carries an opaque `meta` (type-specific, e.g. image w/h/format) and a `reason`
// (which message/type triggered the transfer), so the two id spaces stay decoupled from semantics.
enum class DataType : uint16_t {
	Generic = 0,
	Screenshot = 1, // raw window pixels; meta = {fmt, w, h, d}; receiver saves via core::saveImage
};

// Default chunk size. Must stay <= kMaxFrameSize and small enough that one packet is a modest frame:
// the receiver validates the announced packet size against this, which (since every packet's
// decompressed size is then bounded) is the real safeguard against a decompression bomb on this
// domain -- the transport keeps only absolute (non-ratio) frame caps, so a strongly-compressed
// screenshot packet is never rejected for its ratio.
constexpr uint32_t kRecommendedPacketSize = 32u * 1024; // 32 KiB
constexpr uint32_t kMaxBlockTransferSize = 256u * 1024 * 1024; // whole-blob policy ceiling

// Which side's compression dictionary won negotiation. Server has priority; if it has none the
// client's suggestion (if any) is used; otherwise no dictionary.
enum class DictSource : uint8_t {
	None = 0,
	Server = 1,
	Client = 2,
};

enum class ClientHelloFlags : uint8_t {
	None = 0,
};

enum class MessageType : uint8_t {
	Generic = toInt(Role::Generic),
	Server = toInt(Role::Server),
	Client = toInt(Role::Client),
	ServerReply = 4, // Server replies for a client request, serial is a number of that request
	ClientReply = 5, // Client replies for a server request, serial is a number of that request
	ServerError = 6, // Server replies with a error, serial is a number of request, code is error
	ClientError = 7, // Client replies with a error, serial is a number of request, code is error
};

// Map a Role onto the matching MessageType for a request / reply / error. The offsets follow the
// MessageType layout above: Server=1/Client=2, ServerReply=4/ClientReply=5, ServerError=6/ClientError=7
// -- i.e. reply is role+3 and error is role+5 (there is a reserved gap at value 3). Getting these wrong
// mistypes messages so the peer's isReply()/isError() routing misclassifies them.
static inline constexpr MessageType MessageTypeRequest(Role role) {
	return MessageType(toInt(role));
}

static inline constexpr MessageType MessageTypeReply(Role role) {
	return MessageType(toInt(role) + 3);
}

static inline constexpr MessageType MessageTypeError(Role role) {
	return MessageType(toInt(role) + 5);
}

static_assert(MessageTypeRequest(Role::Server) == MessageType::Server
		&& MessageTypeRequest(Role::Client) == MessageType::Client
		&& MessageTypeReply(Role::Server) == MessageType::ServerReply
		&& MessageTypeReply(Role::Client) == MessageType::ClientReply
		&& MessageTypeError(Role::Server) == MessageType::ServerError
		&& MessageTypeError(Role::Client) == MessageType::ClientError,
		"MessageType role mapping is out of sync with the MessageType enum");

enum class MessageFlags : uint8_t {
	None = 0,
	Compressed = 1 << 0,
	Dictionary = 1 << 1,
};

struct MessageHeader {
	uint8_t msgtype;
	uint8_t msgflags;
	uint8_t domain;
	uint8_t code;
	uint32_t serial;
	uint32_t size;
};

static inline constexpr bool isReply(const MessageHeader &h) {
	return h.msgtype > 3 && h.msgtype <= 5;
}

static inline constexpr bool isReplyOrError(const MessageHeader &h) {
	return h.msgtype > 3 && h.msgtype <= 7;
}

static inline constexpr bool isError(const MessageHeader &h) {
	return h.msgtype > 5 && h.msgtype <= 7;
}

// Client Hello
// 0 - 3	kProtocolMagic
// 4 - 5	Version
// 6		AuthMode
// 7		ClientHelloFlags
// 8 - 9	AuthDataSize (a)
// 10 - 11	DictDataSize (d)
// <12, 12 + a>	AuthData (d)
// <12 + a + 1, 12 + a + 1 + d>	DictData

struct ClientHello {
	uint32_t magic = 0;
	uint16_t version = kProtocolVersion;
	uint8_t authMode = 0;
	uint8_t clientHelloFlags = 0;

	BytesView authData; // bearer key (kBearerKeySize bytes)
	BytesView suggestedDict; // optional LZ4 dictionary the client offers (may be empty)
};

struct ServerHello {
	uint32_t magic = 0;
	uint16_t version = kProtocolVersion;
	uint8_t status = 0;
	uint8_t dictSource = 0;
	BytesView dict; // dictionary bytes when dictSource == Server (else empty)
};

// Shared 64-byte development bearer key (both demo client and server use this by default).
SP_PUBLIC BytesView getDevBearerKey();

// --- stream I/O over a QUIC connection (the SSL* is passed as void* to keep OpenSSL out of this
// header). All reads are bounded by an absolute wall-clock deadline in microseconds
// (sp::platform::clock(Monotonic) timebase). ---

// Message framing: a 12-byte MessageHeader followed by `size` payload bytes (uncompressed). readFrame
// reads one full message bounded by `deadline` and invokes cb(header, payload) (header fields already
// converted to host byte order). Returns false on close/timeout/oversize.

SP_PUBLIC bool readMessagePayload(BytesViewNetwork &view, BytesView dict,
		const Callback<void(const MessageHeader &, BytesView)> &);

SP_PUBLIC bool readFrame(void *ssl, uint64_t deadline, BytesView dict,
		const Callback<void(const MessageHeader &, BytesView payload)> &cb);

SP_PUBLIC bool sendFrame(void *ssl, uint64_t deadline, BytesView dict, MessageType, Domain,
		uint8_t msg, uint32_t serial, BytesView);

// Receive-side stream reassembler. A QUIC stream is an ordered byte stream with no message
// boundaries, so a socket wakeup yields an arbitrary number of bytes -- possibly a partial frame, or
// several frames at once. MessageReader buffers raw bytes until whole `[MessageHeader][size payload]`
// frames are present (payload LZ4-decompressed per the header flags), then dispatches them.
//
// Dispatch is xcb-style: each pending message is offered to the handler, which returns true to
// consume it or false to leave it queued for a later dispatch. This lets a side handle replies/events
// out of order by serial -- a reply that can't be consumed yet (its requester isn't ready) is simply
// deferred and retried, while later messages are processed.
class SP_PUBLIC MessageReader {
public:
	// Append raw stream bytes (from SSL_read_ex). Every newly-complete frame is decoded (decompressed
	// with `dict` when flagged) and queued. Returns false on a framing violation (oversize frame or a
	// compressed payload that won't decode) -- the caller should drop the connection.
	bool append(BytesView raw, BytesView dict);

	void addMessage(const MessageHeader &, BytesView);

	// Offer each pending message to `cb` (header fields in host byte order). `cb` returns true to
	// consume the message, false to keep it for later. Deferred messages are retried within this call
	// until no further progress is made, then again on subsequent dispatch() calls.
	void dispatch(const Callback<bool(const MessageHeader &, BytesView)> &cb);

	bool hasPartialMessage() const { return !_buffer.empty(); }

	bool hasPending() const { return !_pending.empty(); }
	size_t pendingCount() const { return _pending.size(); }
	void clear();

protected:
	struct Message {
		MessageHeader header; // serial/size already in host byte order
		Bytes payload; // decoded (decompressed) payload
	};

	Bytes _buffer; // stream bytes not yet forming a complete frame
	Vector<Message> _pending; // complete messages awaiting a successful dispatch
};

// Client side: send ClientHello (bearer key + suggested dict), read ServerHello into `out`, and fill
// `negotiatedDict` with the dictionary to use for subsequent data frames. Returns true iff
// out.status == Ok.
SP_PUBLIC GlobalError clientHandshake(void *ssl, BytesView bearerKey, BytesView dict,
		uint64_t deadlineUs, const Callback<void(const ServerHello &out)> &);

// Server side: read ClientHello, validate (magic/version/mode + constant-time key compare against
// `expectedKey`), negotiate the dictionary (server priority, else client suggestion, else none),
// and reply with ServerHello (window info on success). Fills `outStatus` and `negotiatedDict`.
// Returns true iff authenticated.
SP_PUBLIC GlobalError serverHandshake(void *ssl, BytesView expectedKey, BytesView serverDict,
		Bytes &negotiatedDict, uint64_t deadlineUs);

// Ping request
SP_PUBLIC GlobalError sendPing(void *ssl, Role, uint32_t serial);

// Ping response
SP_PUBLIC GlobalError sendPong(void *ssl, Role, uint32_t serial);

} // namespace stappler::xenolith::remote

#endif /* XENOLITH_REMOTE_XLREMOTEPROTOCOL_H_ */
