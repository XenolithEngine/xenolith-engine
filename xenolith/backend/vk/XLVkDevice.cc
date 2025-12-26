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

#include "XLVkDevice.h"
#include "XLVk.h"
#include "XLVkPipeline.h"
#include "XLVkTextureSet.h"
#include "XLVkLoop.h"
#include "XLVkAllocator.h"
#include "XLCoreFrameHandle.h"
#include "XLCoreFrameRequest.h"
#include "XLVkRenderPass.h"
#include "SPEventLooper.h"

#ifndef XL_VKDEVICE_LOG
#define XL_VKDEVICE_LOG(...)
#endif

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk {

class ReadImageTask : public core::DeviceQueueTask {
public:
	virtual ~ReadImageTask() = default;

	bool init(const Rc<Image> &img, core::AttachmentLayout l,
			Function<void(const ImageInfoData &, BytesView)> &&cb) {
		if (!DeviceQueueTask::init(vk::getQueueFlags(img->getInfo().type))) {
			return false;
		}

		_layout = l;
		_image = img;
		_callback = sp::move(cb);

		return true;
	}

	virtual bool handleQueueAcquired(core::Device &dev, core::DeviceQueue &queue) override {
		_mempool = Rc<DeviceMemoryPool>::create(static_cast<Device &>(dev).getAllocator(), true);

		auto &info = _image->getInfo();
		auto &extent = info.extent;

		_transferBuffer = _mempool->spawn(AllocationUsage::HostTransitionDestination,
				BufferInfo(core::ForceBufferUsage(core::BufferUsage::TransferDst),
						size_t(extent.width * extent.height * extent.depth
								* core::getFormatBlockSize(info.format)),
						_image->getInfo().type));
		return true;
	}

	virtual void fillCommandBuffer(core::Device &dev, core::CommandBuffer &cbuf) override {
		auto &buf = static_cast<CommandBuffer &>(cbuf);

		auto inImageBarrier =
				ImageMemoryBarrier(_image, VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
						VkImageLayout(_layout), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

		buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
				0, makeSpanView(&inImageBarrier, 1));

		buf.cmdCopyImageToBuffer(_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, _transferBuffer, 0);

		BufferMemoryBarrier bufferOutBarrier(_transferBuffer, VK_ACCESS_TRANSFER_WRITE_BIT,
				VK_ACCESS_HOST_READ_BIT);

		buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0,
				makeSpanView(&bufferOutBarrier, 1));
	}

	virtual void handleComplete(bool success) override {
		if (success) {
			_transferBuffer->map([&](uint8_t *buf, VkDeviceSize size) {
				_callback(_image->getInfo(), BytesView(buf, size));
			});
		} else {
			_callback(_image->getInfo(), BytesView());
		}
	}

protected:
	core::AttachmentLayout _layout = core::AttachmentLayout::Undefined;
	Rc<DeviceMemoryPool> _mempool;
	Rc<Buffer> _transferBuffer;
	Rc<Image> _image;
	Function<void(const ImageInfoData &, BytesView)> _callback;
};

class ReadBufferTask : public core::DeviceQueueTask {
public:
	virtual ~ReadBufferTask() = default;

	bool init(const Rc<Buffer> &buf, Function<void(const BufferInfo &, BytesView)> &&cb) {
		if (!DeviceQueueTask::init(vk::getQueueFlags(buf->getInfo().type))) {
			return false;
		}

		_buffer = buf;
		_callback = sp::move(cb);

		return true;
	}

	virtual bool handleQueueAcquired(core::Device &dev, core::DeviceQueue &queue) override {
		_mempool = Rc<DeviceMemoryPool>::create(static_cast<Device &>(dev).getAllocator(), true);

		auto &info = _buffer->getInfo();

		_transferBuffer = _mempool->spawn(AllocationUsage::HostTransitionDestination,
				BufferInfo(core::ForceBufferUsage(core::BufferUsage::TransferDst),
						size_t(info.size), info.type));

		return true;
	}

	virtual void fillCommandBuffer(core::Device &dev, core::CommandBuffer &buf) override {
		static_cast<CommandBuffer &>(buf).cmdCopyBuffer(_buffer, _transferBuffer);
	}

