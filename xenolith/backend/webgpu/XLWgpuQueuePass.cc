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

#include "XLWgpuQueuePass.h"
#include "XLWgpuLoop.h"
#include "XLWgpuPipeline.h"
#include "XLWgpuTextureSet.h"
#include "XLCoreFrameQueue.h"
#include "XLCoreImageStorage.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::webgpu {

static WGPULoadOp getWGPULoadOp(core::AttachmentLoadOp op) {
	switch (op) {
	case core::AttachmentLoadOp::Load: return WGPULoadOp_Load; break;
	case core::AttachmentLoadOp::Clear: return WGPULoadOp_Clear; break;
	case core::AttachmentLoadOp::DontCare: return WGPULoadOp_Clear; break;
	}
	return WGPULoadOp_Clear;
}

static WGPUStoreOp getWGPUStoreOp(core::AttachmentStoreOp op) {
	switch (op) {
	case core::AttachmentStoreOp::Store: return WGPUStoreOp_Store; break;
	case core::AttachmentStoreOp::DontCare: return WGPUStoreOp_Discard; break;
	}
	return WGPUStoreOp_Store;
}

static WGPUShaderStage getWGPUShaderStage(core::ProgramStage stages) {
	WGPUShaderStage ret = WGPUShaderStage_None;
	if (hasFlag(stages, core::ProgramStage::Vertex)) {
		ret |= WGPUShaderStage_Vertex;
	}
	if (hasFlag(stages, core::ProgramStage::Fragment)) {
		ret |= WGPUShaderStage_Fragment;
	}
	if (hasFlag(stages, core::ProgramStage::Compute)) {
		ret |= WGPUShaderStage_Compute;
	}
	if (ret == WGPUShaderStage_None) {
		// WGSL programs are not reflected, descriptor stages may be left unset
		ret = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment | WGPUShaderStage_Compute;
	}
	return ret;
}

RenderPass::~RenderPass() {
	for (auto &layout : _layouts) {
		for (auto &it : layout.bindGroupLayouts) { wgpuBindGroupLayoutRelease(it); }
		if (layout.pipelineLayout) {
			wgpuPipelineLayoutRelease(layout.pipelineLayout);
		}
	}
	_layouts.clear();
}

void BufferAttachmentHandle::addBufferView(Rc<Buffer> &&buffer, uint64_t offset, uint64_t size) {
	_buffers.emplace_back(BufferView{sp::move(buffer), offset, size});
}

Rc<core::AttachmentHandle> BufferAttachment::makeFrameHandle(const core::FrameQueue &queue) {
	if (_frameHandleCallback) {
		return _frameHandleCallback(*this, queue);
	}

	auto handle = Rc<webgpu::BufferAttachmentHandle>::create(*this, queue);
	if (handle && isStatic()) {
		for (auto &it : getStaticBuffers()) {
			handle->addBufferView(Rc<Buffer>(static_cast<Buffer *>(it)));
		}
	}
	return handle;
}

bool RenderPass::init(Device &dev, const core::QueuePassData &data) {
	_type = data.type;
	_name = data.key.str<Interface>();

	_layouts.resize(data.pipelineLayouts.size());
	for (auto &it : data.pipelineLayouts) {
		if (!makeLayout(dev, *it, _layouts[it->index])) {
			return false;
		}
	}

	return core::RenderPass::init(dev,
			[](core::Device *, core::ObjectType, core::ObjectHandle, void *) { },
			core::ObjectType::RenderPass, core::ObjectHandle::zero(), nullptr);
}

const RenderPass::LayoutData *RenderPass::getLayout(uint32_t index) const {
	if (index < _layouts.size()) {
		return &_layouts[index];
	}
	return nullptr;
}

