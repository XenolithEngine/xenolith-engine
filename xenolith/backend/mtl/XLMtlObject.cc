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

#include "XLMtlObject.h"
#include "XLMtlPipeline.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::mtl {

// shared ClearCallback: every object handle is a __bridge_retained ObjC object
static void clearObjectHandle(core::Device *, core::ObjectType, core::ObjectHandle ptr, void *) {
	releaseHandle((void *)ptr.get());
}

bool Buffer::setup(Device &dev, const core::BufferInfo &info,
		const Callback<size_t(uint8_t *, uint64_t)> *fill) {
	_info = info;

	// shared storage: CPU-visible, coherent on unified memory; a dedicated
	// private-storage allocator with staging is a further optimization step
	id<MTLBuffer> buffer = [dev.getDevice() newBufferWithLength:info.size
														options:MTLResourceStorageModeShared];
	if (!buffer) {
		log::source().error("mtl::Buffer", "Fail to create buffer: ", info.key);
		return false;
	}

	buffer.label = [NSString stringWithUTF8String:info.key.str<Interface>().data()];

	if (fill) {
		(*fill)(reinterpret_cast<uint8_t *>(buffer.contents), info.size);
	}

	_buffer = retainHandle(buffer);

	return core::BufferObject::init(dev, clearObjectHandle, core::ObjectType::Buffer,
			core::ObjectHandle(_buffer));
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

bool Buffer::setData(BytesView data, uint64_t offset) {
	auto buffer = getBuffer();
	if (!buffer || offset + data.size() > buffer.length) {
		return false;
	}
	sprt::memcpy(reinterpret_cast<uint8_t *>(buffer.contents) + offset, data.data(), data.size());
	return true;
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
		return f == core::Filter::Linear ? MTLSamplerMinMagFilterLinear
										 : MTLSamplerMinMagFilterNearest;
	};

	auto getAddressMode = [](core::SamplerAddressMode mode) {
		switch (mode) {
		case core::SamplerAddressMode::Repeat: return MTLSamplerAddressModeRepeat; break;
		case core::SamplerAddressMode::MirroredRepeat:
			return MTLSamplerAddressModeMirrorRepeat;
			break;
		case core::SamplerAddressMode::ClampToEdge:
			return MTLSamplerAddressModeClampToEdge;
			break;
		case core::SamplerAddressMode::ClampToBorder:
			return MTLSamplerAddressModeClampToBorderColor;
			break;
		}
		return MTLSamplerAddressModeClampToEdge;
	};

	@autoreleasepool {
		MTLSamplerDescriptor *desc = [[MTLSamplerDescriptor alloc] init];
		desc.minFilter = getFilter(info.minFilter);
		desc.magFilter = getFilter(info.magFilter);
		desc.mipFilter = info.mipmapMode == core::SamplerMipmapMode::Linear
				? MTLSamplerMipFilterLinear
				: MTLSamplerMipFilterNearest;
		desc.sAddressMode = getAddressMode(info.addressModeU);
		desc.tAddressMode = getAddressMode(info.addressModeV);
		desc.rAddressMode = getAddressMode(info.addressModeW);
		desc.lodMinClamp = info.minLod;
		desc.lodMaxClamp = info.maxLod;
		if (info.compareEnable) {
			desc.compareFunction = getMTLCompareFunction(info.compareOp);
		}
		if (info.anisotropyEnable) {
			desc.maxAnisotropy = NSUInteger(info.maxAnisotropy);
		}
		// gpuResourceID (texture set argument buffers) requires this
		desc.supportArgumentBuffers = YES;

		id<MTLSamplerState> sampler = [dev.getDevice() newSamplerStateWithDescriptor:desc];
		if (!sampler) {
			log::source().error("mtl::Sampler", "Fail to create sampler");
			return false;
		}

		_sampler = retainHandle(sampler);
	}

	return core::Sampler::init(dev, clearObjectHandle, core::ObjectType::Sampler,
			core::ObjectHandle(_sampler));
}

