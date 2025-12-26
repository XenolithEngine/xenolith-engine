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

#include "XLCoreDevice.h"
#include "XLCoreDeviceQueue.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::core {

Device::Device() { }

Device::~Device() { invalidateObjects(); }

bool Device::init(const Instance *instance) {
	_glInstance = instance;
	return true;
}

void Device::end() {
	_started = false;

#if SP_REF_DEBUG
	if (isRetainTrackerEnabled()) {
		log::source().debug("Gl-Device", "Backtrace for ", (void *)this);
		foreachBacktrace([](uint64_t id, Time time, const std::vector<std::string> &vec) {
			StringStream stream;
			stream << "[" << id << ":" << time.toHttp<Interface>() << "]:\n";
			for (auto &it : vec) { stream << "\t" << it << "\n"; }
			log::source().debug("Gl-Device-Backtrace", stream.str());
		});
	}
#endif
}

Rc<Shader> Device::getProgram(StringView name) {
	std::unique_lock<Mutex> lock(_shaderMutex);
	auto it = _shaders.find(name);
	if (it != _shaders.end()) {
		return it->second;
	}
	return nullptr;
}

Rc<Shader> Device::addProgram(Rc<Shader> program) {
	std::unique_lock<Mutex> lock(_shaderMutex);
	auto it = _shaders.find(program->getName());
	if (it == _shaders.end()) {
		_shaders.emplace(program->getName().str<Interface>(), program);
		return program;
	} else {
		return it->second;
	}
}

Rc<Framebuffer> Device::makeFramebuffer(const QueuePassData *, SpanView<Rc<ImageView>>) {
	return nullptr;
}

auto Device::makeImage(StringView, const ImageInfoData &) -> Rc<ImageStorage> { return nullptr; }

Rc<Semaphore> Device::makeSemaphore() { return nullptr; }

Rc<ImageView> Device::makeImageView(const Rc<ImageObject> &, const ImageViewInfo &) {
	return nullptr;
}

Rc<CommandPool> Device::makeCommandPool(uint32_t family, QueueFlags flags) { return nullptr; }

Rc<QueryPool> Device::makeQueryPool(uint32_t family, QueueFlags flags, const QueryPoolInfo &) {
	return nullptr;
}

Rc<TextureSet> Device::makeTextureSet(const TextureSetLayout &) { return nullptr; }

const DeviceQueueFamily *Device::getQueueFamily(uint32_t familyIdx) const {
	for (auto &it : _families) {
		if (it.index == familyIdx) {
			return &it;
		}
	}
	return nullptr;
}

const DeviceQueueFamily *Device::getQueueFamily(QueueFlags ops) const {
	for (auto &it : _families) {
		if (it.preferred == ops) {
			return &it;
		}
	}
	for (auto &it : _families) {
		if ((it.flags & ops) != QueueFlags::None) {
			return &it;
		}
	}
	return nullptr;
}

const DeviceQueueFamily *Device::getQueueFamily(core::PassType type) const {
	switch (type) {
	case core::PassType::Graphics: return getQueueFamily(QueueFlags::Graphics); break;
	case core::PassType::Compute: return getQueueFamily(QueueFlags::Compute); break;
	case core::PassType::Transfer: return getQueueFamily(QueueFlags::Transfer); break;
	case core::PassType::Generic:
		log::
				source()
						.warn("core::Device",
								"core::PassType::Generic can not be assigned to queue family by "
								"it's type;" " please acquire queue family through flags");
		return nullptr;
		break;
	}
	return nullptr;
}

Rc<DeviceQueue> Device::tryAcquireQueue(QueueFlags ops) {
	auto family = (DeviceQueueFamily *)getQueueFamily(ops);
	if (!family) {
		return nullptr;
	}

	std::unique_lock<Mutex> lock(_resourceMutex);
	if (!family->queues.empty()) {
		// XL_VKDEVICE_LOG("tryAcquireQueueSync ", family->index, " (", family->count, ") ", getQueueFlagsDesc(family->flags));
		auto queue = move(family->queues.back());
		family->queues.pop_back();
		return queue;
	}
	return nullptr;
}