bool RenderPass::makeLayout(Device &dev, const core::PipelineLayoutData &data, LayoutData &out) {
	for (auto &set : data.sets) {
		Vector<WGPUBindGroupLayoutEntry> entries;
		entries.reserve(set->descriptors.size());

		for (auto &desc : set->descriptors) {
			WGPUBindGroupLayoutEntry entry = WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;
			entry.binding = desc->index;
			entry.visibility = getWGPUShaderStage(desc->stages);

			switch (desc->type) {
			case core::DescriptorType::Sampler:
				if (desc->sampler.compareEnable) {
					entry.sampler.type = WGPUSamplerBindingType_Comparison;
				} else if (desc->sampler.magFilter == core::Filter::Nearest
						&& desc->sampler.minFilter == core::Filter::Nearest) {
					entry.sampler.type = WGPUSamplerBindingType_NonFiltering;
				} else {
					entry.sampler.type = WGPUSamplerBindingType_Filtering;
				}
				break;
			case core::DescriptorType::SampledImage:
				entry.texture.sampleType = WGPUTextureSampleType_Float;
				entry.texture.viewDimension = WGPUTextureViewDimension_2D;
				break;
			case core::DescriptorType::StorageImage:
				entry.storageTexture.access = WGPUStorageTextureAccess_WriteOnly;
				entry.storageTexture.viewDimension = WGPUTextureViewDimension_2D;
				if (desc->attachment
						&& desc->attachment->attachment->type == core::AttachmentType::Image) {
					auto img = static_cast<core::ImageAttachment *>(
							desc->attachment->attachment->attachment.get());
					entry.storageTexture.format = getWGPUFormat(img->getImageInfo().format);
				}
				break;
			case core::DescriptorType::UniformBuffer:
			case core::DescriptorType::UniformBufferDynamic:
				entry.buffer.type = WGPUBufferBindingType_Uniform;
				entry.buffer.hasDynamicOffset =
						desc->type == core::DescriptorType::UniformBufferDynamic;
				break;
			case core::DescriptorType::StorageBuffer:
			case core::DescriptorType::StorageBufferDynamic:
				// read-only by default: vertex stage can not use read-write storage;
				// writable storage buffers (compute) can be requested via stages
				entry.buffer.type = hasFlag(desc->stages, core::ProgramStage::Compute)
						? WGPUBufferBindingType_Storage
						: WGPUBufferBindingType_ReadOnlyStorage;
				entry.buffer.hasDynamicOffset =
						desc->type == core::DescriptorType::StorageBufferDynamic;
				break;
			default:
				log::source().error("webgpu::RenderPass", _name,
						": unsupported descriptor type for WebGPU: ", toInt(desc->type),
						" (descriptor '", desc->key, "')");
				return false;
				break;
			}

			entries.emplace_back(entry);
		}

		WGPUBindGroupLayoutDescriptor layoutDesc = WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;
		layoutDesc.label = WGPUStringView{set->key.data(), set->key.size()};
		layoutDesc.entryCount = entries.size();
		layoutDesc.entries = entries.data();

		auto bindGroupLayout = wgpuDeviceCreateBindGroupLayout(dev.getDevice(), &layoutDesc);
		if (!bindGroupLayout) {
			log::source().error("webgpu::RenderPass", _name,
					": fail to create bind group layout for set: ", set->key);
			return false;
		}

		out.bindGroupLayouts.emplace_back(bindGroupLayout);
	}

	// texture set (material textures) is bound as the LAST bind group,
	// mirroring the Vulkan backend convention
	if (data.textureSetLayout && data.textureSetLayout->layout) {
		auto texLayout =
				data.textureSetLayout->layout.get_cast<TextureSetLayout>()->getLayout();
		wgpuBindGroupLayoutAddRef(texLayout);
		out.bindGroupLayouts.emplace_back(texLayout);
	}

	WGPUPipelineLayoutDescriptor pipelineLayoutDesc = WGPU_PIPELINE_LAYOUT_DESCRIPTOR_INIT;
	pipelineLayoutDesc.label = WGPUStringView{data.key.data(), data.key.size()};
	pipelineLayoutDesc.bindGroupLayoutCount = out.bindGroupLayouts.size();
	pipelineLayoutDesc.bindGroupLayouts = out.bindGroupLayouts.data();

	out.pipelineLayout = wgpuDeviceCreatePipelineLayout(dev.getDevice(), &pipelineLayoutDesc);
	if (!out.pipelineLayout) {
		log::source().error("webgpu::RenderPass", _name,
				": fail to create pipeline layout: ", data.key);
		return false;
	}

	return true;
}

