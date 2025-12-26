/**
 Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>
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

#include "XLVkLoop.h"
#include "XLVkInstance.h"
#include "XLVkDevice.h"
#include "XLVkTextureSet.h"
#include "XLCoreFrameCache.h"

#include "XLVkTransferQueue.h"
#include "XLVkRenderQueueCompiler.h"
#include "XLVkMeshCompiler.h"
#include "XLVkMaterialCompiler.h"
#include "XLVkPresentationEngine.h"

#include "SPEventLooper.h"
#include "SPEventTimerHandle.h"

#define XL_VK_DEPS_DEBUG 0
#define XL_VK_PAUSE_TIMER 1

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk {

struct DependencyRequest : public Ref {
	Vector<Rc<core::DependencyEvent>> events;
	Function<void(bool)> callback;
	uint64_t initial = 0;
	uint32_t signaled = 0;
	bool success = true;
};

struct Loop::Internal final : memory::AllocPool {
	Internal(memory::pool_t *pool, Loop *l)
	: pool(pool), loop(l), instance(static_cast<Instance *>(l->getInstance())) { }

	void setDevice(Rc<Device> &&dev) {
		device = move(dev);

		transferQueue = Rc<TransferQueue>::create();
		materialQueue = Rc<MaterialCompiler>::create();

		renderQueueCompiler =
				Rc<RenderQueueCompiler>::create(*device, transferQueue, materialQueue);

		compileQueue(transferQueue, [this](bool success) {
			if (!success) {
				return;
			}

			loop->performOnThread([this] {
				if (!_running.load()) {
					return;
				}
				mem_pool::perform([&] {
					for (auto &it : _tmpResources) {
						auto h = loop->makeFrame(transferQueue->makeRequest(sp::move(it)), 0);
						h->update(true);
					}
					_tmpResources.clear();
				}, pool);
			}, loop, true);
		});
		compileQueue(materialQueue, [this](bool success) {
			if (!success) {
				return;
			}

			loop->performOnThread([this] {
				if (!_running.load()) {
					return;
				}

				mem_pool::perform([&] {
					for (auto &req : _tmpMaterials) {
						if (materialQueue->inProgress(req.first->attachment)) {
							materialQueue->appendRequest(req.first->attachment, sp::move(req.first),
									sp::move(req.second));
						} else {
							auto attachment = req.first->attachment;
							materialQueue->setInProgress(attachment);
							materialQueue->runMaterialCompilationFrame(*loop, sp::move(req.first),
									sp::move(req.second));
						}
					}
					_tmpMaterials.clear();
				}, pool);
			}, loop, true);
		});
	}

	void endDevice() {
		if (!device) {
			return;
		}

		defaultFences.clear();
		swapchainFences.clear();
		device->end();
		device = nullptr;
		instance = nullptr;
	}

	void update(bool lockfree = true) {
		auto it = scheduledFences.begin();
		while (it != scheduledFences.end()) {
			if ((*it)->check(*loop, lockfree)) {
				it = scheduledFences.erase(it);
			}
		}

#if XL_VK_PAUSE_TIMER
		if (updateTimerHandle && scheduledFences.empty()) {
			updateTimerHandle->pause();
		}
#endif
	}

	void waitIdle() {
		// wait for all pending tasks on fences
		for (auto &it : scheduledFences) { it->check(*loop, false); }
		scheduledFences.clear();

		if (device) {
			// wait for device
			device->waitIdle();
		}
	}

	void compileResource(Rc<TransferResource> &&req) {
		loop->performOnThread([this, req = move(req)]() mutable {
			if (!_running.load()) {
				return;
			}

			if (!transferQueue->isCompiled()) {
				mem_pool::perform([&] { _tmpResources.emplace_back(move(req)); }, pool);
			} else {
				auto h = loop->makeFrame(transferQueue->makeRequest(sp::move(req)), 0);
				h->update(true);
			}
		}, loop, true);
	}

	void compileQueue(const Rc<core::Queue> &req, Function<void(bool)> &&cb = nullptr) {
		if (!_running.load()) {
			return;
		}

		if (!device) {
			log::source().error("vk::Loop", "No device to compileQueue");
			return;
		}

		auto input = Rc<RenderQueueInput>::alloc();
		input->queue = req;

		auto h = Rc<DeviceFrameHandle>::create(*loop, *device,
				renderQueueCompiler->makeRequest(move(input)), 0);
		if (cb) {
			h->setCompleteCallback(
					[cb = sp::move(cb), req](FrameHandle &handle) { cb(handle.isValid()); });
		}

		h->update(true);
	}

	void compileMaterials(Rc<core::MaterialInputData> &&req, Vector<Rc<DependencyEvent>> &&deps) {
		loop->performOnThread([this, req = move(req), deps = sp::move(deps)]() mutable {
			if (!_running.load()) {
				return;
			}
			mem_pool::perform([&] {
				if (!materialQueue->isCompiled()) {
					_tmpMaterials.emplace_back(move(req),
							mem_pool::Vector<Rc<DependencyEvent>>(deps.begin(), deps.end()));
				} else {
					if (materialQueue->inProgress(req->attachment)) {
						materialQueue->appendRequest(req->attachment, sp::move(req),
								sp::move(deps));
					} else {
						auto attachment = req->attachment;
						materialQueue->setInProgress(attachment);
						materialQueue->runMaterialCompilationFrame(*loop, sp::move(req),
								sp::move(deps));
					}
				}
			}, pool);
		}, loop, true);
	}

	void signalDependencies(const Vector<Rc<DependencyEvent>> &events, Queue *queue, bool success) {
		for (auto &it : events) {
			if (it->signal(queue, success)) {
				auto iit = dependencyRequests.equal_range(it.get());
				auto tmp = iit;
				while (iit.first != iit.second) {
					auto &v = iit.first->second;
					if (!success) {
						v->success = false;
					}
					++v->signaled;
					if (v->signaled == v->events.size()) {
#if XL_VK_DEPS_DEBUG
						StringStream str;
						str << "signalDependencies:";
						for (auto &it : v->events) { str << " " << it->getId(); }
						str << "\n";
						log::source().debug("vk::Loop", "Signal: ", str.str());
#endif
						v->callback(iit.first->second->success);
					}
					++iit.first;
				}
				mem_pool::perform([&] { dependencyRequests.erase(tmp.first, tmp.second); }, pool);
			}
		}
	}

	void waitForDependencies(Vector<Rc<DependencyEvent>> &&events, Function<void(bool)> &&cb) {
		mem_pool::perform([&] {
#if XL_VK_DEPS_DEBUG
			StringStream str;
			str << "waitForDependencies:";
			for (auto &it : events) { str << " " << it->getId(); }
			log::source().debug("vk::Loop", "Wait: ", str.str());
#endif

			auto req = Rc<DependencyRequest>::alloc();
			req->events = sp::move(events);
			req->callback = sp::move(cb);
			req->initial = sp::platform::clock(ClockType::Monotonic);

			mem_pool::perform([&] {
				for (auto &it : req->events) {
					if (it->isSignaled()) {
						if (!it->isSuccessful()) {
							req->success = false;
						}
						++req->signaled;
					} else {
						dependencyRequests.emplace(it.get(), req);
					}
				}
			}, pool);

			if (req->signaled == req->events.size()) {
#if XL_VK_DEPS_DEBUG
				log::source().debug("vk::Loop", "Run: ", str.str());
#endif
				req->callback(req->success);
			}
		}, pool);
	}

	void scheduleFence(Rc<Fence> &&fence) {
		if (!_running) {
			fence->check(*loop, false);
			return;
		}

		if (auto handle = fence->exportFence(*loop, nullptr)) {
			loop->getLooper()->performHandle(handle);
		} else {
			if (scheduledFences.empty()) {
#if XL_VK_PAUSE_TIMER
				auto status = updateTimerHandle->resume();
				if (status != Status::Ok) {
					log::source().error("vk::Loop", "Fail to resume fence scheduler: ", status);
				}
#endif
			}
			mem_pool::perform([&] { scheduledFences.emplace(move(fence)); }, pool);
		}
	}

	memory::allocator_t *alloc = nullptr;
	memory::pool_t *pool = nullptr;

	Loop *loop = nullptr;
	Rc<core::LoopInfo> info;

	Rc<event::TimerHandle> updateTimerHandle;

	std::multimap<DependencyEvent *, Rc<DependencyRequest>, std::less<void>> dependencyRequests;

	Mutex resourceMutex;

	Rc<Instance> instance;
	Rc<Device> device;
	mem_pool::Vector<Rc<Fence>> defaultFences;
	mem_pool::Vector<Rc<Fence>> swapchainFences;
	mem_pool::Set<Rc<Fence>> scheduledFences;

	Rc<RenderQueueCompiler> renderQueueCompiler;
	Rc<TransferQueue> transferQueue;
	Rc<MaterialCompiler> materialQueue;
	Rc<MeshCompiler> meshQueue;

	std::atomic<bool> _running = true;

	mem_pool::Vector<Rc<TransferResource>> _tmpResources;
	mem_pool::Vector<Pair<Rc<core::MaterialInputData>, mem_pool::Vector<Rc<DependencyEvent>>>>
			_tmpMaterials;
};

Loop::~Loop() {
	_looper->performOnThread([internal = _internal, frameCache = _frameCache] {
		internal->_running = false;

		if (frameCache) {
			frameCache->invalidate();
		}

		internal->waitIdle();
		internal->endDevice();

		auto memalloc = internal->alloc;
		auto mempool = internal->pool;

		delete internal;

		if (frameCache) {
			frameCache->invalidate();
		}

		memory::pool::destroy(mempool);
		memory::allocator::destroy(memalloc);
	}, nullptr);
}

bool Loop::init(NotNull<event::Looper> looper, NotNull<core::Instance> instance,
		Rc<LoopInfo> &&info) {
	if (!core::Loop::init(looper, instance, move(info))) {
		return false;
	}

	looper->performOnThread([&] {
		auto alloc = memory::allocator::create();
		auto pool = memory::pool::create(alloc);

		mem_pool::perform([&] {
			_internal = new (pool) vk::Loop::Internal(pool, this);
			_internal->alloc = alloc;
			_internal->pool = pool;
			_internal->info = _info;

			if (auto dev = _instance.get_cast<Instance>()->makeDevice(*_info)) {
				_internal->setDevice(move(dev));
				_frameCache = Rc<FrameCache>::create(*this, *_internal->device);
			} else if (_info->deviceIdx != core::InstanceDefaultDevice) {
				log::source().warn("vk::Loop",
						"Unable to create device with index: ", _info->deviceIdx,
						", fallback to default");
				_info->deviceIdx = core::InstanceDefaultDevice;
				if (auto dev = _instance.get_cast<Instance>()->makeDevice(*_info)) {
					_internal->setDevice(move(dev));
					_frameCache = Rc<FrameCache>::create(*this, *_internal->device);
				} else {
					log::source().error("vk::Loop", "Unable to create device");
					return;
				}
			} else {
				log::source().error("vk::Loop", "Unable to create device");
				return;
			}
		}, pool);
	}, this, true);

	return true;
}

void Loop::run() {
	_looper->performOnThread([&] {
		_internal->updateTimerHandle = _looper->scheduleTimer(event::TimerInfo{
			.completion = event::TimerInfo::Completion::create<Loop>(this,
					[](Loop *loop, event::TimerHandle *, uint32_t valuu, Status status) {
			if (loop->_internal) {
				loop->_internal->update();
			}
			if (loop->_frameCache) {
				loop->_frameCache->clear();
			}
		}),
			.interval = TimeInterval::microseconds(config::PresentationSchedulerInterval),
			.count = event::TimerInfo::Infinite});
		_internal->updateTimerHandle->setUserdata(this);
	}, this, true);
}

void Loop::stop() {
	_looper->performOnThread([&] {
		mem_pool::perform([&] {
			_internal->_running = false;
			_internal->waitIdle();

			_internal->update(false);

			_internal->updateTimerHandle->cancel();
			_internal->updateTimerHandle = nullptr;

			_internal->transferQueue = nullptr;
			_internal->renderQueueCompiler = nullptr;
			_internal->materialQueue = nullptr;
			_internal->meshQueue = nullptr;

			_internal->defaultFences.clear();
			_internal->swapchainFences.clear();

#if SP_REF_DEBUG
			if (_internal->loop->getReferenceCount() > 1) {
				_internal->loop->foreachBacktrace(
						[](uint64_t id, Time time, const std::vector<std::string> &backtrace) {
					StringStream out;
					out << id << ": " << time.toHttp<Interface>() << ":\n";
					for (auto &it : backtrace) { out << "\t" << it << "\n"; }
					log::debug("Contexnt", "Loop refs:\n", out.str());
				});
			}
#endif
		}, _internal->pool);
	}, this);
}

bool Loop::isRunning() const { return _internal && _internal->_running; }

void Loop::compileResource(Rc<core::Resource> &&req, Function<void(bool)> &&cb,
		bool preload) const {
	auto res = Rc<TransferResource>::create(_internal->device->getAllocator(), sp::move(req),
			sp::move(cb));
	if (preload) {
		res->initialize();
	}
	performOnThread([this, res = sp::move(res)]() mutable {
		if (!_internal) {
			return;
		}
		_internal->compileResource(sp::move(res));
	}, const_cast<Loop *>(this), true);
}

void Loop::compileQueue(const Rc<Queue> &req, Function<void(bool)> &&callback) const {
	performOnThread([this, req, callback = sp::move(callback)]() mutable {
		if (!_internal) {
			return;
		}
		_internal->compileQueue(req, sp::move(callback));
	}, const_cast<Loop *>(this), true);
}

void Loop::compileMaterials(Rc<core::MaterialInputData> &&req,
		const Vector<Rc<DependencyEvent>> &deps) const {
	performOnThread([this, req = move(req), deps = deps]() mutable {
		if (!_internal) {
			return;
		}
		_internal->compileMaterials(sp::move(req), sp::move(deps));
	}, const_cast<Loop *>(this), true);
}

void Loop::compileImage(const Rc<core::DynamicImage> &img, Function<void(bool)> &&callback) const {
	performOnThread([this, img, callback = sp::move(callback)]() mutable {
		if (!_internal) {
			return;
		}
		_internal->device->compileImage(*this, img, sp::move(callback));
	}, const_cast<Loop *>(this), true);
}

void Loop::runRenderQueue(Rc<FrameRequest> &&req, uint64_t gen, Function<void(bool)> &&callback) {
	performOnThread([this, req = sp::move(req), gen, callback = sp::move(callback)]() mutable {
		if (!_internal || !_internal->_running.load()) {
			return;
		}

		auto frame = makeFrame(move(req), gen);
		if (frame && callback) {
			frame->setCompleteCallback([this, callback = sp::move(callback)](FrameHandle &handle) {
				if (!_internal || !_internal->_running.load()) {
					return;
				}
				callback(handle.isValid());
			});
		}
		if (frame) {
			frame->update(true);
		}
	}, this, true);
}

void Loop::performInQueue(Rc<thread::Task> &&task) const {
	if (!_internal || !_internal->_running.load()) {
		task->cancel();
		return;
	}

	_looper->performAsync(move(task));
}

void Loop::performInQueue(Function<void()> &&func, Ref *target) const {
	if (!_internal || !_internal->_running.load()) {
		return;
	}

	_looper->performAsync(sp::move(func), target);
}

void Loop::performOnThread(Function<void()> &&func, Ref *target, bool immediate,
		StringView tag) const {
	if (!_internal || !_internal->_running.load()) {
		return;
	}

	if (immediate) {
		if (_looper->isOnThisThread()) {
			func();
			return;
		}
	}

	_looper->performOnThread(sp::move(func), target, immediate, tag);
}

auto Loop::makeFrame(Rc<FrameRequest> &&req, uint64_t gen) -> Rc<FrameHandle> {
	return Rc<DeviceFrameHandle>::create(*this, *_internal->device, move(req), gen);
}

Rc<core::Framebuffer> Loop::acquireFramebuffer(const PassData *data,
		SpanView<Rc<core::ImageView>> views) {
	return _frameCache->acquireFramebuffer(data, views);
}

void Loop::releaseFramebuffer(Rc<core::Framebuffer> &&fb) {
	_frameCache->releaseFramebuffer(move(fb));
}

auto Loop::acquireImage(const ImageAttachment *a, const AttachmentHandle *h,
		const core::ImageInfoData &i) -> Rc<ImageStorage> {
	core::ImageInfoData info(i);
	if (a->isTransient()) {
		if ((info.usage
					& (core::ImageUsage::ColorAttachment | core::ImageUsage::DepthStencilAttachment
							| core::ImageUsage::InputAttachment))
				!= core::ImageUsage::None) {
			info.usage |= core::ImageUsage::TransientAttachment;
		}
	}

	auto views = a->getImageViews(info);
	return _frameCache->acquireImage(a->getId(), info, views);
}

void Loop::releaseImage(Rc<ImageStorage> &&image) {
	performOnThread([this, image = move(image)]() mutable {
		_frameCache->releaseImage(move(image));
	}, this, true);
}

Rc<core::Semaphore> Loop::makeSemaphore() { return _internal->device->makeSemaphore(); }

core::ImageFormat Loop::getCommonFormat() const { return _info->defaultFormat; }

SpanView<core::ImageFormat> Loop::getSupportedDepthStencilFormat() const {
	return _internal->device->getSupportedDepthStencilFormat();
}

Rc<core::Fence> Loop::acquireFence(core::FenceType type) {
	auto initFence = [&](const Rc<Fence> &fence) {
		fence->setFrame([guard = Rc<Loop>(this), fence]() mutable {
			if (guard->_looper->isOnThisThread()) {
				// schedule fence
				guard->_internal->scheduleFence(Rc<Fence>(fence));
				return true;
			} else {
				guard->performOnThread([guard, fence = move(fence)]() mutable {
					if (!fence->check(*guard, true)) {
						return;
					}

					guard->_internal->scheduleFence(move(fence));
				}, guard, true);
				return true;
			}
		}, [guard = Rc<Loop>(this), fence]() mutable {
			if (!guard->_internal) {
				return;
			}
			fence->clear();
			std::unique_lock<Mutex> lock(guard->_internal->resourceMutex);
			switch (fence->getType()) {
			case core::FenceType::Default:
				guard->_internal->defaultFences.emplace_back(move(fence));
				break;
			case core::FenceType::Swapchain:
				guard->_internal->swapchainFences.emplace_back(move(fence));
				break;
			}
		}, 0);
	};

	std::unique_lock<Mutex> lock(_internal->resourceMutex);
	switch (type) {
	case core::FenceType::Default:
		if (!_internal->defaultFences.empty()) {
			auto ref = move(_internal->defaultFences.back());
			_internal->defaultFences.pop_back();
			initFence(ref);
			return ref;
		}
		break;
	case core::FenceType::Swapchain:
		if (!_internal->swapchainFences.empty()) {
			auto ref = move(_internal->swapchainFences.back());
			_internal->swapchainFences.pop_back();
			initFence(ref);
			return ref;
		}
		break;
	}
	lock.unlock();
	auto ref = Rc<Fence>::create(*_internal->device, type);
	initFence(ref);
	return ref;
}

void Loop::signalDependencies(const Vector<Rc<DependencyEvent>> &events, Queue *q, bool success) {
	if (!events.empty()) {
		if (_looper->isOnThisThread() && _internal) {
			_internal->signalDependencies(events, q, success);
			return;
		}

		performOnThread([this, events, success, q = Rc<Queue>(q)]() {
			_internal->signalDependencies(events, q, success);
		}, this, false);
	}
}

void Loop::waitForDependencies(const Vector<Rc<DependencyEvent>> &events,
		Function<void(bool)> &&cb) {
	if (events.empty()) {
		cb(true);
	} else {
		performOnThread([this, events = events, cb = sp::move(cb)]() mutable {
			_internal->waitForDependencies(sp::move(events), sp::move(cb));
		}, this, true);
	}
}

void Loop::waitIdle() {
	if (_internal) {
		_internal->waitIdle();
	}
}

void Loop::captureImage(Function<void(const ImageInfoData &info, BytesView view)> &&cb,
		const Rc<core::ImageObject> &image, core::AttachmentLayout l) {
	performOnThread([this, cb = sp::move(cb), image, l]() mutable {
		_internal->device->readImage(*this, image.cast<Image>(), l, sp::move(cb));
	}, this, true);
}

void Loop::captureBuffer(Function<void(const BufferInfo &info, BytesView view)> &&cb,
		const Rc<core::BufferObject> &buf) {
	if ((buf->getInfo().usage & core::BufferUsage::TransferSrc) != core::BufferUsage::TransferSrc) {
		log::source().error("vk::Loop::captureBuffer", "Buffer '", buf->getName(),
				"' has no BufferUsage::TransferSrc flag to being captured");
	}

	performOnThread([this, cb = sp::move(cb), buf]() mutable {
		_internal->device->readBuffer(*this, buf.cast<Buffer>(), sp::move(cb));
	}, this, true);
}

Rc<core::PresentationEngine> Loop::makePresentationEngine(NotNull<core::PresentationWindow> w,
		core::PresentationOptions opts) {
	return Rc<PresentationEngine>::create(this, _internal->device.get(), w, opts);
}

void Loop::performInit() { }

void Loop::finalizeInit() { }

} // namespace stappler::xenolith::vk
