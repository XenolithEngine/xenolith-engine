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

#ifndef XENOLITH_RENDERER_BASIC2D_BACKEND_SOFT_XL2DSOFTFLATPASS_H_
#define XENOLITH_RENDERER_BASIC2D_BACKEND_SOFT_XL2DSOFTFLATPASS_H_

#include "XL2d.h"

#if MODULE_XENOLITH_RENDERER_BASIC2D_SOFT

#include "XL2dFrameContext.h"
#include "XL2dVertexPlan.h"
#include "XLSoftQueuePass.h"
#include "XLSoftMaterial.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d::soft {

namespace sf = stappler::xenolith::soft;

// Input attachment that accepts and drops its input.
//
// FrameContext2d::initWithQueue requires the lights and particle-emitter attachments to exist by
// name even when the queue does not render them; same pattern as the vk/WebGPU/Metal backends.
class SP_PUBLIC IgnoredInputAttachment : public core::GenericAttachment {
public:
	virtual ~IgnoredInputAttachment() = default;

	virtual Rc<core::AttachmentHandle> makeFrameHandle(const core::FrameQueue &) override;
};

class SP_PUBLIC VertexAttachment : public core::GenericAttachment {
public:
	virtual ~VertexAttachment() = default;

	virtual bool init(AttachmentBuilder &, const core::AttachmentData *materials,
			bool damageTracked = false);

	const core::AttachmentData *getMaterials() const { return _materials; }

	// Whether this queue asked for per-frame damage tracking at all.
	bool isDamageTracked() const { return _damageTracked; }

	virtual Rc<core::AttachmentHandle> makeFrameHandle(const core::FrameQueue &) override;

protected:
	using core::GenericAttachment::init;

	const core::AttachmentData *_materials = nullptr;
	bool _damageTracked = false;
};

class SP_PUBLIC VertexAttachmentHandle : public core::AttachmentHandle {
public:
	virtual ~VertexAttachmentHandle() = default;

	virtual void submitInput(core::FrameQueue &, Rc<core::AttachmentInputData> &&,
			Function<void(bool)> &&) override;

	// The frame, as the flat vertex shader would see it: the very arrays a GPU backend uploads,
	// only here they stay in host memory and are read straight back by the record stage.
	SpanView<VertexSpan> getSpans() const { return _spans; }
	SpanView<Vertex> getVertexes() const { return _vertexes; }
	SpanView<uint32_t> getIndexes() const { return _indexes; }
	SpanView<TransformData> getTransforms() const { return _transforms; }

	// Draw states (scissor/viewport) copied out of the frame context for record time; the context
	// itself must NOT be retained here - it is frame input data, and holding it creates a frame
	// ownership cycle (the frame would never complete).
	SpanView<DrawStateValues> getDrawStates() const { return _drawStates; }

	const core::MaterialSet *getMaterialSet() const { return _materialSet; }

	bool empty() const { return _spans.empty(); }

	bool isDamageTracked() const {
		return static_cast<VertexAttachment *>(_attachment.get())->isDamageTracked();
	}

protected:
	// Builds the backend-neutral VertexPlan and writes it into host arrays. Everything the flat
	// queue can emit goes through it - vertex arrays, deferred results (vector images, labels)
	// and painter-order sorting; only particles are dropped, and the plan drops those itself.
	bool loadVertexes(core::FrameHandle &, const Rc<FrameContextHandle2d> &);

	Rc<core::MaterialSet> _materialSet;
	DamageCollector _damage;
	Vector<VertexSpan> _spans;
	Vector<Vertex> _vertexes;
	Vector<uint32_t> _indexes;
	Vector<TransformData> _transforms;
	Vector<DrawStateValues> _drawStates;
};

// Flat render queue for the software backend: one graphics pass, one subpass, drawing straight
// into the swapchain image. The graph mirrors basic2d::vk::FlatPass exactly - same attachments in
// the same order, same three samplers, and PipelineMaterialInfo values that match it byte for
// byte, because materials are matched to pipelines by that struct's value.
class SP_PUBLIC FlatPass : public core::QueuePass {
public:
	struct RenderQueueInfo {
		core::Loop *target = nullptr;
		Extent2 extent;
		Color4F backgroundColor = Color4F::WHITE;
		core::QueueDamageFlags damage = core::QueueDamageFlags::PresentHint;
	};

	static bool makeRenderQueue(Queue::Builder &, RenderQueueInfo &);

	virtual ~FlatPass() = default;

	virtual bool init(Queue::Builder &queueBuilder, QueuePassBuilder &passBuilder,
			const RenderQueueInfo &info);

	virtual Rc<core::QueuePassHandle> makeFrameHandle(const FrameQueue &) override;

	const core::AttachmentData *getVertexes() const { return _vertexes; }

protected:
	using core::QueuePass::init;

	void makeMaterialSubpass(Queue::Builder &queueBuilder, core::SubpassBuilder &subpassBuilder,
			const core::PipelineLayoutData *layout2d,
			const core::AttachmentPassData *colorAttachment);

	const core::AttachmentData *_output = nullptr;
	const core::AttachmentData *_materials = nullptr;
	const core::AttachmentData *_vertexes = nullptr;
};

class SP_PUBLIC FlatPassHandle : public sf::QueuePassHandle {
public:
	virtual ~FlatPassHandle() = default;

	virtual bool prepare(core::FrameQueue &, Function<void(bool)> &&) override;

protected:
	// Runs the vertex stage and fills the rasterizer's draw list.
	virtual void recordSubpass(core::FrameQueue &, const core::SubpassData &,
			sf::CommandBuffer &) override;

	const VertexAttachmentHandle *_vertexHandle = nullptr;
};

} // namespace stappler::xenolith::basic2d::soft

#endif

#endif /* XENOLITH_RENDERER_BASIC2D_BACKEND_SOFT_XL2DSOFTFLATPASS_H_ */
