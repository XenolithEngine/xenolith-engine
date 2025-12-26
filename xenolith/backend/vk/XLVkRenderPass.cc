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

#include "XLCoreEnum.h"
#include "XLCoreInfo.h"
#include "XLVkDevice.h"
#include "XLVkRenderPass.h"
#include "XLVkQueuePass.h"
#include "XLVkAttachment.h"
#include "XLVkTextureSet.h"
#include "XLCoreAttachment.h"
#include "XLCoreFrameQueue.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk {

bool Framebuffer::init(Device &dev, RenderPass *renderPass,
		SpanView<Rc<core::ImageView>> imageViews) {
	Vector<VkImageView> views;
	views.reserve(imageViews.size());
	_viewIds.reserve(imageViews.size());
	_imageViews.reserve(imageViews.size());
	_renderPass = renderPass;

	auto extent = imageViews.front()->getFramebufferExtent();

	for (auto &it : imageViews) {
		views.emplace_back(((ImageView *)it.get())->getImageView());
		_viewIds.emplace_back(it->getIndex());
		_imageViews.emplace_back(it);

		if (extent != it->getFramebufferExtent()) {
			log::source().error("Framebuffer",
					"Invalid extent for framebuffer image: ", it->getFramebufferExtent());
			return false;
		}
	}

	VkFramebufferCreateInfo framebufferInfo{};
	framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferInfo.renderPass = renderPass->getRenderPass();
	framebufferInfo.attachmentCount = uint32_t(views.size());
	framebufferInfo.pAttachments = views.data();
	framebufferInfo.width = extent.width;
	framebufferInfo.height = extent.height;
	framebufferInfo.layers = extent.depth;

	if (dev.getTable()->vkCreateFramebuffer(dev.getDevice(), &framebufferInfo, nullptr,
				&_framebuffer)
			== VK_SUCCESS) {
		_extent = Extent2(extent.width, extent.height);
		_layerCount = extent.depth;
		return core::Framebuffer::init(dev,
				[](core::Device *dev, core::ObjectType, ObjectHandle ptr, void *) {
			auto d = ((Device *)dev);
			d->getTable()->vkDestroyFramebuffer(d->getDevice(), (VkFramebuffer)ptr.get(), nullptr);
		}, core::ObjectType::Framebuffer, ObjectHandle(_framebuffer));
	}
	return false;
}

PipelineLayout::~PipelineLayout() { invalidate(); }

