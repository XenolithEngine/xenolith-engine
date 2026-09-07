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

#ifndef XENOLITH_REMOTE_XLREMOTESERIALIZE_H_
#define XENOLITH_REMOTE_XLREMOTESERIALIZE_H_

#include "XLRemoteObject.h"
#include "XLCoreQueue.h"
#include "XLCoreResource.h"
#include "XLCoreMaterial.h" // core::MaterialImage for the CompileMaterials codec

namespace STAPPLER_VERSIONIZED stappler::xenolith::remote {

// Serialize a compiled core::Queue / core::Resource (server side) into a CBOR blob and rebuild it as
// a client-side structural mirror (decode side).
//
// The render graph is a pointer-web with shared and ambiguously-owned nodes (e.g. an
// AttachmentPassData is referenced by both its AttachmentData and its QueuePassData). Encoding walks
// the graph, assigns every node a per-type index, and stores cross-references as those indices;
// decoding allocates the node tables in a fresh pool and resolves the indices back to pointers.
//
// Backend gAPI objects (Rc<ImageObject>/<BufferObject>/<Shader>/<GraphicPipeline>/<RenderPass>/...)
// are replaced by a server-assigned object id (via ObjectRegistry) on encode and by a thin handle
// (via ObjectFactory, see XLRemoteObject.h) on decode.
//
// NOT serialized (server-only / deferred to the frame-protocol stage): all callbacks (queue
// begin/end/attach/detach/release, attachment input acquisition/submission/validation, subpass
// prepare/commands, pass availability/submitted/complete); the polymorphic Rc<Attachment> object
// (AttachmentData structure IS mirrored, but `attachment` is left null); the RenderPass recording
// internals (the thin RenderPass carries id + type + index only); inter-pass scheduling metadata
// (QueuePassDependency, QueuePassRequirements, source/target queue dependencies) and PipelineFamily
// grouping. Each QueuePassData on the mirror gets a bare stub Rc<core::QueuePass> so the queue tears
// down safely (QueueData::clear() invalidates it); describe() is NOT usable on the mirror because it
// dereferences the (null) attachment object.
class SP_PUBLIC QueueCodec {
public:
	// Encode a compiled queue (with its internal + linked resources) to a CBOR blob; gAPI objects get
	// ids assigned in `registry`.
	static Bytes encodeQueue(const core::Queue &,
			const HashMap<const core::MaterialAttachment *, Rc<core::MaterialSet>> &,
			ObjectRegistry &registry);

	// Decode a queue blob into a client-side mirror; gAPI objects become thin handles from `factory`.
	// Returns nullptr on malformed input.
	static bool decodeQueue(core::Queue &, BytesView, ObjectFactory &factory);

	// A single MaterialSet update for an already-shared queue (server -> client push). The blob
	// carries the queue id; the owner attachment and material pipelines are referenced by key so the
	// client resolves them against its existing queue mirror. gAPI objects get ids in `registry`.
	static Bytes encodeMaterials(uint64_t queueId, core::MaterialSet &, ObjectRegistry &registry);

	// Apply a material update to the mirror identified by the embedded queue id; replaces the owner
	// MaterialAttachment's MaterialSet. Returns false on malformed input or an unknown queue/owner.
	static bool decodeMaterials(BytesView, ObjectFactory &factory);

