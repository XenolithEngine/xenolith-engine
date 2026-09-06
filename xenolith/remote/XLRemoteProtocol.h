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

/* 'XLRP' -- version-NEUTRAL, and that is the change.
 *
 * It used to be 'XLR1', with the version digit inside the magic as well as in its own field. That
 * makes the version unnegotiable in principle: a peer of a different version fails on the magic, at
 * the first four bytes, before anything can look at a version field or a status byte and say what
 * went wrong. Bumping such a magic also means every future bump has two places to keep in step.
 * From here the magic answers only "is this our protocol at all", and the version answers "which
 * one" -- which is the question a peer can actually be told the answer to. */
constexpr uint32_t kProtocolMagic = 0x584C'5250; // 'XLRP'

/* Version 2: InputEvents and UpdateLayers stopped being raw dumps of C++ structs and became a typed
 * format (see XLRemoteSerialize.h). The same code carrying different bytes is exactly the change a
 * version exists for -- a version-1 peer would parse a v2 batch as its own struct array and act on
 * whatever that produced, so the two are refused for each other at the handshake rather than left
 * to find out.
 *
 * Compatibility with version 1 is deliberately NOT kept. Doing so would mean both codecs living
 * side by side forever and the layout tag still gating v1 sessions -- that is, the milestone half
 * done, in exchange for old peers that do not exist: the only client anyone runs is rebuilt from
 * this tree by live-reload. */
constexpr uint16_t kProtocolVersion = 2;
constexpr uint32_t kBearerKeySize = 64;

// Size of one record in the typed input/layer batches (WindowCode::InputEvents / ::UpdateLayers).
// The layout each one describes is documented with its codec in XLRemoteSerialize.h; the numbers
// live here because they are facts about the wire, and because the peer-info fingerprint reports
// them without wanting the codec's headers.
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

// Which transport stream a domain's messages ride. On a transport without TransportCaps::MultiStream
// every class folds onto the same stream and this mapping changes nothing.
//
// The split is by ORDERING OBLIGATION, not by message size, and that is why Font sits with Window
// rather than with the other bulk carrier:
//
//   Control -- Global, Window, Font. Their relative order carries meaning and the wire is what
//     enforces it: SharedObjectsAnnounce must precede the first AcquireFrame for a window the client
//     has not heard of; AttachQueue must precede the frames it enables; and the client flushes a
//     frame's Domain::Font glyph requests immediately BEFORE its FrameInput (XLRemoteWindow.cc), so
//     that the server registers the gating dependency before it reconciles the frame against it.
//     Putting Font on its own stream would break exactly that, and only under load -- the frame
//     would reconcile against a dependency that has not arrived and the glyphs would go ungated.
//     Font is cheap to keep here anyway: the large font payloads travel as Domain::Data blocks
//     (DataType::Font), so what remains on this domain is requests and metadata.
//
//   Bulk -- Data. The only domain that owes nothing to the order of another, and precisely the one
//     that causes head-of-line blocking today: an 8 MiB screenshot delays every InputEvents behind
//     it. Separating it is the whole point of the exercise.
//
// StreamClass::Frame is deliberately unused for now. Separating frame traffic from input needs the
// "input then frame" ordering to be re-established explicitly, which belongs with the typed wire
// format rather than here; until then it folds onto Control and costs nothing.
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
	IncompatiblePeer = 6, // reserved. Until M6 this refused a peer whose struct layout differed,
	// because InputEvents/UpdateLayers were raw dumps of it; the typed format retired the reason,
	// and a differing wire-contract tag is now reported rather than acted on (see PeerInfo::abi).
	// Kept because a peer may still send it, and because renumbering a wire code buys nothing
	NotImplemented = 254,
	NetworkBackend = 255, // not protocol-related, check backend error reporting
};

