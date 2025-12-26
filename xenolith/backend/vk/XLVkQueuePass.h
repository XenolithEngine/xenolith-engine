/**
 Copyright (c) 2021-2022 Roman Katuntsev <sbkarr@stappler.org>
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

#ifndef XENOLITH_BACKEND_VK_XLVKQUEUEPASS_H_
#define XENOLITH_BACKEND_VK_XLVKQUEUEPASS_H_

#include "XLVkSync.h"
#include "XLVkObject.h"
#include "XLCoreQueuePass.h"
#include <vulkan/vulkan_core.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk {

class CommandBuffer;
class Device;
class Loop;
class Fence;
class CommandPool;
class ImageAttachmentHandle;
class BufferAttachmentHandle;
class DescriptorPool;

struct SP_PUBLIC MaterialTransferData {
	Rc<core::Material> material;
	Rc<Buffer> source;
	Rc<Buffer> target;
};

class SP_PUBLIC QueuePass : public core::QueuePass {
public:
	virtual ~QueuePass();

	virtual bool init(QueuePassBuilder &passBuilder) override;
	virtual void invalidate() override;

	virtual Rc<core::QueuePassHandle> makeFrameHandle(const FrameQueue &) override;

	core::QueueFlags getQueueOps() const { return _queueOps; }

protected:
	core::QueueFlags _queueOps = core::QueueFlags::Graphics;
};

class SP_PUBLIC QueuePassHandle : public core::QueuePassHandle {
public:
	static VkRect2D rotateScissor(const core::FrameConstraints &constraints, const URect &scissor);

	struct ImageInputOutputBarrier {
		vk::ImageMemoryBarrier input;
		vk::ImageMemoryBarrier output;
		core::PipelineStage inputFrom;
		core::PipelineStage inputTo;
		core::PipelineStage outputFrom;
		core::PipelineStage outputTo;
	};

	struct BufferInputOutputBarrier {
		vk::BufferMemoryBarrier input;
		vk::BufferMemoryBarrier output;
		core::PipelineStage inputFrom;
		core::PipelineStage inputTo;
		core::PipelineStage outputFrom;
		core::PipelineStage outputTo;
	};

	virtual ~QueuePassHandle();
	void invalidate();

	virtual bool prepare(FrameQueue &, Function<void(bool)> &&) override;
	virtual void submit(FrameQueue &, Rc<FrameSync> &&, Function<void(bool)> &&onSubmited,
			Function<void(bool)> &&onComplete) override;
	virtual void finalize(FrameQueue &, bool) override;

	virtual core::QueueFlags getQueueOps() const;

	ImageInputOutputBarrier getImageInputOutputBarrier(Device *, Image *, core::AttachmentHandle &,
			const VkImageSubresourceRange &) const;
	BufferInputOutputBarrier getBufferInputOutputBarrier(Device *, Buffer *,
			core::AttachmentHandle &, VkDeviceSize offset, VkDeviceSize size) const;

	void setQueueIdleFlags(core::DeviceIdleFlags);

protected:
	virtual Vector<const core::CommandBuffer *> doPrepareCommands(FrameHandle &);
	virtual bool doSubmit(FrameHandle &frame, Function<void(bool)> &&onSubmited);

	virtual void doSubmitted(FrameHandle &, Function<void(bool)> &&, bool, Rc<core::Fence> &&);

	// called before OnComplete event sended to FrameHandle (so, before any finalization)
	virtual void doComplete(FrameQueue &, Function<void(bool)> &&, bool);

	virtual void doProcessQueries(FrameQueue &, SpanView<Rc<core::QueryPool>> queries);

	virtual void doFinalizeTransfer(core::MaterialSet *materials,
			Vector<ImageMemoryBarrier> &outputImageBarriers,
			Vector<BufferMemoryBarrier> &outputBufferBarriers);

	virtual Vector<MaterialTransferData> updateMaterials(FrameHandle &iframe,
			NotNull<core::MaterialSet> data, SpanView<Rc<core::Material>> materials,
			SpanView<core::MaterialId> dynamicMaterials,
			SpanView<core::MaterialId> materialsToRemove);

	vk::ComputePipeline *getComputePipelineByName(uint32_t subpass, StringView) const;
	vk::ComputePipeline *getComputePipelineBySubName(uint32_t subpass, StringView) const;

	vk::GraphicPipeline *getGraphicPipelineByName(uint32_t subpass, StringView) const;
	vk::GraphicPipeline *getGraphicPipelineBySubName(uint32_t subpass, StringView) const;

	core::DeviceIdleFlags _queueIdleFlags = core::DeviceIdleFlags::None;
	Function<void(bool)> _onPrepared;
	bool _valid = true;
	bool _commandsReady = false;
	bool _descriptorsReady = false;

	Device *_device = nullptr;
	Rc<CommandPool> _pool;
	Rc<DeviceQueue> _queue;
	Vector<Rc<DescriptorPool>> _descriptors;
	Vector<const core::CommandBuffer *> _buffers;
	Rc<FrameSync> _sync;
	core::FrameConstraints _constraints;
};

} // namespace stappler::xenolith::vk

#endif /* XENOLITH_BACKEND_VK_XLVKQUEUEPASS_H_ */
