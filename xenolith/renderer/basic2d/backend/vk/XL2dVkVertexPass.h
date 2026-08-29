/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
 Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#ifndef XENOLITH_RENDERER_BASIC2D_BACKEND_VK_XL2DVKVERTEXPASS_H_
#define XENOLITH_RENDERER_BASIC2D_BACKEND_VK_XL2DVKVERTEXPASS_H_

#include "XL2dVkMaterial.h"
#include "XLCoreFrameCapture.h"
#include "XL2dCommandList.h"
#include "XL2dVkParticlePass.h"
#include "XLCoreFrameDamage.h"

#if MODULE_XENOLITH_BACKEND_VK

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d::vk {

class SP_PUBLIC VertexAttachment : public core::GenericAttachment {
public:
	virtual ~VertexAttachment() = default;

	virtual bool init(AttachmentBuilder &builder, const AttachmentData *, bool flatOrder = false,
			bool damageTracked = false);

	const AttachmentData *getMaterials() const { return _materials; }

	// Queues without a depth buffer (FlatPass) need every draw emitted in painter's order.
	bool isFlatOrder() const { return _flatOrder; }

	// Whether this queue asked for per-frame damage tracking at all.
	bool isDamageTracked() const { return _damageTracked; }

	// Remote render session: the per-frame input this attachment consumes is a FrameContextHandle2d.
	virtual Rc<core::AttachmentInputData> makeInputData(
			NotNull<core::RenderClientChannel> client) const override {
		auto ret = Rc<FrameContextHandle2d>::alloc();
		ret->clock = sprt::platform::clock(sprt::platform::ClockType::Monotonic);
		ret->client = client;
		return ret;
	}

protected:
	using GenericAttachment::init;

	virtual Rc<AttachmentHandle> makeFrameHandle(const FrameQueue &) override;

	const AttachmentData *_materials = nullptr;
	bool _flatOrder = false;
	bool _damageTracked = false;
};

class SP_PUBLIC VertexAttachmentHandle : public core::AttachmentHandle {
public:
	virtual ~VertexAttachmentHandle() = default;

	bool isFlatOrder() const {
		return static_cast<VertexAttachment *>(_attachment.get())->isFlatOrder();
	}

	bool isDamageTracked() const {
		return static_cast<VertexAttachment *>(_attachment.get())->isDamageTracked();
	}

	virtual bool setup(FrameQueue &, Function<void(bool)> &&) override;

	virtual void submitInput(FrameQueue &, Rc<core::AttachmentInputData> &&,
			Function<void(bool)> &&) override;

	SpanView<VertexSpan> getVertexData() const { return _spans; }

	// The Overlay level, kept apart from the content spans so the pass can record it after the frame
	// has been copied out. Empty in the ordinary case, and then the second pass is never opened.
	SpanView<VertexSpan> getOverlayData() const { return _overlaySpans; }

	SpanView<VertexSpan> getShadowSolidData() const { return _shadowSolidSpans; }
	SpanView<VertexSpan> getShadowSdfData() const { return _shadowSdfSpans; }
	const Rc<Buffer> &getIndexes() const { return _indexes; }
	const Rc<Buffer> &getVertexes() const { return _vertexes; }
	const Rc<Buffer> &getTransforms() const { return _transforms; }

	float getMaxShadowValue() const { return _maxShadowValue; }

	const core::MaterialSet *getMaterialSet() const { return _materialSet; }

	const Rc<FrameContextHandle2d> &getCommands() const;

	bool empty() const;

	void loadData(Rc<FrameContextHandle2d> &&data, Rc<Buffer> &&indexes, Rc<Buffer> &&vertexes,
			Rc<Buffer> &&transforms, Vector<VertexSpan> &&spans, Vector<VertexSpan> &&overlaySpans,
			Vector<VertexSpan> &&shadowSolidSpans, Vector<VertexSpan> &&shadowSdfSpans,
			float maxShadowValue, Rc<core::FrameDamageState> &&damage);

	// Written once on the worker that builds the vertex data, before the attachment signals
	// readiness; read afterwards on the loop thread at present time.
	const Rc<core::FrameDamageState> &getDamageState() const { return _damage; }

protected:
	Rc<FrameContextHandle2d> _commands;
	Rc<Buffer> _indexes;
	Rc<Buffer> _vertexes;
	Rc<Buffer> _transforms;
	Vector<VertexSpan> _spans;
	Vector<VertexSpan> _overlaySpans;
	Vector<VertexSpan> _shadowSolidSpans;
	Vector<VertexSpan> _shadowSdfSpans;

	Rc<core::MaterialSet> _materialSet;
	const MaterialAttachmentHandle *_materials = nullptr;
	float _maxShadowValue = 0.0f;
	Rc<core::FrameDamageState> _damage;
};

class SP_PUBLIC VertexPass : public QueuePass {
public:
	using AttachmentHandle = core::AttachmentHandle;

	static core::ImageFormat selectDepthFormat(SpanView<core::ImageFormat> formats);

	virtual ~VertexPass() = default;

	const AttachmentData *getVertexes() const { return _vertexes; }
	const AttachmentData *getMaterials() const { return _materials; }
	const AttachmentData *getParticles() const { return _particles; }

	// Null for a queue that cannot capture; the handle then records no copy at all.
	const AttachmentData *getCapture() const { return _capture; }

	// The image this pass draws into - the presented one, and therefore what a capture copies out of.
	const AttachmentData *getOutput() const { return _output; }

	virtual Rc<QueuePassHandle> makeFrameHandle(const FrameQueue &) override;

protected:
	using QueuePass::init;

	const AttachmentData *_output = nullptr;
	const AttachmentData *_shadow = nullptr;
	const AttachmentData *_depth2d = nullptr;
	const AttachmentData *_depthSdf = nullptr;

	const AttachmentData *_vertexes = nullptr;
	const AttachmentData *_materials = nullptr;
	const AttachmentData *_particles = nullptr;
	const AttachmentData *_capture = nullptr;
};

class SP_PUBLIC VertexPassHandle : public QueuePassHandle {
public:
	static constexpr uint32_t TimestampBeginTag = 0;
	static constexpr uint32_t TimestampEndTag = 1;

	virtual ~VertexPassHandle() { }

	virtual bool prepare(FrameQueue &, Function<void(bool)> &&) override;

protected:
	virtual Vector<const core::CommandBuffer *> doPrepareCommands(FrameHandle &) override;

	virtual void doProcessQueries(FrameQueue &, SpanView<Rc<core::QueryPool>> queries) override;

	// Copy out whatever this frame was asked for, after the render pass has ended and its output
	// barriers have been written - so the source is already in its final layout here, and has to be
	// put back into it.
	void recordFrameCapture(CommandBuffer &);

	// Resolved together in prepare(); all null/false when no capture was asked for, and then
	// recordFrameCapture records nothing at all.
	const core::FrameCaptureInput *_captureInput = nullptr;
	Image *_captureSource = nullptr;

	// Whether the source is the swapchain image. It decides the layout the pass left it in -
	// PresentSrc for a presented image, TransferSrcOptimal for an offscreen one - which is both
	// what the copy has to transition from and what it must restore.
	bool _captureSourcePresented = false;

	// Draw the Overlay level in a second render pass instance over the same framebuffer. Records
	// nothing when there is nothing on it - which is the ordinary case, and what makes an idle
	// overlay free.
	void recordOverlayPass(CommandBuffer &);

	// What goes INSIDE that instance. A queue with more than one subpass has to walk them all, so
	// this is the part a multi-subpass pass overrides.
	virtual void recordOverlaySubpasses(CommandBuffer &, SpanView<VertexSpan> spans);

	virtual void prepareRenderPass(CommandBuffer &);
	virtual void prepareMaterialCommands(core::MaterialSet *materials, CommandBuffer &);

	// One run of the draw loop over a given set of spans; see the definition
	void drawSpans(core::MaterialSet *materials, CommandBuffer &, SpanView<VertexSpan> spans);

	virtual void finalizeRenderPass(CommandBuffer &);

	void clearDynamicState(CommandBuffer &buf);
	void applyDynamicState(const FrameContextHandle2d *commands, CommandBuffer &buf,
			uint32_t stateId);

	const VertexAttachmentHandle *_vertexBuffer = nullptr;
	const MaterialAttachmentHandle *_materialBuffer = nullptr;
	const ParticleEmitterAttachmentHandle *_particles = nullptr;

	StateId _dynamicStateId = maxOf<StateId>();
	DrawStateValues _dynamicState;
};

} // namespace stappler::xenolith::basic2d::vk

#endif

#endif /* XENOLITH_RENDERER_BASIC2D_BACKEND_VK_XL2DVKVERTEXPASS_H_ */
