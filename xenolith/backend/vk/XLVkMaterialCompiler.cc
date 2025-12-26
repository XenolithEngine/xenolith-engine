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

#include "XLVkMaterialCompiler.h"
#include "XLVkObject.h"
#include "XLVkQueuePass.h"
#include "XLVkDeviceQueue.h"
#include "XLCoreFrameRequest.h"
#include "XLCoreFrameQueue.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::vk {

class MaterialCompilationAttachment : public core::GenericAttachment {
public:
	virtual ~MaterialCompilationAttachment() = default;

	virtual Rc<AttachmentHandle> makeFrameHandle(const FrameQueue &) override;
};

class MaterialCompilationAttachmentHandle : public core::AttachmentHandle {
public:
	virtual ~MaterialCompilationAttachmentHandle() = default;

	virtual bool setup(FrameQueue &handle, Function<void(bool)> &&) override;
	virtual void submitInput(FrameQueue &, Rc<core::AttachmentInputData> &&,
			Function<void(bool)> &&) override;

	virtual const Rc<core::MaterialInputData> &getInputData() const { return _inputData; }
	virtual const Rc<core::MaterialSet> &getOriginalSet() const { return _originalSet; }

protected:
	Rc<core::MaterialInputData> _inputData;
	Rc<core::MaterialSet> _originalSet;
};

class MaterialCompilationRenderPass : public QueuePass {
public:
	virtual ~MaterialCompilationRenderPass() = default;

	virtual bool init(QueuePassBuilder &, const AttachmentData *);

	virtual Rc<QueuePassHandle> makeFrameHandle(const FrameQueue &) override;

	const AttachmentData *getMaterialAttachment() const { return _materialAttachment; }

protected:
	using QueuePass::init;

	const AttachmentData *_materialAttachment = nullptr;
};

class MaterialCompilationPassHandle : public QueuePassHandle {
public:
	virtual ~MaterialCompilationPassHandle() = default;

	virtual bool prepare(FrameQueue &, Function<void(bool)> &&) override;
	virtual void finalize(FrameQueue &, bool successful) override;

protected:
	virtual Vector<const core::CommandBuffer *> doPrepareCommands(FrameHandle &) override;
	virtual void doSubmitted(FrameHandle &, Function<void(bool)> &&, bool,
			Rc<core::Fence> &&) override;
	virtual void doComplete(FrameQueue &, Function<void(bool)> &&, bool) override;

	Rc<core::MaterialSet> _outputData;
	MaterialCompilationAttachmentHandle *_materialAttachment;
};

MaterialCompiler::~MaterialCompiler() { }

bool MaterialCompiler::init() {
	using namespace core;

	Queue::Builder builder("MaterialCompiler");

	auto attachment = builder.addAttachemnt("MaterialAttachment",
			[&](AttachmentBuilder &attachmentBuilder) -> Rc<Attachment> {
		attachmentBuilder.defineAsInput();
		attachmentBuilder.defineAsOutput();
		return Rc<MaterialCompilationAttachment>::create(attachmentBuilder);
	});

	builder.addPass("MaterialRenderPass", PassType::Transfer, RenderOrdering(0),
			[&](QueuePassBuilder &passBuilder) -> Rc<core::QueuePass> {
		return Rc<MaterialCompilationRenderPass>::create(passBuilder, attachment);
	});

	if (core::Queue::init(move(builder))) {
		_attachment = attachment;
		return true;
	}
	return false;
}

bool MaterialCompiler::inProgress(const MaterialAttachment *a) const {
	auto it = _inProgress.find(a);
	if (it != _inProgress.end()) {
		return true;
	}
	return false;
}

void MaterialCompiler::setInProgress(const MaterialAttachment *a) { _inProgress.emplace(a); }

void MaterialCompiler::dropInProgress(const MaterialAttachment *a) { _inProgress.erase(a); }

bool MaterialCompiler::hasRequest(const MaterialAttachment *a) const {
	auto it = _requests.find(a);
	if (it != _requests.end()) {
		return true;
	}
	return false;
}