	virtual void handleComplete(bool success) override {
		if (success) {
			_transferBuffer->map([&](uint8_t *buf, VkDeviceSize size) {
				_callback(_buffer->getInfo(), BytesView(buf, size));
			});
		} else {
			_callback(_buffer->getInfo(), BytesView());
		}
	}

protected:
	Rc<DeviceMemoryPool> _mempool;
	Rc<Buffer> _transferBuffer;
	Rc<Buffer> _buffer;
	Function<void(const BufferInfo &, BytesView)> _callback;
};

DeviceFrameHandle::~DeviceFrameHandle() {
	if (!_valid) {
#if XL_VK_FINALIZE_INVALID_FRAMES
		auto dev = (Device *)_device;
		dev->getTable()->vkDeviceWaitIdle(dev->getDevice());
#endif
	}
	_memPools.clear();
}

bool DeviceFrameHandle::init(Loop &loop, Device &device, Rc<FrameRequest> &&req, uint64_t gen) {
	if (!core::FrameHandle::init(loop, device, move(req), gen)) {
		return false;
	}

	_allocator = device.getAllocator();
	return true;
}

DeviceMemoryPool *DeviceFrameHandle::getMemPool(void *key) {
	std::unique_lock<Mutex> lock(_mutex);
	// experimental: multiple pools feature is disabled, advanced memory mapping protection can replace it completely
	auto v = _memPools.find((void *)nullptr);
	if (v == _memPools.end()) {
		v = _memPools
					.emplace((void *)nullptr,
							Rc<DeviceMemoryPool>::create(_allocator,
									_request->isPersistentMapping()))
					.first;
	}
	return v->second;
}

Device::Device() { }

Device::~Device() {
	if (_vkInstance && _device) {
		if (_allocator) {
			_allocator->invalidate(*this);
			_allocator = nullptr;
		}

		clearShaders();
		invalidateObjects();

		_table->vkDestroyDevice(_device, nullptr);
		delete _table;

		_device = nullptr;
		_table = nullptr;
	}
	XL_VKDEVICE_LOG("~Device");
}