CommandBuffer::~CommandBuffer() {
	if (_renderPass) {
		wgpuRenderPassEncoderRelease(_renderPass);
		_renderPass = nullptr;
	}
	if (_computePass) {
		wgpuComputePassEncoderRelease(_computePass);
		_computePass = nullptr;
	}
	if (_commands) {
		wgpuCommandBufferRelease(_commands);
		_commands = nullptr;
	}
	if (_encoder) {
		wgpuCommandEncoderRelease(_encoder);
		_encoder = nullptr;
	}
}

bool CommandBuffer::init(Device &dev) {
	_device = &dev;
	_encoder = wgpuDeviceCreateCommandEncoder(dev.getDevice(), nullptr);
	return _encoder != nullptr;
}

void CommandBuffer::beginRenderPass(SpanView<WGPURenderPassColorAttachment> colors,
		const WGPURenderPassDepthStencilAttachment *depthStencil, Extent2 renderExtent) {
	WGPURenderPassDescriptor passDesc = WGPU_RENDER_PASS_DESCRIPTOR_INIT;
	passDesc.colorAttachmentCount = colors.size();
	passDesc.colorAttachments = colors.data();
	passDesc.depthStencilAttachment = depthStencil;

	_renderPass = wgpuCommandEncoderBeginRenderPass(_encoder, &passDesc);
	_renderExtent = renderExtent;
	_withinRenderpass = true;
}

void CommandBuffer::endRenderPass() {
	if (_renderPass) {
		wgpuRenderPassEncoderEnd(_renderPass);
		wgpuRenderPassEncoderRelease(_renderPass);
		_renderPass = nullptr;
	}
	_withinRenderpass = false;
	++_currentSubpass;
}

void CommandBuffer::beginComputePass() {
	_computePass = wgpuCommandEncoderBeginComputePass(_encoder, nullptr);
}

void CommandBuffer::endComputePass() {
	if (_computePass) {
		wgpuComputePassEncoderEnd(_computePass);
		wgpuComputePassEncoderRelease(_computePass);
		_computePass = nullptr;
	}
	++_currentSubpass;
}

void CommandBuffer::cmdBindPipeline(const core::GraphicPipelineData *pipeline) {
	wgpuRenderPassEncoderSetPipeline(_renderPass,
			static_cast<GraphicPipeline *>(pipeline->pipeline.get())->getPipeline());
	if (pipeline->layout) {
		bindLayoutGroups(pipeline->layout->index);
	}
}

void CommandBuffer::cmdBindPipeline(const core::ComputePipelineData *pipeline) {
	wgpuComputePassEncoderSetPipeline(_computePass,
			static_cast<ComputePipeline *>(pipeline->pipeline.get())->getPipeline());
	if (pipeline->layout) {
		bindLayoutGroups(pipeline->layout->index);
	}
}

void CommandBuffer::cmdBindTextureSet(const core::PipelineLayoutData *layout,
		NotNull<core::TextureSet> set) {
	auto group = static_cast<TextureSet *>(set.get())->getBindGroup();
	if (!group) {
		log::source().error("webgpu::CommandBuffer", "cmdBindTextureSet: empty texture set");
		return;
	}

	auto index = uint32_t(layout->sets.size());
	if (_renderPass) {
		wgpuRenderPassEncoderSetBindGroup(_renderPass, index, group, 0, nullptr);
	} else if (_computePass) {
		wgpuComputePassEncoderSetBindGroup(_computePass, index, group, 0, nullptr);
	}
}

void CommandBuffer::cmdBindMaterialGroup(const core::PipelineLayoutData *layout,
		WGPUBindGroup group) {
	const uint32_t index = uint32_t(layout->sets.size());
	if (_renderPass) {
		wgpuRenderPassEncoderSetBindGroup(_renderPass, index, group, 0, nullptr);
	} else if (_computePass) {
		wgpuComputePassEncoderSetBindGroup(_computePass, index, group, 0, nullptr);
	}
}