bool Device::acquireQueue(QueueFlags ops, FrameHandle &handle,
		Function<void(FrameHandle &, const Rc<DeviceQueue> &)> &&acquire,
		Function<void(FrameHandle &)> &&invalidate, Rc<Ref> &&ref) {

	auto family = (DeviceQueueFamily *)getQueueFamily(ops);
	if (!family) {
		return false;
	}

	std::unique_lock<Mutex> lock(_resourceMutex);
	Rc<DeviceQueue> queue;
	if (!family->queues.empty()) {
		queue = move(family->queues.back());
		family->queues.pop_back();
	} else {
		// XL_VKDEVICE_LOG("acquireQueue-wait ", family->index, " (", family->count, ") ", getQueueFlagsDesc(family->flags));
		family->waiters.emplace_back(DeviceQueueFamily::Waiter(sp::move(acquire),
				sp::move(invalidate), &handle, sp::move(ref)));
	}

	if (queue) {
		queue->setOwner(handle);
		// XL_VKDEVICE_LOG("acquireQueue ", family->index, " (", family->count, ") ", getQueueFlagsDesc(family->flags));
		acquire(handle, queue);
	}
	return true;
}

bool Device::acquireQueue(QueueFlags ops, Loop &loop,
		Function<void(Loop &, const Rc<DeviceQueue> &)> &&acquire,
		Function<void(Loop &)> &&invalidate, Rc<Ref> &&ref) {

	auto family = (DeviceQueueFamily *)getQueueFamily(ops);
	if (!family) {
		return false;
	}

	std::unique_lock<Mutex> lock(_resourceMutex);
	Rc<DeviceQueue> queue;
	if (!family->queues.empty()) {
		queue = move(family->queues.back());
		family->queues.pop_back();
	} else {
		// XL_VKDEVICE_LOG("acquireQueue-wait ", family->index, " (", family->count, ") ", getQueueFlagsDesc(family->flags));
		family->waiters.emplace_back(DeviceQueueFamily::Waiter(sp::move(acquire),
				sp::move(invalidate), &loop, sp::move(ref)));
	}

	lock.unlock();

	if (queue) {
		// XL_VKDEVICE_LOG("acquireQueue ", family->index, " (", family->count, ") ", getQueueFlagsDesc(family->flags));
		acquire(loop, queue);
	}
	return true;
}

void Device::releaseQueue(Rc<DeviceQueue> &&queue) {
	DeviceQueueFamily *family = nullptr;
	for (auto &it : _families) {
		if (it.index == queue->getIndex()) {
			family = &it;
			break;
		}
	}

	if (!family) {
		return;
	}

	queue->reset();

	std::unique_lock<Mutex> lock(_resourceMutex);

	// Проверяем, есть ли асинхронные ожидающие
	if (family->waiters.empty()) {
		// XL_VKDEVICE_LOG("releaseQueue ", family->index, " (", family->count, ") ", getQueueFlagsDesc(family->flags));
		family->queues.emplace_back(move(queue));
	} else {
		if (family->waiters.front().handle) {
			Rc<FrameHandle> handle;
			Rc<Ref> ref;
			Function<void(FrameHandle &, const Rc<DeviceQueue> &)> cb;
			Function<void(FrameHandle &)> invalidate;

			cb = sp::move(family->waiters.front().acquireForFrame);
			invalidate = sp::move(family->waiters.front().releaseForFrame);
			ref = move(family->waiters.front().ref);
			handle = move(family->waiters.front().handle);
			family->waiters.erase(family->waiters.begin());

			lock.unlock();

			if (handle && handle->isValid()) {
				if (cb) {
					queue->setOwner(*handle);
					// XL_VKDEVICE_LOG("release-acquireQueue ", family->index, " (", family->count, ") ", getQueueFlagsDesc(family->flags));
					cb(*handle, queue);
				}
			} else if (invalidate) {
				// XL_VKDEVICE_LOG("invalidate ", family->index, " (", family->count, ") ", getQueueFlagsDesc(family->flags));
				invalidate(*handle);
			}

			handle = nullptr;
		} else if (family->waiters.front().loop) {
			Rc<Loop> loop;
			Rc<Ref> ref;
			Function<void(Loop &, const Rc<DeviceQueue> &)> cb;
			Function<void(Loop &)> invalidate;

			cb = sp::move(family->waiters.front().acquireForLoop);
			invalidate = sp::move(family->waiters.front().releaseForLoop);
			ref = move(family->waiters.front().ref);
			loop = move(family->waiters.front().loop);
			family->waiters.erase(family->waiters.begin());

			lock.unlock();

			if (loop && loop->isRunning()) {
				if (cb) {
					// XL_VKDEVICE_LOG("release-acquireQueue ", family->index, " (", family->count, ") ", getQueueFlagsDesc(family->flags));
					cb(*loop, queue);
				}
			} else if (invalidate) {
				// XL_VKDEVICE_LOG("invalidate ", family->index, " (", family->count, ") ", getQueueFlagsDesc(family->flags));
				invalidate(*loop);
			}

			loop = nullptr;
		}
	}

	queue = nullptr;
}

