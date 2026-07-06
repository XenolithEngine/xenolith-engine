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

#include "XLWgpuObject.h"
#include "XLWgpuPipeline.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::webgpu {

WGPUTextureUsage getWGPUTextureUsage(core::ImageUsage usage) {
	WGPUTextureUsage ret = WGPUTextureUsage_None;
	if (hasFlag(usage, core::ImageUsage::TransferSrc)) {
		ret |= WGPUTextureUsage_CopySrc;
	}
	if (hasFlag(usage, core::ImageUsage::TransferDst)) {
		ret |= WGPUTextureUsage_CopyDst;
	}
	if (hasFlag(usage, core::ImageUsage::Sampled)) {
		ret |= WGPUTextureUsage_TextureBinding;
	}
	if (hasFlag(usage, core::ImageUsage::Storage)) {
		ret |= WGPUTextureUsage_StorageBinding;
	}
	if (hasFlag(usage, core::ImageUsage::ColorAttachment)
			|| hasFlag(usage, core::ImageUsage::DepthStencilAttachment)
			|| hasFlag(usage, core::ImageUsage::InputAttachment)) {
		ret |= WGPUTextureUsage_RenderAttachment;
	}
	return ret;
}

WGPUBufferUsage getWGPUBufferUsage(core::BufferUsage usage) {
	WGPUBufferUsage ret = WGPUBufferUsage_None;
	if (hasFlag(usage, core::BufferUsage::TransferSrc)) {
		ret |= WGPUBufferUsage_CopySrc;
	}
	if (hasFlag(usage, core::BufferUsage::TransferDst)) {
		ret |= WGPUBufferUsage_CopyDst;
	}
	if (hasFlag(usage, core::BufferUsage::UniformBuffer)) {
		ret |= WGPUBufferUsage_Uniform;
	}
	if (hasFlag(usage, core::BufferUsage::StorageBuffer)) {
		ret |= WGPUBufferUsage_Storage;
	}
	if (hasFlag(usage, core::BufferUsage::IndexBuffer)) {
		ret |= WGPUBufferUsage_Index;
	}
	if (hasFlag(usage, core::BufferUsage::VertexBuffer)) {
		ret |= WGPUBufferUsage_Vertex;
	}
	if (hasFlag(usage, core::BufferUsage::IndirectBuffer)) {
		ret |= WGPUBufferUsage_Indirect;
	}
	return ret;
}

bool Buffer::setup(Device &dev, const core::BufferInfo &info,
		const Callback<size_t(uint8_t *, uint64_t)> *fill) {
	_info = info;

	WGPUBufferDescriptor bufDesc = WGPU_BUFFER_DESCRIPTOR_INIT;
	bufDesc.label = WGPUStringView{info.key.data(), info.key.size()};
	bufDesc.usage = getWGPUBufferUsage(info.usage);
	bufDesc.size = info.size;
	bufDesc.mappedAtCreation = fill != nullptr;

	_buffer = wgpuDeviceCreateBuffer(dev.getDevice(), &bufDesc);
	if (!_buffer) {
		log::source().error("webgpu::Buffer", "Fail to create buffer: ", info.key);
		return false;
	}

	if (fill) {
		auto mem = reinterpret_cast<uint8_t *>(wgpuBufferGetMappedRange(_buffer, 0, info.size));
		if (!mem) {
			log::source().error("webgpu::Buffer", "Fail to map buffer: ", info.key);
			wgpuBufferRelease(_buffer);
			_buffer = nullptr;
			return false;
		}
		(*fill)(mem, info.size);
		wgpuBufferUnmap(_buffer);
	}

	return core::BufferObject::init(dev,
			[](core::Device *, core::ObjectType, core::ObjectHandle ptr, void *) {
		wgpuBufferRelease((WGPUBuffer)ptr.get());
	}, core::ObjectType::Buffer, core::ObjectHandle(_buffer));
}

bool Buffer::init(Device &dev, const core::BufferInfo &info, BytesView initialData) {
	if (initialData.empty()) {
		return setup(dev, info, nullptr);
	}

	auto cb = Callback<size_t(uint8_t *, uint64_t)>([&](uint8_t *mem, uint64_t size) -> size_t {
		auto bytes = sprt::min(uint64_t(initialData.size()), size);
		sprt::memcpy(mem, initialData.data(), bytes);
		return size_t(bytes);
	});
	return setup(dev, info, &cb);
}

