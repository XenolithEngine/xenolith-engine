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

#ifndef XENOLITH_BACKEND_MTL_XLMTLQUEUEPASS_H_
#define XENOLITH_BACKEND_MTL_XLMTLQUEUEPASS_H_

#include "XLMtlObject.h"
#include "XLCoreQueuePass.h"
#include "XLCoreAttachment.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::mtl {

class Loop;

// Wrapper for MTLCommandBuffer with render/compute encoder lifecycle.
// Resource binding in Metal goes directly through the encoders
// (setVertexBuffer / setFragmentTexture / ...), there is no descriptor set
// object; per-frame binding logic lives in the QueuePassHandle recording.
class SP_PUBLIC CommandBuffer final : public core::CommandBuffer {
public:
	// frame descriptors resolved against attachments; applied on
	// cmdBindPipeline at sequential per-type argument table indexes
	// (buffers from [[buffer(0)]], textures from [[texture(0)]], samplers
	// from [[sampler(0)]] - see the binding conventions in XLMtl.h)
	struct BindingEntry {
		core::DescriptorType type = core::DescriptorType::Unknown;
		Rc<Ref> object; // Buffer / core::ImageView / core::Sampler
		uint64_t offset = 0;
	};

	// per-frame bindings: [PipelineLayoutData::index][declaration order]
	using FrameBindings = Vector<Vector<BindingEntry>>;

	virtual ~CommandBuffer();

	bool init(Device &);

	void setFrameBindings(const FrameBindings *bindings) { _bindings = bindings; }

	// bind texture set argument buffer at TextureSetBufferIndex and make its
	// textures resident for the current encoder
	void cmdBindTextureSet(const core::PipelineLayoutData *, NotNull<core::TextureSet>);

	void endRenderPass();

	void beginComputePass();
	void endComputePass();

	void cmdBindPipeline(const core::GraphicPipelineData *);
	void cmdBindPipeline(const core::ComputePipelineData *);

	void cmdDraw(uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t firstVertex = 0,
			uint32_t firstInstance = 0);
	// rect is clamped to the render target extent
	void cmdSetScissor(uint32_t x, uint32_t y, uint32_t width, uint32_t height);
	void cmdDrawIndexed(NotNull<Buffer> indexBuffer, uint32_t indexCount,
			uint32_t instanceCount = 1, uint32_t firstIndex = 0, int32_t baseVertex = 0,
			uint32_t firstInstance = 0);

	// dispatch groupsX*groupsY*groupsZ threadgroups of threadsX*threadsY*threadsZ
	// threads. Unlike GLSL/WGSL, MSL carries no workgroup size in the kernel
	// source - the recording callback passes the kernel's logical group size
	// explicitly (it owns both the MSL text and the dispatch math)
	void cmdDispatch(uint32_t groupsX, uint32_t groupsY = 1, uint32_t groupsZ = 1,
			uint32_t threadsX = 1, uint32_t threadsY = 1, uint32_t threadsZ = 1);

#if __OBJC__
	// begins a render command encoder for the given pass descriptor
	bool beginRenderPass(MTLRenderPassDescriptor *, Extent2 renderExtent);

	// ends open encoders; the buffer is ready to be committed by the caller
	id<MTLCommandBuffer> finish();

	id<MTLCommandBuffer> getBuffer() const {
		return bridgeHandle<id<MTLCommandBuffer>>(_buffer);
	}
	id<MTLRenderCommandEncoder> getRenderEncoder() const {
		return bridgeHandle<id<MTLRenderCommandEncoder>>(_renderEncoder);
	}
	id<MTLComputeCommandEncoder> getComputeEncoder() const {
		return bridgeHandle<id<MTLComputeCommandEncoder>>(_computeEncoder);
	}
#endif

	// extent of the current render pass target (for scissor clamping)
	Extent2 getRenderExtent() const { return _renderExtent; }

protected:
	void applyBindings(uint32_t layoutIndex);

	Device *_device = nullptr;
	void *_buffer = nullptr; // __bridge_retained id<MTLCommandBuffer>
	void *_renderEncoder = nullptr; // __bridge_retained id<MTLRenderCommandEncoder>
	void *_computeEncoder = nullptr; // __bridge_retained id<MTLComputeCommandEncoder>
	Extent2 _renderExtent = Extent2(0, 0);
	const FrameBindings *_bindings = nullptr;
};

class SP_PUBLIC QueuePassHandle : public core::QueuePassHandle {
public:
	virtual ~QueuePassHandle();

	virtual bool prepare(core::FrameQueue &, Function<void(bool)> &&) override;
	virtual void submit(core::FrameQueue &, Rc<core::FrameSync> &&,
			Function<void(bool)> &&onSubmited, Function<void(bool)> &&onComplete) override;

protected:
	// per-subpass recording hook: the default runs SubpassData::commandsCallback
	virtual void recordSubpass(core::FrameQueue &, const core::SubpassData &, CommandBuffer &);

	// resolve descriptors against frame attachments into per-layout binding
	// tables (see CommandBuffer::FrameBindings)
	bool buildBindings(core::FrameQueue &);

	Rc<CommandBuffer> recordCommands(core::FrameQueue &);

	Device *_device = nullptr;
	CommandBuffer::FrameBindings _bindings;
};

class SP_PUBLIC ImageAttachment
	: public core::AttachmentTyped<core::AttachmentHandle, core::ImageAttachment> {
public:
	virtual ~ImageAttachment() = default;
};

class SP_PUBLIC BufferAttachmentHandle : public core::AttachmentHandle {
public:
	struct BufferView {
		Rc<Buffer> buffer;
		uint64_t offset = 0;
		uint64_t size = 0; // 0 for whole buffer
	};

	virtual ~BufferAttachmentHandle() = default;

	void clearBufferViews() { _buffers.clear(); }
	void addBufferView(Rc<Buffer> &&, uint64_t offset = 0, uint64_t size = 0);

	SpanView<BufferView> getBuffers() const { return _buffers; }

protected:
	Vector<BufferView> _buffers;
};

// can not use core::AttachmentTyped: wires static buffers into the handle
class SP_PUBLIC BufferAttachment : public core::BufferAttachment {
public:
	virtual ~BufferAttachment() = default;

	virtual Rc<core::AttachmentHandle> makeFrameHandle(const core::FrameQueue &) override;
};

class SP_PUBLIC QueuePass : public core::QueuePassTyped<QueuePassHandle> {
public:
	virtual ~QueuePass() = default;
};

// Metal render passes are transient encoder configurations
// (MTLRenderPassDescriptor built at record time); this object provides pass
// identity for the frame graph (framebuffer cache)
class SP_PUBLIC RenderPass final : public core::RenderPass {
public:
	virtual ~RenderPass() = default;

	bool init(Device &, const core::QueuePassData &);
};

} // namespace stappler::xenolith::mtl

#endif /* XENOLITH_BACKEND_MTL_XLMTLQUEUEPASS_H_ */
