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

#include "XLWgpuLoop.h"
#include "XLWgpuPipeline.h"
#include "XLWgpuObject.h"
#include "XLWgpuQueuePass.h"
#include "XLWgpuPresentation.h"
#include "XLWgpuTextureSet.h"
#include "XLWgpuMaterial.h"
#include "XLCoreFrameCache.h"
#include "XLCoreFrameHandle.h"
#include "XLCoreFrameRequest.h"
#include "XLCorePresentationEngine.h"
#include "XLCoreQueue.h"
#include "XLCoreResource.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::webgpu {

Loop::~Loop() { }

bool Loop::init(NotNull<sprt::dispatch::Looper> looper, NotNull<core::Instance> instance,
		Rc<LoopInfo> &&info) {
	if (!core::Loop::init(looper, instance, move(info))) {
		return false;
	}

	looper->performOnThread([&] {
		auto makeDevice = [&]() -> Rc<Device> {
			if (auto dev = _instance.get_cast<Instance>()->makeDevice(*_info)) {
				return dev;
			}
			if (_info->deviceIdx != core::InstanceDefaultDevice) {
				log::source().warn("webgpu::Loop",
						"Unable to create device with index: ", _info->deviceIdx,
						", fallback to default");
				_info->deviceIdx = core::InstanceDefaultDevice;
				return _instance.get_cast<Instance>()->makeDevice(*_info);
			}
			return nullptr;
		};

		if (auto dev = makeDevice()) {
			_device = move(dev);
			_frameCache = Rc<FrameCache>::create(*this, *_device);
			_running = true;
		} else {
			log::source().error("webgpu::Loop", "Unable to create device");
		}
	}, this, true);

	return _device != nullptr;
}

void Loop::run() {
	_looper->performOnThread([&] {
		_updateTimerHandle = _looper->scheduleTimer(sprt::dispatch::TimerInfo{
			.completion = sprt::dispatch::TimerInfo::Completion::create<Loop>(this,
					[](Loop *loop, sprt::dispatch::TimerHandle *, uint32_t, Status) {
			if (loop->_device) {
				// deliver pending callbacks from completed device work
				wgpuDevicePoll(loop->_device->getDevice(), false, nullptr);
			}
			loop->updateFences();
			if (loop->_frameCache) {
				loop->_frameCache->clear();
			}
		}),
			.interval = TimeInterval::microseconds(config::PresentationSchedulerInterval),
			.count = sprt::dispatch::TimerInfo::Infinite,
		});
		_updateTimerHandle->setUserdata(this);
	}, this, true);
}

void Loop::scheduleFence(Rc<core::Fence> &&fence) {
	_looper->performOnThread([this, fence = sp::move(fence)]() mutable {
		_scheduledFences.emplace_back(sp::move(fence));
	}, this, true);
}

void Loop::updateFences() {
	// fence release callbacks can schedule new fences into _scheduledFences
	// (frame completion starts the next frame), iterate over a local copy
	auto fences = sp::move(_scheduledFences);
	_scheduledFences.clear();

	for (auto &it : fences) {
		if (!it->check(*this, true)) {
			_scheduledFences.emplace_back(sp::move(it));
		}
	}
}

void Loop::stop() {
	_looper->performOnThread([&] {
		_running = false;

		if (_device) {
			_device->waitIdle();
		}

		updateFences();
		if (!_scheduledFences.empty()) {
			for (auto &it : _scheduledFences) { it->check(*this, false); }
			_scheduledFences.clear();
		}

		if (_updateTimerHandle) {
			_updateTimerHandle->cancel();
			_updateTimerHandle = nullptr;
		}

		if (_frameCache) {
			_frameCache->invalidate();
		}

		if (_device) {
			_device->end();
			_device = nullptr;
		}
	}, this);
}

bool Loop::isRunning() const { return _running.load(); }