bool Buffer::init(Device &dev, const core::BufferData &data) {
	if (data.data.empty() && !data.memCallback && !data.stdCallback) {
		return setup(dev, data, nullptr);
	}

	auto cb = Callback<size_t(uint8_t *, uint64_t)>([&](uint8_t *mem, uint64_t size) -> size_t {
		return data.writeData(mem, size_t(size));
	});
	return setup(dev, data, &cb);
}

bool Sampler::init(Device &dev, const core::SamplerInfo &info) {
	_info = info;

	auto getFilter = [](core::Filter f) {
		return f == core::Filter::Linear ? WGPUFilterMode_Linear : WGPUFilterMode_Nearest;
	};

	auto getAddressMode = [](core::SamplerAddressMode mode) {
		switch (mode) {
		case core::SamplerAddressMode::Repeat: return WGPUAddressMode_Repeat; break;
		case core::SamplerAddressMode::MirroredRepeat: return WGPUAddressMode_MirrorRepeat; break;
		case core::SamplerAddressMode::ClampToEdge: return WGPUAddressMode_ClampToEdge; break;
		case core::SamplerAddressMode::ClampToBorder:
			// no border clamp in WebGPU
			return WGPUAddressMode_ClampToEdge;
			break;
		}
		return WGPUAddressMode_Repeat;
	};

	WGPUSamplerDescriptor samplerDesc = WGPU_SAMPLER_DESCRIPTOR_INIT;
	samplerDesc.magFilter = getFilter(info.magFilter);
	samplerDesc.minFilter = getFilter(info.minFilter);
	samplerDesc.mipmapFilter = info.mipmapMode == core::SamplerMipmapMode::Linear
			? WGPUMipmapFilterMode_Linear
			: WGPUMipmapFilterMode_Nearest;
	samplerDesc.addressModeU = getAddressMode(info.addressModeU);
	samplerDesc.addressModeV = getAddressMode(info.addressModeV);
	samplerDesc.addressModeW = getAddressMode(info.addressModeW);
	samplerDesc.lodMinClamp = info.minLod;
	samplerDesc.lodMaxClamp = sprt::max(info.minLod, info.maxLod);
	samplerDesc.maxAnisotropy =
			info.anisotropyEnable ? uint16_t(sprt::max(1.0f, info.maxAnisotropy)) : 1;
	if (info.compareEnable) {
		samplerDesc.compare = getWGPUCompareFunction(info.compareOp);
	}

	_sampler = wgpuDeviceCreateSampler(dev.getDevice(), &samplerDesc);
	if (!_sampler) {
		log::source().error("webgpu::Sampler", "Fail to create sampler");
		return false;
	}

	return core::Object::init(dev,
			[](core::Device *, core::ObjectType, core::ObjectHandle ptr, void *) {
		wgpuSamplerRelease((WGPUSampler)ptr.get());
	}, core::ObjectType::Sampler, core::ObjectHandle(_sampler));
}

bool Image::init(Device &dev, StringView name, const core::ImageInfoData &info) {
	_info = info;
	_name = name.str<Interface>();

	WGPUTextureDescriptor texDesc = WGPU_TEXTURE_DESCRIPTOR_INIT;
	texDesc.label = WGPUStringView{_name.data(), _name.size()};
	texDesc.usage = getWGPUTextureUsage(info.usage);
	texDesc.dimension =
			info.imageType == core::ImageType::Image3D ? WGPUTextureDimension_3D
			: info.imageType == core::ImageType::Image1D
			? WGPUTextureDimension_1D
			: WGPUTextureDimension_2D;
	texDesc.size = WGPUExtent3D{info.extent.width, info.extent.height,
		sprt::max(info.extent.depth, info.arrayLayers.get())};
	texDesc.format = getWGPUFormat(info.format);
	texDesc.mipLevelCount = info.mipLevels.get();
	texDesc.sampleCount = uint32_t(toInt(info.samples));

	_texture = wgpuDeviceCreateTexture(dev.getDevice(), &texDesc);
	if (!_texture) {
		log::source().error("webgpu::Image", "Fail to create texture: ", _name);
		return false;
	}

	return core::ImageObject::init(dev,
			[](core::Device *, core::ObjectType, core::ObjectHandle ptr, void *) {
		wgpuTextureRelease((WGPUTexture)ptr.get());
	}, core::ObjectType::Image, core::ObjectHandle(_texture), nullptr,
			dev.getNextObjectIndex());
}