Rc<CommandPool> Device::acquireCommandPool(QueueFlags c, uint32_t) {
	auto family = (DeviceQueueFamily *)getQueueFamily(c);
	if (!family) {
		return nullptr;
	}

	std::unique_lock<Mutex> lock(_resourceMutex);
	if (!family->pools.empty()) {
		auto ret = family->pools.back();
		family->pools.pop_back();
		return ret;
	}
	lock.unlock();
	return makeCommandPool(family->index, family->flags);
}

Rc<CommandPool> Device::acquireCommandPool(uint32_t familyIndex) {
	auto family = (DeviceQueueFamily *)getQueueFamily(familyIndex);
	if (!family) {
		return nullptr;
	}

	std::unique_lock<Mutex> lock(_resourceMutex);
	if (!family->pools.empty()) {
		auto ret = family->pools.back();
		family->pools.pop_back();
		return ret;
	}
	lock.unlock();
	return makeCommandPool(family->index, family->flags);
}

void Device::releaseCommandPool(core::Loop &loop, Rc<CommandPool> &&pool) {
	auto refId = retain();
	loop.performInQueue(Rc<thread::Task>::create(
			[this, pool = Rc<CommandPool>(pool)](const thread::Task &) -> bool {
		pool->reset(*this);
		return true;
	}, [this, pool = Rc<CommandPool>(pool), refId](const thread::Task &, bool success) mutable {
		if (success) {
			auto idx = pool->getFamilyIdx();
			std::unique_lock<Mutex> lock(_resourceMutex);
			for (auto &it : _families) {
				if (it.index == idx) {
					it.pools.emplace_back(move(pool));
					break;
				}
			}
		}
		release(refId);
	}, this));
}

void Device::releaseCommandPoolUnsafe(Rc<CommandPool> &&pool) {
	pool->reset(*this);

	std::unique_lock<Mutex> lock(_resourceMutex);
	for (auto &it : _families) {
		if (it.index == pool->getFamilyIdx()) {
			it.pools.emplace_back(move(pool));
			break;
		}
	}
}

Rc<QueryPool> Device::acquireQueryPool(QueueFlags c, const QueryPoolInfo &info) {
	auto family = (DeviceQueueFamily *)getQueueFamily(c);
	if (!family) {
		return nullptr;
	}

	std::unique_lock<Mutex> lock(_resourceMutex);
	if (!family->queries.empty()) {
		auto qIt = family->queries.find(info);
		if (qIt != family->queries.end() && !qIt->second.empty()) {
			auto ret = qIt->second.back();
			qIt->second.pop_back();
			return ret;
		}
	}
	lock.unlock();
	return makeQueryPool(family->index, family->flags, info);
}

