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

#include "XLMtlQueuePass.h"
#include "XLMtlPipeline.h"
#include "XLMtlTextureSet.h"
#include "XLMtlLoop.h"
#include "XLCoreFrameQueue.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::mtl {

static MTLLoadAction getMTLLoadAction(core::AttachmentLoadOp op) {
	switch (op) {
	case core::AttachmentLoadOp::Load: return MTLLoadActionLoad; break;
	case core::AttachmentLoadOp::Clear: return MTLLoadActionClear; break;
	case core::AttachmentLoadOp::DontCare: return MTLLoadActionDontCare; break;
	}
	return MTLLoadActionDontCare;
}

static MTLStoreAction getMTLStoreAction(core::AttachmentStoreOp op) {
	switch (op) {
	case core::AttachmentStoreOp::Store: return MTLStoreActionStore; break;
	case core::AttachmentStoreOp::DontCare: return MTLStoreActionDontCare; break;
	}
	return MTLStoreActionDontCare;
}

CommandBuffer::~CommandBuffer() {
	releaseHandle(_computeEncoder);
	_computeEncoder = nullptr;
	releaseHandle(_renderEncoder);
	_renderEncoder = nullptr;
	releaseHandle(_buffer);
	_buffer = nullptr;
}

bool CommandBuffer::init(Device &dev) {
	_device = &dev;

	@autoreleasepool {
		id<MTLCommandBuffer> buffer = [dev.getQueue() commandBuffer];
		if (!buffer) {
			log::source().error("mtl::CommandBuffer", "Fail to create command buffer");
			return false;
		}
		_buffer = retainHandle(buffer);
	}
	return true;
}

bool CommandBuffer::beginRenderPass(MTLRenderPassDescriptor *desc, Extent2 renderExtent) {
	@autoreleasepool {
		id<MTLRenderCommandEncoder> encoder =
				[getBuffer() renderCommandEncoderWithDescriptor:desc];
		if (!encoder) {
			log::source().error("mtl::CommandBuffer", "Fail to create render command encoder");
			return false;
		}
		_renderEncoder = retainHandle(encoder);
		_renderExtent = renderExtent;
	}
	return true;
}

void CommandBuffer::endRenderPass() {
	if (_renderEncoder) {
		[getRenderEncoder() endEncoding];
		releaseHandle(_renderEncoder);
		_renderEncoder = nullptr;
		_renderExtent = Extent2(0, 0);
	}
}

void CommandBuffer::beginComputePass() {
	@autoreleasepool {
		id<MTLComputeCommandEncoder> encoder = [getBuffer() computeCommandEncoder];
		_computeEncoder = retainHandle(encoder);
	}
}

void CommandBuffer::endComputePass() {
	if (_computeEncoder) {
		[getComputeEncoder() endEncoding];
		releaseHandle(_computeEncoder);
		_computeEncoder = nullptr;
	}
}

void CommandBuffer::cmdBindPipeline(const core::GraphicPipelineData *data) {
	auto pipeline = static_cast<GraphicPipeline *>(data->pipeline.get());
	[getRenderEncoder() setRenderPipelineState:pipeline->getPipeline()];
	if (auto ds = pipeline->getDepthStencil()) {
		[getRenderEncoder() setDepthStencilState:ds];
	}
	if (data->layout) {
		applyBindings(data->layout->index);
	}
}

void CommandBuffer::cmdBindPipeline(const core::ComputePipelineData *data) {
	auto pipeline = static_cast<ComputePipeline *>(data->pipeline.get());
	[getComputeEncoder() setComputePipelineState:pipeline->getPipeline()];
	if (data->layout) {
		applyBindings(data->layout->index);
	}
}