void Loop::compileResource(Rc<core::Resource> &&req, Function<void(bool)> &&cb,
		bool preload) const {
	// no transfer queue in WebGPU: direct queue-ordered uploads
	performOnThread([this, req = sp::move(req), cb = sp::move(cb)]() mutable {
		bool success = true;

		for (auto &it : req->getBuffers()) {
			if (!it->buffer) {
				if (auto buffer = Rc<Buffer>::create(*_device, *it)) {
					it->buffer = move(buffer);
				} else {
					log::source().error("webgpu::Loop", "Fail to create resource buffer: ",
							it->key);
					success = false;
				}
			}
		}

		for (auto &it : req->getImages()) {
			if (!it->image) {
				if (auto image = Rc<Image>::create(*_device, *it)) {
					it->image = move(image);
				} else {
					log::source().error("webgpu::Loop", "Fail to create resource image: ",
							it->key);
					success = false;
					continue;
				}
			}

			for (auto &view : it->views) {
				if (!view->view) {
					view->view = Rc<ImageView>::create(*_device, it->image,
							core::ImageViewInfo(*view));
				}
			}
		}

		req->setCompiled(success);

		if (cb) {
			cb(success);
		}
	}, const_cast<Loop *>(this), true);
}

void Loop::compileQueue(const Rc<Queue> &req, Function<void(bool)> &&cb) const {
	performOnThread([this, req, cb = sp::move(cb)]() mutable {
		bool success = true;

		if (!req->prepare(*_device)) {
			log::source().error("webgpu::Loop", "Fail to prepare queue: ", req->getName());
			if (cb) {
				cb(false);
			}
			return;
		}

		for (auto &it : req->getPrograms()) {
			if (auto shader = Rc<Shader>::create(*_device, *it)) {
				it->program = _device->addProgram(shader);
			} else {
				log::source().error("webgpu::Loop", "Fail to compile program: ", it->key);
				success = false;
			}
		}

		// upload internal resource buffers and images directly, no transfer queue required
		if (auto res = req->getInternalResource()) {
			for (auto &it : res->getBuffers()) {
				if (!it->buffer) {
					if (auto buffer = Rc<Buffer>::create(*_device, *it)) {
						it->buffer = move(buffer);
					} else {
						log::source().error("webgpu::Loop",
								"Fail to create resource buffer: ", it->key);
						success = false;
					}
				}
			}

			for (auto &it : res->getImages()) {
				if (!it->image) {
					if (auto image = Rc<Image>::create(*_device, *it)) {
						it->image = move(image);
					} else {
						log::source().error("webgpu::Loop",
								"Fail to create resource image: ", it->key);
						success = false;
						continue;
					}
				}

				for (auto &view : it->views) {
					if (!view->view) {
						view->view = Rc<ImageView>::create(*_device, it->image,
								core::ImageViewInfo(*view));
					}
				}
			}
		}

		// compile texture set layouts (samplers + bind group layout)
		if (success) {
			for (auto &it : req->getTextureSetLayouts()) {
				if (it->layout) {
					continue;
				}

				// the builder pre-sizes compiledSamplers with nulls, fill by index
				{
					memory::context ctx(it->queue->pool);
					if (it->compiledSamplers.size() < it->samplers.size()) {
						it->compiledSamplers.resize(it->samplers.size());
					}
					for (size_t i = 0; i < it->samplers.size(); ++i) {
						if (!it->compiledSamplers[i]) {
							it->compiledSamplers[i] = _device->getSampler(it->samplers[i]);
						}
					}
				}

				if (auto layout = Rc<TextureSetLayout>::create(*_device, *it)) {
					it->layout = move(layout);
				} else {
					log::source().error("webgpu::Loop",
							"Fail to compile texture set layout: ", it->key);
					success = false;
				}
			}
		}

		if (success) {
			// pipelines are stored per-subpass, queue-level tables are not populated
			for (auto &pass : req->getPasses()) {
				if (!pass->impl) {
					pass->impl = Rc<RenderPass>::create(*_device, *pass);
				}

				for (auto &subpass : pass->subpasses) {
					for (auto &it : subpass->graphicPipelines) {
						if (auto pipeline = Rc<GraphicPipeline>::create(*_device, *it)) {
							it->pipeline = move(pipeline);
						} else {
							log::source().error("webgpu::Loop",
									"Fail to compile pipeline: ", it->key);
							success = false;
						}
					}

					for (auto &it : subpass->computePipelines) {
						if (auto pipeline = Rc<ComputePipeline>::create(*_device, *it)) {
							it->pipeline = move(pipeline);
						} else {
							log::source().error("webgpu::Loop",
									"Fail to compile compute pipeline: ", it->key);
							success = false;
						}
					}
				}
			}
		}

		// compile predefined materials on material attachments
		if (success) {
			auto self = const_cast<Loop *>(this);
			for (auto &it : req->getAttachments()) {
				if (it->type != core::AttachmentType::Material) {
					continue;
				}

				auto a = dynamic_cast<core::MaterialAttachment *>(it->attachment.get());
				if (!a || !a->getTargetLayout() || !a->getTargetLayout()->layout) {
					continue;
				}

				a->setCompiled(*_device);

				auto initial = a->getPredefinedMaterials();
				if (initial.empty()) {
					continue;
				}

				auto imageCount =
						TextureSetLayout::getLayoutImageCount(*_device, *a->getTargetLayout());
				auto set = a->allocateSet(*_device, imageCount);

				self->updateMaterialSet(set.get(), initial, SpanView<core::MaterialId>(),
						SpanView<core::MaterialId>());

				a->setMaterials(set);
			}
		}

		if (success) {
			req->setCompiled(*_device, nullptr);
		}

		if (cb) {
			cb(success);
		}
	}, const_cast<Loop *>(this), true);
}