static MTLTextureUsage getMTLTextureUsage(core::ImageUsage usage) {
	MTLTextureUsage ret = MTLTextureUsageUnknown;
	if (hasFlag(usage, core::ImageUsage::Sampled) || hasFlag(usage, core::ImageUsage::TransferSrc)
			|| hasFlag(usage, core::ImageUsage::InputAttachment)) {
		ret |= MTLTextureUsageShaderRead;
	}
	if (hasFlag(usage, core::ImageUsage::Storage)) {
		ret |= MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
	}
	if (hasFlag(usage, core::ImageUsage::ColorAttachment)
			|| hasFlag(usage, core::ImageUsage::DepthStencilAttachment)) {
		ret |= MTLTextureUsageRenderTarget;
	}
	return ret;
}

bool Image::init(Device &dev, StringView name, const core::ImageInfoData &info) {
	_info = info;
	_name = name.str<Interface>();

	@autoreleasepool {
		MTLTextureDescriptor *desc = [[MTLTextureDescriptor alloc] init];
		switch (info.imageType) {
		case core::ImageType::Image1D:
			desc.textureType = info.arrayLayers.get() > 1 ? MTLTextureType1DArray
														  : MTLTextureType1D;
			break;
		case core::ImageType::Image2D:
			desc.textureType = info.arrayLayers.get() > 1 ? MTLTextureType2DArray
														  : MTLTextureType2D;
			break;
		case core::ImageType::Image3D: desc.textureType = MTLTextureType3D; break;
		}
		desc.pixelFormat = getMTLPixelFormat(info.format);
		desc.width = info.extent.width;
		desc.height = info.extent.height;
		desc.depth = sprt::max(uint32_t(1), info.extent.depth);
		desc.mipmapLevelCount = info.mipLevels.get();
		desc.arrayLength = info.arrayLayers.get();
		desc.sampleCount = uint32_t(toInt(info.samples));
		desc.usage = getMTLTextureUsage(info.usage);
		// shared storage keeps replaceRegion uploads possible; attachments
		// benefit from private storage - split when the allocator arrives
		desc.storageMode = hasFlag(info.usage, core::ImageUsage::DepthStencilAttachment)
				? MTLStorageModePrivate
				: MTLStorageModeShared;

		id<MTLTexture> texture = [dev.getDevice() newTextureWithDescriptor:desc];
		if (!texture) {
			log::source().error("mtl::Image", "Fail to create texture: ", _name);
			return false;
		}
		texture.label = [NSString stringWithUTF8String:_name.data()];

		_texture = retainHandle(texture);
	}

	return core::ImageObject::init(dev, clearObjectHandle, core::ObjectType::Image,
			core::ObjectHandle(_texture), nullptr, dev.getNextObjectIndex());
}

bool Image::init(Device &dev, id<MTLTexture> texture, StringView name,
		const core::ImageInfoData &info) {
	_info = info;
	_name = name.str<Interface>();
	_texture = retainHandle(texture);

	return core::ImageObject::init(dev, clearObjectHandle, core::ObjectType::Image,
			core::ObjectHandle(_texture), nullptr, dev.getNextObjectIndex());
}

bool Image::init(Device &dev, const core::ImageData &data) {
	auto info = core::ImageInfoData(data);
	info.usage |= core::ImageUsage::TransferDst;

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

		auto texture = getTexture();
		[texture replaceRegion:MTLRegionMake2D(0, 0, info.extent.width, info.extent.height)
				   mipmapLevel:0
					 withBytes:tmp.data()
				   bytesPerRow:info.extent.width * blockSize];
	}

	return true;
}