/* --- what this build knows how to receive -------------------------------------------------------
 *
 * One bit per message code, one mask per domain, exchanged in PeerInfo (XLRemotePeerInfo.h). A peer
 * can then ask before it sends, rather than sending and learning from a NotImplemented reply -- and,
 * more usefully, a log can say up front what the other side is missing instead of leaving a later
 * symptom unexplained.
 *
 * BUILD-level, not role-level: the mask says "my dispatcher has a case for this code", not "I expect
 * to receive it". Who sends what is already fixed by the protocol's own direction, so narrowing
 * these per role would encode the same fact twice.
 *
 * Maintained by hand beside the enum, which is a second source of truth -- so tests/remote pins the
 * contents, and a code added without a handler (or a handler added without a bit) shows up as a
 * failing assertion rather than as a message that quietly does nothing.
 */
constexpr uint64_t codeBit(uint8_t code) { return code < 64 ? (uint64_t(1) << code) : 0; }

template <typename Code>
constexpr uint64_t codeBit(Code c) {
	return codeBit(uint8_t(toInt(c)));
}

constexpr uint64_t kSupportedGlobalCodes = codeBit(GlobalCode::ClientHello)
		| codeBit(GlobalCode::ServerHello) | codeBit(GlobalCode::Ping) | codeBit(GlobalCode::Pong)
		| codeBit(GlobalCode::SharedObjectsAnnounce) | codeBit(GlobalCode::ServerInfo);

// A name for a handshake/global failure, for logs. A refusal is the one thing a client learns about
// a server it could not talk to, and "status 5" is not an answer a person can act on -- "Busy" says
// to try later, "AuthFailed" says the key is wrong, and they call for opposite responses.
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

	// --- M4: window domain completeness ------------------------------------------------------

	WindowGeometryChanged = 11, // server -> client notification: [windowId, WindowGeometry]. Where
	// the window now is, in the logical space WindowInfo::rect uses. A SIBLING of the frame
	// constraints and not part of them: a title-bar drag must not cost a scene relayout.
	//
	// Constraints have deliberately NO message of their own. They already travel in every
	// AcquireFrame, and the client's Director applies them there exactly as a local one does
	// (Director::acquireFrame -> setFrameConstraints), so a second carrier would be a second
	// source of truth for the same fact. Geometry has no such carrier -- and, unlike constraints,
	// the server already has a live hook that fires only on a real change
	// (AppWindow::notifyWindowGeometry), so the deduplication is done before the wire.

	WindowControl = 12, // client -> server REQUEST: everything a scene can ask of the window it
	// draws into -- close, state flags, fullscreen, frame rate/interval, extent, window menu, back
	// button. ONE code with an operation discriminant rather than nine codes, because the reply is
	// the same for all of them (a Status) and the routing is identical.
	//
	// Payload is a keyed CBOR map, not a positional array: the arguments differ per operation, and
	// "which index means what depends on the op" is exactly the fragility the flat-array style
	// avoids where the fields are homogeneous. Keys: "w" (window id), "op" (WindowControlOp), plus
	// at most one operation-specific value -- see WindowControlOp.
	//
	// Reply: [int32 Status]. The `bool` these calls return to the scene is decided LOCALLY, from
	// the mirrored window state, and is a precondition; the Status is what the window actually did.
	// The server re-checks the precondition, because a client can send anything.

	TextInputControl = 13, // client -> server notification: [w, op, req|cmd]. The scene asking the
	// window's text-input processor to start, stop, or perform an edit (see TextInputOp).
	//
	// A NOTIFICATION and not a request, deliberately. The answer to "did the IME accept this" does
	// not come back as a return value even locally -- it comes back as a state echo, below. Making
	// it a request would also mean an IME activation that timed out could take the whole session
	// down through the request watchdog, and losing a keyboard must not cost the connection.
	// server -> client notification: [windowId, TextInputState]. The echo: what the processor
	// decided the state now is. This is the ONLY source of truth for the client's widget -- the
	// state belongs to the IME on the OS side, never to the application, so a client that updated
	// its own field when it sent the request would be showing text the server has not accepted.
	TextInputState = 14,
};

