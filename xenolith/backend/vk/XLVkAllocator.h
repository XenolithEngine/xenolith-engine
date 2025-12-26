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

#ifndef XENOLITH_BACKEND_VK_XLVKALLOCATOR_H_
#define XENOLITH_BACKEND_VK_XLVKALLOCATOR_H_

#include "XLVkInfo.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk {

class Device;
class DeviceMemory;
class Image;
class Buffer;

enum class AllocationUsage {
	DeviceLocal, // device local only
	DeviceLocalHostVisible, // device local, visible directly on host
	HostTransitionSource, // host-local, used as source for transfer to GPU device (so, non-cached, coherent preferable)
	HostTransitionDestination, // host-local, used as destination for transfer from GPU Device (cached, non-coherent)
	DeviceLocalLazilyAllocated, // memory for transient images
};

enum class AllocationType {
	Unknown,
	Linear,
	Optimal,
};

struct SP_PUBLIC MemoryRequirements {
	VkMemoryRequirements requirements;
	VkDeviceSize targetOffset;
	bool prefersDedicated = false;
	bool requiresDedicated = false;
};

class SP_PUBLIC Allocator : public Ref {
public:
	static constexpr uint64_t PageSize = 8_MiB;
	static constexpr uint64_t MaxIndex = 20; // Largest block
	static constexpr uint64_t PreservePages = 20;

	enum MemHeapType {
		HostLocal,
		DeviceLocal,
		DeviceLocalHostVisible,
	};

	struct MemHeap;

	// slice of device memory
	struct MemNode {
		uint64_t index = 0; // size in pages
		VkDeviceMemory mem = VK_NULL_HANDLE; // device mem block
		VkDeviceSize size = 0; // size in bytes
		VkDeviceSize offset = 0; // current usage offset
		AllocationType lastAllocation =
				AllocationType::Unknown; // last allocation type (for bufferImageGranularity)
		void *ptr = nullptr;
		Mutex *mappingProtection = nullptr;

		explicit operator bool() const { return mem != VK_NULL_HANDLE; }

		size_t getFreeSpace() const { return size - offset; }
	};

	// Memory block, allocated from node as suballocation
	struct MemBlock {
		VkDeviceMemory mem = VK_NULL_HANDLE; // device mem block
		VkDeviceSize offset = 0; // offset in node
		VkDeviceSize size = 0; // reserved size after offset
		uint32_t type = 0; // memory type index
		void *ptr = nullptr;
		Mutex *mappingProtection = nullptr;
		AllocationType allocType = AllocationType::Unknown;

		explicit operator bool() const { return mem != VK_NULL_HANDLE; }
	};

	struct MemType {
		uint32_t idx;
		VkMemoryType type;
		uint64_t min = 2; // in PageSize
		uint64_t last = 0; // largest used index into free
		uint64_t max = PreservePages; // Pages to preserve, 0 - do not preserve
		uint64_t current = PreservePages; // current allocated size in BOUNDARY_SIZE
		std::array<Vector<MemNode>, MaxIndex> buf;