void CommandBuffer::applyBindings(uint32_t layoutIndex) {
	if (!_bindings || layoutIndex >= _bindings->size()) {
		return;
	}

	auto render = getRenderEncoder();
	auto compute = getComputeEncoder();

	NSUInteger bufferIndex = 0;
	NSUInteger textureIndex = 0;
	NSUInteger samplerIndex = 0;

	for (auto &entry : (*_bindings)[layoutIndex]) {
		switch (entry.type) {
		case core::DescriptorType::UniformBuffer:
		case core::DescriptorType::UniformBufferDynamic:
		case core::DescriptorType::StorageBuffer:
		case core::DescriptorType::StorageBufferDynamic: {
			auto buf = static_cast<Buffer *>(entry.object.get())->getBuffer();
			if (render) {
				[render setVertexBuffer:buf offset:entry.offset atIndex:bufferIndex];
				[render setFragmentBuffer:buf offset:entry.offset atIndex:bufferIndex];
			} else if (compute) {
				[compute setBuffer:buf offset:entry.offset atIndex:bufferIndex];
			}
			++bufferIndex;
			break;
		}
		case core::DescriptorType::SampledImage:
		case core::DescriptorType::StorageImage: {
			auto tex = static_cast<ImageView *>(entry.object.get())->getTextureView();
			if (render) {
				[render setFragmentTexture:tex atIndex:textureIndex];
			} else if (compute) {
				[compute setTexture:tex atIndex:textureIndex];
			}
			++textureIndex;
			break;
		}
		case core::DescriptorType::Sampler: {
			auto sampler = static_cast<Sampler *>(entry.object.get())->getSampler();
			if (render) {
				[render setFragmentSamplerState:sampler atIndex:samplerIndex];
			} else if (compute) {
				[compute setSamplerState:sampler atIndex:samplerIndex];
			}
			++samplerIndex;
			break;
		}
		default: break;
		}
	}
}

void CommandBuffer::cmdBindTextureSet(const core::PipelineLayoutData *,
		NotNull<core::TextureSet> set) {
	auto texSet = static_cast<TextureSet *>(set.get());
	auto argBuf = texSet->getArgumentBuffer();

	auto render = getRenderEncoder();
	auto compute = getComputeEncoder();

	// argument buffer handles bypass driver residency tracking: the bound
	// textures are made resident explicitly for this encoder
	for (auto &view : texSet->getBoundViews()) {
		if (!view) {
			continue;
		}
		auto tex = view.get_cast<ImageView>()->getTextureView();
		if (render) {
			[render useResource:tex usage:MTLResourceUsageRead stages:MTLRenderStageFragment];
		} else if (compute) {
			[compute useResource:tex usage:MTLResourceUsageRead];
		}
	}

	if (render) {
		[render setFragmentBuffer:argBuf offset:0 atIndex:TextureSetBufferIndex];
	} else if (compute) {
		[compute setBuffer:argBuf offset:0 atIndex:TextureSetBufferIndex];
	}
}

void CommandBuffer::cmdDraw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex,
		uint32_t firstInstance) {
	[getRenderEncoder() drawPrimitives:MTLPrimitiveTypeTriangle
						   vertexStart:firstVertex
						   vertexCount:vertexCount
						 instanceCount:instanceCount
						  baseInstance:firstInstance];
}

void CommandBuffer::cmdSetScissor(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
	// Metal validates that the scissor rect lies within the render target
	MTLScissorRect rect;
	rect.x = sprt::min(x, _renderExtent.width);
	rect.y = sprt::min(y, _renderExtent.height);
	rect.width = sprt::min(width, _renderExtent.width - uint32_t(rect.x));
	rect.height = sprt::min(height, _renderExtent.height - uint32_t(rect.y));
	[getRenderEncoder() setScissorRect:rect];
}

void CommandBuffer::cmdDrawIndexed(NotNull<Buffer> indexBuffer, uint32_t indexCount,
		uint32_t instanceCount, uint32_t firstIndex, int32_t baseVertex, uint32_t firstInstance) {
	[getRenderEncoder() drawIndexedPrimitives:MTLPrimitiveTypeTriangle
								   indexCount:indexCount
									indexType:MTLIndexTypeUInt32
								  indexBuffer:indexBuffer->getBuffer()
							indexBufferOffset:firstIndex * sizeof(uint32_t)
								instanceCount:instanceCount
								   baseVertex:baseVertex
								 baseInstance:firstInstance];
}

void CommandBuffer::cmdDispatch(uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ,
		uint32_t threadsX, uint32_t threadsY, uint32_t threadsZ) {
	[getComputeEncoder() dispatchThreadgroups:MTLSizeMake(groupsX, groupsY, groupsZ)
						threadsPerThreadgroup:MTLSizeMake(threadsX, threadsY, threadsZ)];
}