// Every WindowCode has a handler on the side that receives it; see the note on codeBit.
constexpr uint64_t kSupportedWindowCodes = codeBit(WindowCode::CompileQueue)
		| codeBit(WindowCode::UpdateMaterials) | codeBit(WindowCode::AcquireFrame)
		| codeBit(WindowCode::FrameInput) | codeBit(WindowCode::FrameCommit)
		| codeBit(WindowCode::AttachQueue) | codeBit(WindowCode::ReadyForNextFrame)
		| codeBit(WindowCode::RequestScreenshot) | codeBit(WindowCode::CompileMaterials)
		| codeBit(WindowCode::InputEvents) | codeBit(WindowCode::UpdateLayers)
		| codeBit(WindowCode::WindowGeometryChanged) | codeBit(WindowCode::WindowControl)
		| codeBit(WindowCode::TextInputControl) | codeBit(WindowCode::TextInputState);


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
//   Cancel    (notification) -- sender: it is abandoning a transfer still in flight; the receiver
//             drops the partial buffer. The mirror image of Unavailable, for the other end.
enum class DataCode : uint8_t {
	Announce = 0,
	Packet = 1,
	Complete = 2,
	Release = 3,
	Unavailable = 4,

	// Cancel (notification, CBOR {id}) -- the SENDER abandons a transfer it is still streaming; the
	// receiver drops the partial buffer and answers nothing.
	//
	// The receiver's half of this already exists and is called Unavailable: "I can no longer hold
	// this". Cancel is deliberately not made to work in both directions, because then one fact would
	// have two names on the wire. What was missing was only the sender's side -- until now the only
	// way to stop sending was Release, which MEANS "I no longer reference the blob" and merely
	// stopped the stream as a side effect of the transfer record going away.
	//
	// Additive: a peer that predates this answers a notification it does not know with nothing at
	// all (an unknown NOTIFICATION is dropped, only a request gets NotImplemented), so the worst case
	// against an old peer is the transfer completing as it does today.
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
	Cancelled = 6, // the sender abandoned the transfer (DataCode::Cancel), or the local side did.
	// Reported to whoever was waiting on the blob so that "it was called off" is distinguishable
	// from "it arrived corrupt" (HashMismatch) and from "the peer refused it" (Declined)
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

// CompileImage is deliberately ABSENT: the code is declared but neither side dispatches it, and a
// peer that advertised it would be promising something it drops on the floor. It is the one entry
// that makes this mask carry information rather than restate the enum.
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

/* Append scalars to a buffer in network byte order.
 *
 * The counterpart of BytesViewNetwork, which the tree has had all along. The write side did not: it
 * was a handful of private helpers taking an ALREADY-swapped value, so every call site had to
 * remember `writeValue32(buf, HostToNetwork(x))` -- and one of them did not, which is why
 * writeServerHello carries a note about a dictionary size that arrived byte-swapped on every
 * little-endian peer. Here the conversion is the writer's, and there is nothing left to forget.
 *
 * Floats travel as their BIT PATTERN rather than as a number, because some of them are not numbers:
 * InputEventData::input.x defaults to NaN and hasLocation() is defined by isnan(), so a NaN that
 * came back as a different NaN -- or as zero -- would change what the event means. */
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

	// `count` zero bytes. Padding inside a fixed-size record is written explicitly so that what
	// travels is defined -- the raw dumps this replaces shipped whatever the compiler left there.
	void writeZero(size_t count);

protected:
	Bytes *_out = nullptr;
};

// Read the IEEE-754 bits of a float back. Symmetric with WireWriter::writeFloatBits, and used
// instead of BytesViewNetwork::readFloat32 for the same reason: the bit pattern is what was sent.
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