bool PipelineLayout::init(Device &dev, const core::PipelineLayoutData &data, uint32_t index) {
	auto cleanup = [&]() {
		for (VkDescriptorSetLayout &set : _layouts) {
			dev.getTable()->vkDestroyDescriptorSetLayout(dev.getDevice(), set, nullptr);
		}
		_layouts.clear();
		return false;
	};

	auto incrementSize = [&](VkDescriptorType type, uint32_t count) {
		auto lb = std::lower_bound(_sizes.begin(), _sizes.end(), type,
				[](const VkDescriptorPoolSize &l, VkDescriptorType r) { return l.type < r; });
		if (lb == _sizes.end()) {
			_sizes.emplace_back(VkDescriptorPoolSize({type, count}));
		} else if (lb->type == type) {
			lb->descriptorCount += count;
		} else {
			_sizes.emplace(lb, VkDescriptorPoolSize({type, count}));
		}
	};

	for (auto &setData : data.sets) {
		++_maxSets;

		Vector<DescriptorBindingInfo> descriptors;

		bool hasFlags = false;
		Vector<VkDescriptorBindingFlags> flags;
		VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
		Vector<VkDescriptorSetLayoutBinding> bindings;
		bindings.reserve(setData->descriptors.size());
		uint32_t bindingIdx = 0;
		for (auto &binding : setData->descriptors) {
			VkDescriptorBindingFlags bindingFlags = 0;
			if (hasFlag(binding->deviceFlags, core::DescriptorFlags::UpdateAfterBind)) {
				bindingFlags |= VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT;
				hasFlags = true;
				_updateAfterBind = true;
			}
			if (hasFlag(binding->deviceFlags, core::DescriptorFlags::PartiallyBound)) {
				bindingFlags |= VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT;
				hasFlags = true;
				_updateAfterBind = true;
			}

			flags.emplace_back(bindingFlags);

			binding->index = bindingIdx;

			VkDescriptorSetLayoutBinding b;
			b.binding = bindingIdx;
			b.descriptorCount = binding->count;
			b.descriptorType = VkDescriptorType(binding->type);
			b.stageFlags = VkShaderStageFlags(binding->stages);
			if (binding->type == core::DescriptorType::Sampler) {
				// do nothing
				log::source().warn("vk::RenderPass",
						"gl::DescriptorType::Sampler is not supported for descriptors");
			} else {
				incrementSize(VkDescriptorType(binding->type), binding->count);
				b.pImmutableSamplers = nullptr;
			}
			bindings.emplace_back(b);
			descriptors.emplace_back(
					DescriptorBindingInfo{binding->type, binding->deviceFlags, binding->count});
			++bindingIdx;
		}
		VkDescriptorSetLayoutCreateInfo layoutInfo{};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.pNext = nullptr;
		layoutInfo.bindingCount = uint32_t(bindings.size());
		layoutInfo.pBindings = bindings.data();
		layoutInfo.flags = 0;

		if (hasFlags) {
			layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT;

			VkDescriptorSetLayoutBindingFlagsCreateInfoEXT bindingFlags;
			bindingFlags.sType =
					VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT;
			bindingFlags.pNext = nullptr;
			bindingFlags.bindingCount = uint32_t(flags.size());
			bindingFlags.pBindingFlags = flags.data();
			layoutInfo.pNext = &bindingFlags;

			if (dev.getTable()->vkCreateDescriptorSetLayout(dev.getDevice(), &layoutInfo, nullptr,
						&setLayout)
					== VK_SUCCESS) {
				_descriptors.emplace_back(sp::move(descriptors));
				_layouts.emplace_back(setLayout);
			} else {
				return cleanup();
			}
		} else {
			if (dev.getTable()->vkCreateDescriptorSetLayout(dev.getDevice(), &layoutInfo, nullptr,
						&setLayout)
					== VK_SUCCESS) {
				_descriptors.emplace_back(sp::move(descriptors));
				_layouts.emplace_back(setLayout);
			} else {
				return cleanup();
			}
		}
	}

	Vector<VkPushConstantRange> ranges;

	auto addRange = [&](VkShaderStageFlags flags, uint32_t offset, uint32_t size) {
		for (auto &it : ranges) {
			if (it.stageFlags == flags) {
				if (offset < it.offset) {
					it.size += (it.offset - offset);
					it.offset = offset;
				}
				if (size > it.size) {
					it.size = size;
				}
				return;
			}
		}

		ranges.emplace_back(VkPushConstantRange{flags, offset, size});
	};

	for (auto &pipeline : data.graphicPipelines) {
		for (auto &shader : pipeline->shaders) {
			for (auto &constantBlock : shader.data->constants) {
				addRange(VkShaderStageFlags(shader.data->stage), constantBlock.offset,
						constantBlock.size);
			}
		}
	}

	for (auto &pipeline : data.computePipelines) {
		for (auto &constantBlock : pipeline->shader.data->constants) {
			addRange(VkShaderStageFlags(pipeline->shader.data->stage), constantBlock.offset,
					constantBlock.size);
		}
	}

	Vector<VkDescriptorSetLayout> layouts(_layouts);

	if (data.textureSetLayout && data.textureSetLayout->layout) {
		layouts.emplace_back(
				data.textureSetLayout->layout.get_cast<TextureSetLayout>()->getLayout());
	}

	VkPipelineLayoutCreateInfo pipelineLayoutInfo;
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.pNext = nullptr;
	pipelineLayoutInfo.flags = 0;
	pipelineLayoutInfo.setLayoutCount = uint32_t(layouts.size());
	pipelineLayoutInfo.pSetLayouts = layouts.data();
	pipelineLayoutInfo.pushConstantRangeCount = uint32_t(ranges.size());
	pipelineLayoutInfo.pPushConstantRanges = ranges.data();

	if (dev.getTable()->vkCreatePipelineLayout(dev.getDevice(), &pipelineLayoutInfo, nullptr,
				&_layout)
			== VK_SUCCESS) {
		_index = index;
		return core::Object::init(dev,
				[](core::Device *dev, core::ObjectType, ObjectHandle layout, void *layouts) {
			auto d = ((Device *)dev);

			for (VkDescriptorSetLayout &set : *(decltype(&_layouts))layouts) {
				d->getTable()->vkDestroyDescriptorSetLayout(d->getDevice(), set, nullptr);
			}

			d->getTable()->vkDestroyPipelineLayout(d->getDevice(), (VkPipelineLayout)layout.get(),
					nullptr);
		}, core::ObjectType::PipelineLayout, ObjectHandle(_layout), &_layouts);
	}
	return cleanup();
}

DescriptorPool::~DescriptorPool() {
	for (auto &it : _sets) { it->bindings.clear(); }
	invalidate();
}

bool DescriptorPool::init(Device &dev, PipelineLayout *layout) {
	auto sizes = layout->getSizes();

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.pNext = nullptr;
	if (layout->haveUpdateAfterBind()) {
		poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT;
	} else {
		poolInfo.flags = 0;
	}
	poolInfo.poolSizeCount = uint32_t(sizes.size());
	poolInfo.pPoolSizes = sizes.data();
	poolInfo.maxSets = layout->getMaxSets();

	if (dev.getTable()->vkCreateDescriptorPool(dev.getDevice(), &poolInfo, nullptr, &_pool)
			!= VK_SUCCESS) {
		return false;
	}

	auto layouts = layout->getLayouts();

	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.pNext = nullptr;
	allocInfo.descriptorPool = _pool;
	allocInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
	allocInfo.pSetLayouts = layouts.data();

	Vector<VkDescriptorSet> sets;
	sets.resize(layouts.size());

	if (dev.getTable()->vkAllocateDescriptorSets(dev.getDevice(), &allocInfo, sets.data())
			!= VK_SUCCESS) {
		sets.clear();
		dev.getTable()->vkDestroyDescriptorPool(dev.getDevice(), _pool, nullptr);
		_pool = VK_NULL_HANDLE;
		return false;
	}

	uint32_t setIndex = 0;
	_sets.reserve(sets.size());
	for (auto &it : sets) {
		auto set = Rc<DescriptorSetBindings>::alloc();
		auto info = layout->getDescriptorsInfo(setIndex);
		set->idx = setIndex;
		set->set = it;
		for (auto &d : info) { set->bindings.emplace_back(d.type, d.flags, d.count); }
		_sets.emplace_back(move(set));
		++setIndex;
	}

	_layout = layout;
	_layoutIndex = layout->getIndex();
	return core::Object::init(dev,
			[](core::Device *dev, core::ObjectType, ObjectHandle pool, void *ptr) {
		auto d = ((Device *)dev);
		d->getTable()->vkDestroyDescriptorPool(d->getDevice(), (VkDescriptorPool)pool.get(),
				nullptr);
	}, core::ObjectType::PipelineLayout, ObjectHandle(_pool));
}

