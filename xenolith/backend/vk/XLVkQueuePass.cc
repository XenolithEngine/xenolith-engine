/**
 Copyright (c) 2021-2022 Roman Katuntsev <sbkarr@stappler.org>
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

#include "XLVkQueuePass.h"

#include "XLCoreEnum.h"
#include "XLCoreMaterial.h"
#include "XLVkAllocator.h"
#include "XLVkAttachment.h"
#include "XLVkDevice.h"
#include "XLVkDeviceQueue.h"
#include "XLVkTextureSet.h"
#include "XLVkRenderPass.h"
#include "XLVkLoop.h"
#include "XLVkPipeline.h"
#include "XLCoreFrameQueue.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk {

QueuePass::~QueuePass() { }

bool QueuePass::init(QueuePassBuilder &passBuilder) {
	if (core::QueuePass::init(passBuilder)) {
		switch (getType()) {
		case core::PassType::Graphics:
		case core::PassType::Generic: _queueOps = core::QueueFlags::Graphics; break;
		case core::PassType::Compute: _queueOps = core::QueueFlags::Compute; break;
		case core::PassType::Transfer: _queueOps = core::QueueFlags::Transfer; break;
		}
		return true;
	}
	return false;
}

void QueuePass::invalidate() { }

Rc<core::QueuePassHandle> QueuePass::makeFrameHandle(const FrameQueue &queue) {
	if (_frameHandleCallback) {
		return _frameHandleCallback(*this, queue);
	}
	return Rc<vk::QueuePassHandle>::create(*this, queue);
}

VkRect2D QueuePassHandle::rotateScissor(const core::FrameConstraints &constraints,
		const URect &scissor) {
	VkRect2D scissorRect{
		{int32_t(scissor.x), int32_t(constraints.extent.height - scissor.y - scissor.height)},
		{scissor.width, scissor.height}};

	switch (core::getPureTransform(constraints.transform)) {
	case core::SurfaceTransformFlags::Rotate90:
		scissorRect.offset.y = scissor.x;
		scissorRect.offset.x = scissor.y;
		std::swap(scissorRect.extent.width, scissorRect.extent.height);
		break;
	case core::SurfaceTransformFlags::Rotate180: scissorRect.offset.y = scissor.y; break;
	case core::SurfaceTransformFlags::Rotate270:
		scissorRect.offset.y = constraints.extent.height - scissor.x - scissor.width;
		scissorRect.offset.x = constraints.extent.width - scissor.y - scissor.height;
		//scissorRect.offset.x = extent.height - scissor.y;
		std::swap(scissorRect.extent.width, scissorRect.extent.height);
		break;
	default: break;
	}

	if (scissorRect.offset.x < 0) {
		scissorRect.extent.width -= scissorRect.offset.x;
		scissorRect.offset.x = 0;
	}

	if (scissorRect.offset.y < 0) {
		scissorRect.extent.height -= scissorRect.offset.y;
		scissorRect.offset.y = 0;
	}

	return scissorRect;
}

QueuePassHandle::~QueuePassHandle() { invalidate(); }

void QueuePassHandle::invalidate() {
	if (_pool) {
		_device->releaseCommandPoolUnsafe(move(_pool));
		_pool = nullptr;
	}

	if (_queue) {
		_device->releaseQueue(move(_queue));
		_queue = nullptr;
	}

	_sync = nullptr;
}

bool QueuePassHandle::prepare(FrameQueue &q, Function<void(bool)> &&cb) {
	_onPrepared = sp::move(cb);
	_device = static_cast<Device *>(q.getFrame()->getDevice());
	_pool = static_cast<CommandPool *>(_device->acquireCommandPool(getQueueOps()).get());

	_constraints = q.getFrame()->getFrameConstraints();

	if (!_pool) {
		invalidate();
		return false;
	}

	prepareSubpasses(q);

	for (uint32_t i = 0; i < _data->pipelineLayouts.size(); ++i) {
		_descriptors.emplace_back(
				static_cast<RenderPass *>(_data->impl.get())->acquireDescriptorPool(*_device, i));
	}

	// If updateAfterBind feature supported for all renderpass bindings
	// - we can use separate thread to update them
	// (ordering for bind|update is not defined in this case)

	if (_data->hasUpdateAfterBind) {
		q.getFrame()->performInQueue([this](FrameHandle &frame) {
			for (auto &it : _descriptors) {
				if (it) {
					if (!static_cast<RenderPass *>(_data->impl.get())
									->writeDescriptors(*this, it, true)) {
						return false;
					}
				}
			}
			return true;
		}, [this](FrameHandle &frame, bool success) {
			if (!success) {
				_valid = false;
				log::source().error("VK-Error", "Fail to doPrepareDescriptors");
			}

			_descriptorsReady = true;
			if (_commandsReady && _descriptorsReady) {
				_onPrepared(_valid);
				_onPrepared = nullptr;
			}
		}, this, "RenderPass::doPrepareDescriptors");
	} else {
		_descriptorsReady = true;
	}

	q.getFrame()->performInQueue([this](FrameHandle &frame) {
		for (auto &it : _descriptors) {
			if (it) {
				if (!static_cast<RenderPass *>(_data->impl.get())
								->writeDescriptors(*this, it, false)) {
					return false;
				}
			}
		}

		auto ret = doPrepareCommands(frame);
		if (!ret.empty()) {
			_buffers = sp::move(ret);
			return true;
		}
		return false;
	}, [this, cb](FrameHandle &frame, bool success) {
		if (!success) {
			log::source().error("VK-Error", "Fail to doPrepareCommands");
			_valid = false;
		}

		_commandsReady = true;
		if (_commandsReady && _descriptorsReady) {
			_onPrepared(_valid);
			_onPrepared = nullptr;
		}
	}, this, "QueuePassHandle::doPrepareCommands");
	return false;
}

void QueuePassHandle::submit(FrameQueue &q, Rc<FrameSync> &&sync, Function<void(bool)> &&onSubmited,
		Function<void(bool)> &&onComplete) {
	if (!_pool) {
		onSubmited(true);
		q.getFrame()->performInQueue(
				[onComplete = sp::move(onComplete)](FrameHandle &frame) mutable {
			onComplete(true);
			return true;
		}, this, "QueuePassHandle::complete");
		return;
	}

	Rc<FrameHandle> f = q.getFrame(); // capture frame ref

	_fence->addRelease([dev = _device, pool = _pool, loop = q.getLoop()](bool success) {
		dev->releaseCommandPool(*loop, Rc<CommandPool>(pool));
	}, nullptr, "QueuePassHandle::submit dev->releaseCommandPool");

	_fence->addQueryCallback(
			[this, func = sp::move(onComplete), q = &q](bool success,
					SpanView<Rc<core::QueryPool>> queries) mutable {
		doProcessQueries(*q, queries);
		doComplete(*q, sp::move(func), success);
	},
			this, "QueuePassHandle::submit onComplete");

	for (auto &pool : _descriptors) {
		if (pool) {
			_fence->addRelease(
					[pool, pass = static_cast<RenderPass *>(_data->impl.get())](
							bool success) mutable { pass->releaseDescriptorPool(move(pool)); },
					_data->impl.get(), "QueuePassHandle::pass->releaseDescriptorPool");
		}
	}

	_sync = move(sync);

	auto ops = getQueueOps();

	_device->acquireQueue(ops, *f.get(),
			[this, onSubmited = sp::move(onSubmited)](FrameHandle &frame,
					const Rc<core::DeviceQueue> &queue) mutable {
		_queue = static_cast<DeviceQueue *>(queue.get());

		frame.performInQueue([this, onSubmited = sp::move(onSubmited)](FrameHandle &frame) mutable {
			if (!doSubmit(frame, sp::move(onSubmited))) {
				return false;
			}
			return true;
		}, this, "QueuePassHandle::submit");
	},
			[this](FrameHandle &frame) {
		_sync = nullptr;
		invalidate();
	}, this);
}

void QueuePassHandle::finalize(FrameQueue &, bool success) { }

core::QueueFlags QueuePassHandle::getQueueOps() const {
	return (static_cast<vk::QueuePass *>(_queuePass.get()))->getQueueOps();
}

Vector<const core::CommandBuffer *> QueuePassHandle::doPrepareCommands(FrameHandle &handle) {
	CommandBufferInfo info;

	auto queue = _device->getQueueFamily(_pool->getFamilyIdx());
	if (queue->timestampValidBits > 0 && _data->acquireTimestamps > 0) {
		info.timestampQueries = _data->acquireTimestamps;
	}

	auto buf = _pool->recordBuffer(*_device, Vector<Rc<DescriptorPool>>(_descriptors),
			[&, this](CommandBuffer &buf) {
		auto pass = _data->impl.cast<vk::RenderPass>().get();
		auto queue = handle.getFrameQueue(_data->queue->queue);
		pass->perform(*this, buf, [&, this] {
			size_t i = 0;
			for (auto &it : _data->subpasses) {
				if (it->commandsCallback != nullptr) {
					it->commandsCallback(*queue, *it, buf);
				}
				if (i + 2 < _data->subpasses.size()) {
					buf.cmdNextSubpass();
				}
				++i;
			}
		}, true);
		return true;
	}, move(info));
	return Vector<const core::CommandBuffer *>{buf};
}

bool QueuePassHandle::doSubmit(FrameHandle &frame, Function<void(bool)> &&onSubmited) {
	auto success = _queue->submit(*_sync, *_pool, *_fence, _buffers, _queueIdleFlags);
	_pool = nullptr;
	frame.performOnGlThread(
			[this, success, onSubmited = sp::move(onSubmited), queue = move(_queue),
					armedTime = _fence->getArmedTime()](FrameHandle &frame) mutable {
		_queueData->submitTime = armedTime;

		if (queue) {
			_device->releaseQueue(move(queue));
			queue = nullptr;
		}

		doSubmitted(frame, sp::move(onSubmited), success == Status::Ok, move(_fence));
		_fence = nullptr;
		invalidate();

		if (success != Status::Ok) {
			log::source().error("VK-Error", "Fail to vkQueueSubmit: ", success);
		}
		_sync = nullptr;
	},
			nullptr, false, "QueuePassHandle::doSubmit");
	return success == Status::Ok;
}

void QueuePassHandle::doSubmitted(FrameHandle &handle, Function<void(bool)> &&func, bool success,
		Rc<core::Fence> &&fence) {
	auto queue = handle.getFrameQueue(_data->queue->queue);
	for (auto &it : _data->submittedCallbacks) { it(*queue, *_data, success); }

	func(success);

	fence->schedule(*_loop);
}

void QueuePassHandle::doComplete(FrameQueue &queue, Function<void(bool)> &&func, bool success) {
	for (auto &it : _data->completeCallbacks) { it(queue, *_data, success); }

	func(success);
}

void QueuePassHandle::doProcessQueries(FrameQueue &queue, SpanView<Rc<core::QueryPool>> queries) { }

void QueuePassHandle::doFinalizeTransfer(core::MaterialSet *materials,
		Vector<ImageMemoryBarrier> &outputImageBarriers,
		Vector<BufferMemoryBarrier> &outputBufferBarriers) {
	if (!materials) {
		return;
	}

	materials->foreachUpdated([&](core::MaterialId, NotNull<core::Material> m) {
		auto buf = static_cast<Buffer *>(m->getBuffer());
		if (auto b = buf->getPendingBarrier()) {
			outputBufferBarriers.emplace_back(*b);
			buf->dropPendingBarrier();
		}
	}, true);

	for (auto &it : materials->getLayouts()) {
		if (it.set) {
			it.set.get_cast<TextureSet>()->foreachPendingImageBarriers(
					[&](const ImageMemoryBarrier &b) { outputImageBarriers.emplace_back(b); },
					true);
			static_cast<TextureSet *>(it.set.get())->dropPendingBarriers();
		} else {
			log::source().error("QueuePassHandle", "No set for material layout");
		}
	}
}

auto QueuePassHandle::updateMaterials(FrameHandle &frame, NotNull<core::MaterialSet> data,
		SpanView<Rc<core::Material>> materials, SpanView<core::MaterialId> dynamicMaterials,
		SpanView<core::MaterialId> materialsToRemove) -> Vector<MaterialTransferData> {
	Vector<MaterialTransferData> ret;

	// update list of materials in set
	auto updated = data->updateMaterials(materials, dynamicMaterials, materialsToRemove,
			[&, this](const core::MaterialImage &image) -> Rc<core::ImageView> {
		for (auto &it : image.image->views) {
			if (*it == image.info || it->view->getInfo() == image.info) {
				return it->view;
			}
		}

		return Rc<ImageView>::create(*_device, image.image->image.get_cast<Image>(),
				image.info);
	});

	if (updated.empty()) {
		return ret;
	}

	auto layout = data->getTargetLayout();

	// update texture layout descriptors
	// here we can place UpdateWhilePending optimization in future, to update set in use instead of copy
	for (auto &it : data->getLayouts()) {
		frame.performRequiredTask([layout, target = &it](FrameHandle &handle) {
			auto dev = static_cast<Device *>(handle.getDevice());

			target->set = ref_cast<TextureSet>(layout->layout->acquireSet(*dev));
			target->set->write(*target);
			return true;
		}, data, "QueuePassHandle::updateMaterials");
	}

	auto pool = static_cast<DeviceFrameHandle &>(frame).getMemPool(&frame);

	auto owner = data->getOwner();

	// regenerate buffers for the updated materials
	for (auto &it : updated) {
		auto bufferData = owner->getMaterialData(it.get());

		auto stagingBuffer = pool->spawn(AllocationUsage::HostTransitionSource,
				BufferInfo(core::BufferUsage::TransferSrc, bufferData.size()));
		auto targetBuffer = owner->allocateMaterialPersistentBuffer(it.get());

		if (stagingBuffer->getSize() != targetBuffer->getSize()) {
			log::source().error("QueuePassHandle",
					"Material buffer size for staging and transfer must match (",
					stagingBuffer->getSize(), " vs ", targetBuffer->getSize(), ")");
		} else {
			stagingBuffer->map([&](uint8_t *ptr, VkDeviceSize size) {
				memcpy(ptr, bufferData.data(), std::min(size_t(size), bufferData.size()));
			}, DeviceMemoryAccess::Flush);

			ret.emplace_back(
					MaterialTransferData{it, stagingBuffer, targetBuffer.get_cast<Buffer>()});
		}
	}

	return ret;
}

vk::ComputePipeline *QueuePassHandle::getComputePipelineByName(uint32_t subpass,
		StringView name) const {
	if (_data->subpasses.size() > subpass) {
		auto pipelineIt = _data->subpasses[subpass]->computePipelines.find(name);
		if (pipelineIt != _data->subpasses[subpass]->computePipelines.end()) {
			return static_cast<vk::ComputePipeline *>((*pipelineIt)->pipeline.get());
		}
	}
	return nullptr;
}

vk::ComputePipeline *QueuePassHandle::getComputePipelineBySubName(uint32_t subpass,
		StringView subname) const {
	if (_data->subpasses.size() > subpass) {
		auto pipelineIt = _data->subpasses[subpass]->computePipelines.find(
				toString(_data->key, "_", subname));
		if (pipelineIt != _data->subpasses[subpass]->computePipelines.end()) {
			return static_cast<vk::ComputePipeline *>((*pipelineIt)->pipeline.get());
		}
	}
	return nullptr;
}

vk::GraphicPipeline *QueuePassHandle::getGraphicPipelineByName(uint32_t subpass,
		StringView name) const {
	if (_data->subpasses.size() > subpass) {
		auto pipelineIt = _data->subpasses[subpass]->graphicPipelines.find(name);
		if (pipelineIt != _data->subpasses[subpass]->graphicPipelines.end()) {
			return static_cast<vk::GraphicPipeline *>((*pipelineIt)->pipeline.get());
		}
	}
	return nullptr;
}

vk::GraphicPipeline *QueuePassHandle::getGraphicPipelineBySubName(uint32_t subpass,
		StringView subname) const {
	if (_data->subpasses.size() > subpass) {
		auto pipelineIt = _data->subpasses[subpass]->graphicPipelines.find(
				toString(_data->key, "_", subname));
		if (pipelineIt != _data->subpasses[subpass]->graphicPipelines.end()) {
			return static_cast<vk::GraphicPipeline *>((*pipelineIt)->pipeline.get());
		}
	}
	return nullptr;
}

QueuePassHandle::ImageInputOutputBarrier QueuePassHandle::getImageInputOutputBarrier(Device *dev,
		Image *image, core::AttachmentHandle &handle, const VkImageSubresourceRange &) const {
	ImageInputOutputBarrier ret;

	auto attachmentData = handle.getAttachment()->getData();
	auto passData = _queuePass->getData();

	size_t passIdx = 0;
	core::AttachmentPassData *prev = nullptr;
	core::AttachmentPassData *current = nullptr;
	core::AttachmentPassData *next = nullptr;

	for (auto &it : attachmentData->passes) {
		if (it->pass == passData) {
			current = it;
			break;
		}
		++passIdx;
	}

	if (passIdx > 0) {
		prev = attachmentData->passes[passIdx - 1];
	}
	if (passIdx + 1 < attachmentData->passes.size()) {
		next = attachmentData->passes[passIdx + 1];
	}

	if (prev) {
		bool hasLayoutTransition = current->initialLayout != prev->finalLayout
				&& current->initialLayout != core::AttachmentLayout::Ignored;
		bool hasReadWriteTransition = false;
		if (core::hasReadAccess(current->dependency.initialAccessMask)
				&& core::hasWriteAccess(prev->dependency.finalAccessMask)) {
			hasReadWriteTransition = true;
		}

		bool hasOwnershipTransfer = false;

		QueueFamilyTransfer transfer;

		if (current->pass->type != prev->pass->type) {
			auto prevQueue = dev->getQueueFamily(prev->pass->type);
			auto currentQueue = dev->getQueueFamily(current->pass->type);
			if (prevQueue != currentQueue) {
				hasOwnershipTransfer = true;
				transfer = QueueFamilyTransfer{prevQueue->index, currentQueue->index};
			}
		}

		if (hasOwnershipTransfer || hasLayoutTransition || hasReadWriteTransition) {
			ret.input = ImageMemoryBarrier(image, VkAccessFlags(prev->dependency.finalAccessMask),
					VkAccessFlags(current->dependency.initialAccessMask),
					VkImageLayout(prev->finalLayout), VkImageLayout(current->initialLayout),
					transfer);
			ret.inputFrom = prev->dependency.finalUsageStage;
			ret.inputTo = current->dependency.initialUsageStage;
		}
	} else {
		// initial image transition
		if (current->initialLayout != core::AttachmentLayout::Undefined) {
			ret.input = ImageMemoryBarrier(image, 0,
					VkAccessFlags(current->dependency.initialAccessMask),
					VkImageLayout(core::AttachmentLayout::Undefined),
					VkImageLayout(current->initialLayout));
			ret.inputFrom = core::PipelineStage::AllCommands;
			ret.inputTo = current->dependency.initialUsageStage;
		}
	}

	if (next) {
		if (current->pass->type != next->pass->type) {
			auto nextQueue = dev->getQueueFamily(next->pass->type);
			auto currentQueue = dev->getQueueFamily(current->pass->type);
			if (nextQueue != currentQueue) {
				ret.output = ImageMemoryBarrier(image,
						VkAccessFlags(current->dependency.finalAccessMask),
						VkAccessFlags(next->dependency.initialAccessMask),
						VkImageLayout(current->finalLayout), VkImageLayout(next->initialLayout),
						QueueFamilyTransfer{currentQueue->index, nextQueue->index});

				ret.outputFrom = current->dependency.finalUsageStage;
				ret.outputTo = next->dependency.initialUsageStage;
			}
		}
	}

	return ret;
}

QueuePassHandle::BufferInputOutputBarrier QueuePassHandle::getBufferInputOutputBarrier(Device *dev,
		Buffer *buffer, core::AttachmentHandle &handle, VkDeviceSize offset,
		VkDeviceSize size) const {
	auto getApplicableStage = [&](core::AttachmentPassData *data,
									  core::PipelineStage stage) -> core::PipelineStage {
		auto q = dev->getQueueFamily(data->pass->pass.get_cast<vk::QueuePass>()->getQueueOps());
		return stage & core::getStagesForQueue(q->flags);
	};

	BufferInputOutputBarrier ret;

	auto attachmentData = handle.getAttachment()->getData();
	auto passData = _queuePass->getData();

	size_t passIdx = 0;
	core::AttachmentPassData *prev = nullptr;
	core::AttachmentPassData *current = nullptr;
	core::AttachmentPassData *next = nullptr;

	for (auto &it : attachmentData->passes) {
		if (it->pass == passData) {
			current = it;
			break;
		}
		++passIdx;
	}

	if (passIdx > 0) {
		prev = attachmentData->passes[passIdx - 1];
	}
	if (passIdx + 1 < attachmentData->passes.size()) {
		next = attachmentData->passes[passIdx + 1];
	}

	if (prev) {
		bool hasReadWriteTransition = false;
		if (core::hasReadAccess(current->dependency.initialAccessMask)
				&& core::hasWriteAccess(prev->dependency.finalAccessMask)) {
			hasReadWriteTransition = true;
		}

		bool hasOwnershipTransfer = false;

		QueueFamilyTransfer transfer;

		if (current->pass->pass.get_cast<vk::QueuePass>()->getQueueOps()
				!= prev->pass->pass.get_cast<vk::QueuePass>()->getQueueOps()) {
			auto prevQueue =
					dev->getQueueFamily(prev->pass->pass.get_cast<vk::QueuePass>()->getQueueOps());
			auto currentQueue = dev->getQueueFamily(
					current->pass->pass.get_cast<vk::QueuePass>()->getQueueOps());
			if (prevQueue != currentQueue) {
				hasOwnershipTransfer = true;
				transfer = QueueFamilyTransfer{prevQueue->index, currentQueue->index};
			}
		}

		if (hasOwnershipTransfer || hasReadWriteTransition) {
			ret.input = BufferMemoryBarrier(buffer, VkAccessFlags(prev->dependency.finalAccessMask),
					VkAccessFlags(current->dependency.initialAccessMask), transfer, offset, size);

			// Vulkan VUID-vkCmdPipelineBarrier-dstStageMask-06462 states to check against CURRENT queue
			ret.inputFrom = getApplicableStage(current, prev->dependency.finalUsageStage);
			if (ret.inputFrom == core::PipelineStage::None) {
				ret.inputFrom = core::PipelineStage::AllCommands;
			}
			ret.inputTo = getApplicableStage(current, current->dependency.initialUsageStage);
		}
	}

	if (next) {
		if (current->pass->pass.get_cast<vk::QueuePass>()->getQueueOps()
				!= next->pass->pass.get_cast<vk::QueuePass>()->getQueueOps()) {
			auto nextQueue =
					dev->getQueueFamily(next->pass->pass.get_cast<vk::QueuePass>()->getQueueOps());
			auto currentQueue = dev->getQueueFamily(
					current->pass->pass.get_cast<vk::QueuePass>()->getQueueOps());
			if (nextQueue != currentQueue) {
				ret.output = BufferMemoryBarrier(buffer,
						VkAccessFlags(current->dependency.finalAccessMask),
						VkAccessFlags(next->dependency.initialAccessMask),
						QueueFamilyTransfer{currentQueue->index, nextQueue->index}, offset, size);

				// Vulkan VUID-vkCmdPipelineBarrier-dstStageMask-06462 states to check against CURRENT queue
				ret.outputFrom = getApplicableStage(current, current->dependency.finalUsageStage);
				ret.outputTo = getApplicableStage(current, next->dependency.initialUsageStage);
				if (ret.outputTo == core::PipelineStage::None) {
					ret.outputTo = core::PipelineStage::AllCommands;
				}
			}
		}
	}

	return ret;
}

void QueuePassHandle::setQueueIdleFlags(core::DeviceIdleFlags flags) { _queueIdleFlags = flags; }

} // namespace stappler::xenolith::vk
