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

#ifndef XENOLITH_BACKEND_VK_XLVKDEVICE_H_
#define XENOLITH_BACKEND_VK_XLVKDEVICE_H_

#include "XLVkInstance.h"
#include "XLVkDeviceQueue.h"
#include "XLVkLoop.h"
#include <sprt/cxx/condition_variable>

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk {

class Fence;
class Allocator;
class TextureSetLayout;
class Sampler;
class Loop;
class DeviceMemoryPool;

class SP_PUBLIC DeviceFrameHandle : public core::FrameHandle {
public:
	virtual ~DeviceFrameHandle();

	bool init(Loop &loop, Device &, Rc<FrameRequest> &&, uint64_t gen);

	Allocator *getAllocator() const { return _allocator; }
	DeviceMemoryPool *getMemPool(void *key);

protected:
	Rc<Allocator> _allocator;
	sprt::mutex _mutex;
	Map<void *, Rc<DeviceMemoryPool>> _memPools;
};

// A simulated VK_ERROR_DEVICE_LOST, for tests only: the device itself stays healthy, the engine is
// told it is gone. Setting a fault cannot be undone.
enum class DeviceTestFault : uint8_t {
	None,
	LoseOnSubmit, // every submit fails
	LoseOnFenceCheck, // a submit goes through; the check of a fence that has signaled says lost
	LoseWithoutSignal, // as LoseOnFenceCheck, and an exported sync_fd is dropped as if it never fired
};

class SP_PUBLIC Device : public core::Device {
public:
	using Features = DeviceInfo::Features;
	using Properties = DeviceInfo::Properties;
	using FrameHandle = core::FrameHandle;

	Device();
	virtual ~Device();

	bool init(const vk::Instance *instance, DeviceInfo &&, const Features &,
			const Vector<StringView> &);

	const Instance *getInstance() const { return _vkInstance; }
	VkDevice getDevice() const { return _device; }
	VkPhysicalDevice getPhysicalDevice() const;

	virtual void end() override;

	SurfaceBackendMask getPresentatonMask() const { return _presentMask; }

	const DeviceInfo &getInfo() const { return _info; }
	const DeviceTable *getTable() const;
	const Rc<Allocator> &getAllocator() const { return _allocator; }

	bool hasExtension(OptionalDeviceExtension) const;

	virtual core::DescriptorFlags getSupportedDescriptorFlags(DescriptorType) const override;

	virtual Rc<core::Framebuffer> makeFramebuffer(const core::QueuePassData *,
			SpanView<Rc<core::ImageView>>) override;
	virtual Rc<ImageStorage> makeImage(StringView, const ImageInfoData &) override;
	virtual Rc<core::Semaphore> makeSemaphore() override;
	virtual Rc<core::ImageView> makeImageView(const Rc<core::ImageObject> &,
			const ImageViewInfo &) override;
	virtual Rc<core::CommandPool> makeCommandPool(uint32_t family, core::QueueFlags flags) override;
	virtual Rc<core::QueryPool> makeQueryPool(uint32_t family, core::QueueFlags flags,
			const core::QueryPoolInfo &) override;
	virtual Rc<core::TextureSet> makeTextureSet(const core::TextureSetLayout &) override;

	// Unsynchronized device call. Use for anything that does not touch a VkQueue.
	template <typename Callback>
	void makeApiCall(const Callback &cb) {
		static_assert(sprt::is_invocable_v<Callback, const DeviceTable &, VkDevice>,
				"Invalid callback type");
		cb(*getTable(), getDevice());
	}

	// Serializes host access to the device's VkQueues, which Vulkan requires to be externally
	// synchronized (submits on workers, presents on window threads, wait-idle against all queues).
	// Never hold it across a wait for unsubmitted work: image acquisition and fence waits stay
	// outside, or the submit that signals them could never run.
	template <typename Callback>
	void makeQueueApiCall(const Callback &cb) {
		static_assert(sprt::is_invocable_v<Callback, const DeviceTable &, VkDevice>,
				"Invalid callback type");
		sprt::unique_lock<sprt::mutex> lock(_queueMutex);
		cb(*getTable(), getDevice());
	}

	bool hasNonSolidFillMode() const;
	bool hasDynamicIndexedBuffers() const;
	bool hasBufferDeviceAddresses() const;
	bool hasExternalFences() const;
	bool isPortabilityMode() const;

	virtual void waitIdle() const override;

	void compileImage(const Loop &loop, const Rc<core::DynamicImage> &, Function<void(bool)> &&);
	void updateImage(const Loop &loop, const Rc<core::DynamicImage> &, Bytes &&,
			Function<void(bool)> &&);

	void readImage(Loop &loop, const Rc<Image> &, core::AttachmentLayout,
			Function<void(const ImageInfoData &, BytesView)> &&);
	void readBuffer(Loop &loop, const Rc<Buffer> &,
			Function<void(const BufferInfo &, BytesView)> &&);

	void setTestFault(DeviceTestFault fault) { _testFault.store(toInt(fault)); }
	DeviceTestFault getTestFault() const { return DeviceTestFault(_testFault.load()); }

	// With export on (the default) and a device that supports it, a scheduled fence is exported as a
	// sync_fd and waits on the looper; off, fences are polled on the loop's timer. Read per fence.
	void setFenceExportEnabled(bool value) { _fenceExport.store(value); }
	bool isFenceExportEnabled() const { return _fenceExport.load(); }

	uint64_t getExportedFenceCount() const { return _exportedFences.load(); }
	void noteFenceExported() { ++_exportedFences; }

private:
	using core::Device::init;

	void doImageTransfer(const Loop &loop, const Rc<core::DynamicImage> &, Bytes &&, bool isUpdate,
			Function<void(bool)> &&);

	friend class DeviceQueue;

	bool setup(const Instance *instance, VkPhysicalDevice p, const Properties &prop,
			const Vector<core::DeviceQueueFamily> &queueFamilies, const Features &features,
			const Vector<StringView> &requiredExtension);

	const vk::Instance *_vkInstance = nullptr;
	DeviceTable *_table = nullptr;
#if VK_HOOK_DEBUG
	const DeviceTable *_original = nullptr;
#endif
	VkDevice _device = VK_NULL_HANDLE;

	SurfaceBackendMask _presentMask;
	DeviceInfo _info;
	Features _enabledFeatures;

	Rc<Allocator> _allocator;

	// set this to false to forcefully disable any of DescriptorIndexing features
	bool _useDescriptorIndexing = true;

	HashMap<VkFormat, VkFormatProperties> _formats;

	sprt::condition_variable _resourceQueueCond;
	sprt::mutex _queueMutex;
	sprt::atomic<uint8_t> _testFault = 0;
	sprt::atomic<bool> _fenceExport = true;
	sprt::atomic<uint64_t> _exportedFences = 0;
};

} // namespace stappler::xenolith::vk

#endif /* XENOLITH_BACKEND_VK_XLVKDEVICE_H_ */