bool Image::init(Device &dev, WGPUTexture texture, StringView name,
		const core::ImageInfoData &info) {
	_info = info;
	_name = name.str<Interface>();
	_texture = texture;

	return core::ImageObject::init(dev,
			[](core::Device *, core::ObjectType, core::ObjectHandle ptr, void *) {
		wgpuTextureRelease((WGPUTexture)ptr.get());
	}, core::ObjectType::Image, core::ObjectHandle(_texture), nullptr,
			dev.getNextObjectIndex());
}

bool Image::init(Device &dev, const core::ImageData &data) {
	auto info = core::ImageInfoData(data);
	info.usage |= core::ImageUsage::TransferDst; // for wgpuQueueWriteTexture

	if (!init(dev, data.key, info)) {
		return false;
	}

	if (!data.data.empty() || data.memCallback || data.stdCallback) {
		auto blockSize = core::getFormatBlockSize(info.format);
		uint64_t expected = uint64_t(info.extent.width) * info.extent.height * info.extent.depth
				* blockSize;

		Vector<uint8_t> tmp;
		tmp.resize(expected, 0);
		data.writeData(tmp.data(), size_t(expected));

		WGPUTexelCopyTextureInfo dst;
		dst.texture = _texture;
		dst.mipLevel = 0;
		dst.origin = WGPUOrigin3D{0, 0, 0};
		dst.aspect = WGPUTextureAspect_All;

		WGPUTexelCopyBufferLayout layout;
		layout.offset = 0;
		layout.bytesPerRow = info.extent.width * blockSize;
		layout.rowsPerImage = info.extent.height;

		WGPUExtent3D writeSize{info.extent.width, info.extent.height,
			sprt::max(info.extent.depth, info.arrayLayers.get())};

		wgpuQueueWriteTexture(dev.getQueue(), &dst, tmp.data(), size_t(expected), &layout,
				&writeSize);
	}

	return true;
}

bool ImageView::init(Device &dev, const Rc<core::ImageObject> &image,
		const core::ImageViewInfo &info) {
	auto img = image.get_cast<Image>();
	if (!img) {
		return false;
	}

	_image = image;
	_info = info;

	auto format = info.format == core::ImageFormat::Undefined ? image->getInfo().format
															  : info.format;

	WGPUTextureViewDescriptor viewDesc = WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT;
	viewDesc.format = getWGPUFormat(format);
	switch (info.type) {
	case core::ImageViewType::ImageView1D: viewDesc.dimension = WGPUTextureViewDimension_1D; break;
	case core::ImageViewType::ImageView2D: viewDesc.dimension = WGPUTextureViewDimension_2D; break;
	case core::ImageViewType::ImageView3D: viewDesc.dimension = WGPUTextureViewDimension_3D; break;
	case core::ImageViewType::ImageView1DArray:
	case core::ImageViewType::ImageView2DArray:
		viewDesc.dimension = WGPUTextureViewDimension_2DArray;
		break;
	case core::ImageViewType::ImageViewCube:
		viewDesc.dimension = WGPUTextureViewDimension_Cube;
		break;
	case core::ImageViewType::ImageViewCubeArray:
		viewDesc.dimension = WGPUTextureViewDimension_CubeArray;
		break;
	}
	viewDesc.baseMipLevel = 0;
	viewDesc.mipLevelCount = image->getInfo().mipLevels.get();
	viewDesc.baseArrayLayer = info.baseArrayLayer.get();
	viewDesc.arrayLayerCount = info.layerCount.get() == maxOf<uint32_t>()
			? image->getInfo().arrayLayers.get() - info.baseArrayLayer.get()
			: info.layerCount.get();

	// component mapping (ColorMode) via the TextureComponentSwizzle feature;
	// R8 empty/solid/font-atlas textures rely on it (e.g. RGB=One, A=R)
	WGPUTextureComponentSwizzleDescriptor swizzleDesc =
			WGPU_TEXTURE_COMPONENT_SWIZZLE_DESCRIPTOR_INIT;
	auto mapSwizzle = [](core::ComponentMapping m) {
		switch (m) {
		case core::ComponentMapping::Identity: return WGPUComponentSwizzle_Undefined;
		case core::ComponentMapping::Zero: return WGPUComponentSwizzle_Zero;
		case core::ComponentMapping::One: return WGPUComponentSwizzle_One;
		case core::ComponentMapping::R: return WGPUComponentSwizzle_R;
		case core::ComponentMapping::G: return WGPUComponentSwizzle_G;
		case core::ComponentMapping::B: return WGPUComponentSwizzle_B;
		case core::ComponentMapping::A: return WGPUComponentSwizzle_A;
		}
		return WGPUComponentSwizzle_Undefined;
	};

	if (info.r != core::ComponentMapping::Identity || info.g != core::ComponentMapping::Identity
			|| info.b != core::ComponentMapping::Identity
			|| info.a != core::ComponentMapping::Identity) {
		if (dev.hasFeature(WGPUFeatureName_TextureComponentSwizzle)) {
			swizzleDesc.swizzle.r = mapSwizzle(info.r);
			swizzleDesc.swizzle.g = mapSwizzle(info.g);
			swizzleDesc.swizzle.b = mapSwizzle(info.b);
			swizzleDesc.swizzle.a = mapSwizzle(info.a);
			viewDesc.nextInChain = &swizzleDesc.chain;
		} else {
			log::source().warn("webgpu::ImageView",
					"Component mapping requested, but TextureComponentSwizzle "
					"is not supported by the device");
		}
	}

	_view = wgpuTextureCreateView(img->getTexture(), &viewDesc);
	if (!_view) {
		log::source().error("webgpu::ImageView", "Fail to create texture view");
		return false;
	}

	// unique index: image views participate in framebuffer cache keys
	_index = dev.getNextObjectIndex();

	return core::ImageView::init(dev,
			[](core::Device *, core::ObjectType, core::ObjectHandle ptr, void *) {
		wgpuTextureViewRelease((WGPUTextureView)ptr.get());
	}, core::ObjectType::ImageView, core::ObjectHandle(_view), nullptr);
}

