/**
 Copyright (c) 2021 Roman Katuntsev <sbkarr@stappler.org>
 Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>
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

#ifndef XENOLITH_BACKEND_VK_XLVKDEVICEQUEUE_H_
#define XENOLITH_BACKEND_VK_XLVKDEVICEQUEUE_H_

#include "XLCoreEnum.h"
#include "XLCoreObject.h"
#include "XLVkInstance.h"
#include "XLVkLoop.h"
#include "XLCoreDevice.h"
#include "XLCoreLoop.h"
#include "XLCoreDeviceQueue.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk {

class Device;
class DeviceQueue;
class CommandPool;
class Semaphore;
class Fence;
class Loop;
class Image;
class ImageView;
class Buffer;
class RenderPass;
class Framebuffer;
class GraphicPipeline;
class ComputePipeline;
class CommandBuffer;
class DeviceMemoryPool;
class PipelineLayout;
class DescriptorPool;
class QueryPool;
struct DescriptorSetBindings;

using PipelineDescriptor = core::PipelineDescriptor;

class SP_PUBLIC DeviceQueue final : public core::DeviceQueue {
public:
	using FrameSync = core::FrameSync;
	using FrameHandle = core::FrameHandle;

	virtual ~DeviceQueue();

	bool init(Device &device, VkQueue queue, uint32_t index, core::QueueFlags flags);

	virtual Status waitIdle() override;

	VkQueue getQueue() const { return _queue; }

protected:
	virtual Status doSubmit(const FrameSync *, core::CommandPool *, core::Fence &,
			SpanView<const core::CommandBuffer *>,
			core::DeviceIdleFlags = core::DeviceIdleFlags::None) override;

	VkQueue _queue;
};


enum class BufferLevel {
	Primary = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
	Secondary = VK_COMMAND_BUFFER_LEVEL_SECONDARY,
};

struct SP_PUBLIC QueueFamilyTransfer {
	uint32_t srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	uint32_t dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
};

struct SP_PUBLIC ImageMemoryBarrier {
	ImageMemoryBarrier() = default;

	ImageMemoryBarrier(Image *, XAccessFlags src, XAccessFlags dst, XImageLayout old,
			XImageLayout _new);
	ImageMemoryBarrier(Image *, XAccessFlags src, XAccessFlags dst, XImageLayout old,
			XImageLayout _new, VkImageSubresourceRange);
	ImageMemoryBarrier(Image *, XAccessFlags src, XAccessFlags dst, XImageLayout old,
			XImageLayout _new, QueueFamilyTransfer);
	ImageMemoryBarrier(Image *, XAccessFlags src, XAccessFlags dst, XImageLayout old,
			XImageLayout _new, QueueFamilyTransfer, VkImageSubresourceRange);

	ImageMemoryBarrier(VkImage, XAccessFlags src, XAccessFlags dst, XImageLayout old,
			XImageLayout _new, VkImageAspectFlags);
	ImageMemoryBarrier(VkImage, XAccessFlags src, XAccessFlags dst, XImageLayout old,
			XImageLayout _new, VkImageSubresourceRange);
	ImageMemoryBarrier(VkImage, XAccessFlags src, XAccessFlags dst, XImageLayout old,
			XImageLayout _new, QueueFamilyTransfer, VkImageAspectFlags);
	ImageMemoryBarrier(VkImage, XAccessFlags src, XAccessFlags dst, XImageLayout old,
			XImageLayout _new, QueueFamilyTransfer, VkImageSubresourceRange);

	ImageMemoryBarrier(const VkImageMemoryBarrier &);

	explicit operator bool() const {
		return srcAccessMask != 0 || dstAccessMask != 0 || oldLayout != VK_IMAGE_LAYOUT_UNDEFINED
				|| newLayout != VK_IMAGE_LAYOUT_UNDEFINED || vkimage != VK_NULL_HANDLE;
	}

	XAccessFlags srcAccessMask = 0;
	XAccessFlags dstAccessMask = 0;
	XImageLayout oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	XImageLayout newLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	QueueFamilyTransfer familyTransfer;
	VkImage vkimage = VK_NULL_HANDLE;
	VkImageSubresourceRange subresourceRange;
	Image *image = nullptr;
};

struct SP_PUBLIC BufferMemoryBarrier {
	BufferMemoryBarrier() = default;

	BufferMemoryBarrier(Buffer *, XAccessFlags src, XAccessFlags dst);
	BufferMemoryBarrier(Buffer *, XAccessFlags src, XAccessFlags dst, VkDeviceSize off,
			VkDeviceSize size);
	BufferMemoryBarrier(Buffer *, XAccessFlags src, XAccessFlags dst, QueueFamilyTransfer);
	BufferMemoryBarrier(Buffer *, XAccessFlags src, XAccessFlags dst, QueueFamilyTransfer,
			VkDeviceSize off, VkDeviceSize size);

	BufferMemoryBarrier(VkBuffer, XAccessFlags src, XAccessFlags dst);
	BufferMemoryBarrier(VkBuffer, XAccessFlags src, XAccessFlags dst, VkDeviceSize off,
			VkDeviceSize size);
	BufferMemoryBarrier(VkBuffer, XAccessFlags src, XAccessFlags dst, QueueFamilyTransfer);
	BufferMemoryBarrier(VkBuffer, XAccessFlags src, XAccessFlags dst, QueueFamilyTransfer,
			VkDeviceSize off, VkDeviceSize size);

	BufferMemoryBarrier(const VkBufferMemoryBarrier &);

	explicit operator bool() const {
		return srcAccessMask != 0 || dstAccessMask != 0 || vkbuffer != VK_NULL_HANDLE;
	}

	XAccessFlags srcAccessMask = 0;
	XAccessFlags dstAccessMask = 0;
	QueueFamilyTransfer familyTransfer;
	VkBuffer vkbuffer = VK_NULL_HANDLE;
	VkDeviceSize offset = 0;
	VkDeviceSize size = VK_WHOLE_SIZE;
	Buffer *buffer = nullptr;
};

struct CommandBufferInfo {
	static constexpr VkCommandBufferUsageFlagBits DefaultFlags =
			VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	VkCommandBufferUsageFlagBits usageFlags = DefaultFlags;
	BufferLevel level = BufferLevel::Primary;
	uint32_t timestampQueries = 0;
};

class SP_PUBLIC CommandBuffer : public core::CommandBuffer {
public:
	virtual ~CommandBuffer();

	bool init(const CommandPool *, const DeviceTable *, VkCommandBuffer,
			Vector<Rc<DescriptorPool>> &&descriptors, CommandBufferInfo &&info);
	void invalidate();

	void cmdPipelineBarrier(XPipelineStage, XPipelineStage, VkDependencyFlags,
			SpanView<ImageMemoryBarrier>);
	void cmdPipelineBarrier(XPipelineStage, XPipelineStage, VkDependencyFlags,
			SpanView<BufferMemoryBarrier>);
	void cmdPipelineBarrier(XPipelineStage, XPipelineStage, VkDependencyFlags,
			SpanView<BufferMemoryBarrier>, SpanView<ImageMemoryBarrier>);
	void cmdGlobalBarrier(XPipelineStage, XPipelineStage, VkDependencyFlags, XAccessFlags src,
			XAccessFlags dst);

	void cmdCopyBuffer(Buffer *src, Buffer *dst);
	void cmdCopyBuffer(Buffer *src, Buffer *dst, VkDeviceSize srcOffset, VkDeviceSize dstOffset,
			VkDeviceSize size);
	void cmdCopyBuffer(Buffer *src, Buffer *dst, SpanView<VkBufferCopy>);
	void cmdCopyBuffer(VkBuffer src, VkBuffer dst, SpanView<VkBufferCopy>);

	void cmdCopyImage(Image *src, XImageLayout, Image *dst, XImageLayout,
			VkFilter filter = VK_FILTER_LINEAR);
	void cmdCopyImage(Image *src, XImageLayout, Image *dst, XImageLayout, const VkImageCopy &copy);
	void cmdCopyImage(Image *src, XImageLayout, Image *dst, XImageLayout, SpanView<VkImageCopy>);

	void cmdCopyBufferToImage(Buffer *, Image *, XImageLayout, VkDeviceSize offset);
	void cmdCopyBufferToImage(Buffer *, Image *, XImageLayout, SpanView<VkBufferImageCopy>);
	void cmdCopyBufferToImage(VkBuffer, VkImage, XImageLayout, SpanView<VkBufferImageCopy>);

	void cmdCopyImageToBuffer(Image *, XImageLayout, Buffer *buf, VkDeviceSize offset);
	void cmdCopyImageToBuffer(Image *, XImageLayout, Buffer *buf, SpanView<VkBufferImageCopy>);

	void cmdClearColorImage(Image *, XImageLayout, const Color4F &);

	void cmdBeginRenderPass(RenderPass *pass, Framebuffer *fb, VkSubpassContents subpass,
			bool alt = false);
	void cmdEndRenderPass();

	void cmdSetViewport(uint32_t firstViewport, SpanView<VkViewport> viewports);
	void cmdSetScissor(uint32_t firstScissor, SpanView<VkRect2D> scissors);

	void cmdBindPipeline(GraphicPipeline *);
	void cmdBindPipeline(ComputePipeline *);

	void cmdBindPipelineWithDescriptors(const core::GraphicPipelineData *, uint32_t firstSet = 0);
	void cmdBindPipelineWithDescriptors(const core::ComputePipelineData *, uint32_t firstSet = 0);

	void cmdBindIndexBuffer(Buffer *, VkDeviceSize offset, VkIndexType indexType);

	void cmdBindDescriptorSets(RenderPass *, uint32_t index, uint32_t firstSet = 0);
	void cmdBindDescriptorSets(RenderPass *, const Rc<DescriptorPool> &, uint32_t firstSet = 0);
	void cmdBindDescriptorSets(RenderPass *, SpanView<VkDescriptorSet>, uint32_t firstSet = 0);

	void cmdBindGraphicDescriptorSets(VkPipelineLayout, SpanView<VkDescriptorSet>,
			uint32_t firstSet = 0);
	void cmdBindComputeDescriptorSets(VkPipelineLayout, SpanView<VkDescriptorSet>,
			uint32_t firstSet = 0);

	void cmdDraw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex,
			uint32_t firstInstance);
	void cmdDrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex,
			int32_t vertexOffset, uint32_t firstInstance);

	void cmdDrawIndirect(Buffer *, uint64_t offset, uint32_t count, uint32_t stride);

	void cmdPushConstants(PipelineLayout *layout, XPipelineStage stageFlags, uint32_t offset,
			BytesView);
	void cmdPushConstants(XPipelineStage stageFlags, uint32_t offset, BytesView);

	void cmdFillBuffer(Buffer *, uint32_t data);
	void cmdFillBuffer(Buffer *, VkDeviceSize dstOffset, VkDeviceSize size, uint32_t data);

	// count in group blocks
	void cmdDispatch(uint32_t groupCountX, uint32_t groupCountY = 1, uint32_t groupCountZ = 1);

	// count in individual elements, not in blocks
	void cmdDispatchPipeline(const core::ComputePipelineData *, uint32_t countX,
			uint32_t countY = 1, uint32_t countZ = 1);

	uint32_t cmdWriteTimestamp(XPipelineStage stage, uint32_t tag);

	uint32_t cmdNextSubpass();

	VkCommandBuffer getBuffer() const { return _buffer; }

	uint32_t getCurrentSubpass() const { return _currentSubpass; }

	uint32_t getBoundLayoutIndex(VkPipelineBindPoint) const;
	PipelineLayout *getBoundLayout(VkPipelineBindPoint) const;

	uint32_t getBoundLayoutIndex(core::PassType) const;
	PipelineLayout *getBoundLayout(core::PassType) const;

	void writeImageTransfer(uint32_t sourceFamily, uint32_t targetFamily, const Rc<Buffer> &,
			const Rc<Image> &);

	uint64_t bindBufferAddress(NotNull<core::BufferObject>);

	virtual void bindBuffer(core::BufferObject *) override;

	bool isNextTimestampAvailable() const;

protected:
	struct BindPoint {
		VkPipelineBindPoint point;
		uint32_t boundLayoutIndex = 0;
		Rc<PipelineLayout> boundLayout;
		Vector<VkDescriptorSet> boundSets;
	};

	bool updateBoundSets(BindPoint &point, SpanView<VkDescriptorSet>, uint32_t firstSet);

	BindPoint *getBindPoint(VkPipelineBindPoint);
	const BindPoint *getBindPoint(VkPipelineBindPoint) const;

	BindPoint *getBindPoint(core::PassType);
	const BindPoint *getBindPoint(core::PassType) const;

	CommandBufferInfo _info;

	Vector<Rc<DescriptorPool>> _availableDescriptors;
	Set<Rc<DescriptorPool>> _usedDescriptors;

	const DeviceTable *_table = nullptr;
	VkCommandBuffer _buffer = VK_NULL_HANDLE;

	Set<Rc<DescriptorSetBindings>> _descriptorSets;
	Set<Rc<DeviceMemoryPool>> _memPool;

	std::array<BindPoint, 2> _bindPoints;

	const GraphicPipeline *_boundGraphicPipeline = nullptr;
	const ComputePipeline *_boundComputePipeline = nullptr;

	Rc<QueryPool> _timestampQueryPool;
};

class SP_PUBLIC CommandPool : public core::CommandPool {
public:
	virtual ~CommandPool() = default;

	bool init(Device &dev, uint32_t familyIdx, core::QueueFlags c = core::QueueFlags::Graphics,
			bool transient = true);

	VkCommandPool getCommandPool() const { return _commandPool; }

	virtual const core::CommandBuffer *recordBuffer(core::Device &dev,
			const Callback<bool(core::CommandBuffer &)> &) override;

	const CommandBuffer *recordBuffer(Device &dev, Vector<Rc<DescriptorPool>> &&descriptors,
			const Callback<bool(CommandBuffer &)> &, CommandBufferInfo && = CommandBufferInfo());

	void freeDefaultBuffers(Device &dev, Vector<VkCommandBuffer> &);
	virtual void reset(core::Device &dev) override;

	void autorelease(Rc<Ref> &&);

protected:
	void recreatePool(Device &dev);

	VkCommandPool _commandPool = VK_NULL_HANDLE;
};

class SP_PUBLIC QueryPool : public core::QueryPool {
public:
	virtual ~QueryPool() = default;

	bool init(Device &dev, uint32_t familyIdx, core::QueueFlags c, const core::QueryPoolInfo &);

	virtual void reset(core::Device &dev) override;

	VkQueryPool getPool() const { return _queryPool; }

	Status getResults(Device &dev, const Callback<void(SpanView<uint64_t>, uint32_t tag)> &);

protected:
	VkQueryPool _queryPool = VK_NULL_HANDLE;
};

} // namespace stappler::xenolith::vk

#endif /* XENOLITH_BACKEND_VK_XLVKDEVICEQUEUE_H_ */
