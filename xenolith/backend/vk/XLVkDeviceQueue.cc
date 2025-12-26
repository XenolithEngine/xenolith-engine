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

#include "XLVkDeviceQueue.h"
#include "SPCore.h"
#include "XLCoreDeviceQueue.h"
#include "XLCoreEnum.h"
#include "XLCoreFrameQueue.h"
#include "XLCoreImageStorage.h"
#include "XLCoreObject.h"
#include "XLVk.h"
#include "XLVkDevice.h"
#include "XLVkObject.h"
#include "XLVkPipeline.h"
#include "XLVkRenderPass.h"
#include "XLVkSync.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk {

DeviceQueue::~DeviceQueue() { }

bool DeviceQueue::init(Device &device, VkQueue queue, uint32_t index, core::QueueFlags flags) {
	if (!core::DeviceQueue::init(device, index, flags)) {
		return false;
	}
	_queue = queue;
	return true;
}

Status DeviceQueue::waitIdle() {
	VkResult result;
	static_cast<Device *>(_device)->makeApiCall(
			[&, this](const DeviceTable &table, VkDevice device) {
		result = table.vkQueueWaitIdle(_queue);
	});
	return getStatus(result);
}

Status DeviceQueue::doSubmit(const FrameSync *sync, core::CommandPool *commandPool,
		core::Fence &fence, SpanView<const core::CommandBuffer *> buffers,
		core::DeviceIdleFlags idle) {
	Vector<VkSemaphore> waitSem;
	Vector<VkPipelineStageFlags> waitStages;
	Vector<VkSemaphore> signalSem;
	Vector<VkCommandBuffer> vkBuffers;
	vkBuffers.reserve(buffers.size());

	for (auto &it : buffers) {
		if (it) {
			vkBuffers.emplace_back(static_cast<const CommandBuffer *>(it)->getBuffer());
		}
	}

	if (sync) {
		for (auto &it : sync->waitAttachments) {
			if (it.semaphore) {
				auto sem = it.semaphore.get_cast<Semaphore>()->getSemaphore();

				if (!it.semaphore->isWaited()) {
					waitSem.emplace_back(sem);
					waitStages.emplace_back(VkPipelineStageFlags(it.stages));
				}
				if (commandPool) {
					commandPool->autorelease(it.semaphore.get());
				}
			}
		}

		for (auto &it : sync->signalAttachments) {
			if (it.semaphore) {
				auto sem = it.semaphore.get_cast<Semaphore>()->getSemaphore();

				signalSem.emplace_back(sem);
				if (commandPool) {
					commandPool->autorelease(it.semaphore.get());
				}
			}
		}
	}

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pNext = nullptr;
	submitInfo.waitSemaphoreCount = uint32_t(waitSem.size());
	submitInfo.pWaitSemaphores = waitSem.data();
	submitInfo.pWaitDstStageMask = waitStages.data();
	submitInfo.commandBufferCount = uint32_t(vkBuffers.size());
	submitInfo.pCommandBuffers = vkBuffers.data();
	submitInfo.signalSemaphoreCount = uint32_t(signalSem.size());
	submitInfo.pSignalSemaphores = signalSem.data();

#if XL_VKAPI_DEBUG
	auto t = sp::platform::clock(ClockType::Monotonic);
	fence.addRelease([frameIdx = _frameIdx, t](bool success) {
		XL_VKAPI_LOG("[", frameIdx, "] vkQueueSubmit [complete]", " [",
				sp::platform::clock(ClockType::Monotonic) - t, "]");
	}, nullptr, "DeviceQueue::submit");
#endif

	VkResult result;
	auto dev = static_cast<Device *>(_device);

	dev->makeApiCall([&, this](const DeviceTable &table, VkDevice device) {
		if (hasFlag(idle, core::DeviceIdleFlags::PreDevice)) {
			table.vkDeviceWaitIdle(dev->getDevice());
		} else if (hasFlag(idle, core::DeviceIdleFlags::PreQueue)) {
			table.vkQueueWaitIdle(_queue);
		}
#if XL_VKAPI_DEBUG
		auto t = sp::platform::clock(ClockType::Monotonic);
		result =
				table.vkQueueSubmit(_queue, 1, &submitInfo, static_cast<Fence &>(fence).getFence());
		XL_VKAPI_LOG("[", _frameIdx, "] vkQueueSubmit: ", result, " ", (void *)_queue, " [",
				sp::platform::clock(ClockType::Monotonic) - t, "]");
#else
		result =
				table.vkQueueSubmit(_queue, 1, &submitInfo, static_cast<Fence &>(fence).getFence());
#endif

		if (hasFlag(idle, core::DeviceIdleFlags::PostDevice)) {
			table.vkDeviceWaitIdle(dev->getDevice());
		} else if (hasFlag(idle, core::DeviceIdleFlags::PostQueue)) {
			table.vkQueueWaitIdle(_queue);
		}
	});

	if (result == VK_SUCCESS) {
		// mark semaphores
		if (sync) {
			for (auto &it : sync->waitAttachments) {
				if (it.semaphore) {
					it.semaphore->setWaited(true);
					if (it.image && !it.image->isSemaphorePersistent()) {
						fence.addRelease(
								[img = it.image, sem = it.semaphore.get(),
										t = it.semaphore->getTimeline()](bool success) {
							sem->setInUse(false, t);
							img->releaseSemaphore(sem);
						},
								it.image, "DeviceQueue::submit::!isSemaphorePersistent");
					} else {
						fence.addRelease(
								[sem = it.semaphore.get(), t = it.semaphore->getTimeline()](
										bool success) { sem->setInUse(false, t); },
								it.semaphore, "DeviceQueue::submit::isSemaphorePersistent");
					}
					fence.autorelease(it.semaphore.get());
					if (commandPool) {
						commandPool->autorelease(it.semaphore.get());
					}
				}
			}

			for (auto &it : sync->signalAttachments) {
				if (it.semaphore) {
					it.semaphore->setSignaled(true);
					it.semaphore->setInUse(true, it.semaphore->getTimeline());
					fence.autorelease(it.semaphore.get());
					if (commandPool) {
						commandPool->autorelease(it.semaphore.get());
					}
				}
			}
		}

		fence.setArmed(*this);

		if (sync) {
			for (auto &it : sync->images) { it.image->setLayout(it.newLayout); }
		}
	}
	return getStatus(result);
}

