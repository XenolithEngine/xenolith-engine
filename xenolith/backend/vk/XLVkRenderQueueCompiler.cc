/**
 Copyright (c) 2021-2022 Roman Katuntsev <sbkarr@stappler.org>
 Copyright (c) 2025 Stappler LLC <admin@stappler.dev>
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

#include "XLVkAttachment.h"
#include "XLVkPipeline.h"
#include "XLVkTextureSet.h"
#include "XLVkRenderPass.h"
#include "XLVkRenderQueueCompiler.h"
#include "XLVkTransferQueue.h"
#include "XLVkMaterialCompiler.h"
#include "XLCoreFrameQueue.h"
#include "XLCoreFrameCache.h"
#include "XLCoreFrameRequest.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk {

class RenderQueueAttachment : public core::GenericAttachment {
public:
	virtual ~RenderQueueAttachment();

	virtual Rc<core::AttachmentHandle> makeFrameHandle(const FrameQueue &) override;
};

class RenderQueueAttachmentHandle : public core::AttachmentHandle {
public:
	virtual ~RenderQueueAttachmentHandle();

	virtual bool setup(FrameQueue &handle, Function<void(bool)> &&) override;
	virtual void submitInput(FrameQueue &, Rc<core::AttachmentInputData> &&,
			Function<void(bool)> &&) override;

	StringView getTargetQueueName() const { return _targetQueueName; }
	const Rc<core::Queue> &getRenderQueue() const { return _input->queue; }
	const Rc<TransferResource> &getTransferResource() const { return _resource; }

protected:
	void runShaders(FrameHandle &frame);
	void runPasses(FrameHandle &frame);
	void runPipelines(FrameHandle &frame);

	void runLayoutCallback();

	struct SamplersCompilationData : public Ref {
		std::atomic<uint32_t> samplersInProcess = 0;
		core::TextureSetLayoutData *layout = nullptr;
		Rc<Device> device;

		bool setSampler(uint32_t, Rc<Sampler> &&);
	};

	Device *_device = nullptr;

	bool _resourceCompiled = false;
	bool _layoutsCompiled = false;
	Function<void(bool)> _layoutCallback;
	std::atomic<size_t> _layoutsInQueue = 0;
	std::atomic<size_t> _programsInQueue = 0;
	std::atomic<size_t> _pipelinesInQueue = 0;
	Rc<TransferResource> _resource;
	Rc<RenderQueueInput> _input;
	String _targetQueueName;
};

class RenderQueuePass : public QueuePass {
public:
	virtual ~RenderQueuePass();

	virtual bool init(QueuePassBuilder &, const AttachmentData *);

	virtual Rc<QueuePassHandle> makeFrameHandle(const FrameQueue &) override;

	const AttachmentData *getAttachment() const { return _attachment; }

protected:
	using QueuePass::init;

	const AttachmentData *_attachment = nullptr;
};

class RenderQueuePassHandle : public QueuePassHandle {
public:
	virtual ~RenderQueuePassHandle();

	virtual bool init(QueuePass &, const FrameQueue &) override;

	virtual bool prepare(FrameQueue &, Function<void(bool)> &&) override;
	virtual void submit(FrameQueue &, Rc<FrameSync> &&, Function<void(bool)> &&onSubmited,
			Function<void(bool)> &&onComplete) override;

	virtual void finalize(FrameQueue &, bool successful) override;

protected:
	virtual bool prepareMaterials(FrameHandle &frame, CommandBuffer &buf,
			core::MaterialAttachment *attachment, Vector<BufferMemoryBarrier> &barriers);

	Rc<TransferResource> _resource;
	Rc<core::Queue> _queue;
	RenderQueueAttachmentHandle *_attachment;
};

bool RenderQueueCompiler::init(Device &dev, TransferQueue *transfer, MaterialCompiler *compiler) {
	using namespace core;

	Queue::Builder builder("RenderQueueCompiler");

	auto attachment = builder.addAttachemnt("RenderQueueAttachment",
			[&](AttachmentBuilder &attachmentBuilder) -> Rc<Attachment> {
		attachmentBuilder.defineAsInput();
		attachmentBuilder.defineAsOutput();
		return Rc<RenderQueueAttachment>::create(attachmentBuilder);
	});

	builder.addPass("RenderQueueRenderPass", PassType::Transfer, RenderOrdering(0),
			[&](QueuePassBuilder &passBuilder) -> Rc<core::QueuePass> {
		return Rc<RenderQueuePass>::create(passBuilder, attachment);
	});

	if (Queue::init(move(builder))) {
		_attachment = attachment;

		prepare(dev);

		for (auto &it : getPasses()) {
			auto pass = Rc<RenderPass>::create(dev, *it);
			it->impl = pass.get();
		}

		_transfer = transfer;
		_materialCompiler = compiler;
		return true;
	}
	return false;
}

auto RenderQueueCompiler::makeRequest(Rc<RenderQueueInput> &&input) -> Rc<FrameRequest> {
	auto ret = Rc<FrameRequest>::create(this);
	ret->addInput(_attachment, move(input));
	return ret;
}

RenderQueueAttachment::~RenderQueueAttachment() { }

auto RenderQueueAttachment::makeFrameHandle(const FrameQueue &handle) -> Rc<AttachmentHandle> {
	return Rc<RenderQueueAttachmentHandle>::create(this, handle);
}

RenderQueueAttachmentHandle::~RenderQueueAttachmentHandle() { }

bool RenderQueueAttachmentHandle::setup(FrameQueue &handle, Function<void(bool)> &&) {
	_device = static_cast<Device *>(handle.getFrame()->getDevice());
	return true;
}

void RenderQueueAttachmentHandle::submitInput(FrameQueue &q, Rc<core::AttachmentInputData> &&data,
		Function<void(bool)> &&cb) {
	_input = (RenderQueueInput *)data.get();
	_targetQueueName = _input->queue->getName().str<Interface>();

	if (!_input || q.isFinalized()) {
		cb(false);
		return;
	}

	q.getFrame()->waitForDependencies(data->waitDependencies,
			[this, cb = sp::move(cb)](FrameHandle &handle, bool success) mutable {
		if (!success || !handle.isValidFlag()) {
			cb(false);
			return;
		}

		_layoutCallback = sp::move(cb);

		if (_input->queue->getInternalResource()) {
			_resourceCompiled = false;
			handle.performInQueue(
					[this](FrameHandle &frame) -> bool {
				_resource = Rc<TransferResource>::create(_device->getAllocator(),
						_input->queue->getInternalResource());
				if (_resource->initialize()) {
					return true;
				}
				return false;
			},
					[this](FrameHandle &frame, bool success) {
				// finalize input receiving
				_resourceCompiled = true;
				runLayoutCallback();
			}, nullptr,
					"RenderQueueAttachmentHandle::submitInput _input->queue->getInternalResource");
		} else {
			_resourceCompiled = true;
		}

		runShaders(handle);
	});
}

void RenderQueueAttachmentHandle::runShaders(FrameHandle &frame) {
	size_t tasksCount = 0;
	Vector<core::ProgramData *> programs;

	_input->queue->prepare(*_device);

	_layoutsInQueue = _input->queue->getTextureSetLayouts().size();

	for (auto &it : _input->queue->getTextureSetLayouts()) {
		auto ref = Rc<SamplersCompilationData>::alloc();
		ref->layout = it;
		ref->device = _device;
		ref->samplersInProcess = uint32_t(it->samplers.size());

		uint32_t i = 0;
		for (auto &iit : it->samplers) {
			frame.performRequiredTask(
					[this, req = iit, ref, i](FrameHandle &frame) {
				if (ref->setSampler(i, Rc<Sampler>::create(*_device, req))) {
					if (_layoutsInQueue.fetch_sub(1) == 1) {
						_layoutsCompiled = true;
						runLayoutCallback();
						runPasses(frame);
					}
				}
				return true;
			}, this,
					toString("RenderQueueAttachmentHandle::runShaders - compile samplers: ",
							_targetQueueName, "::", it->key));
			++i;
		}
	}

	// count phase-1 tasks
	_programsInQueue += _input->queue->getPasses().size();
	tasksCount += _input->queue->getPasses().size();

	for (auto &it : _input->queue->getPrograms()) {
		if (auto p = _device->getProgram(it->key)) {
			it->program = p;
		} else {
			++tasksCount;
			++_programsInQueue;
			programs.emplace_back(it);
		}
	}

	for (auto &it : programs) {
		frame.performRequiredTask(
				[this, req = it](FrameHandle &frame) {
			auto ret = Rc<Shader>::create(*_device, *req);
			if (!ret) {
				log::source().error("RenderQueueAttachmentHandle",
						"Fail to compile shader program ", req->key);
				return false;
			} else {
				req->program = _device->addProgram(ret);
				if (_programsInQueue.fetch_sub(1) == 1) {
					runPipelines(frame);
				}
			}
			return true;
		}, this,
				toString("RenderQueueAttachmentHandle::runShaders - compile shader: ",
						_targetQueueName, "::", it->key));
	}

	if (_input->queue->getTextureSetLayouts().size() == 0
			&& _input->queue->getPasses().size() > 0) {
		_layoutsCompiled = true;
		runLayoutCallback();
		runPasses(frame);
	}

	if (tasksCount == 0) {
		runPipelines(frame);
	}
}

void RenderQueueAttachmentHandle::runPasses(FrameHandle &frame) {
	for (auto &it : _input->queue->getPasses()) {
		frame.performRequiredTask(
				[this, req = it](FrameHandle &frame) -> bool {
			auto ret = Rc<RenderPass>::create(*_device, *req);
			if (!ret) {
				log::source().error("RenderQueueAttachmentHandle", "Fail to compile render pass ",
						req->key);
				return false;
			} else {
				req->impl = ret.get();
				if (_programsInQueue.fetch_sub(1) == 1) {
					runPipelines(frame);
				}
			}
			return true;
		}, this,
				toString("RenderQueueAttachmentHandle::runShaders - compile pass: ",
						_targetQueueName, "::", it->key));
	}
}

void RenderQueueAttachmentHandle::runPipelines(FrameHandle &frame) {
	[[maybe_unused]]
	size_t tasksCount = _pipelinesInQueue.load();
	for (auto &pit : _input->queue->getPasses()) {
		for (auto &sit : pit->subpasses) {
			_pipelinesInQueue += sit->graphicPipelines.size() + sit->computePipelines.size();
			tasksCount += sit->graphicPipelines.size() + sit->computePipelines.size();
		}
	}

	for (auto &pit : _input->queue->getPasses()) {
		for (auto &sit : pit->subpasses) {
			for (auto &it : sit->graphicPipelines) {
				frame.performRequiredTask(
						[this, pass = sit, pipeline = it](FrameHandle &frame) -> bool {
					auto ret =
							Rc<GraphicPipeline>::create(*_device, *pipeline, *pass, *_input->queue);
					if (!ret) {
						log::source().error("RenderQueueAttachmentHandle",
								"Fail to compile pipeline ", pipeline->key);
						return false;
					} else {
						pipeline->pipeline = ret.get();
					}
					return true;
				}, this,
						toString(
								"RenderQueueAttachmentHandle::runPipelines - compile " "graphic " "pipeline: ",
								_targetQueueName, "::", it->key));
			}
			for (auto &it : sit->computePipelines) {
				frame.performRequiredTask(
						[this, pass = sit, pipeline = it](FrameHandle &frame) -> bool {
					auto ret =
							Rc<ComputePipeline>::create(*_device, *pipeline, *pass, *_input->queue);
					if (!ret) {
						log::source().error("RenderQueueAttachmentHandle",
								"Fail to compile pipeline ", pipeline->key);
						return false;
					} else {
						pipeline->pipeline = ret.get();
					}
					return true;
				}, this,
						toString(
								"RenderQueueAttachmentHandle::runPipelines - compile " "compute " "pipeline: ",
								_targetQueueName, "::", it->key));
			}
		}
	}
}

void RenderQueueAttachmentHandle::runLayoutCallback() {
	if (_layoutsCompiled && _resourceCompiled) {
		if (_layoutCallback) {
			_layoutCallback(true);
			_layoutCallback = nullptr;
		}
	}
}

bool RenderQueueAttachmentHandle::SamplersCompilationData::setSampler(uint32_t i,
		Rc<Sampler> &&sampler) {
	layout->compiledSamplers[i] = move(sampler);
	if (samplersInProcess.fetch_sub(1) == 1) {
		layout->layout = Rc<TextureSetLayout>::create(*device, *layout);
		return true;
	}
	return false;
}

RenderQueuePass::~RenderQueuePass() { }

bool RenderQueuePass::init(QueuePassBuilder &passBuilder, const AttachmentData *attachment) {
	passBuilder.addAttachment(attachment);

	if (!QueuePass::init(passBuilder)) {
		return false;
	}

	_attachment = attachment;
	return true;
}

auto RenderQueuePass::makeFrameHandle(const FrameQueue &handle) -> Rc<QueuePassHandle> {
	return Rc<RenderQueuePassHandle>::create(*this, handle);
}

RenderQueuePassHandle::~RenderQueuePassHandle() {
	if (_resource) {
		_resource->invalidate(*_device);
	}
}

bool RenderQueuePassHandle::init(QueuePass &pass, const FrameQueue &queue) {
	if (!QueuePassHandle::init(pass, queue)) {
		return false;
	}

	return true;
}

bool RenderQueuePassHandle::prepare(FrameQueue &frame, Function<void(bool)> &&cb) {
	if (auto a = frame.getAttachment(
				static_cast<RenderQueuePass *>(_queuePass.get())->getAttachment())) {
		_attachment = static_cast<RenderQueueAttachmentHandle *>(a->handle.get());
	}

	_device = static_cast<Device *>(frame.getFrame()->getDevice());
	_queue = _attachment->getRenderQueue();

	prepareSubpasses(frame);

	auto hasMaterials = false;
	auto &res = _attachment->getTransferResource();
	for (auto &it : _queue->getAttachments()) {
		if (auto v = it->attachment.cast<core::MaterialAttachment>()) {

			v->setCompiler(
					static_cast<RenderQueueCompiler *>(_data->queue->queue)->getMaterialCompiler());

			if (!v->getPredefinedMaterials().empty()) {
				hasMaterials = true;
				break;
			}
		}
	}

	if (res || hasMaterials) {
		_resource = res;
		_pool = static_cast<CommandPool *>(
				_device->acquireCommandPool(core::QueueFlags::Transfer).get());
		if (!_pool) {
			invalidate();
			return false;
		}

		frame.getFrame()->performInQueue([this, hasMaterials](FrameHandle &frame) {
			auto buf = _pool->recordBuffer(*_device, Vector<Rc<DescriptorPool>>(_descriptors),
					[&, this](CommandBuffer &buf) {
				Vector<ImageMemoryBarrier> outputImageBarriers;
				Vector<BufferMemoryBarrier> outputBufferBarriers;

				if (_resource) {
					if (!_resource->prepareCommands(_pool->getFamilyIdx(), buf, outputImageBarriers,
								outputBufferBarriers)) {
						log::source().error("vk::RenderQueueCompiler",
								"Fail to compile resource for ", _queue->getName());
						return false;
					}
					_resource->compile();
				}

				if (hasMaterials) {
					for (auto &it : _queue->getAttachments()) {
						if (auto v = it->attachment.cast<core::MaterialAttachment>()) {
							if (!prepareMaterials(frame, buf, v, outputBufferBarriers)) {
								log::source().error("vk::RenderQueueCompiler",
										"Fail to compile predefined materials for ",
										_queue->getName());
								return false;
							}
						}
					}
				}

				buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT,
						VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, outputBufferBarriers,
						outputImageBarriers);
				return true;
			});

			if (buf) {
				_buffers.emplace_back(buf);
			}
			return true;
		}, [this, cb = sp::move(cb)](FrameHandle &frame, bool success) {
			if (success) {
				_commandsReady = true;
				_descriptorsReady = true;
			} else {
				log::source().error("VK-Error", "Fail to doPrepareCommands");
			}
			cb(success);
		}, this, "RenderPass::doPrepareCommands _attachment->getTransferResource");
	} else {
		frame.getFrame()->performOnGlThread([cb = sp::move(cb)](FrameHandle &frame) { cb(true); },
				this, false, "RenderPass::doPrepareCommands");
	}

	return false;
}

void RenderQueuePassHandle::submit(FrameQueue &queue, Rc<FrameSync> &&sync,
		Function<void(bool)> &&onSubmited, Function<void(bool)> &&onComplete) {
	if (_buffers.empty()) {
		onSubmited(true);
		onComplete(true);
	} else {
		QueuePassHandle::submit(queue, sp::move(sync), sp::move(onSubmited), sp::move(onComplete));
	}
}

void RenderQueuePassHandle::finalize(FrameQueue &frame, bool successful) {
	QueuePassHandle::finalize(frame, successful);

	if (!_attachment || !successful) {
		log::source().error("RenderQueueCompiler", "Fail to compile render queue");
		return;
	}

	Vector<uint64_t> passIds;
	auto cache = frame.getLoop()->getFrameCache();
	for (auto &it : _attachment->getRenderQueue()->getPasses()) {
		if (it->impl && it->pass->getType() != core::PassType::Generic) {
			passIds.emplace_back(it->impl->getIndex());
			cache->addRenderPass(it->impl->getIndex());
		}
	}

	Vector<uint64_t> attachmentIds;
	for (auto &it : _attachment->getRenderQueue()->getAttachments()) {
		if (it->type == core::AttachmentType::Image) {
			attachmentIds.emplace_back(it->id);
			cache->addAttachment(it->id);
		}
	}

	_attachment->getRenderQueue()->setCompiled(*_device,
			[loop = Rc<core::Loop>(frame.getLoop()), passIds = sp::move(passIds),
					attachmentIds = sp::move(attachmentIds)]() mutable {
		loop->performOnThread([loop, passIds = sp::move(passIds),
									  attachmentIds = sp::move(attachmentIds)]() mutable {
			auto cache = loop->getFrameCache();
			for (auto &id : passIds) { cache->removeRenderPass(id); }
			for (auto &id : attachmentIds) { cache->removeAttachment(id); }
			cache->removeUnreachableFramebuffers();
		});
	});
}

bool RenderQueuePassHandle::prepareMaterials(FrameHandle &iframe, CommandBuffer &buf,
		core::MaterialAttachment *attachment, Vector<BufferMemoryBarrier> &barriers) {
	auto initial = attachment->getPredefinedMaterials();
	if (initial.empty()) {
		return true;
	}

	// mark attachment as compiled to allow material preparation on it
	// note that in this case setCompiled will be called twice
	attachment->setCompiled(*_device);

	// note that layout may not be compiled yet, recalculate material layout size manualy
	auto imageCount =
			vk::TextureSetLayout::getLayoutImageCount(*_device, *attachment->getTargetLayout());

	auto data = attachment->allocateSet(*_device, imageCount);

	auto buffers = updateMaterials(iframe, data.get(), initial, SpanView<core::MaterialId>(),
			SpanView<core::MaterialId>());

	core::QueueFlags ops = core::QueueFlags::None;
	for (auto &it : attachment->getRenderPasses()) {
		ops |= static_cast<vk::QueuePass *>(it->pass.get())->getQueueOps();
	}

	if (auto q = _device->getQueueFamily(ops)) {
		for (auto &it : buffers) {
			buf.cmdCopyBuffer(it.source, it.target);

			if (q->index == _pool->getFamilyIdx()) {
				barriers.emplace_back(BufferMemoryBarrier(it.target, VK_ACCESS_TRANSFER_WRITE_BIT,
						VK_ACCESS_SHADER_READ_BIT));
			} else {
				barriers.emplace_back(BufferMemoryBarrier(it.target, VK_ACCESS_TRANSFER_WRITE_BIT,
						VK_ACCESS_SHADER_READ_BIT,
						QueueFamilyTransfer{_pool->getFamilyIdx(), q->index}, 0, VK_WHOLE_SIZE));
				it.target->setPendingBarrier(barriers.back());
			}
		}
	}

	iframe.performOnGlThread([attachment, data](FrameHandle &) { attachment->setMaterials(data); },
			nullptr, false, "RenderQueueRenderPassHandle::prepareMaterials");

	return true;
}

} // namespace stappler::xenolith::vk