void CommandBuffer::cmdDraw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex,
		uint32_t firstInstance) {
	wgpuRenderPassEncoderDraw(_renderPass, vertexCount, instanceCount, firstVertex, firstInstance);
}

void CommandBuffer::cmdSetScissor(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
	if (_renderPass) {
		wgpuRenderPassEncoderSetScissorRect(_renderPass, x, y, width, height);
	}
}

void CommandBuffer::cmdBindIndexBuffer(NotNull<Buffer> buffer, WGPUIndexFormat format) {
	wgpuRenderPassEncoderSetIndexBuffer(_renderPass, buffer->getBuffer(), format, 0,
			buffer->getSize());
}

void CommandBuffer::cmdDrawIndexed(uint32_t indexCount, uint32_t instanceCount,
		uint32_t firstIndex, int32_t baseVertex, uint32_t firstInstance) {
	wgpuRenderPassEncoderDrawIndexed(_renderPass, indexCount, instanceCount, firstIndex,
			baseVertex, firstInstance);
}

void CommandBuffer::cmdDispatch(uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ) {
	wgpuComputePassEncoderDispatchWorkgroups(_computePass, groupsX, groupsY, groupsZ);
}

void CommandBuffer::bindLayoutGroups(uint32_t layoutIndex) {
	if (!_bindGroups || layoutIndex >= _bindGroups->size()) {
		return;
	}

	auto &groups = (*_bindGroups)[layoutIndex];
	for (uint32_t i = 0; i < groups.size(); ++i) {
		if (_renderPass) {
			wgpuRenderPassEncoderSetBindGroup(_renderPass, i, groups[i], 0, nullptr);
		} else if (_computePass) {
			wgpuComputePassEncoderSetBindGroup(_computePass, i, groups[i], 0, nullptr);
		}
	}
}

WGPUCommandBuffer CommandBuffer::finish() {
	if (!_commands) {
		_commands = wgpuCommandEncoderFinish(_encoder, nullptr);
	}
	return _commands;
}

QueuePassHandle::~QueuePassHandle() { clearBindGroups(); }

bool QueuePassHandle::prepare(core::FrameQueue &q, Function<void(bool)> &&cb) {
	_loop = q.getLoop();
	_device = static_cast<Device *>(static_cast<Loop *>(_loop)->getDevice());

	prepareSubpasses(q);
	return true;
}

void QueuePassHandle::clearBindGroups() {
	for (auto &layout : _bindGroups) {
		for (auto &it : layout) {
			if (it) {
				wgpuBindGroupRelease(it);
			}
		}
	}
	_bindGroups.clear();
}

