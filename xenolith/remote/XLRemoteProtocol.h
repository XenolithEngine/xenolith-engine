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

/* 'XLRP' -- version-neutral: the magic only identifies the protocol, so a peer of another version
 * still reaches the version field and gets a proper status. */
constexpr uint32_t kProtocolMagic = 0x584C'5250; // 'XLRP'

/* Version 2: InputEvents and UpdateLayers use the typed format (see XLRemoteSerialize.h).
 * Version 3: a 2d frame input carries each data set's identity, the command flags and the state
 * extension (gradient, shaded outline) - see FrameContextHandle2d::serialize.
 * Older peers are refused at the handshake; no compatibility is kept. */
constexpr uint16_t kProtocolVersion = 3;
constexpr uint32_t kBearerKeySize = 64;

// Size of one record in the typed input/layer batches (WindowCode::InputEvents / ::UpdateLayers).
// Layouts are documented with the codec in XLRemoteSerialize.h; peer info reports these sizes.
constexpr uint16_t kInputEventRecordSize = 40;
constexpr uint16_t kWindowLayerRecordSize = 24;

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

// Which transport stream a domain's messages ride. Without TransportCaps::MultiStream every class
// folds onto one stream.
//
// Split by ordering requirement, not size:
//   Control -- Global, Window, Font. Their relative order matters: SharedObjectsAnnounce precedes
//     the first AcquireFrame for a new window, AttachQueue precedes the frames it enables, and a
//     frame's Domain::Font glyph requests precede its FrameInput (XLRemoteWindow.cc) so the server
//     registers the gating dependency first. Large font payloads travel as Domain::Data.
//   Bulk -- Data. Order-independent; kept apart to avoid head-of-line blocking of input.
//
// StreamClass::Frame is unused: separating frames from input needs the "input then frame" ordering
// to be enforced explicitly; until then it folds onto Control.
constexpr StreamClass streamClassForDomain(Domain d) {
	switch (d) {
	case Domain::Data: return StreamClass::Bulk; break;
	default: break;
	}
	return StreamClass::Control;
}

enum class GlobalCode {
	ClientHello = 0,
	ServerHello = 1,
	Ping = 2,
	Pong = 3,
	SharedObjectsAnnounce = 4,

	// Who each side is: CBOR PeerInfo (XLRemotePeerInfo.h). A request the server sends right after
	// the handshake, before it announces anything; the client replies with its own PeerInfo or
	// refuses with GlobalError::IncompatiblePeer. A NotImplemented answer continues the session
	// without peer info.
	ServerInfo = 5,

	// Application messages, in either direction: the payload is any CBOR Value the two applications
	// agree on; the engine only carries it (PeerFeatures::AppMessages says a handler is installed).
	// A request is answered with a CBOR Value, or with GlobalError::NotImplemented when the receiver
	// has no handler; a notification is not answered.
	AppRequest = 6,
	AppNotify = 7,
};

enum class GlobalError : uint8_t {
	Ok = 0,
	BadProtocol = 2, // magic/version mismatch or malformed
	UnsupportedAuth = 3, // unknown auth mode
	AuthFailed = 4, // bearer key mismatch / no server key configured
	Busy = 5, // the server's client slot is taken; it accepts no second connection right now
	IncompatiblePeer = 6, // a peer may still send it; a differing wire-contract tag is only
	// reported, not refused (see PeerInfo::abi)
	NotImplemented = 254,
	NetworkBackend = 255, // not protocol-related, check backend error reporting
};

/* --- what this build knows how to receive -------------------------------------------------------
 *
 * One bit per message code, one mask per domain, exchanged in PeerInfo (XLRemotePeerInfo.h), so a
 * peer can check before sending and logs can report what the other side is missing.
 *
 * Build-level, not role-level: a bit means "my dispatcher has a case for this code". Maintained by
 * hand beside the enums; tests/remote pins the contents.
 */
constexpr uint64_t codeBit(uint8_t code) { return code < 64 ? (uint64_t(1) << code) : 0; }

template <typename Code>
constexpr uint64_t codeBit(Code c) {
	return codeBit(uint8_t(toInt(c)));
}

