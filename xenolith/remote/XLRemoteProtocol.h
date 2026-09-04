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
#include "XLRemoteTransport.h"

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
	Font = 3, // font source sync + glyph rasterization requests; see FontCode below
	Error = 255,
};

enum class GlobalCode {
	ClientHello = 0,
	ServerHello = 1,
	Ping = 2,
	Pong = 3,
	SharedObjectsAnnounce = 4,

	// Who each side is: CBOR PeerInfo (XLRemotePeerInfo.h). A REQUEST the server sends immediately
	// after the handshake and BEFORE it announces anything; the client answers with its own PeerInfo
	// in the reply, or refuses with GlobalError::IncompatiblePeer. Nothing is shared until that
	// exchange completes -- the point of it is to stop a build mismatch before the first raw struct
	// dump, not to report one afterwards.
	//
	// One request/reply rather than two independent notifications: both directions are checked, and
	// the server has an answer before it announces rather than a message it hopes arrived.
	//
	// A version-1 peer that predates this code answers with a NotImplemented error, and the session
	// continues exactly as it did before -- which is what makes this an extension of version 1 and
	// not a new version.
	ServerInfo = 5,
};

enum class GlobalError : uint8_t {
	Ok = 0,
	BadProtocol = 2, // magic/version mismatch or malformed
	UnsupportedAuth = 3, // unknown auth mode
	AuthFailed = 4, // bearer key mismatch / no server key configured
	Busy = 5, // the server's client slot is taken; it accepts no second connection right now
	IncompatiblePeer = 6, // the peer's ABI tag differs: raw struct dumps between these two builds
	// would be memory corruption, not a protocol error (see PeerInfo::abi)
	NotImplemented = 254,
	NetworkBackend = 255, // not protocol-related, check backend error reporting
};

enum class WindowCode {
	CompileQueue = 0,
	UpdateMaterials = 1, // server -> client push: a shared queue's MaterialSet changed
	AcquireFrame = 2, // server -> client request: produce a frame; reply selects a render queue
	FrameInput = 3, // client -> server: one serialized per-attachment input for a frame
	FrameCommit = 4, // client -> server: all inputs for a frame have been submitted
	AttachQueue =
			5, // client -> server request [windowId]: the client compiled the shared queue and
	// attached it to its Director; only on this sync does the server route the
	// window's frames to the client (so AcquireFrame can't arrive before the client
	// is ready). Reply is an empty, atomic acknowledgement.
	ReadyForNextFrame =
			6, // client -> server notification [windowId]: the client's scene has active
	// actions/input and wants another frame; the server schedules the next
	// frame on the window's PresentationEngine. Fire-and-forget (no reply).
	RequestScreenshot = 7, // client -> server notification [windowId]: capture the window's current
	// contents and hand them back over Domain::Data (a Screenshot transfer whose
	// announce `reason` points back at this message). Fire-and-forget (no reply);
	// the captured pixels arrive asynchronously as a block transfer.
	CompileMaterials =
			8, // client -> server notification: compile a runtime material (e.g. a font atlas
	// material) the headless client cannot compile itself. Carries the window id, the
	// client-assigned MaterialIds + pipelines + image refs, and the gating dependency
	// ids; the server resolves the images (the atlas image id -> its DynamicImage),
	// compiles into the window's MaterialSet, signals the deps, and pushes the set
	// back via UpdateMaterials. Fire-and-forget (the push + gating carry the result).
	InputEvents =
			9, // server -> client notification: platform input + window-state events for a window.
	// RAW BINARY (not CBOR): [u64 windowId (network order)][InputEventData[] native layout].
	// The server owns the OS window, so input originates there; it ships the same
	// core::InputEventData batch the local Director would receive. InputEventData is
	// trivially copyable, so the batch travels as an opaque blob (client/server share one
	// build/ABI). Fire-and-forget; the client replays the batch into its Director's scene.
	UpdateLayers =
			10, // client -> server notification: the window's interaction layers (hit/cursor/drag
	// regions) for the OS window. RAW BINARY (not CBOR): [u64 windowId (network order)]
	// [WindowLayer[] native layout]. The reverse of InputEvents: the client's scene graph
	// computes the layers (InputDispatcher) but the server owns the real window, so the
	// client forwards them and the server applies them to the native window (cursor,
	// hit-testing, server-side decorations). WindowLayer is trivially copyable -> opaque
	// blob. Fire-and-forget; the client only sends on change.
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
	Font = 2, // a large font-file blob; meta = {contentHash}; the receiver pins it in its font store
};

// Default chunk size. Must stay <= kMaxFrameSize and small enough that one packet is a modest frame:
// the receiver validates the announced packet size against this, which (since every packet's
// decompressed size is then bounded) is the real safeguard against a decompression bomb on this
// domain -- the transport keeps only absolute (non-ratio) frame caps, so a strongly-compressed
// screenshot packet is never rejected for its ratio.
constexpr uint32_t kRecommendedPacketSize = 64u * 1'024; // 64 KiB
constexpr uint32_t kMaxBlockTransferSize = 512u * 1'024 * 1'024; // whole-blob policy ceiling

