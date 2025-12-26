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

#include "XLVkAllocator.h"
#include "XLVkInstance.h"
#include "XLVkDevice.h"
#include "XLVkObject.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk {

static uint32_t Allocator_getTypeScoreInternal(const Allocator::MemHeap &heap,
		const Allocator::MemType &type, AllocationUsage usage, VkPhysicalDeviceType devType) {
	switch (usage) {
	case AllocationUsage::DeviceLocal:
	case AllocationUsage::DeviceLocalLazilyAllocated:
		switch (heap.type) {
		case Allocator::MemHeapType::DeviceLocal:
			if (type.isDeviceLocal()) {
				uint32_t ret = 32;
				if (type.isHostVisible()) {
					ret -= 2;
				}
				if (type.isHostCoherent()) {
					ret -= 3;
				}
				if (type.isHostCached()) {
					ret -= 4;
				}
				if (usage == AllocationUsage::DeviceLocalLazilyAllocated
						&& type.isLazilyAllocated()) {
					ret += 12;
				}
				return ret;
			}
			return 0;
			break;
		case Allocator::MemHeapType::DeviceLocalHostVisible:
			if (type.isDeviceLocal()) {
				uint32_t ret = 24;
				if (type.isHostVisible()) {
					ret -= 2;
				}
				if (type.isHostCoherent()) {
					ret -= 3;
				}
				if (type.isHostCached()) {
					ret -= 4;
				}
				if (usage == AllocationUsage::DeviceLocalLazilyAllocated
						&& type.isLazilyAllocated()) {
					ret += 12;
				}
				return ret;
			}
			return 0;
			break;
		case Allocator::MemHeapType::HostLocal: return 0; break;
		}
		break;
	case AllocationUsage::DeviceLocalHostVisible:
		switch (heap.type) {
		case Allocator::MemHeapType::DeviceLocalHostVisible:
			if (type.isDeviceLocal() && type.isHostVisible()) {
				uint32_t ret = 32;
				if (devType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
					if (type.isHostCoherent() && type.isHostCoherent()) {
						ret -= 2;
					} else if (type.isHostCoherent()) {
						ret -= 3;
					} else if (type.isHostCached()) {
						ret -= 4;
					}
				} else {
					if (type.isHostCoherent()) {
						ret -= 3;
					}
					if (type.isHostCached()) {
						ret -= 4;
					}
				}
				return ret;
			}
			return 0;
			break;
		case Allocator::MemHeapType::DeviceLocal: return 0; break;
		case Allocator::MemHeapType::HostLocal:
			if (type.isHostVisible()) {
				return 1;
			}
			return 0;
			break;
		}
		break;
	case AllocationUsage::HostTransitionSource:
		switch (heap.type) {
		case Allocator::MemHeapType::HostLocal:
			if (type.isHostVisible()) {
				uint32_t ret = 32;
				if (type.isHostCoherent()) {
					ret += 3;
				}
				if (type.isHostCached()) {
					ret -= 4;
				}
				return ret;
			}
			return 0;
			break;
		case Allocator::MemHeapType::DeviceLocalHostVisible:
			if (type.isHostVisible()) {
				uint32_t ret = 16;
				if (type.isHostCoherent()) {
					ret += 3;
				}
				if (type.isHostCached()) {
					ret -= 4;
				}
				return ret;
			}
			return 0;
			break;
		case Allocator::MemHeapType::DeviceLocal: return 0; break;
		}
		break;
	case AllocationUsage::HostTransitionDestination:
		switch (heap.type) {
		case Allocator::MemHeapType::HostLocal:
			if (type.isHostVisible()) {
				uint32_t ret = 32;
				if (type.isHostCoherent()) {
					ret -= 3;
				}
				if (type.isHostCached()) {
					ret += 4;
				}
				return ret;
			}
			return 0;
			break;
		case Allocator::MemHeapType::DeviceLocalHostVisible:
			if (type.isHostVisible()) {
				uint32_t ret = 16;
				if (type.isHostCoherent()) {
					ret -= 3;
				}
				if (type.isHostCached()) {
					ret += 4;
				}
				return ret;
			}
			return 0;
			break;
		case Allocator::MemHeapType::DeviceLocal: return 0; break;
		}
		break;
	}
	return 0;
}

Allocator::~Allocator() { }

