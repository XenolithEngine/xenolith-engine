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

#ifndef XENOLITH_BACKEND_WEBGPU_XLWGPUQUEUEPASS_H_
#define XENOLITH_BACKEND_WEBGPU_XLWGPUQUEUEPASS_H_

#include "XLWgpuObject.h"
#include "XLCoreQueuePass.h"
#include "XLCoreAttachment.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::webgpu {

class Loop;

class SP_PUBLIC CommandBuffer final : public core::CommandBuffer {
public:
	// per-frame bind groups: [PipelineLayoutData::index][DescriptorSetData::index]
	using FrameBindGroups = Vector<Vector<WGPUBindGroup>>;

	virtual ~CommandBuffer();

	bool init(Device &);

	void setFrameBindGroups(const FrameBindGroups *groups) { _bindGroups = groups; }

	void beginRenderPass(SpanView<WGPURenderPassColorAttachment>,
			const WGPURenderPassDepthStencilAttachment * = nullptr,
			Extent2 renderExtent = Extent2(0, 0));

	// extent of the current render pass target (for scissor clamping)
	Extent2 getRenderExtent() const { return _renderExtent; }
	void endRenderPass();

	void beginComputePass();
	void endComputePass();

	// bind pipeline with bind groups of its pipeline layout
	void cmdBindPipeline(const core::GraphicPipelineData *);
	void cmdBindPipeline(const core::ComputePipelineData *);

	// bind texture set as the last bind group of the layout
	void cmdBindTextureSet(const core::PipelineLayoutData *, NotNull<core::TextureSet>);

	// bind an explicit bind group at the layout's last index (fallback
	// material path: bind group per material)
	void cmdBindMaterialGroup(const core::PipelineLayoutData *, WGPUBindGroup);

	void cmdDraw(uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t firstVertex = 0,
			uint32_t firstInstance = 0);
	// rect must be pre-clamped to the render target extent (WebGPU validates)
	void cmdSetScissor(uint32_t x, uint32_t y, uint32_t width, uint32_t height);
	void cmdBindIndexBuffer(NotNull<Buffer>, WGPUIndexFormat = WGPUIndexFormat_Uint32);
	void cmdDrawIndexed(uint32_t indexCount, uint32_t instanceCount = 1, uint32_t firstIndex = 0,
			int32_t baseVertex = 0, uint32_t firstInstance = 0);
	void cmdDispatch(uint32_t groupsX, uint32_t groupsY = 1, uint32_t groupsZ = 1);

	WGPUCommandBuffer finish();

	WGPUCommandEncoder getEncoder() const { return _encoder; }
	WGPURenderPassEncoder getRenderPassEncoder() const { return _renderPass; }
	WGPUComputePassEncoder getComputePassEncoder() const { return _computePass; }

protected:
	void bindLayoutGroups(uint32_t layoutIndex);

	Device *_device = nullptr;
	WGPUCommandEncoder _encoder = nullptr;
	WGPURenderPassEncoder _renderPass = nullptr;
	Extent2 _renderExtent = Extent2(0, 0);
	WGPUComputePassEncoder _computePass = nullptr;
	WGPUCommandBuffer _commands = nullptr;
	const FrameBindGroups *_bindGroups = nullptr;
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

	bool buildBindGroups(core::FrameQueue &);
	void clearBindGroups();

	Rc<CommandBuffer> recordCommands(core::FrameQueue &);

	Device *_device = nullptr;
	CommandBuffer::FrameBindGroups _bindGroups;
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

// WebGPU render passes are transient encoder objects; this object provides
// pass identity for the frame graph (framebuffer cache) and holds compiled
// bind group / pipeline layouts for the pass
class SP_PUBLIC RenderPass final : public core::RenderPass {
public:
	struct LayoutData {
		Vector<WGPUBindGroupLayout> bindGroupLayouts; // by descriptor set index
		WGPUPipelineLayout pipelineLayout = nullptr;
	};

	virtual ~RenderPass();

	bool init(Device &, const core::QueuePassData &);

	const LayoutData *getLayout(uint32_t index) const;

protected:
	bool makeLayout(Device &, const core::PipelineLayoutData &, LayoutData &);

	Vector<LayoutData> _layouts; // by PipelineLayoutData::index
};

} // namespace stappler::xenolith::webgpu

#endif /* XENOLITH_BACKEND_WEBGPU_XLWGPUQUEUEPASS_H_ */