bool RenderPass::Data::cleanup(Device &dev) {
	if (renderPass) {
		dev.getTable()->vkDestroyRenderPass(dev.getDevice(), renderPass, nullptr);
		renderPass = VK_NULL_HANDLE;
	}

	if (renderPassAlternative) {
		dev.getTable()->vkDestroyRenderPass(dev.getDevice(), renderPassAlternative, nullptr);
		renderPassAlternative = VK_NULL_HANDLE;
	}

	layouts.clear();

	return false;
}

bool RenderPass::init(Device &dev, QueuePassData &data) {
	_type = data.pass->getType();
	_name = data.key.str<Interface>();
	switch (_type) {
	case core::PassType::Graphics: return initGraphicsPass(dev, data); break;
	case core::PassType::Compute: return initComputePass(dev, data); break;
	case core::PassType::Transfer: return initTransferPass(dev, data); break;
	case core::PassType::Generic: return initGenericPass(dev, data); break;
	}

	return false;
}

VkRenderPass RenderPass::getRenderPass(bool alt) const {
	if (alt && _data->renderPassAlternative) {
		return _data->renderPassAlternative;
	}
	return _data->renderPass;
}

Rc<DescriptorPool> RenderPass::acquireDescriptorPool(Device &dev, uint32_t idx) {
	if (idx >= _data->layouts.size()) {
		return nullptr;
	}

	std::unique_lock lock(_descriptorPoolMutex);
	auto &vec = _descriptorPools[idx];
	if (!vec.empty()) {
		auto ret = vec.back();
		vec.pop_back();
		return ret;
	} else {
		lock.unlock();
		if (_data->layouts[idx]->getMaxSets() > 0) {
			return Rc<DescriptorPool>::create(dev, _data->layouts[idx]);
		} else {
			return nullptr;
		}
	}
}

void RenderPass::releaseDescriptorPool(Rc<DescriptorPool> &&pool) {
	auto index = pool->getLayoutIndex();
	std::unique_lock lock(_descriptorPoolMutex);
	auto &vec = _descriptorPools[index];
	vec.emplace_back(move(pool));
}

static bool RenderPass_canSimulatePartiallyBound(core::DescriptorType type) {
	switch (type) {
	case core::DescriptorType::SampledImage:
	case core::DescriptorType::StorageImage:
	case core::DescriptorType::UniformBuffer:
	case core::DescriptorType::StorageBuffer:
	case core::DescriptorType::UniformBufferDynamic:
	case core::DescriptorType::StorageBufferDynamic: return true; break;
	case core::DescriptorType::Sampler:
	case core::DescriptorType::CombinedImageSampler:
	case core::DescriptorType::InputAttachment:
	case core::DescriptorType::StorageTexelBuffer:
	case core::DescriptorType::UniformTexelBuffer:
	case core::DescriptorType::Unknown:
	case core::DescriptorType::Attachment: return false; break;
	}
	return false;
}