id<MTLCommandBuffer> CommandBuffer::finish() {
	endRenderPass();
	endComputePass();
	return getBuffer();
}

QueuePassHandle::~QueuePassHandle() { }

bool QueuePassHandle::prepare(core::FrameQueue &q, Function<void(bool)> &&cb) {
	_loop = q.getLoop();
	_device = static_cast<Device *>(static_cast<Loop *>(_loop)->getDevice());

	prepareSubpasses(q);
	return true;
}

void QueuePassHandle::recordSubpass(core::FrameQueue &q, const core::SubpassData &subpass,
		CommandBuffer &buf) {
	if (subpass.commandsCallback) {
		subpass.commandsCallback(q, subpass, buf);
	}
}

// resolve descriptors against frame attachments; entries follow declaration
// order, matching the argument table index convention in XLMtl.h
bool QueuePassHandle::buildBindings(core::FrameQueue &q) {
	_bindings.clear();
	_bindings.resize(_data->pipelineLayouts.size());

	for (auto &layoutData : _data->pipelineLayouts) {
		auto &entries = _bindings[layoutData->index];

		for (auto &set : layoutData->sets) {
			Map<const core::AttachmentData *, uint32_t> bufferOrdinals;

			for (auto &desc : set->descriptors) {
				CommandBuffer::BindingEntry entry;
				entry.type = desc->type;

				switch (desc->type) {
				case core::DescriptorType::SampledImage:
				case core::DescriptorType::StorageImage: {
					auto aIt = _queueData->attachmentMap.find(desc->attachment->attachment);
					if (aIt == _queueData->attachmentMap.end() || !aIt->second->image) {
						log::source().error("mtl::QueuePassHandle",
								"No image for descriptor: ", desc->key);
						return false;
					}

					auto imgAttachment = static_cast<core::ImageAttachment *>(
							desc->attachment->attachment->attachment.get());
					auto viewInfo = imgAttachment->getImageViewInfo(aIt->second->image->getInfo(),
							*desc->attachment);
					auto view = aIt->second->image->getView(viewInfo);
					if (!view) {
						log::source().error("mtl::QueuePassHandle",
								"No image view for descriptor: ", desc->key);
						return false;
					}
					entry.object = view;
					break;
				}
				case core::DescriptorType::UniformBuffer:
				case core::DescriptorType::UniformBufferDynamic:
				case core::DescriptorType::StorageBuffer:
				case core::DescriptorType::StorageBufferDynamic: {
					auto aIt = _queueData->attachmentMap.find(desc->attachment->attachment);
					if (aIt == _queueData->attachmentMap.end()) {
						log::source().error("mtl::QueuePassHandle",
								"No attachment for buffer descriptor: ", desc->key);
						return false;
					}

					auto bufHandle =
							dynamic_cast<BufferAttachmentHandle *>(aIt->second->handle.get());
					if (!bufHandle || bufHandle->getBuffers().empty()) {
						log::source().error("mtl::QueuePassHandle",
								"No buffers in attachment for descriptor: ", desc->key);
						return false;
					}

					// multiple buffer descriptors of one attachment are resolved
					// to its buffer views in order
					auto &ordinal = bufferOrdinals.emplace(desc->attachment->attachment, 0)
											.first->second;
					if (ordinal >= bufHandle->getBuffers().size()) {
						log::source().error("mtl::QueuePassHandle",
								"Not enough buffer views in attachment for descriptor: ",
								desc->key);
						return false;
					}

					auto &view = bufHandle->getBuffers()[ordinal++];
					entry.object = view.buffer;
					entry.offset = view.offset;
					break;
				}
				case core::DescriptorType::Sampler: {
					auto sampler = _device->getSampler(desc->sampler);
					if (!sampler) {
						log::source().error("mtl::QueuePassHandle",
								"Fail to acquire sampler for descriptor: ", desc->key);
						return false;
					}
					entry.object = sampler;
					break;
				}
				default:
					log::source().error("mtl::QueuePassHandle",
							"Unsupported descriptor type: ", toInt(desc->type), " (descriptor '",
							desc->key, "')");
					return false;
					break;
				}

				entries.emplace_back(sp::move(entry));
			}
		}
	}

	return true;
}