ImageMemoryBarrier::ImageMemoryBarrier(Image *image, XAccessFlags src, XAccessFlags dst,
		XImageLayout old, XImageLayout _new)
: srcAccessMask(src)
, dstAccessMask(dst)
, oldLayout(old)
, newLayout(_new)
, vkimage(image->getImage())
, subresourceRange(VkImageSubresourceRange{VkImageAspectFlags(image->getAspects()), 0,
	  VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS})
, image(image) { }

ImageMemoryBarrier::ImageMemoryBarrier(Image *image, XAccessFlags src, XAccessFlags dst,
		XImageLayout old, XImageLayout _new, VkImageSubresourceRange range)
: srcAccessMask(src)
, dstAccessMask(dst)
, oldLayout(old)
, newLayout(_new)
, vkimage(image->getImage())
, subresourceRange(range)
, image(image) { }

ImageMemoryBarrier::ImageMemoryBarrier(Image *image, XAccessFlags src, XAccessFlags dst,
		XImageLayout old, XImageLayout _new, QueueFamilyTransfer transfer)
: srcAccessMask(src)
, dstAccessMask(dst)
, oldLayout(old)
, newLayout(_new)
, familyTransfer(transfer)
, vkimage(image->getImage())
, subresourceRange(VkImageSubresourceRange{VkImageAspectFlags(image->getAspects()), 0,
	  VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS})
, image(image) { }

ImageMemoryBarrier::ImageMemoryBarrier(Image *image, XAccessFlags src, XAccessFlags dst,
		XImageLayout old, XImageLayout _new, QueueFamilyTransfer transfer,
		VkImageSubresourceRange range)
: srcAccessMask(src)
, dstAccessMask(dst)
, oldLayout(old)
, newLayout(_new)
, familyTransfer(transfer)
, vkimage(image->getImage())
, subresourceRange(range)
, image(image) { }

ImageMemoryBarrier::ImageMemoryBarrier(VkImage image, XAccessFlags src, XAccessFlags dst,
		XImageLayout old, XImageLayout _new, VkImageAspectFlags aspect)
: srcAccessMask(src)
, dstAccessMask(dst)
, oldLayout(old)
, newLayout(_new)
, vkimage(image)
, subresourceRange(VkImageSubresourceRange{aspect, 0, VK_REMAINING_MIP_LEVELS, 0,
	  VK_REMAINING_ARRAY_LAYERS}) { }

ImageMemoryBarrier::ImageMemoryBarrier(VkImage image, XAccessFlags src, XAccessFlags dst,
		XImageLayout old, XImageLayout _new, VkImageSubresourceRange range)
: srcAccessMask(src)
, dstAccessMask(dst)
, oldLayout(old)
, newLayout(_new)
, vkimage(image)
, subresourceRange(range) { }

ImageMemoryBarrier::ImageMemoryBarrier(VkImage image, XAccessFlags src, XAccessFlags dst,
		XImageLayout old, XImageLayout _new, QueueFamilyTransfer transfer,
		VkImageAspectFlags aspect)
: srcAccessMask(src)
, dstAccessMask(dst)
, oldLayout(old)
, newLayout(_new)
, familyTransfer(transfer)
, vkimage(image)
, subresourceRange(VkImageSubresourceRange{aspect, 0, VK_REMAINING_MIP_LEVELS, 0,
	  VK_REMAINING_ARRAY_LAYERS}) { }

ImageMemoryBarrier::ImageMemoryBarrier(VkImage image, XAccessFlags src, XAccessFlags dst,
		XImageLayout old, XImageLayout _new, QueueFamilyTransfer transfer,
		VkImageSubresourceRange range)
: srcAccessMask(src)
, dstAccessMask(dst)
, oldLayout(old)
, newLayout(_new)
, familyTransfer(transfer)
, vkimage(image)
, subresourceRange(range) { }

ImageMemoryBarrier::ImageMemoryBarrier(const VkImageMemoryBarrier &barrier)
: srcAccessMask(barrier.srcAccessMask)
, dstAccessMask(barrier.dstAccessMask)
, oldLayout(barrier.oldLayout)
, newLayout(barrier.newLayout)
, familyTransfer(QueueFamilyTransfer{barrier.srcQueueFamilyIndex, barrier.dstQueueFamilyIndex})
, vkimage(barrier.image)
, subresourceRange(barrier.subresourceRange)
, image(nullptr) { }

BufferMemoryBarrier::BufferMemoryBarrier(Buffer *buf, XAccessFlags src, XAccessFlags dst)
: srcAccessMask(src), dstAccessMask(dst), vkbuffer(buf->getBuffer()), buffer(buf) { }

BufferMemoryBarrier::BufferMemoryBarrier(Buffer *buf, XAccessFlags src, XAccessFlags dst,
		VkDeviceSize offset, VkDeviceSize size)
: srcAccessMask(src)
, dstAccessMask(dst)
, vkbuffer(buf->getBuffer())
, offset(offset)
, size(size)
, buffer(buf) { }

BufferMemoryBarrier::BufferMemoryBarrier(Buffer *buf, XAccessFlags src, XAccessFlags dst,
		QueueFamilyTransfer transfer)
: srcAccessMask(src)
, dstAccessMask(dst)
, familyTransfer(transfer)
, vkbuffer(buf->getBuffer())
, buffer(buf) { }

BufferMemoryBarrier::BufferMemoryBarrier(Buffer *buf, XAccessFlags src, XAccessFlags dst,
		QueueFamilyTransfer transfer, VkDeviceSize offset, VkDeviceSize size)
: srcAccessMask(src)
, dstAccessMask(dst)
, familyTransfer(transfer)
, vkbuffer(buf->getBuffer())
, offset(offset)
, size(size)
, buffer(buf) { }

BufferMemoryBarrier::BufferMemoryBarrier(VkBuffer buf, XAccessFlags src, XAccessFlags dst)
: srcAccessMask(src), dstAccessMask(dst), vkbuffer(buf) { }

BufferMemoryBarrier::BufferMemoryBarrier(VkBuffer buf, XAccessFlags src, XAccessFlags dst,
		VkDeviceSize offset, VkDeviceSize size)