constexpr uint64_t kSupportedGlobalCodes = codeBit(GlobalCode::ClientHello)
		| codeBit(GlobalCode::ServerHello) | codeBit(GlobalCode::Ping) | codeBit(GlobalCode::Pong)
		| codeBit(GlobalCode::SharedObjectsAnnounce) | codeBit(GlobalCode::ServerInfo)
		| codeBit(GlobalCode::AppRequest) | codeBit(GlobalCode::AppNotify);

// A name for a handshake/global failure, for logs.
SP_PUBLIC StringView getGlobalErrorName(GlobalError);

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
			6, // client -> server notification [windowId]: the client's scene changed or moves and
	// wants another frame; the server schedules it on the window's PresentationEngine. No reply:
	// the answer is the AcquireFrame, or FrameDeclined for a window the session may not draw into.
	// The client sends no second request while one is unanswered.
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
	// Packed binary (not CBOR) batch of core::InputEventData records, see
	// serializeInputEvents. The server owns the OS window, so input originates there.
	// Fire-and-forget; the client replays the batch into its Director's scene.
	UpdateLayers =
			10, // client -> server notification: the window's interaction layers (hit/cursor/drag
	// regions). Packed binary (not CBOR) batch of WindowLayer records, see
	// serializeWindowLayers. The client's scene computes the layers; the server applies them
	// to the native window (cursor, hit-testing, server-side decorations).
	// Fire-and-forget; the client only sends on change.

	WindowGeometryChanged = 11, // server -> client notification: [windowId, WindowGeometry]. Where
	// the window now is, in the logical space WindowInfo::rect uses. Separate from the frame
	// constraints (which travel in AcquireFrame), so a window move does not cause a relayout.
	// Sent only on a real change (AppWindow::notifyWindowGeometry).

	WindowControl = 12, // client -> server request: close, state flags, fullscreen, frame
	// rate/interval, extent, window menu, back button, with an operation discriminant.
	// Payload is a keyed CBOR map: "w" (window id), "op" (WindowControlOp), plus at most one
	// operation-specific value -- see WindowControlOp.
	// Reply: [int32 Status]. The `bool` returned to the scene is a local precondition check
	// against the mirrored window state; the server re-checks it.

	TextInputControl = 13, // client -> server notification: [w, op, req|cmd]. The scene asking the
	// window's text-input processor to start, stop, or perform an edit (see TextInputOp). A
	// notification, so a timed-out IME activation cannot trip the request watchdog; the result
	// comes back as TextInputState.

	// server -> client notification: [windowId, TextInputState], the state the processor settled
	// on. The only source of truth for the client's widget: it must not apply its own request.
	TextInputState = 14,

	/* client -> server request: open a window for me. Payload is a keyed map -- the subset of
	WindowInfo a client may ask for (see serializeWindowRequest); the server is free to alter or
	ignore any of it.

	Reply: {"st": Status, "id": the final window id, "win": what was granted}. A refusal is a
	Status, not a protocol error: it is policy (no handler, a type this window system can not make,
	the session's window budget), and it must not cost the session.

	The reply says the window was CREATED, not that it is drawable: it becomes drawable when its
	scene has shared a queue and the announce carries it, with the serial of this request in the
	last announce slot so the asking client knows which of its requests it answers. */
	CreateWindow = 15,

	/* server -> client notification [windowId]: the ReadyForNextFrame for this window will not be
	answered with a frame - the window is not one the session may draw into. The client stops
	waiting for that frame; it asks again after its scene changes. */
	FrameDeclined = 16,
};