bool Image::setData(BytesView data) {
	auto texture = getTexture();
	if (!texture || data.empty()) {
		return false;
	}

	auto blockSize = core::getFormatBlockSize(_info.format);
	const uint64_t bytesPerRow = uint64_t(_info.extent.width) * blockSize;
	if (data.size() < bytesPerRow * _info.extent.height) {
		return false;
	}

	[texture replaceRegion:MTLRegionMake2D(0, 0, _info.extent.width, _info.extent.height)
			   mipmapLevel:0
				 withBytes:data.data()
			   bytesPerRow:bytesPerRow];
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

	auto layerCount = info.layerCount.get() == maxOf<uint32_t>()
			? image->getInfo().arrayLayers.get() - info.baseArrayLayer.get()
			: info.layerCount.get();

	@autoreleasepool {
		auto texture = img->getTexture();

		MTLTextureType viewType = texture.textureType;
		switch (info.type) {
		case core::ImageViewType::ImageView1D: viewType = MTLTextureType1D; break;
		case core::ImageViewType::ImageView1DArray: viewType = MTLTextureType1DArray; break;
		case core::ImageViewType::ImageView2D: viewType = MTLTextureType2D; break;
		case core::ImageViewType::ImageView2DArray: viewType = MTLTextureType2DArray; break;
		case core::ImageViewType::ImageView3D: viewType = MTLTextureType3D; break;
		case core::ImageViewType::ImageViewCube: viewType = MTLTextureTypeCube; break;
		case core::ImageViewType::ImageViewCubeArray:
			viewType = MTLTextureTypeCubeArray;
			break;
		}

		// component mapping (ColorMode swizzle, e.g. RGB=One A=R for font
		// atlases) is expressed with a swizzled texture view
		MTLTextureSwizzleChannels swizzle = MTLTextureSwizzleChannelsDefault;
		auto mapSwizzle = [](core::ComponentMapping m, MTLTextureSwizzle def) {
			switch (m) {
			case core::ComponentMapping::Identity: return def;
			case core::ComponentMapping::Zero: return MTLTextureSwizzleZero;
			case core::ComponentMapping::One: return MTLTextureSwizzleOne;
			case core::ComponentMapping::R: return MTLTextureSwizzleRed;
			case core::ComponentMapping::G: return MTLTextureSwizzleGreen;
			case core::ComponentMapping::B: return MTLTextureSwizzleBlue;
			case core::ComponentMapping::A: return MTLTextureSwizzleAlpha;
			}
			return def;
		};
		swizzle.red = mapSwizzle(info.r, MTLTextureSwizzleRed);
		swizzle.green = mapSwizzle(info.g, MTLTextureSwizzleGreen);
		swizzle.blue = mapSwizzle(info.b, MTLTextureSwizzleBlue);
		swizzle.alpha = mapSwizzle(info.a, MTLTextureSwizzleAlpha);

		id<MTLTexture> view = [texture
				newTextureViewWithPixelFormat:getMTLPixelFormat(format)
								  textureType:viewType
									   levels:NSMakeRange(0, image->getInfo().mipLevels.get())
									   slices:NSMakeRange(info.baseArrayLayer.get(), layerCount)
									  swizzle:swizzle];
		if (!view) {
			log::source().error("mtl::ImageView", "Fail to create texture view");
			return false;
		}

		_view = retainHandle(view);
	}

	// unique index: image views participate in framebuffer cache keys
	_index = dev.getNextObjectIndex();

	return core::ImageView::init(dev, clearObjectHandle, core::ObjectType::ImageView,
			core::ObjectHandle(_view), nullptr);
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

void Fence::arm(id<MTLCommandBuffer> buffer) {
	// flag reference is owned by the handler block, it can outlive the fence
	auto flag = _flag;
	[buffer addCompletedHandler:^(id<MTLCommandBuffer>) { flag->signaled.store(true); }];

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
		// completed handlers run on Metal's internal dispatch queue: waiting
		// for the device drains everything submitted before this point
		auto dev = static_cast<Device *>(_object.device);
		dev->waitIdle();
		if (_flag->signaled.load()) {
			return Status::Ok;
		}
	}

	return Status::Suspended;
}

void Fence::doResetFence() { _flag->signaled.store(false); }

} // namespace stappler::xenolith::mtl