Rc<QueryPool> Device::acquireQueryPool(uint32_t familyIndex, const QueryPoolInfo &info) {
	auto family = (DeviceQueueFamily *)getQueueFamily(familyIndex);
	if (!family) {
		return nullptr;
	}

	std::unique_lock<Mutex> lock(_resourceMutex);
	if (!family->queries.empty()) {
		auto qIt = family->queries.find(info);
		if (qIt != family->queries.end() && !qIt->second.empty()) {
			auto ret = qIt->second.back();
			qIt->second.pop_back();
			return ret;
		}
	}
	lock.unlock();
	return makeQueryPool(family->index, family->flags, info);
}

void Device::releaseQueryPool(core::Loop &loop, Rc<QueryPool> &&pool) {
	auto refId = retain();
	loop.performInQueue(Rc<thread::Task>::create(
			[this, pool = Rc<QueryPool>(pool)](const thread::Task &) -> bool {
		pool->reset(*this);
		return true;
	}, [this, pool = Rc<QueryPool>(pool), refId](const thread::Task &, bool success) mutable {
		if (success) {
			auto idx = pool->getFamilyIdx();
			std::unique_lock<Mutex> lock(_resourceMutex);
			for (auto &it : _families) {
				if (it.index == idx) {
					auto qIt = it.queries.find(pool->getInfo());
					if (qIt == it.queries.end()) {
						qIt = it.queries.emplace(pool->getInfo(), Vector<Rc<QueryPool>>()).first;
					}
					qIt->second.emplace_back(move(pool));
					break;
				}
			}
		}
		release(refId);
	}, this));
}

void Device::releaseQueryPoolUnsafe(Rc<QueryPool> &&pool) {
	pool->reset(*this);

	std::unique_lock<Mutex> lock(_resourceMutex);
	for (auto &it : _families) {
		if (it.index == pool->getFamilyIdx()) {
			auto qIt = it.queries.find(pool->getInfo());
			if (qIt == it.queries.end()) {
				qIt = it.queries.emplace(pool->getInfo(), Vector<Rc<QueryPool>>()).first;
			}
			qIt->second.emplace_back(move(pool));
			break;
		}
	}
}

void Device::runTask(Loop &loop, Rc<DeviceQueueTask> &&t) {
	struct DeviceQueueTaskData : public Ref {
		Device *device = nullptr;
		Rc<Loop> loop;

		Rc<CommandPool> pool;
		Rc<DeviceQueue> queue;
		Rc<Fence> fence;

		Rc<core::DeviceQueueTask> task;

		DeviceQueueTaskData(Device *dev, Rc<Loop> &&l, Rc<core::DeviceQueueTask> &&t)
		: device(dev), loop(move(l)), task(move(t)) { }
	};

	auto taskData = Rc<DeviceQueueTaskData>::alloc(this, &loop, move(t));

	taskData->loop->performOnThread([this, taskData] {
		taskData->device->acquireQueue(taskData->task->getQueueFlags(), *taskData->loop,
				[this, taskData](core::Loop &loop, const Rc<core::DeviceQueue> &queue) {
			taskData->fence =
					ref_cast<Fence>(taskData->loop->acquireFence(core::FenceType::Default));
			taskData->pool = ref_cast<CommandPool>(
					taskData->device->acquireCommandPool(taskData->task->getQueueFlags()));
			taskData->queue = ref_cast<DeviceQueue>(queue);

			if (!taskData->task->handleQueueAcquired(*taskData->device, *queue)) {
				taskData->device->releaseQueue(move(taskData->queue));
				taskData->device->releaseCommandPool(*taskData->loop, move(taskData->pool));
				return;
			}

			taskData->fence->addRelease([taskData](bool success) {
				taskData->device->releaseCommandPool(*taskData->loop, move(taskData->pool));
				taskData->task->handleComplete(success);
			}, this, "TextureSetLayout::readImage transferBuffer->dropPendingBarrier");

			loop.performInQueue(Rc<thread::Task>::create([taskData](const thread::Task &) -> bool {
				auto buf = taskData->pool->recordBuffer(*taskData->device, [&](CommandBuffer &buf) {
					taskData->task->fillCommandBuffer(*taskData->device, buf);
					return true;
				});

				if (taskData->queue->submit(*taskData->fence, buf) == Status::Ok) {
					return true;
				}
				return false;
			}, [taskData](const thread::Task &, bool success) {
				if (taskData->queue) {
					taskData->device->releaseQueue(move(taskData->queue));
				}
				if (!success) {
					taskData->task->handleComplete(false);
				}
				taskData->fence->schedule(*taskData->loop);
				taskData->fence = nullptr;
			}));
		}, [taskData](core::Loop &) { taskData->task->handleComplete(false); });
	}, taskData, true);
}