: srcAccessMask(src), dstAccessMask(dst), vkbuffer(buf), offset(offset), size(size) { }

BufferMemoryBarrier::BufferMemoryBarrier(VkBuffer buf, XAccessFlags src, XAccessFlags dst,
		QueueFamilyTransfer transfer)
: srcAccessMask(src), dstAccessMask(dst), familyTransfer(transfer), vkbuffer(buf) { }

BufferMemoryBarrier::BufferMemoryBarrier(VkBuffer buf, XAccessFlags src, XAccessFlags dst,
		QueueFamilyTransfer transfer, VkDeviceSize offset, VkDeviceSize size)
: srcAccessMask(src)
, dstAccessMask(dst)
, familyTransfer(transfer)
, vkbuffer(buf)
, offset(offset)
, size(size) { }

BufferMemoryBarrier::BufferMemoryBarrier(const VkBufferMemoryBarrier &barrier)
: srcAccessMask(barrier.srcAccessMask)
, dstAccessMask(barrier.dstAccessMask)
, familyTransfer(QueueFamilyTransfer{barrier.srcQueueFamilyIndex, barrier.dstQueueFamilyIndex})
, vkbuffer(barrier.buffer)
, offset(barrier.offset)
, size(barrier.size)
, buffer(nullptr) { }

CommandBuffer::~CommandBuffer() { invalidate(); }

bool CommandBuffer::init(const CommandPool *pool, const DeviceTable *table, VkCommandBuffer buffer,
		Vector<Rc<DescriptorPool>> &&descriptors, CommandBufferInfo &&info) {
	_info = move(info);
	_pool = pool;
	_table = table;
	_buffer = buffer;
	_availableDescriptors = sp::move(descriptors);
	return true;
}

void CommandBuffer::invalidate() {
	_buffer = VK_NULL_HANDLE;
	_availableDescriptors.clear();
	_usedDescriptors.clear();
}

void CommandBuffer::cmdPipelineBarrier(XPipelineStage srcFlags, XPipelineStage dstFlags,
		VkDependencyFlags deps, SpanView<ImageMemoryBarrier> imageBarriers) {
	Vector<VkImageMemoryBarrier> images;
	images.reserve(imageBarriers.size());
	for (auto &it : imageBarriers) {
		images.emplace_back(VkImageMemoryBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr,
			it.srcAccessMask, it.dstAccessMask, it.oldLayout, it.newLayout,
			it.familyTransfer.srcQueueFamilyIndex, it.familyTransfer.dstQueueFamilyIndex,
			it.vkimage, it.subresourceRange});
		bindImage(it.image);
	}

	_table->vkCmdPipelineBarrier(_buffer, srcFlags, dstFlags, deps, 0, nullptr, 0, nullptr,
			uint32_t(images.size()), images.data());
}

void CommandBuffer::cmdPipelineBarrier(XPipelineStage srcFlags, XPipelineStage dstFlags,
		VkDependencyFlags deps, SpanView<BufferMemoryBarrier> bufferBarriers) {
	Vector<VkBufferMemoryBarrier> buffers;
	buffers.reserve(bufferBarriers.size());
	for (auto &it : bufferBarriers) {
		buffers.emplace_back(VkBufferMemoryBarrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, nullptr,
			it.srcAccessMask, it.dstAccessMask, it.familyTransfer.srcQueueFamilyIndex,
			it.familyTransfer.dstQueueFamilyIndex, it.vkbuffer, it.offset, it.size});
		bindBuffer(it.buffer);
	}

	_table->vkCmdPipelineBarrier(_buffer, srcFlags, dstFlags, deps, 0, nullptr,
			uint32_t(buffers.size()), buffers.data(), 0, nullptr);
}

void CommandBuffer::cmdGlobalBarrier(XPipelineStage srcFlags, XPipelineStage dstFlags,
		VkDependencyFlags deps, XAccessFlags src, XAccessFlags dst) {
	VkMemoryBarrier barrier;
	barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	barrier.pNext = nullptr;
	barrier.srcAccessMask = src;
	barrier.dstAccessMask = dst;

	_table->vkCmdPipelineBarrier(_buffer, srcFlags, dstFlags, deps, 1, &barrier, 0, nullptr, 0,
			nullptr);
}

void CommandBuffer::cmdPipelineBarrier(XPipelineStage srcFlags, XPipelineStage dstFlags,
		VkDependencyFlags deps, SpanView<BufferMemoryBarrier> bufferBarriers,
		SpanView<ImageMemoryBarrier> imageBarriers) {
	Vector<VkBufferMemoryBarrier> buffers;
	buffers.reserve(bufferBarriers.size());
	for (auto &it : bufferBarriers) {
		buffers.emplace_back(VkBufferMemoryBarrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, nullptr,
			it.srcAccessMask, it.dstAccessMask, it.familyTransfer.srcQueueFamilyIndex,
			it.familyTransfer.dstQueueFamilyIndex, it.vkbuffer, it.offset, it.size});
		bindBuffer(it.buffer);
	}

	Vector<VkImageMemoryBarrier> images;
	images.reserve(imageBarriers.size());
	for (auto &it : imageBarriers) {
		images.emplace_back(VkImageMemoryBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr,
			it.srcAccessMask, it.dstAccessMask, it.oldLayout, it.newLayout,
			it.familyTransfer.srcQueueFamilyIndex, it.familyTransfer.dstQueueFamilyIndex,
			it.vkimage, it.subresourceRange});
		bindImage(it.image);
	}

	_table->vkCmdPipelineBarrier(_buffer, srcFlags, dstFlags, deps, 0, nullptr,
			uint32_t(buffers.size()), buffers.data(), uint32_t(images.size()), images.data());
}

void CommandBuffer::cmdCopyBuffer(Buffer *src, Buffer *dst) {
	VkBufferCopy copy{0, 0, std::min(src->getSize(), dst->getSize())};
	cmdCopyBuffer(src, dst, makeSpanView(&copy, 1));
}

void CommandBuffer::cmdCopyBuffer(Buffer *src, Buffer *dst, VkDeviceSize srcOffset,
		VkDeviceSize dstOffset, VkDeviceSize size) {
	VkBufferCopy copy{srcOffset, dstOffset, size};
	cmdCopyBuffer(src, dst, makeSpanView(&copy, 1));
}

