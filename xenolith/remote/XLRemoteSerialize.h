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

// CompileMaterials wire codec for a single core::MaterialImage (the headless client forwards a runtime
// material it cannot GPU-compile; see WindowCode::CompileMaterials). The image is referenced by its
// stable wire index -- the server owns the GPU objects and resolves the real image itself -- so only the
// descriptor binding (sampler/set/descriptor) and the view info travel. `image`/`dynamic`/`view` are
// left for the server to fill after it resolves the image by id.
SP_PUBLIC Value serializeMaterialImage(const core::MaterialImage &);
// Decode the wire fields into the returned MaterialImage; `outImageId` receives the image's wire index
// for the caller to resolve to a real image.
SP_PUBLIC core::MaterialImage deserializeMaterialImage(const Value &, uint64_t &outImageId);

} // namespace stappler::xenolith::remote

#endif /* XENOLITH_REMOTE_XLREMOTESERIALIZE_H_ */