void MaterialCompiler::appendRequest(const MaterialAttachment *a, Rc<MaterialInputData> &&req,
		Vector<Rc<core::DependencyEvent>> &&deps) {
	auto it = _requests.find(a);
	if (it == _requests.end()) {
		it = _requests.emplace(a, MaterialRequest()).first;
	}

	for (auto &rem : req->materialsToRemove) {
		auto m = it->second.materials.find(rem);
		if (m != it->second.materials.end()) {
			it->second.materials.erase(m);
		}

		auto d = it->second.dynamic.find(rem);
		if (d != it->second.dynamic.end()) {
			it->second.dynamic.erase(d);
		}

		it->second.remove.emplace(rem);
	}

	for (auto &rem : req->dynamicMaterialsToUpdate) { it->second.dynamic.emplace(rem); }

	for (auto &m : req->materialsToAddOrUpdate) {
		auto materialId = m->getId();
		auto iit = it->second.materials.find(materialId);
		if (iit == it->second.materials.end()) {
			it->second.materials.emplace(materialId, move(m));
		} else {
			iit->second = move(m);
		}
		auto v = it->second.remove.find(materialId);
		if (v != it->second.remove.end()) {
			it->second.remove.erase(v);
		}
	}

	if (it->second.deps.empty()) {
		it->second.deps = sp::move(deps);
	} else {
		for (auto &iit : deps) { it->second.deps.emplace_back(move(iit)); }
	}

	if (it->second.callback) {
		it->second.callback = [cb = sp::move(it->second.callback), cb2 = sp::move(req->callback)] {
			cb();
			cb2();
		};
	} else {
		it->second.callback = sp::move(req->callback);
	}
}

void MaterialCompiler::appendRequest(const MaterialAttachment *a, Rc<MaterialInputData> &&input,
		SpanView<Rc<core::DependencyEvent>> deps) {
	appendRequest(a, sp::move(input), deps.vec<Interface>());
}

void MaterialCompiler::clearRequests() { _requests.clear(); }

auto MaterialCompiler::makeRequest(Rc<MaterialInputData> &&input,
		Vector<Rc<core::DependencyEvent>> &&deps) -> Rc<FrameRequest> {
	auto req = Rc<FrameRequest>::create(this);
	req->addInput(_attachment, move(input));
	req->addSignalDependencies(sp::move(deps));
	return req;
}

auto MaterialCompiler::makeRequest(Rc<MaterialInputData> &&input,
		SpanView<Rc<core::DependencyEvent>> deps) -> Rc<FrameRequest> {
	return makeRequest(sp::move(input), deps.vec<Interface>());
}

void MaterialCompiler::runMaterialCompilationFrame(core::Loop &loop, Rc<MaterialInputData> &&req,
		Vector<Rc<core::DependencyEvent>> &&deps) {
	auto targetAttachment = req->attachment;

	auto h = loop.makeFrame(makeRequest(sp::move(req), sp::move(deps)), 0);
	h->setCompleteCallback([this, targetAttachment](FrameHandle &handle) {
		auto reqIt = _requests.find(targetAttachment);
		if (reqIt != _requests.end()) {
			if (handle.getLoop()->isRunning()) {
				auto deps = sp::move(reqIt->second.deps);
				Rc<MaterialInputData> req = Rc<MaterialInputData>::alloc();
				req->attachment = targetAttachment;
				req->materialsToAddOrUpdate.reserve(reqIt->second.materials.size());
				for (auto &m : reqIt->second.materials) {
					req->materialsToAddOrUpdate.emplace_back(m.second);
				}
				req->materialsToRemove.reserve(reqIt->second.remove.size());
				for (auto &m : reqIt->second.remove) { req->materialsToRemove.emplace_back(m); }
				req->dynamicMaterialsToUpdate.reserve(reqIt->second.dynamic.size());
				for (auto &m : reqIt->second.dynamic) {
					req->dynamicMaterialsToUpdate.emplace_back(m);
				}
				req->callback = sp::move(reqIt->second.callback);
				_requests.erase(reqIt);

				runMaterialCompilationFrame(*handle.getLoop(), move(req),
						Vector<Rc<core::DependencyEvent>>(deps));
			} else {
				clearRequests();
				dropInProgress(targetAttachment);
			}
		} else {
			dropInProgress(targetAttachment);
		}
	});
	h->update(true);
}

void MaterialCompiler::runMaterialCompilationFrame(core::Loop &loop, Rc<MaterialInputData> &&req,
		SpanView<Rc<core::DependencyEvent>> deps) {
	runMaterialCompilationFrame(loop, sp::move(req), deps.vec<Interface>());
}

auto MaterialCompilationAttachment::makeFrameHandle(const FrameQueue &handle)
		-> Rc<AttachmentHandle> {
	return Rc<MaterialCompilationAttachmentHandle>::create(this, handle);
}

bool MaterialCompilationAttachmentHandle::setup(FrameQueue &handle, Function<void(bool)> &&cb) {
	return true;
}