// Domain::Font: split the font system between a headless client (glyph positioning) and the GPU
// server (rasterization + atlas). The client owns its own FontLibrary for metrics and mints FaceIds
// locally; the server adopts those ids and rasterizes into a per-connection atlas. Font *data* is
// content-addressed (a hash of the file bytes) and stored persistently on the server, so a font the
// server already holds is never re-sent.
//
//   SourcesAnnounce (REQUEST, CBOR) -- client lists families/aliases/sources, each source tagged with
//             its contentHash. Reply SourcesReady names the hashes the server is still missing (plus
//             the atlas image's server object id); an error reply means the announce was rejected.
//   FontInline (notification, CBOR {contentHash, bytes}) -- a small missing font shipped inline; large
//             ones come over Domain::Data (DataType::Font, meta={contentHash}).
//   GlyphRequest (notification, RAW BINARY) -- [u32 depId][faces...] asks the server to rasterize a set
//             of (contentHash, spec, faceId) glyphs; depId gates the frame that uses them.
//   AtlasReady (notification, CBOR {depId, ok}) -- the server finished the atlas update for depId.
//   CompileImage (REQUEST) -- reserved; the client never GPU-compiles, so the server just acks.
enum class FontCode : uint8_t {
	SourcesAnnounce = 0,
	SourcesReady = 1,
	FontInline = 2,
	GlyphRequest = 3,
	AtlasReady = 4,
	CompileImage = 5,
};

enum class FontError : uint8_t {
	Ok = 0,
	SourcesNotReady = 1, // a GlyphRequest arrived before the source handshake completed
	UnknownFont = 2, // a request referenced a contentHash the server does not hold
	UnknownFace = 3, // a request referenced an unknown (contentHash, spec) face
	NotImplemented = 254,
	NetworkBackend = 255, // not protocol-related, check backend error reporting
};

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

#if DEBUG
// Shared 64-byte development bearer key (both demo client and server use this by default).
//
// Debug-only on purpose: the value is a fixed pattern computed from a constant, so a release build
// that shipped it would present -- and accept -- a key every reader of this header already knows.
// A release build must be handed a real key (AppThread::setBearerKey / ClientContext::setBearerKey).
SP_PUBLIC BytesView getDevBearerKey();
#endif

// --- stream I/O over a TransportConnection. All reads are bounded by an absolute wall-clock
// deadline in microseconds (sp::platform::clock(Monotonic) timebase). ---

// Message framing: a 12-byte MessageHeader followed by `size` payload bytes (uncompressed). readFrame
// reads one full message bounded by `deadline` and invokes cb(header, payload) (header fields already
// converted to host byte order). Returns false on close/timeout/oversize.

SP_PUBLIC bool readMessagePayload(BytesViewNetwork &view, BytesView dict,
		const Callback<void(const MessageHeader &, BytesView)> &);

SP_PUBLIC bool readFrame(TransportConnection &, uint64_t deadline, BytesView dict,
		const Callback<void(const MessageHeader &, BytesView payload)> &cb);

// Frame one message (header + optionally-compressed payload) onto the end of `out`. The wire bytes
// are exactly what sendFrame would have written.
SP_PUBLIC void encodeFrame(Bytes &out, BytesView dict, MessageType, Domain, uint8_t msg,
		uint32_t serial, BytesView payload);

// Send-side buffer.
//
// sendFrame BLOCKS the calling thread: when the QUIC send buffer or the peer's flow-control window
// is full, SSL_write_ex accepts nothing and it busy-waits in 1ms sleeps up to its deadline. On the
// app thread that stalls the frame the scene is building, and the peer decides for how long.
//
// So a message is framed into this queue instead and drained non-blockingly from the connection's
// poll(), which the looper already drives on socket readiness and on every update tick. Ordering is
// FIFO, which is what the protocol needs; the handshake stays synchronous on purpose (it runs once,
// before any frame, and its deadline is the point).
class SP_PUBLIC OutgoingQueue {
public:
	// A peer that never drains must not grow our memory without bound. Past this the connection is
	// not stalled, it is dead, and the caller should drop it.
	static constexpr size_t kMaxPending = 64u * 1'024 * 1'024;

	// Frame a message and append it. Never blocks. False once the queue is over kMaxPending.
	bool push(BytesView dict, MessageType, Domain, uint8_t code, uint32_t serial, BytesView payload);

	// Write as much as the transport accepts right now. A partial write is success -- the remainder
	// waits for the next call. False only on a fatal write error, i.e. drop the connection.
	bool flush(TransportStream *);

	bool empty() const { return _offset >= _buffer.size(); }
	size_t pending() const { return _buffer.size() - _offset; }
	void clear();

protected:
	Bytes _buffer;
	size_t _offset = 0; // how much of _buffer the transport has already taken
};

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
SP_PUBLIC GlobalError clientHandshake(TransportConnection &, BytesView bearerKey, BytesView dict,
		uint64_t deadlineUs, const Callback<void(const ServerHello &out)> &);

// Server side: read ClientHello, validate (magic/version/mode + constant-time key compare against
// `expectedKey`), negotiate the dictionary (server priority, else client suggestion, else none),
// and reply with ServerHello (window info on success). Fills `outStatus` and `negotiatedDict`.
// Returns true iff authenticated.
// `requireBearerKey` false accepts the client whatever key it presents. Pass false only when the
// TRANSPORT already established who the peer is (TransportCaps::PeerAuthenticated -- unix-domain
// credentials), where the key would add nothing: the kernel's answer is stronger than a shared
// secret, and access control is the socket's permissions.
SP_PUBLIC GlobalError serverHandshake(TransportConnection &, BytesView expectedKey,
		BytesView serverDict, Bytes &negotiatedDict, uint64_t deadlineUs,
		bool requireBearerKey = true);

// Server side: answer a connection with `status` instead of negotiating -- the way to turn one away
// (GlobalError::Busy) so the peer gets a real answer rather than waiting out its own handshake
// deadline. The ClientHello is not read (see the .cc), so this only ever blocks on the write.
// Returns `status`; the caller closes the connection afterwards.
SP_PUBLIC GlobalError serverHandshakeReject(TransportConnection &, GlobalError status,
		uint64_t deadlineUs);


} // namespace stappler::xenolith::remote

#endif /* XENOLITH_REMOTE_XLREMOTEPROTOCOL_H_ */