bool Loop::updateMaterialSet(NotNull<core::MaterialSet> data,
		SpanView<Rc<core::Material>> materials, SpanView<core::MaterialId> dynamicMaterials,
		SpanView<core::MaterialId> materialsToRemove) {
	// assign slots in material layouts, create image views on demand
	auto updated = data->updateMaterials(materials, dynamicMaterials, materialsToRemove,
			[&, this](const core::MaterialImage &image) -> Rc<core::ImageView> {
		for (auto &it : image.image->views) {
			if (*it == image.info || it->view->getInfo() == image.info) {
				return it->view;
			}
		}
		return Rc<ImageView>::create(*_device, image.image->image, image.info);
	});

	if (updated.empty()) {
		return false;
	}

	auto layout = data->getTargetLayout();

	// rebuild texture sets (bind groups are immutable, no in-place update)
	for (auto &it : data->getLayouts()) {
		it.set = layout->layout->acquireSet(*_device);
		it.set->write(it);
	}

	// upload per-material data buffers directly
	auto owner = data->getOwner();
	for (auto &it : updated) {
		auto bufferData = owner->getMaterialData(it.get());
		if (bufferData.empty()) {
			continue;
		}

		auto target = owner->allocateMaterialPersistentBuffer(it.get());
		if (!target) {
			log::source().error("webgpu::Loop",
					"Fail to allocate material buffer for material: ", it->getId());
			continue;
		}

		wgpuQueueWriteBuffer(_device->getQueue(),
				target.get_cast<Buffer>()->getBuffer(), 0, bufferData.data(),
				bufferData.size());
	}

	return true;
}

void Loop::compileMaterials(Rc<core::MaterialInputData> &&req,
		const Vector<Rc<DependencyEvent>> &deps) const {
	// deps are events OTHERS wait on: signal them when the update is applied
	auto loop = const_cast<Loop *>(this);
	loop->performOnThread([loop, req = sp::move(req), deps = deps]() mutable {
		bool success = false;
		auto attachment = req->attachment;
		if (!attachment->getMaterials()) {
			log::source().error("webgpu::Loop",
					"compileMaterials: attachment was not compiled with its queue");
		} else {
			auto newSet = attachment->cloneSet(attachment->getMaterials());

			loop->updateMaterialSet(newSet.get(), req->materialsToAddOrUpdate,
					req->dynamicMaterialsToUpdate, req->materialsToRemove);

			attachment->setMaterials(newSet);
			success = true;
		}

		if (req->callback) {
			req->callback();
		}

		loop->signalDependencies(deps, nullptr, success);
	}, loop, false);
	// NOT immediate: the caller (updateDynamicImage) holds the attachment's
	// dynamic-tracker mutex, updateMaterials re-locks it via addDynamicTracker
}