// resolve descriptors against frame attachments into per-layout bind groups
bool QueuePassHandle::buildBindGroups(core::FrameQueue &q) {
	auto passImpl = _data->impl.get_cast<RenderPass>();
	if (!passImpl) {
		return false;
	}

	clearBindGroups();
	_bindGroups.resize(_data->pipelineLayouts.size());

	for (auto &layoutData : _data->pipelineLayouts) {
		auto layoutImpl = passImpl->getLayout(layoutData->index);
		if (!layoutImpl) {
			return false;
		}

		auto &groups = _bindGroups[layoutData->index];

		for (auto &set : layoutData->sets) {
			Vector<WGPUBindGroupEntry> entries;
			entries.reserve(set->descriptors.size());

			Map<const core::AttachmentData *, uint32_t> bufferOrdinals;

			for (auto &desc : set->descriptors) {
				WGPUBindGroupEntry entry = WGPU_BIND_GROUP_ENTRY_INIT;
				entry.binding = desc->index;

				switch (desc->type) {
				case core::DescriptorType::SampledImage:
				case core::DescriptorType::StorageImage: {
					auto aIt = _queueData->attachmentMap.find(desc->attachment->attachment);
					if (aIt == _queueData->attachmentMap.end() || !aIt->second->image) {
						log::source().error("webgpu::QueuePassHandle",
								"No image for descriptor: ", desc->key);
						return false;
					}

					auto imgAttachment = static_cast<core::ImageAttachment *>(
							desc->attachment->attachment->attachment.get());
					auto viewInfo = imgAttachment->getImageViewInfo(aIt->second->image->getInfo(),
							*desc->attachment);
					auto view = aIt->second->image->getView(viewInfo);
					if (!view) {
						log::source().error("webgpu::QueuePassHandle",
								"No image view for descriptor: ", desc->key);
						return false;
					}
					entry.textureView = view.get_cast<ImageView>()->getTextureView();
					break;
				}
				case core::DescriptorType::UniformBuffer:
				case core::DescriptorType::UniformBufferDynamic:
				case core::DescriptorType::StorageBuffer:
				case core::DescriptorType::StorageBufferDynamic: {
					auto aIt = _queueData->attachmentMap.find(desc->attachment->attachment);
					if (aIt == _queueData->attachmentMap.end()) {
						log::source().error("webgpu::QueuePassHandle",
								"No attachment for buffer descriptor: ", desc->key);
						return false;
					}

					auto bufHandle =
							dynamic_cast<BufferAttachmentHandle *>(aIt->second->handle.get());
					if (!bufHandle || bufHandle->getBuffers().empty()) {
						log::source().error("webgpu::QueuePassHandle",
								"No buffers in attachment for descriptor: ", desc->key);
						return false;
					}

					// multiple buffer descriptors of one attachment are resolved
					// to its buffer views in order
					auto &ordinal = bufferOrdinals
											.emplace(desc->attachment->attachment, 0)
											.first->second;
					if (ordinal >= bufHandle->getBuffers().size()) {
						log::source().error("webgpu::QueuePassHandle",
								"Not enough buffer views in attachment for descriptor: ",
								desc->key);
						return false;
					}

					auto &view = bufHandle->getBuffers()[ordinal++];
					entry.buffer = view.buffer->getBuffer();
					entry.offset = view.offset;
					entry.size = view.size ? view.size : view.buffer->getSize() - view.offset;
					break;
				}
				case core::DescriptorType::Sampler: {
					auto sampler = _device->getSampler(desc->sampler);
					if (!sampler) {
						log::source().error("webgpu::QueuePassHandle",
								"Fail to acquire sampler for descriptor: ", desc->key);
						return false;
					}
					entry.sampler = sampler.get_cast<Sampler>()->getSampler();
					break;
				}
				default:
					log::source().error("webgpu::QueuePassHandle",
							"Unsupported descriptor type for bind group: ", toInt(desc->type),
							" (descriptor '", desc->key, "')");
					return false;
					break;
				}

				entries.emplace_back(entry);
			}

			WGPUBindGroupDescriptor groupDesc = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
			groupDesc.label = WGPUStringView{set->key.data(), set->key.size()};
			groupDesc.layout = layoutImpl->bindGroupLayouts[set->index];
			groupDesc.entryCount = entries.size();
			groupDesc.entries = entries.data();

			auto group = wgpuDeviceCreateBindGroup(_device->getDevice(), &groupDesc);
			if (!group) {
				log::source().error("webgpu::QueuePassHandle",
						"Fail to create bind group for set: ", set->key);
				return false;
			}

			groups.emplace_back(group);
		}
	}

	return true;
}

void QueuePassHandle::recordSubpass(core::FrameQueue &q, const core::SubpassData &subpass,
		CommandBuffer &buf) {
	if (subpass.commandsCallback) {
		subpass.commandsCallback(q, subpass, buf);
	}
}