bool RenderPass::writeDescriptors(const QueuePassHandle &handle, DescriptorPool *pool,
		bool async) const {
	auto dev = (Device *)_object.device;
	auto table = dev->getTable();
	auto data = handle.getData();

	const auto emptyImage = handle.getData()->queue->emptyImage;
	const auto emptyBuffer = handle.getData()->queue->emptyBuffer;

	std::forward_list<Vector<VkDescriptorImageInfo>> images;
	std::forward_list<Vector<VkDescriptorBufferInfo>> buffers;
	std::forward_list<Vector<VkBufferView>> views;

	Vector<VkWriteDescriptorSet> writes;

	Vector<VkDescriptorImageInfo> *localImages = nullptr;
	Vector<VkDescriptorBufferInfo> *localBuffers = nullptr;
	Vector<VkBufferView> *localViews = nullptr;

	VkWriteDescriptorSet writeData;

	auto clearWrite = [](VkWriteDescriptorSet &writeData, DescriptorSetBindings *set, uint32_t desc,
							  uint32_t idx, DescriptorType type) {
		writeData.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeData.pNext = nullptr;
		writeData.dstSet = set->set;
		writeData.dstBinding = desc;
		writeData.dstArrayElement = idx;
		writeData.descriptorCount = 0;
		writeData.descriptorType = VkDescriptorType(type);
		writeData.pImageInfo = VK_NULL_HANDLE;
		writeData.pBufferInfo = VK_NULL_HANDLE;
		writeData.pTexelBufferView = VK_NULL_HANDLE;
	};

	auto flushWrite = [&]() {
		if (localImages) {
			writeData.pImageInfo = localImages->data();
			writeData.descriptorCount = localImages->size();
		}
		if (localBuffers) {
			writeData.pBufferInfo = localBuffers->data();
			writeData.descriptorCount = localBuffers->size();
		}
		if (localViews) {
			writeData.pTexelBufferView = localViews->data();
			writeData.descriptorCount = localViews->size();
		}

		if (writeData.descriptorCount > 0) {
			writes.emplace_back(move(writeData));
		}

		localImages = nullptr;
		localBuffers = nullptr;
		localViews = nullptr;
	};

	auto writeImage = [&](core::AttachmentHandle *a, const PipelineDescriptor &desc,
							  core::DescriptorBinding &bindings, uint32_t idx) {
		if (!localImages) {
			localImages = &images.emplace_front(Vector<VkDescriptorImageInfo>());
		}

		auto h = static_cast<ImageAttachmentHandle *>(a);
		core::DescriptorImageInfo info(&desc, idx);
		if (h->writeDescriptor(handle, info)) {
			localImages->emplace_back(VkDescriptorImageInfo{
				info.sampler ? info.sampler.get_cast<Sampler>()->getSampler() : VK_NULL_HANDLE,
				info.imageView.get_cast<ImageView>()->getImageView(), VkImageLayout(info.layout)});
			// preserve previous binding until new will be accepted by GPU
			if (auto ref = bindings.write(idx, move(info))) {
				handle.autorelease(ref);
			}
		} else {
			return false;
		}
		return true;
	};

	auto writeBuffer = [&](core::AttachmentHandle *a, const PipelineDescriptor &desc,
							   core::DescriptorBinding &bindings, uint32_t idx) {
		if (!localBuffers) {
			localBuffers = &buffers.emplace_front(Vector<VkDescriptorBufferInfo>());
		}

		auto h = static_cast<BufferAttachmentHandle *>(a);
		core::DescriptorBufferInfo info(&desc, idx);
		if (h->writeDescriptor(handle, info)) {
			localBuffers->emplace_back(VkDescriptorBufferInfo{
				info.buffer.get_cast<Buffer>()->getBuffer(), info.offset, info.range});
			// preserve previous binding until new will be accepted by GPU
			if (auto ref = bindings.write(idx, move(info))) {
				handle.autorelease(ref);
			}
		} else {
			return false;
		}
		return true;
	};

	auto writeTexel = [&](core::AttachmentHandle *a, const PipelineDescriptor &desc,
							  core::DescriptorBinding &bindings, uint32_t idx) {
		if (!localViews) {
			localViews = &views.emplace_front(Vector<VkBufferView>());
		}

		auto h = static_cast<TexelAttachmentHandle *>(a);
		core::DescriptorBufferViewInfo info(&desc, idx);
		if (h->writeDescriptor(handle, info)) {
			localViews->emplace_back(VkBufferView(info.target.get()));
			// preserve previous binding until new will be accepted by GPU
			if (auto ref = bindings.write(idx, move(info))) {
				handle.autorelease(ref);
			}
		} else {
			return false;
		}
		return true;
	};

	auto writeEmpty = [&](const PipelineDescriptor &desc, core::DescriptorBinding &bindings,
							  uint32_t idx) {
		switch (bindings.type) {
		case core::DescriptorType::SampledImage:
		case core::DescriptorType::StorageImage:
			if (!localImages) {
				localImages = &images.emplace_front(Vector<VkDescriptorImageInfo>());
			}
			localImages->emplace_back(VkDescriptorImageInfo{VK_NULL_HANDLE,
				emptyImage->views.front()->view.get_cast<ImageView>()->getImageView(),
				VkImageLayout(core::AttachmentLayout::ShaderReadOnlyOptimal)});

			// preserve previous binding until new will be accepted by GPU
			if (auto ref = bindings.write(idx,
						core::DescriptorImageInfo(&desc, idx, emptyImage->views.front()->view))) {
				handle.autorelease(ref);
			}
			return true;
			break;
		case core::DescriptorType::UniformBuffer:
		case core::DescriptorType::StorageBuffer:
		case core::DescriptorType::UniformBufferDynamic:
		case core::DescriptorType::StorageBufferDynamic:
			if (!localBuffers) {
				localBuffers = &buffers.emplace_front(Vector<VkDescriptorBufferInfo>());
			}
			localBuffers->emplace_back(VkDescriptorBufferInfo{
				emptyBuffer->buffer.get_cast<Buffer>()->getBuffer(), 0, VK_WHOLE_SIZE});

			// preserve previous binding until new will be accepted by GPU
			if (auto ref = bindings.write(idx,
						core::DescriptorBufferInfo(&desc, idx, emptyBuffer->buffer))) {
				handle.autorelease(ref);
			}
			return true;
			break;
		case core::DescriptorType::Sampler:
		case core::DescriptorType::CombinedImageSampler:
		case core::DescriptorType::InputAttachment:
		case core::DescriptorType::StorageTexelBuffer:
		case core::DescriptorType::UniformTexelBuffer:
		case core::DescriptorType::Unknown:
		case core::DescriptorType::Attachment: return false; break;
		}
		return false;
	};

	auto writeDescriptor = [&](DescriptorSetBindings *set, const PipelineDescriptor &desc,
								   uint32_t currentDescriptor) {
		auto a = handle.getAttachmentHandle(desc.attachment->attachment);
		if (!a) {
			return false;
		}

		bool success = true;
		uint32_t nextIdx = 0;

		auto simulatePartiallyBound = RenderPass_canSimulatePartiallyBound(desc.type);
		auto &bindings = set->bindings[currentDescriptor];

		clearWrite(writeData, set, currentDescriptor, 0, desc.type);

		auto flushArray = [&](uint32_t idx) {
			if (idx != nextIdx) {
				if (idx < nextIdx) {
					log::
							source()
									.warn("vk::RenderPass", "\"", _name, "\": descriptor [",
											set->idx, "][", currentDescriptor, "] (", bindings.type,
											"): enumerateDirtyDescriptors should return dirty "
											"indexes in " "monotomically incremented order");
				}

				if (!hasFlag(bindings.flags, core::DescriptorFlags::PartiallyBound)
						&& simulatePartiallyBound) {
					for (uint32_t i = nextIdx; i < idx; ++i) {
						if (bindings.get(i).empty()) {
							writeEmpty(desc, bindings, i);
						} else {
							flushWrite();
							clearWrite(writeData, set, currentDescriptor, i, desc.type);
						}
					}
				} else {
					flushWrite();
					clearWrite(writeData, set, currentDescriptor, idx, desc.type);
				}
			}
		};

		switch (desc.type) {
		case DescriptorType::Sampler:
		case DescriptorType::CombinedImageSampler:
		case DescriptorType::SampledImage:
		case DescriptorType::StorageImage:
		case DescriptorType::InputAttachment:
			a->enumerateDirtyDescriptors(handle, desc, bindings, [&](uint32_t idx) {
				flushArray(idx);
				if (!writeImage(a, desc, bindings, idx)) {
					success = false;
				}
				nextIdx = idx + 1;
			});
			break;
		case DescriptorType::StorageTexelBuffer:
		case DescriptorType::UniformTexelBuffer:
			a->enumerateDirtyDescriptors(handle, desc, bindings, [&](uint32_t idx) {
				flushArray(idx);
				if (!writeTexel(a, desc, bindings, idx)) {
					success = false;
				}
				nextIdx = idx + 1;
			});
			break;
		case DescriptorType::UniformBuffer:
		case DescriptorType::StorageBuffer:
		case DescriptorType::UniformBufferDynamic:
		case DescriptorType::StorageBufferDynamic:
			a->enumerateDirtyDescriptors(handle, desc, bindings, [&](uint32_t idx) {
				flushArray(idx);
				if (!writeBuffer(a, desc, bindings, idx)) {
					success = false;
				}
				nextIdx = idx + 1;
			});
			break;
		case DescriptorType::Unknown:
		case DescriptorType::Attachment: break;
		}

		flushWrite();

		// simulate PartiallyBound flag is possible
		if (!hasFlag(bindings.flags, core::DescriptorFlags::PartiallyBound)
				&& bindings.size() != bindings.bound) {
			if (simulatePartiallyBound) {
				for (uint32_t i = nextIdx; i < bindings.size(); ++i) {
					if (bindings.get(i).empty()) {
						writeEmpty(desc, bindings, i);
					} else {
						flushWrite();
						clearWrite(writeData, set, currentDescriptor, i, desc.type);
					}
				}
			} else {
				log::
						source()
								.warn("vk::RenderPass", "\"", _name, "\": descriptor [", set->idx,
										"][", currentDescriptor, "] (", bindings.type,
										"): DescriptorFlags::PartiallyBound is not available on "
										"device " "but descriptor array was not fully bound: ",
										bindings.bound, " of ", bindings.size());
			}
		}

		return success;
	};

	uint32_t layoutIndex = pool->getLayout()->getIndex();

	uint32_t currentSet = 0;
	for (auto &descriptorSetData : data->pipelineLayouts[layoutIndex]->sets) {
		auto set = pool->getSet(currentSet);
		uint32_t currentDescriptor = 0;
		for (auto &it : descriptorSetData->descriptors) {
			if (hasFlag(it->deviceFlags, core::DescriptorFlags::UpdateAfterBind) != async) {
				++currentDescriptor;
				continue;
			}
			if (!writeDescriptor(set, *it, currentDescriptor)) {
				return false;
			}
			++currentDescriptor;
		}
		++currentSet;
	}

	if (!writes.empty()) {
		table->vkUpdateDescriptorSets(dev->getDevice(), uint32_t(writes.size()), writes.data(), 0,
				nullptr);
	}

	return true;
}

