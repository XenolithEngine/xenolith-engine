/**
 Copyright (c) 2021 Roman Katuntsev <sbkarr@stappler.org>
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

#include "XLVkObject.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk {

bool DeviceMemory::init(Allocator *a, DeviceMemoryInfo info, VkDeviceMemory memory,
		AllocationUsage usage) {
	_allocator = a;
	_memory = memory;
	_info = info;
	_usage = usage;

	if (memory) {
		return core::Object::init(*_allocator->getDevice(),
				[](core::Device *dev, core::ObjectType, ObjectHandle ptr, void *) {
			auto d = ((Device *)dev);
			d->makeApiCall([&](const DeviceTable &table, VkDevice device) {
				table.vkFreeMemory(device, (VkDeviceMemory)ptr.get(), nullptr);
			});
		}, core::ObjectType::DeviceMemory, ObjectHandle(_memory));
	} else {
		return core::Object::init(*_allocator->getDevice(),
				[](core::Device *dev, core::ObjectType, ObjectHandle ptr, void *) { },
				core::ObjectType::DeviceMemory, ObjectHandle(_memory));
	}
}

bool DeviceMemory::init(DeviceMemoryPool *p, Allocator::MemBlock &&block, AllocationUsage usage) {
	_allocator = p->getAllocator();
	_pool = p;
	_info = DeviceMemoryInfo{block.size, 1, block.type, false};
	_memBlock = move(block);
	_memory = _memBlock.mem;
	_usage = usage;

	if (_memBlock.ptr) {
		// persistent mapping
		_mappedOffset = 0;
		_mappedSize = _info.size;
	}

	return core::Object::init(*_allocator->getDevice(),
			[](core::Device *dev, core::ObjectType, ObjectHandle ptr, void *thiz) {
		auto mem = static_cast<DeviceMemory *>(thiz);
		mem->_pool->free(move(mem->_memBlock));
	}, core::ObjectType::DeviceMemory, ObjectHandle::zero(), this);
}

bool DeviceMemory::isPersistentMapped() const { return _memBlock.ptr != nullptr; }

uint8_t *DeviceMemory::getPersistentMappedRegion() const {
	return static_cast<uint8_t *>(_memBlock.ptr) + _memBlock.offset;
}

bool DeviceMemory::map(const Callback<void(uint8_t *, VkDeviceSize)> &cb, VkDeviceSize offset,
		VkDeviceSize size, DeviceMemoryAccess access) {
	auto t = _allocator->getType(_info.memoryType);
	if (!t->isHostVisible()) {
		return false;
	}

	bool hostCoherent = t->isHostCoherent();
	auto range = calculateMappedMemoryRange(offset, size);

	std::unique_lock<Mutex> lock(
			_memBlock.mappingProtection ? *_memBlock.mappingProtection : _mappingProtectionMutex);

	uint8_t *mapped = nullptr;
	if (_memBlock.ptr) {
		mapped = static_cast<uint8_t *>(_memBlock.ptr) + _memBlock.offset + offset;
	} else {
		if (_allocator->getDevice()->getTable()->vkMapMemory(_allocator->getDevice()->getDevice(),
					_memory, range.offset, range.size, 0, (void **)&mapped)
				!= VK_SUCCESS) {
			return false;
		}
		mapped += (_memBlock.offset + offset - range.offset);

		_mappedOffset = range.offset;
		_mappedSize = range.size;
	}

	if (!hostCoherent && (access & DeviceMemoryAccess::Invalidate) != DeviceMemoryAccess::None) {
		_allocator->getDevice()->getTable()->vkInvalidateMappedMemoryRanges(
				_allocator->getDevice()->getDevice(), 1, &range);
	}

	cb(static_cast<uint8_t *>(mapped), std::min(_info.size - offset, size));

	if (!hostCoherent && (access & DeviceMemoryAccess::Flush) != DeviceMemoryAccess::None) {
		_allocator->getDevice()->getTable()->vkFlushMappedMemoryRanges(
				_allocator->getDevice()->getDevice(), 1, &range);
	}

	if (!_memBlock.ptr) {
		_mappedOffset = 0;
		_mappedSize = 0;

		_allocator->getDevice()->getTable()->vkUnmapMemory(_allocator->getDevice()->getDevice(),
				_memory);
	}
	return true;
}

void DeviceMemory::invalidateMappedRegion(VkDeviceSize offset, VkDeviceSize size) {
	auto t = _allocator->getType(_info.memoryType);
	if (!t->isHostVisible() || t->isHostCoherent()) {
		return;
	}

	size = std::min(_info.size, size);
	offset += _memBlock.offset;

	offset = std::max(_mappedOffset, offset);
	size = std::min(_mappedSize, size);

	auto range = calculateMappedMemoryRange(offset, size);

	_allocator->getDevice()->getTable()->vkInvalidateMappedMemoryRanges(
			_allocator->getDevice()->getDevice(), 1, &range);
}

void DeviceMemory::flushMappedRegion(VkDeviceSize offset, VkDeviceSize size) {
	auto t = _allocator->getType(_info.memoryType);
	if (!t->isHostVisible() || t->isHostCoherent()) {
		return;
	}

	size = std::min(_info.size, size);
	offset += _memBlock.offset;

	offset = std::max(_mappedOffset, offset);
	size = std::min(_mappedSize, size);

	auto range = calculateMappedMemoryRange(offset, size);

	_allocator->getDevice()->getTable()->vkFlushMappedMemoryRanges(
			_allocator->getDevice()->getDevice(), 1, &range);
}

VkMappedMemoryRange DeviceMemory::calculateMappedMemoryRange(VkDeviceSize offset,
		VkDeviceSize size) const {
	auto t = _allocator->getType(_info.memoryType);

	size = std::min(_info.size, size);
	offset += _memBlock.offset;

	VkDeviceSize atomSize = t->isHostCoherent() ? 1 : _allocator->getNonCoherentAtomSize();

	VkMappedMemoryRange range;
	range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
	range.pNext = nullptr;
	range.memory = _memory;
	range.offset = math::align<VkDeviceSize>(offset - atomSize + 1, atomSize);
	range.size = std::min(_info.size - range.offset,
			math::align<VkDeviceSize>(size + (offset - range.offset), atomSize));
	return range;
}

bool Image::init(Device &dev, StringView key, VkImage image, const ImageInfoData &info,
		uint32_t idx) {
	sprt_passert(!key.empty(), "Image should have a name");

	_name = key.str<Interface>();
	_info = info;
	_image = image;

	auto ret = core::ImageObject::init(dev,
			[](core::Device *dev, core::ObjectType, ObjectHandle ptr, void *thiz) {
		// do nothing
		static_cast<Image *>(thiz)->_memory = nullptr;
	}, core::ObjectType::Image, ObjectHandle(_image), this);
	if (ret) {
		_index = idx;
	}
	return ret;
}

bool Image::init(Device &dev, StringView key, VkImage image, const ImageInfoData &info,
		Rc<DeviceMemory> &&mem, Rc<core::DataAtlas> &&atlas) {
	sprt_passert(!key.empty(), "Image should have a name");

	_name = key.str<Interface>();
	_info = info;
	_image = image;
	_atlas = atlas;
	_memory = move(mem);

	return core::ImageObject::init(dev,
			[](core::Device *dev, core::ObjectType, ObjectHandle ptr, void *thiz) {
		auto d = ((Device *)dev);
		d->getTable()->vkDestroyImage(d->getDevice(), (VkImage)ptr.get(), nullptr);
		static_cast<Image *>(thiz)->_memory = nullptr;
	}, core::ObjectType::Image, ObjectHandle(_image), this);
}

bool Image::init(Device &dev, StringView key, uint64_t idx, VkImage image,
		const ImageInfoData &info, Rc<DeviceMemory> &&mem, Rc<core::DataAtlas> &&atlas) {
	sprt_passert(!key.empty(), "Image should have a name");

	_name = key.str<Interface>();
	_info = info;
	_image = image;
	_atlas = atlas;
	_memory = move(mem);

	return core::ImageObject::init(dev,
			[](core::Device *dev, core::ObjectType, ObjectHandle ptr, void *thiz) {
		auto d = ((Device *)dev);
		d->getTable()->vkDestroyImage(d->getDevice(), (VkImage)ptr.get(), nullptr);
		static_cast<Image *>(thiz)->_memory = nullptr;
	}, core::ObjectType::Image, ObjectHandle(_image), this, idx);
}

void Image::setPendingBarrier(const ImageMemoryBarrier &barrier) {
	_barrier = barrier;
	_barrier->image = this;
	_barrier->vkimage = _image;
}

const ImageMemoryBarrier *Image::getPendingBarrier() const {
	if (_barrier) {
		return &_barrier.value();
	} else {
		return nullptr;
	}
}

void Image::dropPendingBarrier() { _barrier.reset(); }

bool Image::bindMemory(Rc<DeviceMemory> &&mem, VkDeviceSize offset) {
	auto dev = (Device *)_object.device;
	if (dev->getTable()->vkBindImageMemory(dev->getDevice(), _image, mem->getMemory(),
				offset + mem->getBlockOffset())
			== VK_SUCCESS) {
		_memory = move(mem);
		return true;
	}
	return false;
}

bool Buffer::init(Device &dev, VkBuffer buffer, const BufferInfo &info, Rc<DeviceMemory> &&mem,
		VkDeviceSize memoryOffset) {
	_info = info;
	_buffer = buffer;
	_memory = move(mem);
	_memoryOffset = memoryOffset;

	if (!_info.key.empty()) {
		_name = _info.key.str<Interface>();
	}

	if ((_info.usage & core::BufferUsage::ShaderDeviceAddress) != core::BufferUsage::None) {
		if (dev.hasBufferDeviceAddresses()) {
			if (_memory) {
				VkBufferDeviceAddressInfoKHR info;
				info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO_KHR;
				info.pNext = nullptr;
				info.buffer = _buffer;

				_deviceAddress =
						dev.getTable()->vkGetBufferDeviceAddressKHR(dev.getDevice(), &info);
			}
		} else {
			// Remove BDA flag if not available
			_info.usage &= ~core::BufferUsage::ShaderDeviceAddress;
		}
	}

	return core::BufferObject::init(dev,
			[](core::Device *dev, core::ObjectType, ObjectHandle ptr, void *thiz) {
		auto d = ((Device *)dev);
		d->getTable()->vkDestroyBuffer(d->getDevice(), (VkBuffer)ptr.get(), nullptr);
		static_cast<Buffer *>(thiz)->_memory = nullptr;
	}, core::ObjectType::Buffer, ObjectHandle(_buffer), this);
}

void Buffer::setPendingBarrier(const BufferMemoryBarrier &barrier) {
	_barrier = barrier;
	_barrier->buffer = this;
	_barrier->vkbuffer = _buffer;
}

const BufferMemoryBarrier *Buffer::getPendingBarrier() const {
	if (_barrier) {
		return &_barrier.value();
	} else {
		return nullptr;
	}
}

void Buffer::dropPendingBarrier() { _barrier.reset(); }

bool Buffer::bindMemory(Rc<DeviceMemory> &&mem, VkDeviceSize offset) {
	auto dev = (Device *)_object.device;
	if (dev->getTable()->vkBindBufferMemory(dev->getDevice(), _buffer, mem->getMemory(),
				offset + mem->getBlockOffset())
			== VK_SUCCESS) {
		_memoryOffset = offset;
		_memory = move(mem);

		if (_memory
				&& (_info.usage & core::BufferUsage::ShaderDeviceAddress)
						!= core::BufferUsage::None) {
			VkBufferDeviceAddressInfoKHR info;
			info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO_KHR;
			info.pNext = nullptr;
			info.buffer = _buffer;

			_deviceAddress = dev->getTable()->vkGetBufferDeviceAddressKHR(dev->getDevice(), &info);
		}

		return true;
	}
	return false;
}

bool Buffer::map(const Callback<void(uint8_t *, VkDeviceSize)> &cb, VkDeviceSize offset,
		VkDeviceSize size, DeviceMemoryAccess access) {
	size = std::min(_info.size - offset, size);
	offset += _memoryOffset;
	return _memory->map(cb, offset, size, access);
}

bool Buffer::map(const Callback<void(uint8_t *, VkDeviceSize)> &cb, DeviceMemoryAccess access) {
	return map(cb, 0, maxOf<VkDeviceSize>(), access);
}

uint8_t *Buffer::getPersistentMappedRegion(bool invalidate) {
	if (_memory->isPersistentMapped()) {
		if (invalidate) {
			invalidateMappedRegion();
		}
		return _memory->getPersistentMappedRegion() + _memoryOffset;
	}
	return nullptr;
}

void Buffer::invalidateMappedRegion(VkDeviceSize offset, VkDeviceSize size) {
	offset += _memoryOffset;
	size = std::min(size, _info.size);

	_memory->invalidateMappedRegion(offset, size);
}

void Buffer::flushMappedRegion(VkDeviceSize offset, VkDeviceSize size) {
	offset += _memoryOffset;
	size = std::min(size, _info.size);

	_memory->flushMappedRegion(offset, size);
}

bool Buffer::setData(BytesView data, VkDeviceSize offset, DeviceMemoryAccess access) {
	auto size = std::min(size_t(_info.size - offset), data.size());

	return _memory->map([&](uint8_t *ptr, VkDeviceSize size) {
		::__sprt_memcpy(ptr, data.data(), size);
	}, _memoryOffset + offset, size, access);
}

Bytes Buffer::getData(VkDeviceSize size, VkDeviceSize offset, DeviceMemoryAccess access) {
	size = std::min(_info.size - offset, size);

	Bytes ret;

	_memory->map([&](uint8_t *ptr, VkDeviceSize size) {
		ret.resize(size);
		::__sprt_memcpy(ret.data(), ptr, size);
	}, _memoryOffset + offset, size, access);

	return ret;
}

uint64_t Buffer::reserveBlock(uint64_t blockSize, uint64_t alignment) {
	auto alignedSize = math::align(uint64_t(blockSize), alignment);
	auto ret = _targetOffset.fetch_add(alignedSize);
	if (ret + blockSize > _info.size) {
		return maxOf<uint64_t>();
	}
	return ret;
}

bool ImageView::init(Device &dev, VkImage image, VkFormat format) {
	VkImageViewCreateInfo createInfo{};
	sanitizeVkStruct(createInfo);
	createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	createInfo.image = image;
	createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	createInfo.format = format;
	createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	createInfo.subresourceRange.baseMipLevel = 0;
	createInfo.subresourceRange.levelCount = 1;
	createInfo.subresourceRange.baseArrayLayer = 0;
	createInfo.subresourceRange.layerCount = 1;

	if (dev.getTable()->vkCreateImageView(dev.getDevice(), &createInfo, nullptr, &_imageView)
			== VK_SUCCESS) {
		return core::ImageView::init(dev,
				[](core::Device *dev, core::ObjectType, ObjectHandle ptr, void *thiz) {
			auto d = ((Device *)dev);
			d->getTable()->vkDestroyImageView(d->getDevice(), (VkImageView)ptr.get(), nullptr);

			auto obj = static_cast<ImageView *>(thiz);
			if (obj->_releaseCallback) {
				obj->_releaseCallback();
				obj->_releaseCallback = nullptr;
			}
		}, core::ObjectType::ImageView, ObjectHandle(_imageView), this);
	}
	return false;
}

bool ImageView::init(Device &dev, Image *image, const ImageViewInfo &info) {
	VkImageViewCreateInfo createInfo{};
	sanitizeVkStruct(createInfo);
	createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	createInfo.image = image->getImage();

	switch (info.type) {
	case core::ImageViewType::ImageView1D:
		if (image->getInfo().imageType != core::ImageType::Image1D) {
			log::source().error("Vk-ImageView", "Incompatible ImageType '",
					core::getImageTypeName(image->getInfo().imageType), "' and ImageViewType '",
					core::getImageViewTypeName(info.type), "'");
			return false;
		}
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_1D;
		break;
	case core::ImageViewType::ImageView1DArray:
		if (image->getInfo().imageType != core::ImageType::Image1D) {
			log::source().error("Vk-ImageView", "Incompatible ImageType '",
					core::getImageTypeName(image->getInfo().imageType), "' and ImageViewType '",
					core::getImageViewTypeName(info.type), "'");
			return false;
		}
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_1D_ARRAY;
		break;
	case core::ImageViewType::ImageView2D:
		if (image->getInfo().imageType != core::ImageType::Image2D
				&& image->getInfo().imageType != core::ImageType::Image3D) {
			log::source().error("Vk-ImageView", "Incompatible ImageType '",
					core::getImageTypeName(image->getInfo().imageType), "' and ImageViewType '",
					core::getImageViewTypeName(info.type), "'");
			return false;
		}
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		break;
	case core::ImageViewType::ImageView2DArray:
		if (image->getInfo().imageType != core::ImageType::Image2D
				&& image->getInfo().imageType != core::ImageType::Image3D) {
			log::source().error("Vk-ImageView", "Incompatible ImageType '",
					core::getImageTypeName(image->getInfo().imageType), "' and ImageViewType '",
					core::getImageViewTypeName(info.type), "'");
			return false;
		}
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
		break;
	case core::ImageViewType::ImageView3D:
		if (image->getInfo().imageType != core::ImageType::Image3D) {
			log::source().error("Vk-ImageView", "Incompatible ImageType '",
					core::getImageTypeName(image->getInfo().imageType), "' and ImageViewType '",
					core::getImageViewTypeName(info.type), "'");
			return false;
		}
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;
		break;
	case core::ImageViewType::ImageViewCube:
		if (image->getInfo().imageType != core::ImageType::Image2D) {
			log::source().error("Vk-ImageView", "Incompatible ImageType '",
					core::getImageTypeName(image->getInfo().imageType), "' and ImageViewType '",
					core::getImageViewTypeName(info.type), "'");
			return false;
		}
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
		break;
	case core::ImageViewType::ImageViewCubeArray:
		if (image->getInfo().imageType != core::ImageType::Image2D) {
			log::source().error("Vk-ImageView", "Incompatible ImageType '",
					core::getImageTypeName(image->getInfo().imageType), "' and ImageViewType '",
					core::getImageViewTypeName(info.type), "'");
			return false;
		}
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
		break;
	}

	auto format = info.format;
	if (format == core::ImageFormat::Undefined) {
		format = image->getInfo().format;
	}
	createInfo.format = VkFormat(format);

	createInfo.components.r = VkComponentSwizzle(info.r);
	createInfo.components.g = VkComponentSwizzle(info.g);
	createInfo.components.b = VkComponentSwizzle(info.b);
	createInfo.components.a = VkComponentSwizzle(info.a);

	switch (format) {
	case core::ImageFormat::D16_UNORM:
	case core::ImageFormat::X8_D24_UNORM_PACK32:
	case core::ImageFormat::D32_SFLOAT:
		createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		break;
	case core::ImageFormat::S8_UINT:
		createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
		break;
	case core::ImageFormat::D16_UNORM_S8_UINT:
	case core::ImageFormat::D24_UNORM_S8_UINT:
	case core::ImageFormat::D32_SFLOAT_S8_UINT:
		createInfo.subresourceRange.aspectMask |=
				VK_IMAGE_ASPECT_STENCIL_BIT | VK_IMAGE_ASPECT_DEPTH_BIT;
		break;
	default: createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; break;
	}

	createInfo.subresourceRange.baseMipLevel = 0;
	createInfo.subresourceRange.levelCount = image->getInfo().mipLevels.get();
	createInfo.subresourceRange.baseArrayLayer = info.baseArrayLayer.get();
	if (info.layerCount.get() == maxOf<uint32_t>()) {
		createInfo.subresourceRange.layerCount =
				image->getInfo().arrayLayers.get() - info.baseArrayLayer.get();
	} else {
		createInfo.subresourceRange.layerCount = info.layerCount.get();
	}

	if (info.type == core::ImageViewType::ImageView2D
			&& createInfo.subresourceRange.layerCount > 1) {
		createInfo.subresourceRange.layerCount = 1;
	}

	if (dev.getTable()->vkCreateImageView(dev.getDevice(), &createInfo, nullptr, &_imageView)
			== VK_SUCCESS) {
		_info = info;
		_info.format = format;
		_info.baseArrayLayer = core::BaseArrayLayer(createInfo.subresourceRange.baseArrayLayer);
		_info.layerCount = core::ArrayLayers(createInfo.subresourceRange.layerCount);

		_image = image;
		return core::ImageView::init(dev,
				[](core::Device *dev, core::ObjectType, ObjectHandle ptr, void *thiz) {
			auto d = ((Device *)dev);
			d->getTable()->vkDestroyImageView(d->getDevice(), (VkImageView)ptr.get(), nullptr);

			auto obj = static_cast<ImageView *>(thiz);
			if (obj->_releaseCallback) {
				obj->_releaseCallback();
				obj->_releaseCallback = nullptr;
			}
		}, core::ObjectType::ImageView, ObjectHandle(_imageView), this);
	}
	return false;
}

bool Sampler::init(Device &dev, const SamplerInfo &info) {
	VkSamplerCreateInfo createInfo;
	sanitizeVkStruct(createInfo);
	createInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	createInfo.pNext = nullptr;
	createInfo.flags = 0;
	createInfo.magFilter = VkFilter(info.magFilter);
	createInfo.minFilter = VkFilter(info.minFilter);
	createInfo.mipmapMode = VkSamplerMipmapMode(info.mipmapMode);
	createInfo.addressModeU = VkSamplerAddressMode(info.addressModeU);
	createInfo.addressModeV = VkSamplerAddressMode(info.addressModeV);
	createInfo.addressModeW = VkSamplerAddressMode(info.addressModeW);
	createInfo.mipLodBias = info.mipLodBias;
	createInfo.anisotropyEnable = info.anisotropyEnable;
	createInfo.maxAnisotropy = info.maxAnisotropy;
	createInfo.compareEnable = info.compareEnable;
	createInfo.compareOp = VkCompareOp(info.compareOp);
	createInfo.minLod = info.minLod;
	createInfo.maxLod = info.maxLod;
	createInfo.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
	createInfo.unnormalizedCoordinates = false;

	if (dev.getTable()->vkCreateSampler(dev.getDevice(), &createInfo, nullptr, &_sampler)
			== VK_SUCCESS) {
		_info = info;
		return core::Object::init(dev,
				[](core::Device *dev, core::ObjectType, ObjectHandle ptr, void *) {
			auto d = ((Device *)dev);
			d->getTable()->vkDestroySampler(d->getDevice(), (VkSampler)ptr.get(), nullptr);
		}, core::ObjectType::Sampler, ObjectHandle(_sampler));
	}
	return false;
}

} // namespace stappler::xenolith::vk
