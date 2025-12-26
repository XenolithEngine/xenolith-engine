/**
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

#ifndef XENOLITH_CORE_XLCOREDEVICE_H_
#define XENOLITH_CORE_XLCOREDEVICE_H_

#include "XLCoreQueueData.h"
#include "XLCoreObject.h"
#include "XLCoreImageStorage.h"
#include "XLCoreDeviceQueue.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::core {

class Instance;
class Object;
class Loop;

class DeviceQueueTask : public Ref {
public:
	virtual ~DeviceQueueTask() = default;

	virtual bool init(core::QueueFlags flags) {
		_queueFlags = flags;
		return true;
	}

	core::QueueFlags getQueueFlags() const { return _queueFlags; }

	virtual bool handleQueueAcquired(Device &, DeviceQueue &) { return false; }

	virtual void fillCommandBuffer(Device &, CommandBuffer &) { }

	virtual void handleComplete(bool success) { }

protected:
	core::QueueFlags _queueFlags = core::QueueFlags::None;
};

class SP_PUBLIC Device : public Ref {
public:
	using DescriptorType = core::DescriptorType;
	using ImageStorage = core::ImageStorage;

	Device();
	virtual ~Device();

	virtual bool init(const Instance *instance);
	virtual void end();

	Rc<Shader> getProgram(StringView);
	Rc<Shader> addProgram(Rc<Shader>);

	void addObject(Object *);
	void removeObject(Object *);

	SpanView<ImageFormat> getSupportedDepthStencilFormat() const { return _depthFormats; }
	SpanView<ImageFormat> getSupportedColorFormat() const { return _colorFormats; }

	virtual void onLoopStarted(Loop &);
	virtual void onLoopEnded(Loop &);

	virtual DescriptorFlags getSupportedDescriptorFlags(DescriptorType) const;

	virtual Rc<Framebuffer> makeFramebuffer(const QueuePassData *, SpanView<Rc<ImageView>>);
	virtual Rc<ImageStorage> makeImage(StringView, const ImageInfoData &);
	virtual Rc<Semaphore> makeSemaphore();
	virtual Rc<ImageView> makeImageView(const Rc<ImageObject> &, const ImageViewInfo &);
	virtual Rc<CommandPool> makeCommandPool(uint32_t family, QueueFlags flags);
	virtual Rc<QueryPool> makeQueryPool(uint32_t family, QueueFlags flags, const QueryPoolInfo &);
	virtual Rc<TextureSet> makeTextureSet(const TextureSetLayout &);

	const DeviceQueueFamily *getQueueFamily(uint32_t) const;
	const DeviceQueueFamily *getQueueFamily(QueueFlags) const;
	const DeviceQueueFamily *getQueueFamily(PassType) const;

	const Vector<DeviceQueueFamily> &getQueueFamilies() const;

	// acquire DeviceQueue handle
	// - QueueFlags - one of QueueOperations flags, defining capabilities of required queue
	// - core::FrameHandle - frame, in which queue will be used
	// - acquire - function, to call with result, can be called immediately
	// 		or when queue for specified operations become available (on GL thread)
	// - invalidate - function to call when queue query is invalidated (e.g. when frame is invalidated)
	// - ref - ref to preserve until query is completed
	// returns
	// - true is query was completed or scheduled,
	// - false if frame is not valid or no queue family with requested capabilities exists
	//
	// Acquired DeviceQueue must be released with releaseQueue
	bool acquireQueue(QueueFlags, FrameHandle &,
			Function<void(FrameHandle &, const Rc<DeviceQueue> &)> &&acquire,
			Function<void(FrameHandle &)> &&invalidate, Rc<Ref> && = nullptr);
	bool acquireQueue(QueueFlags, Loop &, Function<void(Loop &, const Rc<DeviceQueue> &)> &&acquire,
			Function<void(Loop &)> &&invalidate, Rc<Ref> && = nullptr);
	void releaseQueue(Rc<DeviceQueue> &&);

	// Запросить DeviceQueue синхронно, если возможно
	Rc<DeviceQueue> tryAcquireQueue(QueueFlags);

	Rc<CommandPool> acquireCommandPool(QueueFlags, uint32_t = 0);
	Rc<CommandPool> acquireCommandPool(uint32_t familyIndex);
	void releaseCommandPool(core::Loop &, Rc<CommandPool> &&);
	void releaseCommandPoolUnsafe(Rc<CommandPool> &&);

	Rc<QueryPool> acquireQueryPool(QueueFlags, const QueryPoolInfo &);
	Rc<QueryPool> acquireQueryPool(uint32_t familyIndex, const QueryPoolInfo &);
	void releaseQueryPool(core::Loop &, Rc<QueryPool> &&);
	void releaseQueryPoolUnsafe(Rc<QueryPool> &&);

	void runTask(Loop &loop, Rc<DeviceQueueTask> &&);

	void invalidateSemaphore(Rc<Semaphore> &&sem) const;

	virtual void waitIdle() const;

protected:
	friend class Loop;

	void clearShaders();
	void invalidateObjects();

	bool _started = false;
	const Instance *_glInstance = nullptr;
	Mutex _shaderMutex;
	Mutex _objectMutex;

	Map<String, Rc<Shader>> _shaders;

	HashSet<Object *> _objects;

	Vector<ImageFormat> _depthFormats;
	Vector<ImageFormat> _colorFormats;

	mutable Mutex _resourceMutex;
	uint32_t _resourceQueueWaiters = 0;

	Vector<DeviceQueueFamily> _families;

	mutable Vector<Rc<Semaphore>> _invalidatedSemaphores;
};

} // namespace stappler::xenolith::core

#endif /* XENOLITH_CORE_XLCOREDEVICE_H_ */