// Every WindowCode has a handler on the side that receives it; see the note on codeBit.
constexpr uint64_t kSupportedWindowCodes = codeBit(WindowCode::CompileQueue)
		| codeBit(WindowCode::UpdateMaterials) | codeBit(WindowCode::AcquireFrame)
		| codeBit(WindowCode::FrameInput) | codeBit(WindowCode::FrameCommit)
		| codeBit(WindowCode::AttachQueue) | codeBit(WindowCode::ReadyForNextFrame)
		| codeBit(WindowCode::RequestScreenshot) | codeBit(WindowCode::CompileMaterials)
		| codeBit(WindowCode::InputEvents) | codeBit(WindowCode::UpdateLayers)
		| codeBit(WindowCode::WindowGeometryChanged) | codeBit(WindowCode::WindowControl)
		| codeBit(WindowCode::TextInputControl) | codeBit(WindowCode::TextInputState)
		| codeBit(WindowCode::CreateWindow) | codeBit(WindowCode::FrameDeclined);


// Operations carried by WindowCode::TextInputControl.
enum class TextInputOp : uint8_t {
	Acquire = 0, // "req": array (serializeTextInputRequest) -- start or update IME capture
	Release = 1, // no argument -- stop capture
	Perform = 2, // "cmd": array (serializeTextInputCommand) -- drive the processor as an IME would
};

// Operations carried by WindowCode::WindowControl, with the payload key each one reads.
enum class WindowControlOp : uint8_t {
	Close = 0, // "graceful": bool
	EnableState = 1, // "state": int64 (core::WindowState, 64-bit)
	DisableState = 2, // "state": int64
	SetFullscreen = 3, // "fs": array (serializeFullscreenInfo)
	SetPreferredFrameRate = 4, // "rate": double
	SetPreferredFrameInterval = 5, // "iv": int64 (microseconds)
	SetWindowExtent = 6, // "ext": [width, height]
	OpenWindowMenu = 7, // "pos": [x, y] in scene coords; Vec2::INVALID means "at the pointer"
	BackButton = 8, // no argument
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
// Lifecycle (the "sender" is whoever offers the data; works in both directions):
//   Announce  (request, CBOR header) -- sender offers a blob: id, type, total size, packet count,
//             packet size and a per-packet hash. The receiver agrees by replying without error (a
//             mirror reply == accept) or declines with an error reply.
//   Packet    (notification, binary -- not CBOR) -- one chunk: [u64 id][u32 index][chunk bytes];
//             the transport LZ4 in sendFrame supplies the compression. Streamed back-to-back.
//   Complete  (notification) -- receiver: all packets arrived and every per-packet hash matched.
//   Release   (notification) -- sender: it will no longer reference the blob; the receiver may drop it.
//   Unavailable(notification) -- receiver: it can no longer hold the blob (eviction, even after a
//             Release race); the sender must stop referencing that id.
//   Cancel    (notification) -- sender: it is abandoning a transfer still in flight; the receiver
//             drops the partial buffer. The mirror image of Unavailable, for the other end.
enum class DataCode : uint8_t {
	Announce = 0,
	Packet = 1,
	Complete = 2,
	Release = 3,
	Unavailable = 4,