void CommandBuffer::cmdCopyBuffer(Buffer *src, Buffer *dst, SpanView<VkBufferCopy> copy) {
	bindBuffer(src);
	bindBuffer(dst);

	cmdCopyBuffer(src->getBuffer(), dst->getBuffer(), copy);
}

void CommandBuffer::cmdCopyBuffer(VkBuffer src, VkBuffer dst, SpanView<VkBufferCopy> copy) {
	_table->vkCmdCopyBuffer(_buffer, src, dst, uint32_t(copy.size()), copy.data());
}

void CommandBuffer::cmdCopyImage(Image *src, XImageLayout srcLayout, Image *dst,
		XImageLayout dstLayout, VkFilter filter) {
	auto sourceExtent = src->getInfo().extent;
	auto targetExtent = dst->getInfo().extent;

	if (sourceExtent == targetExtent) {
		VkImageCopy copy{
			VkImageSubresourceLayers{VkImageAspectFlags(src->getAspects()), 0, 0,
				src->getInfo().arrayLayers.get()},
			VkOffset3D{0, 0, 0},
			VkImageSubresourceLayers{VkImageAspectFlags(dst->getAspects()), 0, 0,
				dst->getInfo().arrayLayers.get()},
			VkOffset3D{0, 0, 0},
			VkExtent3D{targetExtent.width, targetExtent.height, targetExtent.depth},
		};

		_table->vkCmdCopyImage(_buffer, src->getImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				dst->getImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
	} else {
		VkImageBlit blit{
			VkImageSubresourceLayers{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0,
				src->getInfo().arrayLayers.get()},
			{VkOffset3D{0, 0, 0},
				VkOffset3D{int32_t(sourceExtent.width), int32_t(sourceExtent.height),
					int32_t(sourceExtent.depth)}},
			VkImageSubresourceLayers{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0,
				dst->getInfo().arrayLayers.get()},
			{VkOffset3D{0, 0, 0},
				VkOffset3D{int32_t(targetExtent.width), int32_t(targetExtent.height),
					int32_t(targetExtent.depth)}},
		};

		_table->vkCmdBlitImage(_buffer, src->getImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				dst->getImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, filter);
	}
}

void CommandBuffer::cmdCopyImage(Image *src, XImageLayout srcLayout, Image *dst,
		XImageLayout dstLayout, const VkImageCopy &copy) {
	bindImage(src);
	bindImage(dst);

	_table->vkCmdCopyImage(_buffer, src->getImage(), srcLayout, dst->getImage(), dstLayout, 1,
			&copy);
}

void CommandBuffer::cmdCopyImage(Image *src, XImageLayout srcLayout, Image *dst,
		XImageLayout dstLayout, SpanView<VkImageCopy> copy) {
	bindImage(src);
	bindImage(dst);

	_table->vkCmdCopyImage(_buffer, src->getImage(), srcLayout, dst->getImage(), dstLayout,
			uint32_t(copy.size()), copy.data());
}

void CommandBuffer::cmdCopyBufferToImage(Buffer *buf, Image *img, XImageLayout layout,
		VkDeviceSize offset) {
	auto &extent = img->getInfo().extent;
	VkImageSubresourceLayers copyLayers(
			{VkImageAspectFlags(img->getAspects()), 0, 0, img->getInfo().arrayLayers.get()});

	VkBufferImageCopy copyRegion{offset, 0, 0, copyLayers, VkOffset3D{0, 0, 0},
		VkExtent3D{extent.width, extent.height, extent.depth}};

	cmdCopyBufferToImage(buf, img, layout, makeSpanView(&copyRegion, 1));
}

void CommandBuffer::cmdCopyBufferToImage(Buffer *buf, Image *img, XImageLayout layout,
		SpanView<VkBufferImageCopy> copy) {
	bindBuffer(buf);
	bindImage(img);

	cmdCopyBufferToImage(buf->getBuffer(), img->getImage(), layout, copy);
}

void CommandBuffer::cmdCopyBufferToImage(VkBuffer buf, VkImage img, XImageLayout layout,
		SpanView<VkBufferImageCopy> copy) {
	_table->vkCmdCopyBufferToImage(_buffer, buf, img, layout, uint32_t(copy.size()), copy.data());
}

void CommandBuffer::cmdCopyImageToBuffer(Image *img, XImageLayout layout, Buffer *buf,
		VkDeviceSize offset) {
	auto &extent = img->getInfo().extent;
	VkImageSubresourceLayers copyLayers(
			{VkImageAspectFlags(img->getAspects()), 0, 0, img->getInfo().arrayLayers.get()});

	VkBufferImageCopy copyRegion{offset, 0, 0, copyLayers, VkOffset3D{0, 0, 0},
		VkExtent3D{extent.width, extent.height, extent.depth}};

	cmdCopyImageToBuffer(img, layout, buf, makeSpanView(&copyRegion, 1));
}

void CommandBuffer::cmdCopyImageToBuffer(Image *img, XImageLayout layout, Buffer *buf,
		SpanView<VkBufferImageCopy> copy) {
	bindBuffer(buf);
	bindImage(img);

	_table->vkCmdCopyImageToBuffer(_buffer, img->getImage(), layout, buf->getBuffer(),
			uint32_t(copy.size()), copy.data());
}

void CommandBuffer::cmdClearColorImage(Image *image, XImageLayout layout, const Color4F &color) {
	VkClearColorValue clearColorEmpty;
	clearColorEmpty.float32[0] = color.r;
	clearColorEmpty.float32[1] = color.g;
	clearColorEmpty.float32[2] = color.b;
	clearColorEmpty.float32[3] = color.a;

	VkImageSubresourceRange range{VkImageAspectFlags(image->getAspects()), 0,
		image->getInfo().mipLevels.get(), 0, image->getInfo().arrayLayers.get()};

	bindImage(image);
	_table->vkCmdClearColorImage(_buffer, image->getImage(), layout, &clearColorEmpty, 1, &range);
}

void CommandBuffer::cmdBeginRenderPass(RenderPass *pass, Framebuffer *fb, VkSubpassContents subpass,
		bool alt) {
	auto &clearValues = pass->getClearValues();
	auto currentExtent = fb->getExtent();

	VkRenderPassBeginInfo renderPassInfo{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, nullptr};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.pNext = nullptr;
	renderPassInfo.renderPass = pass->getRenderPass(alt);
	renderPassInfo.framebuffer = fb->getFramebuffer();
	renderPassInfo.renderArea.offset = {0, 0};
	renderPassInfo.renderArea.extent = VkExtent2D{currentExtent.width, currentExtent.height};
	renderPassInfo.clearValueCount = uint32_t(clearValues.size());
	renderPassInfo.pClearValues = clearValues.data();

	bindFramebuffer(fb);
	_table->vkCmdBeginRenderPass(_buffer, &renderPassInfo, subpass);

	_currentSubpass = 0;
	_withinRenderpass = true;
}

void CommandBuffer::cmdEndRenderPass() {
	_table->vkCmdEndRenderPass(_buffer);

	_withinRenderpass = false;
	_currentSubpass = 0;
}

void CommandBuffer::cmdSetViewport(uint32_t firstViewport, SpanView<VkViewport> viewports) {
	_table->vkCmdSetViewport(_buffer, firstViewport, uint32_t(viewports.size()), viewports.data());
}

void CommandBuffer::cmdSetScissor(uint32_t firstScissor, SpanView<VkRect2D> scissors) {
	// log::source().verbose("CommandBuffer", "cmdSetScissor: ", scissors.front().offset.x,
	// " ", scissors.front().offset.y, " ", 		scissors.front().extent.width, " ",
	//scissors.front().extent.height);
	_table->vkCmdSetScissor(_buffer, firstScissor, uint32_t(scissors.size()), scissors.data());
}

void CommandBuffer::cmdBindPipeline(GraphicPipeline *pipeline) {
	if (pipeline != _boundGraphicPipeline) {
		_table->vkCmdBindPipeline(_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
				pipeline->getPipeline());
		_boundGraphicPipeline = pipeline;
	}
}

void CommandBuffer::cmdBindPipeline(ComputePipeline *pipeline) {
	if (pipeline != _boundComputePipeline) {
		_table->vkCmdBindPipeline(_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->getPipeline());
		_boundComputePipeline = pipeline;
	}
}

void CommandBuffer::cmdBindPipelineWithDescriptors(const core::GraphicPipelineData *data,
		uint32_t firstSet) {
	auto texPool = _availableDescriptors[data->layout->index];
	auto renderPass = static_cast<RenderPass *>(data->subpass->pass->impl.get());
	if (texPool) {
		cmdBindDescriptorSets(renderPass, texPool, firstSet);
	} else if (firstSet == 0) {
		auto pt = getBindPoint(VK_PIPELINE_BIND_POINT_GRAPHICS);
		pt->boundSets.clear();
		pt->boundLayout = renderPass->getPipelineLayout(data->layout->index);
		pt->boundLayoutIndex = data->layout->index;
	} else {
		log::source().error("CommandBuffer", "Fail to bind set with index ", firstSet,
				": no sets available");
	}
	cmdBindPipeline(static_cast<GraphicPipeline *>(data->pipeline.get()));
}

void CommandBuffer::cmdBindPipelineWithDescriptors(const core::ComputePipelineData *data,
		uint32_t firstSet) {
	auto texPool = _availableDescriptors[data->layout->index];
	auto renderPass = static_cast<RenderPass *>(data->subpass->pass->impl.get());
	if (texPool) {
		cmdBindDescriptorSets(renderPass, texPool, firstSet);
	} else if (firstSet == 0) {
		auto pt = getBindPoint(VK_PIPELINE_BIND_POINT_COMPUTE);
		pt->boundSets.clear();
		pt->boundLayout = renderPass->getPipelineLayout(data->layout->index);
		pt->boundLayoutIndex = data->layout->index;
	} else {
		log::source().error("CommandBuffer", "Fail to bind set with index ", firstSet,
				": no sets available");
	}
	cmdBindPipeline(static_cast<ComputePipeline *>(data->pipeline.get()));
}

void CommandBuffer::cmdBindIndexBuffer(Buffer *buf, VkDeviceSize offset, VkIndexType indexType) {
	_table->vkCmdBindIndexBuffer(_buffer, buf->getBuffer(), offset, indexType);
}

void CommandBuffer::cmdBindDescriptorSets(RenderPass *pass, uint32_t index, uint32_t firstSet) {
	cmdBindDescriptorSets(pass, _availableDescriptors[index], firstSet);
}

void CommandBuffer::cmdBindDescriptorSets(RenderPass *pass, const Rc<DescriptorPool> &pool,
		uint32_t firstSet) {
	if (!pool) {
		return;
	}

	auto passType = pass->getType();
	auto pt = getBindPoint(passType);

	if (!pt) {
		log::source().error("vk::CommandBuffer", "Invalid bind point");
		return;
	}

	auto sets = pool->getSets();

	auto targetLayout = pool->getLayout();

	Vector<VkDescriptorSet> bindSets;
	bindSets.reserve(sets.size());
	for (auto &it : sets) { bindSets.emplace_back(it->set); }

	auto doBindSets = [&] {
		for (auto &it : sets) { _descriptorSets.emplace(it); }

		pt->boundLayout = targetLayout;

		_table->vkCmdBindDescriptorSets(_buffer, pt->point, pt->boundLayout->getLayout(), firstSet,
				uint32_t(bindSets.size()), bindSets.data(), 0, nullptr);

		auto it = std::find(_availableDescriptors.begin(), _availableDescriptors.end(), pool);
		if (it == _availableDescriptors.end()) {
			_usedDescriptors.emplace(pool);
		}
	};

	if (targetLayout != pt->boundLayout) {
		updateBoundSets(*pt, bindSets, firstSet);
		doBindSets();
	} else if (!updateBoundSets(*pt, bindSets, firstSet)) {
		doBindSets();
	}
}

void CommandBuffer::cmdBindDescriptorSets(RenderPass *pass, SpanView<VkDescriptorSet> sets,
		uint32_t firstSet) {
	auto pt = getBindPoint(pass->getType());

	if (!pt || !pt->boundLayout) {
		log::source().error("vk::CommandBuffer", "Try to rebind sets when no layout is bound");
	}

	if (!updateBoundSets(*pt, sets, firstSet)) {
		_table->vkCmdBindDescriptorSets(_buffer, pt->point, pt->boundLayout->getLayout(), firstSet,
				uint32_t(sets.size()), sets.data(), 0, nullptr);
	}
}

void CommandBuffer::cmdBindGraphicDescriptorSets(VkPipelineLayout layout,
		SpanView<VkDescriptorSet> sets, uint32_t firstSet) {
	_table->vkCmdBindDescriptorSets(_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, firstSet,
			uint32_t(sets.size()), sets.data(), 0, nullptr);
}

void CommandBuffer::cmdBindComputeDescriptorSets(VkPipelineLayout layout,
		SpanView<VkDescriptorSet> sets, uint32_t firstSet) {
	_table->vkCmdBindDescriptorSets(_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, layout, firstSet,
			uint32_t(sets.size()), sets.data(), 0, nullptr);
}

void CommandBuffer::cmdDraw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex,
		uint32_t firstInstance) {
	_table->vkCmdDraw(_buffer, vertexCount, instanceCount, firstVertex, firstInstance);
}
void CommandBuffer::cmdDrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex,
		int32_t vertexOffset, uint32_t firstInstance) {
	_table->vkCmdDrawIndexed(_buffer, indexCount, instanceCount, firstIndex, vertexOffset,
			firstInstance);
}

