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

#ifndef XENOLITH_BACKEND_VK_XLVKOBJECT_H_
#define XENOLITH_BACKEND_VK_XLVKOBJECT_H_

#include "XLVkDevice.h"
#include "XLVkAllocator.h"
#include "XLCoreObject.h"
#include "XLCoreImageStorage.h" // IWYU pragma: keep

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk {

class Surface;

struct SP_PUBLIC DeviceMemoryInfo {
	VkDeviceSize size;
	VkDeviceSize alignment;
	uint32_t memoryType;
	bool dedicated;
};

enum class DeviceMemoryAccess : uint32_t {
	None = 0,
	Invalidate = 1 << 0,
	Flush = 1 << 1,
	Full = Invalidate | Flush
};

SP_DEFINE_ENUM_AS_MASK(DeviceMemoryAccess)

class SP_PUBLIC DeviceMemory : public core::Object {
public:
	virtual ~DeviceMemory() { }

	bool init(Allocator *, DeviceMemoryInfo, VkDeviceMemory, AllocationUsage);
	bool init(DeviceMemoryPool *, Allocator::MemBlock &&, AllocationUsage);

	bool isPersistentMapped() const;

	uint8_t *getPersistentMappedRegion() const;

	const DeviceMemoryInfo &getInfo() const { return _info; }
	VkDeviceMemory getMemory() const { return _memory; }
	AllocationUsage getUsage() const { return _usage; }
	DeviceMemoryPool *getPool() const { return _pool; }

	VkDeviceSize getBlockOffset() const { return _memBlock.offset; }

	bool isMappable() const {
		return _usage != AllocationUsage::DeviceLocal
				&& _usage != AllocationUsage::DeviceLocalLazilyAllocated;
	}

	bool map(const Callback<void(uint8_t *, VkDeviceSize)> &, VkDeviceSize offset = 0,
			VkDeviceSize size = maxOf<VkDeviceSize>(),
			DeviceMemoryAccess = DeviceMemoryAccess::Full);

	void invalidateMappedRegion(VkDeviceSize offset = 0, VkDeviceSize size = maxOf<VkDeviceSize>());
	void flushMappedRegion(VkDeviceSize offset = 0, VkDeviceSize size = maxOf<VkDeviceSize>());

protected:
	using core::Object::init;

	VkMappedMemoryRange calculateMappedMemoryRange(VkDeviceSize offset, VkDeviceSize size) const;

	DeviceMemoryInfo _info;
	DeviceMemoryPool *_pool = nullptr;
	VkDeviceMemory _memory = VK_NULL_HANDLE;
	AllocationUsage _usage = AllocationUsage::DeviceLocal;
	Allocator::MemBlock _memBlock;
	Rc<Allocator> _allocator;

	VkDeviceSize _mappedOffset = 0;
	VkDeviceSize _mappedSize = 0;

	Mutex _mappingProtectionMutex;
};

class SP_PUBLIC Image : public core::ImageObject {
public:
	virtual ~Image() = default;

	// non-owining image wrapping
	bool init(Device &dev, StringView key, VkImage, const ImageInfoData &, uint32_t);

	// owning image wrapping
	bool init(Device &dev, StringView key, VkImage, const ImageInfoData &, Rc<DeviceMemory> &&,
			Rc<core::DataAtlas> && = Rc<core::DataAtlas>());
	bool init(Device &dev, StringView key, uint64_t, VkImage, const ImageInfoData &,
			Rc<DeviceMemory> &&, Rc<core::DataAtlas> && = Rc<core::DataAtlas>());

	VkImage getImage() const { return _image; }
	DeviceMemory *getMemory() const { return _memory; }

	void setPendingBarrier(const ImageMemoryBarrier &);
	const ImageMemoryBarrier *getPendingBarrier() const;
	void dropPendingBarrier();

	bool bindMemory(Rc<DeviceMemory> &&, VkDeviceSize = 0);

protected:
	using core::ImageObject::init;

	Rc<DeviceMemory> _memory;
	VkImage _image = VK_NULL_HANDLE;
	std::optional<ImageMemoryBarrier> _barrier;
};

class SP_PUBLIC Buffer : public core::BufferObject {
public:
	virtual ~Buffer() { }

	bool init(Device &dev, VkBuffer, const BufferInfo &, Rc<DeviceMemory> &&,
			VkDeviceSize memoryOffset);

	VkBuffer getBuffer() const { return _buffer; }
	DeviceMemory *getMemory() const { return _memory; }

	void setPendingBarrier(const BufferMemoryBarrier &);
	const BufferMemoryBarrier *getPendingBarrier() const;
	void dropPendingBarrier();

	bool bindMemory(Rc<DeviceMemory> &&, VkDeviceSize = 0);

	bool map(const Callback<void(uint8_t *, VkDeviceSize)> &, VkDeviceSize offset = 0,
			VkDeviceSize size = maxOf<VkDeviceSize>(),
			DeviceMemoryAccess = DeviceMemoryAccess::Full);

	bool map(const Callback<void(uint8_t *, VkDeviceSize)> &, DeviceMemoryAccess);

	uint8_t *getPersistentMappedRegion(bool invalidate = true);

	void invalidateMappedRegion(VkDeviceSize offset = 0, VkDeviceSize size = maxOf<VkDeviceSize>());
	void flushMappedRegion(VkDeviceSize offset = 0, VkDeviceSize size = maxOf<VkDeviceSize>());

	bool setData(BytesView, VkDeviceSize offset = 0,
			DeviceMemoryAccess = DeviceMemoryAccess::Flush);
	Bytes getData(VkDeviceSize size = maxOf<VkDeviceSize>(), VkDeviceSize offset = 0,
			DeviceMemoryAccess = DeviceMemoryAccess::Invalidate);

	// returns maxOf<uint64_t>() on overflow
	uint64_t reserveBlock(uint64_t blockSize, uint64_t alignment);
	uint64_t getReservedSize() const { return _targetOffset.load(); }

protected:
	using core::BufferObject::init;

	Rc<DeviceMemory> _memory;
	VkDeviceSize _memoryOffset = 0;
	VkBuffer _buffer = VK_NULL_HANDLE;
	std::optional<BufferMemoryBarrier> _barrier;

	std::atomic<uint64_t> _targetOffset = 0;
};

class SP_PUBLIC ImageView : public core::ImageView {
public:
	virtual ~ImageView() { }

	bool init(Device &dev, VkImage, VkFormat format);
	bool init(Device &dev, Image *, const ImageViewInfo &);

	VkImageView getImageView() const { return _imageView; }

protected:
	using core::ImageView::init;

	VkImageView _imageView = VK_NULL_HANDLE;
};

class SP_PUBLIC Sampler : public core::Sampler {
public:
	virtual ~Sampler() { }

	bool init(Device &dev, const SamplerInfo &);

	VkSampler getSampler() const { return _sampler; }

protected:
	using core::Sampler::init;

	VkSampler _sampler = VK_NULL_HANDLE;
};

} // namespace stappler::xenolith::vk

#endif /* XENOLITH_BACKEND_VK_XLVKOBJECT_H_ */