bool Device::init(const vk::Instance *inst, DeviceInfo &&info, const Features &features,
		const Vector<StringView> &extensions) {
	Set<uint32_t> uniqueQueueFamilies = {info.graphicsFamily.index, info.presentFamily.index,
		info.transferFamily.index, info.computeFamily.index};

	auto emplaceQueueFamily = [&, this](DeviceInfo::QueueFamilyInfo &info, uint32_t count,
									  core::QueueFlags preferred) {
		for (auto &it : _families) {
			if (it.index == info.index) {
				it.preferred |= preferred;
				it.count = std::min(it.count + count,
						std::min(info.count, uint32_t(std::thread::hardware_concurrency())));
				return;
			}
		}
		count = std::min(count,
				std::min(info.count, uint32_t(std::thread::hardware_concurrency())));
		_families.emplace_back(core::DeviceQueueFamily({info.index, count, preferred, info.flags,
			info.timestampValidBits, info.minImageTransferGranularity}));
	};

	_presentMask = info.presentFamily.presentSurfaceMask;

	info.presentFamily.count = 1;

	emplaceQueueFamily(info.graphicsFamily, std::thread::hardware_concurrency(),
			core::QueueFlags::Graphics);
	emplaceQueueFamily(info.presentFamily, 1, core::QueueFlags::Present);
	emplaceQueueFamily(info.transferFamily, 2, core::QueueFlags::Transfer);
	emplaceQueueFamily(info.computeFamily, std::thread::hardware_concurrency(),
			core::QueueFlags::Compute);

	if (!setup(inst, info.device, info.properties, _families, features, extensions)) {
		return false;
	}

	if (!core::Device::init(inst)) {
		return false;
	}

	_vkInstance = inst;
	_info = move(info);

	if constexpr (s_printVkInfo) {
		log::source().verbose("Vk-Info", "Device info:\n", info.description());
	}

	for (auto &it : _families) {
		it.queues.reserve(it.count);
		it.pools.reserve(it.count);
		for (uint32_t i = 0; i < it.count; ++i) {
			VkQueue queue = VK_NULL_HANDLE;
			getTable()->vkGetDeviceQueue(_device, it.index, i, &queue);

			it.queues.emplace_back(Rc<DeviceQueue>::create(*this, queue, it.index, it.flags));
			it.pools.emplace_back(Rc<CommandPool>::create(*this, it.index, it.preferred));
		}
	}

	_allocator = Rc<Allocator>::create(*this, _info.device, _info.features, _info.properties);

	do {
		VkFormatProperties properties;

		auto addDepthFormat = [&, this](VkFormat fmt) {
			_vkInstance->vkGetPhysicalDeviceFormatProperties(_info.device, fmt, &properties);
			_formats.emplace(fmt, properties);
			if ((properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
					!= 0) {
				_depthFormats.emplace_back(core::ImageFormat(fmt));
			}
		};

		auto addColorFormat = [&, this](VkFormat fmt) {
			_vkInstance->vkGetPhysicalDeviceFormatProperties(_info.device, fmt, &properties);
			_formats.emplace(fmt, properties);
			if ((properties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) != 0
					&& (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_TRANSFER_DST_BIT)
							!= 0) {
				_colorFormats.emplace_back(core::ImageFormat(fmt));
			}
		};

		addDepthFormat(VK_FORMAT_D16_UNORM);
		addDepthFormat(VK_FORMAT_X8_D24_UNORM_PACK32);
		addDepthFormat(VK_FORMAT_D32_SFLOAT);
		addDepthFormat(VK_FORMAT_S8_UINT);
		addDepthFormat(VK_FORMAT_D16_UNORM_S8_UINT);
		addDepthFormat(VK_FORMAT_D24_UNORM_S8_UINT);
		addDepthFormat(VK_FORMAT_D32_SFLOAT_S8_UINT);

		addColorFormat(VK_FORMAT_R8_UNORM);
		addColorFormat(VK_FORMAT_R8G8_UNORM);
		addColorFormat(VK_FORMAT_R8G8B8_UNORM);
		addColorFormat(VK_FORMAT_R8G8B8A8_UNORM);
	} while (0);

	return true;
}

VkPhysicalDevice Device::getPhysicalDevice() const { return _info.device; }

void Device::end() {
	for (auto &it : _families) {
		for (auto &b : it.pools) { b->invalidate(); }
		it.queries.clear();
		it.pools.clear();
	}

	core::Device::end();
}

#if VK_HOOK_DEBUG
static thread_local uint64_t s_vkFnCallStart = 0;
#endif

const DeviceTable *Device::getTable() const {
#if VK_HOOK_DEBUG
	setDeviceHookThreadContext([](void *ctx, const char *name, PFN_vkVoidFunction fn) {
		s_vkFnCallStart = platform::device::_clock();
	}, [](void *ctx, const char *name, PFN_vkVoidFunction fn) {
		auto dt = platform::device::_clock() - s_vkFnCallStart;
		if (dt > 200'000) {
			log::source().debug("Vk-Call-Timeout", name, ": ", dt);
		}
	}, _original, nullptr, (void *)this);
#endif

	return _table;
}

bool Device::hasExtension(OptionalDeviceExtension ext) const {
	return _info.features.optionals.test(toInt(ext));
}

core::DescriptorFlags Device::getSupportedDescriptorFlags(DescriptorType type) const {
	if (!_useDescriptorIndexing) {
		return core::DescriptorFlags::None;
	}
	core::DescriptorFlags flags = core::DescriptorFlags::None;
	if (_info.features.deviceDescriptorIndexing.descriptorBindingPartiallyBound) {
		flags |= core::DescriptorFlags::PartiallyBound;
	}
	if (_info.features.deviceDescriptorIndexing.descriptorBindingUpdateUnusedWhilePending) {
		flags |= core::DescriptorFlags::UpdateWhilePending;
	}
	if (_info.features.deviceDescriptorIndexing.descriptorBindingVariableDescriptorCount) {
		flags |= core::DescriptorFlags::VariableDescriptorCount;
	}
	if (_info.features.deviceDescriptorIndexing.runtimeDescriptorArray) {
		flags |= core::DescriptorFlags::RuntimeDescriptorArray;
	}

	switch (type) {
	case DescriptorType::Sampler: break;
	case DescriptorType::CombinedImageSampler:
		if (_info.features.device10.features.shaderSampledImageArrayDynamicIndexing) {
			flags |= core::DescriptorFlags::DynamicIndexing;
		}
		if (_info.features.deviceDescriptorIndexing.shaderSampledImageArrayNonUniformIndexing) {
			flags |= core::DescriptorFlags::NonUniformIndexing;
		}
		if (_info.properties.deviceDescriptorIndexing
						.shaderSampledImageArrayNonUniformIndexingNative) {
			flags |= core::DescriptorFlags::NonUniformIndexingNative;
		}
		if (_info.features.deviceDescriptorIndexing.descriptorBindingSampledImageUpdateAfterBind) {
			flags |= core::DescriptorFlags::UpdateAfterBind;
		}
		break;
	case DescriptorType::SampledImage:
		if (_info.features.device10.features.shaderSampledImageArrayDynamicIndexing) {
			flags |= core::DescriptorFlags::DynamicIndexing;
		}
		if (_info.features.deviceDescriptorIndexing.shaderSampledImageArrayNonUniformIndexing) {
			flags |= core::DescriptorFlags::NonUniformIndexing;
		}
		if (_info.properties.deviceDescriptorIndexing
						.shaderSampledImageArrayNonUniformIndexingNative) {
			flags |= core::DescriptorFlags::NonUniformIndexingNative;
		}
		if (_info.features.deviceDescriptorIndexing.descriptorBindingSampledImageUpdateAfterBind) {
			flags |= core::DescriptorFlags::UpdateAfterBind;
		}
		break;
	case DescriptorType::StorageImage:
		if (_info.features.device10.features.shaderStorageImageArrayDynamicIndexing) {
			flags |= core::DescriptorFlags::DynamicIndexing;
		}
		if (_info.features.deviceDescriptorIndexing.shaderStorageImageArrayNonUniformIndexing) {
			flags |= core::DescriptorFlags::NonUniformIndexing;
		}
		if (_info.properties.deviceDescriptorIndexing
						.shaderStorageImageArrayNonUniformIndexingNative) {
			flags |= core::DescriptorFlags::NonUniformIndexingNative;
		}
		if (_info.features.deviceDescriptorIndexing.descriptorBindingStorageImageUpdateAfterBind) {
			flags |= core::DescriptorFlags::UpdateAfterBind;
		}
		break;
	case DescriptorType::UniformTexelBuffer:
		if (_info.features.deviceDescriptorIndexing.shaderUniformTexelBufferArrayDynamicIndexing) {
			flags |= core::DescriptorFlags::DynamicIndexing;
		}
		if (_info.features.deviceDescriptorIndexing
						.shaderUniformTexelBufferArrayNonUniformIndexing) {
			flags |= core::DescriptorFlags::NonUniformIndexing;
		}
		if (_info.features.deviceDescriptorIndexing
						.descriptorBindingUniformTexelBufferUpdateAfterBind) {
			flags |= core::DescriptorFlags::UpdateAfterBind;
		}
		break;
	case DescriptorType::StorageTexelBuffer:
		if (_info.features.deviceDescriptorIndexing.shaderStorageTexelBufferArrayDynamicIndexing) {
			flags |= core::DescriptorFlags::DynamicIndexing;
		}
		if (_info.features.deviceDescriptorIndexing
						.shaderStorageTexelBufferArrayNonUniformIndexing) {
			flags |= core::DescriptorFlags::NonUniformIndexing;
		}
		if (_info.features.deviceDescriptorIndexing
						.descriptorBindingStorageTexelBufferUpdateAfterBind) {
			flags |= core::DescriptorFlags::UpdateAfterBind;
		}
		break;
	case DescriptorType::UniformBuffer:
	case DescriptorType::UniformBufferDynamic:
		if (_info.features.device10.features.shaderUniformBufferArrayDynamicIndexing) {
			flags |= core::DescriptorFlags::DynamicIndexing;
		}
		if (_info.features.deviceDescriptorIndexing.shaderUniformBufferArrayNonUniformIndexing) {
			flags |= core::DescriptorFlags::NonUniformIndexing;
		}
		if (_info.properties.deviceDescriptorIndexing
						.shaderUniformBufferArrayNonUniformIndexingNative) {
			flags |= core::DescriptorFlags::NonUniformIndexingNative;
		}
		if (_info.features.deviceDescriptorIndexing.descriptorBindingUniformBufferUpdateAfterBind) {
			flags |= core::DescriptorFlags::UpdateAfterBind;
		}
		break;
	case DescriptorType::StorageBuffer:
	case DescriptorType::StorageBufferDynamic:
		if (_info.features.device10.features.shaderStorageBufferArrayDynamicIndexing) {
			flags |= core::DescriptorFlags::DynamicIndexing;
		}
		if (_info.features.deviceDescriptorIndexing.shaderStorageBufferArrayNonUniformIndexing) {
			flags |= core::DescriptorFlags::NonUniformIndexing;
		}
		if (_info.properties.deviceDescriptorIndexing
						.shaderStorageBufferArrayNonUniformIndexingNative) {
			flags |= core::DescriptorFlags::NonUniformIndexingNative;
		}
		if (_info.features.deviceDescriptorIndexing.descriptorBindingStorageBufferUpdateAfterBind) {
			flags |= core::DescriptorFlags::UpdateAfterBind;
		}
		break;
	case DescriptorType::InputAttachment:
		if (_info.features.deviceDescriptorIndexing.shaderInputAttachmentArrayDynamicIndexing) {
			flags |= core::DescriptorFlags::DynamicIndexing;
		}
		if (_info.features.deviceDescriptorIndexing.shaderInputAttachmentArrayNonUniformIndexing) {
			flags |= core::DescriptorFlags::NonUniformIndexing;
		}
		if (_info.properties.deviceDescriptorIndexing
						.shaderInputAttachmentArrayNonUniformIndexingNative) {
			flags |= core::DescriptorFlags::NonUniformIndexingNative;
		}
		break;
	case DescriptorType::Attachment:
	case DescriptorType::Unknown: break;
	}
	return flags;
}

Rc<core::Framebuffer> Device::makeFramebuffer(const core::QueuePassData *pass,
		SpanView<Rc<core::ImageView>> views) {
	return Rc<Framebuffer>::create(*this, (RenderPass *)pass->impl.get(), views);
}

auto Device::makeImage(StringView key, const ImageInfoData &imageInfo) -> Rc<ImageStorage> {
	bool isTransient =
			(imageInfo.usage & core::ImageUsage::TransientAttachment) != core::ImageUsage::None;

	auto img = _allocator->spawnPersistent(isTransient ? AllocationUsage::DeviceLocalLazilyAllocated
													   : AllocationUsage::DeviceLocal,
			key, imageInfo, false);

	return Rc<ImageStorage>::create(move(img));
}

Rc<core::Semaphore> Device::makeSemaphore() {
	auto ret = Rc<Semaphore>::create(*this, core::SemaphoreType::Default);
	return ret;
}

Rc<core::ImageView> Device::makeImageView(const Rc<core::ImageObject> &img,
		const ImageViewInfo &info) {
	auto ret = Rc<ImageView>::create(*this, (Image *)img.get(), info);
	return ret;
}

Rc<core::CommandPool> Device::makeCommandPool(uint32_t family, core::QueueFlags flags) {
	return Rc<CommandPool>::create(*this, family, flags);
}

Rc<core::QueryPool> Device::makeQueryPool(uint32_t family, core::QueueFlags flags,
		const core::QueryPoolInfo &info) {
	return Rc<QueryPool>::create(*this, family, flags, info);
}

Rc<core::TextureSet> Device::makeTextureSet(const core::TextureSetLayout &layout) {
	return Rc<TextureSet>::create(*this, layout);
}

bool Device::hasNonSolidFillMode() const {
	return _info.features.device10.features.fillModeNonSolid;
}

bool Device::hasDynamicIndexedBuffers() const {
	return _info.features.device10.features.shaderStorageBufferArrayDynamicIndexing;
}

bool Device::hasBufferDeviceAddresses() const {
	return _info.features.deviceBufferDeviceAddress.bufferDeviceAddress;
}

bool Device::hasExternalFences() const {
	return _info.features.optionals[toInt(OptionalDeviceExtension::ExternalFenceFd)]
			&& (_info.features.fenceSyncFd.externalFenceFeatures
					& VK_EXTERNAL_FENCE_FEATURE_EXPORTABLE_BIT);
}

bool Device::isPortabilityMode() const {
#ifdef VK_ENABLE_BETA_EXTENSIONS
	return _info.features.devicePortability.sType
			== VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PORTABILITY_SUBSET_FEATURES_KHR;
#else
	return false;
#endif
}

void Device::waitIdle() const {
	std::unique_lock lock(_resourceMutex);

	_table->vkDeviceWaitIdle(_device);

	core::Device::waitIdle();
}

void Device::compileImage(const Loop &loop, const Rc<core::DynamicImage> &img,
		Function<void(bool)> &&cb) {
	struct CompileImageTask : public Ref {
		Function<void(bool)> callback;
		Rc<core::DynamicImage> image;
		Rc<Loop> loop;
		Rc<Device> device;

		Rc<Buffer> transferBuffer;
		Rc<Image> resultImage;
		Rc<CommandPool> pool;
		Rc<DeviceQueue> queue;
		Rc<Fence> fence;
	};

	auto task = Rc<CompileImageTask>::create();
	task->callback = sp::move(cb);
	task->image = img;
	task->loop = (Loop *)&loop;
	task->device = this;

	loop.performInQueue([this, task]() {
		// make transfer buffer

		task->image->acquireData([&](BytesView view) {
			task->transferBuffer = task->device->getAllocator()->spawnPersistent(
					AllocationUsage::HostTransitionSource,
					BufferInfo(core::ForceBufferUsage(core::BufferUsage::TransferSrc),
							core::PassType::Transfer),
					view);
		});

		sprt_passert(!task->image->getInfo().key.empty(), "Unnamed dynamic image");

		task->resultImage =
				task->device->getAllocator()->spawnPersistent(AllocationUsage::DeviceLocal,
						task->image->getInfo().key, task->image->getInfo(), false);

		if (!task->transferBuffer) {
			task->loop->performOnThread([task] { task->callback(false); });
			return;
		}

		task->loop->performOnThread([this, task] {
			task->device->acquireQueue(core::QueueFlags::Transfer, *task->loop,
					[this, task](core::Loop &loop, const Rc<core::DeviceQueue> &queue) {
				task->fence = ref_cast<Fence>(task->loop->acquireFence(core::FenceType::Default));
				task->pool = ref_cast<CommandPool>(
						task->device->acquireCommandPool(core::QueueFlags::Transfer));
				task->queue = ref_cast<DeviceQueue>(queue);

				task->fence->addRelease([task](bool) {
					task->device->releaseCommandPool(*task->loop, move(task->pool));
					task->transferBuffer
							->dropPendingBarrier(); // hold reference while commands is active
				}, this, "TextureSetLayout::compileImage transferBuffer->dropPendingBarrier");

				loop.performInQueue(
						Rc<thread::Task>::create([this, task](const thread::Task &) -> bool {
					auto buf = task->pool->recordBuffer(*task->device, Vector<Rc<DescriptorPool>>(),
							[&, this](CommandBuffer &buf) {
						auto f = getQueueFamily(task->resultImage->getInfo().type);
						buf.writeImageTransfer(task->pool->getFamilyIdx(),
								f ? f->index : VK_QUEUE_FAMILY_IGNORED, task->transferBuffer,
								task->resultImage);
						return true;
					});

					if (task->queue->submit(*task->fence, buf) == Status::Ok) {
						return true;
					}
					return false;
				}, [task](const thread::Task &, bool success) {
					if (task->queue) {
						task->device->releaseQueue(move(task->queue));
					}
					if (success) {
						task->image->setImage(task->resultImage.get());
						task->callback(true);
					} else {
						task->callback(false);
					}
					task->fence->schedule(*task->loop);
					task->fence = nullptr;
				}));
			}, [task](core::Loop &) { task->callback(false); });
		});
	}, task);
}

void Device::readImage(Loop &loop, const Rc<Image> &image, core::AttachmentLayout l,
		Function<void(const ImageInfoData &, BytesView)> &&cb) {
	if (!image) {
		log::source().error("vk::Device", "readImage: Image is null");
		return;
	}

	runTask(loop, Rc<ReadImageTask>::create(image, l, sp::move(cb)));
}

void Device::readBuffer(Loop &loop, const Rc<Buffer> &buf,
		Function<void(const BufferInfo &, BytesView)> &&cb) {
	if (!buf) {
		log::source().error("vk::Device", "readBuffer: Buffer is null");
		return;
	}

	runTask(loop, Rc<ReadBufferTask>::create(buf, sp::move(cb)));
}

bool Device::setup(const Instance *instance, VkPhysicalDevice p, const Properties &prop,
		const Vector<core::DeviceQueueFamily> &queueFamilies, const Features &f,
		const Vector<StringView> &ext) {
	_enabledFeatures = f;

	Vector<const char *> requiredExtension;
	requiredExtension.reserve(ext.size());
	for (auto &it : ext) { requiredExtension.emplace_back(it.data()); }

	Vector<VkDeviceQueueCreateInfo> queueCreateInfos;

	uint32_t maxQueues = 0;
	for (auto &it : queueFamilies) { maxQueues = std::max(it.count, maxQueues); }

	Vector<float> queuePriority;
	queuePriority.resize(maxQueues, 1.0f);

	for (auto &queueFamily : queueFamilies) {
		VkDeviceQueueCreateInfo queueCreateInfo = {};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamily.index;
		queueCreateInfo.queueCount = queueFamily.count;
		queueCreateInfo.pQueuePriorities = queuePriority.data();
		queueCreateInfos.push_back(queueCreateInfo);
	}

	VkDeviceCreateInfo deviceCreateInfo = {};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	void *next = nullptr;
#ifdef VK_ENABLE_BETA_EXTENSIONS
	if (_enabledFeatures.optionals.test(toInt(OptionalDeviceExtension::Portability))) {
		_enabledFeatures.devicePortability.pNext = next;
		next = &_enabledFeatures.devicePortability;
	}
#endif
#if VK_VERSION_1_3
	if (prop.device10.properties.apiVersion >= VK_API_VERSION_1_3) {
		_enabledFeatures.device13.pNext = next;
		_enabledFeatures.device12.pNext = &_enabledFeatures.device13;
		_enabledFeatures.device11.pNext = &_enabledFeatures.device12;
		_enabledFeatures.device10.pNext = &_enabledFeatures.device11;
		deviceCreateInfo.pNext = &_enabledFeatures.device11;
	} else
#endif
			if (prop.device10.properties.apiVersion >= VK_API_VERSION_1_2) {
		_enabledFeatures.device12.pNext = next;
		_enabledFeatures.device11.pNext = &_enabledFeatures.device12;
		_enabledFeatures.device10.pNext = &_enabledFeatures.device11;
		deviceCreateInfo.pNext = &_enabledFeatures.device11;
	} else {
		if (_enabledFeatures.optionals[toInt(OptionalDeviceExtension::Storage16Bit)]) {
			_enabledFeatures.device16bitStorage.pNext = next;
			next = &_enabledFeatures.device16bitStorage;
		}
		if (_enabledFeatures.optionals[toInt(OptionalDeviceExtension::Storage8Bit)]) {
			_enabledFeatures.device8bitStorage.pNext = next;
			next = &_enabledFeatures.device8bitStorage;
		}
		if (_enabledFeatures.optionals[toInt(OptionalDeviceExtension::ShaderFloat16Int8)]) {
			_enabledFeatures.deviceShaderFloat16Int8.pNext = next;
			next = &_enabledFeatures.deviceShaderFloat16Int8;
		}
		if (_enabledFeatures.optionals[toInt(OptionalDeviceExtension::DescriptorIndexing)]) {
			_enabledFeatures.deviceDescriptorIndexing.pNext = next;
			next = &_enabledFeatures.deviceDescriptorIndexing;
		}
		if (_enabledFeatures.optionals[toInt(OptionalDeviceExtension::DeviceAddress)]) {
			_enabledFeatures.deviceBufferDeviceAddress.pNext = next;
			next = &_enabledFeatures.deviceBufferDeviceAddress;
		}
		deviceCreateInfo.pNext = next;
	}
	deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
	deviceCreateInfo.pEnabledFeatures = &_enabledFeatures.device10.features;
	deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtension.size());
	deviceCreateInfo.ppEnabledExtensionNames = requiredExtension.data();

	if (hasFlag(instance->getFlags(), core::InstanceFlags::Validation)) {
		deviceCreateInfo.enabledLayerCount =
				static_cast<uint32_t>(sizeof(s_validationLayers) / sizeof(const char *));
		deviceCreateInfo.ppEnabledLayerNames = s_validationLayers;
	} else {
		deviceCreateInfo.enabledLayerCount = 0;
	}

	if (instance->vkCreateDevice(p, &deviceCreateInfo, nullptr, &_device) != VK_SUCCESS) {
		return false;
	}

#if VK_HOOK_DEBUG
	auto hookTable = new DeviceTable(DeviceTable::makeHooks());
	_original = new DeviceTable(instance->vkGetDeviceProcAddr, _device);
	_table = hookTable;
#else
	_table = new DeviceTable(instance->vkGetDeviceProcAddr, _device);
#endif

	return true;
}

} // namespace stappler::xenolith::vk