bool Allocator::init(Device &dev, VkPhysicalDevice device, const DeviceInfo::Features &features,
		const DeviceInfo::Properties &props) {
	_device = &dev;
	_bufferImageGranularity = props.device10.properties.limits.bufferImageGranularity;
	_nonCoherentAtomSize = props.device10.properties.limits.nonCoherentAtomSize;

	if (features.optionals[toInt(OptionalDeviceExtension::GetMemoryRequirements2)]) {
		_hasMemReq2 = true;
	}
	if (features.optionals[toInt(OptionalDeviceExtension::DedicatedAllocation)]) {
		_hasDedicated = true;
	}

	if (features.optionals[toInt(OptionalDeviceExtension::MemoryBudget)]) {
		_memBudget.pNext = nullptr;
		_memProperties.pNext = &_memBudget;
		_hasBudget = true;
	} else {
		_memProperties.pNext = nullptr;
	}

	dev.getInstance()->vkGetPhysicalDeviceMemoryProperties2KHR(device, &_memProperties);

	for (uint32_t i = 0; i < _memProperties.memoryProperties.memoryHeapCount; ++i) {
		auto &it =
				_memHeaps.emplace_back(MemHeap{i, _memProperties.memoryProperties.memoryHeaps[i]});
		if ((it.heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0) {
			it.type = DeviceLocal;
		}
		for (uint32_t j = 0; j < _memProperties.memoryProperties.memoryTypeCount; ++j) {
			if (_memProperties.memoryProperties.memoryTypes[j].heapIndex == i) {
				it.types.emplace_back(MemType{j, _memProperties.memoryProperties.memoryTypes[j]});
				if ((_memProperties.memoryProperties.memoryTypes[j].propertyFlags
							& VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
						!= 0) {
					if (it.type == DeviceLocal) {
						it.type = DeviceLocalHostVisible;
					}
				}
			}
		}

		if (_hasBudget) {
			it.budget = _memBudget.heapBudget[i];
			it.usage = _memBudget.heapUsage[i];
		}
	}

	for (auto &it : _memHeaps) {
		for (auto &jt : it.types) { _memTypes.emplace_back(&jt); }
	}

	std::sort(_memTypes.begin(), _memTypes.end(),
			[](const MemType *l, const MemType *r) { return l->idx < r->idx; });

	if constexpr (s_printVkInfo) {
		StringStream stream;
		stream << "[Memory]\n";
		for (auto &it : _memHeaps) {
			stream << "\t[Heap] " << it.idx << ": " << it.heap.size << " bytes;";
			if (_hasBudget) {
				stream << " Budget: " << it.budget << "; Usage: " << it.usage << ";";
			}
			if ((it.heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0) {
				stream << " DeviceLocal;";
			}
			if ((it.heap.flags & VK_MEMORY_HEAP_MULTI_INSTANCE_BIT) != 0) {
				stream << " MultiInstance;";
			}
			stream << "\n";
			for (auto &jt : it.types) {
				stream << "\t\t[Type] " << jt.idx;
				if ((jt.type.propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0) {
					stream << " DeviceLocal;";
				}
				if ((jt.type.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0) {
					stream << " HostVisible;";
				}
				if ((jt.type.propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0) {
					stream << " HostCoherent;";
				}
				if ((jt.type.propertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) != 0) {
					stream << " HostCached;";
				}
				if ((jt.type.propertyFlags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT) != 0) {
					stream << " LazilyAllocated;";
				}
				if ((jt.type.propertyFlags & VK_MEMORY_PROPERTY_PROTECTED_BIT) != 0) {
					stream << " Protected;";
				}
				if ((jt.type.propertyFlags & VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD) != 0) {
					stream << " DeviceCoherent;";
				}
				if ((jt.type.propertyFlags & VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD) != 0) {
					stream << " DeviceUncached;";
				}
				stream << "\n";
			}
		}
		log::source().verbose("Vk-Info", stream.str());
	}

	return true;
}

void Allocator::invalidate(Device &dev) {
	for (auto &type : _memTypes) {
		for (auto &nodes : type->buf) {
			for (auto &node : nodes) {
				if (node.ptr) {
					_device->getTable()->vkUnmapMemory(_device->getDevice(), node.mem);
					const_cast<MemNode &>(node).ptr = nullptr;
				}
				_device->makeApiCall([&](const DeviceTable &table, VkDevice device) {
					table.vkFreeMemory(device, node.mem, nullptr);
				});
				const_cast<MemNode &>(node).mem = VK_NULL_HANDLE;
			}
		}
	}

	_device = nullptr;
}

void Allocator::update() {
	if (_device && _physicalDevice && _hasBudget) {
		_memBudget.pNext = nullptr;
		_memProperties.pNext = &_memBudget;
		_device->getInstance()->vkGetPhysicalDeviceMemoryProperties2KHR(_physicalDevice,
				&_memProperties);

		for (uint32_t i = 0; i < _memProperties.memoryProperties.memoryHeapCount; ++i) {
			auto &it = _memHeaps[i];
			it.budget = _memBudget.heapBudget[i];
			it.usage = _memBudget.heapUsage[i];
		}
	}
}

uint32_t Allocator::getInitialTypeMask() const {
	uint32_t ret = 0;
	for (size_t i = 0; i < _memProperties.memoryProperties.memoryTypeCount; ++i) { ret |= 1 << i; }
	return ret;
}

const Allocator::MemType *Allocator::getType(uint32_t idx) const {
	if (idx < _memTypes.size()) {
		return _memTypes[idx];
	}
	return nullptr;
}

void Allocator::lock() { _mutex.lock(); }

void Allocator::unlock() { _mutex.unlock(); }

Allocator::MemNode Allocator::alloc(MemType *type, uint64_t in_size, bool persistent) {
	std::unique_lock<Mutex> lock;

	// PageSize boundary should be large enough to match all alignment requirements
	uint64_t size = uint64_t(math::align<uint64_t>(in_size, PageSize));
	if (size < in_size) {
		return MemNode();
	}

	if (size < type->min * PageSize) {
		size = type->min * PageSize;
	}

	uint64_t index = size / PageSize - type->min + 1;

	/* First see if there are any nodes in the area we know
	 * our node will fit into. */
	if (index <= type->last) {
		lock = std::unique_lock<Mutex>(_mutex);
		/* Walk the free list to see if there are
		 * any nodes on it of the requested size */
		uint64_t max_index = type->last;
		auto ref = &type->buf[index];
		uint64_t i = index;
		while (type->buf[i].empty() && i < max_index) {
			ref++;
			i++;
		}

		if (!ref->empty()) {
			auto node = ref->back();
			ref->pop_back();

			if (ref->empty()) {
				// revert `last` value
				do {
					ref--;
					max_index--;
				} while (max_index > 0 && ref->empty());
				type->last = max_index;
			}

			type->current += node.index + (type->min - 1);
			if (type->current > type->max) {
				type->current = type->max;
			}

			if (persistent && !node.ptr) {
				if (_device->getTable()->vkMapMemory(_device->getDevice(), node.mem, 0, node.size,
							0, &node.ptr)
						!= VK_SUCCESS) {
					return MemNode();
				}
			} else if (!persistent && node.ptr) {
				_device->getTable()->vkUnmapMemory(_device->getDevice(), node.mem);
				node.ptr = nullptr;
			}

			return node;
		}
	} else if (!type->buf[0].empty()) {
		lock = std::unique_lock<Mutex>(_mutex);
		/* If we found nothing, seek the sink (at index 0), if
		 * it is not empty. */

		/* Walk the free list to see if there are
		 * any nodes on it of the requested size */

		auto ref = SpanView<MemNode>(type->buf[0]);
		auto it = ref.begin();

		while (it != ref.end() && index > it->index) { ++it; }

		if (it != ref.end()) {
			MemNode node = *it;
			type->buf[0].erase(type->buf[0].begin() + (it - ref.begin()));
			type->current += node.index + (type->min - 1);
			if (type->current > type->max) {
				type->current = type->max;
			}

			if (persistent && !node.ptr) {
				if (_device->getTable()->vkMapMemory(_device->getDevice(), node.mem, 0, node.size,
							0, &node.ptr)
						!= VK_SUCCESS) {
					return MemNode();
				}
			} else if (!persistent && node.ptr) {
				_device->getTable()->vkUnmapMemory(_device->getDevice(), node.mem);
				node.ptr = nullptr;
			}

			return node;
		}
	}

	/* If we haven't got a suitable node, malloc a new one
	 * and initialize it. */
	if (lock.owns_lock()) {
		lock.unlock();
	}

	MemNode ret;
	VkMemoryAllocateInfo allocInfo{};
	VkMemoryAllocateFlagsInfoKHR flagsInfo;

	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = size;
	allocInfo.memoryTypeIndex = type->idx;

	if (_device->hasBufferDeviceAddresses()) {
		flagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO_KHR;
		flagsInfo.pNext = allocInfo.pNext;
		flagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;

		allocInfo.pNext = &flagsInfo;
	}

	VkResult result = VK_ERROR_UNKNOWN;
	_device->makeApiCall([&](const DeviceTable &table, VkDevice device) {
		result = table.vkAllocateMemory(device, &allocInfo, nullptr, &ret.mem);
	});

	if (result != VK_SUCCESS) {
		return MemNode();
	}

	if (persistent && (type->type.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0) {
		if (_device->getTable()->vkMapMemory(_device->getDevice(), ret.mem, 0, size, 0, &ret.ptr)
				!= VK_SUCCESS) {
			_device->makeApiCall([&](const DeviceTable &table, VkDevice device) {
				table.vkFreeMemory(device, ret.mem, nullptr);
			});
			return MemNode();
		}
	}

	ret.index = index;
	ret.size = size;
	ret.offset = 0;
	return ret;
}

void Allocator::free(MemType *type, SpanView<MemNode> nodes) {
	Vector<MemNode> freelist;

	std::unique_lock<Mutex> lock(_mutex);

	uint64_t max_index = type->last;
	uint64_t max_free_index = type->max;
	uint64_t current_free_index = type->current;

	/* Walk the list of submitted nodes and free them one by one,
	 * shoving them in the right 'size' buckets as we go. */

	for (auto &node : nodes) {
		uint64_t index = node.index;

		// do not store large allocations
		if (max_free_index != maxOf<uint64_t>() && index + (type->min - 1) > current_free_index) {
			freelist.emplace_back(node);
		} else if (index < MaxIndex) {
			/* Add the node to the appropriate 'size' bucket.  Adjust
			 * the max_index when appropriate. */
			if (type->buf[index].empty() && index > max_index) {
				max_index = index;
			}
			type->buf[index].emplace_back(node);
			if (current_free_index >= index + (type->min - 1)) {
				current_free_index -= index + (type->min - 1);
			} else {
				current_free_index = 0;
			}
		} else {
			/* This node is too large to keep in a specific size bucket,
			 * just add it to the sink (at index 0).
			 */
			type->buf[0].emplace_back(node);
			if (current_free_index >= index + (type->min - 1)) {
				current_free_index -= index + (type->min - 1);
			} else {
				current_free_index = 0;
			}
		}
	}

	type->last = max_index;
	type->current = current_free_index;

	for (const MemNode &it : freelist) {
		if (it.ptr) {
			_device->getTable()->vkUnmapMemory(_device->getDevice(), it.mem);
			const_cast<MemNode &>(it).ptr = nullptr;
		}
		_device->makeApiCall([&](const DeviceTable &table, VkDevice device) {
			table.vkFreeMemory(device, it.mem, nullptr);
		});
	}
}

Allocator::MemType *Allocator::findMemoryType(uint32_t typeFilter, AllocationUsage type) const {
	// best match
	uint32_t score = 0;
	uint32_t idx = maxOf<uint32_t>();
	for (auto &it : _memTypes) {
		if ((typeFilter & (1 << it->idx))) {
			auto s = Allocator_getTypeScoreInternal(_memHeaps[it->type.heapIndex], *it, type,
					_device->getInfo().properties.device10.properties.deviceType);
			if (s && s > score) {
				score = s;
				idx = it->idx;
			}
		}
	}

	if (idx != maxOf<uint32_t>()) {
		return (Allocator::MemType *)_memTypes[idx];
	}

	auto getTypeName = [&](AllocationUsage t) {
		switch (t) {
		case AllocationUsage::DeviceLocal: return StringView("DeviceLocal"); break;
		case AllocationUsage::DeviceLocalHostVisible:
			return StringView("DeviceLocalHostVisible");
			break;
		case AllocationUsage::HostTransitionDestination:
			return StringView("HostTransitionDestination");
			break;
		case AllocationUsage::HostTransitionSource:
			return StringView("HostTransitionSource");
			break;
		default: break;
		}
		return StringView("Unknown");
	};

	log::source().error("Vk-Error", "Fail to find required memory type for ", getTypeName(type));
	return nullptr;
}

MemoryRequirements Allocator::getBufferMemoryRequirements(VkBuffer target) {
	MemoryRequirements ret;
	VkMemoryRequirements2 memRequirements = {VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2, nullptr};

	if (hasMemReq2Feature() && hasDedicatedFeature()) {
		VkMemoryDedicatedRequirements memDedicatedReqs = {
			VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS};
		memRequirements.pNext = &memDedicatedReqs;

		VkBufferMemoryRequirementsInfo2 info = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2,
			nullptr, target};
		_device->getTable()->vkGetBufferMemoryRequirements2(_device->getDevice(), &info,
				&memRequirements);

		ret.requiresDedicated = memDedicatedReqs.requiresDedicatedAllocation;
		ret.prefersDedicated = memDedicatedReqs.prefersDedicatedAllocation;
		ret.requirements = memRequirements.memoryRequirements;
	} else {
		_device->getTable()->vkGetBufferMemoryRequirements(_device->getDevice(), target,
				&ret.requirements);
	}
	return ret;
}

MemoryRequirements Allocator::getImageMemoryRequirements(VkImage target) {
	MemoryRequirements ret;
	VkMemoryRequirements2 memRequirements = {VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2, nullptr};

	if (hasMemReq2Feature() && hasDedicatedFeature()) {
		VkMemoryDedicatedRequirements memDedicatedReqs = {
			VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS, nullptr};
		memRequirements.pNext = &memDedicatedReqs;

		VkImageMemoryRequirementsInfo2 info = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2,
			nullptr, target};
		_device->getTable()->vkGetImageMemoryRequirements2(_device->getDevice(), &info,
				&memRequirements);

		ret.requiresDedicated = memDedicatedReqs.requiresDedicatedAllocation;
		ret.prefersDedicated = memDedicatedReqs.prefersDedicatedAllocation;
		ret.requirements = memRequirements.memoryRequirements;
	} else {
		_device->getTable()->vkGetImageMemoryRequirements(_device->getDevice(), target,
				&ret.requirements);
	}
	return ret;
}

Rc<Buffer> Allocator::spawnPersistent(AllocationUsage usage, const BufferInfo &info,
		BytesView view) {
	auto target = preallocate(info, view);
	if (!target) {
		return nullptr;
	}

	if (!allocateDedicated(usage, target)) {
		return nullptr;
	}

	if (!view.empty()) {
		if (!target->setData(view, 0)) {
			return nullptr;
		}
	}

	return target;
}

Rc<Image> Allocator::spawnPersistent(AllocationUsage usage, StringView key,
		const core::ImageInfoData &info, bool preinitialized, uint64_t forceId) {
	auto target = preallocate(key, info, preinitialized, forceId);
	if (!target) {
		return nullptr;
	}

	if (!allocateDedicated(usage, target)) {
		return nullptr;
	}

	return target;
}

Rc<Buffer> Allocator::preallocate(const BufferInfo &info, BytesView view) {
	auto usage = info.usage;
	if ((usage & core::BufferUsage::ShaderDeviceAddress) != core::BufferUsage::None) {
		if (!_device->hasBufferDeviceAddresses()) {
			usage &= ~core::BufferUsage::ShaderDeviceAddress;
		}
	}

	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = (view.empty() ? info.size : view.size());
	bufferInfo.flags = VkBufferCreateFlags(info.flags);
	bufferInfo.usage = VkBufferUsageFlags(usage);
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VkBuffer target = VK_NULL_HANDLE;
	if (_device->getTable()->vkCreateBuffer(_device->getDevice(), &bufferInfo, nullptr, &target)
			!= VK_SUCCESS) {
		return nullptr;
	}

	if (bufferInfo.size != info.size) {
		BufferInfo tmpInfo(info);
		tmpInfo.size = bufferInfo.size;
		return Rc<Buffer>::create(*_device, target, tmpInfo, nullptr, 0);
	} else {
		return Rc<Buffer>::create(*_device, target, info, nullptr, 0);
	}
}

Rc<Image> Allocator::preallocate(StringView key, const ImageInfoData &info, bool preinitialized,
		uint64_t forceId) {
	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.pNext = nullptr;
	imageInfo.flags = VkImageCreateFlags(info.flags);
	imageInfo.imageType = VkImageType(info.imageType);
	imageInfo.format = VkFormat(info.format);
	imageInfo.extent = VkExtent3D({info.extent.width, info.extent.height, info.extent.depth});
	imageInfo.mipLevels = info.mipLevels.get();
	imageInfo.arrayLayers = info.arrayLayers.get();
	imageInfo.samples = VkSampleCountFlagBits(info.samples);
	imageInfo.tiling = VkImageTiling(info.tiling);
	imageInfo.usage = VkImageUsageFlags(info.usage);
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (preinitialized) {
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
	} else {
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	}

	VkImage target = VK_NULL_HANDLE;
	if (_device->getTable()->vkCreateImage(_device->getDevice(), &imageInfo, nullptr, &target)
			!= VK_SUCCESS) {
		return nullptr;
	}

	if (forceId) {
		return Rc<Image>::create(*_device, key, forceId, target, info, nullptr);
	} else {
		return Rc<Image>::create(*_device, key, target, info, nullptr);
	}
}

Rc<DeviceMemory> Allocator::emplaceObjects(AllocationUsage usage, SpanView<Image *> images,
		SpanView<Buffer *> buffers) {
	Vector<MemoryRequirements> bufferRequirements;
	bufferRequirements.reserve(buffers.size());
	Vector<MemoryRequirements> imageRequirements;
	imageRequirements.reserve(images.size());

	uint32_t linearObjects = 0;
	uint32_t nonLinearObjects = 0;

	auto mask = getInitialTypeMask();
	bool requiresBufferAdresses = false;

	for (auto &it : buffers) {
		if ((it->getInfo().usage & core::BufferUsage::ShaderDeviceAddress)
				!= core::BufferUsage::None) {
			requiresBufferAdresses = true;
		}

		auto req = getBufferMemoryRequirements(it->getBuffer());
		if (!req.prefersDedicated && !req.requiresDedicated) {
			mask &= req.requirements.memoryTypeBits;
		}
		if (mask == 0) {
			log::source().error("vk::Allocator", "emplaceObjects: fail to find common memory type");
			return nullptr;
		}
		bufferRequirements.emplace_back(req);
		++linearObjects;
	}

	for (auto &it : images) {
		auto req = getImageMemoryRequirements(it->getImage());
		if (!req.prefersDedicated && !req.requiresDedicated) {
			mask &= req.requirements.memoryTypeBits;
		}
		if (mask == 0) {
			log::source().error("vk::Allocator", "emplaceObjects: fail to find common memory type");
			return nullptr;
		}
		imageRequirements.emplace_back(req);

		if (it->getInfo().tiling == core::ImageTiling::Optimal) {
			++nonLinearObjects;
		} else {
			++linearObjects;
		}
	}

	auto allocMemType = findMemoryType(mask, usage);
	if (!allocMemType) {
		log::source().error("vk::Allocator", "emplaceObjects: fail to find memory type");
		return nullptr;
	}

	VkDeviceSize nonCoherentAtomSize = 1;
	if (allocMemType->isHostVisible()) {
		if (!allocMemType->isHostCoherent()) {
			nonCoherentAtomSize = getNonCoherentAtomSize();
		}
	}

	VkDeviceSize requiredMemory = 0;

	uint32_t i = 0;
	if (nonLinearObjects > 0) {
		for (auto &it : images) {
			if (!imageRequirements[i].requiresDedicated && !imageRequirements[i].prefersDedicated) {
				if (it->getInfo().tiling == core::ImageTiling::Optimal) {
					requiredMemory = math::align<VkDeviceSize>(requiredMemory,
							std::max(imageRequirements[i].requirements.alignment,
									nonCoherentAtomSize));
					imageRequirements[i].targetOffset = requiredMemory;
					requiredMemory += imageRequirements[i].requirements.size;
				}
			}
			++i;
		}
	}

	if (nonLinearObjects > 0 && linearObjects > 0) {
		requiredMemory = math::align<VkDeviceSize>(requiredMemory, getBufferImageGranularity());
	}

	if (linearObjects) {
		i = 0;
		for (auto &it : images) {
			if (!imageRequirements[i].requiresDedicated && !imageRequirements[i].prefersDedicated) {
				if (it->getInfo().tiling != core::ImageTiling::Optimal) {
					requiredMemory = math::align<VkDeviceSize>(requiredMemory,
							std::max(imageRequirements[i].requirements.alignment,
									nonCoherentAtomSize));
					imageRequirements[i].targetOffset = requiredMemory;
					requiredMemory += imageRequirements[i].requirements.size;
				}
			}
			++i;
		}

		i = 0;
		for (auto &it : bufferRequirements) {
			if (!it.requiresDedicated && !it.prefersDedicated) {
				requiredMemory = math::align<VkDeviceSize>(requiredMemory,
						std::max(it.requirements.alignment, nonCoherentAtomSize));
				it.targetOffset = requiredMemory;
				requiredMemory += it.requirements.size;
			}
			++i;
		}
	}

	VkDeviceMemory memObject;
	if (requiredMemory > 0) {
		VkMemoryAllocateInfo allocInfo;
		VkMemoryAllocateFlagsInfoKHR flagsInfo;

		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.pNext = nullptr;
		allocInfo.allocationSize = requiredMemory;
		allocInfo.memoryTypeIndex = allocMemType->idx;

		if (_device->hasBufferDeviceAddresses() && requiresBufferAdresses) {
			flagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO_KHR;
			flagsInfo.pNext = allocInfo.pNext;
			flagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;

			allocInfo.pNext = &flagsInfo;
		}

		if (_device->getTable()->vkAllocateMemory(_device->getDevice(), &allocInfo, nullptr,
					&memObject)
				!= VK_SUCCESS) {
			log::source().error("vk::Allocator", "emplaceObjects: fail to allocate memory");
			return nullptr;
		}
	}

	auto memory = Rc<DeviceMemory>::create(this,
			DeviceMemoryInfo{requiredMemory, 1, allocMemType->idx, false}, memObject, usage);

	// bind memory
	if (nonLinearObjects > 0) {
		i = 0;
		for (auto &it : images) {
			if (imageRequirements[i].requiresDedicated || imageRequirements[i].prefersDedicated) {
				if (!allocateDedicated(usage, it)) {
					return nullptr;
				}
			} else {
				if (it->getInfo().tiling == core::ImageTiling::Optimal) {
					it->bindMemory(Rc<DeviceMemory>(memory), imageRequirements[i].targetOffset);
				}
			}
			++i;
		}
	}

	if (linearObjects > 0) {
		i = 0;
		for (auto &it : images) {
			if (!imageRequirements[i].requiresDedicated && !imageRequirements[i].prefersDedicated) {
				if (it->getInfo().tiling != core::ImageTiling::Optimal) {
					it->bindMemory(Rc<DeviceMemory>(memory), imageRequirements[i].targetOffset);
				}
			}
			++i;
		}

		i = 0;
		for (auto &it : buffers) {
			if (bufferRequirements[i].requiresDedicated || bufferRequirements[i].prefersDedicated) {
				if (!allocateDedicated(usage, it)) {
					return nullptr;
				}
			} else {
				it->bindMemory(Rc<DeviceMemory>(memory), bufferRequirements[i].targetOffset);
			}
			++i;
		}
	}
	return memory;
}

bool Allocator::allocateDedicated(AllocationUsage usage, Buffer *target) {
	auto req = getBufferMemoryRequirements(target->getBuffer());
	auto type = findMemoryType(req.requirements.memoryTypeBits, usage);
	if (!type) {
		log::source().error("vk::Allocator", "Buffer: allocateDedicated: Fail to find memory type");
		return false;
	}

	VkDeviceMemory memory;

	VkMemoryAllocateInfo allocInfo;
	VkMemoryDedicatedAllocateInfo dedicatedInfo;
	VkMemoryAllocateFlagsInfoKHR flagsInfo;

	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.pNext = nullptr;
	allocInfo.allocationSize = req.requirements.size;
	allocInfo.memoryTypeIndex = type->idx;

	if (hasDedicatedFeature()) {
		dedicatedInfo.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
		dedicatedInfo.pNext = allocInfo.pNext;
		dedicatedInfo.image = VK_NULL_HANDLE;
		dedicatedInfo.buffer = target->getBuffer();

		allocInfo.pNext = &dedicatedInfo;
	}

	if (_device->hasBufferDeviceAddresses()
			&& (target->getInfo().usage & core::BufferUsage::ShaderDeviceAddress)
					!= core::BufferUsage::None) {
		flagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO_KHR;
		flagsInfo.pNext = allocInfo.pNext;
		flagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;

		allocInfo.pNext = &flagsInfo;
	}

	VkResult result = VK_ERROR_UNKNOWN;
	_device->makeApiCall([&](const DeviceTable &table, VkDevice device) {
		result = table.vkAllocateMemory(device, &allocInfo, nullptr, &memory);
	});

	if (result != VK_SUCCESS) {
		return false;
	}

	target->bindMemory(Rc<DeviceMemory>::create(this,
			DeviceMemoryInfo{req.requirements.size, req.requirements.alignment, type->idx, true},
			memory, usage));
	return true;
}

bool Allocator::allocateDedicated(AllocationUsage usage, Image *target) {
	auto req = getImageMemoryRequirements(target->getImage());
	auto type = findMemoryType(req.requirements.memoryTypeBits, usage);
	if (!type) {
		log::source().error("vk::Allocator", "Image: allocateDedicated: Fail to find memory type");
		return false;
	}

	VkDeviceMemory memory;

	VkMemoryAllocateInfo allocInfo;
	VkMemoryDedicatedAllocateInfo dedicatedInfo;

	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.pNext = nullptr;
	allocInfo.allocationSize = req.requirements.size;
	allocInfo.memoryTypeIndex = type->idx;

	if (hasDedicatedFeature()) {
		dedicatedInfo.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
		dedicatedInfo.pNext = allocInfo.pNext;
		dedicatedInfo.image = target->getImage();
		dedicatedInfo.buffer = VK_NULL_HANDLE;

		allocInfo.pNext = &dedicatedInfo;
	}

	VkResult result = VK_ERROR_UNKNOWN;
	_device->makeApiCall([&](const DeviceTable &table, VkDevice device) {
		result = table.vkAllocateMemory(device, &allocInfo, nullptr, &memory);
	});
	if (result != VK_SUCCESS) {
		log::source().error("vk::Allocator",
				"Image: allocateDedicated: Fail to allocate memory for dedicated allocation");
		return false;
	}

	target->bindMemory(Rc<DeviceMemory>::create(this,
			DeviceMemoryInfo{req.requirements.size, req.requirements.alignment, type->idx, true},
			memory, usage));
	return true;
}

DeviceMemoryPool::~DeviceMemoryPool() {
	for (auto &it : _mappingProtection) { delete it.second; }

	if (_allocator) {
		for (auto &it : _buffers) { it->invalidate(); }
		_buffers.clear();
		for (auto &it : _images) { it->invalidate(); }
		_images.clear();
		for (auto &it : _heaps) { clear(&it.second); }
	}
}

bool DeviceMemoryPool::init(const Rc<Allocator> &alloc, bool persistentMapping) {
	_allocator = alloc;
	_persistentMapping = persistentMapping;
	return true;
}

Rc<Buffer> DeviceMemoryPool::spawn(AllocationUsage type, const BufferInfo &info) {
	auto buffer = _allocator->preallocate(info);
	auto requirements = _allocator->getBufferMemoryRequirements(buffer->getBuffer());

	if (requirements.requiresDedicated) {
		// TODO: deal with dedicated allocations
		log::source().error("DeviceMemoryPool", "Dedicated allocation required");
	} else {
		auto memType = _allocator->findMemoryType(requirements.requirements.memoryTypeBits, type);
		if (!memType) {
			return nullptr;
		}

		std::unique_lock<Mutex> lock(_mutex);
		MemData *pool = nullptr;
		auto it = _heaps.find(memType->idx);
		if (it == _heaps.end()) {
			pool = &_heaps.emplace(memType->idx, MemData{memType}).first->second;
		} else {
			pool = &it->second;
		}

		if (auto mem = alloc(pool, requirements.requirements.size,
					requirements.requirements.alignment, AllocationType::Linear, type)) {
			if (buffer->bindMemory(Rc<DeviceMemory>::create(this, move(mem), type))) {
				_buffers.emplace_front(buffer);
				return buffer;
			} else {
				log::source().error("DeviceMemoryPool", "Fail to bind memory for buffer: ", type);
			}
		} else {
			log::source().error("DeviceMemoryPool", "Fail to allocate memory for buffer: ", type);
		}
	}
	return nullptr;
}

Rc<Image> DeviceMemoryPool::spawn(AllocationUsage type, StringView key, const ImageInfoData &data) {
	auto image = _allocator->preallocate(key, data, false);
	auto requirements = _allocator->getImageMemoryRequirements(image->getImage());

	if (requirements.requiresDedicated) {
		// TODO: deal with dedicated allocations
		log::source().error("DeviceMemoryPool", "Dedicated allocation required");
	} else {
		auto memType = _allocator->findMemoryType(requirements.requirements.memoryTypeBits, type);
		if (!memType) {
			return nullptr;
		}

		std::unique_lock<Mutex> lock(_mutex);
		MemData *pool = nullptr;
		auto it = _heaps.find(memType->idx);
		if (it == _heaps.end()) {
			pool = &_heaps.emplace(memType->idx, MemData{memType}).first->second;
		} else {
			pool = &it->second;
		}

		if (auto mem = alloc(pool, requirements.requirements.size,
					requirements.requirements.alignment,
					(data.tiling == core::ImageTiling::Optimal) ? AllocationType::Optimal
																: AllocationType::Linear,
					type)) {
			if (image->bindMemory(Rc<DeviceMemory>::create(this, move(mem), type))) {
				_images.emplace_front(image);
				return image;
			} else {
				log::source().error("DeviceMemoryPool", "Fail to bind memory for image");
			}
		} else {
			log::source().error("DeviceMemoryPool", "Fail to allocate memory for image: ", type);
		}
	}

	return nullptr;
}

Rc<Buffer> DeviceMemoryPool::spawnPersistent(AllocationUsage usage, const BufferInfo &info) {
	return _allocator->spawnPersistent(usage, info);
}

Device *DeviceMemoryPool::getDevice() const { return _allocator->getDevice(); }

Allocator::MemBlock DeviceMemoryPool::alloc(MemData *mem, VkDeviceSize in_size,
		VkDeviceSize alignment, AllocationType allocType, AllocationUsage type) {
	if (allocType == AllocationType::Unknown) {
		return Allocator::MemBlock();
	}

	auto size = math::align<VkDeviceSize>(in_size, alignment);

	Allocator::MemNode *node = nullptr;

	// try unused blocks
	Allocator::MemBlock block = tryReuse(mem, in_size, alignment, allocType);
	if (block.mem) {
		return block;
	}

	size_t alignedOffset = 0;
	for (auto &it : mem->mem) {
		alignedOffset = math::align<VkDeviceSize>(it.offset, alignment);

		if (mem->type->isHostVisible() && !mem->type->isHostCoherent()) {
			alignedOffset =
					math::align<VkDeviceSize>(alignedOffset, _allocator->getNonCoherentAtomSize());
		}

		if (it.lastAllocation != allocType && it.lastAllocation != AllocationType::Unknown) {
			alignedOffset = math::align<VkDeviceSize>(alignedOffset,
					_allocator->getBufferImageGranularity());
		}

		if (alignedOffset + size < it.size) {
			node = &it;
			break;
		}
	}

	if (!node) {
		size_t reqSize = size;
		auto b = _allocator->alloc(mem->type, reqSize,
				(type == AllocationUsage::DeviceLocal) ? false : _persistentMapping);
		mem->mem.emplace_back(b);
		node = &mem->mem.back();
		node->mappingProtection = _mappingProtection.emplace(node->mem, new Mutex()).first->second;
		alignedOffset = 0;
	}

	if (node && node->mem) {
		node->offset = alignedOffset + size;
		node->lastAllocation = allocType;
		return Allocator::MemBlock({node->mem, alignedOffset, size, mem->type->idx, node->ptr,
			node->mappingProtection, allocType});
	}

	return block;
}

void DeviceMemoryPool::free(Allocator::MemBlock &&block) {
	auto it = _heaps.find(block.type);
	if (it != _heaps.end()) {
		it->second.freed.emplace_back(move(block));
	}
}

void DeviceMemoryPool::clear(MemData *mem) {
	_allocator->free(mem->type, mem->mem);
	mem->mem.clear();
	mem->freed.clear();
}

Allocator::MemBlock DeviceMemoryPool::tryReuse(MemData *mem, VkDeviceSize size,
		VkDeviceSize alignment, AllocationType allocType) {
	auto targetAlignment = std::countr_zero(alignment);

	auto fIt = mem->freed.begin();
	while (fIt != mem->freed.end()) {
		auto sourceAlignment = std::countr_zero(fIt->offset);
		if (fIt->allocType == allocType && sourceAlignment >= targetAlignment
				&& fIt->size >= size) {
			auto ret = std::move(*fIt);
			mem->freed.erase(fIt);
			return ret;
		}
	}

	return Allocator::MemBlock();
}

} // namespace stappler::xenolith::vk