void RenderPass::perform(const QueuePassHandle &handle, CommandBuffer &buf,
		const Callback<void()> &cb, bool writeBarriers) {
	bool useAlternative = false;
	for (auto &it : _variableAttachments) {
		if (auto aHandle = handle.getAttachmentHandle(it->attachment)) {
			if (aHandle->getQueueData()->image
					&& !aHandle->getQueueData()->image->isSwapchainImage()) {
				useAlternative = true;
				break;
			}
		}
	}

	Vector<QueuePassHandle::ImageInputOutputBarrier> imageBarriersData;
	Vector<QueuePassHandle::BufferInputOutputBarrier> bufferBarriersData;

	Vector<vk::ImageMemoryBarrier> imageBarriers;
	Vector<vk::BufferMemoryBarrier> bufferBarriers;

	core::PipelineStage fromStage = core::PipelineStage::None;
	core::PipelineStage toStage = core::PipelineStage::None;

	if (writeBarriers) {
		bool hasPendings = false;

		for (auto &it : handle.getQueueData()->attachments) {
			it.second->handle->enumerateAttachmentObjects(
					[&](core::Object *obj, const core::SubresourceRangeInfo &range) {
				switch (range.type) {
				case core::ObjectType::Buffer: {
					auto buf = static_cast<Buffer *>(obj);
					auto b = handle.getBufferInputOutputBarrier(
							static_cast<Device *>(_object.device), buf, *it.second->handle,
							range.buffer.offset, range.buffer.size);
					if (auto pending = buf->getPendingBarrier()) {
						b.input = *pending;
						buf->dropPendingBarrier();
						hasPendings = true;
					}
					if (b.input || b.output) {
						bufferBarriersData.emplace_back(move(b));
					}
					break;
				}
				case core::ObjectType::Image: {
					auto img = static_cast<Image *>(obj);
					auto b = handle.getImageInputOutputBarrier(
							static_cast<Device *>(_object.device), img, *it.second->handle,
							VkImageSubresourceRange{
								VkImageAspectFlags(range.image.aspectMask),
								range.image.baseMipLevel,
								range.image.levelCount,
								range.image.baseArrayLayer,
								range.image.layerCount,
							});
					if (auto pending = img->getPendingBarrier()) {
						b.input = *pending;
						img->dropPendingBarrier();
						hasPendings = true;
					}
					if (b.input || b.output) {
						imageBarriersData.emplace_back(move(b));
					}
					break;
				}
				default: break;
				}
			});
		}

		for (auto &it : imageBarriersData) {
			if (it.input) {
				fromStage |= it.inputFrom;
				toStage |= it.inputTo;
				imageBarriers.emplace_back(move(it.input));
			}
		}
		for (auto &it : bufferBarriersData) {
			if (it.input) {
				fromStage |= it.inputFrom;
				toStage |= it.inputTo;
				bufferBarriers.emplace_back(move(it.input));
			}
		}

		if (hasPendings) {
			if (fromStage == core::PipelineStage::None) {
				fromStage = core::PipelineStage::AllCommands;
			}
			if (toStage == core::PipelineStage::None) {
				toStage = core::PipelineStage::AllCommands;
			}
		}

		if ((!imageBarriersData.empty() || !bufferBarriersData.empty())
				&& (fromStage != core::PipelineStage::None)
				&& (toStage != core::PipelineStage::None)) {
			buf.cmdPipelineBarrier(VkPipelineStageFlags(fromStage), VkPipelineStageFlags(toStage),
					0, bufferBarriers, imageBarriers);
		}
	}

	if (_data->renderPass) {
		buf.cmdBeginRenderPass(this, (Framebuffer *)handle.getFramebuffer(),
				VK_SUBPASS_CONTENTS_INLINE, useAlternative);

		cb();

		buf.cmdEndRenderPass();
	} else {
		cb();
	}

	if (writeBarriers) {
		fromStage = core::PipelineStage::None;
		toStage = core::PipelineStage::None;

		imageBarriers.clear();
		bufferBarriers.clear();

		for (auto &it : imageBarriersData) {
			if (it.output) {
				fromStage |= it.outputFrom;
				toStage |= it.outputTo;
				imageBarriers.emplace_back(move(it.output));
			}
		}
		for (auto &it : bufferBarriersData) {
			if (it.output) {
				fromStage |= it.outputFrom;
				toStage |= it.outputTo;
				bufferBarriers.emplace_back(move(it.output));
			}
		}

		if (!imageBarriers.empty() || !bufferBarriers.empty()) {
			buf.cmdPipelineBarrier(VkPipelineStageFlags(fromStage), VkPipelineStageFlags(toStage),
					0, bufferBarriers, imageBarriers);
		}
	}
}