void MaterialCompilationAttachmentHandle::submitInput(FrameQueue &q,
		Rc<core::AttachmentInputData> &&data, Function<void(bool)> &&cb) {
	auto d = data.cast<core::MaterialInputData>();
	if (!d || q.isFinalized()) {
		cb(false);
	}

	q.getFrame()->waitForDependencies(data->waitDependencies,
			[this, d = sp::move(d), cb = sp::move(cb)](FrameHandle &handle, bool success) {
		handle.performOnGlThread([this, d = sp::move(d), cb = sp::move(cb)](FrameHandle &handle) {
			_inputData = d;
			_originalSet = _inputData->attachment->getMaterials();
			cb(true);
		}, this, true, "MaterialCompilationAttachmentHandle::submitInput");
	});
}

bool MaterialCompilationRenderPass::init(QueuePassBuilder &passBuilder,
		const AttachmentData *attachment) {
	passBuilder.addAttachment(attachment);

	if (!QueuePass::init(passBuilder)) {
		return false;
	}

	_queueOps = core::QueueFlags::Transfer;
	_materialAttachment = attachment;
	return true;
}

auto MaterialCompilationRenderPass::makeFrameHandle(const FrameQueue &handle)
		-> Rc<QueuePassHandle> {
	return Rc<MaterialCompilationPassHandle>::create(*this, handle);
}

bool MaterialCompilationPassHandle::prepare(FrameQueue &frame, Function<void(bool)> &&cb) {
	if (auto a = frame.getAttachment(static_cast<MaterialCompilationRenderPass *>(_queuePass.get())
						->getMaterialAttachment())) {
		_materialAttachment = static_cast<MaterialCompilationAttachmentHandle *>(a->handle.get());
	}

	auto &originalData = _materialAttachment->getOriginalSet();
	auto &inputData = _materialAttachment->getInputData();
	_outputData = inputData->attachment->cloneSet(originalData);

	return QueuePassHandle::prepare(frame, sp::move(cb));
}

void MaterialCompilationPassHandle::finalize(FrameQueue &handle, bool successful) {
	QueuePassHandle::finalize(handle, successful);
}

Vector<const core::CommandBuffer *> MaterialCompilationPassHandle::doPrepareCommands(
		FrameHandle &handle) {
	auto &inputData = _materialAttachment->getInputData();
	auto buffers = updateMaterials(handle, _outputData.get(), inputData->materialsToAddOrUpdate,
			inputData->dynamicMaterialsToUpdate, inputData->materialsToRemove);
	if (buffers.empty()) {
		return Vector<const core::CommandBuffer *>();
	}

	core::QueueFlags ops = core::QueueFlags::None;
	for (auto &it : inputData->attachment->getRenderPasses()) {
		ops |= static_cast<vk::QueuePass *>(it->pass.get())->getQueueOps();
	}

	auto q = _device->getQueueFamily(ops);
	if (!q) {
		return Vector<const core::CommandBuffer *>();
	}

	VkPipelineStageFlags targetStages = 0;
	if ((_pool->getClass() & core::QueueFlags::Graphics) != core::QueueFlags::None) {
		targetStages |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	if ((_pool->getClass() & core::QueueFlags::Compute) != core::QueueFlags::None) {
		targetStages |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
	}
	if (!targetStages) {
		targetStages = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
	}

	auto buf = _pool->recordBuffer(*_device, Vector<Rc<DescriptorPool>>(_descriptors),
			[&, this](CommandBuffer &buf) {
		Vector<BufferMemoryBarrier> barriers;
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
		buf.cmdPipelineBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, targetStages, 0, barriers);
		return true;
	});
	return Vector<const core::CommandBuffer *>{buf};
}

void MaterialCompilationPassHandle::doSubmitted(FrameHandle &frame, Function<void(bool)> &&func,
		bool success, Rc<core::Fence> &&fence) {
	if (success) {
		_materialAttachment->getInputData()->attachment->setMaterials(_outputData);
	}

	QueuePassHandle::doSubmitted(frame, sp::move(func), success, sp::move(fence));
	frame.signalDependencies(success);
}

void MaterialCompilationPassHandle::doComplete(FrameQueue &queue, Function<void(bool)> &&func,
		bool success) {
	if (auto &cb = _materialAttachment->getInputData()->callback) {
		cb();
	}

	QueuePassHandle::doComplete(queue, sp::move(func), success);
}

} // namespace stappler::xenolith::vk