void CommandBuffer::cmdDrawIndirect(Buffer *buf, uint64_t offset, uint32_t count, uint32_t stride) {
	bindBuffer(buf);
	_table->vkCmdDrawIndirect(_buffer, buf->getBuffer(), offset, count, stride);
}

void CommandBuffer::cmdPushConstants(PipelineLayout *layout, XPipelineStage stageFlags,
		uint32_t offset, BytesView data) {
	XLASSERT(layout, "cmdPushConstants without bound layout");
	_table->vkCmdPushConstants(_buffer, layout->getLayout(), stageFlags, offset,
			uint32_t(data.size()), data.data());
}

void CommandBuffer::cmdPushConstants(XPipelineStage stageFlags, uint32_t offset, BytesView data) {
	BindPoint *point = nullptr;

	if (hasFlag(VkShaderStageFlagBits(stageFlags.value), VK_SHADER_STAGE_COMPUTE_BIT)) {
		point = getBindPoint(VK_PIPELINE_BIND_POINT_COMPUTE);
	} else if (hasFlag(uint32_t(stageFlags.value),
					   uint32_t(VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR
							   | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR
							   | VK_SHADER_STAGE_INTERSECTION_BIT_KHR
							   | VK_SHADER_STAGE_CALLABLE_BIT_KHR))) {
		point = getBindPoint(VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR);
	} else {
		point = getBindPoint(VK_PIPELINE_BIND_POINT_GRAPHICS);
	}

	if (point) {
		cmdPushConstants(point->boundLayout, stageFlags, offset, data);
	} else {
		log::source().error("CommandBuffer",
				"No bound point available for stageFlags: ", stageFlags.value);
	}
}