bool RenderPass::initGraphicsPass(Device &dev, QueuePassData &data) {
	bool hasAlternative = false;
	Data pass;

	size_t attachmentReferences = 0;
	for (auto &desc : data.attachments) {
		if (desc->attachment->type != core::AttachmentType::Image || desc->subpasses.empty()) {
			continue;
		}

		VkAttachmentDescription attachment;
		VkAttachmentDescription attachmentAlternative;

		bool mayAlias = false;
		for (auto &u : desc->subpasses) {
			if (u->usage == core::AttachmentUsage::InputOutput
					|| u->usage == core::AttachmentUsage::InputDepthStencil) {
				mayAlias = true;
			}
		}

		auto imageAttachment = (core::ImageAttachment *)desc->attachment->attachment.get();
		auto &info = imageAttachment->getImageInfo();

		attachmentAlternative.flags = attachment.flags =
				(mayAlias ? VK_ATTACHMENT_DESCRIPTION_MAY_ALIAS_BIT : 0);
		attachmentAlternative.format = attachment.format = VkFormat(info.format);
		attachmentAlternative.samples = attachment.samples = VkSampleCountFlagBits(info.samples);
		attachmentAlternative.loadOp = attachment.loadOp = VkAttachmentLoadOp(desc->loadOp);
		attachmentAlternative.storeOp = attachment.storeOp = VkAttachmentStoreOp(desc->storeOp);
		attachmentAlternative.stencilLoadOp = attachment.stencilLoadOp =
				VkAttachmentLoadOp(desc->stencilLoadOp);
		attachmentAlternative.stencilStoreOp = attachment.stencilStoreOp =
				VkAttachmentStoreOp(desc->stencilStoreOp);
		attachmentAlternative.initialLayout = attachment.initialLayout =
				VkImageLayout(desc->initialLayout);
		attachmentAlternative.finalLayout = attachment.finalLayout =
				VkImageLayout(desc->finalLayout);

		if (desc->finalLayout == core::AttachmentLayout::PresentSrc) {
			hasAlternative = true;
			attachmentAlternative.finalLayout =
					VkImageLayout(core::AttachmentLayout::TransferSrcOptimal);
			_variableAttachments.emplace(desc);
		}

		desc->index = uint32_t(_attachmentDescriptions.size());

		_attachmentDescriptions.emplace_back(attachment);
		_attachmentDescriptionsAlternative.emplace_back(attachmentAlternative);

		auto fmt = core::getImagePixelFormat(imageAttachment->getImageInfo().format);
		switch (fmt) {
		case core::PixelFormat::D:
			if (desc->loadOp == core::AttachmentLoadOp::Clear) {
				auto c = ((core::ImageAttachment *)desc->attachment->attachment.get())
								 ->getClearColor();
				VkClearValue clearValue;
				clearValue.depthStencil.depth = c.r;
				_clearValues.emplace_back(clearValue);
			} else {
				VkClearValue clearValue;
				clearValue.depthStencil.depth = 0.0f;
				_clearValues.emplace_back(clearValue);
			}
			break;
		case core::PixelFormat::DS:
			if (desc->stencilLoadOp == core::AttachmentLoadOp::Clear
					|| desc->loadOp == core::AttachmentLoadOp::Clear) {
				auto c = imageAttachment->getClearColor();
				VkClearValue clearValue;
				clearValue.depthStencil.depth = c.r;
				clearValue.depthStencil.stencil = 0;
				_clearValues.emplace_back(clearValue);
			} else {
				VkClearValue clearValue;
				clearValue.depthStencil.depth = 0.0f;
				clearValue.depthStencil.stencil = 0;
				_clearValues.emplace_back(clearValue);
			}
			break;
		case core::PixelFormat::S:
			if (desc->stencilLoadOp == core::AttachmentLoadOp::Clear) {
				VkClearValue clearValue;
				clearValue.depthStencil.stencil = 0;
				_clearValues.emplace_back(clearValue);
			} else {
				VkClearValue clearValue;
				clearValue.depthStencil.stencil = 0;
				_clearValues.emplace_back(clearValue);
			}
			break;
		default:
			if (desc->loadOp == core::AttachmentLoadOp::Clear) {
				auto c = imageAttachment->getClearColor();
				VkClearValue clearValue;
				clearValue.color = VkClearColorValue{{c.r, c.g, c.b, c.a}};
				_clearValues.emplace_back(clearValue);
			} else {
				VkClearValue clearValue;
				clearValue.color = VkClearColorValue{{0.0f, 0.0f, 0.0f, 1.0f}};
				_clearValues.emplace_back(clearValue);
			}
			break;
		}

		attachmentReferences += desc->subpasses.size();

		if (data.subpasses.size() >= 3 && desc->subpasses.size() < data.subpasses.size()) {
			uint32_t initialSubpass = desc->subpasses.front()->subpass->index;
			uint32_t finalSubpass = desc->subpasses.back()->subpass->index;

			for (size_t i = initialSubpass + 1; i < finalSubpass; ++i) {
				bool found = false;
				for (auto &u : desc->subpasses) {
					if (u->subpass->index == i) {
						found = true;
					}
				}
				if (!found) {
					data.subpasses[i]->preserve.emplace_back(desc->index);
				}
			}
		}
	}

	_attachmentReferences.reserve(attachmentReferences);

	for (auto &it : data.subpasses) {
		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

		if (!it->inputImages.empty()) {
			auto off = _attachmentReferences.size();

			for (auto &iit : it->inputImages) {
				VkAttachmentReference attachmentRef{};
				if (!iit) {
					attachmentRef.attachment = VK_ATTACHMENT_UNUSED;
					attachmentRef.layout = VK_IMAGE_LAYOUT_UNDEFINED;
				} else {
					attachmentRef.attachment = iit->pass->index;
					attachmentRef.layout = VkImageLayout(iit->layout);
				}
				_attachmentReferences.emplace_back(attachmentRef);
			}

			subpass.inputAttachmentCount = uint32_t(it->inputImages.size());
			subpass.pInputAttachments = _attachmentReferences.data() + off;
		}

		if (!it->outputImages.empty()) {
			auto off = _attachmentReferences.size();

			for (auto &iit : it->outputImages) {
				VkAttachmentReference attachmentRef{};
				if (!iit) {
					attachmentRef.attachment = VK_ATTACHMENT_UNUSED;
					attachmentRef.layout = VK_IMAGE_LAYOUT_UNDEFINED;
				} else {
					attachmentRef.attachment = iit->pass->index;
					attachmentRef.layout = VkImageLayout(iit->layout);
				}
				_attachmentReferences.emplace_back(attachmentRef);
			}

			subpass.colorAttachmentCount = uint32_t(it->outputImages.size());
			subpass.pColorAttachments = _attachmentReferences.data() + off;
		}

		if (!it->resolveImages.empty()) {
			auto resolveImages = it->resolveImages;
			if (resolveImages.size() < it->outputImages.size()) {
				resolveImages.resize(it->outputImages.size(), nullptr);
			}

			auto off = _attachmentReferences.size();

			for (auto &iit : resolveImages) {
				VkAttachmentReference attachmentRef{};
				if (!iit) {
					attachmentRef.attachment = VK_ATTACHMENT_UNUSED;
					attachmentRef.layout = VK_IMAGE_LAYOUT_UNDEFINED;
				} else {
					attachmentRef.attachment = iit->pass->index;
					attachmentRef.layout = VkImageLayout(iit->layout);
				}
				_attachmentReferences.emplace_back(attachmentRef);
			}

			subpass.pResolveAttachments = _attachmentReferences.data() + off;
		}

		if (it->depthStencil) {
			VkAttachmentReference attachmentRef{};
			attachmentRef.attachment = it->depthStencil->pass->index;
			attachmentRef.layout = VkImageLayout(it->depthStencil->layout);
			_attachmentReferences.emplace_back(attachmentRef);
			subpass.pDepthStencilAttachment = &_attachmentReferences.back();
		}

		if (!it->preserve.empty()) {
			subpass.preserveAttachmentCount = uint32_t(it->preserve.size());
			subpass.pPreserveAttachments = it->preserve.data();
		}

		_subpasses.emplace_back(subpass);
	}

	_subpassDependencies.reserve(data.dependencies.size());

	//  TODO: deal with internal dependencies through AttachmentDependencyInfo

	for (auto &it : data.dependencies) {
		VkSubpassDependency dependency{};
		dependency.srcSubpass = (it.srcSubpass == core::SubpassDependency::External)
				? VK_SUBPASS_EXTERNAL
				: it.srcSubpass;
		dependency.dstSubpass = (it.dstSubpass == core::SubpassDependency::External)
				? VK_SUBPASS_EXTERNAL
				: it.dstSubpass;
		dependency.srcStageMask = VkPipelineStageFlags(it.srcStage);
		dependency.srcAccessMask = VkAccessFlags(it.srcAccess);
		dependency.dstStageMask = VkPipelineStageFlags(it.dstStage);
		dependency.dstAccessMask = VkAccessFlags(it.dstAccess);
		dependency.dependencyFlags = 0;
		if (it.byRegion) {
			dependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
		}

		_subpassDependencies.emplace_back(dependency);
	}

	VkRenderPassCreateInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = uint32_t(_attachmentDescriptions.size());
	renderPassInfo.pAttachments = _attachmentDescriptions.data();
	renderPassInfo.subpassCount = uint32_t(_subpasses.size());
	renderPassInfo.pSubpasses = _subpasses.data();
	renderPassInfo.dependencyCount = uint32_t(_subpassDependencies.size());
	renderPassInfo.pDependencies = _subpassDependencies.data();

	if (dev.getTable()->vkCreateRenderPass(dev.getDevice(), &renderPassInfo, nullptr,
				&pass.renderPass)
			!= VK_SUCCESS) {
		return pass.cleanup(dev);
	}

	if (hasAlternative) {
		renderPassInfo.attachmentCount = uint32_t(_attachmentDescriptionsAlternative.size());
		renderPassInfo.pAttachments = _attachmentDescriptionsAlternative.data();

		if (dev.getTable()->vkCreateRenderPass(dev.getDevice(), &renderPassInfo, nullptr,
					&pass.renderPassAlternative)
				!= VK_SUCCESS) {
			return pass.cleanup(dev);
		}
	}

	if (initDescriptors(dev, data, pass)) {
		auto l = new Data(move(pass));
		_data = l;
		return core::RenderPass::init(dev,
				[](core::Device *dev, core::ObjectType, ObjectHandle, void *ptr) {
			auto d = ((Device *)dev);
			auto l = (Data *)ptr;
			l->cleanup(*d);
			delete l;
		}, core::ObjectType::RenderPass, ObjectHandle(_data->renderPass), l);
	}

	return pass.cleanup(dev);
}

