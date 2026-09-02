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

#include "XLGlesLoop.h"
#include "XLGlesObject.h"
#include "XLGlesPipeline.h"
#include "XLGlesTextureSet.h"
#include "XLGlesQueuePass.h"
#include "XLGlesPresentation.h"
#include "XLGlesHeadlessPresentation.h"
#include "XLGlesWindowedPresentation.h"

#include "XLCoreFrameHandle.h"
#include "XLCoreFrameRequest.h"
#include "XLCoreResource.h"
#include "XLCoreDynamicImage.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::gles {

bool Loop::init(NotNull<sprt::dispatch::Looper> looper, NotNull<core::Instance> instance,
		Rc<LoopInfo> &&info) {
	if (!core::Loop::init(looper, instance, move(info))) {
		return false;
	}

	looper->performOnThread([&] {
		if (auto dev = _instance.get_cast<Instance>()->makeDevice(*_info)) {
			_device = move(dev);
			_frameCache = Rc<FrameCache>::create(*this, *_device);
			_running = true;
		} else {
			log::source().error("gles::Loop", "Unable to create device");
		}
	}, this, true);

	return _device != nullptr;
}

void Loop::run() {
	_looper->performOnThread([&] {
		// The context is single-threaded and nothing else has to be polled: the timer exists to
		// retire fences (a GLsync reports itself through glClientWaitSync) and to let the frame
		// cache drop what the last frames stopped using.
		_updateTimerHandle = _looper->scheduleTimer(sprt::dispatch::TimerInfo{
			.completion = sprt::dispatch::TimerInfo::Completion::create<Loop>(this,
					[](Loop *loop, sprt::dispatch::TimerHandle *, uint32_t, Status) {
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
	// A fence release callback can schedule new fences (frame completion starts the next frame),
	// so iterate a local copy.
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

void Loop::compileResource(Rc<core::Resource> &&req, Function<void(bool)> &&cb, bool preload) const {
	// "Uploading" a resource is filling GL storage: the factories bind and push the bytes as part
	// of creation, so there is no separate transfer step to schedule.
	performOnThread([this, req = sp::move(req), cb = sp::move(cb)]() mutable {
		bool success = true;

		for (auto &it : req->getBuffers()) {
			if (!it->buffer) {
				if (auto buffer = Rc<Buffer>::create(*_device, *it)) {
					it->buffer = move(buffer);
				} else {
					log::source().error("gles::Loop", "Fail to create resource buffer: ", it->key);
					success = false;
				}
			}
		}

		for (auto &it : req->getImages()) {
			if (!it->image) {
				if (auto image = Rc<Image>::create(*_device, *it)) {
					it->image = move(image);
				} else {
					log::source().error("gles::Loop", "Fail to create resource image: ", it->key);
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

void Loop::compileQueue(const Rc<core::Queue> &req, Function<void(bool)> &&cb) const {
	performOnThread([this, req, cb = sp::move(cb)]() mutable {
		bool success = true;

		if (!req->prepare(*_device)) {
			log::source().error("gles::Loop", "Fail to prepare queue: ", req->getName());
			if (cb) {
				cb(false);
			}
			return;
		}

		for (auto &it : req->getPrograms()) {
			if (!it->program) {
				if (auto shader = Rc<Shader>::create(*_device, *it)) {
					it->program = _device->addProgram(shader);
				} else {
					log::source().error("gles::Loop", "Fail to compile program: ", it->key);
					success = false;
				}
			}
		}

		if (auto res = req->getInternalResource()) {
			for (auto &it : res->getBuffers()) {
				if (!it->buffer) {
					if (auto buffer = Rc<Buffer>::create(*_device, *it)) {
						it->buffer = move(buffer);
					} else {
						log::source().error("gles::Loop", "Fail to create resource buffer: ",
								it->key);
						success = false;
					}
				}
			}

			for (auto &it : res->getImages()) {
				if (!it->image) {
					if (auto image = Rc<Image>::create(*_device, *it)) {
						it->image = move(image);
					} else {
						log::source().error("gles::Loop", "Fail to create resource image: ",
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
		}

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
					log::source().error("gles::Loop", "Fail to compile texture set layout: ",
							it->key);
					success = false;
				}
			}
		}

		if (success) {
			for (auto &pass : req->getPasses()) {
				if (!pass->impl) {
					pass->impl = Rc<RenderPass>::create(*_device, *pass);
				}

				for (auto &subpass : pass->subpasses) {
					for (auto &it : subpass->graphicPipelines) {
						if (!it->pipeline) {
							if (auto pipeline = Rc<GraphicPipeline>::create(*_device, *it)) {
								it->pipeline = move(pipeline);
							} else {
								log::source().error("gles::Loop", "Fail to compile pipeline: ",
										it->key);
								success = false;
							}
						}
					}

					// Compute is not part of the flat contract; a queue that asks for it is not one
					// this backend can execute, and silently ignoring it would render a half-correct
					// frame instead of saying so.
					if (!subpass->computePipelines.empty()) {
						log::source().error("gles::Loop",
								"Compute pipelines are not supported by the GLES backend: ",
								subpass->key);
						success = false;
					}
				}
			}
		}

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

				auto set = a->allocateSet(*_device, a->getTargetLayout()->imageCountIndexed);

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

	for (auto &it : data->getLayouts()) {
		it.set = layout->layout->acquireSet(*_device);
		it.set->write(it);
	}

	// The software backend fills a per-material buffer here for its CPU rasterizer; the GLES draw
	// path reads a material's image and pipeline directly at record time, so it has no such data.

	return true;
}

void Loop::compileMaterials(Rc<core::MaterialInputData> &&req,
		const Vector<Rc<DependencyEvent>> &deps) const {
	auto loop = const_cast<Loop *>(this);
	loop->performOnThread([loop, req = sp::move(req), deps = deps]() mutable {
		bool success = false;
		auto attachment = req->attachment;
		if (!attachment->getMaterials()) {
			log::source().error("gles::Loop",
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
	// NOT immediate: the caller (updateDynamicImage) holds the attachment's dynamic-tracker mutex
	// and signalling walks it through the dependency graph - same reason as the software backend.
}

void Loop::compileImage(const Rc<core::DynamicImage> &image, Function<void(bool)> &&cb) const {
	performOnThread([this, image, cb = sp::move(cb)]() mutable {
		auto info = image->getInfo();

		Rc<Image> img;
		image->acquireData([&](BytesView data) {
			img = Rc<Image>::create(*_device, info.key, core::ImageInfoData(info), data);
		});
		if (!img) {
			log::source().error("gles::Loop", "compileImage: fail to create image");
			if (cb) {
				cb(false);
			}
			return;
		}

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

auto Loop::makeFrame(Rc<core::FrameRequest> &&req, uint64_t gen) -> Rc<FrameHandle> {
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

Rc<core::Semaphore> Loop::makeSemaphore() { return _device->makeSemaphore(); }

core::ImageFormat Loop::getCommonFormat() const { return _info->defaultFormat; }

SpanView<core::ImageFormat> Loop::getSupportedDepthStencilFormat() const {
	return _device ? _device->getSupportedDepthStencilFormat() : SpanView<core::ImageFormat>();
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

void Loop::waitForDependencies(const Vector<Rc<DependencyEvent>> &deps, Function<void(bool)> &&cb) {
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
		auto img = image.get_cast<Image>();
		if (!img || !_device) {
			log::source().error("gles::Loop", "captureImage: not a GLES image");
			cb(core::ImageInfoData(), BytesView());
			return;
		}

		auto &table = _device->getTable();
		const auto info = img->getInfo();
		const uint32_t width = uint32_t(info.extent.width);
		const uint32_t height = uint32_t(info.extent.height);

		if (!table.glGenFramebuffers || !table.glReadPixels) {
			log::source().error("gles::Loop", "captureImage: GL entrypoints are missing");
			cb(core::ImageInfoData(), BytesView());
			return;
		}

		GLuint fbo = 0;
		table.glGenFramebuffers(1, &fbo);
		bool ok = false;
		if (fbo != 0) {
			table.glBindFramebuffer(GL_FRAMEBUFFER, fbo);
			table.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
					img->getGlName(), 0);
			if (table.glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE) {
				_captureStorage.resize(size_t(width) * size_t(height) * 4, uint8_t(0));
				table.glReadPixels(0, 0, GLsizei(width), GLsizei(height), GL_RGBA,
						GL_UNSIGNED_BYTE, _captureStorage.data());
				ok = true;
			} else {
				log::source().error("gles::Loop", "captureImage: incomplete readback framebuffer");
			}
			table.glBindFramebuffer(GL_FRAMEBUFFER, 0);

			// init and readback both run on the loop thread, where the context is current - a
			// direct delete here is safe without going through the deferred queue.
			table.glDeleteFramebuffers(1, &fbo);
		}

		if (!ok) {
			cb(core::ImageInfoData(), BytesView());
			return;
		}

		// No row flip: glReadPixels hands back rows starting at GL row 0, and GL row 0 is where
		// this backend puts the image's TOP row (the vertex shader does not mirror the geometry -
		// see XL2dGlesFlatPass.cc), so what comes out is already host order.
		cb(info, BytesView(_captureStorage));
	}, this, true);
}

void Loop::captureBuffer(Function<void(const core::BufferInfo &info, BytesView view)> &&cb,
		const Rc<core::BufferObject> &buffer) {
	performOnThread([this, cb = sp::move(cb), buffer]() mutable {
		auto buf = buffer.get_cast<Buffer>();
		if (!buf || !_device) {
			log::source().error("gles::Loop", "captureBuffer: not a GLES buffer");
			cb(core::BufferInfo(), BytesView());
			return;
		}

		auto &table = _device->getTable();
		const auto info = buf->getInfo();
		if (!table.glBindBuffer || !table.glMapBufferRange) {
			log::source().error("gles::Loop", "captureBuffer: GL entrypoints are missing");
			cb(core::BufferInfo(), BytesView());
			return;
		}

		bool ok = false;
		table.glBindBuffer(GL_COPY_WRITE_BUFFER, buf->getGlName());
		if (void *mem = table.glMapBufferRange(GL_COPY_WRITE_BUFFER, 0, GLsizei(info.size),
				GL_MAP_READ_BIT)) {
			_captureStorage.resize(size_t(info.size), uint8_t(0));
			sprt::memcpy(_captureStorage.data(), mem, size_t(info.size));
			table.glUnmapBuffer(GL_COPY_WRITE_BUFFER);
			ok = true;
		} else {
			log::source().error("gles::Loop", "captureBuffer: fail to map buffer");
		}
		table.glBindBuffer(GL_COPY_WRITE_BUFFER, 0);

		if (!ok) {
			cb(core::BufferInfo(), BytesView());
			return;
		}

		cb(info, BytesView(_captureStorage));
	}, this, true);
}

Rc<core::PresentationEngine> Loop::makePresentationEngine(NotNull<core::PresentationWindow> w,
		core::PresentationOptions opts) {
	if (opts.headless) {
		return Rc<HeadlessPresentationEngine>::create(this, _device.get(), w, opts);
	}
	return Rc<WindowedPresentationEngine>::create(this, _device.get(), w, opts);
}

} // namespace stappler::xenolith::gles