void CommandBuffer::cmdFillBuffer(Buffer *buffer, uint32_t data) {
	cmdFillBuffer(buffer, 0, VK_WHOLE_SIZE, data);
}

void CommandBuffer::cmdFillBuffer(Buffer *buffer, VkDeviceSize dstOffset, VkDeviceSize size,
		uint32_t data) {
	bindBuffer(buffer);
	_table->vkCmdFillBuffer(_buffer, buffer->getBuffer(), dstOffset, size, data);
}

void CommandBuffer::cmdDispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) {
	_table->vkCmdDispatch(_buffer, groupCountX, groupCountY, groupCountZ);
}

void CommandBuffer::cmdDispatchPipeline(const core::ComputePipelineData *pipeline, uint32_t countX,
		uint32_t countY, uint32_t countZ) {
	cmdBindPipelineWithDescriptors(pipeline);
	cmdDispatch((countX - 1) / pipeline->pipeline->getLocalX() + 1,
			(countY - 1) / pipeline->pipeline->getLocalY() + 1,
			(countZ - 1) / pipeline->pipeline->getLocalZ() + 1);
}

uint32_t CommandBuffer::cmdWriteTimestamp(XPipelineStage stage, uint32_t tag) {
	if (!_timestampQueryPool) {
		auto dev = static_cast<Device *>(_pool->getObjectData().device);

		Rc<QueryPool> pool;
		pool = dev->acquireQueryPool(_pool->getFamilyIdx(),
						  core::QueryPoolInfo{
							  .type = core::QueryType::Timestamp,
							  .queryCount = _info.timestampQueries,
						  })
					   .get_cast<QueryPool>();
		if (pool) {
			_queryPools.emplace_back(pool);
			_timestampQueryPool = move(pool);

			_table->vkCmdResetQueryPool(_buffer, _timestampQueryPool->getPool(), 0,
					_timestampQueryPool->getInfo().queryCount);
		}
	}

	if (_timestampQueryPool) {
		auto nextTimestamp = _timestampQueryPool->armNextQuery(tag);
		if (nextTimestamp != maxOf<uint32_t>()) {
			_table->vkCmdWriteTimestamp(_buffer, VkPipelineStageFlagBits(stage.value),
					_timestampQueryPool->getPool(), nextTimestamp);
		}
		return nextTimestamp;
	}
	return maxOf<uint32_t>();
}

uint32_t CommandBuffer::cmdNextSubpass() {
	if (_withinRenderpass) {
		_table->vkCmdNextSubpass(_buffer, VK_SUBPASS_CONTENTS_INLINE);
		++_currentSubpass;
		return _currentSubpass;
	}
	return 0;
}

uint32_t CommandBuffer::getBoundLayoutIndex(VkPipelineBindPoint pt) const {
	auto p = getBindPoint(pt);
	if (p) {
		return p->boundLayoutIndex;
	}
	return maxOf<uint32_t>();
}

PipelineLayout *CommandBuffer::getBoundLayout(VkPipelineBindPoint pt) const {
	auto p = getBindPoint(pt);
	if (p) {
		return p->boundLayout;
	}
	return nullptr;
}

uint32_t CommandBuffer::getBoundLayoutIndex(core::PassType t) const {
	auto p = getBindPoint(t);
	if (p) {
		return p->boundLayoutIndex;
	}
	return maxOf<uint32_t>();
}