		bool isDeviceLocal() const {
			return (type.propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0;
		}
		bool isHostVisible() const {
			return (type.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0;
		}
		bool isHostCoherent() const {
			return (type.propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0;
		}
		bool isHostCached() const {
			return (type.propertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) != 0;
		}
		bool isLazilyAllocated() const {
			return (type.propertyFlags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT) != 0;
		}
		bool isProtected() const {
			return (type.propertyFlags & VK_MEMORY_PROPERTY_PROTECTED_BIT) != 0;
		}
	};

	struct MemHeap {
		uint32_t idx;
		VkMemoryHeap heap;
		Vector<MemType> types;
		MemHeapType type = HostLocal;
		VkDeviceSize budget = 0;
		VkDeviceSize usage = 0;
		VkDeviceSize currentUsage = 0;
	};

	virtual ~Allocator();

	bool init(Device &dev, VkPhysicalDevice device, const DeviceInfo::Features &features,
			const DeviceInfo::Properties &props);
	void invalidate(Device &dev);

	void update();

	uint32_t getInitialTypeMask() const;
	const Vector<MemHeap> &getMemHeaps() const { return _memHeaps; }
	Device *getDevice() const { return _device; }

	bool hasBudgetFeature() const { return _hasBudget; }
	bool hasMemReq2Feature() const { return _hasMemReq2; }
	bool hasDedicatedFeature() const { return _hasDedicated; }
	VkDeviceSize getBufferImageGranularity() const { return _bufferImageGranularity; }
	VkDeviceSize getNonCoherentAtomSize() const { return _nonCoherentAtomSize; }

	Mutex &getMutex() const { return _mutex; }

	const MemType *getType(uint32_t) const;

	MemType *findMemoryType(uint32_t typeFilter, AllocationUsage) const;

	MemoryRequirements getBufferMemoryRequirements(VkBuffer target);
	MemoryRequirements getImageMemoryRequirements(VkImage target);

	Rc<Buffer> spawnPersistent(AllocationUsage, const BufferInfo &, BytesView = BytesView());
	Rc<Image> spawnPersistent(AllocationUsage, StringView, const ImageInfoData &,
			bool preinitialized, uint64_t forceId = 0);

	Rc<Buffer> preallocate(const BufferInfo &, BytesView = BytesView());
	Rc<Image> preallocate(StringView key, const ImageInfoData &, bool preinitialized,
			uint64_t forceId = 0);

	Rc<DeviceMemory> emplaceObjects(AllocationUsage usage, SpanView<Image *>, SpanView<Buffer *>);

protected:
	friend class DeviceMemoryPool;

	void lock();
	void unlock();

	MemNode alloc(MemType *, uint64_t, bool persistent = false);
	void free(MemType *, SpanView<MemNode>);

	bool allocateDedicated(AllocationUsage usage, Buffer *);
	bool allocateDedicated(AllocationUsage usage, Image *);

	mutable Mutex _mutex;
	VkPhysicalDevice _physicalDevice = VK_NULL_HANDLE;
	Device *_device = nullptr;
	VkPhysicalDeviceMemoryBudgetPropertiesEXT _memBudget = {
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT};
	VkPhysicalDeviceMemoryProperties2KHR _memProperties = {
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2_KHR};
	Vector<MemHeap> _memHeaps;
	Vector<const MemType *> _memTypes;

	VkDeviceSize _bufferImageGranularity = 1;
	VkDeviceSize _nonCoherentAtomSize = 1;
	bool _hasBudget = false;
	bool _hasMemReq2 = false;
	bool _hasDedicated = false;
};

class SP_PUBLIC DeviceMemoryPool : public Ref {
public:
	struct MemData {
		Allocator::MemType *type = nullptr;
		Vector<Allocator::MemNode> mem;
		Vector<Allocator::MemBlock> freed;
	};

	virtual ~DeviceMemoryPool();

	bool init(const Rc<Allocator> &, bool persistentMapping = false);

	Rc<Buffer> spawn(AllocationUsage type, const BufferInfo &);
	Rc<Image> spawn(AllocationUsage type, StringView, const ImageInfoData &);

	Rc<Buffer> spawnPersistent(AllocationUsage, const BufferInfo &);

	Device *getDevice() const;
	Allocator *getAllocator() const { return _allocator; }

	Mutex &getMutex() { return _mutex; }

	Allocator::MemBlock alloc(MemData *, VkDeviceSize size, VkDeviceSize alignment,
			AllocationType allocType, AllocationUsage type);
	void free(Allocator::MemBlock &&);

protected:
	void clear(MemData *);

	Allocator::MemBlock tryReuse(MemData *, VkDeviceSize size, VkDeviceSize alignment,
			AllocationType allocType);

	Mutex _mutex;
	bool _persistentMapping = false;
	Rc<Allocator> _allocator;
	Map<int64_t, MemData> _heaps;
	Map<VkDeviceMemory, Mutex *> _mappingProtection;
	std::forward_list<Rc<Buffer>> _buffers;
	std::forward_list<Rc<Image>> _images;
};

} // namespace stappler::xenolith::vk

namespace std {

inline std::ostream &operator<<(std::ostream &stream,
		STAPPLER_VERSIONIZED_NAMESPACE::xenolith::vk::AllocationUsage usage) {
	switch (usage) {
	case STAPPLER_VERSIONIZED_NAMESPACE::xenolith::vk::AllocationUsage::DeviceLocal:
		stream << "DeviceLocal";
		break;
	case STAPPLER_VERSIONIZED_NAMESPACE::xenolith::vk::AllocationUsage::DeviceLocalHostVisible:
		stream << "DeviceLocalHostVisible";
		break;
	case STAPPLER_VERSIONIZED_NAMESPACE::xenolith::vk::AllocationUsage::DeviceLocalLazilyAllocated:
		stream << "DeviceLocalLazilyAllocated";
		break;
	case STAPPLER_VERSIONIZED_NAMESPACE::xenolith::vk::AllocationUsage::HostTransitionSource:
		stream << "HostTransitionSource";
		break;
	case STAPPLER_VERSIONIZED_NAMESPACE::xenolith::vk::AllocationUsage::HostTransitionDestination:
		stream << "HostTransitionDestination";
		break;
	}
	return stream;
}

} // namespace std

#endif /* XENOLITH_BACKEND_VK_XLVKALLOCATOR_H_ */