void Device::invalidateSemaphore(Rc<Semaphore> &&sem) const {
	std::unique_lock lock(_resourceMutex);
	_invalidatedSemaphores.emplace_back(move(sem));
}

void Device::waitIdle() const { _invalidatedSemaphores.clear(); }

const Vector<DeviceQueueFamily> &Device::getQueueFamilies() const { return _families; }

void Device::addObject(Object *obj) {
	std::unique_lock<Mutex> lock(_objectMutex);
	_objects.emplace(obj);
}

void Device::removeObject(Object *obj) {
	std::unique_lock<Mutex> lock(_objectMutex);
	_objects.erase(obj);
}

void Device::onLoopStarted(Loop &loop) { }

void Device::onLoopEnded(Loop &) { }

DescriptorFlags Device::getSupportedDescriptorFlags(DescriptorType) const {
	return DescriptorFlags::None;
}

void Device::clearShaders() { _shaders.clear(); }

void Device::invalidateObjects() {
	Vector<ObjectData> data;

	std::unique_lock<Mutex> lock(_objectMutex);
	for (auto &it : _objects) {
		if (auto img = dynamic_cast<ImageObject *>(it)) {
			log::source().warn("Gl-Device", "Image ", (void *)it, " \"", img->getName(), "\" ((",
					typeid(*it).name(), ") [rc:", it->getReferenceCount(),
					"] was not destroyed before device destruction");
		} else if (auto pass = dynamic_cast<RenderPass *>(it)) {
			log::source().warn("Gl-Device", "RenderPass ", (void *)it, " \"", pass->getName(),
					"\" (", typeid(*it).name(), ") [rc:", it->getReferenceCount(),
					"] was not destroyed before device destruction");
		} else if (auto obj = dynamic_cast<BufferObject *>(it)) {
			log::source().warn("Gl-Device", "Buffer ", (void *)it, " \"", obj->getName(), "\" ((",
					typeid(*it).name(), ") [rc:", it->getReferenceCount(),
					"] was not destroyed before device destruction");
		} else {
			auto name = it->getName();
			if (!name.empty()) {
				log::source().warn("Gl-Device", "Object ", (void *)it, " \"", name, "\" ((",
						typeid(*it).name(), ") [rc:", it->getReferenceCount(),
						"] was not destroyed before device destruction");
			} else {
				log::source().warn("Gl-Device", "Object ", (void *)it, " (", typeid(*it).name(),
						") [rc:", it->getReferenceCount(),
						"] was not destroyed before device destruction");
			}
		}
#if SP_REF_DEBUG
		log::source().warn("Gl-Device", "Backtrace for ", (void *)it);
		it->foreachBacktrace([](uint64_t id, Time time, const std::vector<std::string> &vec) {
			StringStream stream;
			stream << "[" << id << ":" << time.toHttp<Interface>() << "]:\n";
			for (auto &it : vec) { stream << "\t" << it << "\n"; }
			log::source().warn("Gl-Device-Backtrace", stream.str());
		});
#endif

		auto d = (ObjectData *)&it->getObjectData();

		data.emplace_back(*d);

		d->callback = nullptr;
	}
	_objects.clear();
	lock.unlock();

	for (auto &_object : data) {
		if (_object.callback) {
			_object.callback(_object.device, _object.type, _object.handle, _object.ptr);
		}
	}

	data.clear();
}

} // namespace stappler::xenolith::core