PipelineLayout *CommandBuffer::getBoundLayout(core::PassType t) const {
	auto p = getBindPoint(t);
	if (p) {
		return p->boundLayout;
	}
	return nullptr;
}

void CommandBuffer::writeImageTransfer(uint32_t sourceFamily, uint32_t targetFamily,
		const Rc<Buffer> &buffer, const Rc<Image> &image) {
	ImageMemoryBarrier inImageBarrier(image, 0, VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	cmdPipelineBarrier(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
			makeSpanView(&inImageBarrier, 1));

	auto sourceFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	auto targetFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

	if (image->getInfo().type != core::PassType::Generic && sourceFamily != VK_QUEUE_FAMILY_IGNORED
			&& targetFamily != VK_QUEUE_FAMILY_IGNORED) {
		if (sourceFamily != targetFamily) {
			sourceFamilyIndex = sourceFamily;
			targetFamilyIndex = targetFamily;
		}
	}

	cmdCopyBufferToImage(buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0);

	ImageMemoryBarrier outImageBarrier(image, VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			QueueFamilyTransfer{sourceFamilyIndex, targetFamilyIndex});

	cmdPipelineBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0,
			makeSpanView(&outImageBarrier, 1));

	if (targetFamilyIndex != VK_QUEUE_FAMILY_IGNORED) {
		image->setPendingBarrier(outImageBarrier);
	}
}

uint64_t CommandBuffer::bindBufferAddress(NotNull<core::BufferObject> buffer) {
	auto dev = buffer->getDeviceAddress();
	if (!dev) {
		log::source().error("CommandBuffer", "BufferDeviceAddress is not available for the buffer");
		return 0;
	} else {
		bindBuffer(buffer);
		return dev;
	}
}

void CommandBuffer::bindBuffer(core::BufferObject *buffer) {
	core::CommandBuffer::bindBuffer(buffer);
	if (buffer) {
		if (auto pool = static_cast<Buffer *>(buffer)->getMemory()->getPool()) {
			_memPool.emplace(pool);
		}
	}
}

bool CommandBuffer::isNextTimestampAvailable() const {
	return _timestampQueryPool->getUsedQueries() < _info.timestampQueries;
}

bool CommandBuffer::updateBoundSets(BindPoint &point, SpanView<VkDescriptorSet> sets,
		uint32_t firstSet) {
	auto size = sets.size() + firstSet;
	if (size <= point.boundSets.size()) {
		if (memcmp(point.boundSets.data() + firstSet, sets.data(),
					sizeof(VkDescriptorSet) * sets.size())
				== 0) {
			return true;
		}
	}

	point.boundSets.resize(size);
	memcpy(point.boundSets.data() + firstSet, sets.data(), sizeof(VkDescriptorSet) * sets.size());
	return false;
}

CommandBuffer::BindPoint *CommandBuffer::getBindPoint(VkPipelineBindPoint pt) {
	switch (pt) {
	case VK_PIPELINE_BIND_POINT_GRAPHICS:
		_bindPoints[0].point = VK_PIPELINE_BIND_POINT_GRAPHICS;
		return &_bindPoints[0];
		break;
	case VK_PIPELINE_BIND_POINT_COMPUTE:
		_bindPoints[1].point = VK_PIPELINE_BIND_POINT_COMPUTE;
		return &_bindPoints[1];
		break;
	case VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR:
	case VK_PIPELINE_BIND_POINT_SUBPASS_SHADING_HUAWEI:
	default: break;
	}
	return nullptr;
}

const CommandBuffer::BindPoint *CommandBuffer::getBindPoint(VkPipelineBindPoint pt) const {
	switch (pt) {
	case VK_PIPELINE_BIND_POINT_GRAPHICS: return &_bindPoints[0]; break;
	case VK_PIPELINE_BIND_POINT_COMPUTE: return &_bindPoints[1]; break;
	case VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR:
	case VK_PIPELINE_BIND_POINT_SUBPASS_SHADING_HUAWEI:
	default: break;
	}
	return nullptr;
}


CommandBuffer::BindPoint *CommandBuffer::getBindPoint(core::PassType t) {
	return getBindPoint((t == core::PassType::Compute) ? VK_PIPELINE_BIND_POINT_COMPUTE
													   : VK_PIPELINE_BIND_POINT_GRAPHICS);
}

const CommandBuffer::BindPoint *CommandBuffer::getBindPoint(core::PassType t) const {
	return getBindPoint((t == core::PassType::Compute) ? VK_PIPELINE_BIND_POINT_COMPUTE
													   : VK_PIPELINE_BIND_POINT_GRAPHICS);
}

static void CommandPool_destroy(core::Device *dev, core::ObjectType, core::ObjectHandle ptr,
		void *) {
	auto d = static_cast<Device *>(dev);
	auto target = (VkCommandPool)ptr.get();
	if (target) {
		d->getTable()->vkDestroyCommandPool(d->getDevice(), target, nullptr);
		target = VK_NULL_HANDLE;
	}
}

bool CommandPool::init(Device &dev, uint32_t familyIdx, core::QueueFlags c, bool transient) {
	_familyIdx = familyIdx;
	_class = c;

	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.pNext = nullptr;
	poolInfo.queueFamilyIndex = familyIdx;
	poolInfo.flags = 0; // transient ? VK_COMMAND_POOL_CREATE_TRANSIENT_BIT : 0;

	if (dev.getTable()->vkCreateCommandPool(dev.getDevice(), &poolInfo, nullptr, &_commandPool)
			== VK_SUCCESS) {
		return core::CommandPool::init(dev, CommandPool_destroy, core::ObjectType::CommandPool,
				core::ObjectHandle(_commandPool));
	}

	return false;
}

const core::CommandBuffer *CommandPool::recordBuffer(core::Device &dev,
		const Callback<bool(core::CommandBuffer &)> &cb) {
	return recordBuffer(static_cast<Device &>(dev), Vector<Rc<DescriptorPool>>(),
			[&](CommandBuffer &buf) { return cb(buf); });
}

