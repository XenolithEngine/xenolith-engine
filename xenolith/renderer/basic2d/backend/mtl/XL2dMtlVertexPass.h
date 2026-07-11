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

#ifndef XENOLITH_RENDERER_BASIC2D_BACKEND_MTL_XL2DMTLVERTEXPASS_H_
#define XENOLITH_RENDERER_BASIC2D_BACKEND_MTL_XL2DMTLVERTEXPASS_H_

#include "XLMtlQueuePass.h"
#include "XLMtlMaterial.h"
#include "XL2dCommandList.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d::mtl {

namespace mt = stappler::xenolith::mtl;

/* Metal material vertex pass for the 2d renderer (initial slice).
 *
 * Consumes FrameContextHandle2d command lists (VertexArray/Deferred commands:
 * no particles or shadows yet), packs geometry into storage buffers (vertex
 * pulling) and draws material spans; per-span parameters are addressed by
 * [[instance_id]] (firstInstance carries the span index) */

// MSL programs for the pass (entry point `main0`). The fragment shader reads
// the texture set argument buffer, whose MSL struct layout depends on the
// layout's sampler/image counts - the source is assembled per queue
SP_PUBLIC StringView getMaterialVertexShader();
SP_PUBLIC String getMaterialFragmentShader(uint32_t samplerCount, uint32_t imageCount);

// shader-visible per-span data, addressed by [[instance_id]]
struct SpanData {
	uint32_t samplerImageIdx = 0;
	uint32_t instanceTransformIdx = 0;
	// packed ComponentMapping (4 bits per component, see core::ComponentMapping)
	uint32_t colorMode = 0;
	// painter-order depth of the span (float bits)
	float depth = 0.0f;
	// data-atlas region in the combined atlas buffer: element offset (u32
	// units) and pow2 slot count; 0 slots = material has no atlas
	uint32_t atlasOffset = 0;
	uint32_t atlasSlots = 0;
};

class SP_PUBLIC VertexAttachment : public core::GenericAttachment {
public:
	virtual ~VertexAttachment() = default;

	virtual bool init(AttachmentBuilder &, const core::AttachmentData *materials);

	virtual Rc<core::AttachmentHandle> makeFrameHandle(const core::FrameQueue &) override;

	const core::AttachmentData *getMaterials() const { return _materials; }

	// combined data-atlas buffer, rebuilt when the set of live atlases
	// changes (atlases are immutable once compiled, identity implies content);
	// accessed on the gl thread only
	struct AtlasBufferCache {
		Vector<const core::DataAtlas *> atlases;
		Vector<uint32_t> offsets; // element offsets in u32 units
		Rc<mt::Buffer> buffer;
	};

	AtlasBufferCache &getAtlasCache() { return _atlasCache; }

protected:
	using core::GenericAttachment::init;

	const core::AttachmentData *_materials = nullptr;
	AtlasBufferCache _atlasCache;
};

class SP_PUBLIC VertexAttachmentHandle : public mt::BufferAttachmentHandle {
public:
	virtual ~VertexAttachmentHandle() = default;

	virtual void submitInput(core::FrameQueue &, Rc<core::AttachmentInputData> &&,
			Function<void(bool)> &&) override;

	const Rc<mt::Buffer> &getIndexes() const { return _indexes; }
	SpanView<VertexSpan> getSpans() const { return _spans; }

	// draw states (scissor) copied from the frame context for record time;
	// the context itself must NOT be retained here - it is frame input data
	// and holding it creates a frame ownership cycle (frame never completes)
	SpanView<DrawStateValues> getDrawStates() const { return _drawStates; }

	bool empty() const { return !_indexes || _spans.empty(); }

protected:
	bool loadVertexes(core::FrameHandle &, const Rc<FrameContextHandle2d> &);

	Rc<mt::Buffer> _indexes;
	Vector<VertexSpan> _spans;
	Rc<core::MaterialSet> _materialSet;
	Vector<DrawStateValues> _drawStates;
};

// passthrough for scene inputs the Metal slice does not process yet
// (shadow lights, particle emitters)
class SP_PUBLIC IgnoredInputAttachment : public core::GenericAttachment {
public:
	virtual ~IgnoredInputAttachment() = default;

	virtual Rc<core::AttachmentHandle> makeFrameHandle(const core::FrameQueue &) override;
};

class SP_PUBLIC MaterialVertexPass : public mt::QueuePass {
public:
	struct RenderQueueInfo {
		NotNull<core::Loop> target;
		Extent2 extent;
		Color4F backgroundColor = Color4F::WHITE;
	};

	// scene-compatible render queue: attachments named for FrameContext2d
	// (VertexInput2d/MaterialInput2d + light/particle stubs)
	static bool makeRenderQueue(core::Queue::Builder &, RenderQueueInfo &);

	virtual ~MaterialVertexPass() = default;

	virtual bool init(QueuePassBuilder &, const core::AttachmentData *vertexes,
			const core::AttachmentData *materials);

	virtual Rc<core::QueuePassHandle> makeFrameHandle(const core::FrameQueue &) override;

	const core::AttachmentData *getVertexes() const { return _vertexes; }
	const core::AttachmentData *getMaterials() const { return _materials; }

protected:
	using QueuePass::init;

	const core::AttachmentData *_vertexes = nullptr;
	const core::AttachmentData *_materials = nullptr;
};

class SP_PUBLIC MaterialVertexPassHandle : public mt::QueuePassHandle {
public:
	virtual ~MaterialVertexPassHandle() = default;

	virtual bool prepare(core::FrameQueue &, Function<void(bool)> &&) override;

protected:
	virtual void recordSubpass(core::FrameQueue &, const core::SubpassData &,
			mt::CommandBuffer &) override;

	const VertexAttachmentHandle *_vertexHandle = nullptr;
	Rc<core::MaterialSet> _materialSet;
};

} // namespace stappler::xenolith::basic2d::mtl

#endif /* XENOLITH_RENDERER_BASIC2D_BACKEND_MTL_XL2DMTLVERTEXPASS_H_ */