bool RenderPass::initComputePass(Device &dev, QueuePassData &data) {
	Data pass;
	if (initDescriptors(dev, data, pass)) {
		auto l = new Data(move(pass));
		_data = l;
		return core::RenderPass::init(dev,
				[](core::Device *dev, core::ObjectType, ObjectHandle, void *ptr) {
			auto d = ((Device *)dev);
			auto l = (Data *)ptr;
			l->cleanup(*d);
			delete l;
		}, core::ObjectType::RenderPass, ObjectHandle(_data->renderPass), l);
	}

	return pass.cleanup(dev);
}

bool RenderPass::initTransferPass(Device &dev, QueuePassData &) {
	// init nothing - no descriptors or render pass implementation needed
	auto l = new Data();
	_data = l;
	return core::RenderPass::init(dev,
			[](core::Device *dev, core::ObjectType, ObjectHandle, void *ptr) {
		auto d = ((Device *)dev);
		auto l = (Data *)ptr;
		l->cleanup(*d);
		delete l;
	}, core::ObjectType::RenderPass, ObjectHandle(_data->renderPass), l);
}

bool RenderPass::initGenericPass(Device &dev, QueuePassData &) {
	// init nothing - no descriptors or render pass implementation needed
	auto l = new Data();
	_data = l;
	return core::RenderPass::init(dev,
			[](core::Device *dev, core::ObjectType, ObjectHandle, void *ptr) {
		auto d = ((Device *)dev);
		auto l = (Data *)ptr;
		l->cleanup(*d);
		delete l;
	}, core::ObjectType::RenderPass, ObjectHandle(_data->renderPass), l);
}

bool RenderPass::initDescriptors(Device &dev, const QueuePassData &data, Data &pass) {
	uint32_t index = 0;
	for (auto &it : data.pipelineLayouts) {
		auto layout = Rc<PipelineLayout>::create(dev, *it, index);
		if (layout) {
			pass.layouts.emplace_back(move(layout));
		} else {
			pass.layouts.clear();
			return false;
		}
		++index;
	}

	_descriptorPools.resize(pass.layouts.size());

	// preallocate one pool for each layout
	index = 0;
	for (auto &it : pass.layouts) {
		if (it->getMaxSets() > 0) {
			_descriptorPools[index].emplace_back(Rc<DescriptorPool>::create(dev, it));
		}
		++index;
	}

	return true;
}

} // namespace stappler::xenolith::vk