	// Cancel (notification, CBOR {id}) -- the sender abandons a transfer it is still streaming; the
	// receiver drops the partial buffer and answers nothing. Sender-only: the receiver's
	// counterpart is Unavailable. A peer without it drops the unknown notification.
	Cancel = 5,
};

constexpr uint64_t kSupportedDataCodes = codeBit(DataCode::Announce) | codeBit(DataCode::Packet)
		| codeBit(DataCode::Complete) | codeBit(DataCode::Release)
		| codeBit(DataCode::Unavailable) | codeBit(DataCode::Cancel);


enum class DataError : uint8_t {
	Ok = 0,
	Declined = 1, // receiver refuses the offer (policy / capacity); the Announce error-reply code
	TooLarge = 2, // total size or packet size exceeds the agreed limits
	BadHeader = 3, // malformed/inconsistent Announce header
	HashMismatch = 4, // a received packet failed its announced hash
	UnknownTransfer = 5, // a Packet/Complete/Release/Unavailable referenced an unknown id
	Cancelled = 6, // the sender abandoned the transfer (DataCode::Cancel), or the local side did
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

// Default chunk size. Must stay <= kMaxFrameSize. The receiver validates the announced packet size
// against it, which bounds decompressed packet size (the transport has no ratio-based caps).
constexpr uint32_t kRecommendedPacketSize = 64u * 1'024; // 64 KiB
constexpr uint32_t kMaxBlockTransferSize = 512u * 1'024 * 1'024; // whole-blob policy ceiling

// Domain::Font: split the font system between a headless client (glyph positioning) and the GPU
// server (rasterization + atlas). The client owns its own FontLibrary for metrics and mints FaceIds
// locally; the server adopts those ids and rasterizes into a per-connection atlas. Font *data* is
// content-addressed (a hash of the file bytes) and stored persistently on the server, so a font the
// server already holds is never re-sent.
//
//   SourcesAnnounce (request, CBOR) -- client lists families/aliases/sources, each source tagged
//             with its contentHash. Reply SourcesReady names the hashes the server is still
//             missing (plus the atlas image's server object id); an error reply means the announce
//             was rejected.
//   FontInline (notification, CBOR {contentHash, bytes}) -- a small missing font shipped inline;
//             large ones come over Domain::Data (DataType::Font, meta={contentHash}).
//   GlyphRequest (notification, binary) -- [u32 depId][faces...] asks the server to rasterize a set
//             of (contentHash, spec, faceId) glyphs; depId gates the frame that uses them.
//   AtlasReady (notification, CBOR {depId, ok}) -- the server finished the atlas update for depId.
//   CompileImage (request) -- reserved; not dispatched by either side.
enum class FontCode : uint8_t {
	SourcesAnnounce = 0,
	SourcesReady = 1,
	FontInline = 2,
	GlyphRequest = 3,
	AtlasReady = 4,
	CompileImage = 5,
};

// CompileImage is absent: the code is declared but neither side dispatches it.
constexpr uint64_t kSupportedFontCodes = codeBit(FontCode::SourcesAnnounce)
		| codeBit(FontCode::SourcesReady) | codeBit(FontCode::FontInline)
		| codeBit(FontCode::GlyphRequest) | codeBit(FontCode::AtlasReady);

// The mask this build advertises for `d`, or 0 for a domain it does not implement at all.
constexpr uint64_t getSupportedCodes(Domain d) {
	switch (d) {
	case Domain::Global: return kSupportedGlobalCodes; break;
	case Domain::Window: return kSupportedWindowCodes; break;
	case Domain::Data: return kSupportedDataCodes; break;
	case Domain::Font: return kSupportedFontCodes; break;
	default: break;
	}
	return 0;
}


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
// MessageType layout above: reply is role+3 and error is role+5 (value 3 is a reserved gap);
// isReply()/isError() routing depends on it.
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
// Debug-only: the key is a public constant. Release builds must be given a real key
// (AppThread::setBearerKey / ClientContext::setBearerKey).
SP_PUBLIC BytesView getDevBearerKey();
#endif

/* Append scalars to a buffer in network byte order; the write-side counterpart of
 * BytesViewNetwork. Values are passed in host order and converted here.
 *
 * Floats travel as their bit pattern: InputEventData::input.x defaults to NaN and hasLocation() is
 * defined by isnan(), so NaN must round-trip exactly. */
class SP_PUBLIC WireWriter {
public:
	explicit WireWriter(Bytes &out) : _out(&out) { }

	void writeU8(uint8_t);
	void writeU16(uint16_t);
	void writeU32(uint32_t);
	void writeU64(uint64_t);

	// The IEEE-754 bits of `v`, not a rounded value. See the note above on NaN.
	void writeFloatBits(float v);

	void writeBytes(BytesView);

	// `count` zero bytes; padding inside a fixed-size record is written explicitly.
	void writeZero(size_t count);

protected:
	Bytes *_out = nullptr;
};

// Read the IEEE-754 bits of a float back; the counterpart of WireWriter::writeFloatBits.
SP_PUBLIC float readFloatBits(BytesViewNetwork &);

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

// Send-side buffer. sendFrame blocks while the transport's send buffer or the peer's flow-control
// window is full, so messages are framed into this FIFO queue and drained non-blockingly from the
// connection's poll() (socket readiness and update ticks). The handshake stays synchronous.
class SP_PUBLIC OutgoingQueue {
public:
	// Past this many pending bytes the peer is considered dead and the caller should drop it.
	static constexpr size_t kMaxPending = 64u * 1'024 * 1'024;