bool Framebuffer::init(Device &dev, const core::QueuePassData *pass,
		SpanView<Rc<core::ImageView>> views) {
	_renderPass = pass->impl;

	if (!views.empty()) {
		auto extent = views.front()->getFramebufferExtent();
		_extent = Extent2(extent.width, extent.height);
		_layerCount = extent.depth;
	}

	for (auto &it : views) {
		_imageViews.emplace_back(it);
		_viewIds.emplace_back(it->getIndex());
	}

	return core::Framebuffer::init(dev,
			[](core::Device *, core::ObjectType, core::ObjectHandle, void *) { },
			core::ObjectType::Framebuffer, core::ObjectHandle::zero());
}

bool Semaphore::init(Device &dev) {
	return core::Semaphore::init(dev,
			[](core::Device *, core::ObjectType, core::ObjectHandle, void *) { },
			core::ObjectType::Semaphore, core::ObjectHandle::zero());
}

bool Fence::init(Device &dev, core::FenceType type) {
	_type = type;
	_flag = Rc<SignalFlag>::alloc();
	return core::Object::init(dev,
			[](core::Device *, core::ObjectType, core::ObjectHandle, void *) { },
			core::ObjectType::Fence, core::ObjectHandle::zero());
}

void Fence::arm(WGPUQueue queue) {
	WGPUQueueWorkDoneCallbackInfo cbInfo = WGPU_QUEUE_WORK_DONE_CALLBACK_INFO_INIT;
	cbInfo.mode = WGPUCallbackMode_AllowProcessEvents;
	cbInfo.callback = [](WGPUQueueWorkDoneStatus status, WGPUStringView message, void *userdata1,
			void *) {
		auto flag = reinterpret_cast<Rc<SignalFlag> *>(userdata1);
		if (status != WGPUQueueWorkDoneStatus_Success) {
			log::source().error("webgpu::Fence", "OnSubmittedWorkDone failed: ",
					toStringView(message));
		}
		(*flag)->signaled.store(true);
		delete flag;
	};

	// flag reference is owned by the callback, it can outlive the fence
	cbInfo.userdata1 = new Rc<SignalFlag>(_flag);

	wgpuQueueOnSubmittedWorkDone(queue, cbInfo);

	setArmed();
}

void Fence::signal() {
	setArmed();
	_flag->signaled.store(true);
}

Status Fence::doCheckFence(bool lockfree) {
	if (_flag->signaled.load()) {
		return Status::Ok;
	}

	if (!lockfree) {
		auto dev = static_cast<Device *>(_object.device);
		while (!_flag->signaled.load()) { wgpuDevicePoll(dev->getDevice(), true, nullptr); }
		return Status::Ok;
	}

	return Status::Suspended;
}

void Fence::doResetFence() { _flag->signaled.store(false); }

} // namespace stappler::xenolith::webgpu