Rc<CommandBuffer> QueuePassHandle::recordCommands(core::FrameQueue &q) {
	auto buf = Rc<CommandBuffer>::create(*_device);
	if (!buf) {
		return nullptr;
	}

	if (!buildBindings(q)) {
		return nullptr;
	}

	buf->setFrameBindings(&_bindings);

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

		@autoreleasepool {
			MTLRenderPassDescriptor *desc = [MTLRenderPassDescriptor renderPassDescriptor];

			NSUInteger colorIndex = 0;
			for (auto &out : subpass->outputImages) {
				auto view = getViewForAttachment(out);
				if (!view) {
					log::source().error("mtl::QueuePassHandle",
							"No image view for attachment: ", out->key);
					return nullptr;
				}
				auto &imgExtent = view->getImage()->getInfo().extent;
				renderExtent = Extent2(imgExtent.width, imgExtent.height);

				auto imgAttachment = static_cast<core::ImageAttachment *>(
						out->pass->attachment->attachment.get());
				auto clearColor = imgAttachment->getClearColor();

				MTLRenderPassColorAttachmentDescriptor *color =
						desc.colorAttachments[colorIndex];
				color.texture = view.get_cast<ImageView>()->getTextureView();
				color.loadAction = getMTLLoadAction(out->pass->loadOp);
				color.storeAction = getMTLStoreAction(out->pass->storeOp);
				color.clearColor =
						MTLClearColorMake(clearColor.r, clearColor.g, clearColor.b, clearColor.a);

				++colorIndex;
			}

			if (subpass->depthStencil) {
				auto view = getViewForAttachment(subpass->depthStencil);
				if (view) {
					auto imgAttachment = static_cast<core::ImageAttachment *>(
							subpass->depthStencil->pass->attachment->attachment.get());
					desc.depthAttachment.texture = view.get_cast<ImageView>()->getTextureView();
					desc.depthAttachment.loadAction =
							getMTLLoadAction(subpass->depthStencil->pass->loadOp);
					desc.depthAttachment.storeAction =
							getMTLStoreAction(subpass->depthStencil->pass->storeOp);
					desc.depthAttachment.clearDepth = imgAttachment->getClearColor().r;
				}
			}

			if (!buf->beginRenderPass(desc, renderExtent)) {
				return nullptr;
			}
		}

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
	}, this, "mtl::QueuePassHandle::submit");

	// keep the recorded command buffer alive until the GPU is done
	_fence->autorelease(buf);

	auto commands = buf->finish();

	// completed handlers must be registered before commit
	static_cast<Fence *>(_fence.get())->arm(commands);

	[commands commit];

	for (auto &it : _data->submittedCallbacks) { it(q, *_data, true); }

	onSubmited(true);

	auto fence = move(_fence);
	_fence = nullptr;
	fence->schedule(*_loop);
}

Rc<core::AttachmentHandle> BufferAttachment::makeFrameHandle(const core::FrameQueue &queue) {
	if (_frameHandleCallback) {
		return _frameHandleCallback(*this, queue);
	}

	auto handle = Rc<BufferAttachmentHandle>::create(*this, queue);
	if (handle && isStatic()) {
		for (auto &it : getStaticBuffers()) {
			handle->addBufferView(Rc<Buffer>(static_cast<Buffer *>(it)));
		}
	}
	return handle;
}

void BufferAttachmentHandle::addBufferView(Rc<Buffer> &&buffer, uint64_t offset, uint64_t size) {
	_buffers.emplace_back(BufferView{sp::move(buffer), offset, size});
}

bool RenderPass::init(Device &dev, const core::QueuePassData &data) {
	_type = data.type;
	_name = data.key.str<Interface>();
	return core::RenderPass::init(dev,
			[](core::Device *, core::ObjectType, core::ObjectHandle, void *) { },
			core::ObjectType::RenderPass, core::ObjectHandle::zero(), nullptr);
}

} // namespace stappler::xenolith::mtl