void Loop::compileImage(const Rc<core::DynamicImage> &image, Function<void(bool)> &&cb) const {
	performOnThread([this, image, cb = sp::move(cb)]() mutable {
		auto info = image->getInfo();

		auto img = Rc<Image>::create(*_device, info.key, core::ImageInfoData(info));
		if (!img) {
			log::source().error("webgpu::Loop", "compileImage: fail to create image");
			if (cb) {
				cb(false);
			}
			return;
		}

		// direct upload of the initial image content
		image->acquireData([&](BytesView data) {
			if (data.empty()) {
				return;
			}

			auto blockSize = core::getFormatBlockSize(info.format);

			WGPUTexelCopyTextureInfo dst = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
			dst.texture = img->getTexture();

			WGPUTexelCopyBufferLayout layout;
			layout.offset = 0;
			layout.bytesPerRow = info.extent.width * blockSize;
			layout.rowsPerImage = info.extent.height;

			WGPUExtent3D writeExtent{info.extent.width, info.extent.height, info.extent.depth};

			wgpuQueueWriteTexture(_device->getQueue(), &dst, data.data(), data.size(), &layout,
					&writeExtent);
		});

		image->setImage(img.get());

		if (cb) {
			cb(true);
		}
	}, const_cast<Loop *>(this), true);
}

void Loop::runRenderQueue(Rc<FrameRequest> &&req, uint64_t gen, Function<void(bool)> &&cb) {
	performOnThread([this, req = sp::move(req), gen, cb = sp::move(cb)]() mutable {
		if (!_running.load()) {
			return;
		}

		auto frame = makeFrame(move(req), gen);
		if (frame && cb) {
			frame->setCompleteCallback([this, cb = sp::move(cb)](FrameHandle &handle) {
				if (!_running.load()) {
					return;
				}
				cb(handle.isValid());
			});
		}
		if (frame) {
			frame->update(true);
		}
	}, this, true);
}

void Loop::performInQueue(Rc<sprt::dispatch::Task> &&task) const {
	if (!_running.load()) {
		task->cancel();
		return;
	}

	_looper->performAsync(move(task));
}

void Loop::performInQueue(Function<void()> &&func, Ref *target) const {
	if (!_running.load()) {
		return;
	}

	_looper->performAsync(sp::move(func), target);
}