	// Frame a message and append it. Never blocks. False once the queue is over kMaxPending.
	bool push(BytesView dict, MessageType, Domain, uint8_t code, uint32_t serial,
			BytesView payload);

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

// The server side of the setup handshake, advanced a step at a time. No call blocks: step() takes
// what the transport has and returns, so many handshakes can run on one thread and a silent peer
// holds nothing up. Time is passed in, which also makes deadlines testable.
class SP_PUBLIC ServerHandshake {
public:
	enum class State {
		Idle,
		ReadingHello,
		HelloReceived, // negotiate() and reply() decide the answer
		Replying,
		Done, // the ServerHello was written
		Failed, // closed, malformed frame, or past the deadline
	};

	void begin(uint64_t deadlineUs);
	void setDeadline(uint64_t deadlineUs) { _deadline = deadlineUs; }

	// Reads no more than the hello still needs, so bytes a client sends after it stay in the
	// stream for the connection.
	State step(TransportConnection &, uint64_t nowUs);

	// From HelloReceived: check magic, version, auth mode and key, and choose the dictionary.
	GlobalError negotiate(BytesView expectedKey, BytesView serverDict, bool requireBearerKey);

	// Queue the ServerHello. From ReadingHello or Failed it answers without a hello, which a refusal
	// does after waiting a little (see serverHandshakeReject).
	void reply(GlobalError, BytesView serverDict);

	State getState() const { return _state; }
	GlobalError getReplied() const { return _replied; }
	BytesView getNegotiatedDict() const { return _negotiatedDict; }

	// The key the client sent, from HelloReceived on; empty before that. A view into the hello,
	// valid until the next begin().
	BytesView getPresentedKey() const;

protected:
	State _state = State::Idle;
	uint64_t _deadline = 0;
	Bytes _in;
	Bytes _out;
	size_t _outOffset = 0;
	bool _helloValid = false;
	ClientHello _hello; // views into _in
	DictSource _dictSource = DictSource::None;
	Bytes _negotiatedDict;
	GlobalError _replied = GlobalError::BadProtocol;
};

// The client side, likewise non-blocking.
class SP_PUBLIC ClientHandshake {
public:
	enum class State {
		Idle,
		Writing,
		ReadingReply,
		Done, // a ServerHello arrived: getResult() is Ok or the server's refusal
		Failed, // NetworkBackend (closed, deadline) or BadProtocol (malformed reply)
	};

	// Queue the ClientHello.
	void begin(BytesView bearerKey, BytesView dict, uint64_t deadlineUs);

	State step(TransportConnection &, uint64_t nowUs);

	State getState() const { return _state; }
	GlobalError getResult() const { return _result; }

	// Valid in Done; its dictionary points into this handshake.
	const ServerHello &getServerHello() const { return _hello; }

protected:
	State _state = State::Idle;
	uint64_t _deadline = 0;
	Bytes _in;
	Bytes _out;
	size_t _outOffset = 0;
	ServerHello _hello;
	GlobalError _result = GlobalError::NetworkBackend;
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
// `requireBearerKey` false accepts any key. Pass false only when the transport authenticated the
// peer (TransportCaps::PeerAuthenticated -- unix-domain credentials, socket permissions).
SP_PUBLIC GlobalError serverHandshake(TransportConnection &, BytesView expectedKey,
		BytesView serverDict, Bytes &negotiatedDict, uint64_t deadlineUs,
		bool requireBearerKey = true);

// Server side: answer a connection with `status` instead of negotiating (e.g. GlobalError::Busy),
// so the peer does not wait out its handshake deadline. Waits briefly for the ClientHello first
// (kRejectHelloWaitUs) and answers regardless.
// Returns `status`; the caller closes the connection afterwards.
SP_PUBLIC GlobalError serverHandshakeReject(TransportConnection &, GlobalError status,
		uint64_t deadlineUs);


} // namespace stappler::xenolith::remote

#endif /* XENOLITH_REMOTE_XLREMOTEPROTOCOL_H_ */