	// Resource-only (also used internally by the queue codec).
	static Bytes encodeResource(const core::Resource &, ObjectRegistry &registry);
	static Rc<core::Resource> decodeResource(BytesView, ObjectFactory &factory);
};

SP_PUBLIC Value serializeFrameConstraints(const core::FrameConstraints &);
SP_PUBLIC core::FrameConstraints deserializeFrameConstraints(const Value &);

SP_PUBLIC Value serializeWindowInfo(const sprt::window::WindowInfo &);
SP_PUBLIC Rc<sprt::window::WindowInfo> deserializeWindowInfo(const Value &);

SP_PUBLIC Value serializeSwapchainConfig(const core::SwapchainConfig &);
SP_PUBLIC core::SwapchainConfig deserializeSwapchainConfig(const Value &);

// Where the window is, in the logical space WindowInfo::rect uses. A sibling of FrameConstraints and
// not part of it, for the same reason handleWindowGeometryChanged is a sibling of
// handleConstraintsChanged: a window that only MOVED must not cost a scene relayout.
SP_PUBLIC Value serializeWindowGeometry(const sprt::window::WindowGeometry &);
SP_PUBLIC sprt::window::WindowGeometry deserializeWindowGeometry(const Value &);

/* Frame telemetry, sent to the client alongside every AcquireFrame.
 *
 * Field-by-field, and NOT as a raw dump, because both structs grow extra members under
 * `#if XL_FRAME_ACCOUNT` -- their size is a build-flag fact. The ABI tag from M3 hashes only
 * InputEventData and WindowLayer, so a server built with the flag and a client built without it
 * connect successfully TODAY; a dump would corrupt that pair rather than merely disagree.
 *
 * The flagged fields are therefore appended at the END of the array and read only when both the
 * array is long enough and this build has the members to put them in. That is what lets the two
 * sides agree on the prefix and ignore the rest. */
// Already used by the WindowInfo codec; exported because WindowControl's SetFullscreen carries the
// same structure. EdidInfo::vendor is deliberately NOT serialized -- it is a derived lookup cache
// over vendorId, and a StringView into a string that would not survive the trip.
SP_PUBLIC Value serializeFullscreenInfo(const sprt::window::FullscreenInfo &);
SP_PUBLIC sprt::window::FullscreenInfo deserializeFullscreenInfo(const Value &);

SP_PUBLIC Value serializeFrameTiming(const core::FrameTimingInfo &);
SP_PUBLIC core::FrameTimingInfo deserializeFrameTiming(const Value &);

SP_PUBLIC Value serializeDrawStat(const core::DrawStat &);
SP_PUBLIC core::DrawStat deserializeDrawStat(const Value &);

/* Text input, in both directions (WindowCode::TextInputControl and ::TextInputState).
 *
 * The text is carried as UTF-8 while the cursors stay UTF-16 INDICES, which is only sound because
 * the round trip reproduces the same UTF-16 sequence -- UTF-8 is the transport encoding here, not a
 * re-indexing. Everything on both sides of the wire indexes in UTF-16 (TextCursor is defined
 * against it), so converting the offsets too would be the bug, not the fix.
 *
 * The one text this cannot carry is a lone surrogate, which a live IME can legally produce
 * mid-composition and UTF-8 cannot represent. Known limitation; the headless processor does not
 * compose, so it does not arise on the test path.
 *
 * TextCursor::InvalidCursor is {Max<uint32_t>, 0} and is a VALUE, not an absence -- it is the
 * default for a command's replacement/marked ranges. It travels as those numbers. */
SP_PUBLIC Value serializeTextInputRequest(const core::TextInputRequest &);
SP_PUBLIC core::TextInputRequest deserializeTextInputRequest(const Value &);

SP_PUBLIC Value serializeTextInputState(const core::TextInputState &);
SP_PUBLIC core::TextInputState deserializeTextInputState(const Value &);

SP_PUBLIC Value serializeTextInputCommand(const core::TextInputCommand &);
SP_PUBLIC core::TextInputCommand deserializeTextInputCommand(const Value &);

// CompileMaterials wire codec for a single core::MaterialImage (the headless client forwards a runtime
// material it cannot GPU-compile; see WindowCode::CompileMaterials). The image is referenced by its
// stable wire index -- the server owns the GPU objects and resolves the real image itself -- so only the
// descriptor binding (sampler/set/descriptor) and the view info travel. `image`/`dynamic`/`view` are
// left for the server to fill after it resolves the image by id.
SP_PUBLIC Value serializeMaterialImage(const core::MaterialImage &);
// Decode the wire fields into the returned MaterialImage; `outImageId` receives the image's wire index
// for the caller to resolve to a real image.
SP_PUBLIC core::MaterialImage deserializeMaterialImage(const Value &, uint64_t &outImageId);

/* --- the typed wire format for input and layers (M6) -----------------------------------------
 *
 * These two are the only structures that used to travel as a RAW DUMP of their C++ layout, and the
 * only reason the ABI tag from M3 had to gate a session: with a dump, a build disagreement is not a
 * rejected message, it is one process reading another's padding as a keycode. Everything else on
 * this wire has always been field-by-field.
 *
 * Packed binary rather than a CBOR array -- unlike every other codec in this header, and on purpose.
 * Input is the hot path: a batch travels on every frame that saw a pointer move, and a data::Value
 * per field per event would allocate on both sides for no benefit. The layout below is fixed, so
 * there is nothing for a self-describing encoding to describe.
 *
 * Batch envelope, both messages:
 *
 *   [u64 windowId][u16 recordSize][u16 reserved][u32 count][record x count]
 *
 * `recordSize` is what makes the format extensible without a version: a peer that appends a field
 * to the record stays readable by an older one, which decodes the prefix it knows and STEPS BY THE
 * DECLARED SIZE to the next record. That is the binary form of the trick the CBOR codecs above get
 * from a short array, and it is why the size is on the wire rather than assumed from a constant.
 *
 * Every field is written explicitly, including padding, so what travels is a fact of the protocol
 * rather than of the compiler. Floats travel as their IEEE bits (WireWriter::writeFloatBits): NaN is
 * a value here, not an absence -- InputEventData::input.x defaults to NaN and hasLocation() is
 * defined by isnan().
 *
 * NOT fixed by any of this: what the NUMBERS mean. `event` rides as an integer, so two builds that
 * disagree about which InputEventName is 7 -- a value inserted in the middle -- misread each other
 * exactly as a dump would. The enum values are part of the wire contract, and tests/remote pins
 * them; that is the half of the old ABI tag that was doing real work. */

// InputEventData record: 40 bytes.
//   [u32 id][u32 event][u32 button][u32 modifiers][u32 x bits][u32 y bits][16 bytes variant]
// The variant is selected by InputEventInfo[event].dataType, so it is the EVENT that says how the
// last 16 bytes are read -- not the sender's memory layout:
//   Point  : [u32 valueX bits][u32 valueY bits][u32 density bits][u32 zero]
//   Key    : [u16 keycode][u16 compose][u32 keysym][u32 keychar][u32 zero]
//   Window : [u64 state][u64 changes]
//   None   : 16 zero bytes
// The size itself lives in XLRemoteProtocol.h, with the other facts about the wire.

SP_PUBLIC void serializeInputEvents(Bytes &out, uint64_t windowId,
		SpanView<core::InputEventData>);

// Returns false when the blob is not a well-formed batch (short header, or a record size that
// cannot hold the fields this build reads). `outWindowId` is filled whenever the header parsed.
SP_PUBLIC bool deserializeInputEvents(BytesView, uint64_t &outWindowId,
		Vector<core::InputEventData> &out);

// WindowLayer record: 24 bytes.
//   [u32 x bits][u32 y bits][u32 width bits][u32 height bits][u8 cursor][3 zero][u32 flags]

SP_PUBLIC void serializeWindowLayers(Bytes &out, uint64_t windowId,
		SpanView<sprt::window::WindowLayer>);

SP_PUBLIC bool deserializeWindowLayers(BytesView, uint64_t &outWindowId,
		sprt::window::Vector<sprt::window::WindowLayer> &out);

} // namespace stappler::xenolith::remote

#endif /* XENOLITH_REMOTE_XLREMOTESERIALIZE_H_ */