void Loop::performOnThread(Function<void()> &&func, Ref *target, bool immediate,
		StringView tag) const {
	if (!_running.load()) {
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
	if (!_device) {
		return nullptr;
	}
	return Rc<FrameHandle>::create(*this, *_device, move(req), gen);
}

Rc<core::Framebuffer> Loop::acquireFramebuffer(const PassData *data,
		SpanView<Rc<core::ImageView>> views) {
	return _frameCache->acquireFramebuffer(data, views);
}

void Loop::releaseFramebuffer(Rc<core::Framebuffer> &&fb) {
	_frameCache->releaseFramebuffer(sp::move(fb));
}

auto Loop::acquireImage(const ImageAttachment *a, const AttachmentHandle *h,
		const core::ImageInfoData &i) -> Rc<ImageStorage> {
	auto views = a->getImageViews(i);
	return _frameCache->acquireImage(a->getId(), i, views);
}

void Loop::releaseImage(Rc<ImageStorage> &&image) {
	performOnThread([this, image = sp::move(image)]() mutable {
		_frameCache->releaseImage(sp::move(image));
	}, this, true);
}

Rc<core::Semaphore> Loop::makeSemaphore() {
	// WebGPU has no user-visible semaphores, queue submission order is tracked
	// by the implementation; state-only stub keeps frame graph bookkeeping happy
	return _device->makeSemaphore();
}

core::ImageFormat Loop::getCommonFormat() const { return _info->defaultFormat; }

SpanView<core::ImageFormat> Loop::getSupportedDepthStencilFormat() const {
	return _device->getSupportedDepthStencilFormat();
}

Rc<core::Fence> Loop::acquireFence(core::FenceType type) {
	auto fence = Rc<Fence>::create(*_device, type);
	if (!fence) {
		return nullptr;
	}

	fence->setFrame([guard = Rc<Loop>(this), f = fence.get()]() mutable {
		guard->scheduleFence(Rc<core::Fence>(f));
		return true;
	}, [] { }, 0);

	return fence;
}

void Loop::signalDependencies(const Vector<Rc<DependencyEvent>> &deps, Queue *q, bool success) {
	if (deps.empty()) {
		return;
	}

	performOnThread([this, deps, q = Rc<Queue>(q), success]() {
		for (auto &ev : deps) {
			if (!ev->signal(q, success)) {
				continue;
			}

			auto it = _dependencyRequests.find(ev.get());
			if (it == _dependencyRequests.end()) {
				continue;
			}

			auto requests = sp::move(it->second);
			_dependencyRequests.erase(it);

			for (auto &req : requests) {
				if (!success) {
					req->success = false;
				}
				++req->signaled;
				if (req->signaled == uint32_t(req->events.size()) && req->callback) {
					req->callback(req->success);
					req->callback = nullptr;
				}
			}
		}
	}, const_cast<Loop *>(this), true);
}

void Loop::waitForDependencies(const Vector<Rc<DependencyEvent>> &deps,
		Function<void(bool)> &&cb) {
	if (deps.empty()) {
		if (cb) {
			cb(true);
		}
		return;
	}

	performOnThread([this, deps, cb = sp::move(cb)]() mutable {
		auto req = Rc<DependencyRequest>::alloc();
		req->events = deps;
		req->callback = sp::move(cb);

		for (auto &ev : req->events) {
			if (ev->isSignaled()) {
				if (!ev->isSuccessful()) {
					req->success = false;
				}
				++req->signaled;
			} else {
				auto it = _dependencyRequests.find(ev.get());
				if (it == _dependencyRequests.end()) {
					_dependencyRequests.emplace(ev.get(), Vector<Rc<DependencyRequest>>{req});
				} else {
					it->second.emplace_back(req);
				}
			}
		}

		if (req->signaled == uint32_t(req->events.size()) && req->callback) {
			req->callback(req->success);
			req->callback = nullptr;
		}
	}, const_cast<Loop *>(this), true);
}

void Loop::drain(Function<void()> &&cb) {
	performOnThread([this, cb = sp::move(cb)]() mutable {
		if (!_device) {
			if (cb) {
				cb();
			}
			return;
		}
		_device->drain([this, cb = sp::move(cb)]() mutable {
			// the device callback may arrive from any polling context
			performOnThread([cb = sp::move(cb)] {
				if (cb) {
					cb();
				}
			}, this, true);
		});
	}, this, true);
}

void Loop::waitIdle() {
	performOnThread([this] {
		if (_device) {
			_device->waitIdle();
		}
	}, this, true);
}

void Loop::captureImage(Function<void(const core::ImageInfoData &info, BytesView view)> &&cb,
		const Rc<core::ImageObject> &image, core::AttachmentLayout l) {
	performOnThread([this, cb = sp::move(cb), image]() mutable {
		auto info = image->getInfo();
		auto blockSize = core::getFormatBlockSize(info.format);

		// bytesPerRow must be 256-aligned for texture-to-buffer copies
		const uint64_t bytesPerRow = math::align(uint64_t(info.extent.width) * blockSize,
				uint64_t(256));
		const uint64_t bufferSize = bytesPerRow * info.extent.height;

		WGPUBufferDescriptor bufDesc = WGPU_BUFFER_DESCRIPTOR_INIT;
		bufDesc.label = WGPUStringView{"captureImage", WGPU_STRLEN};
		bufDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead;
		bufDesc.size = bufferSize;
		auto buffer = wgpuDeviceCreateBuffer(_device->getDevice(), &bufDesc);

		auto encoder = wgpuDeviceCreateCommandEncoder(_device->getDevice(), nullptr);

		WGPUTexelCopyTextureInfo copySrc;
		copySrc.texture = static_cast<Image *>(image.get())->getTexture();
		copySrc.mipLevel = 0;
		copySrc.origin = WGPUOrigin3D{0, 0, 0};
		copySrc.aspect = WGPUTextureAspect_All;

		WGPUTexelCopyBufferInfo copyDst;
		copyDst.layout.offset = 0;
		copyDst.layout.bytesPerRow = uint32_t(bytesPerRow);
		copyDst.layout.rowsPerImage = info.extent.height;
		copyDst.buffer = buffer;

		WGPUExtent3D copySize{info.extent.width, info.extent.height, 1};
		wgpuCommandEncoderCopyTextureToBuffer(encoder, &copySrc, &copyDst, &copySize);

		auto commands = wgpuCommandEncoderFinish(encoder, nullptr);
		wgpuCommandEncoderRelease(encoder);

		wgpuQueueSubmit(_device->getQueue(), 1, &commands);
		wgpuCommandBufferRelease(commands);

		// fully asynchronous readback (the only model a browser offers): the
		// map callback is delivered by the loop's regular device poll (or by
		// the event loop in a browser build), nothing blocks here
		struct CaptureContext {
			Rc<Loop> loop;
			Function<void(const core::ImageInfoData &, BytesView)> callback;
			WGPUBuffer buffer;
			core::ImageInfoData info;
			uint64_t bytesPerRow;
			uint64_t bufferSize;
			uint32_t blockSize;
		};

		auto ctx = new CaptureContext{this, sp::move(cb), buffer, info, bytesPerRow, bufferSize,
			uint32_t(blockSize)};

		WGPUBufferMapCallbackInfo mapCallback = WGPU_BUFFER_MAP_CALLBACK_INFO_INIT;
		mapCallback.mode = WGPUCallbackMode_AllowProcessEvents;
		mapCallback.callback = [](WGPUMapAsyncStatus status, WGPUStringView message,
				void *userdata1, void *) {
			auto ctx = reinterpret_cast<CaptureContext *>(userdata1);
			ctx->loop->performOnThread([ctx, status] {
				if (status == WGPUMapAsyncStatus_Success) {
					auto mapped = reinterpret_cast<const uint8_t *>(
							wgpuBufferGetConstMappedRange(ctx->buffer, 0, ctx->bufferSize));

					// repack tightly (drop row alignment padding)
					const auto rowBytes = size_t(ctx->info.extent.width) * ctx->blockSize;
					Bytes data;
					data.resize(rowBytes * ctx->info.extent.height);
					for (uint32_t row = 0; row < ctx->info.extent.height; ++row) {
						sprt::memcpy(data.data() + size_t(row) * rowBytes,
								mapped + row * ctx->bytesPerRow, rowBytes);
					}

					wgpuBufferUnmap(ctx->buffer);

					ctx->callback(ctx->info, data);
				} else {
					log::source().error("webgpu::Loop",
							"captureImage: fail to map readback buffer");
					ctx->callback(ctx->info, BytesView());
				}

				wgpuBufferRelease(ctx->buffer);
				delete ctx;
			}, ctx->loop.get(), true);
		};
		mapCallback.userdata1 = ctx;

		wgpuBufferMapAsync(buffer, WGPUMapMode_Read, 0, bufferSize, mapCallback);
	}, this, true);
}

void Loop::captureBuffer(Function<void(const core::BufferInfo &info, BytesView view)> &&cb,
		const Rc<core::BufferObject> &) {
	log::source().error("webgpu::Loop", "captureBuffer is not implemented");
}

Rc<core::PresentationEngine> Loop::makePresentationEngine(
		NotNull<core::PresentationWindow> window, core::PresentationOptions opts) {
	return Rc<PresentationEngine>::create(this, _device.get(), window, opts);
}

} // namespace stappler::xenolith::webgpu