Rc<CommandBuffer> QueuePassHandle::recordCommands(core::FrameQueue &q) {
	auto buf = Rc<CommandBuffer>::create(*_device);
	if (!buf) {
		return nullptr;
	}

	if (!buildBindGroups(q)) {
		return nullptr;
	}

	buf->setFrameBindGroups(&_bindGroups);

	if (_data->type == core::PassType::Compute) {
		for (auto &subpass : _data->subpasses) {
			buf->beginComputePass();

			recordSubpass(q, *subpass, *buf);

			buf->endComputePass();
		}

		buf->finish();
		return buf;
	}

	for (auto &subpass : _data->subpasses) {
		Vector<WGPURenderPassColorAttachment> colors;
		WGPURenderPassDepthStencilAttachment depthStencil =
				WGPU_RENDER_PASS_DEPTH_STENCIL_ATTACHMENT_INIT;
		bool hasDepthStencil = false;

		auto getViewForAttachment =
				[&](const core::AttachmentSubpassData *desc) -> Rc<core::ImageView> {
			auto aIt = _queueData->attachmentMap.find(desc->pass->attachment);
			if (aIt == _queueData->attachmentMap.end() || !aIt->second->image) {
				return nullptr;
			}

			auto imgAttachment = static_cast<core::ImageAttachment *>(
					desc->pass->attachment->attachment.get());
			auto viewInfo =
					imgAttachment->getImageViewInfo(aIt->second->image->getInfo(), *desc->pass);
			return aIt->second->image->getView(viewInfo);
		};

		Extent2 renderExtent(0, 0);

		for (auto &out : subpass->outputImages) {
			auto view = getViewForAttachment(out);
			if (!view) {
				log::source().error("webgpu::QueuePassHandle",
						"No image view for attachment: ", out->key);
				return nullptr;
			}
			auto &imgExtent = view->getImage()->getInfo().extent;
			renderExtent = Extent2(imgExtent.width, imgExtent.height);

			auto imgAttachment = static_cast<core::ImageAttachment *>(
					out->pass->attachment->attachment.get());
			auto clearColor = imgAttachment->getClearColor();

			WGPURenderPassColorAttachment color = WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;
			color.view = view.get_cast<ImageView>()->getTextureView();
			color.loadOp = getWGPULoadOp(out->pass->loadOp);
			color.storeOp = getWGPUStoreOp(out->pass->storeOp);
			color.clearValue = WGPUColor{clearColor.r, clearColor.g, clearColor.b, clearColor.a};

			colors.emplace_back(color);
		}

		if (subpass->depthStencil) {
			auto view = getViewForAttachment(subpass->depthStencil);
			if (view) {
				auto imgAttachment = static_cast<core::ImageAttachment *>(
						subpass->depthStencil->pass->attachment->attachment.get());
				depthStencil.view = view.get_cast<ImageView>()->getTextureView();
				depthStencil.depthLoadOp = getWGPULoadOp(subpass->depthStencil->pass->loadOp);
				depthStencil.depthStoreOp = getWGPUStoreOp(subpass->depthStencil->pass->storeOp);
				depthStencil.depthClearValue = imgAttachment->getClearColor().r;
				hasDepthStencil = true;
			}
		}

		buf->beginRenderPass(colors, hasDepthStencil ? &depthStencil : nullptr, renderExtent);

		recordSubpass(q, *subpass, *buf);

		buf->endRenderPass();
	}

	buf->finish();
	return buf;
}

void QueuePassHandle::submit(core::FrameQueue &q, Rc<core::FrameSync> &&sync,
		Function<void(bool)> &&onSubmited, Function<void(bool)> &&onComplete) {
	auto buf = recordCommands(q);
	if (!buf) {
		onSubmited(false);
		return;
	}

	_fence = _loop->acquireFence(core::FenceType::Default);
	if (!_fence) {
		onSubmited(false);
		return;
	}

	_fence->setTag(getName());
	_fence->addRelease(
			[this, guard = Rc<core::FrameQueue>(&q), onComplete = sp::move(onComplete)](
					bool success) {
		for (auto &it : _data->completeCallbacks) { it(*guard, *_data, success); }
		onComplete(success);
	}, this, "webgpu::QueuePassHandle::submit");

	// keep recorded commands alive until GPU is done
	_fence->autorelease(buf);

	auto commands = buf->finish();
	wgpuQueueSubmit(_device->getQueue(), 1, &commands);

	static_cast<Fence *>(_fence.get())->arm(_device->getQueue());

	for (auto &it : _data->submittedCallbacks) { it(q, *_data, true); }

	onSubmited(true);

	auto fence = move(_fence);
	_fence = nullptr;
	fence->schedule(*_loop);
}

} // namespace stappler::xenolith::webgpu