const CommandBuffer *CommandPool::recordBuffer(Device &dev,
		Vector<Rc<DescriptorPool>> &&descriptors, const Callback<bool(CommandBuffer &)> &cb,
		CommandBufferInfo &&info) {
	if (!_commandPool) {
		return nullptr;
	}

	if (info.timestampQueries > 0) {
		auto &limits = dev.getInfo().properties.device10.properties.limits;
		if ((!hasFlag(_class, core::QueueFlags::Graphics)
					&& !hasFlag(_class, core::QueueFlags::Compute))
				|| limits.timestampPeriod == 0
				|| dev.getQueueFamily(_familyIdx)->timestampValidBits == 0) {
			log::source().error("CommandPool", "Timestamps for this queue is not available");
			return nullptr;
		}
	}

	if (_invalidated) {
		recreatePool(dev);
	}

	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = _commandPool;
	allocInfo.level = VkCommandBufferLevel(info.level);
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer buf;
	if (dev.getTable()->vkAllocateCommandBuffers(dev.getDevice(), &allocInfo, &buf) != VK_SUCCESS) {
		return nullptr;
	}

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.pNext = nullptr;
	beginInfo.flags = info.usageFlags;
	beginInfo.pInheritanceInfo = nullptr;

	if (dev.getTable()->vkBeginCommandBuffer(buf, &beginInfo) != VK_SUCCESS) {
		dev.getTable()->vkFreeCommandBuffers(dev.getDevice(), _commandPool, 1, &buf);
		return nullptr;
	}

	auto b = Rc<CommandBuffer>::create(this, dev.getTable(), buf, sp::move(descriptors),
			sp::move(info));
	if (!b) {
		dev.getTable()->vkEndCommandBuffer(buf);
		dev.getTable()->vkFreeCommandBuffers(dev.getDevice(), _commandPool, 1, &buf);
		return nullptr;
	}

	auto result = cb(*b);

	dev.getTable()->vkEndCommandBuffer(buf);

	if (!result) {
		dev.getTable()->vkFreeCommandBuffers(dev.getDevice(), _commandPool, 1, &buf);
		return nullptr;
	}

	return static_cast<const CommandBuffer *>(_buffers.emplace_back(move(b)).get());
}

void CommandPool::freeDefaultBuffers(Device &dev, Vector<VkCommandBuffer> &vec) {
	if (_commandPool) {
		dev.getTable()->vkFreeCommandBuffers(dev.getDevice(), _commandPool,
				static_cast<uint32_t>(vec.size()), vec.data());
	}
	vec.clear();
}

void CommandPool::reset(core::Device &cdev) {
	auto &dev = static_cast<Device &>(cdev);

	if (_commandPool) {
		// dev.getTable()->vkResetCommandPool(dev.getDevice(), _commandPool, release
		// ? VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT : 0);

		Vector<VkCommandBuffer> buffersToFree;
		for (auto &it : _buffers) {
			if (it) {
				buffersToFree.emplace_back(
						static_cast<const CommandBuffer *>(it.get())->getBuffer());
			}
		}
		if (buffersToFree.size() > 0) {
			dev.getTable()->vkFreeCommandBuffers(dev.getDevice(), _commandPool,
					static_cast<uint32_t>(buffersToFree.size()), buffersToFree.data());
		}

		if (dev.isPortabilityMode()) {
			_invalidated = true;
		} else {
			recreatePool(dev);
		}
	}

	core::CommandPool::reset(cdev);
}

void CommandPool::autorelease(Rc<Ref> &&ref) { _autorelease.emplace_back(move(ref)); }

void CommandPool::recreatePool(Device &dev) {
	if (_commandPool) {
		dev.getTable()->vkDestroyCommandPool(dev.getDevice(), _commandPool, nullptr);
		_commandPool = VkCommandPool(0);
		_object.handle = ObjectHandle::zero();
	}

	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.pNext = nullptr;
	poolInfo.queueFamilyIndex = _familyIdx;
	poolInfo.flags = 0; // transient ? VK_COMMAND_POOL_CREATE_TRANSIENT_BIT : 0;

	dev.getTable()->vkCreateCommandPool(dev.getDevice(), &poolInfo, nullptr, &_commandPool);

	_object.handle = ObjectHandle(_commandPool);

	_invalidated = false;
}

static void QueryPool_destroy(core::Device *dev, core::ObjectType, core::ObjectHandle ptr, void *) {
	auto d = static_cast<Device *>(dev);
	auto target = (VkQueryPool)ptr.get();
	if (target) {
		d->getTable()->vkDestroyQueryPool(d->getDevice(), target, nullptr);
		target = VK_NULL_HANDLE;
	}
}

bool QueryPool::init(Device &dev, uint32_t familyIdx, core::QueueFlags c,
		const core::QueryPoolInfo &qinfo) {
	_info = qinfo;

	VkQueryPoolCreateInfo info;
	info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
	info.pNext = nullptr;
	info.queryType = VkQueryType(_info.type);
	info.queryCount = _info.queryCount;
	info.pipelineStatistics = toInt(_info.statFlags);
	info.flags = 0;

	if (dev.getTable()->vkCreateQueryPool(dev.getDevice(), &info, nullptr, &_queryPool)
			== VK_SUCCESS) {

		return core::QueryPool::init(dev, QueryPool_destroy, core::ObjectType::QueryPool,
				core::ObjectHandle(_queryPool));
		return true;
	}
	return false;
}

void QueryPool::reset(core::Device &cdev) { core::QueryPool::reset(cdev); }

Status QueryPool::getResults(Device &dev,
		const Callback<void(SpanView<uint64_t>, uint32_t tag)> &cb) {
	if (_usedQueries == 0) {
		return Status::Declined;
	}

	uint32_t valuesInQuery = 1;
	Vector<uint64_t> results;

	switch (_info.type) {
	case core::QueryType::Timestamp: results.resize(_usedQueries); break;
	default: return Status::ErrorNotImplemented;
	}

	VkDeviceSize stride = valuesInQuery * sizeof(uint64_t);

	auto res = dev.getTable()->vkGetQueryPoolResults(dev.getDevice(), _queryPool, 0, _usedQueries,
			results.size() * sizeof(uint64_t), results.data(), stride,
			VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
	if (res == VK_SUCCESS) {
		auto buf = makeSpanView(results);
		for (uint32_t i = 0; i < _usedQueries; ++i) {
			cb(buf.sub(0, valuesInQuery), _tags[i]);
			buf += valuesInQuery;
		}
	}

	return getStatus(res);
}

} // namespace stappler::xenolith::vk
